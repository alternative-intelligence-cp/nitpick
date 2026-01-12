/**
 * @file ternary_codegen.cpp
 * @brief LLVM IR generation for balanced ternary and nonary arithmetic
 *
 * ARIA-013: Balanced Ternary and Nonary Intrinsics
 *
 * This file generates LLVM IR for trit/tryte/nit/nyte types:
 *
 * Atomic Types (inline LLVM operations with clamping):
 * - trit: Single balanced ternary digit (-1, 0, 1)
 * - nit:  Single balanced nonary digit (-4 to 4)
 *
 * Composite Types (calls to runtime intrinsics):
 * - tryte: 10 trits packed in 16 bits (Split-Byte format)
 * - nyte:  5 nits packed in 16 bits (Biased-Radix format)
 *
 * For tryte/nyte, we emit calls to the C runtime intrinsics defined in
 * ternary_ops.cpp (aria_tryte_add, aria_nyte_add, etc.) because:
 * - tryte uses Split-Byte packing (two 5-trit trybbles)
 * - nyte uses Biased-Radix storage (value + 29524 bias)
 * - Both require LUT-based arithmetic with proper carry propagation
 */

#include "backend/ir/ternary_codegen.h"
#include "frontend/sema/type.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <iostream>  // For std::cerr debug output

namespace aria {

TernaryCodegen::TernaryCodegen(llvm::LLVMContext& context, llvm::IRBuilder<>& builder)
    : context(context), builder(builder), module(nullptr) {}

// ============================================================================
// Private helpers
// ============================================================================

bool TernaryCodegen::isCompositeType(Type* type) const {
    std::string typeName = type->toString();
    return (typeName == "tryte" || typeName == "nyte");
}

llvm::Function* TernaryCodegen::getOrDeclareIntrinsic(const std::string& name, bool isBinaryOp) {
    if (!module) {
        return nullptr;  // Module not set, cannot declare intrinsics
    }

    // Check if already declared
    if (llvm::Function* existing = module->getFunction(name)) {
        return existing;
    }

    // Declare the intrinsic
    llvm::Type* i16 = llvm::Type::getInt16Ty(context);

    llvm::FunctionType* funcType;
    if (isBinaryOp) {
        // Binary operation: (i16, i16) -> i16
        funcType = llvm::FunctionType::get(i16, {i16, i16}, false);
    } else {
        // Unary operation: (i16) -> i16
        funcType = llvm::FunctionType::get(i16, {i16}, false);
    }

    llvm::Function* func = llvm::Function::Create(
        funcType,
        llvm::Function::ExternalLinkage,
        name,
        module
    );

    return func;
}

llvm::Value* TernaryCodegen::clampToRange(llvm::Value* value, Type* type) {
    llvm::Value* maxVal = getMaxValue(type);
    llvm::Value* minVal = getMinValue(type);

    // Ensure value has the correct type for comparison
    llvm::IntegerType* targetType = getTernaryLLVMType(type);
    if (value->getType() != targetType) {
        value = builder.CreateIntCast(value, targetType, true, "clamp_cast");
    }

    // if (value > max) value = max;
    llvm::Value* exceedsMax = builder.CreateICmpSGT(value, maxVal);
    value = builder.CreateSelect(exceedsMax, maxVal, value);

    // if (value < min) value = min;
    llvm::Value* belowMin = builder.CreateICmpSLT(value, minVal);
    value = builder.CreateSelect(belowMin, minVal, value);

    return value;
}

// ============================================================================
// Public arithmetic operations
// ============================================================================

llvm::Value* TernaryCodegen::generateAdd(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    std::string typeName = type->toString();

    if (isCompositeType(type)) {
        // Composite type (tryte/nyte): call runtime intrinsic
        llvm::Function* addFn = nullptr;
        if (typeName == "tryte") {
            if (!fn_tryte_add) {
                fn_tryte_add = getOrDeclareIntrinsic("aria_tryte_add", true);
            }
            addFn = fn_tryte_add;
        } else {  // nyte
            if (!fn_nyte_add) {
                fn_nyte_add = getOrDeclareIntrinsic("aria_nyte_add", true);
            }
            addFn = fn_nyte_add;
        }

        if (addFn) {
            // Ensure operands are i16
            llvm::Type* i16 = builder.getInt16Ty();
            if (lhs->getType() != i16) {
                lhs = builder.CreateIntCast(lhs, i16, true, "tryte_cast_lhs");
            }
            if (rhs->getType() != i16) {
                rhs = builder.CreateIntCast(rhs, i16, true, "tryte_cast_rhs");
            }
            return builder.CreateCall(addFn, {lhs, rhs}, typeName + "_add");
        }
    }

    // Atomic type (trit/nit): inline LLVM operation with clamping
    llvm::Value* result = builder.CreateAdd(lhs, rhs, "add_tmp");
    return clampToRange(result, type);
}

llvm::Value* TernaryCodegen::generateSub(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    std::string typeName = type->toString();

    if (isCompositeType(type)) {
        // Composite type (tryte/nyte): call runtime intrinsic
        llvm::Function* subFn = nullptr;
        if (typeName == "tryte") {
            if (!fn_tryte_sub) {
                fn_tryte_sub = getOrDeclareIntrinsic("aria_tryte_sub", true);
            }
            subFn = fn_tryte_sub;
        } else {  // nyte
            if (!fn_nyte_sub) {
                fn_nyte_sub = getOrDeclareIntrinsic("aria_nyte_sub", true);
            }
            subFn = fn_nyte_sub;
        }

        if (subFn) {
            llvm::Type* i16 = builder.getInt16Ty();
            if (lhs->getType() != i16) {
                lhs = builder.CreateIntCast(lhs, i16, true, "tryte_cast_lhs");
            }
            if (rhs->getType() != i16) {
                rhs = builder.CreateIntCast(rhs, i16, true, "tryte_cast_rhs");
            }
            return builder.CreateCall(subFn, {lhs, rhs}, typeName + "_sub");
        }
    }

    // Atomic type (trit/nit): inline LLVM operation with clamping
    llvm::Value* result = builder.CreateSub(lhs, rhs, "sub_tmp");
    return clampToRange(result, type);
}

llvm::Value* TernaryCodegen::generateMul(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    std::string typeName = type->toString();

    if (isCompositeType(type)) {
        // Composite type (tryte/nyte): call runtime intrinsic
        llvm::Function* mulFn = nullptr;
        if (typeName == "tryte") {
            if (!fn_tryte_mul) {
                fn_tryte_mul = getOrDeclareIntrinsic("aria_tryte_mul", true);
            }
            mulFn = fn_tryte_mul;
        } else {  // nyte
            if (!fn_nyte_mul) {
                fn_nyte_mul = getOrDeclareIntrinsic("aria_nyte_mul", true);
            }
            mulFn = fn_nyte_mul;
        }

        if (mulFn) {
            llvm::Type* i16 = builder.getInt16Ty();
            if (lhs->getType() != i16) {
                lhs = builder.CreateIntCast(lhs, i16, true, "tryte_cast_lhs");
            }
            if (rhs->getType() != i16) {
                rhs = builder.CreateIntCast(rhs, i16, true, "tryte_cast_rhs");
            }
            return builder.CreateCall(mulFn, {lhs, rhs}, typeName + "_mul");
        }
    }

    // Atomic type (trit/nit): inline LLVM operation with clamping
    llvm::Value* result = builder.CreateMul(lhs, rhs, "mul_tmp");
    return clampToRange(result, type);
}

llvm::Value* TernaryCodegen::generateDiv(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    std::string typeName = type->toString();

    if (isCompositeType(type)) {
        // Composite type (tryte/nyte): call runtime intrinsic
        llvm::Function* divFn = nullptr;
        if (typeName == "tryte") {
            if (!fn_tryte_div) {
                fn_tryte_div = getOrDeclareIntrinsic("aria_tryte_div", true);
            }
            divFn = fn_tryte_div;
        } else {  // nyte
            if (!fn_nyte_div) {
                fn_nyte_div = getOrDeclareIntrinsic("aria_nyte_div", true);
            }
            divFn = fn_nyte_div;
        }

        if (divFn) {
            llvm::Type* i16 = builder.getInt16Ty();
            if (lhs->getType() != i16) {
                lhs = builder.CreateIntCast(lhs, i16, true, "tryte_cast_lhs");
            }
            if (rhs->getType() != i16) {
                rhs = builder.CreateIntCast(rhs, i16, true, "tryte_cast_rhs");
            }
            return builder.CreateCall(divFn, {lhs, rhs}, typeName + "_div");
        }
    }

    // Atomic type (trit/nit): inline LLVM operation with clamping
    llvm::IntegerType* llvmType = getTernaryLLVMType(type);

    // Check for division by zero
    llvm::Value* zero = llvm::ConstantInt::get(llvmType, 0);
    llvm::Value* isZero = builder.CreateICmpEQ(rhs, zero);

    // If divisor is zero, return 0 (balanced ternary convention)
    llvm::Value* result = builder.CreateSDiv(lhs, rhs);
    result = builder.CreateSelect(isZero, zero, result);

    return clampToRange(result, type);
}

llvm::Value* TernaryCodegen::generateMod(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    std::string typeName = type->toString();
    std::cerr << "[TERNARY MOD] Called for type: " << typeName << ", isComposite=" << isCompositeType(type) << std::endl;

    if (isCompositeType(type)) {
        // Composite type (tryte/nyte): call runtime intrinsic
        // CRITICAL: Runtime functions implement balanced remainder, not truncating modulo
        llvm::Function* modFn = nullptr;
        if (typeName == "tryte") {
            std::cerr << "[TERNARY MOD] Getting aria_tryte_mod" << std::endl;
            if (!fn_tryte_mod) {
                fn_tryte_mod = getOrDeclareIntrinsic("aria_tryte_mod", true);
                std::cerr << "[TERNARY MOD] Declared fn_tryte_mod=" << (void*)fn_tryte_mod << std::endl;
            }
            modFn = fn_tryte_mod;
        } else {  // nyte
            std::cerr << "[TERNARY MOD] Getting aria_nyte_mod" << std::endl;
            if (!fn_nyte_mod) {
                fn_nyte_mod = getOrDeclareIntrinsic("aria_nyte_mod", true);
                std::cerr << "[TERNARY MOD] Declared fn_nyte_mod=" << (void*)fn_nyte_mod << std::endl;
            }
            modFn = fn_nyte_mod;
        }

        std::cerr << "[TERNARY MOD] modFn=" << (void*)modFn << std::endl;
        if (modFn) {
            std::cerr << "[TERNARY MOD] Creating call to " << typeName << "_mod" << std::endl;
            llvm::Type* i16 = builder.getInt16Ty();
            if (lhs->getType() != i16) {
                lhs = builder.CreateIntCast(lhs, i16, true, "tryte_cast_lhs");
            }
            if (rhs->getType() != i16) {
                rhs = builder.CreateIntCast(rhs, i16, true, "tryte_cast_rhs");
            }
            llvm::Value* result = builder.CreateCall(modFn, {lhs, rhs}, typeName + "_mod");
            std::cerr << "[TERNARY MOD] Created call, result=" << (void*)result << std::endl;
            // DEBUG: Print the instruction immediately
            result->print(llvm::errs());
            llvm::errs() << "\n";
            return result;
        }
        std::cerr << "[TERNARY MOD] ERROR: modFn is nullptr!" << std::endl;
    }

    // Atomic type (trit/nit): inline LLVM operation with clamping
    // For atomic types, native modulo is acceptable since they're not biased
    std::cerr << "[TERNARY MOD] Using inline srem (atomic type)" << std::endl;
    llvm::IntegerType* llvmType = getTernaryLLVMType(type);

    // Check for division by zero
    llvm::Value* zero = llvm::ConstantInt::get(llvmType, 0);
    llvm::Value* isZero = builder.CreateICmpEQ(rhs, zero);

    // If divisor is zero, return 0 (balanced ternary convention)
    llvm::Value* result = builder.CreateSRem(lhs, rhs);
    result = builder.CreateSelect(isZero, zero, result);

    return clampToRange(result, type);
}

llvm::Value* TernaryCodegen::generateNeg(llvm::Value* operand, Type* type) {
    std::string typeName = type->toString();

    if (isCompositeType(type)) {
        // Composite type (tryte/nyte): call runtime intrinsic
        llvm::Function* negFn = nullptr;
        if (typeName == "tryte") {
            if (!fn_tryte_neg) {
                fn_tryte_neg = getOrDeclareIntrinsic("aria_tryte_neg", false);
            }
            negFn = fn_tryte_neg;
        } else {  // nyte
            if (!fn_nyte_neg) {
                fn_nyte_neg = getOrDeclareIntrinsic("aria_nyte_neg", false);
            }
            negFn = fn_nyte_neg;
        }

        if (negFn) {
            llvm::Type* i16 = builder.getInt16Ty();
            if (operand->getType() != i16) {
                operand = builder.CreateIntCast(operand, i16, true, "tryte_cast");
            }
            return builder.CreateCall(negFn, {operand}, typeName + "_neg");
        }
    }

    // Atomic type (trit/nit): inline LLVM operation
    // Negation should never overflow for balanced ternary (symmetric range)
    llvm::Value* result = builder.CreateNeg(operand, "neg_tmp");
    return clampToRange(result, type);
}

// ============================================================================
// Public helpers
// ============================================================================

llvm::Value* TernaryCodegen::getMaxValue(Type* type) {
    llvm::IntegerType* llvmType = getTernaryLLVMType(type);
    std::string typeName = type->toString();

    if (typeName == "trit") {
        // trit max: 1
        return llvm::ConstantInt::getSigned(llvmType, 1);
    } else if (typeName == "nit") {
        // nit max: 4
        return llvm::ConstantInt::getSigned(llvmType, 4);
    } else if (typeName == "tryte" || typeName == "nyte") {
        // tryte/nyte: (3^10 - 1) / 2 = (9^5 - 1) / 2 = 29524
        return llvm::ConstantInt::getSigned(llvmType, 29524);
    }

    // Fallback
    return llvm::ConstantInt::getSigned(llvmType, 1);
}

llvm::Value* TernaryCodegen::getMinValue(Type* type) {
    llvm::IntegerType* llvmType = getTernaryLLVMType(type);
    std::string typeName = type->toString();

    if (typeName == "trit") {
        // trit min: -1
        return llvm::ConstantInt::getSigned(llvmType, -1);
    } else if (typeName == "nit") {
        // nit min: -4
        return llvm::ConstantInt::getSigned(llvmType, -4);
    } else if (typeName == "tryte" || typeName == "nyte") {
        // tryte/nyte: -(3^10 - 1) / 2 = -29524
        return llvm::ConstantInt::getSigned(llvmType, -29524);
    }

    // Fallback
    return llvm::ConstantInt::getSigned(llvmType, -1);
}

llvm::IntegerType* TernaryCodegen::getTernaryLLVMType(Type* type) {
    std::string typeName = type->toString();

    if (typeName == "trit") {
        // 3-bit signed for trit (-1, 0, 1) with overflow detection
        return builder.getIntNTy(3);
    } else if (typeName == "nit") {
        // 4-bit signed for nit (-4 to 4)
        return builder.getIntNTy(4);
    } else if (typeName == "tryte" || typeName == "nyte") {
        // 16-bit for packed composite types
        return builder.getInt16Ty();
    }

    // Fallback
    return builder.getIntNTy(2);
}

} // namespace aria

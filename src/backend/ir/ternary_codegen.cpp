#include "backend/ir/ternary_codegen.h"
#include "frontend/sema/type.h"
#include <llvm/IR/Constants.h>

namespace aria {

TernaryCodegen::TernaryCodegen(llvm::LLVMContext& context, llvm::IRBuilder<>& builder)
    : context(context), builder(builder) {}

// ============================================================================
// Public arithmetic operations
// ============================================================================

llvm::Value* TernaryCodegen::generateAdd(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    // Perform addition
    llvm::Value* result = builder.CreateAdd(lhs, rhs);
    
    // Clamp to valid range
    return clampToRange(result, type);
}

llvm::Value* TernaryCodegen::generateSub(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    // Perform subtraction
    llvm::Value* result = builder.CreateSub(lhs, rhs);
    
    // Clamp to valid range
    return clampToRange(result, type);
}

llvm::Value* TernaryCodegen::generateMul(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    // Perform multiplication
    llvm::Value* result = builder.CreateMul(lhs, rhs);
    
    // Clamp to valid range
    return clampToRange(result, type);
}

llvm::Value* TernaryCodegen::generateDiv(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    llvm::IntegerType* llvmType = getTernaryLLVMType(type);
    
    // Check for division by zero
    llvm::Value* zero = llvm::ConstantInt::get(llvmType, 0);
    llvm::Value* isZero = builder.CreateICmpEQ(rhs, zero);
    
    // If divisor is zero, return 0 (balanced ternary convention)
    llvm::Value* result = builder.CreateSDiv(lhs, rhs);
    result = builder.CreateSelect(isZero, zero, result);
    
    // Clamp to valid range
    return clampToRange(result, type);
}

llvm::Value* TernaryCodegen::generateNeg(llvm::Value* operand, Type* type) {
    // Negation is just 0 - operand
    llvm::Value* result = builder.CreateNeg(operand);
    
    // Clamp to valid range (though negation should never overflow for balanced ternary)
    return clampToRange(result, type);
}

// ============================================================================
// Public helpers
// ============================================================================

llvm::Value* TernaryCodegen::getMaxValue(Type* type) {
    llvm::IntegerType* llvmType = getTernaryLLVMType(type);
    std::string typeName = type->toString();
    
    if (typeName == "trit") {
        // trit max: 1 (use signed to ensure proper comparison)
        return llvm::ConstantInt::getSigned(llvmType, 1);
    } else if (typeName == "nit") {
        // nit max: 4
        return llvm::ConstantInt::getSigned(llvmType, 4);
    } else if (typeName == "tryte") {
        // tryte: 10 trits packed = 3^10 - 1 / 2 = 29524
        // (3^10 = 59049, balanced range is symmetric around 0)
        return llvm::ConstantInt::getSigned(llvmType, 29524);
    } else if (typeName == "nyte") {
        // nyte: 5 nits packed = 9^5 - 1 / 2 = 29524
        // (9^5 = 59049, balanced range is symmetric around 0)
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
    } else if (typeName == "tryte") {
        // tryte: 10 trits packed = -(3^10 - 1) / 2 = -29524
        return llvm::ConstantInt::getSigned(llvmType, -29524);
    } else if (typeName == "nyte") {
        // nyte: 5 nits packed = -(9^5 - 1) / 2 = -29524
        return llvm::ConstantInt::getSigned(llvmType, -29524);
    }
    
    // Fallback
    return llvm::ConstantInt::getSigned(llvmType, -1);
}

llvm::IntegerType* TernaryCodegen::getTernaryLLVMType(Type* type) {
    std::string typeName = type->toString();
    
    if (typeName == "trit") {
        return builder.getIntNTy(3);  // 3-bit signed for overflow detection
    } else if (typeName == "nit") {
        return builder.getIntNTy(4);  // 4-bit signed
    } else if (typeName == "tryte" || typeName == "nyte") {
        return builder.getInt16Ty();  // 16-bit for packed types
    }
    
    // Fallback
    return builder.getIntNTy(2);
}

// ============================================================================
// Private helpers
// ============================================================================

llvm::Value* TernaryCodegen::clampToRange(llvm::Value* value, Type* type) {
    llvm::Value* maxVal = getMaxValue(type);
    llvm::Value* minVal = getMinValue(type);
    
    // if (value > max) value = max;
    llvm::Value* exceedsMax = builder.CreateICmpSGT(value, maxVal);
    value = builder.CreateSelect(exceedsMax, maxVal, value);
    
    // if (value < min) value = min;
    llvm::Value* belowMin = builder.CreateICmpSLT(value, minVal);
    value = builder.CreateSelect(belowMin, minVal, value);
    
    return value;
}

} // namespace aria

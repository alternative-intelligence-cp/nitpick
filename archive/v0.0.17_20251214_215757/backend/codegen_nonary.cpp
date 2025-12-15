/**
 * src/backend/codegen_nonary.cpp
 * 
 * Balanced Nonary Type LLVM Code Generation Implementation
 * Version: 0.0.12
 */

#include "codegen_nonary.h"
#include "nonary_ops.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>

namespace aria {
namespace backend {

// ========== Type Checking ==========

bool NonaryLowerer::isNonaryType(const std::string& typeName) {
    return typeName == "nit" || typeName == "nyte";
}

// ========== Sentinel Values ==========

llvm::Value* NonaryLowerer::getNyteSentinel() {
    return llvm::ConstantInt::get(llvm::Type::getInt16Ty(llvmContext), NYTE_ERR);
}

// ========== Initialization ==========

void NonaryLowerer::ensureInitialized() {
    if (initializeFunc) {
        return;  // Already initialized
    }

    // Declare void @_aria_nonary_initialize()
    llvm::FunctionType* funcType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(llvmContext),
        {},
        false
    );

    initializeFunc = llvm::Function::Create(
        funcType,
        llvm::Function::ExternalLinkage,
        "_aria_nonary_initialize",
        module
    );

    // Call it once at module init (this should be called from main or module constructor)
    // For now, we'll rely on the runtime being initialized elsewhere
}

// ========== Helper: Function Declaration ==========

llvm::Function* NonaryLowerer::getOrDeclareRuntimeFunc(
    const std::string& name,
    llvm::Type* returnType,
    const std::vector<llvm::Type*>& argTypes
) {
    // Check if function already exists
    llvm::Function* func = module->getFunction(name);
    if (func) {
        return func;
    }

    // Create function type
    llvm::FunctionType* funcType = llvm::FunctionType::get(
        returnType,
        argTypes,
        false  // not vararg
    );

    // Declare external function
    func = llvm::Function::Create(
        funcType,
        llvm::Function::ExternalLinkage,
        name,
        module
    );

    return func;
}

// ========== Arithmetic Operations ==========

llvm::Value* NonaryLowerer::createNyteAdd(llvm::Value* lhs, llvm::Value* rhs) {
    if (!addNytesFunc) {
        llvm::Type* i16 = llvm::Type::getInt16Ty(llvmContext);
        addNytesFunc = getOrDeclareRuntimeFunc("_aria_nyte_add", i16, {i16, i16});
    }

    return builder.CreateCall(addNytesFunc, {lhs, rhs}, "nyte.add");
}

llvm::Value* NonaryLowerer::createNyteSub(llvm::Value* lhs, llvm::Value* rhs) {
    if (!subNytesFunc) {
        llvm::Type* i16 = llvm::Type::getInt16Ty(llvmContext);
        subNytesFunc = getOrDeclareRuntimeFunc("_aria_nyte_sub", i16, {i16, i16});
    }

    return builder.CreateCall(subNytesFunc, {lhs, rhs}, "nyte.sub");
}

llvm::Value* NonaryLowerer::createNyteMul(llvm::Value* lhs, llvm::Value* rhs) {
    if (!mulNytesFunc) {
        llvm::Type* i16 = llvm::Type::getInt16Ty(llvmContext);
        mulNytesFunc = getOrDeclareRuntimeFunc("_aria_nyte_mul", i16, {i16, i16});
    }

    return builder.CreateCall(mulNytesFunc, {lhs, rhs}, "nyte.mul");
}

llvm::Value* NonaryLowerer::createNyteDiv(llvm::Value* lhs, llvm::Value* rhs) {
    if (!divNytesFunc) {
        llvm::Type* i16 = llvm::Type::getInt16Ty(llvmContext);
        divNytesFunc = getOrDeclareRuntimeFunc("_aria_nyte_div", i16, {i16, i16});
    }

    return builder.CreateCall(divNytesFunc, {lhs, rhs}, "nyte.div");
}

llvm::Value* NonaryLowerer::createNyteMod(llvm::Value* lhs, llvm::Value* rhs) {
    if (!modNytesFunc) {
        llvm::Type* i16 = llvm::Type::getInt16Ty(llvmContext);
        modNytesFunc = getOrDeclareRuntimeFunc("_aria_nyte_mod", i16, {i16, i16});
    }

    return builder.CreateCall(modNytesFunc, {lhs, rhs}, "nyte.mod");
}

llvm::Value* NonaryLowerer::createNyteNegate(llvm::Value* val) {
    if (!negateNyteFunc) {
        llvm::Type* i16 = llvm::Type::getInt16Ty(llvmContext);
        negateNyteFunc = getOrDeclareRuntimeFunc("_aria_nyte_negate", i16, {i16});
    }

    return builder.CreateCall(negateNyteFunc, {val}, "nyte.neg");
}

// ========== Comparison Operations ==========

llvm::Value* NonaryLowerer::createNyteCompare(llvm::Value* lhs, llvm::Value* rhs) {
    if (!compareNytesFunc) {
        llvm::Type* i32 = llvm::Type::getInt32Ty(llvmContext);
        llvm::Type* i16 = llvm::Type::getInt16Ty(llvmContext);
        compareNytesFunc = getOrDeclareRuntimeFunc("_aria_nyte_compare", i32, {i16, i16});
    }

    return builder.CreateCall(compareNytesFunc, {lhs, rhs}, "nyte.cmp");
}

llvm::Value* NonaryLowerer::createNyteEquals(llvm::Value* lhs, llvm::Value* rhs) {
    // Direct comparison works for equality
    return builder.CreateICmpEQ(lhs, rhs, "nyte.eq");
}

llvm::Value* NonaryLowerer::createNyteLessThan(llvm::Value* lhs, llvm::Value* rhs) {
    // Unsigned less-than works due to biased encoding
    // (value + bias) preserves ordering
    return builder.CreateICmpULT(lhs, rhs, "nyte.lt");
}

// ========== Conversion Operations ==========

llvm::Value* NonaryLowerer::createIntToNyte(llvm::Value* val) {
    if (!binaryToNyteFunc) {
        llvm::Type* i16 = llvm::Type::getInt16Ty(llvmContext);
        llvm::Type* i32 = llvm::Type::getInt32Ty(llvmContext);
        binaryToNyteFunc = getOrDeclareRuntimeFunc("_aria_int_to_nyte", i16, {i32});
    }

    return builder.CreateCall(binaryToNyteFunc, {val}, "int.to.nyte");
}

llvm::Value* NonaryLowerer::createNyteToInt(llvm::Value* val) {
    if (!nyteToBinaryFunc) {
        llvm::Type* i32 = llvm::Type::getInt32Ty(llvmContext);
        llvm::Type* i16 = llvm::Type::getInt16Ty(llvmContext);
        nyteToBinaryFunc = getOrDeclareRuntimeFunc("_aria_nyte_to_int", i32, {i16});
    }

    return builder.CreateCall(nyteToBinaryFunc, {val}, "nyte.to.int");
}

// ========== Literal Creation ==========

llvm::Value* NonaryLowerer::createNitLiteral(int8_t value) {
    // Validate range
    if (value < NIT_MIN || value > NIT_MAX) {
        // Return 0 for invalid literals (should be caught by type checker)
        return llvm::ConstantInt::get(llvm::Type::getInt8Ty(llvmContext), 0);
    }

    return llvm::ConstantInt::get(llvm::Type::getInt8Ty(llvmContext), value, true);
}

llvm::Value* NonaryLowerer::createNyteLiteral(int32_t value) {
    // Validate range
    if (value < NYTE_MIN || value > NYTE_MAX) {
        // Return ERR sentinel for out-of-range literals
        return getNyteSentinel();
    }

    // Apply bias: stored = value + 29,524
    uint16_t packed = static_cast<uint16_t>(value + NYTE_BIAS);
    return llvm::ConstantInt::get(llvm::Type::getInt16Ty(llvmContext), packed);
}

} // namespace backend
} // namespace aria

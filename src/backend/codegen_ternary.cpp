/**
 * src/backend/codegen_ternary.cpp
 * 
 * Balanced Ternary Code Generation Implementation
 */

#include "codegen_ternary.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>

namespace aria {
namespace backend {

using namespace llvm;

bool TernaryLowerer::isTernaryType(const std::string& typeName) {
    return typeName == "trit" || typeName == "tryte";
}

Value* TernaryLowerer::getTryteSentinel() {
    // TRYTE_ERR = 0xFFFF
    Type* i16Type = Type::getInt16Ty(llvmContext);
    return ConstantInt::get(i16Type, 0xFFFF);
}

void TernaryLowerer::ensureInitialized() {
    // Generate a call to TernaryOps::initialize()
    // This is typically done once at module startup
    Function* initFunc = getInitializeFunc();
    builder.CreateCall(initFunc);
}

// ========== Arithmetic Operations ==========

Value* TernaryLowerer::createTryteAdd(Value* lhs, Value* rhs) {
    Function* addFunc = getAddTrytesFunc();
    return builder.CreateCall(addFunc, {lhs, rhs}, "tryte_add");
}

Value* TernaryLowerer::createTryteSub(Value* lhs, Value* rhs) {
    Function* subFunc = getSubTrytesFunc();
    return builder.CreateCall(subFunc, {lhs, rhs}, "tryte_sub");
}

Value* TernaryLowerer::createTryteMul(Value* lhs, Value* rhs) {
    Function* mulFunc = getMulTrytesFunc();
    return builder.CreateCall(mulFunc, {lhs, rhs}, "tryte_mul");
}

Value* TernaryLowerer::createTryteDiv(Value* lhs, Value* rhs) {
    Function* divFunc = getDivTrytesFunc();
    return builder.CreateCall(divFunc, {lhs, rhs}, "tryte_div");
}

Value* TernaryLowerer::createTryteNeg(Value* val) {
    Function* negFunc = getNegateTryteFunc();
    return builder.CreateCall(negFunc, {val}, "tryte_neg");
}

// ========== Conversion Operations ==========

Value* TernaryLowerer::convertBinaryToTryte(Value* val) {
    Function* convFunc = getBinaryToTryteFunc();
    
    // May need to extend or truncate val to i32
    Type* i32Type = Type::getInt32Ty(llvmContext);
    Value* i32Val = val;
    
    if (val->getType() != i32Type) {
        if (val->getType()->getIntegerBitWidth() < 32) {
            i32Val = builder.CreateSExt(val, i32Type, "ext_to_i32");
        } else {
            i32Val = builder.CreateTrunc(val, i32Type, "trunc_to_i32");
        }
    }
    
    return builder.CreateCall(convFunc, {i32Val}, "bin_to_tryte");
}

Value* TernaryLowerer::convertTryteToBinary(Value* val) {
    Function* convFunc = getTryteToBinaryFunc();
    return builder.CreateCall(convFunc, {val}, "tryte_to_bin");
}

// ========== Function Declaration Helpers ==========

Function* TernaryLowerer::getAddTrytesFunc() {
    if (addTrytesFunc) {
        return addTrytesFunc;
    }
    
    // Declare: uint16_t addTrytes(uint16_t a, uint16_t b)
    Type* i16Type = Type::getInt16Ty(llvmContext);
    FunctionType* funcType = FunctionType::get(
        i16Type,                      // return type
        {i16Type, i16Type},          // parameter types
        false                         // not vararg
    );
    
    // Use C++ mangled name: _ZN4aria7backend10TernaryOps10addTrytesEtt
    addTrytesFunc = Function::Create(
        funcType,
        Function::ExternalLinkage,
        "_ZN4aria7backend10TernaryOps10addTrytesEtt",
        module
    );
    
    return addTrytesFunc;
}

Function* TernaryLowerer::getSubTrytesFunc() {
    if (subTrytesFunc) {
        return subTrytesFunc;
    }
    
    Type* i16Type = Type::getInt16Ty(llvmContext);
    FunctionType* funcType = FunctionType::get(
        i16Type,
        {i16Type, i16Type},
        false
    );
    
    // Mangled: _ZN4aria7backend10TernaryOps15subtractTrytesEtt
    subTrytesFunc = Function::Create(
        funcType,
        Function::ExternalLinkage,
        "_ZN4aria7backend10TernaryOps15subtractTrytesEtt",
        module
    );
    
    return subTrytesFunc;
}

Function* TernaryLowerer::getMulTrytesFunc() {
    if (mulTrytesFunc) {
        return mulTrytesFunc;
    }
    
    Type* i16Type = Type::getInt16Ty(llvmContext);
    FunctionType* funcType = FunctionType::get(
        i16Type,
        {i16Type, i16Type},
        false
    );
    
    // Mangled: _ZN4aria7backend10TernaryOps14multiplyTrytesEtt
    mulTrytesFunc = Function::Create(
        funcType,
        Function::ExternalLinkage,
        "_ZN4aria7backend10TernaryOps14multiplyTrytesEtt",
        module
    );
    
    return mulTrytesFunc;
}

Function* TernaryLowerer::getDivTrytesFunc() {
    if (divTrytesFunc) {
        return divTrytesFunc;
    }
    
    Type* i16Type = Type::getInt16Ty(llvmContext);
    FunctionType* funcType = FunctionType::get(
        i16Type,
        {i16Type, i16Type},
        false
    );
    
    // Mangled: _ZN4aria7backend10TernaryOps12divideTrytesEtt
    divTrytesFunc = Function::Create(
        funcType,
        Function::ExternalLinkage,
        "_ZN4aria7backend10TernaryOps12divideTrytesEtt",
        module
    );
    
    return divTrytesFunc;
}

Function* TernaryLowerer::getNegateTryteFunc() {
    if (negateTryteFunc) {
        return negateTryteFunc;
    }
    
    Type* i16Type = Type::getInt16Ty(llvmContext);
    FunctionType* funcType = FunctionType::get(
        i16Type,
        {i16Type},
        false
    );
    
    // Mangled: _ZN4aria7backend10TernaryOps11negateTryteEt
    negateTryteFunc = Function::Create(
        funcType,
        Function::ExternalLinkage,
        "_ZN4aria7backend10TernaryOps11negateTryteEt",
        module
    );
    
    return negateTryteFunc;
}

Function* TernaryLowerer::getBinaryToTryteFunc() {
    if (binaryToTryteFunc) {
        return binaryToTryteFunc;
    }
    
    Type* i16Type = Type::getInt16Ty(llvmContext);
    Type* i32Type = Type::getInt32Ty(llvmContext);
    FunctionType* funcType = FunctionType::get(
        i16Type,
        {i32Type},
        false
    );
    
    // Mangled: _ZN4aria7backend10TernaryOps13binaryToTryteEi
    binaryToTryteFunc = Function::Create(
        funcType,
        Function::ExternalLinkage,
        "_ZN4aria7backend10TernaryOps13binaryToTryteEi",
        module
    );
    
    return binaryToTryteFunc;
}

Function* TernaryLowerer::getTryteToBinaryFunc() {
    if (tryteToBinaryFunc) {
        return tryteToBinaryFunc;
    }
    
    Type* i16Type = Type::getInt16Ty(llvmContext);
    Type* i32Type = Type::getInt32Ty(llvmContext);
    FunctionType* funcType = FunctionType::get(
        i32Type,
        {i16Type},
        false
    );
    
    // Mangled: _ZN4aria7backend10TernaryOps13tryteToBinaryEt
    tryteToBinaryFunc = Function::Create(
        funcType,
        Function::ExternalLinkage,
        "_ZN4aria7backend10TernaryOps13tryteToBinaryEt",
        module
    );
    
    return tryteToBinaryFunc;
}

Function* TernaryLowerer::getInitializeFunc() {
    if (initializeFunc) {
        return initializeFunc;
    }
    
    Type* voidType = Type::getVoidTy(llvmContext);
    FunctionType* funcType = FunctionType::get(
        voidType,
        {},  // no parameters
        false
    );
    
    // Mangled: _ZN4aria7backend10TernaryOps10initializeEv
    initializeFunc = Function::Create(
        funcType,
        Function::ExternalLinkage,
        "_ZN4aria7backend10TernaryOps10initializeEv",
        module
    );
    
    return initializeFunc;
}

} // namespace backend
} // namespace aria

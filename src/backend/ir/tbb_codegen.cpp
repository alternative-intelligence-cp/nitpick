#include "backend/ir/tbb_codegen.h"
#include <stdexcept>

namespace aria {

using namespace aria::sema;

TBBCodegen::TBBCodegen(llvm::LLVMContext& ctx, llvm::IRBuilder<>& bldr)
    : context(ctx), builder(bldr) {}

unsigned TBBCodegen::getTBBBitWidth(Type* type) {
    if (type->getKind() != TypeKind::PRIMITIVE) {
        throw std::runtime_error("getTBBBitWidth called on non-primitive type");
    }

    auto* primType = static_cast<PrimitiveType*>(type);
    const std::string& name = primType->getName();

    if (name == "tbb8") return 8;
    if (name == "tbb16") return 16;
    if (name == "tbb32") return 32;
    if (name == "tbb64") return 64;

    throw std::runtime_error("getTBBBitWidth called on non-TBB type: " + name);
}

llvm::IntegerType* TBBCodegen::getTBBLLVMType(Type* type) {
    unsigned bitWidth = getTBBBitWidth(type);
    return llvm::IntegerType::get(context, bitWidth);
}

llvm::Value* TBBCodegen::getErrSentinel(Type* type) {
    unsigned bitWidth = getTBBBitWidth(type);
    llvm::IntegerType* llvmType = getTBBLLVMType(type);

    // ERR sentinel is the minimum value of the signed type:
    // tbb8:  -128 (0x80)
    // tbb16: -32768 (0x8000)
    // tbb32: -2147483648 (0x80000000)
    // tbb64: -9223372036854775808 (0x8000000000000000)
    int64_t errValue;
    switch (bitWidth) {
        case 8:  errValue = -128; break;
        case 16: errValue = -32768; break;
        case 32: errValue = -2147483648LL; break;
        case 64: errValue = INT64_MIN; break;
        default: throw std::runtime_error("Invalid TBB bit width");
    }

    return llvm::ConstantInt::get(llvmType, errValue, true);
}

llvm::Value* TBBCodegen::getMaxValue(Type* type) {
    unsigned bitWidth = getTBBBitWidth(type);
    llvm::IntegerType* llvmType = getTBBLLVMType(type);

    // Max valid value (ERR - 1):
    // tbb8:  +127
    // tbb16: +32767
    // tbb32: +2147483647
    // tbb64: +9223372036854775807
    int64_t maxValue;
    switch (bitWidth) {
        case 8:  maxValue = 127; break;
        case 16: maxValue = 32767; break;
        case 32: maxValue = 2147483647LL; break;
        case 64: maxValue = INT64_MAX; break;
        default: throw std::runtime_error("Invalid TBB bit width");
    }

    return llvm::ConstantInt::get(llvmType, maxValue, true);
}

llvm::Value* TBBCodegen::getMinValue(Type* type) {
    unsigned bitWidth = getTBBBitWidth(type);
    llvm::IntegerType* llvmType = getTBBLLVMType(type);

    // Min valid value (NOT ERR, but lowest valid number):
    // tbb8:  -127
    // tbb16: -32767
    // tbb32: -2147483647
    // tbb64: -9223372036854775807
    int64_t minValue;
    switch (bitWidth) {
        case 8:  minValue = -127; break;
        case 16: minValue = -32767; break;
        case 32: minValue = -2147483647LL; break;
        case 64: minValue = -INT64_MAX; break;
        default: throw std::runtime_error("Invalid TBB bit width");
    }

    return llvm::ConstantInt::get(llvmType, minValue, true);
}

llvm::Value* TBBCodegen::isErr(llvm::Value* value, Type* type) {
    llvm::Value* errSentinel = getErrSentinel(type);
    return builder.CreateICmpEQ(value, errSentinel, "is_err");
}

llvm::Value* TBBCodegen::checkAddOverflow(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    llvm::Value* maxVal = getMaxValue(type);
    llvm::Value* minVal = getMinValue(type);

    // For addition overflow detection:
    // If lhs > 0 and rhs > 0 and lhs > max - rhs: overflow
    // If lhs < 0 and rhs < 0 and lhs < min - rhs: underflow

    llvm::Value* zero = llvm::ConstantInt::get(getTBBLLVMType(type), 0, true);

    llvm::Value* lhsPositive = builder.CreateICmpSGT(lhs, zero, "lhs_pos");
    llvm::Value* rhsPositive = builder.CreateICmpSGT(rhs, zero, "rhs_pos");
    llvm::Value* bothPositive = builder.CreateAnd(lhsPositive, rhsPositive, "both_pos");

    llvm::Value* maxMinusRhs = builder.CreateSub(maxVal, rhs, "max_minus_rhs");
    llvm::Value* overflowPos = builder.CreateICmpSGT(lhs, maxMinusRhs, "overflow_pos");
    llvm::Value* willOverflow = builder.CreateAnd(bothPositive, overflowPos, "will_overflow");

    llvm::Value* lhsNegative = builder.CreateICmpSLT(lhs, zero, "lhs_neg");
    llvm::Value* rhsNegative = builder.CreateICmpSLT(rhs, zero, "rhs_neg");
    llvm::Value* bothNegative = builder.CreateAnd(lhsNegative, rhsNegative, "both_neg");

    llvm::Value* minMinusRhs = builder.CreateSub(minVal, rhs, "min_minus_rhs");
    llvm::Value* underflowNeg = builder.CreateICmpSLT(lhs, minMinusRhs, "underflow_neg");
    llvm::Value* willUnderflow = builder.CreateAnd(bothNegative, underflowNeg, "will_underflow");

    return builder.CreateOr(willOverflow, willUnderflow, "overflow");
}

llvm::Value* TBBCodegen::checkSubOverflow(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    llvm::Value* maxVal = getMaxValue(type);
    llvm::Value* minVal = getMinValue(type);

    // For subtraction overflow detection:
    // lhs - rhs can overflow if lhs > 0, rhs < 0, and lhs > max + rhs
    // lhs - rhs can underflow if lhs < 0, rhs > 0, and lhs < min + rhs

    llvm::Value* zero = llvm::ConstantInt::get(getTBBLLVMType(type), 0, true);

    llvm::Value* lhsPositive = builder.CreateICmpSGT(lhs, zero, "lhs_pos");
    llvm::Value* rhsNegative = builder.CreateICmpSLT(rhs, zero, "rhs_neg");
    llvm::Value* posMinusNeg = builder.CreateAnd(lhsPositive, rhsNegative, "pos_minus_neg");

    llvm::Value* maxPlusRhs = builder.CreateAdd(maxVal, rhs, "max_plus_rhs");
    llvm::Value* overflowSub = builder.CreateICmpSGT(lhs, maxPlusRhs, "overflow_sub");
    llvm::Value* willOverflow = builder.CreateAnd(posMinusNeg, overflowSub, "will_overflow");

    llvm::Value* lhsNegative = builder.CreateICmpSLT(lhs, zero, "lhs_neg");
    llvm::Value* rhsPositive = builder.CreateICmpSGT(rhs, zero, "rhs_pos");
    llvm::Value* negMinusPos = builder.CreateAnd(lhsNegative, rhsPositive, "neg_minus_pos");

    llvm::Value* minPlusRhs = builder.CreateAdd(minVal, rhs, "min_plus_rhs");
    llvm::Value* underflowSub = builder.CreateICmpSLT(lhs, minPlusRhs, "underflow_sub");
    llvm::Value* willUnderflow = builder.CreateAnd(negMinusPos, underflowSub, "will_underflow");

    return builder.CreateOr(willOverflow, willUnderflow, "overflow");
}

llvm::Value* TBBCodegen::checkMulOverflow(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    llvm::Value* maxVal = getMaxValue(type);
    llvm::Value* minVal = getMinValue(type);
    llvm::Value* zero = llvm::ConstantInt::get(getTBBLLVMType(type), 0, true);

    // Multiplication overflow is more complex
    // We use the LLVM overflow intrinsics for simplicity
    unsigned bitWidth = getTBBBitWidth(type);
    llvm::Type* overflowStructType = llvm::StructType::get(
        context,
        {getTBBLLVMType(type), llvm::IntegerType::get(context, 1)}
    );

    llvm::Function* smulFunc = llvm::Intrinsic::getDeclaration(
        builder.GetInsertBlock()->getModule(),
        llvm::Intrinsic::smul_with_overflow,
        {getTBBLLVMType(type)}
    );

    llvm::Value* mulResult = builder.CreateCall(smulFunc, {lhs, rhs}, "mul_overflow");
    llvm::Value* overflow = builder.CreateExtractValue(mulResult, 1, "overflow_bit");

    // Also need to check if result exceeds our TBB range (not just signed overflow)
    llvm::Value* result = builder.CreateExtractValue(mulResult, 0, "mul_result");
    llvm::Value* exceedsMax = builder.CreateICmpSGT(result, maxVal, "exceeds_max");
    llvm::Value* belowMin = builder.CreateICmpSLT(result, minVal, "below_min");
    llvm::Value* outOfRange = builder.CreateOr(exceedsMax, belowMin, "out_of_range");

    return builder.CreateOr(overflow, outOfRange, "mul_overflow");
}

llvm::Value* TBBCodegen::generateAdd(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();

    // Create basic blocks for control flow
    llvm::BasicBlock* checkLhsErrBB = llvm::BasicBlock::Create(context, "check_lhs_err", currentFunc);
    llvm::BasicBlock* checkRhsErrBB = llvm::BasicBlock::Create(context, "check_rhs_err", currentFunc);
    llvm::BasicBlock* checkOverflowBB = llvm::BasicBlock::Create(context, "check_overflow", currentFunc);
    llvm::BasicBlock* doAddBB = llvm::BasicBlock::Create(context, "do_add", currentFunc);
    llvm::BasicBlock* returnErrBB = llvm::BasicBlock::Create(context, "return_err", currentFunc);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "merge", currentFunc);

    // Check if lhs is ERR
    builder.CreateBr(checkLhsErrBB);

    builder.SetInsertPoint(checkLhsErrBB);
    llvm::Value* lhsIsErr = isErr(lhs, type);
    builder.CreateCondBr(lhsIsErr, returnErrBB, checkRhsErrBB);

    // Check if rhs is ERR
    builder.SetInsertPoint(checkRhsErrBB);
    llvm::Value* rhsIsErr = isErr(rhs, type);
    builder.CreateCondBr(rhsIsErr, returnErrBB, checkOverflowBB);

    // Check for overflow
    builder.SetInsertPoint(checkOverflowBB);
    llvm::Value* willOverflow = checkAddOverflow(lhs, rhs, type);
    builder.CreateCondBr(willOverflow, returnErrBB, doAddBB);

    // Perform addition
    builder.SetInsertPoint(doAddBB);
    llvm::Value* result = builder.CreateAdd(lhs, rhs, "add_result");
    builder.CreateBr(mergeBB);

    // Return ERR sentinel
    builder.SetInsertPoint(returnErrBB);
    llvm::Value* errSentinel = getErrSentinel(type);
    builder.CreateBr(mergeBB);

    // Merge results
    builder.SetInsertPoint(mergeBB);
    llvm::PHINode* phi = builder.CreatePHI(getTBBLLVMType(type), 2, "add_phi");
    phi->addIncoming(result, doAddBB);
    phi->addIncoming(errSentinel, returnErrBB);

    return phi;
}

llvm::Value* TBBCodegen::generateSub(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();

    llvm::BasicBlock* checkLhsErrBB = llvm::BasicBlock::Create(context, "check_lhs_err", currentFunc);
    llvm::BasicBlock* checkRhsErrBB = llvm::BasicBlock::Create(context, "check_rhs_err", currentFunc);
    llvm::BasicBlock* checkOverflowBB = llvm::BasicBlock::Create(context, "check_overflow", currentFunc);
    llvm::BasicBlock* doSubBB = llvm::BasicBlock::Create(context, "do_sub", currentFunc);
    llvm::BasicBlock* returnErrBB = llvm::BasicBlock::Create(context, "return_err", currentFunc);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "merge", currentFunc);

    builder.CreateBr(checkLhsErrBB);

    builder.SetInsertPoint(checkLhsErrBB);
    llvm::Value* lhsIsErr = isErr(lhs, type);
    builder.CreateCondBr(lhsIsErr, returnErrBB, checkRhsErrBB);

    builder.SetInsertPoint(checkRhsErrBB);
    llvm::Value* rhsIsErr = isErr(rhs, type);
    builder.CreateCondBr(rhsIsErr, returnErrBB, checkOverflowBB);

    builder.SetInsertPoint(checkOverflowBB);
    llvm::Value* willOverflow = checkSubOverflow(lhs, rhs, type);
    builder.CreateCondBr(willOverflow, returnErrBB, doSubBB);

    builder.SetInsertPoint(doSubBB);
    llvm::Value* result = builder.CreateSub(lhs, rhs, "sub_result");
    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(returnErrBB);
    llvm::Value* errSentinel = getErrSentinel(type);
    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(mergeBB);
    llvm::PHINode* phi = builder.CreatePHI(getTBBLLVMType(type), 2, "sub_phi");
    phi->addIncoming(result, doSubBB);
    phi->addIncoming(errSentinel, returnErrBB);

    return phi;
}

llvm::Value* TBBCodegen::generateMul(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();

    llvm::BasicBlock* checkLhsErrBB = llvm::BasicBlock::Create(context, "check_lhs_err", currentFunc);
    llvm::BasicBlock* checkRhsErrBB = llvm::BasicBlock::Create(context, "check_rhs_err", currentFunc);
    llvm::BasicBlock* checkOverflowBB = llvm::BasicBlock::Create(context, "check_overflow", currentFunc);
    llvm::BasicBlock* doMulBB = llvm::BasicBlock::Create(context, "do_mul", currentFunc);
    llvm::BasicBlock* returnErrBB = llvm::BasicBlock::Create(context, "return_err", currentFunc);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "merge", currentFunc);

    builder.CreateBr(checkLhsErrBB);

    builder.SetInsertPoint(checkLhsErrBB);
    llvm::Value* lhsIsErr = isErr(lhs, type);
    builder.CreateCondBr(lhsIsErr, returnErrBB, checkRhsErrBB);

    builder.SetInsertPoint(checkRhsErrBB);
    llvm::Value* rhsIsErr = isErr(rhs, type);
    builder.CreateCondBr(rhsIsErr, returnErrBB, checkOverflowBB);

    builder.SetInsertPoint(checkOverflowBB);
    llvm::Value* willOverflow = checkMulOverflow(lhs, rhs, type);
    builder.CreateCondBr(willOverflow, returnErrBB, doMulBB);

    builder.SetInsertPoint(doMulBB);
    llvm::Value* result = builder.CreateMul(lhs, rhs, "mul_result");
    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(returnErrBB);
    llvm::Value* errSentinel = getErrSentinel(type);
    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(mergeBB);
    llvm::PHINode* phi = builder.CreatePHI(getTBBLLVMType(type), 2, "mul_phi");
    phi->addIncoming(result, doMulBB);
    phi->addIncoming(errSentinel, returnErrBB);

    return phi;
}

llvm::Value* TBBCodegen::generateDiv(llvm::Value* lhs, llvm::Value* rhs, Type* type) {
    llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();

    llvm::BasicBlock* checkLhsErrBB = llvm::BasicBlock::Create(context, "check_lhs_err", currentFunc);
    llvm::BasicBlock* checkRhsErrBB = llvm::BasicBlock::Create(context, "check_rhs_err", currentFunc);
    llvm::BasicBlock* checkDivZeroBB = llvm::BasicBlock::Create(context, "check_div_zero", currentFunc);
    llvm::BasicBlock* doDivBB = llvm::BasicBlock::Create(context, "do_div", currentFunc);
    llvm::BasicBlock* returnErrBB = llvm::BasicBlock::Create(context, "return_err", currentFunc);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "merge", currentFunc);

    builder.CreateBr(checkLhsErrBB);

    builder.SetInsertPoint(checkLhsErrBB);
    llvm::Value* lhsIsErr = isErr(lhs, type);
    builder.CreateCondBr(lhsIsErr, returnErrBB, checkRhsErrBB);

    builder.SetInsertPoint(checkRhsErrBB);
    llvm::Value* rhsIsErr = isErr(rhs, type);
    builder.CreateCondBr(rhsIsErr, returnErrBB, checkDivZeroBB);

    builder.SetInsertPoint(checkDivZeroBB);
    llvm::Value* zero = llvm::ConstantInt::get(getTBBLLVMType(type), 0, true);
    llvm::Value* isZero = builder.CreateICmpEQ(rhs, zero, "is_zero");
    builder.CreateCondBr(isZero, returnErrBB, doDivBB);

    builder.SetInsertPoint(doDivBB);
    llvm::Value* result = builder.CreateSDiv(lhs, rhs, "div_result");
    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(returnErrBB);
    llvm::Value* errSentinel = getErrSentinel(type);
    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(mergeBB);
    llvm::PHINode* phi = builder.CreatePHI(getTBBLLVMType(type), 2, "div_phi");
    phi->addIncoming(result, doDivBB);
    phi->addIncoming(errSentinel, returnErrBB);

    return phi;
}

llvm::Value* TBBCodegen::generateNeg(llvm::Value* operand, Type* type) {
    llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();

    llvm::BasicBlock* checkErrBB = llvm::BasicBlock::Create(context, "check_err", currentFunc);
    llvm::BasicBlock* checkMinBB = llvm::BasicBlock::Create(context, "check_min", currentFunc);
    llvm::BasicBlock* doNegBB = llvm::BasicBlock::Create(context, "do_neg", currentFunc);
    llvm::BasicBlock* returnErrBB = llvm::BasicBlock::Create(context, "return_err", currentFunc);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "merge", currentFunc);

    builder.CreateBr(checkErrBB);

    builder.SetInsertPoint(checkErrBB);
    llvm::Value* operandIsErr = isErr(operand, type);
    builder.CreateCondBr(operandIsErr, returnErrBB, checkMinBB);

    // Negating the minimum valid value would overflow
    builder.SetInsertPoint(checkMinBB);
    llvm::Value* minVal = getMinValue(type);
    llvm::Value* isMin = builder.CreateICmpEQ(operand, minVal, "is_min");
    builder.CreateCondBr(isMin, returnErrBB, doNegBB);

    builder.SetInsertPoint(doNegBB);
    llvm::Value* result = builder.CreateNeg(operand, "neg_result");
    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(returnErrBB);
    llvm::Value* errSentinel = getErrSentinel(type);
    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(mergeBB);
    llvm::PHINode* phi = builder.CreatePHI(getTBBLLVMType(type), 2, "neg_phi");
    phi->addIncoming(result, doNegBB);
    phi->addIncoming(errSentinel, returnErrBB);

    return phi;
}

} // namespace aria

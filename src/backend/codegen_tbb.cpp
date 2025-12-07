#include "codegen_tbb.h"
#include "codegen.h"
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Constants.h>

namespace aria {
namespace backend {

using namespace llvm;

TBBLowerer::TBBLowerer(CodeGenContext& c) : ctx(c) {}

bool TBBLowerer::isTBBType(const std::string& typeName) {
    return typeName == "tbb8" || 
           typeName == "tbb16" || 
           typeName == "tbb32" || 
           typeName == "tbb64";
}

Value* TBBLowerer::getSentinel(Type* type) {
    if (!type->isIntegerTy()) {
        return nullptr;
    }
    
    unsigned width = type->getIntegerBitWidth();
    // Sentinel = minimum signed value for this bit width
    // For i8: -128 (0x80)
    // For i16: -32768 (0x8000)
    // For i32: -2147483648 (0x80000000)
    // For i64: -9223372036854775808 (0x8000000000000000)
    return ConstantInt::get(ctx.llvmContext, APInt::getSignedMinValue(width));
}

Value* TBBLowerer::createAdd(Value* lhs, Value* rhs) {
    return createOp(0, lhs, rhs);
}

Value* TBBLowerer::createSub(Value* lhs, Value* rhs) {
    return createOp(1, lhs, rhs);
}

Value* TBBLowerer::createMul(Value* lhs, Value* rhs) {
    return createOp(2, lhs, rhs);
}

Value* TBBLowerer::createOp(unsigned opCode, Value* lhs, Value* rhs) {
    Type* type = lhs->getType();
    Value* sentinel = getSentinel(type);

    // STEP 1: Sticky Input Check
    // If either input is ERR, result must be ERR (no computation needed)
    Value* lhsIsErr = ctx.builder->CreateICmpEQ(lhs, sentinel, "lhs_is_err");
    Value* rhsIsErr = ctx.builder->CreateICmpEQ(rhs, sentinel, "rhs_is_err");
    Value* inputErr = ctx.builder->CreateOr(lhsIsErr, rhsIsErr, "input_err");

    // STEP 2: Perform Operation with Overflow Detection
    Value* rawResult = nullptr;
    Value* overflow = nullptr;
    Intrinsic::ID intrinsicID;

    switch (opCode) {
        case 0: intrinsicID = Intrinsic::sadd_with_overflow; break;
        case 1: intrinsicID = Intrinsic::ssub_with_overflow; break;
        case 2: intrinsicID = Intrinsic::smul_with_overflow; break;
        default: 
            // Fallback: return sentinel for unknown operation
            return sentinel;
    }

    // Get the LLVM intrinsic function
    Function* intrinsic = Intrinsic::getDeclaration(
        ctx.module.get(), 
        intrinsicID, 
        {type}
    );

    // Call intrinsic: returns {result, overflow_flag}
    Value* resultStruct = ctx.builder->CreateCall(intrinsic, {lhs, rhs});
    rawResult = ctx.builder->CreateExtractValue(resultStruct, 0, "raw_result");
    overflow = ctx.builder->CreateExtractValue(resultStruct, 1, "overflow");

    // STEP 3: Result Sentinel Check
    // Even if overflow didn't occur, if the calculated result happens to equal
    // the sentinel bit pattern, it represents ERR in TBB semantics.
    // Example: For tbb8, if 100 + 28 = 128, but 128 doesn't fit in signed i8,
    // the bit pattern is 0x80 which is -128 (the sentinel).
    Value* resultIsSentinel = ctx.builder->CreateICmpEQ(
        rawResult, 
        sentinel, 
        "result_is_sentinel"
    );

    // STEP 4: Combine All Error Conditions
    // Result is ERR if:
    //   - Either input was ERR (sticky)
    //   - Overflow occurred
    //   - Result bit pattern equals sentinel
    Value* anyError = ctx.builder->CreateOr(inputErr, overflow, "has_overflow");
    anyError = ctx.builder->CreateOr(anyError, resultIsSentinel, "any_error");

    // STEP 5: Select Final Result
    // If any error condition is true, return sentinel; otherwise return raw result
    return ctx.builder->CreateSelect(
        anyError, 
        sentinel, 
        rawResult, 
        "tbb_result"
    );
}

Value* TBBLowerer::createDiv(Value* lhs, Value* rhs) {
    Type* type = lhs->getType();
    Value* sentinel = getSentinel(type);

    // STEP 1: Check Input Sticky Errors
    Value* lhsIsErr = ctx.builder->CreateICmpEQ(lhs, sentinel, "lhs_is_err");
    Value* rhsIsErr = ctx.builder->CreateICmpEQ(rhs, sentinel, "rhs_is_err");
    Value* inputErr = ctx.builder->CreateOr(lhsIsErr, rhsIsErr, "input_err");

    // STEP 2: Check Division by Zero
    Value* zero = ConstantInt::get(type, 0);
    Value* divByZero = ctx.builder->CreateICmpEQ(rhs, zero, "div_by_zero");

    // STEP 3: Check Overflow Case (ERR / -1)
    // The only signed division overflow is: MIN_INT / -1 = MAX_INT + 1
    // For TBB, MIN_INT is the sentinel, so ERR / -1 must remain ERR
    Value* minusOne = ConstantInt::get(type, -1, true);
    Value* rhsIsMinusOne = ctx.builder->CreateICmpEQ(rhs, minusOne, "rhs_is_minus_one");
    Value* lhsIsSentinel = ctx.builder->CreateICmpEQ(lhs, sentinel, "lhs_is_sentinel");
    Value* overflowCase = ctx.builder->CreateAnd(
        lhsIsSentinel, 
        rhsIsMinusOne, 
        "overflow_case"
    );

    // STEP 4: Perform Safe Division
    // To avoid CPU traps, use a safe divisor (1) when error is detected
    Value* hasUnsafeDiv = ctx.builder->CreateOr(divByZero, overflowCase, "unsafe_div");
    Value* safeDivisor = ctx.builder->CreateSelect(
        hasUnsafeDiv, 
        ConstantInt::get(type, 1), 
        rhs, 
        "safe_divisor"
    );

    // Perform the actual division with the safe divisor
    Value* rawResult = ctx.builder->CreateSDiv(lhs, safeDivisor, "raw_div");

    // STEP 5: Check if Result is Sentinel (edge case coverage)
    Value* resultIsSentinel = ctx.builder->CreateICmpEQ(
        rawResult, 
        sentinel, 
        "result_is_sentinel"
    );

    // STEP 6: Combine All Error Conditions
    Value* totalError = ctx.builder->CreateOr(inputErr, hasUnsafeDiv, "has_error");
    totalError = ctx.builder->CreateOr(totalError, resultIsSentinel, "total_error");

    // STEP 7: Select Final Result
    return ctx.builder->CreateSelect(
        totalError, 
        sentinel, 
        rawResult, 
        "tbb_div"
    );
}

Value* TBBLowerer::createMod(Value* lhs, Value* rhs) {
    Type* type = lhs->getType();
    Value* sentinel = getSentinel(type);

    // Similar logic to division
    Value* lhsIsErr = ctx.builder->CreateICmpEQ(lhs, sentinel);
    Value* rhsIsErr = ctx.builder->CreateICmpEQ(rhs, sentinel);
    Value* inputErr = ctx.builder->CreateOr(lhsIsErr, rhsIsErr);

    Value* zero = ConstantInt::get(type, 0);
    Value* modByZero = ctx.builder->CreateICmpEQ(rhs, zero);

    // Overflow case: ERR % -1 = ERR
    Value* minusOne = ConstantInt::get(type, -1, true);
    Value* rhsIsMinusOne = ctx.builder->CreateICmpEQ(rhs, minusOne);
    Value* lhsIsSentinel = ctx.builder->CreateICmpEQ(lhs, sentinel);
    Value* overflowCase = ctx.builder->CreateAnd(lhsIsSentinel, rhsIsMinusOne);

    Value* hasUnsafeMod = ctx.builder->CreateOr(modByZero, overflowCase);
    Value* safeDivisor = ctx.builder->CreateSelect(
        hasUnsafeMod,
        ConstantInt::get(type, 1),
        rhs
    );

    Value* rawResult = ctx.builder->CreateSRem(lhs, safeDivisor);
    Value* resultIsSentinel = ctx.builder->CreateICmpEQ(rawResult, sentinel);

    Value* totalError = ctx.builder->CreateOr(inputErr, hasUnsafeMod);
    totalError = ctx.builder->CreateOr(totalError, resultIsSentinel);

    return ctx.builder->CreateSelect(totalError, sentinel, rawResult, "tbb_mod");
}

Value* TBBLowerer::createNeg(Value* operand) {
    Type* type = operand->getType();
    Value* sentinel = getSentinel(type);

    // Check if input is already ERR
    Value* inputIsErr = ctx.builder->CreateICmpEQ(operand, sentinel, "input_is_err");

    // Perform negation: -x
    Value* zero = ConstantInt::get(type, 0);
    Value* rawResult = ctx.builder->CreateSub(zero, operand, "raw_neg");

    // Check for overflow: -MIN_INT overflows to MIN_INT (the sentinel)
    // Use intrinsic for safety
    Function* intrinsic = Intrinsic::getDeclaration(
        ctx.module.get(),
        Intrinsic::ssub_with_overflow,
        {type}
    );
    Value* resultStruct = ctx.builder->CreateCall(intrinsic, {zero, operand});
    Value* overflow = ctx.builder->CreateExtractValue(resultStruct, 1);

    // Check if result is sentinel
    Value* resultIsSentinel = ctx.builder->CreateICmpEQ(rawResult, sentinel);

    // Combine error conditions
    Value* anyError = ctx.builder->CreateOr(inputIsErr, overflow);
    anyError = ctx.builder->CreateOr(anyError, resultIsSentinel);

    return ctx.builder->CreateSelect(anyError, sentinel, rawResult, "tbb_neg");
}

} // namespace backend
} // namespace aria

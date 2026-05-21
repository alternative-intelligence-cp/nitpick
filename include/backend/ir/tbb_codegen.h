#ifndef ARIA_TBB_CODEGEN_H
#define ARIA_TBB_CODEGEN_H

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include "frontend/sema/type.h"

namespace npk {

using Type = npk::sema::Type;

/**
 * @brief TBBCodegen - Safe Balanced Ternary arithmetic code generation
 * 
 * Generates LLVM IR for TBB (Ternary Balanced Binary) types with:
 * - ERR sentinel detection (min value of each type)
 * - Overflow checking (result > max or < -max)
 * - Sticky ERR propagation (if input is ERR, output is ERR)
 * 
 * TBB Ranges (from research_002):
 * - tbb8:  range [-127, +127],     ERR = -128 (0x80)
 * - tbb16: range [-32767, +32767], ERR = -32768 (0x8000)
 * - tbb32: range [-2147483647, +2147483647], ERR = -2147483648 (0x80000000)
 * - tbb64: range [-9223372036854775807, +9223372036854775807], ERR = -9223372036854775808 (0x8000000000000000)
 */
class TBBCodegen {
public:
    TBBCodegen(llvm::LLVMContext& context, llvm::IRBuilder<>& builder);

    /**
     * @brief Generate safe TBB addition with overflow and ERR checking
     * @param lhs Left operand (tbb type)
     * @param rhs Right operand (tbb type)
     * @param type The TBB type (tbb8/16/32/64)
     * @return LLVM Value representing the result or ERR sentinel
     */
    llvm::Value* generateAdd(llvm::Value* lhs, llvm::Value* rhs, Type* type);

    /**
     * @brief Generate safe TBB subtraction with overflow and ERR checking
     */
    llvm::Value* generateSub(llvm::Value* lhs, llvm::Value* rhs, Type* type);

    /**
     * @brief Generate safe TBB multiplication with overflow and ERR checking
     */
    llvm::Value* generateMul(llvm::Value* lhs, llvm::Value* rhs, Type* type);

    /**
     * @brief Generate safe TBB division with ERR checking
     */
    llvm::Value* generateDiv(llvm::Value* lhs, llvm::Value* rhs, Type* type);

    /**
     * @brief Generate safe TBB modulo with ERR checking
     */
    llvm::Value* generateMod(llvm::Value* lhs, llvm::Value* rhs, Type* type);

    /**
     * @brief Generate safe TBB negation
     */
    llvm::Value* generateNeg(llvm::Value* operand, Type* type);

    /**
     * @brief Generate sentinel-preserving TBB widening (ARIA-018)
     *
     * Standard sign extension would convert source ERR to a valid value:
     *   tbb8 ERR (-128) → sext → -128 in tbb16 (valid, not ERR!)
     *
     * This generates:
     *   isErr = (src == srcSentinel)
     *   widened = sext(src)
     *   result = select(isErr, dstSentinel, widened)
     *
     * @param srcVal Source TBB value to widen
     * @param srcType Source TBB type (tbb8/16/32)
     * @param dstType Destination TBB type (tbb16/32/64)
     * @return Widened value with sentinel preservation
     */
    llvm::Value* generateWiden(llvm::Value* srcVal, Type* srcType, Type* dstType);

    /**
     * @brief Generate ERR-sticky TBB comparison (v0.31.2.3 D-16/16a).
     *
     * D-16a semantics: when either operand is the ERR sentinel, the bool
     * result follows the rule "ERR is unequal to everything including
     * itself" — `==`, `<`, `<=`, `>`, `>=` evaluate to false; `!=`
     * evaluates to true. Otherwise the integer ICmp with `pred` is used.
     *
     * @param lhs Left operand (tbb type)
     * @param rhs Right operand (tbb type)
     * @param type The TBB type (tbb8/16/32/64)
     * @param pred LLVM ICmp predicate (signed flavour for ordered ops)
     * @return LLVM i1 value with ERR-sticky semantics
     */
    llvm::Value* generateCmp(llvm::Value* lhs, llvm::Value* rhs, Type* type,
                             llvm::CmpInst::Predicate pred);

    // ========================================================================
    // Public helpers for testing
    // ========================================================================

    /**
     * @brief Get ERR sentinel value for the given TBB type
     * @param type TBB type (tbb8/16/32/64)
     * @return LLVM constant representing the ERR sentinel
     */
    llvm::Value* getErrSentinel(Type* type);

    /**
     * @brief Get maximum valid value for the given TBB type
     * @param type TBB type (tbb8/16/32/64)
     * @return LLVM constant representing the max value
     */
    llvm::Value* getMaxValue(Type* type);

    /**
     * @brief Get minimum valid value for the given TBB type (NOT ERR, but the lowest valid value)
     * @param type TBB type (tbb8/16/32/64)
     * @return LLVM constant representing the min value
     */
    llvm::Value* getMinValue(Type* type);

    /**
     * @brief Get LLVM integer type for TBB type
     */
    llvm::IntegerType* getTBBLLVMType(Type* type);

private:
    llvm::LLVMContext& context;
    llvm::IRBuilder<>& builder;

    /**
     * @brief Check if value is ERR sentinel
     * @param value Value to check
     * @param type TBB type
     * @return LLVM i1 value (1 if ERR, 0 otherwise)
     */
    llvm::Value* isErr(llvm::Value* value, Type* type);

    /**
     * @brief Generate overflow checking for addition
     * Returns true if overflow would occur
     */
    llvm::Value* checkAddOverflow(llvm::Value* lhs, llvm::Value* rhs, Type* type);

    /**
     * @brief Generate overflow checking for subtraction
     * Returns true if overflow would occur
     */
    llvm::Value* checkSubOverflow(llvm::Value* lhs, llvm::Value* rhs, Type* type);

    /**
     * @brief Generate overflow checking for multiplication
     * Returns true if overflow would occur
     */
    llvm::Value* checkMulOverflow(llvm::Value* lhs, llvm::Value* rhs, Type* type);

    /**
     * @brief Get bit width for TBB type (8, 16, 32, or 64)
     */
    unsigned getTBBBitWidth(Type* type);
};

} // namespace npk

#endif // ARIA_TBB_CODEGEN_H

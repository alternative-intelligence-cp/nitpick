#ifndef ARIA_TBB_CODEGEN_H
#define ARIA_TBB_CODEGEN_H

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include "frontend/sema/type.h"

namespace aria {

using Type = aria::sema::Type;

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
     * @brief Generate safe TBB negation
     */
    llvm::Value* generateNeg(llvm::Value* operand, Type* type);

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
     * @brief Get LLVM integer type for TBB type
     */
    llvm::IntegerType* getTBBLLVMType(Type* type);

    /**
     * @brief Get bit width for TBB type (8, 16, 32, or 64)
     */
    unsigned getTBBBitWidth(Type* type);
};

} // namespace aria

#endif // ARIA_TBB_CODEGEN_H

#ifndef ARIA_TERNARY_CODEGEN_H
#define ARIA_TERNARY_CODEGEN_H

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include "frontend/sema/type.h"

namespace aria {

using Type = aria::sema::Type;

/**
 * @brief TernaryCodegen - Balanced ternary and nonary arithmetic code generation
 * 
 * Generates LLVM IR for balanced ternary/nonary types:
 * - trit:  Single balanced ternary digit (-1, 0, 1) in 2 bits
 * - tryte: 10 trits packed in 16 bits
 * - nit:   Single nonary digit (-4 to 4) in 4 bits  
 * - nyte:  5 nits packed in 16 bits
 * 
 * Balanced ternary uses symmetric ranges around zero, unlike standard binary.
 * Operations that exceed the valid range return the max/min boundary value.
 * 
 * Valid Ranges:
 * - trit:  [-1, +1]
 * - tryte: Implementation-defined (10 trits)
 * - nit:   [-4, +4]
 * - nyte:  Implementation-defined (5 nits)
 */
class TernaryCodegen {
public:
    TernaryCodegen(llvm::LLVMContext& context, llvm::IRBuilder<>& builder);

    /**
     * @brief Generate balanced ternary/nonary addition with range checking
     * @param lhs Left operand (trit/tryte/nit/nyte type)
     * @param rhs Right operand (same type)
     * @param type The ternary type
     * @return LLVM Value representing the result (clamped to valid range)
     */
    llvm::Value* generateAdd(llvm::Value* lhs, llvm::Value* rhs, Type* type);

    /**
     * @brief Generate balanced ternary/nonary subtraction with range checking
     */
    llvm::Value* generateSub(llvm::Value* lhs, llvm::Value* rhs, Type* type);

    /**
     * @brief Generate balanced ternary/nonary multiplication with range checking
     */
    llvm::Value* generateMul(llvm::Value* lhs, llvm::Value* rhs, Type* type);

    /**
     * @brief Generate balanced ternary/nonary division with range checking
     */
    llvm::Value* generateDiv(llvm::Value* lhs, llvm::Value* rhs, Type* type);

    /**
     * @brief Generate balanced ternary/nonary negation
     */
    llvm::Value* generateNeg(llvm::Value* operand, Type* type);

    // ========================================================================
    // Public helpers
    // ========================================================================

    /**
     * @brief Get maximum valid value for the given ternary type
     * @param type Ternary type (trit/tryte/nit/nyte)
     * @return LLVM constant representing the max value
     */
    llvm::Value* getMaxValue(Type* type);

    /**
     * @brief Get minimum valid value for the given ternary type
     * @param type Ternary type (trit/tryte/nit/nyte)
     * @return LLVM constant representing the min value
     */
    llvm::Value* getMinValue(Type* type);

    /**
     * @brief Get LLVM integer type for ternary type
     */
    llvm::IntegerType* getTernaryLLVMType(Type* type);

private:
    llvm::LLVMContext& context;
    llvm::IRBuilder<>& builder;

    /**
     * @brief Clamp value to valid range for the type
     * @param value Value to clamp
     * @param type Ternary type
     * @return Clamped value (min if too low, max if too high)
     */
    llvm::Value* clampToRange(llvm::Value* value, Type* type);
};

} // namespace aria

#endif // ARIA_TERNARY_CODEGEN_H

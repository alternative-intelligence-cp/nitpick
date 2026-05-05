#ifndef ARIA_TERNARY_CODEGEN_H
#define ARIA_TERNARY_CODEGEN_H

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include "frontend/sema/type.h"

namespace npk {

using Type = npk::sema::Type;

/**
 * @brief TernaryCodegen - Balanced ternary and nonary arithmetic code generation
 *
 * Generates LLVM IR for balanced ternary/nonary types:
 * - trit:  Single balanced ternary digit (-1, 0, 1) in 3 bits
 * - tryte: 10 trits packed in 16 bits (Split-Byte format)
 * - nit:   Single nonary digit (-4 to 4) in 4 bits
 * - nyte:  5 nits packed in 16 bits (Biased-Radix format)
 *
 * For atomic types (trit/nit): Uses inline LLVM arithmetic with clamping.
 * For composite types (tryte/nyte): Emits calls to runtime intrinsics from
 * ternary_ops.cpp for proper Split-Byte packing and LUT-based arithmetic.
 *
 * Runtime Intrinsics (ARIA-013):
 * - npk_tryte_add, npk_tryte_sub, npk_tryte_mul, npk_tryte_div, npk_tryte_neg
 * - npk_nyte_add, npk_nyte_sub, npk_nyte_mul, npk_nyte_div, npk_nyte_neg
 *
 * Valid Ranges:
 * - trit:  [-1, +1]
 * - tryte: [-29524, +29524] (3^10 - 1) / 2
 * - nit:   [-4, +4]
 * - nyte:  [-29524, +29524] (9^5 - 1) / 2
 */
class TernaryCodegen {
public:
    TernaryCodegen(llvm::LLVMContext& context, llvm::IRBuilder<>& builder);

    /**
     * @brief Set the module for declaring runtime intrinsics
     * Must be called before using generateXxx methods for tryte/nyte types.
     */
    void setModule(llvm::Module* mod) { module = mod; }

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
     * @brief Generate balanced ternary/nonary modulo with balanced remainder
     * Uses round-to-nearest division: r = a - (b * round(a/b))
     * This is different from truncating modulo used in standard arithmetic.
     */
    llvm::Value* generateMod(llvm::Value* lhs, llvm::Value* rhs, Type* type);

    /**
     * @brief Generate balanced ternary/nonary negation
     */
    llvm::Value* generateNeg(llvm::Value* operand, Type* type);

    /**
     * @brief Generate balanced ternary/nonary logical AND (Kleene logic)
     * Uses minimum semantics: AND(a, b) = min(a, b)
     */
    llvm::Value* generateAnd(llvm::Value* lhs, llvm::Value* rhs, Type* type);

    /**
     * @brief Generate balanced ternary/nonary logical OR (Kleene logic)
     * Uses maximum semantics: OR(a, b) = max(a, b)
     */
    llvm::Value* generateOr(llvm::Value* lhs, llvm::Value* rhs, Type* type);

    /**
     * @brief Generate balanced ternary/nonary logical NOT (Kleene logic)
     * Uses negation semantics: NOT(a) = -a
     */
    llvm::Value* generateNot(llvm::Value* operand, Type* type);

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
    llvm::Module* module = nullptr;

    // Cached runtime function pointers
    llvm::Function* fn_tryte_add = nullptr;
    llvm::Function* fn_tryte_sub = nullptr;
    llvm::Function* fn_tryte_mul = nullptr;
    llvm::Function* fn_tryte_div = nullptr;
    llvm::Function* fn_tryte_mod = nullptr;  // CRITICAL: Balanced modulo
    llvm::Function* fn_tryte_neg = nullptr;
    llvm::Function* fn_nyte_add = nullptr;
    llvm::Function* fn_nyte_sub = nullptr;
    llvm::Function* fn_nyte_mul = nullptr;
    llvm::Function* fn_nyte_div = nullptr;
    llvm::Function* fn_nyte_mod = nullptr;  // CRITICAL: Balanced modulo
    llvm::Function* fn_nyte_neg = nullptr;

    // Atomic type arithmetic runtime function pointers
    llvm::Function* fn_trit_add = nullptr;
    llvm::Function* fn_trit_sub = nullptr;
    llvm::Function* fn_trit_mul = nullptr;
    llvm::Function* fn_nit_add = nullptr;
    llvm::Function* fn_nit_sub = nullptr;
    llvm::Function* fn_nit_mul = nullptr;
    llvm::Function* fn_nit_neg = nullptr;

    // Kleene logic runtime function pointers
    llvm::Function* fn_trit_and = nullptr;
    llvm::Function* fn_trit_or = nullptr;
    llvm::Function* fn_trit_not = nullptr;
    llvm::Function* fn_nit_and = nullptr;
    llvm::Function* fn_nit_or = nullptr;
    llvm::Function* fn_nit_not = nullptr;

    /**
     * @brief Check if type is a composite ternary type (tryte/nyte)
     * Composite types require runtime intrinsic calls.
     */
    bool isCompositeType(Type* type) const;

    /**
     * @brief Get or declare a runtime intrinsic function
     * @param name Function name (e.g., "npk_tryte_add")
     * @param isBinaryOp True for binary ops, false for unary
     * @param isAtomic True for trit/nit (i8), false for tryte/nyte (i16)
     */
    llvm::Function* getOrDeclareIntrinsic(const std::string& name, bool isBinaryOp, bool isAtomic = false);

    /**
     * @brief Clamp value to valid range for the type (for trit/nit inline ops)
     * @param value Value to clamp
     * @param type Ternary type
     * @return Clamped value (min if too low, max if too high)
     */
    llvm::Value* clampToRange(llvm::Value* value, Type* type);
};

} // namespace npk

#endif // ARIA_TERNARY_CODEGEN_H

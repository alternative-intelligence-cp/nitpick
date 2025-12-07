#ifndef ARIA_CODEGEN_TBB_H
#define ARIA_CODEGEN_TBB_H

#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <string>

namespace aria {
namespace backend {

// Forward declaration
class CodeGenContext;

/**
 * TBBLowerer - Twisted Balanced Binary Type Safety Implementation
 * 
 * Implements sticky error propagation for TBB types (tbb8, tbb16, tbb32, tbb64).
 * 
 * CRITICAL REQUIREMENT:
 * The minimum signed value serves as the ERR sentinel:
 *   tbb8:  -128 (0x80)
 *   tbb16: -32768 (0x8000)
 *   tbb32: -2147483648 (0x80000000)
 *   tbb64: -9223372036854775808 (0x8000000000000000)
 * 
 * STICKY ERROR SEMANTICS:
 *   ERR + x = ERR
 *   x + ERR = ERR
 *   overflow(op) = ERR
 *   ERR cannot heal via wrapping
 * 
 * This class intercepts all arithmetic operations on TBB types and injects
 * LLVM intrinsics for overflow detection and sentinel checking.
 */
class TBBLowerer {
    CodeGenContext& ctx;

public:
    explicit TBBLowerer(CodeGenContext& c);

    /**
     * Check if a type name represents a TBB type.
     * @param typeName The Aria type name (e.g., "tbb8", "int8", "float64")
     * @return true if typeName is tbb8, tbb16, tbb32, or tbb64
     */
    static bool isTBBType(const std::string& typeName);

    /**
     * Get the ERR sentinel value for a given LLVM integer type.
     * Returns the minimum signed value for the bit width.
     * @param type LLVM integer type (i8, i16, i32, i64)
     * @return LLVM constant representing the sentinel value
     */
    llvm::Value* getSentinel(llvm::Type* type);

    /**
     * Create a safe addition operation with sticky error propagation.
     * Equivalent to: (lhs == ERR || rhs == ERR || overflow) ? ERR : lhs + rhs
     * 
     * Uses llvm.sadd.with.overflow intrinsic.
     * 
     * @param lhs Left operand (must be TBB type)
     * @param rhs Right operand (must be TBB type)
     * @return LLVM value representing the safe result
     */
    llvm::Value* createAdd(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create a safe subtraction operation with sticky error propagation.
     * Uses llvm.ssub.with.overflow intrinsic.
     */
    llvm::Value* createSub(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create a safe multiplication operation with sticky error propagation.
     * Uses llvm.smul.with.overflow intrinsic.
     */
    llvm::Value* createMul(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create a safe division operation with sticky error propagation.
     * 
     * Division has special edge cases:
     *   1. Division by zero → ERR
     *   2. ERR / -1 → ERR (would overflow to MAX+1)
     *   3. ERR / x → ERR (input sticky)
     *   4. x / ERR → ERR (input sticky)
     * 
     * @param lhs Dividend (must be TBB type)
     * @param rhs Divisor (must be TBB type)
     * @return LLVM value representing the safe result
     */
    llvm::Value* createDiv(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create a safe modulo operation with sticky error propagation.
     * Similar to division, but uses srem instruction.
     */
    llvm::Value* createMod(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create a safe negation operation.
     * Special case: -ERR = ERR (negating sentinel stays sentinel)
     * Note: -(MAX+1) would overflow, but MAX+1 is the sentinel, so it's already ERR.
     */
    llvm::Value* createNeg(llvm::Value* operand);

private:
    /**
     * Internal helper for arithmetic operations.
     * @param opCode 0=Add, 1=Sub, 2=Mul
     * @param lhs Left operand
     * @param rhs Right operand
     * @return LLVM value with sticky error propagation
     */
    llvm::Value* createOp(unsigned opCode, llvm::Value* lhs, llvm::Value* rhs);
};

} // namespace backend
} // namespace aria

#endif // ARIA_CODEGEN_TBB_H

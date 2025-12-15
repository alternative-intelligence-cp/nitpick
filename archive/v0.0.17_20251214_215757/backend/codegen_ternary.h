#ifndef ARIA_CODEGEN_TERNARY_H
#define ARIA_CODEGEN_TERNARY_H

#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <string>

namespace aria {
namespace backend {

/**
 * TernaryLowerer - Balanced Ternary Type Code Generation
 * 
 * Implements code generation for balanced ternary types (trit, tryte).
 * 
 * BALANCED TERNARY ARCHITECTURE:
 *   trit:  Single balanced ternary digit {-1, 0, 1}, stored as int8
 *   tryte: 10 balanced ternary digits, stored as uint16
 *          Range: [-29,524, +29,524]
 *          Error sentinel: 0xFFFF (65,535)
 * 
 * STICKY ERROR SEMANTICS:
 *   ERR + x = ERR
 *   x + ERR = ERR
 *   overflow(op) = ERR
 *   ERR cannot heal
 * 
 * This class intercepts all arithmetic operations on ternary types and
 * generates LLVM IR that calls the TernaryOps runtime functions.
 */
class TernaryLowerer {
    llvm::LLVMContext& llvmContext;
    llvm::IRBuilder<>& builder;
    llvm::Module* module;

    // Cached function declarations for TernaryOps runtime
    llvm::Function* addTrytesFunc = nullptr;
    llvm::Function* subTrytesFunc = nullptr;
    llvm::Function* mulTrytesFunc = nullptr;
    llvm::Function* divTrytesFunc = nullptr;
    llvm::Function* negateTryteFunc = nullptr;
    llvm::Function* binaryToTryteFunc = nullptr;
    llvm::Function* tryteToBinaryFunc = nullptr;
    llvm::Function* initializeFunc = nullptr;

public:
    explicit TernaryLowerer(llvm::LLVMContext& ctx, llvm::IRBuilder<>& bld, llvm::Module* mod)
        : llvmContext(ctx), builder(bld), module(mod) {}

    /**
     * Check if a type name represents a ternary type.
     * @param typeName The Aria type name (e.g., "trit", "tryte", "int32")
     * @return true if typeName is trit or tryte
     */
    static bool isTernaryType(const std::string& typeName);

    /**
     * Get the ERR sentinel value for tryte type.
     * @return LLVM constant representing 0xFFFF
     */
    llvm::Value* getTryteSentinel();

    /**
     * Ensure TernaryOps::initialize() has been called.
     * This should be called once at module initialization.
     */
    void ensureInitialized();

    /**
     * Create a safe addition operation for tryte with sticky error propagation.
     * Generates: call i16 @_Z10addTrytestt(i16 %lhs, i16 %rhs)
     * 
     * @param lhs Left operand (must be i16 tryte)
     * @param rhs Right operand (must be i16 tryte)
     * @return LLVM value representing the safe result
     */
    llvm::Value* createTryteAdd(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create a safe subtraction operation for tryte.
     * Generates: call i16 @_Z14subtractTrytestt(i16 %lhs, i16 %rhs)
     */
    llvm::Value* createTryteSub(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create a safe multiplication operation for tryte.
     * Generates: call i16 @_Z14multiplyTrytestt(i16 %lhs, i16 %rhs)
     */
    llvm::Value* createTryteMul(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create a safe division operation for tryte.
     * Generates: call i16 @_Z12divideTrytestt(i16 %lhs, i16 %rhs)
     * 
     * Division by zero → 0xFFFF (ERR)
     */
    llvm::Value* createTryteDiv(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create a negation operation for tryte.
     * Generates: call i16 @_Z11negateTrytet(i16 %val)
     * 
     * Negation in balanced ternary is simple trit inversion.
     */
    llvm::Value* createTryteNeg(llvm::Value* val);

    /**
     * Convert a binary integer to tryte.
     * Generates: call i16 @_Z13binaryToTrytei(i32 %val)
     * 
     * @param val LLVM i32 value
     * @return tryte (i16) or 0xFFFF if out of range
     */
    llvm::Value* convertBinaryToTryte(llvm::Value* val);

    /**
     * Convert a tryte to binary integer.
     * Generates: call i32 @_Z13tryteToBinaryt(i16 %val)
     * 
     * @param val LLVM i16 tryte value
     * @return i32 binary value (0 if ERR)
     */
    llvm::Value* convertTryteToBinary(llvm::Value* val);

private:
    /**
     * Declare or retrieve the TernaryOps runtime function for addition.
     * Mangled name: _Z10addTrytestt
     */
    llvm::Function* getAddTrytesFunc();

    /**
     * Declare or retrieve the TernaryOps runtime function for subtraction.
     * Mangled name: _Z14subtractTrytestt
     */
    llvm::Function* getSubTrytesFunc();

    /**
     * Declare or retrieve the TernaryOps runtime function for multiplication.
     * Mangled name: _Z14multiplyTrytestt
     */
    llvm::Function* getMulTrytesFunc();

    /**
     * Declare or retrieve the TernaryOps runtime function for division.
     * Mangled name: _Z12divideTrytestt
     */
    llvm::Function* getDivTrytesFunc();

    /**
     * Declare or retrieve the TernaryOps runtime function for negation.
     * Mangled name: _Z11negateTrytet
     */
    llvm::Function* getNegateTryteFunc();

    /**
     * Declare or retrieve the binary to tryte conversion function.
     * Mangled name: _Z13binaryToTrytei
     */
    llvm::Function* getBinaryToTryteFunc();

    /**
     * Declare or retrieve the tryte to binary conversion function.
     * Mangled name: _Z13tryteToBinaryt
     */
    llvm::Function* getTryteToBinaryFunc();

    /**
     * Declare or retrieve the TernaryOps initialization function.
     * Mangled name: _ZN4aria7backend10TernaryOps10initializeEv
     */
    llvm::Function* getInitializeFunc();
};

} // namespace backend
} // namespace aria

#endif // ARIA_CODEGEN_TERNARY_H

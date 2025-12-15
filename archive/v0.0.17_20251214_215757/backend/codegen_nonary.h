/**
 * src/backend/codegen_nonary.h
 * 
 * Balanced Nonary Type Code Generation
 * Version: 0.0.12
 * 
 * Implements LLVM IR generation for balanced nonary types (nit, nyte).
 */

#ifndef ARIA_CODEGEN_NONARY_H
#define ARIA_CODEGEN_NONARY_H

#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <string>

namespace aria {
namespace backend {

/**
 * NonaryLowerer - Balanced Nonary Type Code Generation
 * 
 * Implements code generation for balanced nonary types (nit, nyte).
 * 
 * BALANCED NONARY ARCHITECTURE:
 *   nit:  Single balanced nonary digit {-4, -3, -2, -1, 0, 1, 2, 3, 4}, stored as int8
 *   nyte: 5 balanced nonary digits, stored as uint16
 *         Range: [-29,524, +29,524]
 *         Bias: 29,524 (stored = value + 29,524)
 *         Error sentinel: 0xFFFF (65,535)
 * 
 * STICKY ERROR SEMANTICS:
 *   ERR + x = ERR
 *   x + ERR = ERR
 *   overflow(op) = ERR
 *   ERR cannot heal
 * 
 * This class intercepts all arithmetic operations on nonary types and
 * generates LLVM IR that calls the NonaryOps runtime functions.
 */
class NonaryLowerer {
    llvm::LLVMContext& llvmContext;
    llvm::IRBuilder<>& builder;
    llvm::Module* module;

    // Cached function declarations for NonaryOps runtime
    llvm::Function* addNytesFunc = nullptr;
    llvm::Function* subNytesFunc = nullptr;
    llvm::Function* mulNytesFunc = nullptr;
    llvm::Function* divNytesFunc = nullptr;
    llvm::Function* modNytesFunc = nullptr;
    llvm::Function* negateNyteFunc = nullptr;
    llvm::Function* compareNytesFunc = nullptr;
    llvm::Function* binaryToNyteFunc = nullptr;
    llvm::Function* nyteToBinaryFunc = nullptr;
    llvm::Function* initializeFunc = nullptr;

public:
    explicit NonaryLowerer(llvm::LLVMContext& ctx, llvm::IRBuilder<>& bld, llvm::Module* mod)
        : llvmContext(ctx), builder(bld), module(mod) {}

    /**
     * Check if a type name represents a nonary type.
     * @param typeName The Aria type name (e.g., "nit", "nyte", "int32")
     * @return true if typeName is nit or nyte
     */
    static bool isNonaryType(const std::string& typeName);

    /**
     * Get the ERR sentinel value for nyte type.
     * @return LLVM constant representing 0xFFFF
     */
    llvm::Value* getNyteSentinel();

    /**
     * Ensure NonaryOps::initialize() has been called.
     * This should be called once at module initialization.
     */
    void ensureInitialized();

    /**
     * Create a safe addition operation for nyte with sticky error propagation.
     * Generates: call i16 @_aria_nyte_add(i16 %lhs, i16 %rhs)
     * 
     * @param lhs Left operand (must be i16 nyte)
     * @param rhs Right operand (must be i16 nyte)
     * @return LLVM value representing the safe result
     */
    llvm::Value* createNyteAdd(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create a safe subtraction operation for nyte.
     * Generates: call i16 @_aria_nyte_sub(i16 %lhs, i16 %rhs)
     */
    llvm::Value* createNyteSub(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create a safe multiplication operation for nyte.
     * Generates: call i16 @_aria_nyte_mul(i16 %lhs, i16 %rhs)
     */
    llvm::Value* createNyteMul(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create a safe division operation for nyte.
     * Generates: call i16 @_aria_nyte_div(i16 %lhs, i16 %rhs)
     * 
     * Division by zero → 0xFFFF (ERR)
     */
    llvm::Value* createNyteDiv(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create a safe modulo operation for nyte.
     * Generates: call i16 @_aria_nyte_mod(i16 %lhs, i16 %rhs)
     * 
     * Modulo by zero → 0xFFFF (ERR)
     */
    llvm::Value* createNyteMod(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create a negation operation for nyte.
     * Generates: call i16 @_aria_nyte_negate(i16 %val)
     */
    llvm::Value* createNyteNegate(llvm::Value* val);

    /**
     * Create a comparison operation for nyte.
     * Generates: call i32 @_aria_nyte_compare(i16 %lhs, i16 %rhs)
     * 
     * @return -1 if lhs < rhs, 0 if lhs == rhs, 1 if lhs > rhs
     */
    llvm::Value* createNyteCompare(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create an equality check for nyte.
     * Generates: icmp eq i16 %lhs, %rhs
     */
    llvm::Value* createNyteEquals(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Create a less-than check for nyte.
     * Generates: icmp ult i16 %lhs, %rhs
     * (Uses unsigned comparison because of biased encoding)
     */
    llvm::Value* createNyteLessThan(llvm::Value* lhs, llvm::Value* rhs);

    /**
     * Convert a binary integer to nyte.
     * Generates: call i16 @_aria_int_to_nyte(i32 %val)
     */
    llvm::Value* createIntToNyte(llvm::Value* val);

    /**
     * Convert a nyte to binary integer.
     * Generates: call i32 @_aria_nyte_to_int(i16 %val)
     */
    llvm::Value* createNyteToInt(llvm::Value* val);

    /**
     * Create a nit literal value.
     * @param value The nit value (must be in [-4, 4])
     * @return LLVM i8 constant
     */
    llvm::Value* createNitLiteral(int8_t value);

    /**
     * Create a nyte literal value.
     * @param value The logical value (must be in [-29524, 29524])
     * @return LLVM i16 constant (biased encoding)
     */
    llvm::Value* createNyteLiteral(int32_t value);

private:
    /**
     * Declare or get the runtime function for nonary operations.
     * @param name The function name (e.g., "_aria_nyte_add")
     * @param returnType The LLVM return type
     * @param argTypes Vector of argument types
     * @return The LLVM function declaration
     */
    llvm::Function* getOrDeclareRuntimeFunc(
        const std::string& name,
        llvm::Type* returnType,
        const std::vector<llvm::Type*>& argTypes
    );
};

} // namespace backend
} // namespace aria

#endif // ARIA_CODEGEN_NONARY_H

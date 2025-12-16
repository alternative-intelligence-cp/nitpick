#ifndef ARIA_SEMA_TYPE_CHECKER_H
#define ARIA_SEMA_TYPE_CHECKER_H

#include "type.h"
#include "symbol_table.h"
#include "frontend/ast/ast_node.h"
#include "frontend/ast/expr.h"
#include "frontend/token.h"
#include <string>
#include <memory>

namespace aria {
namespace sema {

/**
 * TypeChecker - Performs type checking and type inference for expressions and statements
 * 
 * Phase 3.2.2: Type Checking for Expressions
 * 
 * Responsibilities:
 * - Infer types of expressions (literals, identifiers, binary/unary ops, calls)
 * - Check type compatibility for operations
 * - Validate operator types based on research_024 (arithmetic/bitwise) and research_025 (comparison/logical)
 * - Handle TBB type semantics (sticky errors, sentinel checks)
 * - Enforce strict boolean logic (no truthiness)
 * - Manage type coercion rules
 * 
 * Key Features:
 * - Literal type inference: int64, double, string, bool
 * - Identifier lookup in symbol table for type resolution
 * - Binary operator type checking with promotion/coercion
 * - Unary operator type validation
 * - Function call argument type matching
 * - TBB ERR propagation checking
 * - Strict boolean enforcement (no implicit truthiness)
 */
class TypeChecker {
private:
    TypeSystem* typeSystem;
    SymbolTable* symbolTable;
    std::vector<std::string> errors;  // Accumulated type errors
    
    // Current function return type (for return statement checking)
    Type* currentFunctionReturnType;
    
    // ========================================================================
    // Expression Type Inference
    // ========================================================================
    
    /**
     * Infer the type of a literal expression
     * 
     * Rules:
     * - Integer literals: int64 (default), can be narrowed with explicit type annotation
     * - Float literals: double (default for decimal), flt32 if suffixed with 'f'
     * - String literals: string
     * - Boolean literals: bool
     * - Null literals: UnknownType (will be resolved based on context)
     */
    Type* inferLiteral(LiteralExpr* expr);
    
    /**
     * Infer the type of an identifier expression
     * 
     * Rules:
     * - Lookup identifier in symbol table
     * - Return ErrorType if not found (with error message)
     * - Return symbol's declared type if found
     */
    Type* inferIdentifier(IdentifierExpr* expr);
    
    /**
     * Infer the type of a binary operation expression
     * 
     * Rules (based on research_024 and research_025):
     * - Arithmetic operators (+, -, *, /, %):
     *   * Require numeric types (int*, uint*, flt*, tbb*)
     *   * Promote operands to common type (widening)
     *   * TBB types stick to TBB (preserve error semantics)
     *   * Result type is the promoted type
     * 
     * - Bitwise operators (&, |, ^, ~, <<, >>):
     *   * UNSIGNED MANDATE: Only unsigned types allowed
     *   * Error if signed or TBB types used
     *   * Result type is same as operand type
     * 
     * - Comparison operators (==, !=, <, <=, >, >=):
     *   * Require compatible types
     *   * Result type is always bool
     *   * TBB comparisons: ERR handling (ERR == ERR is true, ERR < valid is undefined)
     * 
     * - Logical operators (&&, ||):
     *   * Strict boolean requirement (no truthiness)
     *   * Both operands must be bool
     *   * Result type is bool
     * 
     * - Spaceship operator (<=>):
     *   * Result type is int (returns -1, 0, or 1)
     */
    Type* inferBinaryOp(BinaryExpr* expr);
    
    /**
     * Infer the type of a unary operation expression
     * 
     * Rules (based on research_024 and research_026):
     * - Arithmetic negation (-): numeric types → same type
     * - Logical NOT (!): bool → bool (strict, no truthiness)
     * - Bitwise NOT (~): unsigned types → same type
     * - Address-of (@): T → T@ (pointer type)
     * - Pin (#): T (GC object) → wild T@ (pinned pointer)
     * - Borrow/Iterate ($): Array/Iterator → element type
     * - Unwrap (?): result<T> → T (with default handling)
     */
    Type* inferUnaryOp(UnaryExpr* expr);
    
    /**
     * Infer the type of a function call expression
     * 
     * Rules:
     * - Lookup function identifier to get function type
     * - Check argument count matches parameter count
     * - Check each argument type is assignable to parameter type
     * - Return function's return type
     */
    Type* inferCallExpr(CallExpr* expr);
    
    /**
     * Infer the type of an array index expression
     * 
     * Rules:
     * - Base must be array type (T[], T[N])
     * - Index must be integer type
     * - Result type is element type T
     */
    Type* inferIndexExpr(IndexExpr* expr);
    
    /**
     * Infer the type of a member access expression
     * 
     * Rules:
     * - Object must be struct or union type
     * - Member must exist in the type
     * - Result type is member's type
     */
    Type* inferMemberAccessExpr(MemberAccessExpr* expr);
    
    /**
     * Infer the type of a ternary expression
     * 
     * Rules:
     * - Condition must be bool
     * - True and false branches must have compatible types
     * - Result type is common type of branches
     */
    Type* inferTernaryExpr(TernaryExpr* expr);
    
    // ========================================================================
    // Type Compatibility and Coercion
    // ========================================================================
    
    /**
     * Find common type for binary operation (type promotion/widening)
     * 
     * Rules:
     * - int8 + int16 → int16 (widening to larger type)
     * - int32 + flt32 → flt32 (integer to float promotion)
     * - tbb8 + tbb16 → tbb16 (TBB widening preserves error semantics)
     * - int32 + tbb32 → ERROR (cannot mix standard and TBB)
     * - uint8 + int8 → ERROR (no implicit signed/unsigned mixing for safety)
     */
    Type* findCommonType(Type* left, Type* right);
    
    /**
     * Check if a type can be implicitly coerced to target type
     * 
     * Allowed coercions:
     * - Numeric widening: int8 → int16 → int32 → int64
     * - Integer to float: int32 → flt32, int64 → flt64
     * - TBB widening: tbb8 → tbb16 → tbb32 → tbb64
     * 
     * Disallowed coercions:
     * - Narrowing: int32 → int8 (requires explicit cast)
     * - Float to int: flt32 → int32 (requires explicit cast)
     * - Standard ↔ TBB: int32 ↔ tbb32 (requires explicit cast)
     * - Signed ↔ Unsigned: int32 ↔ uint32 (requires explicit cast for safety)
     */
    bool canCoerce(Type* from, Type* to);
    
    /**
     * Check if binary operator is valid for given operand types
     * 
     * Returns: Result type if valid, ErrorType otherwise
     */
    Type* checkBinaryOperator(frontend::TokenType op, Type* leftType, Type* rightType);
    
    /**
     * Check if unary operator is valid for given operand type
     * 
     * Returns: Result type if valid, ErrorType otherwise
     */
    Type* checkUnaryOperator(frontend::TokenType op, Type* operandType);
    
    // ========================================================================
    // Error Handling
    // ========================================================================
    
    void addError(const std::string& message, int line, int column);
    void addError(const std::string& message, ASTNode* node);
    
public:
    TypeChecker(TypeSystem* typeSystem, SymbolTable* symbolTable)
        : typeSystem(typeSystem), symbolTable(symbolTable),
          currentFunctionReturnType(nullptr) {}
    
    /**
     * Infer the type of an expression
     * 
     * This is the main entry point for type inference.
     * Dispatches to specific inference methods based on node type.
     * 
     * Returns: The inferred type, or ErrorType if type checking fails
     */
    Type* inferType(ASTNode* expr);
    
    /**
     * Check type compatibility for statements (Phase 3.2.3)
     * 
     * TODO: Implement in Phase 3.2.3
     * - Variable declaration initializer type checking
     * - Assignment type compatibility
     * - Return statement type matching
     * - Control flow condition type checking
     */
    // void checkStatement(ASTNode* stmt);
    
    /**
     * Get accumulated type errors
     */
    const std::vector<std::string>& getErrors() const { return errors; }
    
    /**
     * Check if type checking has errors
     */
    bool hasErrors() const { return !errors.empty(); }
    
    /**
     * Clear accumulated errors
     */
    void clearErrors() { errors.clear(); }
    
    /**
     * Set current function return type (for return statement checking)
     */
    void setCurrentFunctionReturnType(Type* type) { currentFunctionReturnType = type; }
};

} // namespace sema
} // namespace aria

#endif // ARIA_SEMA_TYPE_CHECKER_H

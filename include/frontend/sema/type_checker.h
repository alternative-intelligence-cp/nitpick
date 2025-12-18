#ifndef ARIA_SEMA_TYPE_CHECKER_H
#define ARIA_SEMA_TYPE_CHECKER_H

#include "type.h"
#include "symbol_table.h"
#include "frontend/ast/ast_node.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
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
    // TBB Type Validation (Phase 3.2.4)
    // ========================================================================
    
    /**
     * Check if a type is a TBB (Twisted Balanced Binary) type
     * 
     * TBB types: tbb8, tbb16, tbb32, tbb64
     */
    bool isTBBType(Type* type);
    
    /**
     * Get the ERR sentinel value for a TBB type
     * 
     * Returns:
     * - tbb8:  -128 (0x80)
     * - tbb16: -32768 (0x8000)
     * - tbb32: -2147483648 (0x80000000)
     * - tbb64: -9223372036854775808 (0x8000000000000000)
     */
    int64_t getTBBErrorSentinel(Type* type);
    
    /**
     * Get the valid range for a TBB type (excluding ERR sentinel)
     * 
     * Returns pair of (min, max):
     * - tbb8:  [-127, +127]
     * - tbb16: [-32767, +32767]
     * - tbb32: [-2147483647, +2147483647]
     * - tbb64: [-9223372036854775807, +9223372036854775807]
     */
    std::pair<int64_t, int64_t> getTBBValidRange(Type* type);
    
    /**
     * Validate that a literal value is not the ERR sentinel for a TBB type
     * 
     * Rules:
     * - Assigning ERR sentinel directly should produce a warning
     * - Use ERR keyword literal instead for clarity
     */
    void checkTBBLiteralValue(int64_t value, Type* type, ASTNode* node);
    
    /**
     * Check if operation produces ERR (sticky error propagation)
     * 
     * Rules:
     * - ERR + anything = ERR
     * - ERR * anything = ERR
     * - ERR in any arithmetic operation produces ERR
     * - Overflow in TBB operations produces ERR
     */
    bool isERRProducingOperation(Type* resultType, Type* leftType, Type* rightType);
    
    // ========================================================================
    // Balanced Ternary/Nonary Type Validation (Phase 3.2.5)
    // ========================================================================
    
    /**
     * Check if a type is a balanced ternary/nonary type
     * 
     * Balanced types: trit, tryte, nit, nyte
     */
    bool isBalancedType(Type* type);
    
    /**
     * Check if a type is trit (balanced ternary digit)
     * 
     * trit: single balanced ternary digit {-1, 0, 1}
     */
    bool isTritType(Type* type);
    
    /**
     * Check if a type is tryte (10 trits)
     * 
     * tryte: 10 trits stored in uint16
     * Range: [-29524, +29524] (59,049 values)
     */
    bool isTryteType(Type* type);
    
    /**
     * Check if a type is nit (balanced nonary digit)
     * 
     * nit: single balanced nonary digit {-4, -3, -2, -1, 0, 1, 2, 3, 4}
     */
    bool isNitType(Type* type);
    
    /**
     * Check if a type is nyte (5 nits)
     * 
     * nyte: 5 nits stored in uint16
     * Range: [-29524, +29524] (59,049 values)
     */
    bool isNyteType(Type* type);
    
    /**
     * Get valid digit values for balanced atomic types
     * 
     * Returns:
     * - trit: {-1, 0, 1}
     * - nit: {-4, -3, -2, -1, 0, 1, 2, 3, 4}
     * - tryte/nyte: empty (composite types, not digit validation)
     */
    std::vector<int> getBalancedValidDigits(Type* type);
    
    /**
     * Get valid range for balanced composite types
     * 
     * Returns pair of (min, max):
     * - tryte: [-29524, +29524]
     * - nyte: [-29524, +29524]
     * - trit/nit: N/A (use getBalancedValidDigits instead)
     */
    std::pair<int64_t, int64_t> getBalancedCompositeRange(Type* type);
    
    /**
     * Validate that a literal value is valid for a balanced type
     * 
     * Rules:
     * - trit: must be exactly -1, 0, or 1
     * - nit: must be -4, -3, -2, -1, 0, 1, 2, 3, or 4
     * - tryte: composite type, range checked at runtime
     * - nyte: composite type, range checked at runtime
     */
    void checkBalancedLiteralValue(int64_t value, Type* type, ASTNode* node);
    
    // ========================================================================    // Standard Integer Type Validation
    // ========================================================================
    
    /**
     * Check if a type is a standard integer type (int8, int16, int32, int64, uint8, uint16, uint32, uint64)
     */
    bool isStandardIntType(Type* type) const;
    
    /**
     * Check if an int64_t literal value fits in the target integer type (silent check, no errors)
     * 
     * This is used for context-aware literal typing in binary expressions.
     * For example, in "x + 10" where x is int32, we check if 10 fits in int32
     * to avoid unnecessary widening to int64.
     * 
     * Returns: true if the literal fits, false otherwise (no error reporting)
     */
    bool literalFitsInType(int64_t value, Type* type) const;
    
    /**
     * Check if an int64_t literal value can fit in the target integer type (with error reporting)
     * 
     * This enables safe narrowing conversions at compile time.
     * For example, the literal 42 (int64_t) can be assigned to int32, int16, or int8
     * because 42 fits within their ranges.
     * 
     * Rules:
     * - int8: range [-128, 127]
     * - int16: range [-32768, 32767]
     * - int32: range [-2147483648, 2147483647]
     * - int64: always fits (same type)
     * - uint8: range [0, 255]
     * - uint16: range [0, 65535]
     * - uint32: range [0, 4294967295]
     * - uint64: non-negative values always fit
     * 
     * Returns: true if the literal fits, false otherwise (and reports error)
     */
    bool canLiteralFitInIntType(int64_t value, Type* type, ASTNode* node);
    
    // ========================================================================    // Error Handling
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
     * Main entry point for statement type checking.
     * Dispatches to specific checking methods based on statement type.
     * Validates type safety for all statement constructs.
     */
    void checkStatement(ASTNode* stmt);
    
    /**
     * Check variable declaration statement
     * 
     * Rules:
     * - If initializer exists, its type must be assignable to declared type
     * - const variables must have initializer
     * - Declared type must exist in type system
     */
    void checkVarDecl(VarDeclStmt* stmt);
    
    /**
     * Check assignment expression
     * 
     * Rules:
     * - Left side must be assignable (identifier, index, member access)
     * - Right side type must be assignable to left side type
     * - Cannot assign to const variables
     */
    void checkAssignment(BinaryExpr* expr);
    
    /**
     * Check return statement
     * 
     * Rules:
     * - Return type must match current function's return type
     * - Void functions cannot return values
     * - Non-void functions must return values
     */
    void checkReturnStmt(ReturnStmt* stmt);
    
    /**
     * Check if statement
     * 
     * Rules:
     * - Condition must be bool type
     * - No truthiness allowed (must use explicit comparison)
     */
    void checkIfStmt(IfStmt* stmt);
    
    /**
     * Check while statement
     * 
     * Rules:
     * - Condition must be bool type
     * - No truthiness allowed
     */
    void checkWhileStmt(WhileStmt* stmt);
    
    /**
     * Check for statement
     * 
     * Rules:
     * - Condition (if present) must be bool type
     * - Initializer and update can be any expression
     */
    void checkForStmt(ForStmt* stmt);
    
    /**
     * Check block statement (recursively check all statements)
     */
    void checkBlockStmt(BlockStmt* stmt);
    
    /**
     * Check expression statement (just infer its type)
     */
    void checkExpressionStmt(ExpressionStmt* stmt);
    
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

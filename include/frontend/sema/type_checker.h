#ifndef ARIA_SEMA_TYPE_CHECKER_H
#define ARIA_SEMA_TYPE_CHECKER_H

#include "type.h"
#include "symbol_table.h"
#include "generic_resolver.h"
#include "const_evaluator.h"  // Phase 2.2: Compile-time evaluation
#include "module_loader.h"     // Module system integration
#include "frontend/ast/ast_node.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include "frontend/token.h"
#include <string>
#include <memory>
#include <map>

namespace aria {
namespace sema {

/**
 * ResultCheckState - Tracks Result<T> check state through control flow
 * 
 * Phase 2: Control Flow Analysis for Result Enforcement
 * Tracks whether Result variables have been checked and what we know about their state
 */
struct ResultCheckState {
    enum class State {
        UNCHECKED,       // Haven't checked .is_error yet
        CHECKED,         // Checked .is_error, but don't know if true/false
        KNOWN_ERROR,     // Know .is_error == true
        KNOWN_SUCCESS    // Know .is_error == false
    };
    
    // Map variable name -> state
    std::unordered_map<std::string, State> states;
    
    // Mark a Result as checked (.is_error was accessed)
    void markChecked(const std::string& varName);
    
    // Mark a Result as known error (.is_error == true)
    void markKnownError(const std::string& varName);
    
    // Mark a Result as known success (.is_error == false)
    void markKnownSuccess(const std::string& varName);
    
    // Check if a Result has been checked
    bool isChecked(const std::string& varName) const;
    
    // Get the state of a Result
    State getState(const std::string& varName) const;
    
    // Merge states from two branches (conservative join)
    // If branches disagree, result is CHECKED (known checked, value unknown)
    static ResultCheckState merge(const ResultCheckState& thenState, 
                                   const ResultCheckState& elseState);
};

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
    GenericResolver* genericResolver;
    Monomorphizer* monomorphizer;
    ConstEvaluator* constEvaluator;  // Phase 2.2: Compile-time evaluation
    ModuleLoader* moduleLoader;       // Module loading and import resolution
    std::vector<std::string> errors;  // Accumulated type errors
    std::vector<std::string> warnings;  // Accumulated type warnings (v0.4.3)
    
    // Module tracking: maps module symbol names to their LoadedModule
    std::unordered_map<std::string, LoadedModule*> loadedModules;
    
    // Current function return type (for return statement checking)
    // Functions return Result<T>, so currentFunctionReturnType is the Result type
    Type* currentFunctionReturnType;
    
    // Current function value type (the T in Result<T>)
    // Used by pass/fail statements to validate value types
    Type* currentFunctionValueType;

    // Current function name (for exit() restriction to main/failsafe)
    std::string currentFunctionName;
    
    // Current module path (for resolving relative imports)
    std::string currentModulePath;
    
    // Generic struct registry (Session 13)
    // Maps struct name -> generic struct declaration AST
    std::unordered_map<std::string, StructDeclStmt*> genericStructRegistry;
    
    // v0.2.41: Rules table — maps rules name -> RulesDeclStmt for limit<> validation
    std::unordered_map<std::string, RulesDeclStmt*> rulesTable;
    
    // v0.2.41: Tracks which variables have limit<> constraints (name -> rules name)
    std::unordered_map<std::string, std::string> limitedVariables;
    
    // Phase 2: Control flow analysis for Result<T> enforcement
    // Tracks check state through branches and early returns
    ResultCheckState currentResultState;
    
    // Phase 1.5: Immutable .is_error enforcement  
    // Tracks which Result variables have 'wild' modifier
    // Only wild Results can modify .is_error field (prevents error state corruption)
    // Non-wild Results have immutable .is_error (errors must propagate)
    std::unordered_set<std::string> wildResults;
    
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
     * Infer the type of a comptime expression: comptime(expr)
     * Evaluates the expression at compile time using ConstEvaluator
     * and returns the type of the resulting constant value.
     */
    Type* inferComptimeExpr(ComptimeExpr* expr);
    
    /**
     * Infer the type of a template literal expression
     * 
     * Rules:
     * - Template literals always produce string type
     * - Type-check all interpolated expressions
     * - TODO: Add toString() conversion requirements
     */
    Type* inferTemplateLiteral(TemplateLiteralExpr* expr);
    
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
     * Infer the type of a vector constructor expression
     * 
     * Rules:
     * - All component types must match
     * - Component count must match dimension (2, 3, or 9)
     * - Default component type is flt32 if inferred from float literals
     * - Result type is vec2/vec3/vec9 with inferred component type
     */
    Type* inferVectorConstructor(VectorConstructorExpr* expr);
    
    /**
     * Infer the type of a cast expression (@cast or @cast_unchecked)
     * 
     * Rules:
     * - Safe casts (widening): i8→i64, f32→f64 always succeed
     * - Checked casts (narrowing): i64→i8 require runtime check (unless unchecked)
     * - Unchecked casts: no runtime check, may truncate
     * - Invalid casts: incompatible types cause compile error
     * - Result type is the target type specified in @cast<Type>(expr)
     */
    Type* inferCastExpr(CastExpr* expr);
    
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
    
    /**
     * Infer the type of an unwrap expression (? operator)
     * 
     * Syntax: result ? default_value
     * 
     * Rules:
     * - Result expression and default value must have compatible types
     * - Returns the common type of result and default value
     * - Full result type checking will be added when result types are implemented
     */
    Type* inferUnwrapExpr(UnwrapExpr* expr);
    
    /**
     * Infer the type of a defaults expression (v0.4.3)
     * Syntax: expr ?| fallback   OR   expr defaults fallback
     */
    Type* inferDefaultsExpr(DefaultsExpr* expr);
    
    /**
     * Infer the type of a move expression
     * 
     * Syntax: move(variable)
     * 
     * Rules:
     * - Variable must be a valid identifier
     * - Returns the same type as the source variable
     * - Move tracking and invalidation is handled by the borrow checker
     */
    Type* inferMoveExpr(MoveExpr* expr);
    
    /**
     * Infer the type of an object literal expression
     * 
     * Rules:
     * - If type_name is set: struct constructor, return struct type
     * - If type_name is empty: dynamic object, return obj type
     * - Field values must be compatible with struct field types (if struct)
     */
    Type* inferObjectLiteral(ObjectLiteralExpr* expr);
    
    /**
     * Infer the type of an array literal expression
     * 
     * Rules:
     * - Empty array: cannot infer type (error)
     * - Non-empty: infer from first element
     * - All elements must have compatible types
     * - Returns pointer to element type (arrays decay to pointers)
     */
    Type* inferArrayLiteral(ArrayLiteralExpr* expr);
    
    /**
     * Infer type of range expression (start..end or start...end)
     * 
     * Rules:
     * - Start and end must be integer types
     * - Returns int64 as placeholder (actual type is opaque struct)
     */
    Type* inferRangeExpr(RangeExpr* expr);
    
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
     * Check if FFI pointer conversion is allowed (P1.5)
     * 
     * Automatic conversions at extern boundaries:
     * - void* → wild T-> (from C function return to Aria pointer)
     * - wild T-> → void* (from Aria pointer to C function parameter)
     * 
     * Safety: Only works at FFI boundaries (extern blocks)
     */
    bool canConvertFFIPointer(Type* from, Type* to);
    
    /**
     * Check if binary operator is valid for given operand types
     * 
     * Returns: Result type if valid, ErrorType otherwise
     */
    Type* checkBinaryOperator(frontend::TokenType op, Type* leftType, Type* rightType,
                              ASTNode* sourceNode = nullptr);
    
    /**
     * Check if unary operator is valid for given operand type
     * 
     * Returns: Result type if valid, ErrorType otherwise
     */
    Type* checkUnaryOperator(frontend::TokenType op, Type* operandType,
                             ASTNode* sourceNode = nullptr);
    
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
     * Check if a type is a numeric exotic type (frac*, tfp*, vec9, etc.)
     * 
     * Numeric exotic types: frac8, frac16, frac32, frac64, tfp32, tfp64,
     *                       vec9, dvec9, tmatrix, ttensor
     */
    bool isNumericExoticType(Type* type);
    
    /**
     * P1-5: Check if a type is numeric (for dimensional analysis)
     * 
     * Includes: int*, uint*, flt*, tbb*, frac*, tfp*, vec9, dvec9,
     *          trit, tryte, nit, nyte, fix* (fixed-point)
     */
    bool isNumericType(Type* type);
    
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
    void addWarning(const std::string& message, ASTNode* node);
    
public:
    TypeChecker(TypeSystem* typeSystem, SymbolTable* symbolTable,
                GenericResolver* resolver = nullptr, Monomorphizer* morpher = nullptr,
                ModuleLoader* loader = nullptr, const std::string& modulePath = "")
        : typeSystem(typeSystem), symbolTable(symbolTable),
          genericResolver(resolver), monomorphizer(morpher),
          constEvaluator(new ConstEvaluator(symbolTable)),  // Phase 2.2
          moduleLoader(loader),
          currentModulePath(modulePath),
          currentFunctionReturnType(nullptr),
          currentFunctionValueType(nullptr),
          currentFunctionName("") {}
    
    ~TypeChecker() {
        delete constEvaluator;  // Clean up
    }
    
    /**
     * Main entry point: Type check entire module AST
     * 
     * Walks the AST recursively, performing:
     * - Type inference on all expressions
     * - Type checking on all statements
     * - Generic function specialization detection
     * - Symbol table population
     * 
     * This method triggers the entire type checking pipeline.
     */
    void check(ASTNode* module);
    
    /**
     * Infer the type of an expression
     * 
     * This is the main entry point for type inference.
     * Dispatches to specific inference methods based on node type.
     * 
     * Returns: The inferred type, or ErrorType if type checking fails
     */
    Type* inferType(ASTNode* expr);
    
    // ========================================================================
    // Result<T> "No Checky No Val" Enforcement
    // ========================================================================
    
    /**
     * Mark a Result variable as checked (when .is_error is accessed)
     * 
     * Once marked, the variable can safely access .value or .error
     * until it goes out of scope.
     */
    void markResultChecked(const std::string& varName);
    
    /**
     * Check if a Result variable has been checked
     * 
     * Returns true if .is_error was accessed on this variable
     */
    bool isResultChecked(const std::string& varName) const;
    
    /**
     * Get variable name from an expression (for Result tracking)
     * 
     * Handles: IDENTIFIER, MEMBER_ACCESS (for struct.result_field)
     * Returns empty string if not a simple variable reference
     */
    std::string getVariableName(ASTNode* expr) const;
    
    /**
     * Check type compatibility for statements (Phase 3.2.3)
     * 
     * Main entry point for statement type checking.
     * Dispatches to specific checking methods based on statement type.
     * Validates type safety for all statement constructs.
     */
    void checkStatement(ASTNode* stmt);
    
    // ========================================================================
    // Type Resolution
    // ========================================================================
    
    /**
     * Resolve a Type from an AST type node
     * 
     * Rules:
     * - TYPE_ANNOTATION: Simple type (int32, string, etc.)
     * - ARRAY_TYPE: Array type (int32[], int32[10], etc.)
     * - POINTER_TYPE: Pointer type (int32@, string@, etc.)
     * - Returns ERROR type if resolution fails
     */
    Type* resolveTypeNode(ASTNode* typeNode);
    
    // ========================================================================
    // Statement Type Checking
    // ========================================================================
    
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
     * Check function declaration statement
     * 
     * Rules:
     * - Return type must exist in type system
     * - All parameter types must exist in type system
     * - Function body statements are type-checked
     * - Generic function calls in body trigger specialization
     */
    void checkFuncDecl(FuncDeclStmt* stmt);
    
    /**
     * Check comptime block statement: comptime { ... }
     * Evaluates the block at compile time using ConstEvaluator.
     */
    void checkComptimeBlock(ComptimeBlockStmt* stmt);
    
    /**
     * 
     * Rules:
     * - All contract expressions must evaluate to bool
     * - Variables in preconditions (requires) must be function parameters
     * - Variables in postconditions (ensures) can reference 'result' keyword
     * - Contract expressions must be pure (no side effects)
     * 
     * @param contracts Vector of contract clause expressions
     * @param isPostcondition true for ensures clauses (allows 'result'), false for requires
     * @param valueType Return value type (for 'result' validation in postconditions)
     * @param stmt Statement for error reporting
     */
    void validateContracts(const std::vector<ASTNodePtr>& contracts, 
                          bool isPostcondition, 
                          Type* valueType,
                          ASTNode* stmt);
    
    /**
     * Check struct declaration statement
     * 
     * Rules:
     * - Struct name must be unique (not already defined)
     * - All field types must exist in type system
     * - Register struct type in type system
     */
    void checkStructDecl(StructDeclStmt* stmt);
    
    /**
     * Check enum declaration
     * 
     * Rules:
     * - Enum name must be unique
     * - All variant names must be unique within the enum
     * - All variant values must be integer literals
     * - Register enum variants in symbol table
     */
    void checkEnumDecl(EnumDeclStmt* stmt);

    /**
     * v0.2.41: Check Rules declaration (refinement types)
     * - Register rules in rulesTable for limit<> reference
     * - Validate cascaded rules references exist
     */
    void checkRulesDecl(RulesDeclStmt* stmt);
    
    // v0.2.43: Validate $.field references in rule conditions against type parameter
    void validateRulesDollarFields(ASTNode* node, const std::string& typeName,
                                    StructType* structType, ASTNode* errorNode);
    
    // v0.2.41: Compile-time rule validation for limit<> variables
    void validateLimitRules(const std::string& rulesName, int64_t value, ASTNode* stmt);
    void validateLimitRulesFloat(const std::string& rulesName, double value, ASTNode* stmt);
    bool evaluateRuleConditionInt(ASTNode* node, int64_t dollarValue);
    int64_t evalRuleOperandInt(ASTNode* node, int64_t dollarValue);
    bool evaluateRuleConditionFloat(ASTNode* node, double dollarValue);
    double evalRuleOperandFloat(ASTNode* node, double dollarValue);

    /**
     * Check Type declaration and desugar to struct + methods
     * 
     * Type Oriented Programming (TOP) - better than OOP!
     * Desugars Type:Name into:
     * - Combined struct (struct:internal + struct:interface fields)
     * - Prefixed methods: TypeName_create, TypeName_destroy, TypeName_method
     * - Static members accessible via Type.MEMBER syntax
     * 
     * Rules:
     * - Type name must be unique
     * - Creates composable units without inheritance
     * - Zero-cost abstraction via compile-time desugaring
     */
    void checkTypeDecl(TypeDeclStmt* stmt);

    /**
     * Check trait declaration (WP 005)
     *
     * Rules:
     * - Trait name must not already be defined
     * - Register trait in symbol table
     * - Validate method signatures
     */
    void checkTraitDecl(TraitDeclStmt* stmt);

    /**
     * Check trait implementation (WP 005)
     *
     * Rules:
     * - Trait must exist
     * - Type must exist
     * - All trait methods must be implemented
     * - Method signatures must match trait
     */
    void checkImplDecl(ImplDeclStmt* stmt);

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
     * Check pass statement (result success)
     * 
     * Rules:
     * - Pass value type must match function return type
     * - Cannot be used in void functions
     */
    void checkPassStmt(PassStmt* stmt);
    
    /**
     * Check fail statement (result error)
     * 
     * Rules:
     * - Error code must be integer type
     */
    void checkFailStmt(FailStmt* stmt);
    
    void checkProveStmt(ProveStmt* stmt);
    void checkAssertStaticStmt(AssertStaticStmt* stmt);
    
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
     * Check till statement
     * 
     * Rules:
     * - Limit must be integer type
     * - Step must be integer type
     * - $ variable is automatically available as iteration value
     */
    void checkTillStmt(TillStmt* stmt);
    
    /**
     * Check loop statement
     * 
     * Rules:
     * - Start must be integer type
     * - Limit must be integer type
     * - Step must be integer type
     * - $ variable is automatically available as iteration value
     */
    void checkLoopStmt(LoopStmt* stmt);
    
    /**
     * Check when statement (tri-state loop)
     * 
     * Rules:
     * - Condition must be bool type
     * - then block executes on normal completion
     * - end block executes on break or initial false
     */
    void checkWhenStmt(WhenStmt* stmt);
    
    /**
     * Check pick statement (pattern matching)
     * 
     * Rules:
     * - Selector type determines valid patterns
     * - Must be exhaustive (all cases covered)
     * - TBB types must handle ERR sentinel
     */
    void checkPickStmt(PickStmt* stmt);
    
    /**
     * Check block statement (recursively check all statements)
     */
    void checkBlockStmt(BlockStmt* stmt);
    
    /**
     * Check expression statement (just infer its type)
     */
    void checkExpressionStmt(ExpressionStmt* stmt);
    
    /**
     * Check use statement (module import)
     * Validates import path structure and format
     */
    void checkUseStmt(UseStmt* stmt);
    
    /**
     * Check mod statement (module declaration)
     * Validates module name and recursively checks inline module bodies
     */
    void checkModStmt(ModStmt* stmt);
    
    // ========================================================================
    // Phase 2: Control Flow Analysis Helpers
    // ========================================================================
    
    /**
     * Analyze an if condition to extract Result check knowledge
     * Updates thenState and elseState based on what the condition tells us
     * 
     * Example: if (r.is_error == false)
     *   thenState knows: r is SUCCESS
     *   elseState knows: r is ERROR
     */
    void analyzeConditionForResultChecks(ASTNode* condition,
                                          ResultCheckState& thenState,
                                          ResultCheckState& elseState);
    
    /**
     * Check if a branch (block/statement) always returns/exits
     * Used to handle early returns: if (err) { return; } → after if, we know !err
     */
    bool branchAlwaysReturns(ASTNode* branch);
    
    /**
     * Check extern statement (FFI declarations)
     * Registers external function and variable symbols in current scope
     */
    void checkExternStmt(ExternStmt* stmt);
    
    /**
     * Recursively analyze a specialized function body for nested generic calls.
     * When a generic function is monomorphized, its body may contain calls to
     * other generic functions that also need specialization (transitive monomorphization).
     */
    void analyzeSpecializedBody(FuncDeclStmt* specDecl, const TypeSubstitution& outerSub);
    
    // ========================================================================
    // Module Symbol Importing (Phase 3 from research_module_loading_system)
    // ========================================================================
    
    /**
     * Import all public symbols from module into current scope (wildcard import)
     * 
     * Syntax: use std.io.*;
     * 
     * Rules:
     * - Only PUBLIC visibility symbols are imported
     * - Symbols are imported directly into current scope
     * - Name collisions produce errors
     */
    void importWildcardSymbols(LoadedModule* module, UseStmt* stmt);
    
    /**
     * Import selected symbols from module (selective import)
     * 
     * Syntax: use std.{array, map};
     * 
     * Rules:
     * - Each symbol must exist in module
     * - Symbol visibility must allow import
     * - Aliases are supported: use std.{array as arr}
     * - Importing non-existent symbol is an error
     */
    void importSelectiveSymbols(LoadedModule* module, UseStmt* stmt);
    
    /**
     * Import module as namespace
     * 
     * Syntax: use std.io;
     * 
     * Rules:
     * - Module is imported as a namespace
     * - Symbols accessed via namespace: io.print()
     * - Namespace can be aliased: use std.io as my_io;
     */
    void importModuleNamespace(LoadedModule* module, UseStmt* stmt, const std::string& modulePath);
    
    /**
     * Get accumulated type errors
     */
    const std::vector<std::string>& getErrors() const { return errors; }
    
    /**
     * Get accumulated type warnings (v0.4.3)
     */
    const std::vector<std::string>& getWarnings() const { return warnings; }

    /**
     * Check if type checking has errors
     */
    bool hasErrors() const { return !errors.empty(); }
    
    /**
     * Get the rules table for Z3 verification pass (v0.2.45)
     */
    const std::unordered_map<std::string, RulesDeclStmt*>& getRulesTable() const { return rulesTable; }
    
    /**
     * Get limited variables map (var name -> rules name) for Z3 verification (v0.2.45)
     */
    const std::unordered_map<std::string, std::string>& getLimitedVariables() const { return limitedVariables; }

    /**
     * Validate that the required failsafe() function exists
     * 
     * Every Aria program must define a failsafe(int32) function
     * for handling unrecoverable errors. This enforces accountability
     * for error handling - an empty failsafe is valid but documented.
     * 
     * Returns: true if failsafe() is defined with correct signature
     */
    bool validateFailsafeExists();
    
    /**
     * Validate that func:main is defined in the program.
     * Every Aria program must define a main() function as the entry point.
     * 
     * Returns: true if main() is defined with correct signature
     */
    bool validateMainExists();
    
    /**
     * Clear accumulated errors
     */
    void clearErrors() { errors.clear(); }
    
    /**
     * Set current function return type (for return statement checking)
     */
    void setCurrentFunctionReturnType(Type* type) { currentFunctionReturnType = type; }
    
    // ========================================================================
    // v0.8.3: Macro System — AST-Level Macros, Derive, Comptime, Attributes
    // ========================================================================
    
    /**
     * Register an AST-level macro declaration for later expansion.
     */
    void checkMacroDecl(MacroDeclStmt* stmt);
    
    /**
     * Expand an AST-level macro invocation: name!(args)
     * Returns the type of the expanded expression.
     */
    Type* inferMacroInvocation(MacroInvocationExpr* expr);
    
    /**
     * Expand #[derive(Trait1, Trait2, ...)] attributes on a struct.
     * Generates synthetic ImplDeclStmt nodes for each derive trait.
     */
    void expandDeriveAttributes(StructDeclStmt* stmt);
    
    /**
     * Process #[...] attributes on a function declaration.
     * Applies well-known attribute effects (inline, noinline, gpu_kernel, etc.)
     */
    void processAttributes(FuncDeclStmt* stmt);

private:
    // v0.8.3: Macro registry — maps macro name -> MacroDeclStmt*
    std::map<std::string, MacroDeclStmt*> macroRegistry;
    
    // v0.8.3: Synthetic AST nodes generated by derive expansion
    // Kept alive until end of type checking
    std::vector<ASTNodePtr> syntheticNodes;
    
    // v0.8.3: Clone an AST subtree with parameter substitution
    ASTNodePtr cloneAST(ASTNode* node, const std::map<std::string, ASTNodePtr>& substitutions);

public:
    // v0.8.4: Return synthetic nodes (derive-generated impls) for IR injection
    const std::vector<ASTNodePtr>& getSyntheticNodes() const { return syntheticNodes; }
};

} // namespace sema
} // namespace aria

#endif // ARIA_SEMA_TYPE_CHECKER_H

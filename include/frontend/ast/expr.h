#ifndef ARIA_EXPR_H
#define ARIA_EXPR_H

#include "ast_node.h"
#include "frontend/token.h"
#include <variant>

namespace aria {

// Import Token from frontend namespace for convenience
using aria::frontend::Token;
using aria::frontend::TokenType;

/**
 * Literal expression node
 * Represents: integer, float, string, boolean, null literals
 * 
 * ZERO IMPLICIT CONVERSION POLICY:
 * All numeric literals MUST have explicit type suffixes (u32, i64, f32, etc.)
 * The explicit_type field stores the type suffix to prevent any implicit conversions
 */
class LiteralExpr : public ASTNode {
public:
    std::variant<int64_t, double, std::string, bool, std::monostate> value;
    std::string raw_value_string;  // For high-precision literals (flt128, int128, etc.)
    std::string explicit_type;     // Type suffix: "u32", "i64", "f32", "tbb8", etc.
    
    // DEPRECATED constructors (for backward compatibility during migration)
    explicit LiteralExpr(int64_t val, int line = 0, int column = 0)
        : ASTNode(NodeType::LITERAL, line, column), value(val), explicit_type("") {}
    
    explicit LiteralExpr(double val, int line = 0, int column = 0)
        : ASTNode(NodeType::LITERAL, line, column), value(val), explicit_type("") {}
    
    explicit LiteralExpr(const std::string& val, int line = 0, int column = 0)
        : ASTNode(NodeType::LITERAL, line, column), value(val), explicit_type("") {}
    
    explicit LiteralExpr(bool val, int line = 0, int column = 0)
        : ASTNode(NodeType::LITERAL, line, column), value(val), explicit_type("") {}
    
    // Null literal
    explicit LiteralExpr(std::monostate, int line = 0, int column = 0)
        : ASTNode(NodeType::LITERAL, line, column), value(std::monostate{}), explicit_type("") {}
    
    // DEPRECATED: High-precision constructors (for compatibility - prefer typed versions)
    // Disambiguation: 5th parameter tells whether 2nd parameter is raw_text (true) or type (false)
    explicit LiteralExpr(int64_t val, const std::string& raw_or_type, int line, int column, bool is_raw_text = true)
        : ASTNode(NodeType::LITERAL, line, column), value(val), 
          raw_value_string(is_raw_text ? raw_or_type : ""), 
          explicit_type(is_raw_text ? "" : raw_or_type) {}
    
    explicit LiteralExpr(double val, const std::string& raw_text, int line, int column)
        : ASTNode(NodeType::LITERAL, line, column), value(val), raw_value_string(raw_text), explicit_type("") {}
    
    // NEW: Typed literal constructors (Zero Implicit Conversion Policy)
    // Integer literals with explicit type (small values that fit in int64_t)
    static std::shared_ptr<LiteralExpr> makeTypedInt(int64_t val, const std::string& type, int line, int column) {
        auto lit = std::make_shared<LiteralExpr>(val, line, column);
        lit->explicit_type = type;
        return lit;
    }
    
    // Integer literals with explicit type AND high-precision raw text
    static std::shared_ptr<LiteralExpr> makeTypedIntWithRaw(int64_t val, const std::string& raw_text, 
                                                             const std::string& type, int line, int column) {
        auto lit = std::make_shared<LiteralExpr>(val, raw_text, line, column);
        lit->explicit_type = type;
        return lit;
    }
    
    // Float literals with explicit type
    static std::shared_ptr<LiteralExpr> makeTypedFloat(double val, const std::string& raw_text, 
                                                        const std::string& type, int line, int column) {
        auto lit = std::make_shared<LiteralExpr>(val, raw_text, line, column);
        lit->explicit_type = type;
        return lit;
    }
    
    // Helper methods
    bool hasRawValue() const { return !raw_value_string.empty(); }
    const std::string& getRawValue() const { return raw_value_string; }
    bool hasExplicitType() const { return !explicit_type.empty(); }
    const std::string& getExplicitType() const { return explicit_type; }
    
    std::string toString() const override;
};

/**
 * Identifier expression node
 * Represents: variable names, function names
 */
class IdentifierExpr : public ASTNode {
public:
    std::string name;
    
    IdentifierExpr(const std::string& n, int line = 0, int column = 0)
        : ASTNode(NodeType::IDENTIFIER, line, column), name(n) {}
    
    std::string toString() const override;
};

/**
 * Template literal expression node
 * Represents: `text &{expr} more text`
 * Contains alternating string parts and interpolated expressions
 */
class TemplateLiteralExpr : public ASTNode {
public:
    std::vector<std::string> parts;              // String parts
    std::vector<std::shared_ptr<ASTNode>> interpolations;  // Expressions to interpolate
    
    TemplateLiteralExpr(int line = 0, int column = 0)
        : ASTNode(NodeType::TEMPLATE_LITERAL, line, column) {}
    
    std::string toString() const override;
};

/**
 * Binary operation expression node
 * Represents: a + b, x * y, etc.
 */
class BinaryExpr : public ASTNode {
public:
    ASTNodePtr left;
    Token op;
    ASTNodePtr right;
    
    BinaryExpr(ASTNodePtr l, const Token& o, ASTNodePtr r, int line = 0, int column = 0)
        : ASTNode(NodeType::BINARY_OP, line, column), left(l), op(o), right(r) {}
    
    std::string toString() const override;
};

/**
 * Range expression node
 * Represents: start..end (inclusive), start...end (exclusive)
 */
class RangeExpr : public ASTNode {
public:
    ASTNodePtr start;
    ASTNodePtr end;
    bool isExclusive;  // true for ..., false for ..
    
    RangeExpr(ASTNodePtr s, ASTNodePtr e, bool exclusive, int line = 0, int column = 0)
        : ASTNode(NodeType::RANGE, line, column), start(s), end(e), isExclusive(exclusive) {}
    
    std::string toString() const override;
};

/**
 * Unary operation expression node
 * Represents: -x, !flag, ~bits, @ptr, #ref, $iter
 */
class UnaryExpr : public ASTNode {
public:
    Token op;
    ASTNodePtr operand;
    bool isPostfix;
    
    // Borrow checker annotations
    bool creates_loan;      // True if this is a $ (borrow) operator
    bool is_mutable_loan;   // True if mutable borrow ($x), false if immutable (!$x)
    std::string loan_target; // Variable name being borrowed (for $ operator)
    bool creates_pin;       // True if this is a # (pin) operator
    std::string pin_target;  // Variable name being pinned (for # operator)
    
    UnaryExpr(const Token& o, ASTNodePtr operand, bool isPost = false, int line = 0, int column = 0)
        : ASTNode(NodeType::UNARY_OP, line, column), op(o), operand(operand), isPostfix(isPost),
          creates_loan(false), is_mutable_loan(false), loan_target(""), creates_pin(false), pin_target("") {}
    
    std::string toString() const override;
};

/**
 * Function call expression node
 * Represents: func(arg1, arg2, ...) or func::<T, U>(arg1, arg2, ...)
 */
class CallExpr : public ASTNode {
public:
    ASTNodePtr callee;
    std::vector<ASTNodePtr> arguments;
    std::vector<std::string> explicitTypeArgs;  // For turbofish syntax: ::<T, U>
    std::string specializedMangledName;  // For generic calls: resolved mangled name
    bool isPipelineCall = false;  // True for pipeline-generated calls (|>, <|)
    
    CallExpr(ASTNodePtr callee, const std::vector<ASTNodePtr>& args, int line = 0, int column = 0)
        : ASTNode(NodeType::CALL, line, column), callee(callee), arguments(args) {}
    
    CallExpr(ASTNodePtr callee, const std::vector<ASTNodePtr>& args, 
             const std::vector<std::string>& typeArgs, int line = 0, int column = 0)
        : ASTNode(NodeType::CALL, line, column), callee(callee), arguments(args), 
          explicitTypeArgs(typeArgs) {}
    
    std::string toString() const override;
};

/**
 * Array index expression node
 * Represents: arr[index]
 */
class IndexExpr : public ASTNode {
public:
    ASTNodePtr array;
    ASTNodePtr index;
    
    IndexExpr(ASTNodePtr arr, ASTNodePtr idx, int line = 0, int column = 0)
        : ASTNode(NodeType::INDEX, line, column), array(arr), index(idx) {}
    
    std::string toString() const override;
};

/**
 * Member access expression node
 * Represents: obj.member, obj->member, obj?.member
 */
class MemberAccessExpr : public ASTNode {
public:
    ASTNodePtr object;
    std::string member;
    bool isPointerAccess;  // true for ->, false for .
    bool isSafeNavigation; // true for ?., false otherwise
    
    MemberAccessExpr(ASTNodePtr obj, const std::string& mem, bool isPtr = false, bool isSafeNav = false, int line = 0, int column = 0)
        : ASTNode(isPtr ? NodeType::POINTER_MEMBER : NodeType::MEMBER_ACCESS, line, column),
          object(obj), member(mem), isPointerAccess(isPtr), isSafeNavigation(isSafeNav) {}
    
    std::string toString() const override;
};

/**
 * Ternary expression node
 * Represents: is condition : true_value : false_value
 */
class TernaryExpr : public ASTNode {
public:
    ASTNodePtr condition;
    ASTNodePtr trueValue;
    ASTNodePtr falseValue;
    
    TernaryExpr(ASTNodePtr cond, ASTNodePtr trueVal, ASTNodePtr falseVal, int line = 0, int column = 0)
        : ASTNode(NodeType::TERNARY, line, column),
          condition(cond), trueValue(trueVal), falseValue(falseVal) {}
    
    std::string toString() const override;
};

/**
 * Assignment expression node
 * Represents: x = 5, y += 3, etc.
 */
class AssignmentExpr : public ASTNode {
public:
    ASTNodePtr target;
    Token op;  // =, +=, -=, *=, /=, %=
    ASTNodePtr value;
    
    AssignmentExpr(ASTNodePtr tgt, const Token& o, ASTNodePtr val, int line = 0, int column = 0)
        : ASTNode(NodeType::ASSIGNMENT, line, column), target(tgt), op(o), value(val) {}
    
    std::string toString() const override;
};

/**
 * Array literal expression node
 * Represents: [1, 2, 3, 4]
 */
class ArrayLiteralExpr : public ASTNode {
public:
    std::vector<ASTNodePtr> elements;
    
    ArrayLiteralExpr(const std::vector<ASTNodePtr>& elems, int line = 0, int column = 0)
        : ASTNode(NodeType::ARRAY_LITERAL, line, column), elements(elems) {}
    
    std::string toString() const override;
};

/**
 * Await expression node (Async/Await)
 * Represents: await future_expression
 * 
 * Based on research_029_async_await_system.txt:
 * - Suspends execution if the Future is not ready
 * - Only valid within async functions or async blocks
 * - Operand must be a type implementing the Future trait
 */
class AwaitExpr : public ASTNode {
public:
    ASTNodePtr operand;  // Expression that yields a Future
    
    AwaitExpr(ASTNodePtr expr, int line = 0, int column = 0)
        : ASTNode(NodeType::AWAIT, line, column), operand(expr) {}
    
    std::string toString() const override;
};

/**
 * Move expression node
 * Represents: move(x) - explicit ownership transfer
 * 
 * Transfer ownership of a variable to the destination, invalidating the source.
 * Primarily used for wild memory to prevent use-after-free and double-free.
 * The borrow checker tracks moved variables and reports use-after-move errors.
 * 
 * Example:
 *   wild buffer:data = allocate(1024);
 *   wild buffer:moved = move(data);  // data is now invalid
 *   // use(data);  // ERROR: value used after move
 */
class MoveExpr : public ASTNode {
public:
    std::string variableName;  // Name of the variable being moved
    ASTNodePtr variable;       // Expression representing the variable (typically IdentifierExpr)
    
    MoveExpr(const std::string& varName, ASTNodePtr var, int line = 0, int column = 0)
        : ASTNode(NodeType::MOVE, line, column), 
          variableName(varName), variable(std::move(var)) {}
    
    std::string toString() const override;
};

/**
 * Cast expression node
 * Represents: @cast<TargetType>(expr) for checked casts, @cast_unchecked<T>(expr) for unchecked
 * 
 * Explicit type conversions in Aria's zero-implicit-conversion type system:
 * 
 * SAFE CASTS (widening - always succeed):
 *   @cast<int64>(int8_val)  // i8 → i64 (fits in range)
 *   @cast<f64>(f32_val)     // f32 → f64 (no precision loss)
 * 
 * CHECKED CASTS (narrowing - runtime validation):
 *   @cast<int8>(int64_val)  // Panics if value doesn't fit in int8 range
 *   @cast<f32>(f64_val)     // Panics if precision would be lost
 * 
 * UNCHECKED CASTS (truncation - performance, unsafe):
 *   @cast_unchecked<int8>(int64_val)  // Truncates without check, no panic
 * 
 * Philosophy: All type conversions must be explicit. The type system ensures
 * safety-critical code (AGI/robotics) has no hidden conversions that could
 * cause actuator limit violations or sensor reading corruption.
 */
class CastExpr : public ASTNode {
public:
    ASTNodePtr expression;     // Expression being cast
    ASTNodePtr targetTypeNode; // Type to cast to (TypeAnnotation node)
    std::string targetType;    // Target type name ("int64", "f32", etc.)
    bool isUnchecked;          // true for @cast_unchecked, false for @cast
    
    CastExpr(ASTNodePtr expr, ASTNodePtr typeNode, const std::string& targetTypeName, 
             bool unchecked = false, int line = 0, int column = 0)
        : ASTNode(NodeType::CAST, line, column),
          expression(std::move(expr)), targetTypeNode(std::move(typeNode)),
          targetType(targetTypeName), isUnchecked(unchecked) {}
    
    std::string toString() const override;
};

/**
 * Object literal expression node
 * Represents: { key: value, ... } syntax for both dynamic objects and struct initialization
 * 
 * Based on research_014_composite_types_part1.txt:
 * - When type_name is empty: Creates a dynamic obj (hash map)
 * - When type_name is set: Initializes a struct (e.g., Point{ x:1, y:2 })
 * - Fields are evaluated at runtime and added to the object
 */
class ObjectLiteralExpr : public ASTNode {
public:
    struct Field {
        std::string name;
        ASTNodePtr value;
        
        Field(const std::string& n, ASTNodePtr v)
            : name(n), value(std::move(v)) {}
    };
    
    std::vector<Field> fields;
    std::string type_name;  // Empty for dynamic obj, set for struct constructors
    
    ObjectLiteralExpr(const std::vector<Field>& flds, 
                      const std::string& typeName = "",
                      int line = 0, int column = 0)
        : ASTNode(NodeType::OBJECT_LITERAL, line, column), 
          fields(flds), type_name(typeName) {}
    
    ObjectLiteralExpr(int line = 0, int column = 0)
        : ASTNode(NodeType::OBJECT_LITERAL, line, column), type_name("") {}
    
    std::string toString() const override;
};

/**
 * Lambda expression node (Closures)
 * Represents: (x, y) => x + y  or  int64(int64 x, int64 y) { return x + y; }
 * 
 * Based on research_016_functional_types.txt:
 * - Fat pointer representation: { method_ptr, env_ptr }
 * - Captures can be by-value, by-reference, or by-move
 * - Must respect Appendage Theory for lifetime safety
 */
class LambdaExpr : public ASTNode {
public:
    enum class CaptureMode {
        BY_VALUE,      // Copy into closure environment
        BY_REFERENCE,  // Capture as pointer (borrowing)
        BY_MOVE        // Transfer ownership to closure
    };
    
    struct CapturedVar {
        std::string name;
        CaptureMode mode;
        ASTNodePtr typeAnnotation;  // Optional type hint
        
        CapturedVar(const std::string& n, CaptureMode m, ASTNodePtr type = nullptr)
            : name(n), mode(m), typeAnnotation(type) {}
    };
    
    std::vector<ASTNodePtr> parameters;      // Function parameters
    std::string returnTypeName;              // Return type annotation (string like "int64", "string")
    ASTNodePtr body;                         // Function body (BlockStmt)
    std::vector<CapturedVar> capturedVars;   // Variables captured from outer scope (filled by semantic analysis)
    bool isAsync;                            // true for async closures
    
    LambdaExpr(const std::vector<ASTNodePtr>& params, 
               const std::string& retType,
               ASTNodePtr bodyNode,
               int line = 0, int column = 0)
        : ASTNode(NodeType::LAMBDA, line, column),
          parameters(params), returnTypeName(retType), body(bodyNode), isAsync(false) {}
    
    std::string toString() const override;
};

/**
 * Unwrap expression node (? and ?? operators)
 * Represents: result ? default_value  (Result unwrap)
 *         or: nullable ?? default_value (Null coalescing)
 * 
 * ? operator unwraps a Result type, returning the value if successful,
 * or the default value if there was an error.
 * 
 * ?? operator checks if a pointer is null, returning the dereferenced
 * value if not null, or the default value if null.
 * 
 * Examples: 
 *   string:content = readFile("config.txt") ? "default";
 *   int32:value = ptr ?? 0i32;
 */
class UnwrapExpr : public ASTNode {
public:
    ASTNodePtr result;        // Expression that returns a result type or pointer
    ASTNodePtr defaultValue;  // Default value if result contains error or is null
    bool isNullCoalesce;      // true for ??, false for ? or !!
    bool isFailsafe;          // true for !!, false for ? or ??
    
    UnwrapExpr(ASTNodePtr res, ASTNodePtr defVal, int line = 0, int column = 0, bool nullCoalesce = false, bool failsafe = false)
        : ASTNode(NodeType::UNWRAP, line, column),
          result(res), defaultValue(defVal), isNullCoalesce(nullCoalesce), isFailsafe(failsafe) {}
    
    std::string toString() const override;
};

/**
 * Defaults expression node — scoped expression fallback (v0.4.3)
 * Represents: expr ?| fallback, expr defaults fallback
 *
 * Wraps the *entire* preceding sub-expression. If any intermediate Result
 * in that sub-expression produces ERR during evaluation, the whole chain
 * short-circuits to the fallback value.
 *
 * The fallback must be a literal, variable, or `unknown` — NOT a function call.
 */
class DefaultsExpr : public ASTNode {
public:
    ASTNodePtr expr;          // The sub-expression that may produce ERR at any point
    ASTNodePtr fallback;      // The fallback value (literal/variable/unknown only)
    
    DefaultsExpr(ASTNodePtr expression, ASTNodePtr fallbackVal, int line = 0, int column = 0)
        : ASTNode(NodeType::DEFAULTS, line, column),
          expr(std::move(expression)), fallback(std::move(fallbackVal)) {}
    
    std::string toString() const override {
        return "Defaults(" + expr->toString() + " ?| " + fallback->toString() + ")";
    }
};

/**
 * Vector constructor expression node
 * Represents: vec2(x, y), vec3(x, y, z), vec9(c0, c1, ..., c8)
 * 
 * Constructs a vector from component values.
 * Component types are inferred from the literals (default: flt32).
 * 
 * Examples:
 *   vec2(1.0, 2.0)           - 2D vector with flt32 components
 *   vec3(10.0, 20.0, 30.0)   - 3D vector with flt32 components
 *   vec9(1.0, 2.0, ..., 9.0) - 9D vector with flt32 components
 */
class VectorConstructorExpr : public ASTNode {
public:
    int dimension;                        // 2, 3, or 9
    std::vector<ASTNodePtr> components;   // Component expressions
    
    VectorConstructorExpr(int dim, const std::vector<ASTNodePtr>& comps, int line = 0, int column = 0)
        : ASTNode(NodeType::VECTOR_CONSTRUCTOR, line, column),
          dimension(dim), components(comps) {}
    
    std::string toString() const override;
};

/**
 * Compile-time expression node
 * Represents: comptime(expr) — forces compile-time evaluation of an expression
 * The inner expression must be evaluable by the const evaluator.
 */
class ComptimeExpr : public ASTNode {
public:
    ASTNodePtr expr;  // The expression to evaluate at compile time
    
    // Result computed by the type checker / const evaluator
    // Stored here so the IR generator can emit the constant directly
    int64_t intResult = 0;
    double floatResult = 0.0;
    bool boolResult = false;
    std::string stringResult;
    std::string resultTypeName;  // e.g. "int64", "flt64", "bool", "str"
    bool evaluated = false;      // true once the type checker has evaluated this
    
    ComptimeExpr(ASTNodePtr expression, int line = 0, int column = 0)
        : ASTNode(NodeType::COMPTIME_EXPR, line, column),
          expr(std::move(expression)) {}
    
    std::string toString() const override;
};

} // namespace aria

#endif // ARIA_EXPR_H

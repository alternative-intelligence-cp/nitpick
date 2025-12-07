#ifndef ARIA_FRONTEND_AST_EXPR_H
#define ARIA_FRONTEND_AST_EXPR_H

#include "../ast.h"
#include <string>
#include <memory>
#include <vector>

namespace aria {
namespace frontend {

// Base Expression Class
class Expression : public AstNode {
public:
    virtual ~Expression() = default;
};

// When Expression (Pattern Matching Expression)
// Example: when { x == 1 then 10; x == 2 then 20; end }
struct WhenCase {
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Expression> result;
};

class WhenExpr : public Expression {
public:
    std::vector<WhenCase> cases;
    std::unique_ptr<Expression> else_result;  // Optional else case

    WhenExpr() = default;

    void accept(AstVisitor& visitor) override {
        // visitor.visit(this);
    }
};

// Await Expression (Async/Await)
// Example: await asyncFunction()
class AwaitExpr : public Expression {
public:
    std::unique_ptr<Expression> expression;

    AwaitExpr(std::unique_ptr<Expression> expr)
        : expression(std::move(expr)) {}

    void accept(AstVisitor& visitor) override {
        // visitor.visit(this);
    }
};

// Variable Reference Expression
// Example: x, myVar
class VarExpr : public Expression {
public:
    std::string name;

    VarExpr(const std::string& n) : name(n) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Integer Literal Expression
// Example: 42, 0xFF, 512
class IntLiteral : public Expression {
public:
    int64_t value;

    IntLiteral(int64_t v) : value(v) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Float Literal Expression
// Example: 3.14, 1.5e10, 1e-5
class FloatLiteral : public Expression {
public:
    double value;

    FloatLiteral(double v) : value(v) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Boolean Literal Expression
// Example: true, false
class BoolLiteral : public Expression {
public:
    bool value;

    BoolLiteral(bool v) : value(v) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Null Literal Expression
// Example: NULL
class NullLiteral : public Expression {
public:
    NullLiteral() {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// String Literal Expression
// Example: "hello world", "whats up"
class StringLiteral : public Expression {
public:
    std::string value;

    StringLiteral(const std::string& v) : value(v) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Template String Part (for interpolation)
struct TemplatePart {
    enum Type { STRING, EXPR } type;
    std::string string_value;                    // If type == STRING
    std::unique_ptr<Expression> expr_value;      // If type == EXPR
    
    TemplatePart(const std::string& s) 
        : type(STRING), string_value(s) {}
    
    TemplatePart(std::unique_ptr<Expression> e) 
        : type(EXPR), expr_value(std::move(e)) {}
};

// Template String Expression
// Example: `Value is &{val}`, `Result: &{x + y}`
class TemplateString : public Expression {
public:
    std::vector<TemplatePart> parts;

    TemplateString() = default;

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Ternary Expression (is operator)
// Example: is x > 0 : positive : negative
class TernaryExpr : public Expression {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Expression> true_expr;
    std::unique_ptr<Expression> false_expr;

    TernaryExpr(std::unique_ptr<Expression> cond, 
                std::unique_ptr<Expression> true_val,
                std::unique_ptr<Expression> false_val)
        : condition(std::move(cond)), 
          true_expr(std::move(true_val)), 
          false_expr(std::move(false_val)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Unwrap Expression (? operator)
// Example: test2(3,5) ? -1  // If test2 returns error, use -1 as default
class UnwrapExpr : public Expression {
public:
    std::unique_ptr<Expression> expression;   // Expression that might fail
    std::unique_ptr<Expression> default_value;  // Default if error

    UnwrapExpr(std::unique_ptr<Expression> expr, std::unique_ptr<Expression> def)
        : expression(std::move(expr)), default_value(std::move(def)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Binary Operation Expression
// Example: a + b, x * y
class BinaryOp : public Expression {
public:
    enum OpType {
        ADD, SUB, MUL, DIV, MOD,
        EQ, NE, LT, GT, LE, GE,
        LOGICAL_AND, LOGICAL_OR,
        BITWISE_AND, BITWISE_OR, BITWISE_XOR,
        LSHIFT, RSHIFT,
        ASSIGN, PLUS_ASSIGN, MINUS_ASSIGN, STAR_ASSIGN, SLASH_ASSIGN, MOD_ASSIGN,
        AND_ASSIGN, OR_ASSIGN, XOR_ASSIGN, LSHIFT_ASSIGN, RSHIFT_ASSIGN
    };

    OpType op;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;

    BinaryOp(OpType o, std::unique_ptr<Expression> l, std::unique_ptr<Expression> r)
        : op(o), left(std::move(l)), right(std::move(r)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Unary Operation Expression
// Example: -x, !flag, ~bits, x++, x--
class UnaryOp : public Expression {
public:
    enum OpType {
        NEG,            // -
        LOGICAL_NOT,    // !
        BITWISE_NOT,    // ~
        POST_INC,       // x++
        POST_DEC,       // x--
        ADDRESS_OF,     // @ (address/pointer operator)
        PIN             // # (memory pinning operator)
    };

    OpType op;
    std::unique_ptr<Expression> operand;

    UnaryOp(OpType o, std::unique_ptr<Expression> opnd)
        : op(o), operand(std::move(opnd)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Function Call Expression
// Example: foo(a, b, c)
class CallExpr : public Expression {
public:
    std::string function_name;
    std::vector<std::unique_ptr<Expression>> arguments;

    CallExpr(const std::string& name) : function_name(name) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Object Literal Expression (for Result and anonymous objects)
// Example: { err: NULL, val: 42 }
// Also used for struct constructors: Point{ x: 10, y: 20 }
class ObjectLiteral : public Expression {
public:
    struct Field {
        std::string name;
        std::unique_ptr<Expression> value;
    };
    
    std::vector<Field> fields;
    std::string type_name;  // For struct constructors, stores the struct type name

    ObjectLiteral() = default;

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Member Access Expression
// Example: obj.field, result.err, result.val
class MemberAccess : public Expression {
public:
    std::unique_ptr<Expression> object;
    std::string member_name;
    bool is_safe;  // true for ?. operator

    MemberAccess(std::unique_ptr<Expression> obj, const std::string& member, bool safe = false)
        : object(std::move(obj)), member_name(member), is_safe(safe) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Array Literal Expression
// Example: [1, 2, 3, 4, 5]
class ArrayLiteral : public Expression {
public:
    std::vector<std::unique_ptr<Expression>> elements;

    ArrayLiteral() = default;

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Array Index Expression
// Example: arr[i], matrix[x][y]
class IndexExpr : public Expression {
public:
    std::unique_ptr<Expression> array;
    std::unique_ptr<Expression> index;

    IndexExpr(std::unique_ptr<Expression> arr, std::unique_ptr<Expression> idx)
        : array(std::move(arr)), index(std::move(idx)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Cast Expression
// Example: (int64)x, (BinaryFunc)wildx_buffer
class CastExpr : public Expression {
public:
    std::string target_type;  // Type to cast to
    std::unique_ptr<Expression> expression;  // Expression being cast

    CastExpr(const std::string& type, std::unique_ptr<Expression> expr)
        : target_type(type), expression(std::move(expr)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Forward declaration for lambda
struct FuncParam;
class Block;

// Lambda Expression (Anonymous Function)
// Example: int8(int8:a, int8:b) { return { err: NULL, val: a + b }; }
// Example with immediate execution: int8(int8:a){...}(10)
// Example with auto-wrap: *int8(int8:a){ return a; }  // Compiler wraps to {err:NULL, val:a}
class LambdaExpr : public Expression {
public:
    std::string return_type;
    std::vector<FuncParam> parameters;
    std::unique_ptr<Block> body;
    
    // Optional immediate call arguments
    bool is_immediately_invoked = false;
    std::vector<std::unique_ptr<Expression>> call_arguments;
    
    // Auto-wrap flag: if true, compiler wraps return values in {err:NULL, val:...}
    // Set when return type is prefixed with * (e.g., *int8)
    bool auto_wrap = false;

    LambdaExpr(const std::string& ret_type, std::vector<FuncParam> params, std::unique_ptr<Block> b)
        : return_type(ret_type), parameters(std::move(params)), body(std::move(b)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_AST_EXPR_H

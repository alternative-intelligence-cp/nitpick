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

// Binary Operation Expression
// Example: a + b, x * y
class BinaryOp : public Expression {
public:
    enum OpType {
        ADD, SUB, MUL, DIV, MOD,
        EQ, NE, LT, GT, LE, GE,
        LOGICAL_AND, LOGICAL_OR,
        BITWISE_AND, BITWISE_OR, BITWISE_XOR,
        LSHIFT, RSHIFT
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
// Example: -x, !flag, ~bits
class UnaryOp : public Expression {
public:
    enum OpType {
        NEG,            // -
        LOGICAL_NOT,    // !
        BITWISE_NOT     // ~
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

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_AST_EXPR_H

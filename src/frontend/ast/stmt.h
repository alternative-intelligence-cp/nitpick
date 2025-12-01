#ifndef ARIA_FRONTEND_AST_STMT_H
#define ARIA_FRONTEND_AST_STMT_H

#include "../ast.h"
#include "expr.h"
#include <string>
#include <memory>
#include <vector>

namespace aria {
namespace frontend {

// Base Statement Class
class Statement : public AstNode {
public:
    virtual ~Statement() = default;
};

// Variable Declaration Statement
// Example: int64:x = 42;
class VarDecl : public Statement {
public:
    std::string name;
    std::string type;
    std::unique_ptr<Expression> initializer;
    bool is_stack = false;
    bool is_wild = false;

    VarDecl(const std::string& n, const std::string& t)
        : name(n), type(t) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Return Statement
// Example: return 42;
class ReturnStmt : public Statement {
public:
    std::unique_ptr<Expression> value;

    ReturnStmt(std::unique_ptr<Expression> v)
        : value(std::move(v)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// If Statement
// Example: if (cond) { ... } else { ... }
class IfStmt : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Block> then_block;
    std::unique_ptr<Block> else_block;  // May be nullptr

    IfStmt(std::unique_ptr<Expression> cond, std::unique_ptr<Block> then_b)
        : condition(std::move(cond)), then_block(std::move(then_b)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_AST_STMT_H

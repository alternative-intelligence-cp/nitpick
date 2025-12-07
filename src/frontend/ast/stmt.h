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
    bool is_wildx = false;  // Executable memory for JIT compilation
    bool is_const = false;  // Bug #72: compile-time constant

    VarDecl(const std::string& t, const std::string& n, std::unique_ptr<Expression> init = nullptr)
        : name(n), type(t), initializer(std::move(init)) {}

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

    IfStmt(std::unique_ptr<Expression> cond, std::unique_ptr<Block> then_b, std::unique_ptr<Block> else_b = nullptr)
        : condition(std::move(cond)), then_block(std::move(then_b)), else_block(std::move(else_b)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Expression Statement
// Wraps an expression as a statement (e.g., function call)
class ExpressionStmt : public Statement {
public:
    std::unique_ptr<Expression> expression;

    ExpressionStmt(std::unique_ptr<Expression> expr)
        : expression(std::move(expr)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Function Parameter
struct FuncParam {
    std::string type;
    std::string name;
    std::unique_ptr<Expression> default_value;  // Optional default
    
    FuncParam(const std::string& t, const std::string& n, std::unique_ptr<Expression> def = nullptr)
        : type(t), name(n), default_value(std::move(def)) {}
};

// Struct Field
struct StructField {
    std::string type;
    std::string name;
    
    StructField(const std::string& t, const std::string& n)
        : type(t), name(n) {}
};

// Struct Declaration
// Example: const Point = struct { x: int64, y: int64, };
class StructDecl : public Statement {
public:
    std::string name;
    std::vector<StructField> fields;
    bool is_const = true;  // Structs are typically const type definitions
    
    StructDecl(const std::string& n, std::vector<StructField> f)
        : name(n), fields(std::move(f)) {}
    
    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Function Declaration (Bug #70: async support)
// Example: func:add = int8(int8:a, int8:b) { return {err:NULL, val:a+b}; }
// Example with auto-wrap: func:add = *int8(int8:a, int8:b) { return a+b; }
class FuncDecl : public Statement {
public:
    std::string name;
    std::vector<FuncParam> parameters;
    std::string return_type;
    std::unique_ptr<Block> body;
    bool is_async = false;  // Bug #70: async function support
    bool is_pub = false;    // public visibility
    bool auto_wrap = false; // Auto-wrap returns in {err:NULL, val:...}
    
    FuncDecl(const std::string& n, std::vector<FuncParam> params, const std::string& ret_type, std::unique_ptr<Block> b)
        : name(n), parameters(std::move(params)), return_type(ret_type), body(std::move(b)) {}
    
    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Async Block Statement (Bug #70: async blocks with catch)
// Example: async { ... } catch (err:e) { ... }
class AsyncBlock : public Statement {
public:
    std::unique_ptr<Block> body;
    std::unique_ptr<Block> catch_block;  // May be nullptr
    std::string error_var;               // Variable name for caught error
    
    AsyncBlock(std::unique_ptr<Block> b, std::unique_ptr<Block> catch_b = nullptr, const std::string& err_var = "")
        : body(std::move(b)), catch_block(std::move(catch_b)), error_var(err_var) {}
    
    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_AST_STMT_H

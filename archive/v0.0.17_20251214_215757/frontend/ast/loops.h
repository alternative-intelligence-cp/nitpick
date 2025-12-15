#ifndef ARIA_FRONTEND_AST_LOOPS_H
#define ARIA_FRONTEND_AST_LOOPS_H

#include "../ast.h"
#include "expr.h"
#include <memory>

namespace aria {
namespace frontend {

// Loop Statement (Range Loop with explicit start)
// Example: loop(1, 100, 1) { ... } or loop(100, 0, 2) { ... }
// Direction determined by start vs limit comparison
// Step is ALWAYS positive (magnitude only)
// Uses $ as iterator variable
class LoopStmt : public Statement {
public:
    std::unique_ptr<Expression> start;
    std::unique_ptr<Expression> limit;
    std::unique_ptr<Expression> step;
    std::unique_ptr<Block> body;

    LoopStmt(std::unique_ptr<Expression> st, std::unique_ptr<Expression> lim, 
             std::unique_ptr<Expression> stp, std::unique_ptr<Block> b)
        : start(std::move(st)), limit(std::move(lim)), 
          step(std::move(stp)), body(std::move(b)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Till Loop (Iteration Loop)
// Example: till(100, 1) { ... }
// Direction determined by step sign (positive = up from 0, negative = down from limit)
// Uses $ as iterator variable
class TillLoop : public Statement {
public:
    std::unique_ptr<Expression> limit;
    std::unique_ptr<Expression> step;
    std::unique_ptr<Block> body;

    TillLoop(std::unique_ptr<Expression> lim, std::unique_ptr<Expression> stp, std::unique_ptr<Block> b)
        : limit(std::move(lim)), step(std::move(stp)), body(std::move(b)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// For Loop (Iterator-based Loop)
// Example: for x in collection { ... }
class ForLoop : public Statement {
public:
    std::string iterator_name;
    std::unique_ptr<Expression> iterable;
    std::unique_ptr<Block> body;

    ForLoop(const std::string& iter, std::unique_ptr<Expression> itbl, std::unique_ptr<Block> b)
        : iterator_name(iter), iterable(std::move(itbl)), body(std::move(b)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// While Loop
// Example: while condition { ... }
class WhileLoop : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Block> body;

    WhileLoop(std::unique_ptr<Expression> cond, std::unique_ptr<Block> b)
        : condition(std::move(cond)), body(std::move(b)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Break Statement
// Example: break; or break(label);
class BreakStmt : public Statement {
public:
    std::string label;  // Optional label for multi-level breaks

    BreakStmt(const std::string& lbl = "") : label(lbl) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Continue Statement
// Example: continue; or continue(label);
class ContinueStmt : public Statement {
public:
    std::string label;  // Optional label for multi-level continues

    ContinueStmt(const std::string& lbl = "") : label(lbl) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_AST_LOOPS_H

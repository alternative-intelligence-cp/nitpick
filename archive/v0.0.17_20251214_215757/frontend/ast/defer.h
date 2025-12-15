#ifndef ARIA_FRONTEND_AST_DEFER_H
#define ARIA_FRONTEND_AST_DEFER_H

#include "../ast.h"
#include <memory>

namespace aria {
namespace frontend {

// Defer Statement
// Example: defer { cleanup(); }
// Executes the block when the current scope exits
class DeferStmt : public Statement {
public:
    std::unique_ptr<Block> body;

    DeferStmt(std::unique_ptr<Block> b)
        : body(std::move(b)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_AST_DEFER_H

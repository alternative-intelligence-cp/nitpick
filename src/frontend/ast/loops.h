#ifndef ARIA_FRONTEND_AST_LOOPS_H
#define ARIA_FRONTEND_AST_LOOPS_H

#include "../ast.h"
#include "expr.h"
#include <memory>

namespace aria {
namespace frontend {

// Till Loop (Iteration Loop)
// Example: till(100, 1) { ... }
// Iterates from 0 to limit with step, using $ as iterator variable
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

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_AST_LOOPS_H

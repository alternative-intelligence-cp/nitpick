#ifndef ARIA_FRONTEND_AST_CONTROL_FLOW_H
#define ARIA_FRONTEND_AST_CONTROL_FLOW_H

#include "../ast.h"
#include "stmt.h"
#include "expr.h"
#include <memory>
#include <vector>

namespace aria {
namespace frontend {

// Pick Case (Pattern Matching Case)
// Represents a single case in a pick statement
class PickCase {
public:
    enum CaseType {
        EXACT,          // Exact value match: (5)
        LESS_THAN,      // Less than: (<9)
        GREATER_THAN,   // Greater than: (>9)
        LESS_EQUAL,     // Less or equal: (<=9)
        GREATER_EQUAL,  // Greater or equal: (>=9)
        RANGE,          // Range match: (1..10) or (1...10)
        WILDCARD,       // Default case: (*)
        DESTRUCTURE_OBJ,// Object destructuring: ({ key: value })
        DESTRUCTURE_ARR,// Array destructuring: ([a, b, c])
        UNREACHABLE     // Labeled unreachable: label:(!)
    };

    CaseType type;
    std::string label;  // Optional label for fall() targets
    std::unique_ptr<Expression> value_start;
    std::unique_ptr<Expression> value_end;  // For range cases
    std::unique_ptr<Block> body;
    bool is_range_exclusive = false;  // true for ..., false for ..

    PickCase(CaseType t, std::unique_ptr<Block> b)
        : type(t), body(std::move(b)) {}
};

// Fall Statement (Explicit Fallthrough in pick)
// Example: fall(label);
class FallStmt : public Statement {
public:
    std::string target_label;

    FallStmt(const std::string& label) : target_label(label) {}

    void accept(AstVisitor& visitor) override {
        // visitor.visit(this);
    }
};

// Pick Statement (Pattern Matching)
// Example: pick (x) { 0 => { ... }, <9 => { ... }, _ => { ... } }
class PickStmt : public Statement {
public:
    std::unique_ptr<Expression> selector;
    std::vector<PickCase> cases;

    PickStmt(std::unique_ptr<Expression> sel)
        : selector(std::move(sel)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// When Loop (Conditional Loop)
// Example: when (condition) { ... }
class WhenLoop : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Block> body;

    WhenLoop(std::unique_ptr<Expression> cond, std::unique_ptr<Block> b)
        : condition(std::move(cond)), body(std::move(b)) {}

    void accept(AstVisitor& visitor) override {
        // Note: visit method needs to be added to AstVisitor if used
    }
};

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_AST_CONTROL_FLOW_H

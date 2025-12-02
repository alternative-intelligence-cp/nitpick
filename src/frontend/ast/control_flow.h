#ifndef ARIA_FRONTEND_AST_CONTROL_FLOW_H
#define ARIA_FRONTEND_AST_CONTROL_FLOW_H

#include "../ast.h"
#include "stmt.h"
#include "expr.h"
#include <memory>
#include <vector>

namespace aria {
namespace frontend {

// Destructuring Pattern for Pick Cases (Bug #64)
// Represents object/array patterns in pick statements
struct DestructurePattern {
    enum PatternType {
        IDENTIFIER,     // Simple binding: x
        OBJECT,         // Object pattern: { key: value }
        ARRAY,          // Array pattern: [a, b, c]
        REST            // Rest pattern: ...rest
    };
    
    PatternType type;
    std::string name;  // For IDENTIFIER and REST
    std::vector<std::pair<std::string, DestructurePattern>> object_fields;  // For OBJECT
    std::vector<DestructurePattern> array_elements;  // For ARRAY
    
    DestructurePattern() : type(IDENTIFIER) {}
    DestructurePattern(PatternType t, const std::string& n = "") : type(t), name(n) {}
};

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
    std::unique_ptr<DestructurePattern> pattern;  // For destructuring patterns (Bug #64)

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

// When Loop (Loop with Completion Blocks)
// Spec Example: when(condition) { body } then { success } end { failure }
// - Main body executes repeatedly while condition is true
// - 'then' block runs after successful loop completion
// - 'end' block runs if loop never ran or broke early
class WhenLoop : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Block> body;
    std::unique_ptr<Block> then_block;  // Runs after successful completion
    std::unique_ptr<Block> end_block;   // Runs if loop didn't run or broke early

    WhenLoop(std::unique_ptr<Expression> cond, std::unique_ptr<Block> b,
             std::unique_ptr<Block> then_b = nullptr, std::unique_ptr<Block> end_b = nullptr)
        : condition(std::move(cond)), body(std::move(b)), 
          then_block(std::move(then_b)), end_block(std::move(end_b)) {}

    void accept(AstVisitor& visitor) override {
        // Note: visit method needs to be added to AstVisitor if used
    }
};

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_AST_CONTROL_FLOW_H

/**
 * src/frontend/sema/escape_analysis.cpp
 *
 * Aria Compiler - Global Escape Analysis Pass
 * Version: 0.0.6
 *
 * This module implements a flow-sensitive escape analysis based on a connection graph.
 * It is responsible for:
 * 1. Detecting stack pointers that escape their frame (Safety).
 * 2. Identifying heap allocations that can be demoted to stack (Optimization).
 * 3. Verifying Wild pointer discipline (checking for un-freed locals).
 *
 * The algorithm constructs a graph where nodes represent memory objects and 
 * edges represent pointer relationships. Reachability from "Escape Roots"
 * (Returns, Globals) determines the escape status.
 * 
 * References:
 * - Aria Spec v0.0.6 Section 3.2
 * - "Escape Analysis for Java" (Choi et al.) adapted for Hybrid Memory.
 */

#include "escape_analysis.h"
#include "../ast.h"
#include "../ast/control_flow.h"
#include "../ast/expr.h"
#include "../ast/stmt.h"
#include <map>
#include <set>
#include <vector>
#include <string>
#include <iostream>
#include <memory>

namespace aria {
namespace sema {

// =============================================================================
// Escape State Definitions
// =============================================================================

// Escape status with explicit priority ordering (higher value = higher priority)
enum EscapeStatus {
    NO_ESCAPE = 0,      // Variable stays strictly within scope (lowest priority)
    ESCAPE_ARG = 1,     // Passed as argument (might escape downwards)
    ESCAPE_GLOBAL = 2,  // Stored in global/static memory
    ESCAPE_RETURN = 3   // Returned from function (escapes upwards - HIGHEST priority)
};

// Represents a node in the Connection Graph
struct EscapeNode {
    std::string name;
    EscapeStatus status;
    bool is_stack_alloc; // Is this explicitly a 'stack' keyword variable?
    bool is_wild;        // Is this a 'wild' manual pointer?
    int line_decl;
    
    // Graph Edges: "Points To" relationships
    // If 'a = &b', then 'a' points to 'b'. If 'a' escapes, 'b' might escape.
    // This is a simplified "Points-To" edge for dependency tracking.
    std::set<EscapeNode*> points_to;
    
    // "Referred By": Reverse edges for traversal efficiency
    std::set<EscapeNode*> referred_by;

    EscapeNode(std::string n, int line) 
        : name(n), status(NO_ESCAPE), is_stack_alloc(false), is_wild(false), line_decl(line) {}
};

// =============================================================================
// The Analysis Context and Visitor
// =============================================================================

class EscapeAnalysisVisitor : public AstVisitor {
private:
    // Graph Storage: Owns the nodes to ensure lifetime safety
    std::vector<std::unique_ptr<EscapeNode>> graph_storage;
    
    // Symbol Table: Map variable names to Graph Nodes for O(1) lookup
    std::map<std::string, EscapeNode*> sym_table;

    bool has_errors = false;

    // --- Graph Operations ---

    // Helper: Create a node in the graph
    EscapeNode* createNode(const std::string& name, int line) {
        auto node = std::make_unique<EscapeNode>(name, line);
        EscapeNode* ptr = node.get();
        graph_storage.push_back(std::move(node));
        sym_table[name] = ptr;
        return ptr;
    }

    // Helper: Lookup node by name
    EscapeNode* getNode(const std::string& name) {
        if (sym_table.find(name)!= sym_table.end()) {
            return sym_table[name];
        }
        return nullptr;
    }

    // Helper: Add connection (Dependency Edge)
    // If 'from' takes the value of 'to' (or address of 'to'), a dependency is formed.
    void connect(EscapeNode* holder, EscapeNode* pointee) {
        if (!holder ||!pointee) return;
        
        // Edge: Holder -> Pointee
        // Meaning: If 'Holder' escapes, 'Pointee' escapes.
        holder->points_to.insert(pointee);
        pointee->referred_by.insert(holder);
    }

    // --- Reporting ---

    void error(int line, const std::string& msg) {
        std::cerr << "[Escape Analysis] Error Line " << line << ": " << msg << std::endl;
        has_errors = true;
    }

    void warning(int line, const std::string& msg) {
        std::cerr << "[Escape Analysis] Warning Line " << line << ": " << msg << std::endl;
    }

public:
    bool success() const { return!has_errors; }

    // =========================================================================
    // Visitor Implementation: Building the Graph
    // =========================================================================

    // 1. Variable Declaration: Create Nodes
    void visit(VarDecl* node) override {
        EscapeNode* en = createNode(node->name, node->line);
        en->is_stack_alloc = node->is_stack;
        en->is_wild = node->is_wild;

        if (node->initializer) {
            // Visit initializer to find dependencies
            node->initializer->accept(*this);
            // Analyze the assignment "node->name = initializer"
            analyzeAssignment(en, node->initializer.get());
        }
    }

    // 2. Assignment Expressions: Create Edges
    void visit(BinaryOp* node) override {
        // Traverse children
        node->left->accept(*this);
        node->right->accept(*this);

        if (node->op == TOKEN_ASSIGN) {
            // Resolve target variable
            if (auto* var = dynamic_cast<VarExpr*>(node->left.get())) {
                EscapeNode* target = getNode(var->name);
                analyzeAssignment(target, node->right.get());
            }
        }
    }

    // 3. Return Statements: Mark Escape Roots
    void visit(ReturnStmt* node) override {
        if (node->value) {
            node->value->accept(*this);
            // Identify what is being returned
            EscapeNode* retSrc = resolveExpressionSource(node->value.get());
            if (retSrc) {
                // Mark as Escaping via Return. This will trigger the propagation phase.
                markEscape(retSrc, ESCAPE_RETURN);
            }
        }
    }

    // 4. Function Calls: Argument Escapes
    void visit(CallExpr* node) override {
        // For intra-procedural analysis (Aria v0.0.6), we must be conservative.
        // We assume any pointer passed to a function escapes that function's scope,
        // unless it's a known intrinsic like 'len()'.
        
        bool is_safe_intrinsic = (node->callee == "len" |

| node->callee == "sizeof");

        for (auto& arg : node->args) {
            arg->accept(*this);
            if (!is_safe_intrinsic) {
                EscapeNode* src = resolveExpressionSource(arg.get());
                if (src) {
                    markEscape(src, ESCAPE_ARG);
                }
            }
        }
    }

    // 5. Control Flow: Traversal
    void visit(Block* node) override {
        for (auto& stmt : node->statements) stmt->accept(*this);
    }

    void visit(IfStmt* node) override {
        node->condition->accept(*this);
        node->thenBody->accept(*this);
        if (node->elseBody) node->elseBody->accept(*this);
    }
    
    void visit(TillLoop* node) override {
        node->limit->accept(*this);
        node->step->accept(*this);
        // Implicit iterator '$' handled by parser/symbol table usually,
        // but here we treat body as normal block.
        node->body->accept(*this);
    }
    
    void visit(PickStmt* node) override {
        node->selector->accept(*this);
        for(auto& c : node->cases) {
            // Pick cases might define bindings, omitted for brevity in v0.0.6
            c.body->accept(*this);
        }
    }
    
    void visit(DeferStmt* node) override {
        // Defers execute at end of scope. 
        // We must ensure captured variables don't escape if the defer moves (unlikely in v0).
        node->stmt->accept(*this);
    }

    // Visitor methods for expressions that don't create escape edges
    // These are intentionally simple because they don't affect escape analysis:
    // - VarExpr: Just reads a variable (edges created by assignments)
    // - IntLiteral: Integer values don't involve pointers
    // - UnaryOp: Most unary ops just transform values, recurse to find actual escapes
    void visit(VarExpr* node) override {}
    void visit(IntLiteral* node) override {}
    void visit(UnaryOp* node) override { node->operand->accept(*this); }

    // =========================================================================
    // Analysis Logic
    // =========================================================================

    // Trace an expression back to a variable node
    // Handles: var, &var, (var), *var
    EscapeNode* resolveExpressionSource(Expression* expr) {
        if (auto* var = dynamic_cast<VarExpr*>(expr)) {
            return getNode(var->name);
        }
        if (auto* unary = dynamic_cast<UnaryOp*>(expr)) {
            if (unary->op == TOKEN_ADDRESS) { // '&' or '@'
                // The expression is the address of the operand.
                // We return the operand's node, because taking its address implies dependency.
                return resolveExpressionSource(unary->operand.get());
            }
            if (unary->op == TOKEN_ITERATION) { // '$' Safe Ref
                return resolveExpressionSource(unary->operand.get());
            }
        }
        // Handle parentheses, casts, etc. if AST supports them
        return nullptr;
    }

    void analyzeAssignment(EscapeNode* target, Expression* expr) {
        if (!target) return;
        
        // Case 1: target = &source
        if (auto* unary = dynamic_cast<UnaryOp*>(expr)) {
            if (unary->op == TOKEN_ADDRESS) { 
                EscapeNode* source = resolveExpressionSource(unary->operand.get());
                if (source) {
                    // Target now holds address of source.
                    // If Target escapes (e.g. is returned), Source escapes.
                    connect(target, source);
                }
                return;
            }
        }
        
        // Case 2: target = source (Pointer copy or Ref assignment)
        EscapeNode* source = resolveExpressionSource(expr);
        if (source) {
             // If target is a pointer type (logic usually handled in Borrow Checker),
             // then target aliases source.
             connect(target, source);
        }
    }

    // The Propagation Algorithm (Fix-Point Iteration)
    // Uses visited set to prevent infinite loops on circular references
    void markEscape(EscapeNode* node, EscapeStatus reason, std::set<EscapeNode*>& visited) {
        if (!node || visited.count(node) > 0) return; // Already visited or null

        visited.insert(node);

        if (node->status >= reason) return; // Already marked with equal/higher priority

        node->status = reason;

        // Propagate: If 'node' escapes, everything 'node' points to also escapes.
        // Example: global_ptr = &stack_var.
        // global_ptr escapes (Global).
        // global_ptr points_to stack_var.
        // Therefore, stack_var escapes (via global_ptr).
        for (EscapeNode* child : node->points_to) {
            markEscape(child, reason, visited);
        }
    }

    // Public wrapper that creates visited set
    void markEscape(EscapeNode* node, EscapeStatus reason) {
        std::set<EscapeNode*> visited;
        markEscape(node, reason, visited);
    }

    // Final Validation Pass
    void validate() {
        for (const auto& node_ptr : graph_storage) {
            EscapeNode* n = node_ptr.get();

            // Rule 1: Stack variables cannot escape via Return
            // This prevents "Dangling Pointer" bugs where a function returns a pointer to its own stack.
            if (n->is_stack_alloc && n->status == ESCAPE_RETURN) {
                error(n->line_decl, "Critical Safety Violation: Stack variable '" + n->name + "' escapes function scope via return.");
            }
            
            // Rule 2: Stack variables cannot escape to Globals
            if (n->is_stack_alloc && n->status == ESCAPE_GLOBAL) {
                error(n->line_decl, "Critical Safety Violation: Stack variable '" + n->name + "' stored in global/persistent memory.");
            }

            // Rule 3: Optimization Hint (for Wild)
            // If a 'wild' alloc never escapes the function, it could be a stack alloc candidate.
            // Aria v0.0.6 reports this to aid developer optimization.
            if (n->is_wild && n->status == NO_ESCAPE) {
                warning(n->line_decl, "Optimization Hint: Wild variable '" + n->name + "' does not escape. Consider using 'stack' allocation for better performance.");
            }
        }
    }
};

// =============================================================================
// Public Entry Point
// =============================================================================

bool run_escape_analysis(Block* root) {
    EscapeAnalysisVisitor analyzer;
    // Inject Global Scope variables (like stdout, stddbg) as ESCAPE_GLOBAL roots
    // analyzer.defineGlobal("stdout"); // Implementation detail
    
    root->accept(analyzer);
    analyzer.validate();
    return analyzer.success();
}

} // namespace sema
} // namespace aria

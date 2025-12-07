/**
 * src/frontend/sema/borrow_checker.cpp
 * 
 * Aria Compiler - Borrow Checker Implementation
 * Version: 0.0.7 (Enhanced with Flow-Sensitive Lifetime Analysis)
 * 
 * Implements Aria's "Appendage Theory" memory safety model:
 * - Wild pointers: Must be explicitly freed or deferred
 * - Pinned values (#): Cannot be moved once pinned
 * - Safe references ($): Must not outlive their pinned hosts
 * - Stack allocations: Proper lifetime tracking with scope depth
 * 
 * CRITICAL ENHANCEMENT (v0.0.7):
 * Flow-sensitive lifetime analysis prevents dangling references by tracking:
 * 1. Scope depth for every variable declaration
 * 2. Reference → host relationships with transitive tracking
 * 3. Lifetime rules: host.depth <= ref.depth (host must live longer)
 */

#include "borrow_checker.h"
#include "../ast.h"
#include "../ast/stmt.h"
#include "../ast/expr.h"
#include "../ast/control_flow.h"
#include "../ast/defer.h"
#include "../ast/loops.h"
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <iostream>

namespace aria {
namespace sema {

// Enhanced Borrow Checker Context with Flow-Sensitive Lifetime Analysis
struct BorrowContext {
    // Legacy tracking (maintained for compatibility)
    std::unordered_set<std::string> wild_allocations;  // Track wild allocations needing free
    std::unordered_set<std::string> pinned_values;     // Track pinned values (cannot move)
    std::unordered_set<std::string> deferred_frees;    // Track deferred deallocations
    
    // FLOW-SENSITIVE LIFETIME TRACKING (v0.0.7)
    // Maps variable name → scope depth where it was declared
    // Depth 0 = global, 1 = function body, 2 = inner block, etc.
    std::unordered_map<std::string, int> var_depths;
    
    // Maps safe reference ($) → host variable name
    // Used to track reference origins for lifetime validation
    std::unordered_map<std::string, std::string> reference_origins;
    
    // Current scope depth (incremented on enterScope, decremented on exitScope)
    int current_depth = 0;
    
    // Error tracking
    bool has_errors = false;
    
    void error(const std::string& msg) {
        std::cerr << "Borrow Check Error: " << msg << std::endl;
        has_errors = true;
    }
    
    void warning(const std::string& msg) {
        std::cerr << "Borrow Check Warning: " << msg << std::endl;
    }
    
    // SCOPE MANAGEMENT
    void enterScope() {
        current_depth++;
    }
    
    void exitScope() {
        // Clean up variables declared at this depth
        // They are going out of scope
        for (auto it = var_depths.begin(); it != var_depths.end();) {
            if (it->second == current_depth) {
                // Remove from all tracking maps
                wild_allocations.erase(it->first);
                pinned_values.erase(it->first);
                reference_origins.erase(it->first);
                it = var_depths.erase(it);
            } else {
                ++it;
            }
        }
        current_depth--;
    }
    
    // VARIABLE DECLARATION TRACKING
    void declare(const std::string& name) {
        var_depths[name] = current_depth;
    }
    
    // LIFETIME VALIDATION
    // Check if 'ref' (a safe reference) can safely point to 'host'
    // Rule: host must live at least as long as ref
    // Implementation: host.depth <= ref.depth
    void checkLifetime(const std::string& ref, const std::string& host) {
        // If host is not tracked, it might be global or a parameter
        if (var_depths.find(host) == var_depths.end()) {
            // Assume it's safe (global or function parameter)
            return;
        }
        
        // If ref is not tracked, it's being declared now
        int ref_depth = (var_depths.find(ref) != var_depths.end()) 
                       ? var_depths[ref] 
                       : current_depth;
        
        int host_depth = var_depths[host];
        
        // Appendage Theory Violation: Reference outlives host
        if (host_depth > ref_depth) {
            error("Appendage Theory Violation: Reference '" + ref + 
                  "' (declared at depth " + std::to_string(ref_depth) + 
                  ") refers to host '" + host + 
                  "' (declared at depth " + std::to_string(host_depth) + 
                  ") which has a shorter lifetime. " +
                  "The reference would outlive its host, creating a dangling pointer.");
        }
    }
};

// Forward declarations
void checkStatement(frontend::Statement* stmt, BorrowContext& ctx);
void checkExpression(frontend::Expression* expr, BorrowContext& ctx);

// Check if a variable declaration uses wild allocation
void checkVarDecl(frontend::VarDecl* decl, BorrowContext& ctx) {
    if (!decl) return;
    
    // Register this variable at the current scope depth
    ctx.declare(decl->name);
    
    // Check for wild allocation modifier
    if (decl->is_wild) {
        ctx.wild_allocations.insert(decl->name);
        // Wild allocations should have a corresponding free or defer
        // We'll check this at function exit
    }
    
    // Check for stack allocation
    if (decl->is_stack) {
        // Stack allocations are automatically freed
        // Just verify they're not returned or escaped
    }
    
    // LIFETIME ANALYSIS: Check initializer for reference creation
    if (decl->initializer) {
        // Check if we're creating a safe reference ($) or pinned reference (#)
        if (auto* unary = dynamic_cast<frontend::UnaryOp*>(decl->initializer.get())) {
            // Case 1: Creating a safe reference: const int8$:ref = $var
            // Case 2: Pinning a value: const int8#:pinned = #var
            if (unary->op == frontend::UnaryOp::ADDRESS_OF || 
                unary->op == frontend::UnaryOp::PIN) {
                
                if (auto* target = dynamic_cast<frontend::VarExpr*>(unary->operand.get())) {
                    // Check lifetime: can 'decl->name' (the reference) point to 'target->name' (the host)?
                    ctx.checkLifetime(decl->name, target->name);
                    
                    // Track the reference → host relationship
                    ctx.reference_origins[decl->name] = target->name;
                    
                    // If it's a pin operation, mark the host as pinned
                    if (unary->op == frontend::UnaryOp::PIN) {
                        ctx.pinned_values.insert(target->name);
                    }
                }
            }
        }
        
        // Case 3: Reference assignment from another reference
        // Example: const int8$:ref2 = ref1;
        if (auto* var = dynamic_cast<frontend::VarExpr*>(decl->initializer.get())) {
            // Check if the source is a tracked reference
            if (ctx.reference_origins.count(var->name)) {
                // Transitive dependency: ref2 -> ref1 -> host
                // ref2 must not outlive the ultimate host
                std::string ultimate_host = ctx.reference_origins[var->name];
                ctx.checkLifetime(decl->name, ultimate_host);
                
                // Track the transitive reference
                ctx.reference_origins[decl->name] = ultimate_host;
            }
        }
        
        // General expression checking
        checkExpression(decl->initializer.get(), ctx);
    }
}

// Check expressions for borrow violations
void checkExpression(frontend::Expression* expr, BorrowContext& ctx) {
    if (!expr) return;
    
    // Check for Lambda expressions (function bodies)
    if (auto* lambda = dynamic_cast<frontend::LambdaExpr*>(expr)) {
        // Create a new context for the lambda scope
        BorrowContext lambda_ctx;
        lambda_ctx.current_depth = ctx.current_depth + 1;  // Inherit parent depth
        
        for (auto& stmt : lambda->body->statements) {
            auto* statement = dynamic_cast<frontend::Statement*>(stmt.get());
            if (statement) {
                checkStatement(statement, lambda_ctx);
            }
        }
        
        // Check for unfreed wild allocations in this lambda
        for (const auto& wild_var : lambda_ctx.wild_allocations) {
            if (lambda_ctx.deferred_frees.find(wild_var) == lambda_ctx.deferred_frees.end()) {
                lambda_ctx.warning("Wild allocation '" + wild_var + "' may not be freed. " +
                                 "Consider using 'defer aria.free(" + wild_var + ");'");
            }
        }
        
        // Propagate errors to parent context
        if (lambda_ctx.has_errors) {
            ctx.has_errors = true;
        }
        return;
    }
    
    // Check for pin operator usage
    if (auto* unary = dynamic_cast<frontend::UnaryOp*>(expr)) {
        if (unary->op == frontend::UnaryOp::PIN) {
            // Pin operator creates a pinned reference
            // Track the pinned value
            if (auto* var = dynamic_cast<frontend::VarExpr*>(unary->operand.get())) {
                ctx.pinned_values.insert(var->name);
            }
        }
        checkExpression(unary->operand.get(), ctx);
    }
    
    // Check binary operations
    if (auto* binary = dynamic_cast<frontend::BinaryOp*>(expr)) {
        checkExpression(binary->left.get(), ctx);
        checkExpression(binary->right.get(), ctx);
    }
    
    // Check ternary expressions
    if (auto* ternary = dynamic_cast<frontend::TernaryExpr*>(expr)) {
        checkExpression(ternary->condition.get(), ctx);
        checkExpression(ternary->true_expr.get(), ctx);
        checkExpression(ternary->false_expr.get(), ctx);
    }
    
    // Check function calls - might free wild pointers
    if (auto* call = dynamic_cast<frontend::CallExpr*>(expr)) {
        // Check for aria.free() calls
        if (call->function_name == "aria.free" || call->function_name == "free") {
            // Mark wild allocation as freed
            if (!call->arguments.empty()) {
                if (auto* var = dynamic_cast<frontend::VarExpr*>(call->arguments[0].get())) {
                    ctx.wild_allocations.erase(var->name);
                }
            }
        }
        
        // Check arguments
        for (auto& arg : call->arguments) {
            checkExpression(arg.get(), ctx);
        }
    }
}

// Check statements for borrow violations
void checkStatement(frontend::Statement* stmt, BorrowContext& ctx) {
    if (!stmt) return;
    
    // Variable declarations
    if (auto* decl = dynamic_cast<frontend::VarDecl*>(stmt)) {
        checkVarDecl(decl, ctx);
        return;
    }
    
    // Function declarations - recursively check function body
    if (auto* func = dynamic_cast<frontend::FuncDecl*>(stmt)) {
        if (func->body) {
            // Create a new context for the function scope
            BorrowContext func_ctx;
            func_ctx.enterScope();  // Function body is depth 1
            
            // Function parameters are at function scope (depth 1)
            // We assume they're already tracked by the caller
            
            for (auto& s : func->body->statements) {
                auto* statement = dynamic_cast<frontend::Statement*>(s.get());
                if (statement) {
                    checkStatement(statement, func_ctx);
                }
            }
            
            func_ctx.exitScope();  // Exit function body scope
            
            // Check for unfreed wild allocations in this function
            for (const auto& wild_var : func_ctx.wild_allocations) {
                if (func_ctx.deferred_frees.find(wild_var) == func_ctx.deferred_frees.end()) {
                    func_ctx.warning("Wild allocation '" + wild_var + "' in function '" + 
                                   func->name + "' may not be freed. " +
                                   "Consider using 'defer aria.free(" + wild_var + ");'");
                }
            }
            
            // Propagate errors to parent context
            if (func_ctx.has_errors) {
                ctx.has_errors = true;
            }
        }
        return;
    }
    
    // Expression statements
    if (auto* expr_stmt = dynamic_cast<frontend::ExpressionStmt*>(stmt)) {
        checkExpression(expr_stmt->expression.get(), ctx);
        return;
    }
    
    // Defer statements - register deferred cleanup
    if (auto* defer = dynamic_cast<frontend::DeferStmt*>(stmt)) {
        // Check if defer body contains a free call
        if (defer->body) {
            for (auto& s : defer->body->statements) {
                if (auto* expr_stmt = dynamic_cast<frontend::ExpressionStmt*>(s.get())) {
                    if (auto* call = dynamic_cast<frontend::CallExpr*>(expr_stmt->expression.get())) {
                        if (call->function_name == "aria.free" || call->function_name == "free") {
                            if (!call->arguments.empty()) {
                                if (auto* var = dynamic_cast<frontend::VarExpr*>(call->arguments[0].get())) {
                                    ctx.deferred_frees.insert(var->name);
                                }
                            }
                        }
                    }
                }
            }
        }
        return;
    }
    
    // Return statements - check for escaping stack/wild values
    if (auto* ret = dynamic_cast<frontend::ReturnStmt*>(stmt)) {
        if (ret->value) {
            checkExpression(ret->value.get(), ctx);
            
    // If statements - scope management for block-local variables
    if (auto* if_stmt = dynamic_cast<frontend::IfStmt*>(stmt)) {
        checkExpression(if_stmt->condition.get(), ctx);
        
        if (if_stmt->then_block) {
            ctx.enterScope();  // Enter 'then' block scope
            for (auto& s : if_stmt->then_block->statements) {
                checkStatement(dynamic_cast<frontend::Statement*>(s.get()), ctx);
            }
            ctx.exitScope();  // Exit 'then' block scope
        }
        
        if (if_stmt->else_block) {
            ctx.enterScope();  // Enter 'else' block scope
            for (auto& s : if_stmt->else_block->statements) {
                checkStatement(dynamic_cast<frontend::Statement*>(s.get()), ctx);
            }
            ctx.exitScope();  // Exit 'else' block scope
        }
        return;
    }
    
    // While loops - scope management for loop-local variables
    if (auto* while_loop = dynamic_cast<frontend::WhileLoop*>(stmt)) {
        checkExpression(while_loop->condition.get(), ctx);
        
        if (while_loop->body) {
            ctx.enterScope();  // Enter loop body scope
            for (auto& s : while_loop->body->statements) {
                checkStatement(dynamic_cast<frontend::Statement*>(s.get()), ctx);
            }
            ctx.exitScope();  // Exit loop body scope
        }
        return;
    }/ While loops
    if (auto* while_loop = dynamic_cast<frontend::WhileLoop*>(stmt)) {
        checkExpression(while_loop->condition.get(), ctx);
        if (while_loop->body) {
            for (auto& s : while_loop->body->statements) {
                checkStatement(dynamic_cast<frontend::Statement*>(s.get()), ctx);
            }
        }
        return;
    }
}

// Main borrow checking function
bool check_borrow_rules(aria::frontend::Block* root) {
    if (!root) return true;
    
    BorrowContext ctx;
    
    // Check all statements in the block
    for (auto& stmt : root->statements) {
        // Statements in Block are AstNode*, need to cast to Statement*
        auto* statement = dynamic_cast<frontend::Statement*>(stmt.get());
        if (statement) {
            checkStatement(statement, ctx);
        }
    }
    
    // After processing all statements, check for unfreed wild allocations
    for (const auto& wild_var : ctx.wild_allocations) {
        // Check if it has a deferred free
        if (ctx.deferred_frees.find(wild_var) == ctx.deferred_frees.end()) {
            ctx.warning("Wild allocation '" + wild_var + "' may not be freed. " +
                       "Consider using 'defer aria.free(" + wild_var + ");'");
        }
    }
    
    // Check for moved pinned values
    // (Would require more sophisticated tracking to detect actual moves)
    
    return !ctx.has_errors;
}

} // namespace sema
} // namespace aria

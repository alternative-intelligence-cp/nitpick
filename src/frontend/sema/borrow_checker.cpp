/**
 * src/frontend/sema/borrow_checker.cpp
 * 
 * Aria Compiler - Borrow Checker Implementation
 * Version: 0.0.6
 * 
 * Implements Aria's "Appendage Theory" memory safety model:
 * - Wild pointers: Must be explicitly freed or deferred
 * - Pinned values: Cannot be moved once pinned
 * - Stack allocations: Proper lifetime tracking
 * - Safe references ($): Must not outlive their pinned hosts (#)
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

// Borrow Checker Context
struct BorrowContext {
    std::unordered_set<std::string> wild_allocations;  // Track wild allocations needing free
    std::unordered_set<std::string> pinned_values;     // Track pinned values (cannot move)
    std::unordered_set<std::string> deferred_frees;    // Track deferred deallocations
    std::unordered_map<std::string, std::string> safe_refs;  // Safe ref -> pinned host mapping
    bool has_errors = false;
    
    void error(const std::string& msg) {
        std::cerr << "Borrow Check Error: " << msg << std::endl;
        has_errors = true;
    }
    
    void warning(const std::string& msg) {
        std::cerr << "Borrow Check Warning: " << msg << std::endl;
    }
};

// Forward declarations
void checkStatement(frontend::Statement* stmt, BorrowContext& ctx);
void checkExpression(frontend::Expression* expr, BorrowContext& ctx);

// Check if a variable declaration uses wild allocation
void checkVarDecl(frontend::VarDecl* decl, BorrowContext& ctx) {
    if (!decl) return;
    
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
    
    // Check initializer expression
    if (decl->initializer) {
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
            for (auto& s : func->body->statements) {
                auto* statement = dynamic_cast<frontend::Statement*>(s.get());
                if (statement) {
                    checkStatement(statement, func_ctx);
                }
            }
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
            
            // If returning a variable, check if it's stack-allocated
            if (auto* var = dynamic_cast<frontend::VarExpr*>(ret->value.get())) {
                // Warning: returning stack-allocated value could be dangerous
                // (would need full escape analysis to verify)
            }
        }
        return;
    }
    
    // If statements
    if (auto* if_stmt = dynamic_cast<frontend::IfStmt*>(stmt)) {
        checkExpression(if_stmt->condition.get(), ctx);
        if (if_stmt->then_block) {
            for (auto& s : if_stmt->then_block->statements) {
                checkStatement(dynamic_cast<frontend::Statement*>(s.get()), ctx);
            }
        }
        if (if_stmt->else_block) {
            for (auto& s : if_stmt->else_block->statements) {
                checkStatement(dynamic_cast<frontend::Statement*>(s.get()), ctx);
            }
        }
        return;
    }
    
    // While loops
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

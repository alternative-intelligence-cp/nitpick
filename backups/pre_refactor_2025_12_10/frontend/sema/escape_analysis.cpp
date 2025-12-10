/**
 * src/frontend/sema/escape_analysis.cpp
 * 
 * Aria Compiler - Escape Analysis Implementation
 * Version: 0.0.6
 * 
 * Implements escape analysis to detect when local values (stack or wild)
 * escape their scope, which could lead to dangling pointers or use-after-free.
 */

#include "escape_analysis.h"
#include "../ast.h"
#include "../ast/stmt.h"
#include "../ast/expr.h"
#include "../ast/control_flow.h"
#include "../ast/defer.h"
#include "../ast/loops.h"
#include <unordered_set>
#include <string>
#include <iostream>

namespace aria {
namespace sema {

// Escape Analysis Context
struct EscapeContext {
    std::unordered_set<std::string> stack_locals;   // Stack-allocated variables
    std::unordered_set<std::string> wild_locals;    // Wild-allocated variables
    std::unordered_set<std::string> wildx_locals;   // WildX (executable) allocated variables - SECURITY CRITICAL
    std::unordered_set<std::string> escaped_vars;   // Variables that have escaped
    int escape_count = 0;
    bool has_errors = false;
    bool has_wildx_violations = false;  // Track critical wildx violations
    
    void warning(const std::string& msg) {
        std::cerr << "Escape Analysis Warning: " << msg << std::endl;
    }
    
    void error(const std::string& msg) {
        std::cerr << "Escape Analysis Error: " << msg << std::endl;
        has_errors = true;
    }
    
    void security_error(const std::string& msg) {
        std::cerr << "\n*** SECURITY VIOLATION ***" << std::endl;
        std::cerr << "WildX Escape Analysis Error: " << msg << std::endl;
        std::cerr << "WildX pointers (executable memory) MUST NOT escape their scope." << std::endl;
        std::cerr << "This is a critical security violation that could enable code injection." << std::endl;
        std::cerr << "*** END SECURITY VIOLATION ***\n" << std::endl;
        has_errors = true;
        has_wildx_violations = true;
    }
};

// Forward declarations
void analyzeStatement(frontend::Statement* stmt, EscapeContext& ctx);
void analyzeExpression(frontend::Expression* expr, EscapeContext& ctx, bool is_escaping);

// Check if expression references a wildx pointer (SECURITY CRITICAL)
bool referencesWildX(frontend::Expression* expr, const EscapeContext& ctx) {
    if (!expr) return false;
    
    // Direct wildx variable reference
    if (auto* var = dynamic_cast<frontend::VarExpr*>(expr)) {
        return ctx.wildx_locals.find(var->name) != ctx.wildx_locals.end();
    }
    
    // Address-of wildx
    if (auto* unary = dynamic_cast<frontend::UnaryOp*>(expr)) {
        if (unary->op == frontend::UnaryOp::ADDRESS_OF) {
            return referencesWildX(unary->operand.get(), ctx);
        }
    }
    
    // Member access on wildx
    if (auto* member = dynamic_cast<frontend::MemberAccess*>(expr)) {
        return referencesWildX(member->object.get(), ctx);
    }
    
    return false;
}

// Check if expression references a local variable that shouldn't escape
bool referencesLocal(frontend::Expression* expr, const EscapeContext& ctx) {
    if (!expr) return false;
    
    // Check for direct variable reference
    if (auto* var = dynamic_cast<frontend::VarExpr*>(expr)) {
        return ctx.stack_locals.find(var->name) != ctx.stack_locals.end() ||
               ctx.wildx_locals.find(var->name) != ctx.wildx_locals.end();
    }
    
    // Check for address-of operator on local
    if (auto* unary = dynamic_cast<frontend::UnaryOp*>(expr)) {
        if (unary->op == frontend::UnaryOp::ADDRESS_OF) {
            return referencesLocal(unary->operand.get(), ctx);
        }
    }
    
    return false;
}

// Analyze variable declaration
void analyzeVarDecl(frontend::VarDecl* decl, EscapeContext& ctx) {
    if (!decl) return;
    
    // Track stack allocations
    if (decl->is_stack) {
        ctx.stack_locals.insert(decl->name);
    }
    
    // Track wildx allocations (SECURITY CRITICAL)
    // WildX = executable memory - highest security concern
    if (decl->is_wildx) {
        ctx.wildx_locals.insert(decl->name);
        // Note: wildx implies wild, but we track separately for stricter checks
    }
    
    // Track wild allocations
    if (decl->is_wild && !decl->is_wildx) {
        ctx.wild_locals.insert(decl->name);
    }
    
    // Analyze initializer
    if (decl->initializer) {
        analyzeExpression(decl->initializer.get(), ctx, false);
    }
}

// Analyze expression for escaping
void analyzeExpression(frontend::Expression* expr, EscapeContext& ctx, bool is_escaping) {
    if (!expr) return;
    
    // Lambda expressions (function bodies)
    if (auto* lambda = dynamic_cast<frontend::LambdaExpr*>(expr)) {
        // Create new scope for lambda
        EscapeContext lambda_ctx;
        for (auto& stmt : lambda->body->statements) {
            auto* statement = dynamic_cast<frontend::Statement*>(stmt.get());
            if (statement) {
                analyzeStatement(statement, lambda_ctx);
            }
        }
        // Propagate errors
        if (lambda_ctx.has_errors) {
            ctx.has_errors = true;
        }
        return;
    }
    
    // Unary operations
    if (auto* unary = dynamic_cast<frontend::UnaryOp*>(expr)) {
        // Address-of operator creates a pointer that could escape
        if (unary->op == frontend::UnaryOp::ADDRESS_OF && is_escaping) {
            // CRITICAL: Check for wildx address leakage
            if (referencesWildX(unary->operand.get(), ctx)) {
                auto* var = dynamic_cast<frontend::VarExpr*>(unary->operand.get());
                if (var) {
                    ctx.security_error("Taking address of wildx pointer '" + var->name + 
                                     "' in escaping context. WildX addresses must never escape.");
                } else {
                    ctx.security_error("Taking address of wildx memory in escaping context. " +
                                     std::string("WildX addresses must never escape."));
                }
                ctx.escape_count++;
            } else if (referencesLocal(unary->operand.get(), ctx)) {
                auto* var = dynamic_cast<frontend::VarExpr*>(unary->operand.get());
                if (var) {
                    ctx.warning("Taking address of stack variable '" + var->name + 
                              "' that may escape its scope");
                    ctx.escaped_vars.insert(var->name);
                    ctx.escape_count++;
                }
            }
        }
        analyzeExpression(unary->operand.get(), ctx, is_escaping);
        return;
    }
    
    // Binary operations
    if (auto* binary = dynamic_cast<frontend::BinaryOp*>(expr)) {
        analyzeExpression(binary->left.get(), ctx, is_escaping);
        analyzeExpression(binary->right.get(), ctx, is_escaping);
        return;
    }
    
    // Cast expressions - check for wildx to dyn casts
    if (auto* cast = dynamic_cast<frontend::CastExpr*>(expr)) {
        // =====================================================================
        // SECURITY CHECK: WildX to dyn type cast
        // =====================================================================
        // Casting wildx to dyn (dynamic/generic type) is forbidden without
        // explicit runtime verification because it enables type confusion
        // attacks on executable memory.
        if (referencesWildX(cast->expression.get(), ctx)) {
            // Check if target type is 'dyn' or contains 'dyn'
            if (cast->target_type.find("dyn") != std::string::npos) {
                ctx.security_error("Casting wildx pointer to 'dyn' type is FORBIDDEN without runtime verification. " +
                                 std::string("This could enable type confusion attacks on executable memory."));
                ctx.escape_count++;
            }
        }
        analyzeExpression(cast->expression.get(), ctx, is_escaping);
        return;
    }
    
    // Ternary expressions
    if (auto* ternary = dynamic_cast<frontend::TernaryExpr*>(expr)) {
        analyzeExpression(ternary->condition.get(), ctx, false);
        analyzeExpression(ternary->true_expr.get(), ctx, is_escaping);
        analyzeExpression(ternary->false_expr.get(), ctx, is_escaping);
        return;
    }
    
    // Function calls
    if (auto* call = dynamic_cast<frontend::CallExpr*>(expr)) {
        // =====================================================================
        // SECURITY CHECK: WildX pointers passed to functions
        // =====================================================================
        // Passing wildx to external functions is dangerous unless the function
        // is explicitly marked as wildx-safe. For now, we ban all escapes.
        for (auto& arg : call->arguments) {
            if (referencesWildX(arg.get(), ctx)) {
                ctx.security_error("Passing wildx pointer to function '" + call->function_name + 
                                 "'. WildX pointers (executable memory) should not be passed to generic functions. " +
                                 "This could enable code injection if the function stores or returns the pointer.");
                ctx.escape_count++;
            }
            // Arguments might escape to the called function
            analyzeExpression(arg.get(), ctx, true);
        }
        return;
    }
}

// Analyze return statement for escaping values
void analyzeReturn(frontend::ReturnStmt* ret, EscapeContext& ctx) {
    if (!ret || !ret->value) return;
    
    // =========================================================================
    // CRITICAL SECURITY CHECK: WildX pointers MUST NEVER escape
    // =========================================================================
    // WildX pointers point to executable memory. Allowing them to escape
    // creates a code injection vector. This is a HARD ERROR, not a warning.
    if (referencesWildX(ret->value.get(), ctx)) {
        if (auto* var = dynamic_cast<frontend::VarExpr*>(ret->value.get())) {
            ctx.security_error("Returning wildx pointer '" + var->name + 
                             "' is FORBIDDEN. WildX pointers (executable memory) must never escape their scope.");
        } else {
            ctx.security_error("Return value contains wildx pointer. " 
                             "WildX pointers (executable memory) must never escape their scope.");
        }
        ctx.escaped_vars.insert("<wildx_return>");
        ctx.escape_count++;
        return;  // Don't perform further checks if wildx violation detected
    }
    
    // Check if returning a stack-allocated variable
    if (auto* var = dynamic_cast<frontend::VarExpr*>(ret->value.get())) {
        if (ctx.stack_locals.find(var->name) != ctx.stack_locals.end()) {
            ctx.error("Returning stack-allocated variable '" + var->name + 
                     "' which will be destroyed after function returns");
            ctx.escaped_vars.insert(var->name);
            ctx.escape_count++;
        }
    }
    
    // Check if returning address of local
    if (referencesLocal(ret->value.get(), ctx)) {
        ctx.warning("Return value may reference local stack variable");
    }
    
    // Analyze the return value expression
    analyzeExpression(ret->value.get(), ctx, true);
}

// Analyze statement for escape violations
void analyzeStatement(frontend::Statement* stmt, EscapeContext& ctx) {
    if (!stmt) return;
    
    // Variable declarations
    if (auto* decl = dynamic_cast<frontend::VarDecl*>(stmt)) {
        analyzeVarDecl(decl, ctx);
        return;
    }
    
    // Return statements - critical for escape analysis
    if (auto* ret = dynamic_cast<frontend::ReturnStmt*>(stmt)) {
        analyzeReturn(ret, ctx);
        return;
    }
    
    // Expression statements
    if (auto* expr_stmt = dynamic_cast<frontend::ExpressionStmt*>(stmt)) {
        analyzeExpression(expr_stmt->expression.get(), ctx, false);
        return;
    }
    
    // If statements
    if (auto* if_stmt = dynamic_cast<frontend::IfStmt*>(stmt)) {
        analyzeExpression(if_stmt->condition.get(), ctx, false);
        if (if_stmt->then_block) {
            for (auto& s : if_stmt->then_block->statements) {
                analyzeStatement(dynamic_cast<frontend::Statement*>(s.get()), ctx);
            }
        }
        if (if_stmt->else_block) {
            for (auto& s : if_stmt->else_block->statements) {
                analyzeStatement(dynamic_cast<frontend::Statement*>(s.get()), ctx);
            }
        }
        return;
    }
    
    // While loops
    if (auto* while_loop = dynamic_cast<frontend::WhileLoop*>(stmt)) {
        analyzeExpression(while_loop->condition.get(), ctx, false);
        if (while_loop->body) {
            for (auto& s : while_loop->body->statements) {
                analyzeStatement(dynamic_cast<frontend::Statement*>(s.get()), ctx);
            }
        }
        return;
    }
    
    // Defer statements
    if (auto* defer = dynamic_cast<frontend::DeferStmt*>(stmt)) {
        if (defer->body) {
            for (auto& s : defer->body->statements) {
                analyzeStatement(dynamic_cast<frontend::Statement*>(s.get()), ctx);
            }
        }
        return;
    }
}

// Main escape analysis function
EscapeAnalysisResult run_escape_analysis(aria::frontend::Block* root) {
    if (!root) {
        EscapeAnalysisResult result;
        return result;
    }
    
    EscapeContext ctx;
    
    // Analyze all statements
    for (auto& stmt : root->statements) {
        auto* statement = dynamic_cast<frontend::Statement*>(stmt.get());
        if (statement) {
            analyzeStatement(statement, ctx);
        }
    }
    
    // Build result
    EscapeAnalysisResult result;
    result.has_escapes = !ctx.escaped_vars.empty();
    result.escaped_count = ctx.escape_count;
    result.has_wildx_violations = ctx.has_wildx_violations;
    
    return result;
}

} // namespace sema
} // namespace aria

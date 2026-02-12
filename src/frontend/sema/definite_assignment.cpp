#include "frontend/sema/definite_assignment.h"
#include <sstream>

namespace aria {
namespace sema {

// ============================================================================
// AssignmentContext Implementation
// ============================================================================

void AssignmentContext::enterScope() {
    current_depth++;
    scope_stack.push_back({});
}

void AssignmentContext::exitScope() {
    if (current_depth > 0) {
        // Remove variables declared in this scope
        if (!scope_stack.empty()) {
            for (const auto& var : scope_stack.back()) {
                var_states.erase(var);
            }
            scope_stack.pop_back();
        }
        current_depth--;
    }
}

AssignmentContext AssignmentContext::snapshot() const {
    AssignmentContext snap;
    snap.var_states = this->var_states;
    snap.current_depth = this->current_depth;
    snap.scope_stack = this->scope_stack;
    return snap;
}

void AssignmentContext::restore(const AssignmentContext& snap) {
    this->var_states = snap.var_states;
    this->current_depth = snap.current_depth;
    this->scope_stack = snap.scope_stack;
}

void AssignmentContext::merge(const AssignmentContext& then_state, const AssignmentContext& else_state) {
    // Conservative merging: variable is ASSIGNED only if ASSIGNED in BOTH branches
    
    // Collect all variables from both states
    std::unordered_set<std::string> all_vars;
    for (const auto& [name, _] : then_state.var_states) {
        all_vars.insert(name);
    }
    for (const auto& [name, _] : else_state.var_states) {
        all_vars.insert(name);
    }
    
    // Merge each variable's state
    for (const auto& name : all_vars) {
        auto then_it = then_state.var_states.find(name);
        auto else_it = else_state.var_states.find(name);
        
        AssignmentState then_st = (then_it != then_state.var_states.end()) 
            ? then_it->second : AssignmentState::UNASSIGNED;
        AssignmentState else_st = (else_it != else_state.var_states.end())
            ? else_it->second : AssignmentState::UNASSIGNED;
        
        // Intersection logic:
        if (then_st == AssignmentState::ASSIGNED && else_st == AssignmentState::ASSIGNED) {
            // Both branches definitely assign
            var_states[name] = AssignmentState::ASSIGNED;
        } else if (then_st == AssignmentState::UNASSIGNED && else_st == AssignmentState::UNASSIGNED) {
            // Neither branch assigns
            var_states[name] = AssignmentState::UNASSIGNED;
        } else {
            // One branch assigns, other doesn't (or mixed states)
            var_states[name] = AssignmentState::MAYBE_ASSIGNED;
        }
    }
}

void AssignmentContext::assign(const std::string& name) {
    var_states[name] = AssignmentState::ASSIGNED;
}

AssignmentState AssignmentContext::getState(const std::string& name) const {
    auto it = var_states.find(name);
    if (it != var_states.end()) {
        return it->second;
    }
    // Variable not in our tracking → either undefined or from outer scope
    // For definite assignment, we assume outer scope variables are assigned
    return AssignmentState::ASSIGNED;
}

// ============================================================================
// DefiniteAssignmentAnalyzer Implementation
// ============================================================================

std::vector<AssignmentError> DefiniteAssignmentAnalyzer::analyze(ASTNode* ast) {
    if (!ast) {
        return errors;
    }
    
    // Reset state
    errors.clear();
    ctx = AssignmentContext();
    
    // Handle module structure like TypeChecker does
    if (ast->type == ASTNode::NodeType::BLOCK || ast->type == ASTNode::NodeType::PROGRAM) {
        BlockStmt* blockStmt = static_cast<BlockStmt*>(ast);
        for (const auto& stmt : blockStmt->statements) {
            if (stmt) {
                checkStatement(stmt.get());
            }
        }
    } else {
        // Single statement module (edge case)
        checkStatement(ast);
    }
    
    return errors;
}

void DefiniteAssignmentAnalyzer::addError(const std::string& message, ASTNode* node) {
    AssignmentError error;
    error.message = message;
    error.node = node;
    error.line = node ? node->line : 0;
    error.column = node ? node->column : 0;
    errors.push_back(error);
}

bool DefiniteAssignmentAnalyzer::isAssignmentTarget(ASTNode* node) {
    // Check if this node is the target of an assignment
    // This is a heuristic - in a full implementation, we'd track this in the AST
    return false;  // Placeholder
}

// ============================================================================
// Statement Handlers
// ============================================================================

void DefiniteAssignmentAnalyzer::checkStatement(ASTNode* stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case ASTNode::NodeType::VAR_DECL:
            checkVarDecl(static_cast<VarDeclStmt*>(stmt));
            break;
            
        case ASTNode::NodeType::BINARY_OP: {
            BinaryExpr* binop = static_cast<BinaryExpr*>(stmt);
            if (binop->op.lexeme == "=") {
                checkAssignment(binop);
            } else {
                checkExpression(binop);
            }
            break;
        }
            
        case ASTNode::NodeType::IF:
            checkIfStmt(static_cast<IfStmt*>(stmt));
            break;
            
        case ASTNode::NodeType::WHILE:
            checkWhileStmt(static_cast<WhileStmt*>(stmt));
            break;
            
        case ASTNode::NodeType::FOR:
            checkForStmt(static_cast<ForStmt*>(stmt));
            break;
            
        case ASTNode::NodeType::BLOCK:
            checkBlock(static_cast<BlockStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::EXPRESSION_STMT: {
            // Expression statement - unwrap and check the expression
            ExpressionStmt* exprStmt = static_cast<ExpressionStmt*>(stmt);
            if (exprStmt->expression) {
                checkExpression(exprStmt->expression.get());
            }
            break;
        }
            
        case ASTNode::NodeType::FUNC_DECL:  {
            FuncDeclStmt* func = static_cast<FuncDeclStmt*>(stmt);
            if (func->body) {
                ctx.enterScope();
                // Register parameters as assigned (they come from caller)
                for (const auto& paramPtr : func->parameters) {
                    if (paramPtr && paramPtr->type == ASTNode::NodeType::PARAMETER) {
                        ParameterNode* param = static_cast<ParameterNode*>(paramPtr.get());
                        ctx.var_states[param->paramName] = AssignmentState::ASSIGNED;
                        if (!ctx.scope_stack.empty()) {
                            ctx.scope_stack.back().push_back(param->paramName);
                        }
                    }
                }
                checkStatement(func->body.get());
                ctx.exitScope();
            }
            break;
        }
            
        default:
            // For other statements, check as expression
            checkExpression(stmt);
            break;
    }
}

void DefiniteAssignmentAnalyzer::checkVarDecl(VarDeclStmt* decl) {
    if (!decl) return;
    
    // Register variable as UNASSIGNED initially (unless it's wild)
    // Wild variables allow manual field initialization without complete initialization
    if (decl->isWild) {
        ctx.var_states[decl->varName] = AssignmentState::ASSIGNED;
    } else {
        ctx.var_states[decl->varName] = AssignmentState::UNASSIGNED;
    }
    
    // Add to current scope
    if (!ctx.scope_stack.empty()) {
        ctx.scope_stack.back().push_back(decl->varName);
    }
    
    // If there's an initializer, check it first, then mark as assigned
    if (decl->initializer) {
        checkExpression(decl->initializer.get());
        // After initialization, variable is definitely assigned
        ctx.var_states[decl->varName] = AssignmentState::ASSIGNED;
    }
}

void DefiniteAssignmentAnalyzer::checkAssignment(BinaryExpr* assign) {
    if (!assign || assign->op.lexeme != "=") return;
    
    // Check RHS first (may reference variables)
    checkExpression(assign->right.get());
    
    // Mark LHS as assigned (but don't check it - it's being written, not read)
    if (assign->left->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(assign->left.get());
        ctx.assign(ident->name);
    }
    // Note: We deliberately don't call checkExpression(assign->left) because
    // the LHS is being assigned to, not read from
}

void DefiniteAssignmentAnalyzer::checkIfStmt(IfStmt* ifStmt) {
    if (!ifStmt) return;
    
    // Check condition
    checkExpression(ifStmt->condition.get());
    
    // Snapshot state before branches
    AssignmentContext before_branches = ctx.snapshot();
    
    // Analyze then branch
    ctx.restore(before_branches);
    checkStatement(ifStmt->thenBranch.get());
    AssignmentContext after_then = ctx.snapshot();
    
    // Analyze else branch (if it exists)
    AssignmentContext after_else;
    if (ifStmt->elseBranch) {
        ctx.restore(before_branches);
        checkStatement(ifStmt->elseBranch.get());
        after_else = ctx.snapshot();
    } else {
        // No else branch → else path doesn't assign anything
        after_else = before_branches;
    }
    
    // Merge states from both branches
    ctx.restore(before_branches);
    ctx.merge(after_then, after_else);
}

void DefiniteAssignmentAnalyzer::checkWhileStmt(WhileStmt* whileStmt) {
    if (!whileStmt) return;
    
    // Check condition
    checkExpression(whileStmt->condition.get());
    
    // Loop body might not execute (condition could be false)
    // So variables assigned in loop are MAYBE_ASSIGNED after loop
    AssignmentContext before_loop = ctx.snapshot();
    
    // Analyze loop body
    checkStatement(whileStmt->body.get());
    AssignmentContext after_loop = ctx.snapshot();
    
    // Conservative merge: assume loop might not execute
    ctx.restore(before_loop);
    
    // Mark any variables assigned in loop as MAYBE_ASSIGNED
    for (const auto& [name, state] : after_loop.var_states) {
        if (state == AssignmentState::ASSIGNED) {
            auto before_it = before_loop.var_states.find(name);
            if (before_it == before_loop.var_states.end() ||
                before_it->second != AssignmentState::ASSIGNED) {
                // Was assigned in loop but not before
                ctx.var_states[name] = AssignmentState::MAYBE_ASSIGNED;
            }
        }
    }
}

void DefiniteAssignmentAnalyzer::checkForStmt(ForStmt* forStmt) {
    if (!forStmt) return;
    
    ctx.enterScope();
    
    // Check initializer
    if (forStmt->initializer) {
        checkStatement(forStmt->initializer.get());
    }
    
    // Check condition
    if (forStmt->condition) {
        checkExpression(forStmt->condition.get());
    }
    
    // Similar to while: loop might not execute
    AssignmentContext before_loop = ctx.snapshot();
    
    // Analyze body
    checkStatement(forStmt->body.get());
    
    // Check update
    if (forStmt->update) {
        checkExpression(forStmt->update.get());
    }
    
    AssignmentContext after_loop = ctx.snapshot();
    
    // Conservative merge
    ctx.restore(before_loop);
    for (const auto& [name, state] : after_loop.var_states) {
        if (state == AssignmentState::ASSIGNED) {
            auto before_it = before_loop.var_states.find(name);
            if (before_it == before_loop.var_states.end() ||
                before_it->second != AssignmentState::ASSIGNED) {
                ctx.var_states[name] = AssignmentState::MAYBE_ASSIGNED;
            }
        }
    }
    
    ctx.exitScope();
}

void DefiniteAssignmentAnalyzer::checkBlock(BlockStmt* block) {
    if (!block) return;
    
    ctx.enterScope();
    
    for (const auto& stmt : block->statements) {
        checkStatement(stmt.get());
    }
    
    ctx.exitScope();
}

// ============================================================================
// Expression Handlers
// ============================================================================

void DefiniteAssignmentAnalyzer::checkExpression(ASTNode* expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case ASTNode::NodeType::IDENTIFIER:
            checkIdentifier(static_cast<IdentifierExpr*>(expr));
            break;
            
        case ASTNode::NodeType::BINARY_OP: {
            BinaryExpr* binop = static_cast<BinaryExpr*>(expr);
            // Handle assignment specially - don't check LHS as it's being written
            if (binop->op.lexeme == "=") {
                checkAssignment(binop);
            } else {
                // Other binary operations - check both sides
                checkExpression(binop->left.get());
                checkExpression(binop->right.get());
            }
            break;
        }
            
        case ASTNode::NodeType::UNARY_OP: {
            UnaryExpr* unary = static_cast<UnaryExpr*>(expr);
            checkExpression(unary->operand.get());
            break;
        }
            
        case ASTNode::NodeType::CALL: {
            CallExpr* call = static_cast<CallExpr*>(expr);
            checkExpression(call->callee.get());
            for (const auto& arg : call->arguments) {
                checkExpression(arg.get());
            }
            break;
        }
            
        case ASTNode::NodeType::INDEX: {
            IndexExpr* index = static_cast<IndexExpr*>(expr);
            checkExpression(index->array.get());
            checkExpression(index->index.get());
            break;
        }
            
        case ASTNode::NodeType::MEMBER_ACCESS: {
            MemberAccessExpr* member = static_cast<MemberAccessExpr*>(expr);
            checkExpression(member->object.get());
            break;
        }
            
        default:
            // Literals, etc. - no variable reads
            break;
    }
}

void DefiniteAssignmentAnalyzer::checkIdentifier(IdentifierExpr* ident) {
    if (!ident) return;
    
    // Check if variable is being read (not assigned to)
    AssignmentState state = ctx.getState(ident->name);
    
    if (state == AssignmentState::UNASSIGNED) {
        std::ostringstream msg;
        msg << "Use of uninitialized variable '" << ident->name << "'";
        addError(msg.str(), ident);
    } else if (state == AssignmentState::MAYBE_ASSIGNED) {
        std::ostringstream msg;
        msg << "Use of possibly uninitialized variable '" << ident->name << "'";
        addError(msg.str(), ident);
    }
    // ASSIGNED → OK, no error
}

} // namespace sema
} // namespace aria

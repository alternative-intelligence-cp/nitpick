#include "frontend/sema/borrow_checker.h"
#include <sstream>
#include <algorithm>

namespace aria {
namespace sema {

// ============================================================================
// LifetimeContext Implementation
// ============================================================================

void LifetimeContext::enterScope() {
    current_depth++;
    scope_stack.push_back({});
}

void LifetimeContext::exitScope() {
    if (current_depth > 0) {
        // Pop variables declared in this scope
        if (!scope_stack.empty()) {
            scope_stack.pop_back();
        }
        current_depth--;
    }
}

LifetimeContext LifetimeContext::snapshot() const {
    LifetimeContext snap;
    snap.var_depths = this->var_depths;
    snap.loan_origins = this->loan_origins;
    snap.active_loans = this->active_loans;
    snap.active_pins = this->active_pins;
    snap.pending_wild_frees = this->pending_wild_frees;
    snap.wild_states = this->wild_states;
    snap.current_depth = this->current_depth;
    snap.scope_stack = this->scope_stack;
    return snap;
}

void LifetimeContext::restore(const LifetimeContext& snap) {
    this->var_depths = snap.var_depths;
    this->loan_origins = snap.loan_origins;
    this->active_loans = snap.active_loans;
    this->active_pins = snap.active_pins;
    this->pending_wild_frees = snap.pending_wild_frees;
    this->wild_states = snap.wild_states;
    this->current_depth = snap.current_depth;
    this->scope_stack = snap.scope_stack;
}

void LifetimeContext::merge(const LifetimeContext& then_state, const LifetimeContext& else_state) {
    // Conservative merging for safety
    // A variable is valid only if valid in ALL branches
    
    // Merge wild states: variable is freed only if freed in BOTH branches
    for (auto& [var, state] : then_state.wild_states) {
        auto else_it = else_state.wild_states.find(var);
        if (else_it != else_state.wild_states.end()) {
            // Variable exists in both branches
            if (state == WildState::FREED && else_it->second == WildState::FREED) {
                wild_states[var] = WildState::FREED;
            } else if (state == WildState::MOVED && else_it->second == WildState::MOVED) {
                wild_states[var] = WildState::MOVED;
            } else {
                // Conservative: if states differ, treat as allocated
                wild_states[var] = WildState::ALLOCATED;
            }
        }
    }
    
    // Merge pending frees: variable needs free if it needs free in ANY branch
    pending_wild_frees = then_state.pending_wild_frees;
    for (const auto& var : else_state.pending_wild_frees) {
        pending_wild_frees.insert(var);
    }
    
    // Remove variables that were freed in both branches
    for (auto& [var, state] : wild_states) {
        if (state == WildState::FREED) {
            pending_wild_frees.erase(var);
        }
    }
    
    // Merge loan origins: union of origins from both branches (phi node logic)
    for (auto& [ref, origins] : then_state.loan_origins) {
        auto else_it = else_state.loan_origins.find(ref);
        if (else_it != else_state.loan_origins.end()) {
            // Merge origins from both branches
            loan_origins[ref] = origins;
            loan_origins[ref].insert(else_it->second.begin(), else_it->second.end());
        }
    }
}

// ============================================================================
// BorrowChecker Implementation
// ============================================================================

std::vector<BorrowError> BorrowChecker::analyze(ASTNode* ast) {
    if (!ast) {
        return errors;
    }
    
    // Reset state
    errors.clear();
    ctx = LifetimeContext();
    
    // Analyze the AST
    checkStatement(ast);
    
    return errors;
}

// ============================================================================
// Lifetime Tracking (Phase 3.3.1)
// ============================================================================

void BorrowChecker::registerVariable(const std::string& name, ASTNode* node) {
    // Register variable at current depth
    ctx.var_depths[name] = ctx.current_depth;
    
    // Add to current scope's variable list
    if (!ctx.scope_stack.empty()) {
        ctx.scope_stack.back().push_back(name);
    }
}

int BorrowChecker::getVariableDepth(const std::string& name) const {
    auto it = ctx.var_depths.find(name);
    if (it != ctx.var_depths.end()) {
        return it->second;
    }
    return -1;  // Variable not found
}

bool BorrowChecker::validateLifetime(const std::string& host, const std::string& reference, ASTNode* node) {
    int host_depth = getVariableDepth(host);
    int ref_depth = getVariableDepth(reference);
    
    if (host_depth == -1) {
        addError("Cannot borrow undefined variable '" + host + "'", node);
        return false;
    }
    
    if (ref_depth == -1) {
        // Reference not yet registered (this is normal during initialization)
        return true;
    }
    
    // Appendage Theory: Depth(Host) <= Depth(Reference)
    // Host must be declared in outer or equal scope
    if (host_depth > ref_depth) {
        addError("Reference '" + reference + "' cannot outlive host '" + host + "' " +
                "(host declared in inner scope)", node);
        return false;
    }
    
    return true;
}

// ============================================================================
// Borrow Rules Enforcement (Phase 3.3.2)
// ============================================================================

void BorrowChecker::recordBorrow(const std::string& host, const std::string& reference,
                                 bool is_mutable, ASTNode* node) {
    // Validate lifetime first
    if (!validateLifetime(host, reference, node)) {
        return;
    }
    
    // Check borrow rules
    if (!checkBorrowRules(host, is_mutable, node)) {
        return;
    }
    
    // Record the loan
    Loan loan(reference, is_mutable, node->line, node->column);
    ctx.active_loans[host].push_back(loan);
    
    // Record origin for lifetime tracking
    ctx.loan_origins[reference].insert(host);
}

bool BorrowChecker::checkBorrowRules(const std::string& host, bool is_mutable, ASTNode* node) {
    // Check if variable is pinned (pinned variables have restricted borrowing)
    if (isPinned(host)) {
        if (is_mutable) {
            addError("Cannot borrow pinned variable '" + host + "' as mutable", node);
            return false;
        }
    }
    
    // Get existing loans for this host
    auto it = ctx.active_loans.find(host);
    if (it == ctx.active_loans.end() || it->second.empty()) {
        // No existing loans, borrow is allowed
        return true;
    }
    
    const auto& loans = it->second;
    
    // Rule: 1 mutable XOR N immutable references
    if (is_mutable) {
        // Requesting mutable borrow
        // Error if ANY loans exist (mutable or immutable)
        const Loan& first = loans[0];
        addError("Cannot borrow '" + host + "' as mutable because it is already borrowed",
                node,
                "Existing " + std::string(first.is_mutable ? "mutable" : "immutable") + 
                " borrow by '" + first.borrower + "' created here",
                first.creation_line, first.creation_column);
        return false;
    } else {
        // Requesting immutable borrow
        // Error if a mutable loan exists
        for (const auto& loan : loans) {
            if (loan.is_mutable) {
                addError("Cannot borrow '" + host + "' as immutable because it is already borrowed as mutable",
                        node,
                        "Mutable borrow by '" + loan.borrower + "' created here",
                        loan.creation_line, loan.creation_column);
                return false;
            }
        }
        // Multiple immutable borrows are allowed
        return true;
    }
}

void BorrowChecker::releaseBorrows(const std::string& var) {
    // Remove all loans where this variable is the borrower
    for (auto& [host, loans] : ctx.active_loans) {
        loans.erase(
            std::remove_if(loans.begin(), loans.end(),
                [&var](const Loan& loan) { return loan.borrower == var; }),
            loans.end()
        );
    }
    
    // Remove from loan origins
    ctx.loan_origins.erase(var);
    
    // Check if any other variables borrowed from this one
    std::vector<std::string> invalidated_refs;
    for (const auto& [ref, origins] : ctx.loan_origins) {
        if (origins.count(var) > 0) {
            invalidated_refs.push_back(ref);
        }
    }
    
    // Note: We don't remove these here, the error is reported at scope exit
    // This allows for proper error messages about dangling references
}

// ============================================================================
// Pinning Support (Phase 3.3.2)
// ============================================================================

void BorrowChecker::recordPin(const std::string& host, const std::string& pin_ref, ASTNode* node) {
    // Check if already pinned
    if (isPinned(host)) {
        addError("Variable '" + host + "' is already pinned by '" + 
                ctx.active_pins[host] + "'", node);
        return;
    }
    
    // Validate lifetime
    if (!validateLifetime(host, pin_ref, node)) {
        return;
    }
    
    // Record the pin
    ctx.active_pins[host] = pin_ref;
    ctx.loan_origins[pin_ref].insert(host);
}

bool BorrowChecker::isPinned(const std::string& var) const {
    return ctx.active_pins.find(var) != ctx.active_pins.end();
}

void BorrowChecker::releasePin(const std::string& var) {
    // Find and remove pins where this variable is the pin reference
    std::vector<std::string> to_unpin;
    for (const auto& [host, pin_ref] : ctx.active_pins) {
        if (pin_ref == var) {
            to_unpin.push_back(host);
        }
    }
    
    for (const auto& host : to_unpin) {
        ctx.active_pins.erase(host);
    }
}

// ============================================================================
// Wild Memory Safety (Phase 3.3.3)
// ============================================================================

void BorrowChecker::recordWildAlloc(const std::string& var, ASTNode* node) {
    ctx.pending_wild_frees.insert(var);
    ctx.wild_states[var] = WildState::ALLOCATED;
}

void BorrowChecker::recordWildFree(const std::string& var, ASTNode* node) {
    // Check if variable is allocated
    auto it = ctx.wild_states.find(var);
    if (it == ctx.wild_states.end()) {
        addError("Cannot free undefined variable '" + var + "'", node);
        return;
    }
    
    if (it->second == WildState::FREED) {
        addError("Double free of variable '" + var + "' (already freed)", node);
        return;
    }
    
    if (it->second == WildState::MOVED) {
        addError("Cannot free moved variable '" + var + "'", node);
        return;
    }
    
    // Mark as freed
    ctx.wild_states[var] = WildState::FREED;
    ctx.pending_wild_frees.erase(var);
}

bool BorrowChecker::checkWildUse(const std::string& var, ASTNode* node) {
    auto it = ctx.wild_states.find(var);
    if (it == ctx.wild_states.end()) {
        // Not a wild variable, no check needed
        return true;
    }
    
    if (it->second == WildState::FREED) {
        addError("Use after free: variable '" + var + "' was already freed", node);
        return false;
    }
    
    if (it->second == WildState::MOVED) {
        addError("Use after move: variable '" + var + "' was moved", node);
        return false;
    }
    
    return true;
}

void BorrowChecker::checkForLeaks() {
    // Check for wild memory that wasn't freed
    if (!ctx.scope_stack.empty()) {
        const auto& current_scope_vars = ctx.scope_stack.back();
        
        for (const auto& var : current_scope_vars) {
            if (ctx.pending_wild_frees.count(var) > 0) {
                addError("Memory leak: wild variable '" + var + "' was not freed before scope exit",
                        1, 1);  // We don't have node info at scope exit, use line 1
            }
        }
    }
}

// ============================================================================
// AST Traversal
// ============================================================================

void BorrowChecker::checkStatement(ASTNode* stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case ASTNode::NodeType::VAR_DECL:
            checkVarDecl(static_cast<VarDeclStmt*>(stmt));
            break;
            
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
            checkBlockStmt(static_cast<BlockStmt*>(stmt));
            break;
            
        case ASTNode::NodeType::RETURN:
            checkReturnStmt(static_cast<ReturnStmt*>(stmt));
            break;
            
        case ASTNode::NodeType::EXPRESSION_STMT: {
            auto* exprStmt = static_cast<ExpressionStmt*>(stmt);
            if (exprStmt->expression) {
                checkExpression(exprStmt->expression.get());
            }
            break;
        }
            
        case ASTNode::NodeType::BINARY_OP:
            checkBinaryExpr(static_cast<BinaryExpr*>(stmt));
            break;
            
        default:
            // Other statement types don't require borrow checking
            break;
    }
}

void BorrowChecker::checkExpression(ASTNode* expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case ASTNode::NodeType::BINARY_OP:
            checkBinaryExpr(static_cast<BinaryExpr*>(expr));
            break;
            
        case ASTNode::NodeType::UNARY_OP:
            checkUnaryExpr(static_cast<UnaryExpr*>(expr));
            break;
            
        case ASTNode::NodeType::IDENTIFIER:
            checkIdentifier(static_cast<IdentifierExpr*>(expr));
            break;
            
        case ASTNode::NodeType::CALL:
            checkCallExpr(static_cast<CallExpr*>(expr));
            break;
            
        default:
            // Other expression types don't require borrow checking
            break;
    }
}

// ============================================================================
// Statement Visitors
// ============================================================================

void BorrowChecker::checkVarDecl(VarDeclStmt* stmt) {
    if (!stmt) return;
    
    // Register the variable first
    registerVariable(stmt->varName, stmt);
    
    // Check initializer if present
    if (stmt->initializer) {
        checkExpression(stmt->initializer.get());
        
        // Check if initializer is a borrow or pin operation
        if (stmt->initializer->type == ASTNode::NodeType::UNARY_OP) {
            UnaryExpr* unary = static_cast<UnaryExpr*>(stmt->initializer.get());
            
            // Check for $ (borrow) or # (pin)
            if (unary->op.type == frontend::TokenType::TOKEN_DOLLAR) {
                // Borrow operation: int32$:ref = $x
                if (unary->operand->type == ASTNode::NodeType::IDENTIFIER) {
                    IdentifierExpr* host = static_cast<IdentifierExpr*>(unary->operand.get());
                    bool is_mutable = stmt->typeName.find("$mut") != std::string::npos;
                    recordBorrow(host->name, stmt->varName, is_mutable, stmt);
                }
            } else if (unary->op.type == frontend::TokenType::TOKEN_HASH) {
                // Pin operation: wild int32*:ptr = #x
                // This creates a pointer to GC memory, NOT a wild allocation
                if (unary->operand->type == ASTNode::NodeType::IDENTIFIER) {
                    IdentifierExpr* host = static_cast<IdentifierExpr*>(unary->operand.get());
                    recordPin(host->name, stmt->varName, stmt);
                }
                // Don't record as wild allocation - it's just a pin
                return;
            }
        }
    }
    
    // Check if this is a wild allocation (only if NOT a pin operation)
    // A pin creates a wild pointer to GC memory, not a wild allocation
    if (stmt->typeName.find("wild") == 0 && stmt->initializer) {
        // Check if initializer is not a pin operation
        bool is_pin = false;
        if (stmt->initializer->type == ASTNode::NodeType::UNARY_OP) {
            UnaryExpr* unary = static_cast<UnaryExpr*>(stmt->initializer.get());
            if (unary->op.type == frontend::TokenType::TOKEN_HASH) {
                is_pin = true;
            }
        }
        
        if (!is_pin) {
            // This is an actual wild allocation (e.g., wild int32:ptr = malloc(...))
            recordWildAlloc(stmt->varName, stmt);
        }
    }
}

void BorrowChecker::checkAssignment(BinaryExpr* expr) {
    if (!expr) return;
    
    // Check left side (ensure it's not pinned or borrowed mutably)
    if (expr->left->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* target = static_cast<IdentifierExpr*>(expr->left.get());
        
        // Check if target is pinned
        if (isPinned(target->name)) {
            addError("Cannot assign to pinned variable '" + target->name + "'", expr);
            return;
        }
        
        // Check if target is borrowed
        auto it = ctx.active_loans.find(target->name);
        if (it != ctx.active_loans.end() && !it->second.empty()) {
            const Loan& loan = it->second[0];
            addError("Cannot assign to '" + target->name + "' because it is borrowed",
                    expr,
                    "Borrowed by '" + loan.borrower + "' here",
                    loan.creation_line, loan.creation_column);
            return;
        }
        
        // Check for use-after-free
        checkWildUse(target->name, expr);
    }
    
    // Check right side
    checkExpression(expr->right.get());
}

void BorrowChecker::checkIfStmt(IfStmt* stmt) {
    if (!stmt) return;
    
    // Check condition
    checkExpression(stmt->condition.get());
    
    // Snapshot state before branching
    auto pre_branch_state = ctx.snapshot();
    
    // Analyze THEN branch
    ctx.enterScope();
    if (stmt->thenBranch) {
        checkStatement(stmt->thenBranch.get());
    }
    ctx.exitScope();
    auto then_state = ctx.snapshot();
    
    // Restore and analyze ELSE branch
    ctx.restore(pre_branch_state);
    LifetimeContext else_state;
    if (stmt->elseBranch) {
        ctx.enterScope();
        checkStatement(stmt->elseBranch.get());
        ctx.exitScope();
        else_state = ctx.snapshot();
    } else {
        // No else block, else state is same as pre-branch
        else_state = pre_branch_state;
    }
    
    // Merge states
    ctx.merge(then_state, else_state);
}

void BorrowChecker::checkWhileStmt(WhileStmt* stmt) {
    if (!stmt) return;
    
    // Check condition
    checkExpression(stmt->condition.get());
    
    // Enter loop scope
    ctx.enterScope();
    
    // Check body
    if (stmt->body) {
        checkStatement(stmt->body.get());
    }
    
    // Exit loop scope
    ctx.exitScope();
}

void BorrowChecker::checkForStmt(ForStmt* stmt) {
    if (!stmt) return;
    
    // Enter loop scope
    ctx.enterScope();
    
    // Check initializer
    if (stmt->initializer) {
        checkStatement(stmt->initializer.get());
    }
    
    // Check condition
    if (stmt->condition) {
        checkExpression(stmt->condition.get());
    }
    
    // Check body
    if (stmt->body) {
        checkStatement(stmt->body.get());
    }
    
    // Check update
    if (stmt->update) {
        checkExpression(stmt->update.get());
    }
    
    // Exit loop scope
    ctx.exitScope();
}

void BorrowChecker::checkBlockStmt(BlockStmt* stmt) {
    if (!stmt) return;
    
    // Enter block scope
    ctx.enterScope();
    
    // Check all statements in the block
    for (const auto& s : stmt->statements) {
        checkStatement(s.get());
    }
    
    // Check for memory leaks before exiting scope
    checkForLeaks();
    
    // Release borrows and pins for variables going out of scope
    if (!ctx.scope_stack.empty()) {
        const auto& dying_vars = ctx.scope_stack.back();
        for (const auto& var : dying_vars) {
            releaseBorrows(var);
            releasePin(var);
            
            // Check for dangling references
            for (const auto& [ref, origins] : ctx.loan_origins) {
                if (origins.count(var) > 0) {
                    int ref_depth = getVariableDepth(ref);
                    if (ref_depth >= 0 && ref_depth < ctx.current_depth) {
                        addError("Reference '" + ref + "' outlives its host '" + var + "'",
                                1, 1);  // Line info not available at scope exit
                    }
                }
            }
        }
    }
    
    // Exit block scope
    ctx.exitScope();
}

void BorrowChecker::checkReturnStmt(ReturnStmt* stmt) {
    if (!stmt) return;
    
    // Check return value
    if (stmt->value) {
        checkExpression(stmt->value.get());
        
        // TODO: Check that returned references don't point to local variables
        // This requires analyzing the return value to see if it's a reference
        // and validating that it doesn't borrow from local scope
    }
}

// ============================================================================
// Expression Visitors
// ============================================================================

void BorrowChecker::checkBinaryExpr(BinaryExpr* expr) {
    if (!expr) return;
    
    // Check if this is an assignment
    if (expr->op.type == frontend::TokenType::TOKEN_EQUAL) {
        checkAssignment(expr);
        return;
    }
    
    // Check both sides
    checkExpression(expr->left.get());
    checkExpression(expr->right.get());
}

void BorrowChecker::checkUnaryExpr(UnaryExpr* expr) {
    if (!expr) return;
    
    // Check operand first
    checkExpression(expr->operand.get());
    
    // Borrow and pin operations are handled at variable declaration
    // Here we just validate the operand
}

void BorrowChecker::checkIdentifier(IdentifierExpr* expr) {
    if (!expr) return;
    
    // Check for use-after-free on wild pointers
    checkWildUse(expr->name, expr);
}

void BorrowChecker::checkCallExpr(CallExpr* expr) {
    if (!expr) return;
    
    // Check all arguments
    for (const auto& arg : expr->arguments) {
        checkExpression(arg.get());
    }
    
    // TODO: Check function signature for ownership transfer
    // Some functions may take ownership (move) of arguments
    // This requires function signature analysis
}

// ============================================================================
// Error Reporting
// ============================================================================

void BorrowChecker::addError(const std::string& message, ASTNode* node) {
    if (node) {
        errors.emplace_back(node->line, node->column, message);
    } else {
        errors.emplace_back(0, 0, message);
    }
}

void BorrowChecker::addError(const std::string& message, int line, int column) {
    errors.emplace_back(line, column, message);
}

void BorrowChecker::addError(const std::string& message, ASTNode* node,
                            const std::string& related_msg, int related_line, int related_col) {
    if (node) {
        errors.emplace_back(node->line, node->column, message, 
                           related_line, related_col, related_msg);
    } else {
        errors.emplace_back(0, 0, message, related_line, related_col, related_msg);
    }
}

} // namespace sema
} // namespace aria

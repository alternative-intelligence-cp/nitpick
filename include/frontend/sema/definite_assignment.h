#ifndef ARIA_FRONTEND_SEMA_DEFINITE_ASSIGNMENT_H
#define ARIA_FRONTEND_SEMA_DEFINITE_ASSIGNMENT_H

#include "frontend/ast/stmt.h"
#include "frontend/ast/expr.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace npk {
namespace sema {

/**
 * Phase 2.3: Definite Assignment Analysis
 * 
 * Prevents reading uninitialized variables through static dataflow analysis.
 * Similar to Java/C#'s definite assignment rules.
 * 
 * Three-State Model:
 * - UNASSIGNED: Variable has been declared but not yet assigned
 * - ASSIGNED: Variable has definitely been assigned on all paths
 * - MAYBE_ASSIGNED: Variable has been assigned on some paths but not all
 * 
 * Rules:
 * - After VarDecl: State = UNASSIGNED
 * - After Assignment: State = ASSIGNED
 * - After if-else:
 *   - Both branches assign → ASSIGNED
 *   - One branch assigns → MAYBE_ASSIGNED
 *   - Neither assigns → UNASSIGNED
 * - Reading UNASSIGNED or MAYBE_ASSIGNED variables is an error
 */

enum class AssignmentState {
    UNASSIGNED,      // Declared but never assigned
    ASSIGNED,        // Definitely assigned on all paths
    MAYBE_ASSIGNED   // Assigned on some paths but not all
};

struct AssignmentError {
    std::string message;
    ASTNode* node;
    int line;
    int column;
};

/**
 * Context for tracking assignment state during analysis.
 * Uses snapshot/restore/merge pattern like BorrowChecker.
 */
class AssignmentContext {
public:
    // Variable name → Assignment state
    std::unordered_map<std::string, AssignmentState> var_states;
    
    // Current scope depth (for nested scopes)
    int current_depth = 0;
    
    // Scope stack tracking variables declared in each scope
    std::vector<std::vector<std::string>> scope_stack;
    
    // Enter a new scope
    void enterScope();
    
    // Exit a scope and remove variables
    void exitScope();
    
    // Create snapshot of current state (for branch analysis)
    AssignmentContext snapshot() const;
    
    // Restore state from snapshot
    void restore(const AssignmentContext& snap);
    
    // Merge states from two branches (conservative intersection)
    // Variable is ASSIGNED only if ASSIGNED in BOTH branches
    void merge(const AssignmentContext& then_state, const AssignmentContext& else_state);
    
    // Mark variable as assigned
    void assign(const std::string& name);
    
    // Get state of variable (or UNASSIGNED if not found)
    AssignmentState getState(const std::string& name) const;
};

/**
 * Definite Assignment Analyzer
 * 
 * Performs dataflow analysis to track variable assignment state
 * and detect reads of uninitialized variables.
 */
class DefiniteAssignmentAnalyzer {
public:
    // Analyze AST and return list of errors
    std::vector<AssignmentError> analyze(ASTNode* ast);
    
private:
    AssignmentContext ctx;
    std::vector<AssignmentError> errors;
    
    // Analysis methods
    void checkStatement(ASTNode* stmt);
    void checkExpression(ASTNode* expr);
    
    // Statement handlers
    void checkVarDecl(VarDeclStmt* decl);
    void checkAssignment(BinaryExpr* assign);
    void checkIfStmt(IfStmt* ifStmt);
    void checkWhileStmt(WhileStmt* whileStmt);
    void checkForStmt(ForStmt* forStmt);
    void checkBlock(BlockStmt* block);
    
    // Expression handlers
    void checkIdentifier(IdentifierExpr* ident);
    
    // Utility methods
    void addError(const std::string& message, ASTNode* node);
    bool isAssignmentTarget(ASTNode* node);
};

} // namespace sema
} // namespace npk

#endif // ARIA_FRONTEND_SEMA_DEFINITE_ASSIGNMENT_H

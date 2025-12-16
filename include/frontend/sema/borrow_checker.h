#ifndef ARIA_SEMA_BORROW_CHECKER_H
#define ARIA_SEMA_BORROW_CHECKER_H

#include "frontend/ast/ast_node.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <memory>

namespace aria {
namespace sema {

/**
 * BorrowChecker - Enforces memory safety through lifetime analysis
 * 
 * Phase 3.3: Borrow Checker Integration
 * 
 * Implements Aria's "Appendage Theory" for hybrid memory safety:
 * - Stack: Lexically scoped, RAII-style lifetime tracking
 * - GC Heap: Managed memory with pinning support (#)
 * - Wild Heap: Manual memory with leak detection
 * 
 * Key Responsibilities:
 * - Lifetime tracking via scope depth analysis
 * - Borrow rules: 1 mutable XOR N immutable references
 * - Memory safety: prevent use-after-free, double-free, dangling pointers
 * - Pinning contract: ensure GC objects remain stable while pinned
 * - Wild memory hygiene: detect leaks and use-after-free
 * 
 * Based on research_001: Borrow Checker Foundations
 */

// ============================================================================
// Data Structures for Lifetime Analysis
// ============================================================================

/**
 * Represents a single borrow/loan of a variable
 */
struct Loan {
    std::string borrower;    // Name of the reference variable
    bool is_mutable;         // true for $mut, false for $
    int creation_line;       // Line where borrow was created
    int creation_column;     // Column where borrow was created
    
    Loan(const std::string& b, bool mut, int line, int col)
        : borrower(b), is_mutable(mut), creation_line(line), creation_column(col) {}
};

/**
 * Tracks the state of a wild pointer for use-after-free detection
 */
enum class WildState {
    ALLOCATED,   // Memory allocated, can be used
    FREED,       // Memory freed, cannot be used
    MOVED        // Ownership transferred, cannot be used
};

/**
 * Lifetime tracking context - core data structure for borrow checking
 * 
 * Implements the Scope-Weighted Control Flow Graph (SW-CFG) analysis
 * described in research_001, Section 3.2
 */
struct LifetimeContext {
    // Maps variable name -> Declaration Scope Depth
    // Used for Appendage Theory: Depth(Host) <= Depth(Reference)
    std::unordered_map<std::string, int> var_depths;
    
    // Maps Reference -> Set of Origins (Hosts)
    // A reference may point to different hosts depending on CFG path (phi nodes)
    std::unordered_map<std::string, std::set<std::string>> loan_origins;
    
    // Maps Host -> List of Active Loans
    // Used to enforce Mutability XOR Aliasing rules (1 mutable OR N immutable)
    std::unordered_map<std::string, std::vector<Loan>> active_loans;
    
    // Tracks variables currently pinned by the # operator
    // Key: Host Variable, Value: Pinning Reference Name
    // Pinned variables cannot be moved, reassigned, or collected by GC
    std::unordered_map<std::string, std::string> active_pins;
    
    // Tracks wild allocations requiring cleanup (for leak detection)
    // Variables in this set must be freed before going out of scope
    std::unordered_set<std::string> pending_wild_frees;
    
    // Tracks the state of wild pointers (allocated, freed, moved)
    std::unordered_map<std::string, WildState> wild_states;
    
    // Current traversal depth (0 = global, 1 = function body, etc.)
    int current_depth;
    
    // Stack of variables declared at each scope level
    // Used for cleanup when exiting scopes
    std::vector<std::vector<std::string>> scope_stack;
    
    LifetimeContext() : current_depth(0) {
        scope_stack.push_back({}); // Global scope
    }
    
    /**
     * Enter a new scope (block, function, loop, etc.)
     */
    void enterScope();
    
    /**
     * Exit current scope, performing cleanup and validation
     */
    void exitScope();
    
    /**
     * Create a snapshot of current state (for branching analysis)
     */
    LifetimeContext snapshot() const;
    
    /**
     * Restore state from a snapshot
     */
    void restore(const LifetimeContext& snap);
    
    /**
     * Merge two states from different control flow branches
     * Conservative merging: variable is valid only if valid in ALL branches
     */
    void merge(const LifetimeContext& then_state, const LifetimeContext& else_state);
};

/**
 * Represents a borrow checking error
 */
struct BorrowError {
    int line;
    int column;
    std::string message;
    
    // Optional: Location of the conflicting borrow/definition
    int related_line;
    int related_column;
    std::string related_message;
    
    BorrowError(int l, int c, const std::string& msg)
        : line(l), column(c), message(msg), related_line(-1), related_column(-1) {}
    
    BorrowError(int l, int c, const std::string& msg, 
                int rl, int rc, const std::string& rmsg)
        : line(l), column(c), message(msg), 
          related_line(rl), related_column(rc), related_message(rmsg) {}
};

// ============================================================================
// Borrow Checker Class
// ============================================================================

class BorrowChecker {
private:
    LifetimeContext ctx;
    std::vector<BorrowError> errors;
    
    // ========================================================================
    // Lifetime Tracking (Phase 3.3.1)
    // ========================================================================
    
    /**
     * Register a variable declaration at current scope depth
     */
    void registerVariable(const std::string& name, ASTNode* node);
    
    /**
     * Check if a variable is in scope and return its depth
     * Returns -1 if variable is not found
     */
    int getVariableDepth(const std::string& name) const;
    
    /**
     * Validate Appendage Theory: Depth(Host) <= Depth(Reference)
     * 
     * Ensures that a reference does not outlive its host by checking
     * that the host is declared in an outer or equal scope
     */
    bool validateLifetime(const std::string& host, const std::string& reference, ASTNode* node);
    
    // ========================================================================
    // Borrow Rules Enforcement (Phase 3.3.2)
    // ========================================================================
    
    /**
     * Record a new borrow (loan) of a variable
     * 
     * @param host The variable being borrowed
     * @param reference The reference variable receiving the borrow
     * @param is_mutable true for $mut, false for $
     * @param node AST node for error reporting
     */
    void recordBorrow(const std::string& host, const std::string& reference, 
                     bool is_mutable, ASTNode* node);
    
    /**
     * Check borrow rules: 1 mutable XOR N immutable references
     * 
     * @param host The variable being borrowed
     * @param is_mutable true if requesting mutable borrow
     * @param node AST node for error reporting
     * @return true if borrow is allowed, false otherwise
     */
    bool checkBorrowRules(const std::string& host, bool is_mutable, ASTNode* node);
    
    /**
     * Release all borrows of a variable (when it goes out of scope)
     */
    void releaseBorrows(const std::string& var);
    
    // ========================================================================
    // Pinning Support (Phase 3.3.2)
    // ========================================================================
    
    /**
     * Record that a variable is pinned by the # operator
     * 
     * Pinned variables:
     * - Cannot be moved or reassigned
     * - Cannot be collected by GC (runtime cooperation)
     * - Remain stable in memory
     */
    void recordPin(const std::string& host, const std::string& pin_ref, ASTNode* node);
    
    /**
     * Check if a variable is currently pinned
     */
    bool isPinned(const std::string& var) const;
    
    /**
     * Release a pin (when pinning reference goes out of scope)
     */
    void releasePin(const std::string& var);
    
    // ========================================================================
    // Wild Memory Safety (Phase 3.3.3)
    // ========================================================================
    
    /**
     * Record allocation of wild memory
     */
    void recordWildAlloc(const std::string& var, ASTNode* node);
    
    /**
     * Record deallocation of wild memory
     */
    void recordWildFree(const std::string& var, ASTNode* node);
    
    /**
     * Check for use-after-free on wild memory
     */
    bool checkWildUse(const std::string& var, ASTNode* node);
    
    /**
     * Detect memory leaks (wild memory not freed before scope exit)
     */
    void checkForLeaks();
    
    // ========================================================================
    // AST Traversal
    // ========================================================================
    
    void checkStatement(ASTNode* stmt);
    void checkExpression(ASTNode* expr);
    
    // Statement visitors
    void checkVarDecl(VarDeclStmt* stmt);
    void checkAssignment(BinaryExpr* expr);
    void checkIfStmt(IfStmt* stmt);
    void checkWhileStmt(WhileStmt* stmt);
    void checkForStmt(ForStmt* stmt);
    void checkBlockStmt(BlockStmt* stmt);
    void checkReturnStmt(ReturnStmt* stmt);
    
    // Expression visitors
    void checkBinaryExpr(BinaryExpr* expr);
    void checkUnaryExpr(UnaryExpr* expr);
    void checkIdentifier(IdentifierExpr* expr);
    void checkCallExpr(CallExpr* expr);
    
    // ========================================================================
    // Error Reporting
    // ========================================================================
    
    void addError(const std::string& message, ASTNode* node);
    void addError(const std::string& message, int line, int column);
    void addError(const std::string& message, ASTNode* node,
                 const std::string& related_msg, int related_line, int related_col);
    
public:
    BorrowChecker() = default;
    
    /**
     * Analyze an AST for borrow checking violations
     * 
     * @param ast Root of the AST to analyze
     * @return List of borrow checking errors (empty if no errors)
     */
    std::vector<BorrowError> analyze(ASTNode* ast);
    
    /**
     * Check if borrow checking passed (no errors)
     */
    bool hasErrors() const { return !errors.empty(); }
    
    /**
     * Get list of all borrow checking errors
     */
    const std::vector<BorrowError>& getErrors() const { return errors; }
};

} // namespace sema
} // namespace aria

#endif // ARIA_SEMA_BORROW_CHECKER_H

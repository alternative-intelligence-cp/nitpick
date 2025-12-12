#ifndef ARIA_FRONTEND_SEMA_BORROW_CHECKER_H
#define ARIA_FRONTEND_SEMA_BORROW_CHECKER_H

#include "lifetime_context.h"
#include "../ast.h"
#include <memory>
#include <string>
#include <vector>

namespace aria {

// Forward declarations
namespace frontend {
    class AstNode;
    class VarDecl;
    class UnaryOp;
    class BinaryOp;
    class IfStmt;
    class WhileLoop;
    class ForLoop;
    class TillLoop;
    class WhenLoop;
    class Block;
    class FuncDecl;
    class ReturnStmt;
    class CallExpr;
    class VarExpr;
    class DeferStmt;
    class ExpressionStmt;
    class LambdaExpr;
}

/**
 * BorrowChecker - AST visitor that enforces Appendage Theory
 * 
 * This class implements the borrow checker for Aria, enforcing:
 * 1. Appendage Inequality: Depth(Host) ≤ Depth(Reference)
 * 2. Mutability XOR Aliasing (RW-Lock semantics)
 * 3. Pinning Contract (# operator prevents GC movement)
 * 4. Wild/GC Boundary (wild memory cannot hold unpinned GC refs)
 * 
 * Uses LifetimeContext for tracking variable lifetimes and borrows.
 */
class BorrowChecker : public frontend::AstVisitor {
public:
    BorrowChecker();
    ~BorrowChecker() override = default;

    /**
     * Main entry point - analyze an AST
     * @param root Root AST node to analyze
     * @return true if analysis succeeded, false if errors found
     */
    bool analyze(frontend::AstNode* root);

    /**
     * Get all collected errors
     * @return Vector of error messages
     */
    std::vector<std::string> get_errors() const { return errors_; }

    /**
     * Check if analysis has errors
     * @return true if errors were found
     */
    bool has_errors() const { return !errors_.empty(); }

    // ========================================================================
    // AstVisitor Interface - Expression Nodes
    // ========================================================================

    void visit(frontend::VarExpr* node) override;
    void visit(frontend::IntLiteral* node) override;
    void visit(frontend::FloatLiteral* node) override;
    void visit(frontend::BoolLiteral* node) override;
    void visit(frontend::NullLiteral* node) override;
    void visit(frontend::StringLiteral* node) override;
    void visit(frontend::TemplateString* node) override;
    void visit(frontend::TernaryExpr* node) override;
    void visit(frontend::BinaryOp* node) override;
    void visit(frontend::UnaryOp* node) override;
    void visit(frontend::CallExpr* node) override;
    void visit(frontend::ObjectLiteral* node) override;
    void visit(frontend::MemberAccess* node) override;
    void visit(frontend::VectorLiteral* node) override;
    void visit(frontend::ArrayLiteral* node) override;
    void visit(frontend::IndexExpr* node) override;
    void visit(frontend::UnwrapExpr* node) override;
    void visit(frontend::LambdaExpr* node) override;
    void visit(frontend::CastExpr* node) override;

    // ========================================================================
    // AstVisitor Interface - Statement Nodes
    // ========================================================================

    void visit(frontend::VarDecl* node) override;
    void visit(frontend::FuncDecl* node) override;
    void visit(frontend::StructDecl* node) override;
    void visit(frontend::ReturnStmt* node) override;
    void visit(frontend::ExpressionStmt* node) override;
    void visit(frontend::IfStmt* node) override;
    void visit(frontend::Block* node) override;

    // ========================================================================
    // AstVisitor Interface - Control Flow
    // ========================================================================

    void visit(frontend::PickStmt* node) override;
    void visit(frontend::FallStmt* node) override;
    void visit(frontend::TillLoop* node) override;
    void visit(frontend::WhenLoop* node) override;
    void visit(frontend::DeferStmt* node) override;
    void visit(frontend::ForLoop* node) override;
    void visit(frontend::WhileLoop* node) override;
    void visit(frontend::BreakStmt* node) override;
    void visit(frontend::ContinueStmt* node) override;

    // ========================================================================
    // AstVisitor Interface - Async/Module System
    // ========================================================================

    void visit(frontend::WhenExpr* node) override;
    void visit(frontend::AwaitExpr* node) override;
    void visit(frontend::SpawnExpr* node) override;
    void visit(frontend::AsyncBlock* node) override;
    void visit(frontend::UseStmt* node) override;
    void visit(frontend::ModDef* node) override;
    void visit(frontend::ExternBlock* node) override;
    void visit(frontend::TraitDecl* node) override;
    void visit(frontend::ImplDecl* node) override;

private:
    // ========================================================================
    // Core Tracking State
    // ========================================================================

    /// Lifetime context for tracking borrows, scopes, variables
    LifetimeContext context_;

    /// Collected error messages
    std::vector<std::string> errors_;

    /// Current function being analyzed (for return checking)
    std::string current_function_;

    /// Whether we're inside a loop (for break/continue validation)
    int loop_depth_;

    /// Whether we're inside a defer block (defer restrictions)
    bool in_defer_;

    // ========================================================================
    // Helper Methods - Error Reporting
    // ========================================================================

    /**
     * Report an error with optional node context
     * @param message Error message
     * @param node Optional AST node for line number
     */
    void report_error(const std::string& message, frontend::AstNode* node = nullptr);

    // ========================================================================
    // Helper Methods - Borrow Operations
    // ========================================================================

    /**
     * Handle $ (reference) operator - create borrow
     * @param operand Expression being borrowed
     * @param is_mutable Whether this is $mut (mutable borrow)
     * @param node AST node for error reporting
     * @return Name of temporary variable created for borrow
     */
    std::string handle_borrow_operator(frontend::AstNode* operand, bool is_mutable, 
                                       frontend::AstNode* node);

    /**
     * Handle # (pin) operator - pin GC object
     * @param operand Expression being pinned
     * @param node AST node for error reporting
     * @return Name of temporary variable created for wild pointer
     */
    std::string handle_pin_operator(frontend::AstNode* operand, frontend::AstNode* node);

    /**
     * Handle @ (address) operator - get pointer
     * @param operand Expression to take address of
     * @param node AST node for error reporting
     */
    void handle_address_operator(frontend::AstNode* operand, frontend::AstNode* node);

    /**
     * Handle * (dereference) operator
     * @param operand Expression being dereferenced
     * @param node AST node for error reporting
     */
    void handle_dereference_operator(frontend::AstNode* operand, frontend::AstNode* node);

    // ========================================================================
    // Helper Methods - Variable Name Extraction
    // ========================================================================

    /**
     * Extract variable name from expression (if it's a VarExpr)
     * @param node AST node
     * @return Variable name or empty string
     */
    std::string get_var_name(frontend::AstNode* node);

    // ========================================================================
    // Helper Methods - Control Flow Analysis
    // ========================================================================

    /**
     * Analyze branches (if/else, pick) - merge borrow states
     * @param then_branch Then block
     * @param else_branch Optional else block
     */
    void analyze_branches(frontend::Block* then_branch, frontend::Block* else_branch);

    /**
     * Analyze loop body - ensure borrows don't escape loop
     * @param body Loop body
     */
    void analyze_loop(frontend::Block* body);

    // ========================================================================
    // Helper Methods - Wild Memory Tracking
    // ========================================================================

    /**
     * Track wild allocation (aria.alloc, aria.alloc_buffer, etc.)
     * @param var_name Variable receiving allocation
     * @param node AST node for error reporting
     */
    void track_wild_allocation(const std::string& var_name, frontend::AstNode* node);

    /**
     * Track wild deallocation (aria.free)
     * @param var_name Variable being freed
     * @param node AST node for error reporting
     */
    void track_wild_free(const std::string& var_name, frontend::AstNode* node);

    /**
     * Verify all wild allocations have been freed
     * @param scope_name Scope name for error messages
     */
    void check_wild_leaks(const std::string& scope_name);

    // ========================================================================
    // Helper Methods - Variable Access Validation
    // ========================================================================

    /**
     * Validate variable can be read (not moved, not uninitialized)
     * @param var_name Variable name
     * @param node AST node for error reporting
     * @return true if valid for read
     */
    bool validate_read_access(const std::string& var_name, frontend::AstNode* node);

    /**
     * Validate variable can be written (not borrowed, not moved)
     * @param var_name Variable name
     * @param node AST node for error reporting
     * @return true if valid for write
     */
    bool validate_write_access(const std::string& var_name, frontend::AstNode* node);

    /**
     * Validate variable can be moved (not borrowed, not already moved)
     * @param var_name Variable name
     * @param node AST node for error reporting
     * @return true if valid for move
     */
    bool validate_move_access(const std::string& var_name, frontend::AstNode* node);

    /// Counter for generating temporary variable names
    int temp_var_counter_;
};

// ============================================================================
// Convenience Functions (sema namespace)
// ============================================================================

namespace sema {
    /**
     * Check borrow rules for an AST (wrapper around BorrowChecker)
     * @param root Root AST node
     * @return true if no borrow errors, false if violations found
     */
    bool check_borrow_rules(frontend::AstNode* root);
}

} // namespace aria

#endif // ARIA_FRONTEND_SEMA_BORROW_CHECKER_H

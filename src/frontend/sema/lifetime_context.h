#ifndef ARIA_LIFETIME_CONTEXT_H
#define ARIA_LIFETIME_CONTEXT_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

namespace aria {

// Forward declarations
class ASTNode;
class VarDecl;
class UnaryOp;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * Memory region classification for variables
 * Based on research_001: Three memory regions (stack, GC heap, wild heap)
 */
enum class MemoryRegion {
    STACK,      // Automatic storage (lexical scope)
    GC_HEAP,    // Garbage collected heap (movable)
    WILD_HEAP,  // Manual heap (fixed address)
    UNKNOWN     // Not yet determined
};

/**
 * Borrow kind for reference tracking
 * Implements Rule 1: Mutability XOR Aliasing (RW-Lock)
 */
enum class BorrowKind {
    IMMUTABLE,  // Shared read-only reference ($x)
    MUTABLE     // Exclusive mutable reference ($mut x)
};

/**
 * Variable state for flow-sensitive analysis
 * Tracks variable status through control flow
 */
enum class VarState {
    UNINITIALIZED,  // Declared but not initialized
    INITIALIZED,    // Has valid value
    MOVED,          // Value moved out (invalid for wild types)
    BORROWED_IMM,   // Currently has immutable borrows
    BORROWED_MUT    // Currently has mutable borrow
};

// ============================================================================
// Core Data Structures
// ============================================================================

/**
 * Represents a single borrow (loan) of a variable
 * Tracks the Appendage (reference) to a Host (referent)
 */
struct Loan {
    std::string var_name;           // Name of borrowed variable (Host)
    BorrowKind kind;                // Immutable or mutable
    int scope_depth;                // Depth where borrow was created
    int ref_scope_depth;            // Depth of the reference variable (Appendage)
    ASTNode* borrow_site;           // AST node where borrow occurred
    std::string ref_var_name;       // Name of reference variable (Appendage)
    
    Loan(const std::string& var, BorrowKind k, int depth, int ref_depth,
         ASTNode* site, const std::string& ref_name)
        : var_name(var), kind(k), scope_depth(depth), ref_scope_depth(ref_depth),
          borrow_site(site), ref_var_name(ref_name) {}
};

/**
 * Information about a variable tracked by the borrow checker
 * Maintains state for Appendage Theory validation
 */
struct VarInfo {
    std::string name;               // Variable name
    int scope_depth;                // Depth where declared (Host depth)
    MemoryRegion region;            // Stack, GC, or wild heap
    bool is_pinned;                 // Whether pinned via # operator
    VarState state;                 // Current state (initialized, moved, etc.)
    VarDecl* decl_node;             // AST node of declaration
    
    // Active borrows (loans) of this variable
    std::vector<Loan> active_loans;
    
    VarInfo(const std::string& n, int depth, MemoryRegion reg, VarDecl* decl)
        : name(n), scope_depth(depth), region(reg), is_pinned(false),
          state(VarState::UNINITIALIZED), decl_node(decl) {}
    
    // Check if variable has any active mutable borrows
    bool has_mutable_borrow() const {
        for (const auto& loan : active_loans) {
            if (loan.kind == BorrowKind::MUTABLE) return true;
        }
        return false;
    }
    
    // Check if variable has any active immutable borrows
    bool has_immutable_borrows() const {
        for (const auto& loan : active_loans) {
            if (loan.kind == BorrowKind::IMMUTABLE) return true;
        }
        return false;
    }
    
    // Count of active borrows
    size_t active_borrow_count() const {
        return active_loans.size();
    }
};

/**
 * Represents a lexical scope in the program
 * Tracks variables and borrows within a scope block
 */
struct Scope {
    int depth;                                  // Scope depth (0=global, 1=function, 2+=nested)
    std::map<std::string, VarInfo> variables;   // Variables declared in this scope
    std::set<std::string> moved_vars;           // Variables moved out in this scope
    
    explicit Scope(int d) : depth(d) {}
};

// ============================================================================
// LifetimeContext Class
// ============================================================================

/**
 * Central tracking system for borrow checker
 * 
 * Implements Appendage Theory by maintaining:
 * 1. Scope depth hierarchy (for depth inequality checking)
 * 2. Variable information (Host metadata)
 * 3. Active loans (Appendage tracking)
 * 4. Flow-sensitive state (through control flow)
 * 
 * Based on research_001 section "Implementation Strategy"
 */
class LifetimeContext {
private:
    // Stack of lexical scopes (innermost at top)
    std::vector<Scope> scope_stack_;
    
    // Current scope depth (top of stack)
    int current_depth_;
    
    // Global variable tracking (across all scopes)
    std::map<std::string, VarInfo*> global_var_map_;
    
    // Counter for generating unique IDs
    int next_loan_id_;
    
    // Wild memory leak tracking
    std::set<std::string> pending_wild_frees_; // Variables awaiting free/defer
    std::set<std::string> freed_wild_vars_;     // Variables already freed (use-after-free detection)

public:
    LifetimeContext();
    ~LifetimeContext() = default;
    
    // ========================================================================
    // Scope Management
    // ========================================================================
    
    /**
     * Enter a new lexical scope (function, block, if, while, etc.)
     * Pushes new scope onto stack and increments depth
     */
    void enter_scope();
    
    /**
     * Exit current scope
     * Validates all borrows end properly, pops scope from stack
     */
    void exit_scope();
    
    /**
     * Get current scope depth
     * Returns 0 for global, 1 for function, 2+ for nested blocks
     */
    int current_depth() const { return current_depth_; }
    
    /**
     * Get reference to current scope
     */
    Scope& current_scope();
    const Scope& current_scope() const;
    
    // ========================================================================
    // Variable Management
    // ========================================================================
    
    /**
     * Declare a new variable in current scope
     * Adds to scope's variable map and global tracking
     * 
     * @param name Variable name
     * @param region Memory region (stack, gc, wild)
     * @param decl AST node of declaration
     * @return Pointer to created VarInfo
     */
    VarInfo* declare_variable(const std::string& name, MemoryRegion region, VarDecl* decl);
    
    /**
     * Mark variable as initialized
     * Transitions state from UNINITIALIZED to INITIALIZED
     */
    void initialize_variable(const std::string& name);
    
    /**
     * Mark variable as moved
     * For wild types, moves invalidate the variable
     * For GC types, moves are copies (no invalidation)
     */
    void move_variable(const std::string& name);
    
    /**
     * Lookup variable by name
     * Searches current scope and all parent scopes
     * 
     * @return VarInfo* if found, nullptr otherwise
     */
    VarInfo* lookup_variable(const std::string& name);
    const VarInfo* lookup_variable(const std::string& name) const;
    
    /**
     * Check if variable exists in any scope
     */
    bool variable_exists(const std::string& name) const;
    
    // ========================================================================
    // Borrow Tracking (Core Appendage Theory)
    // ========================================================================
    
    /**
     * Create a borrow (loan) of a variable
     * 
     * Implements Appendage Theory checks:
     * 1. Appendage Inequality: Depth(Host) ≤ Depth(Reference)
     * 2. Mutability XOR Aliasing: No conflicting borrows
     * 
     * @param var_name Name of variable being borrowed (Host)
     * @param kind Immutable or mutable borrow
     * @param ref_var_name Name of reference variable (Appendage)
     * @param ref_scope_depth Scope depth where reference lives
     * @param borrow_site AST node where borrow occurs
     * @return true if borrow is valid, false if violates rules
     */
    bool create_borrow(const std::string& var_name, BorrowKind kind,
                      const std::string& ref_var_name, int ref_scope_depth,
                      ASTNode* borrow_site);
    
    /**
     * End a borrow when reference goes out of scope
     * Removes loan from Host's active_loans
     * 
     * @param var_name Name of borrowed variable (Host)
     * @param ref_var_name Name of reference variable (Appendage)
     */
    void end_borrow(const std::string& var_name, const std::string& ref_var_name);
    
    /**
     * End all borrows for variables declared in a specific scope
     * Called when exiting a scope
     */
    void end_borrows_for_scope(int scope_depth);
    
    /**
     * Check if variable can be borrowed mutably
     * Validates Rule 1: No existing borrows (immutable or mutable)
     */
    bool can_borrow_mutably(const std::string& var_name) const;
    
    /**
     * Check if variable can be borrowed immutably
     * Validates Rule 1: No existing mutable borrows
     */
    bool can_borrow_immutably(const std::string& var_name) const;
    
    /**
     * Get all active loans for a variable
     */
    const std::vector<Loan>& get_active_loans(const std::string& var_name) const;
    
    // ========================================================================
    // Pinning Operations (# operator)
    // ========================================================================
    
    /**
     * Pin a GC object to allow wild pointer access
     * Implements Rule 3: Pinning Contract
     * 
     * @param var_name Name of GC variable to pin
     * @return true if pin successful, false if variable not GC
     */
    bool pin_variable(const std::string& var_name);
    
    /**
     * Unpin a variable when wild pointer lifetime ends
     */
    void unpin_variable(const std::string& var_name);
    
    /**
     * Check if variable is currently pinned
     */
    bool is_pinned(const std::string& var_name) const;
    
    // ========================================================================
    // Validation Helpers
    // ========================================================================
    
    /**
     * Validate Appendage Inequality: Depth(Host) ≤ Depth(Reference)
     * 
     * @param host_depth Scope depth of borrowed variable
     * @param ref_depth Scope depth of reference variable
     * @return true if inequality holds (valid borrow)
     */
    bool validate_depth_inequality(int host_depth, int ref_depth) const;
    
    /**
     * Check if variable is in valid state for use
     * (not moved, not uninitialized)
     */
    bool is_valid_for_use(const std::string& var_name) const;
    
    /**
     * Check if variable is in valid state for borrowing
     */
    bool is_valid_for_borrow(const std::string& var_name) const;
    
    // ========================================================================
    // Debugging and Diagnostics
    // ========================================================================
    
    /**
     * Dump current context state for debugging
     */
    void dump_state() const;
    
    /**
     * Get human-readable description of borrow error
     */
    std::string get_borrow_error_message(const std::string& var_name,
                                        BorrowKind attempted_kind) const;
    
    // ========================================================================
    // Wild Memory Leak Detection (Rule 2)
    // ========================================================================
    
    /**
     * Track wild allocation (aria.alloc, new, etc.)
     * Adds to pending_wild_frees_ set
     */
    void track_wild_allocation(const std::string& var_name);
    
    /**
     * Track wild deallocation (aria.free, defer aria.free)
     * Removes from pending_wild_frees_, adds to freed_wild_vars_
     */
    void track_wild_free(const std::string& var_name);
    
    /**
     * Check if variable has been freed (use-after-free detection)
     */
    bool is_freed(const std::string& var_name) const;
    
    /**
     * Get all wild variables pending free in current scope
     * Used to check for memory leaks on scope exit
     */
    std::vector<std::string> get_pending_wild_frees() const;
    
    /**
     * Clear pending wild frees for current scope
     * Called after validation or defer registration
     */
    void clear_pending_wild_frees_for_scope();
};

} // namespace aria

#endif // ARIA_LIFETIME_CONTEXT_H

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
#include <map>
#include <memory>

// Forward declaration for optional Z3 integration (v0.6.1)
namespace npk { class Z3Verifier; }

namespace npk {
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

// ============================================================================
// Access Path Tracking (Gemini Report: Split Borrows)
// ============================================================================

/**
 * AccessPath - Tracks the exact path being borrowed for split borrow analysis
 *
 * Enables disjoint field borrowing:
 *   - torus.sector[0] and torus.sector[1] are disjoint
 *   - torus.emitters and torus.grid are disjoint
 *   - torus.sector and torus.sector[0] overlap (parent/child)
 *
 * Format: base_var.field1.field2[index]...
 */
struct AccessPath {
    std::string base_var;              // Root variable name
    std::vector<std::string> fields;   // Field/index access chain
    
    // v0.6.1: Store index expression AST nodes for Z3 disjointness proofs
    // Parallel to fields vector — non-null only for [*] placeholder entries
    std::vector<ASTNode*> index_exprs;

    AccessPath() = default;
    explicit AccessPath(const std::string& base) : base_var(base) {}
    AccessPath(const std::string& base, std::vector<std::string> f)
        : base_var(base), fields(std::move(f)) {}

    /**
     * Returns true if this path is a prefix of or equal to 'other'
     * Example: a.x is prefix of a.x.y
     */
    bool isPrefixOf(const AccessPath& other) const {
        if (base_var != other.base_var) return false;
        if (fields.size() > other.fields.size()) return false;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (fields[i] != other.fields[i]) return false;
        }
        return true;
    }

    /**
     * Returns true if paths are disjoint (e.g. x.a vs x.b)
     * Two paths are disjoint if they share a common prefix but diverge
     * v0.6.1: Uses index_exprs for Z3-backed disjointness when both have [*]
     */
    bool isDisjointFrom(const AccessPath& other) const {
        // v0.25.3 (BORROW-006): conservative cross-pointer aliasing.
        // If either path goes through a pointer dereference ("->field"
        // segment), a different base_var does NOT guarantee disjointness —
        // two distinct pointer variables may alias the same host. Without
        // alias analysis (planned for BORROW-007/008), treat such pairs as
        // overlapping. Two paths starting at the SAME pointer variable still
        // follow the normal prefix rules, so ptr->x and ptr->y stay disjoint.
        auto pathHasDeref = [](const AccessPath& p) {
            for (const auto& seg : p.fields) {
                if (seg.size() >= 2 && seg[0] == '-' && seg[1] == '>') return true;
            }
            return false;
        };
        if (base_var != other.base_var) {
            if (pathHasDeref(*this) || pathHasDeref(other)) {
                return false;  // Conservative: may alias
            }
            return true;  // Different non-pointer roots
        }

        // Find where paths diverge
        size_t min_len = std::min(fields.size(), other.fields.size());
        for (size_t i = 0; i < min_len; ++i) {
            if (fields[i] != other.fields[i]) {
                // A dynamic index placeholder may alias any concrete index, so
                // [*] vs [0] is overlapping unless a later proof says otherwise.
                if (fields[i] == "[*]" || other.fields[i] == "[*]") {
                    return false;
                }
                // They diverge here - check if both have more fields
                // a.x vs a.y -> disjoint (diverge at same level)
                return true;
            }
            // v0.6.1: Both are [*] but may have different index expressions
            // Two [*] entries with literal sub-expressions can be compared
            if (fields[i] == "[*]" && other.fields[i] == "[*]") {
                // Check if we have index expressions that are literal and different
                if (i < index_exprs.size() && i < other.index_exprs.size() &&
                    index_exprs[i] && other.index_exprs[i]) {
                    auto* e1 = index_exprs[i];
                    auto* e2 = other.index_exprs[i];
                    // Quick literal comparison
                    if (e1->type == ASTNode::NodeType::LITERAL &&
                        e2->type == ASTNode::NodeType::LITERAL) {
                        auto* l1 = static_cast<LiteralExpr*>(e1);
                        auto* l2 = static_cast<LiteralExpr*>(e2);
                        if (std::holds_alternative<int64_t>(l1->value) &&
                            std::holds_alternative<int64_t>(l2->value)) {
                            if (std::get<int64_t>(l1->value) != std::get<int64_t>(l2->value)) {
                                return true;  // Different constant indices
                            }
                        }
                    }
                    // If both are different identifier expressions, check names
                    if (e1->type == ASTNode::NodeType::IDENTIFIER &&
                        e2->type == ASTNode::NodeType::IDENTIFIER) {
                        auto* id1 = static_cast<IdentifierExpr*>(e1);
                        auto* id2 = static_cast<IdentifierExpr*>(e2);
                        // Same variable name means same index — not disjoint here
                        // Different names: can't prove without Z3, continue
                    }
                }
            }
        }

        // One is a prefix of the other - they overlap
        return false;
    }

    /**
     * Check if this path conflicts with another for borrowing purposes
     * Conflict if: same path, one is prefix of other, or overlapping indices
     */
    bool conflictsWith(const AccessPath& other) const {
        return !isDisjointFrom(other);
    }

    std::string toString() const {
        std::string result = base_var;
        for (const auto& field : fields) {
            if (!field.empty() && field[0] == '[') {
                result += field;  // Array index
            } else {
                result += "." + field;  // Field access
            }
        }
        return result;
    }

    bool operator==(const AccessPath& other) const {
        return base_var == other.base_var && fields == other.fields;
    }

    bool operator<(const AccessPath& other) const {
        if (base_var != other.base_var) return base_var < other.base_var;
        return fields < other.fields;
    }
};

// Hash function for AccessPath (for use in unordered containers)
struct AccessPathHash {
    size_t operator()(const AccessPath& path) const {
        size_t h = std::hash<std::string>{}(path.base_var);
        for (const auto& f : path.fields) {
            h ^= std::hash<std::string>{}(f) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

/**
 * PinAliasInfo - provenance for pointer variables derived from a pin path.
 *
 * Example:
 *   stack Box->:pin = #box;
 *   stack Leaf->:alias = pin->leaf;
 *
 * The alias must preserve the read-only pin contract, even though it is no
 * longer syntactically rooted at the original pin reference.
 */
struct PinAliasInfo {
    std::string pin_ref;
    std::string host;
    std::string source_path;

    bool operator==(const PinAliasInfo& other) const {
        return pin_ref == other.pin_ref &&
               host == other.host &&
               source_path == other.source_path;
    }
};

// ============================================================================
// Two-Phase Borrowing (Gemini Report: Method Call Patterns)
// ============================================================================

/**
 * LoanPhase - Tracks the phase of a two-phase borrow
 *
 * Two-phase borrowing enables patterns like vec.push(vec.len()):
 *   1. RESERVED: Mutable borrow is "reserved" but acts as shared
 *   2. ACTIVE: Borrow is activated to full mutable after args evaluated
 *
 * This allows reading the receiver while evaluating arguments.
 */
enum class LoanPhase {
    RESERVED,   // Reserved mutable borrow (acts as shared)
    ACTIVE      // Fully active borrow (exclusive for mutable)
};

/**
 * Represents a single borrow/loan of a variable or access path
 */
struct Loan {
    std::string borrower;    // Name of the reference variable
    bool is_mutable;         // true for $$m, false for $$i
    int creation_line;       // Line where borrow was created
    int creation_column;     // Column where borrow was created
    AccessPath path;         // The exact path being borrowed (for split borrows)
    LoanPhase phase;         // Two-phase borrowing state (RESERVED or ACTIVE)

    Loan(const std::string& b, bool mut, int line, int col)
        : borrower(b), is_mutable(mut), creation_line(line), creation_column(col),
          path(b), phase(LoanPhase::ACTIVE) {}

    Loan(const std::string& b, bool mut, int line, int col, const AccessPath& p)
        : borrower(b), is_mutable(mut), creation_line(line), creation_column(col),
          path(p), phase(LoanPhase::ACTIVE) {}

    Loan(const std::string& b, bool mut, int line, int col, const AccessPath& p, LoanPhase ph)
        : borrower(b), is_mutable(mut), creation_line(line), creation_column(col),
          path(p), phase(ph) {}

    /**
     * Check if this loan is currently active (not just reserved)
     */
    bool isActive() const { return phase == LoanPhase::ACTIVE; }

    /**
     * Check if this loan blocks mutable access
     * Reserved loans allow shared reads, active mutable loans block all
     */
    bool blocksMutableAccess() const {
        return phase == LoanPhase::ACTIVE && is_mutable;
    }

    /**
     * Check if this loan blocks any access to the path
     */
    bool blocksAccess(bool requesting_mutable) const {
        if (phase == LoanPhase::RESERVED) {
            // Reserved loans only block other mutable borrows
            return requesting_mutable;
        }
        // Active loans: mutable blocks all, immutable blocks mutable
        return is_mutable || requesting_mutable;
    }
};

/**
 * Tracks the state of a wild pointer for use-after-free detection
 *
 * State Machine (ARIA-014):
 * UNINITIALIZED -> ALLOCATED (via alloc)
 * ALLOCATED -> FREED (via free)
 * ALLOCATED -> MOVED (via move/assign)
 * FREED -> [error on access or double-free]
 * MOVED -> [error on access]
 */
enum class WildState {
    UNINITIALIZED,  // Declared but no value assigned
    ALLOCATED,      // Memory allocated, can be used
    FREED,          // Memory freed, cannot be used
    MOVED,          // Ownership transferred, cannot be used
    UNKNOWN,        // Indeterminate state (after complex branching)
    MAY_FREED       // May have been freed (loop-carried uncertainty)
};

// ============================================================================
// TBB Value State Tracking (Gemini Report: TBB ERR → Release Borrows)
// ============================================================================

/**
 * TBBValueState - Tracks whether a TBB variable is in Valid or Error state
 *
 * When a TBB variable enters ERR state (via check like `if val == ERR`),
 * it semantically holds no useful data. Borrows from that variable can
 * be considered released on the error path, enabling error recovery patterns:
 *
 *   let result = torus.compute_wave();
 *   if result == ERR {
 *       torus.reset();  // Allowed: ERR releases borrow
 *   }
 */
enum class TBBValueState {
    VALID,    // Normal value, may hold data
    ERROR,    // Known to be ERR sentinel
    UNKNOWN   // Could be either (merge of valid/error paths)
};

// ============================================================================
// Liveness Analysis (Gemini Report: Loop-Carried Dependencies)
// ============================================================================

/**
 * LivenessAnalysis - Tracks which variables are "live" at program points
 *
 * A variable is live at point P if:
 *   1. It may be used after P (before being reassigned)
 *   2. It holds a value that could affect program behavior
 *
 * Used by mergeLoopBackEdge to filter out dead variables,
 * preventing false conflicts from zombie loans.
 */
struct LivenessAnalysis {
    // Variables that are live at the loop header
    std::unordered_set<std::string> live_at_header;

    // Variables that are used after the loop (for escape analysis)
    std::unordered_set<std::string> live_after_loop;

    /**
     * Check if a variable is live at the loop header
     */
    bool isLiveAtHeader(const std::string& var) const {
        return live_at_header.count(var) > 0;
    }

    /**
     * Mark a variable as live at header
     */
    void markLiveAtHeader(const std::string& var) {
        live_at_header.insert(var);
    }

    /**
     * Clear all liveness info
     */
    void clear() {
        live_at_header.clear();
        live_after_loop.clear();
    }
};

/**
 * Constants for fixpoint iteration algorithm
 */
constexpr int WIDENING_THRESHOLD = 3;  // Max iterations before widening
constexpr int MAX_FIXPOINT_ITERATIONS = 10;  // Absolute maximum iterations

/**
 * Memory region a binding lives in.
 *
 * v0.27.1 (MEM-DEC-007): introduced as a per-binding tag on
 * LifetimeContext so downstream slices (ARIA-029 GC_REF_FROM_WILD,
 * ARIA-031 STACK_REF_INTO_GC_FIELD, Handle<T> arena tying) can
 * answer "what region does this name live in?" without re-deriving
 * it from the type string each time.
 *
 * Field-level region propagation through struct types is deferred
 * to v0.27.3 where it is actually consumed.
 */
enum class Region {
    Unknown,  // Not yet classified (initial state, externs, params w/o annotation)
    Stack,    // Default region: scoped to the declaring frame
    Gc,       // Generational GC heap (auto-pinned + shadow-stack rooted)
    Wild,     // Manual heap (npk_alloc / npk_free); FFI buffers
    Wildx,    // Manual heap with PROT_EXEC (W^X discipline); JIT
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

    // v0.27.1: Maps variable name -> Region (stack/gc/wild/wildx).
    // Populated at variable declaration in checkVarDecl from the
    // VarDeclStmt::isWild/isWildx/isGC/isStack flags. Defaults to
    // Region::Stack for non-annotated locals (matches MEM-DEC-001:
    // default region is stack, not GC).
    std::unordered_map<std::string, Region> var_regions;
    
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

    // Pointer variables derived from a pin-rooted path. These aliases retain
    // the pin's read-only contract and may not be used for mutation or by-value
    // escape into an unconstrained callee.
    std::unordered_map<std::string, PinAliasInfo> pin_derived_aliases;

    // Variables whose declared type is pointer-like (contains ->). This lets
    // reassignment tracking distinguish pointer aliases from scalar reads of a
    // pin-rooted path.
    std::unordered_set<std::string> pointer_vars;
    
    // Tracks wild allocations requiring cleanup (for leak detection)
    // Variables in this set must be freed before going out of scope
    std::unordered_set<std::string> pending_wild_frees;

    // Tracks defer obligations - wild vars that have a matching defer statement
    // Key: wild variable name, Value: line of defer statement
    // ARIA-014: Every wild allocation must be paired with a defer free
    std::unordered_map<std::string, int> defer_obligations;
    
    // Tracks the state of wild pointers (allocated, freed, moved)
    std::unordered_map<std::string, WildState> wild_states;

    // v0.6.3: Per-allocation size tracking for bounds checking
    // Maps wild variable name -> allocation size (string expression from source)
    // Updated on alloc() and realloc(), cleared on free()
    std::unordered_map<std::string, std::string> wild_alloc_sizes;

    // Tracks all variables that have been moved (not just wild)
    // Used to detect use-after-move for all types
    std::unordered_set<std::string> moved_variables;

    // ========================================================================
    // TBB Value State Tracking (Gemini Report)
    // ========================================================================
    // Tracks whether TBB variables are in Valid or Error state
    // When a variable is known to be ERR, borrows from it can be released
    std::unordered_map<std::string, TBBValueState> tbb_value_states;

    // ========================================================================
    // Access Path Based Loans (Gemini Report: Split Borrows)
    // ========================================================================
    // Maps AccessPath -> List of Active Loans
    // Enables split borrowing: torus.sector[0] and torus.sector[1] can be
    // borrowed simultaneously because their paths are disjoint
    std::unordered_map<AccessPath, std::vector<Loan>, AccessPathHash> path_loans;

    // Current traversal depth (0 = global, 1 = function body, etc.)
    int current_depth;
    
    // Stack of variables declared at each scope level
    // Used for cleanup when exiting scopes
    std::vector<std::vector<std::string>> scope_stack;
    
    LifetimeContext() : current_depth(0) {
        scope_stack.push_back({}); // Global scope
    }

    /**
     * v0.27.1: Look up the region a binding lives in.
     * Returns Region::Unknown for names that were never declared
     * inside the current function (e.g., free identifiers, externs).
     */
    Region region_of(const std::string& name) const {
        auto it = var_regions.find(name);
        return (it != var_regions.end()) ? it->second : Region::Unknown;
    }

    /**
     * v0.27.1: Record the region for a binding. Called from
     * checkVarDecl after parsing the qualifier flags. Idempotent
     * for repeated declarations of the same name in nested scopes
     * (last write wins, matching how var_depths is treated).
     */
    void set_region(const std::string& name, Region r) {
        var_regions[name] = r;
    }

    /**
     * Get current scope depth
     */
    int getScopeDepth() const {
        return current_depth;
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

    /**
     * Merge for loop back-edge analysis (fixpoint iteration)
     * Returns true if the state changed (need another iteration)
     * Uses set union for active_loans (May-Analysis)
     *
     * Original version (for backwards compatibility)
     */
    bool mergeLoopBackEdge(const LifetimeContext& back_edge_state);

    /**
     * Liveness-aware merge for loop back-edge (Gemini Report improvement)
     *
     * Only propagates loans for variables that are LIVE at the loop header.
     * This prevents false conflicts from "zombie" loans that would be dead
     * by the start of the next iteration.
     *
     * @param back_edge_state State at end of loop body
     * @param liveness Liveness analysis results
     * @return true if state changed (need another iteration)
     */
    bool mergeLoopBackEdgeLivenessAware(const LifetimeContext& back_edge_state,
                                         const LivenessAnalysis& liveness);

    /**
     * Apply widening operator to force convergence
     * Called after WIDENING_THRESHOLD iterations
     * Promotes uncertain states to conservative top values
     */
    void widen();

    /**
     * Check if two states are equivalent (for fixpoint detection)
     */
    bool equivalent(const LifetimeContext& other) const;

    // ========================================================================
    // TBB Value State Methods (Gemini Report)
    // ========================================================================

    /**
     * Mark a TBB variable as being in ERR state
     * Releases any borrows that originated from this variable
     */
    void markTBBError(const std::string& var);

    /**
     * Mark a TBB variable as having a valid (non-ERR) value
     */
    void markTBBValid(const std::string& var);

    /**
     * Check if a TBB variable is known to be in ERR state
     */
    bool isTBBError(const std::string& var) const;

    /**
     * Release all loans that originated from a variable in ERR state
     * Called when we detect a TBB variable is ERR
     */
    void releaseLoansForTBBError(const std::string& var);

    // ========================================================================
    // Split Borrow Methods (Gemini Report)
    // ========================================================================

    /**
     * Check if a borrow of the given path would conflict with existing loans
     * Uses access path analysis to allow disjoint borrows
     *
     * @param path The access path being borrowed
     * @param is_mutable Whether this is a mutable borrow
     * @return nullptr if no conflict, or pointer to conflicting loan
     */
    const Loan* checkPathConflict(const AccessPath& path, bool is_mutable) const;

    /**
     * Record a loan with access path tracking
     */
    void recordPathLoan(const AccessPath& path, const Loan& loan);

    /**
     * Release loans for a specific access path
     */
    void releasePathLoans(const AccessPath& path);
};

/**
 * Severity level for borrow checker diagnostics
 */
enum class BorrowSeverity {
    ERROR,
    WARNING,
    HINT
};

/**
 * Represents a borrow checking error
 */
struct BorrowError {
    int line;
    int column;
    std::string message;
    BorrowSeverity severity = BorrowSeverity::ERROR;
    
    // Diagnostic code (e.g., "ARIA-014")
    std::string code;
    
    // Optional: Location of the conflicting borrow/definition
    int related_line;
    int related_column;
    std::string related_message;
    
    // Optional: Suggested fix
    std::string suggestion;
    
    BorrowError(int l, int c, const std::string& msg)
        : line(l), column(c), message(msg), related_line(-1), related_column(-1) {}
    
    BorrowError(int l, int c, const std::string& msg, 
                int rl, int rc, const std::string& rmsg)
        : line(l), column(c), message(msg), 
          related_line(rl), related_column(rc), related_message(rmsg) {}
    
    BorrowError(int l, int c, const std::string& msg, const std::string& suggest)
        : line(l), column(c), message(msg), related_line(-1), related_column(-1),
          suggestion(suggest) {}
};

// ============================================================================
// Function Borrow Summary (v0.6.0: Inter-Procedural Analysis)
// ============================================================================

/**
 * ParamOwnership - How a function parameter interacts with the borrow system
 */
enum class ParamOwnership {
    COPY,       // Parameter is passed by value (default for non-borrow types)
    BORROW_IMM, // Parameter is an immutable borrow ($$i)
    BORROW_MUT, // Parameter is a mutable borrow ($$m)
    MOVE        // Parameter takes ownership (wild pointer passed by value)
};

/**
 * FunctionBorrowSummary - Cached summary of a function's borrow semantics
 *
 * Built during initial pass over all function declarations.
 * Used at call sites to check ownership transfer and borrow validity.
 */
struct FunctionBorrowSummary {
    std::string func_name;
    
    // Per-parameter ownership info (indexed by parameter position)
    std::vector<ParamOwnership> param_ownership;
    std::vector<std::string> param_names;
    
    // Return type borrow info
    bool returns_borrow_imm = false;  // Function returns $$i reference
    bool returns_borrow_mut = false;  // Function returns $$m reference
    bool returns_wild = false;        // Function returns wild pointer
    
    // v0.6.2: Trait method receiver info
    bool is_trait_method = false;     // True if this is an impl method
    std::string impl_type;            // Type name for impl methods (e.g. "Circle")
    std::string trait_name;           // Trait name (e.g. "Drawable")
    int self_param_index = -1;        // Index of self parameter (-1 if none)
    ParamOwnership self_ownership = ParamOwnership::COPY;  // How self is passed

    // v0.25.4 (BORROW-008): Index of the parameter that the returned borrow
    // derives from. -1 = not a borrow-returning function, or could not infer
    // (e.g. multiple borrow params). Heuristic: when returns_borrow_imm/mut
    // is set and exactly one parameter is itself a borrow, the return is
    // assumed to derive from that parameter. Multi-borrow cases require an
    // explicit annotation (deferred to a later cycle).
    int return_borrow_source_param = -1;
    
    // v0.28.3 (ARIA-032 cross-function Phase 1): indices of params that
    // this function destroys via a direct `HandleArena_destroy(param)`
    // call somewhere in its body. Discovered statically in `buildSummary`
    // by scanning the function body for `HandleArena_destroy(<ident>)`
    // calls whose argument identifier matches a parameter name. At call
    // sites, the matching argument's bound arena is marked destroyed.
    std::set<size_t> destroys_param_indices;

    // v0.28.5 (ARIA-032 FFI passthrough rule): true when this function is
    // declared `extern` (no body to analyse). Call sites use this to emit
    // the FFI passthrough warning when a tracked handle is passed to an
    // extern callee without an explicit `@cast<int64>(...)` wrapper.
    bool is_extern = false;

    // Line of declaration (for diagnostics)
    int decl_line = 0;
    int decl_column = 0;
};

// ============================================================================
// v0.27.1: Region helpers
// ============================================================================

/**
 * Derive the Region of a variable declaration from its qualifier
 * flags. Precedence: wildx > wild > gc > stack > default(stack).
 *
 * Note: this looks at the VarDeclStmt only; it does not inspect the
 * initializer. That is correct for v0.27.1 (binding-site classification
 * matches how the parser surfaces the keyword). v0.27.3 will extend
 * this to handle field-level region propagation through struct types.
 */
inline Region regionFromVarDecl(const ::npk::VarDeclStmt& stmt) {
    if (stmt.isWildx) return Region::Wildx;
    if (stmt.isWild)  return Region::Wild;
    if (stmt.isGC)    return Region::Gc;
    if (stmt.isStack) return Region::Stack;
    // MEM-DEC-001: default region is stack, not GC.
    return Region::Stack;
}

/**
 * Human-readable name for a Region (used by --dump-regions and
 * future diagnostic messages).
 */
inline const char* regionName(Region r) {
    switch (r) {
        case Region::Stack:   return "stack";
        case Region::Gc:      return "gc";
        case Region::Wild:    return "wild";
        case Region::Wildx:   return "wildx";
        case Region::Unknown: return "unknown";
    }
    return "unknown";
}

// ============================================================================
// Borrow Checker Class
// ============================================================================

class BorrowChecker {
private:
    LifetimeContext ctx;
    std::vector<BorrowError> errors;
    
    // v0.6.3: Debug instrumentation flag (--borrow-debug)
    bool borrow_debug = false;
    
    // v0.6.4: Borrow dump flag (--borrow-dump)
    bool borrow_dump_enabled = false;
    
    // ========================================================================
    // Z3 Integration (v0.6.1: Precision Improvements)
    // ========================================================================
    
    // Optional Z3 verifier for precision proofs (nullptr if Z3 not available)
    Z3Verifier* z3_verifier = nullptr;

    // A-004: Rules/limit registry for Z3-backed index disjointness proofs.
    // Populated by setRulesTable() from main.cpp before analyze() is called.
    const std::unordered_map<std::string, RulesDeclStmt*>* rules_table_ = nullptr;

    // v0.29.3 DROP-DEC-007: wild RAII opt-in flag. When true, recordWildAlloc
    // is skipped for canonical `wild T:x = T{...}` struct bindings (IRGen
    // emits `npk_free` at scope end). See setWildRaiiEnabled().
    bool wild_raii_enabled_ = false;

    // v0.29.4 DROP-DEC-007: wildx RAII opt-in flag. When true, recordWildAlloc
    // is skipped for `wildx T->:p = wildx_alloc(N);` bindings (IRGen emits
    // `npk_wildx_free` at scope end). See setWildxRaiiEnabled().
    bool wildx_raii_enabled_ = false;

    // A-004: Maps variable name → limitRulesName, populated in checkVarDecl.
    std::unordered_map<std::string, std::string> var_limit_rules_;

    // v0.27.9 (ARIA-032 — handle-outlives-arena): track which arena binding
    // each Handle<T> binding was allocated from, plus the set of arena
    // bindings that have been explicitly destroyed via HandleArena.destroy.
    // Populated by checkVarDecl when the initializer is HandleArena.alloc(a,
    // size) where `a` is a plain identifier; consulted by checkCallExpr at
    // every HandleArena.{deref,free} use to fail the static check if the
    // arena has been destroyed earlier in the same function body.
    std::unordered_map<std::string, std::string> handle_arena_map_;
    std::unordered_set<std::string> destroyed_arenas_;

    // v0.28.4 (ARIA-032 Phase 2): arenas declared LOCALLY in the current
    // function (initializer was `HandleArena_create()`). Used by return/
    // pass to reject returning a `Handle<T>` (or a fresh `HandleArena_alloc`
    // call) whose arena is local — the handle is dead the moment the
    // function frame unwinds. Parameters and arenas threaded in from a
    // caller are NOT entered here, so handles from them may legitimately
    // escape. Save/restored around each function body in checkStatement.
    std::unordered_set<std::string> local_arenas_;

    // v0.28.4.1 (ARIA-032 Phase 2 part B): for each struct-typed binding,
    // the list of arena names referenced by its handle-bearing fields at
    // construction time. Populated in checkVarDecl when the initializer
    // is an ObjectLiteralExpr with a struct type_name and one or more
    // fields whose value is an IdentifierExpr in handle_arena_map_.
    // Consulted by checkHandleArenaEscape at return/pass: if any tracked
    // arena is in local_arenas_, the struct cannot escape. Save/restored
    // per-function like the other handle maps.
    std::unordered_map<std::string, std::vector<std::string>>
        struct_handle_arenas_;

    // Prove two index expressions are disjoint using Z3
    // Returns true if provably disjoint, false if unknown/overlapping
    bool proveIndexDisjoint(ASTNode* idx1, ASTNode* idx2);
    
    // ========================================================================
    // Function Summary Registry (v0.6.0: Inter-Procedural Analysis)
    // ========================================================================
    
    // Maps function name -> borrow summary (populated during collectFunctionSummaries)
    std::unordered_map<std::string, FunctionBorrowSummary> func_summaries;
    
    // Name of the function currently being analyzed (empty if global scope)
    std::string current_function;

    // ========================================================================
    // Trait Registry (v0.6.2: Trait Integration)
    // ========================================================================
    
    // Maps trait name -> list of method signatures (for validation)
    std::unordered_map<std::string, std::vector<TraitMethod>> trait_methods;
    
    // Maps type name -> set of implemented trait names
    std::unordered_map<std::string, std::unordered_set<std::string>> type_traits;
    
    // Types that implement Copyable are exempt from move semantics
    std::unordered_set<std::string> copyable_types;
    
    // Types that implement Droppable have custom destructors
    std::unordered_set<std::string> droppable_types;
    
    /**
     * First-pass: collect borrow summaries for all function declarations
     * Walks the AST without doing full analysis, just extracts signatures.
     */
    void collectFunctionSummaries(ASTNode* ast);
    
    /**
     * Build a FunctionBorrowSummary from a FuncDeclStmt
     */
    FunctionBorrowSummary buildSummary(FuncDeclStmt* func);
    
    /**
     * Register function parameters as variables in the current scope
     * Also records borrows for $$i/$$m parameters
     */
    void registerFunctionParams(FuncDeclStmt* func);
    
    /**
     * At a call site, check argument ownership against callee summary
     * Enforces: move semantics, borrow rules, mutability requirements
     */
    // v0.25.5 (BORROW-010): `param_offset` lets UFCS method-call dispatch
    // skip the summary's implicit-`self` slot so call-site arguments line up
    // with the remaining parameters. Defaults to 0 (direct calls).
    void checkCallOwnership(CallExpr* expr, const FunctionBorrowSummary& summary,
                            size_t param_offset = 0);
    
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
    * @param is_mutable true for $$m, false for $$i
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
     * Record that a pointer variable aliases a path derived from a pin.
     */
    void recordPinDerivedAlias(const std::string& alias, ASTNode* initializer, ASTNode* node);

    /**
     * Detect whether an expression is rooted in a pin reference or pin-derived
     * pointer alias, returning provenance for diagnostics.
     */
    bool findPinnedRootedPath(ASTNode* node,
                              std::string& pin_ref,
                              std::string& host,
                              std::string& path) const;
    
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
    void recordWildAlloc(const std::string& var, ASTNode* node, const std::string& size_expr = "");
    
    /**
     * Record deallocation of wild memory
     */
    void recordWildFree(const std::string& var, ASTNode* node);
    
    /**
     * Check for use-after-free on wild memory
     */
    bool checkWildUse(const std::string& var, ASTNode* node);
    
    /**
     * v0.6.3: Record reallocation — invalidates existing borrows, updates size
     */
    void recordWildRealloc(const std::string& var, ASTNode* node, const std::string& new_size_expr);

    /**
     * Detect memory leaks (wild memory not freed before scope exit)
     */
    void checkForLeaks();

    /**
     * v0.25.1 (BORROW-003): Detect memory leaks across ALL enclosing scopes,
     * up to the function root. Used on early-exit statements (`pass`, `fail`,
     * `return`, `exit`) where the normal scope-pop leak check never fires
     * because the inner scopes are abandoned by control flow.
     */
    void checkForLeaksAllScopes();

    // ========================================================================
    // Defer Enforcement (ARIA-014)
    // ========================================================================

    /**
     * Record a defer statement that frees a wild variable
     * ARIA-014: Every wild allocation must be paired with defer free
     */
    void recordDeferFree(const std::string& var, int line);

    /**
     * Check if a wild variable has a matching defer statement
     */
    bool hasDeferObligation(const std::string& var) const;

    /**
     * Verify defer obligations are satisfied at scope exit
     */
    void verifyDeferObligations();

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
    void checkLoopStmt(LoopStmt* stmt);
    void checkBlockStmt(BlockStmt* stmt);
    void checkReturnStmt(ReturnStmt* stmt);
    void checkPassStmt(PassStmt* stmt);
    void checkFailStmt(FailStmt* stmt);
    void checkTillStmt(TillStmt* stmt);
    void checkDeferStmt(DeferStmt* stmt);

    // v0.20.4: Lambda / closure escape analysis
    // isEscaping=true when the lambda is directly returned/passed from the function.
    // isPassedToFunc=true when passed as an argument to another function.
    void checkLambdaExpr(LambdaExpr* lambda, bool isEscaping, bool isPassedToFunc);

    // A-003: Borrow checking across async await suspension points
    void checkAwaitExpr(AwaitExpr* expr);

    // v0.6.1: Pattern match borrow flow
    void checkPickStmt(PickStmt* stmt);
    void checkWhenStmt(WhenStmt* stmt);
    
    // v0.6.1: Return type borrow validation
    void checkReturnBorrowEscape(ASTNode* returnValue, ASTNode* context);

    // v0.28.4 (ARIA-032 Phase 2): reject returning/passing a Handle<T> bound
    // to a locally-created arena, or a fresh `HandleArena.alloc(<localArena>,
    // ..)` expression. Consults `local_arenas_` + `handle_arena_map_`.
    void checkHandleArenaEscape(ASTNode* value, ASTNode* context);

    // v0.6.2: Trait Integration
    void checkTraitDeclStmt(TraitDeclStmt* stmt);
    void checkImplDeclStmt(ImplDeclStmt* stmt);

    // ========================================================================
    // Deep AST Scanning for Defer Blocks
    // ========================================================================

    /**
     * Deep scan a defer block for free() calls
     * Handles nested structures: blocks, conditionals, loops, wrappers
     *
     * @param node The AST node to scan
     * @param deferLine Line of the defer statement (for error reporting)
     * @param must_free If true, requires free on ALL paths (default: false = ANY path)
     * @return Set of variable names that are freed in this subtree
     */
    std::unordered_set<std::string> scanDeferBlockForFree(ASTNode* node, int deferLine, bool must_free = false);

    /**
     * Check if a function name is a known deallocator
     * Includes built-ins and user-annotated functions
     */
    bool isDeallocator(const std::string& funcName) const;

    /**
     * Check if this is a method-style cleanup call (e.g., x.dispose())
     * Returns the object being disposed, or empty string if not a cleanup call
     */
    std::string checkMethodCleanup(CallExpr* call) const;
    
    // Expression visitors
    void checkBinaryExpr(BinaryExpr* expr);
    void checkUnaryExpr(UnaryExpr* expr);
    void checkIdentifier(IdentifierExpr* expr);
    void checkCallExpr(CallExpr* expr);
    void checkMoveExpr(MoveExpr* expr);

    /**
     * ARIA-017: Check if a reference would escape its scope
     * Detects returning or passing references to local variables
     */
    void checkReferenceEscape(ASTNode* value, ASTNode* context);

    // ========================================================================
    // Liveness Analysis (Gemini Report)
    // ========================================================================

    /**
     * Compute liveness information for a loop
     * Determines which variables are live at the loop header
     *
     * @param loopBody The loop body AST node
     * @param loopCondition The loop condition (may use variables)
     * @return LivenessAnalysis with live variable sets
     */
    LivenessAnalysis computeLoopLiveness(ASTNode* loopBody, ASTNode* loopCondition);

    /**
     * Collect variables used in an expression (for liveness)
     */
    void collectUsedVariables(ASTNode* expr, std::unordered_set<std::string>& used);

    // ========================================================================
    // Two-Phase Borrowing (Gemini Report)
    // ========================================================================

    /**
     * Create a reserved (not yet active) mutable borrow
     * Used for method call receivers during argument evaluation
     *
     * @param host Variable being borrowed
     * @param reference Reference variable
     * @param node AST node for error reporting
     * @return The created Loan (in RESERVED phase)
     */
    Loan createReservedLoan(const std::string& host, const std::string& reference, ASTNode* node);

    /**
     * Activate a reserved loan to full mutable
     * Called after method arguments are evaluated
     *
     * @param loan The loan to activate
     * @param node AST node for error reporting
     * @return true if activation succeeded
     */
    bool activateLoan(Loan& loan, ASTNode* node);

    // ========================================================================
    // TBB Error Detection (Gemini Report)
    // ========================================================================

    /**
     * Check if a condition is testing for TBB ERR state
     * Patterns: x == ERR, x == TBB8_ERR, x.is_err(), etc.
     *
     * @param condition The condition expression
     * @return Variable name if this is an ERR check, empty string otherwise
     */
    std::string detectTBBErrorCheck(ASTNode* condition);

    /**
     * Check if a type name is a TBB type
     */
    bool isTBBType(const std::string& typeName) const;

    // ========================================================================
    // Access Path Extraction (Gemini Report: Split Borrows)
    // ========================================================================

    /**
     * Extract the access path from an expression
     * Handles: identifier, member access, array subscript
     *
     * @param expr The expression to analyze
     * @return AccessPath representing the full path
     */
    AccessPath extractAccessPath(ASTNode* expr);

    /**
     * Record a borrow with access path tracking
     * Enables split borrowing of disjoint fields
     */
    void recordBorrowWithPath(const AccessPath& path, const std::string& reference,
                              bool is_mutable, ASTNode* node);

    /**
     * Check borrow rules with access path awareness
     * Allows disjoint paths to be borrowed simultaneously
     */
    bool checkBorrowRulesWithPath(const AccessPath& path, bool is_mutable, ASTNode* node);

    // ========================================================================
    // Error Reporting
    // ========================================================================
    
    void tagCode(const std::string& code);  // Tag last error with diagnostic code
    void addError(const std::string& message, ASTNode* node);
    void addError(const std::string& message, int line, int column);
    void addError(const std::string& message, ASTNode* node,
                 const std::string& related_msg, int related_line, int related_col);
    void addErrorWithSuggestion(const std::string& message, ASTNode* node,
                                const std::string& suggestion);
    void addErrorWithSuggestion(const std::string& message, ASTNode* node,
                                const std::string& related_msg, int related_line, int related_col,
                                const std::string& suggestion);
    void addWarning(const std::string& message, ASTNode* node);
    void addWarning(const std::string& message, ASTNode* node,
                    const std::string& suggestion);
    
public:
    BorrowChecker() = default;
    
    /**
     * Set Z3 verifier for precision improvements (v0.6.1)
     * When set, enables Z3-backed index disjointness proofs and
     * conditional borrow narrowing.
     */
    void setZ3Verifier(Z3Verifier* z3v) { z3_verifier = z3v; }

    /**
     * A-004: Set the Rules declaration table for Z3-backed index disjointness.
     * Call this before analyze() when the program uses limit<Rules> qualifiers.
     */
    void setRulesTable(const std::unordered_map<std::string, RulesDeclStmt*>* rules) { rules_table_ = rules; }

    /**
     * v0.29.3 DROP-DEC-007: opt-in RAII flag. When set (driven by
     * `TypeChecker::hasWildRaii()` after sema sees `use "drop.npk".*;`),
     * the borrow checker suppresses the ARIA-014 leak obligation for
     * wild-struct bindings (the IR generator emits `npk_free` at scope
     * end). Pre-RAII modules leave this false and keep the existing
     * explicit-free contract.
     */
    void setWildRaiiEnabled(bool enabled) { wild_raii_enabled_ = enabled; }
    void setWildxRaiiEnabled(bool enabled) { wildx_raii_enabled_ = enabled; }
    
    /**
     * v0.6.3: Enable borrow debug diagnostics (--borrow-debug)
     * Prints allocation/free/realloc/borrow events to stderr
     */
    void setBorrowDebug(bool enabled) { borrow_debug = enabled; }
    void setBorrowDump(bool enabled) { borrow_dump_enabled = enabled; }
    
    /**
     * v0.6.4: Dump borrow state visualization (--borrow-dump)
     * Outputs per-function summary: active loans, pins, wild state, moves
     */
    void dumpBorrowState(std::ostream& out, const std::string& func_name = "") const;
    
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
} // namespace npk

#endif // ARIA_SEMA_BORROW_CHECKER_H

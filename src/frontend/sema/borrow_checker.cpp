#include "frontend/sema/borrow_checker.h"
#include <sstream>
#include <algorithm>
#include <iostream>

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
    snap.moved_variables = this->moved_variables;
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
    this->moved_variables = snap.moved_variables;
    this->current_depth = snap.current_depth;
    this->scope_stack = snap.scope_stack;
}

void LifetimeContext::merge(const LifetimeContext& then_state, const LifetimeContext& else_state) {
    // Conservative merging for safety (ARIA-014)
    // A variable is valid only if valid in ALL branches

    // Merge wild states: conservative approach for state machine
    for (auto& [var, state] : then_state.wild_states) {
        auto else_it = else_state.wild_states.find(var);
        if (else_it != else_state.wild_states.end()) {
            // Variable exists in both branches
            if (state == else_it->second) {
                // Same state in both branches - use that state
                wild_states[var] = state;
            } else if (state == WildState::FREED || else_it->second == WildState::FREED) {
                // If freed in ANY branch, conservative: treat as potentially freed
                // This is unsafe to use - mark as UNKNOWN
                wild_states[var] = WildState::UNKNOWN;
            } else if (state == WildState::MOVED || else_it->second == WildState::MOVED) {
                // If moved in ANY branch, treat as moved (unsafe to use)
                wild_states[var] = WildState::MOVED;
            } else {
                // States differ but neither freed/moved - mark as UNKNOWN
                wild_states[var] = WildState::UNKNOWN;
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

    // ARIA-021: Merge loan origins using UNION from EITHER branch (phi node logic)
    // If a reference might borrow from different sources on different paths,
    // we must track all possible origins conservatively
    for (const auto& [ref, origins] : then_state.loan_origins) {
        loan_origins[ref].insert(origins.begin(), origins.end());
    }
    for (const auto& [ref, origins] : else_state.loan_origins) {
        loan_origins[ref].insert(origins.begin(), origins.end());
    }

    // ARIA-021: Merge active_loans using UNION (if loan may exist, assume it exists)
    // This is critical for Bug #5: conditional borrow merge
    // After: if (cond) { r = $x; } else { r = $y; }
    // Both x and y should be considered potentially borrowed
    for (const auto& [host, loans] : then_state.active_loans) {
        auto& our_loans = active_loans[host];
        for (const auto& loan : loans) {
            // Only add if not already present
            bool found = false;
            for (const auto& existing : our_loans) {
                if (existing.borrower == loan.borrower) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                our_loans.push_back(loan);
            }
        }
    }
    for (const auto& [host, loans] : else_state.active_loans) {
        auto& our_loans = active_loans[host];
        for (const auto& loan : loans) {
            bool found = false;
            for (const auto& existing : our_loans) {
                if (existing.borrower == loan.borrower) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                our_loans.push_back(loan);
            }
        }
    }

    // ARIA-021: Merge active_pins using UNION
    for (const auto& [host, pinner] : then_state.active_pins) {
        if (active_pins.find(host) == active_pins.end()) {
            active_pins[host] = pinner;
        }
    }
    for (const auto& [host, pinner] : else_state.active_pins) {
        if (active_pins.find(host) == active_pins.end()) {
            active_pins[host] = pinner;
        }
    }
}

bool LifetimeContext::mergeLoopBackEdge(const LifetimeContext& back_edge_state) {
    // Original implementation - delegates to liveness-aware version with empty liveness
    // This maintains backwards compatibility while enabling the new feature
    LivenessAnalysis empty_liveness;

    // For backwards compatibility, consider all variables as live
    // This preserves the original conservative behavior
    for (const auto& [var, _] : back_edge_state.var_depths) {
        empty_liveness.markLiveAtHeader(var);
    }

    return mergeLoopBackEdgeLivenessAware(back_edge_state, empty_liveness);
}

bool LifetimeContext::mergeLoopBackEdgeLivenessAware(const LifetimeContext& back_edge_state,
                                                      const LivenessAnalysis& liveness) {
    // Liveness-aware fixpoint iteration (Gemini Report improvement)
    // Only propagate loans for LIVE variables at loop header
    // This prevents false conflicts from zombie loans

    bool changed = false;

    // ========================================================================
    // Liveness-Aware Loan Merging (Gemini Report: Loop-Carried Dependencies)
    // ========================================================================
    // Only propagate loans if BOTH the host AND the borrower are live at the
    // loop header. Dead variables' loans are "zombie" loans that would cause
    // false conflicts in the next iteration.

    for (const auto& [host, loans] : back_edge_state.active_loans) {
        // Skip if the host variable is not live at the loop header
        // Dead hosts mean the loan is a zombie that won't affect next iteration
        if (!liveness.isLiveAtHeader(host)) {
            continue;
        }

        auto& our_loans = active_loans[host];
        for (const auto& loan : loans) {
            // Also check if the borrower is live - if the reference dies,
            // the loan it holds should not propagate
            if (!liveness.isLiveAtHeader(loan.borrower)) {
                continue;
            }

            // Check if this exact loan already exists
            bool found = false;
            for (const auto& existing : our_loans) {
                if (existing.borrower == loan.borrower &&
                    existing.is_mutable == loan.is_mutable &&
                    existing.creation_line == loan.creation_line) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                our_loans.push_back(loan);
                changed = true;
            }
        }
    }

    // Merge loan origins: union (also liveness-filtered)
    for (const auto& [ref, origins] : back_edge_state.loan_origins) {
        // Only propagate if the reference is live
        if (!liveness.isLiveAtHeader(ref)) {
            continue;
        }

        auto& our_origins = loan_origins[ref];
        size_t old_size = our_origins.size();
        for (const auto& origin : origins) {
            // Only add origins that are live
            if (liveness.isLiveAtHeader(origin)) {
                our_origins.insert(origin);
            }
        }
        if (our_origins.size() > old_size) {
            changed = true;
        }
    }

    // Merge wild states: conservative lattice join for loop analysis
    // Implements explicit MAY_FREED handling as per Appendage Theory
    for (const auto& [var, state] : back_edge_state.wild_states) {
        auto it = wild_states.find(var);
        if (it == wild_states.end()) {
            wild_states[var] = state;
            changed = true;
        } else if (it->second != state) {
            // States differ - apply conservative lattice merge
            WildState current = it->second;
            WildState back = state;
            WildState merged;

            // Case 1: ALLOCATED ⊔ FREED = MAY_FREED (The critical MAY_FREED scenario)
            // If variable was freed/moved in the loop (back_edge) and ALLOCATED at start,
            // it is now MAY_FREED. This forces the analyzer to reject uses unless re-init.
            if ((current == WildState::ALLOCATED && back == WildState::FREED) ||
                (current == WildState::FREED && back == WildState::ALLOCATED)) {
                merged = WildState::MAY_FREED;
            }
            // Case 2: ALLOCATED ⊔ MOVED = MAY_FREED (ownership transferred in loop)
            else if ((current == WildState::ALLOCATED && back == WildState::MOVED) ||
                     (current == WildState::MOVED && back == WildState::ALLOCATED)) {
                merged = WildState::MAY_FREED;
            }
            // Case 3: UNINITIALIZED ⊔ (anything != UNINITIALIZED) = UNKNOWN
            // Variable may not be initialized on first iteration entry path
            else if (current == WildState::UNINITIALIZED && back != WildState::UNINITIALIZED) {
                merged = WildState::UNKNOWN;
            }
            else if (back == WildState::UNINITIALIZED && current != WildState::UNINITIALIZED) {
                merged = WildState::UNKNOWN;
            }
            // Case 4: UNKNOWN or MAY_FREED is the sink (Top) - stays at top
            else if (current == WildState::UNKNOWN || back == WildState::UNKNOWN ||
                     current == WildState::MAY_FREED || back == WildState::MAY_FREED) {
                merged = WildState::UNKNOWN;
            }
            // Case 5: Identical terminal states stay the same
            else if (current == back) {
                merged = current;
            }
            // Case 6: Any other combination -> UNKNOWN (conservative)
            else {
                merged = WildState::UNKNOWN;
            }

            if (it->second != merged) {
                it->second = merged;
                changed = true;
            }
        }
    }

    // Merge pending wild frees: union
    for (const auto& var : back_edge_state.pending_wild_frees) {
        if (pending_wild_frees.insert(var).second) {
            changed = true;
        }
    }

    // Merge moved variables: union
    for (const auto& var : back_edge_state.moved_variables) {
        if (moved_variables.insert(var).second) {
            changed = true;
        }
    }

    return changed;
}

// Constant for widening: max loans before collapsing to unknown sentinel
static constexpr size_t MAX_LOANS_PER_HOST = 100;
static const std::string UNKNOWN_LOAN_MARKER = "*UNKNOWN_WIDENED*";

void LifetimeContext::widen() {
    // Widening operator: promote uncertain states to conservative top
    // Called after WIDENING_THRESHOLD iterations to force convergence
    // This guarantees termination of the fixpoint algorithm

    // ========================================================================
    // Part 1: Wild State Widening
    // ========================================================================
    // Promote any non-terminal state to UNKNOWN
    for (auto& [var, state] : wild_states) {
        if (state == WildState::MAY_FREED) {
            state = WildState::UNKNOWN;  // Conservative: might be freed
        }
        // Also widen ALLOCATED in loops - it may not remain allocated
        // after arbitrary iterations
        if (state == WildState::ALLOCATED) {
            state = WildState::UNKNOWN;
        }
    }

    // ========================================================================
    // Part 2: Active Loan Widening (Fixes unbounded loan growth)
    // ========================================================================
    // If a host has > MAX_LOANS_PER_HOST active loans, the code is too complex
    // to analyze precisely. We force convergence by replacing the set with a
    // single "Universal Unknown Loan" sentinel.
    for (auto& [host, loans] : active_loans) {
        if (loans.size() > MAX_LOANS_PER_HOST) {
            // Unbounded growth detected. This happens in pathological loops
            // (e.g., while(true) { let r = $h }) where a new borrow is created
            // in every iteration.

            // Clear and replace with conservative sentinel
            loans.clear();

            // Create "Top" element: unknown mutable borrow
            // Mutable because it's exclusive - blocks all other access
            Loan unknown_loan(UNKNOWN_LOAN_MARKER, true, 0, 0);
            loans.push_back(unknown_loan);

            // Note: This forces the analysis to assume the variable is
            // permanently borrowed for the remainder of the scope
        }

        // Also check for widened sentinel stickiness:
        // If we already have the unknown sentinel, keep only it
        if (!loans.empty() && loans[0].borrower == UNKNOWN_LOAN_MARKER) {
            if (loans.size() > 1) {
                // Other loans accumulated - collapse back to just sentinel
                Loan sentinel = loans[0];
                loans.clear();
                loans.push_back(sentinel);
            }
        }
    }
}

/**
 * Helper to establish structural equality between two Loan objects.
 *
 * Verifies that the borrower identity, mutability permissions, and
 * source provenance are identical. This is critical for correct fixpoint
 * convergence - comparing only counts can cause premature termination
 * when different loans exist but the count stays the same.
 */
static bool are_loans_equal(const Loan& a, const Loan& b) {
    // Check Borrower Identity (Variable Name)
    if (a.borrower != b.borrower) return false;

    // Check Mutability Permission (Read-Only vs Read-Write)
    if (a.is_mutable != b.is_mutable) return false;

    // Check Provenance (Source Location)
    // This distinguishes between the same variable being borrowed
    // at different points in the code.
    if (a.creation_line != b.creation_line) return false;
    if (a.creation_column != b.creation_column) return false;

    return true;
}

bool LifetimeContext::equivalent(const LifetimeContext& other) const {
    // Check if two states are equivalent for fixpoint detection
    // CRITICAL: Must use structural equality, not just counts!

    // ==========================================================
    // 1. Compare Wild Memory States
    // ==========================================================
    if (wild_states.size() != other.wild_states.size()) return false;
    for (const auto& [var, state] : wild_states) {
        auto it = other.wild_states.find(var);
        if (it == other.wild_states.end() || it->second != state) return false;
    }

    // ==========================================================
    // 2. Compare Active Loans (STRUCTURAL EQUALITY)
    // ==========================================================
    // PREVIOUS DEFECT: Only checked size().
    // FIX: Checks structural identity of every loan.
    if (active_loans.size() != other.active_loans.size()) return false;
    for (const auto& [host, my_loans] : active_loans) {
        // Find the same host in the other context
        auto it = other.active_loans.find(host);

        // If host is missing in the other context, states are not equivalent
        if (it == other.active_loans.end()) return false;

        const auto& other_loans = it->second;

        // Fast path: Count mismatch implies inequality
        if (my_loans.size() != other_loans.size()) return false;

        // Deep Path: Structural Comparison
        // We assume deterministic ordering from AST traversal.
        for (size_t i = 0; i < my_loans.size(); ++i) {
            if (!are_loans_equal(my_loans[i], other_loans[i])) {
                // Identity mismatch found (e.g. different borrower name)
                return false;
            }
        }
    }

    // ==========================================================
    // 3. Compare Loan Origins (Phi Node Merging)
    // ==========================================================
    if (loan_origins.size() != other.loan_origins.size()) return false;
    for (const auto& [ref, origins] : loan_origins) {
        auto it = other.loan_origins.find(ref);
        if (it == other.loan_origins.end() || it->second != origins) return false;
    }

    // ==========================================================
    // 4. Compare Pending Frees (Leak Detection)
    // ==========================================================
    if (pending_wild_frees != other.pending_wild_frees) return false;

    // ==========================================================
    // 5. Compare Moved Variables (Use-After-Move)
    // ==========================================================
    if (moved_variables != other.moved_variables) return false;

    // If all checks pass, the states are mathematically equivalent.
    return true;
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
        addErrorWithSuggestion("Cannot borrow '" + host + "' as mutable because it is already borrowed",
                node,
                "Existing " + std::string(first.is_mutable ? "mutable" : "immutable") + 
                " borrow by '" + first.borrower + "' created here",
                first.creation_line, first.creation_column,
                "hint: consider borrowing immutably with '$" + host + "', or limit the existing borrow's scope");
        return false;
    } else {
        // Requesting immutable borrow
        // Error if a mutable loan exists
        for (const auto& loan : loans) {
            if (loan.is_mutable) {
                addErrorWithSuggestion("Cannot borrow '" + host + "' as immutable because it is already borrowed as mutable",
                        node,
                        "Mutable borrow by '" + loan.borrower + "' created here",
                        loan.creation_line, loan.creation_column,
                        "hint: end the mutable borrow before creating an immutable one, or clone the value");
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
        addErrorWithSuggestion("Double free of variable '" + var + "' (already freed)", node,
                "hint: remove the duplicate free(), or set '" + var + "' to null after the first free");
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

    switch (it->second) {
        case WildState::FREED:
            addErrorWithSuggestion("Use after free: variable '" + var + "' was already freed", node,
                "hint: do not use '" + var + "' after calling free() on it");
            return false;

        case WildState::MOVED:
            addErrorWithSuggestion("Use after move: variable '" + var + "' was moved", node,
                "hint: clone the value before moving if you need it afterward");
            return false;

        case WildState::UNINITIALIZED:
            addErrorWithSuggestion("Use of uninitialized wild pointer '" + var + "'", node,
                "hint: allocate memory for '" + var + "' before using it");
            return false;

        case WildState::UNKNOWN:
            addError("Use of wild pointer '" + var + "' with unknown state " +
                    "(may have been freed or moved in a branch)", node);
            return false;

        case WildState::MAY_FREED:
            addError("Use of wild pointer '" + var + "' that may have been freed " +
                    "in a previous loop iteration", node);
            return false;

        case WildState::ALLOCATED:
            return true;
    }

    return true;
}

void BorrowChecker::checkForLeaks() {
    // Check for wild memory that wasn't freed
    if (!ctx.scope_stack.empty()) {
        const auto& current_scope_vars = ctx.scope_stack.back();

        for (const auto& var : current_scope_vars) {
            if (ctx.pending_wild_frees.count(var) > 0) {
                BorrowError err(1, 1, "Memory leak: wild variable '" + var + "' was not freed before scope exit",
                        "hint: add 'defer { free(" + var + "); }' after allocation, or free before end of scope");
                errors.push_back(err);
            }
        }
    }
}

// ============================================================================
// Defer Enforcement (ARIA-014)
// ============================================================================

void BorrowChecker::recordDeferFree(const std::string& var, int line) {
    ctx.defer_obligations[var] = line;
    // Remove from pending_wild_frees since we have a defer
    ctx.pending_wild_frees.erase(var);
}

bool BorrowChecker::hasDeferObligation(const std::string& var) const {
    return ctx.defer_obligations.find(var) != ctx.defer_obligations.end();
}

void BorrowChecker::verifyDeferObligations() {
    // ARIA-014: Every wild allocation must be paired with defer free
    // Check that all wild variables in current scope have defer obligations
    if (!ctx.scope_stack.empty()) {
        const auto& current_scope_vars = ctx.scope_stack.back();

        for (const auto& var : current_scope_vars) {
            auto state_it = ctx.wild_states.find(var);
            if (state_it != ctx.wild_states.end() &&
                state_it->second == WildState::ALLOCATED) {
                // Check if this wild var has a matching defer
                if (!hasDeferObligation(var) &&
                    ctx.pending_wild_frees.count(var) > 0) {
                    addError("Safety Violation: Wild allocation '" + var +
                            "' is missing a mandatory 'defer free(" + var + ")' statement",
                            1, 1);
                }
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
        
        case ASTNode::NodeType::FUNC_DECL: {
            // Check function body
            auto* funcDecl = static_cast<FuncDeclStmt*>(stmt);
            if (funcDecl->body) {
                ctx.enterScope();
                checkStatement(funcDecl->body.get());
                ctx.exitScope();
            }
            break;
        }
        
        case ASTNode::NodeType::PROGRAM: {
            // Program is a container of declarations
            auto* program = static_cast<ProgramNode*>(stmt);
            for (const auto& decl : program->declarations) {
                checkStatement(decl.get());
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
            checkBlockStmt(static_cast<BlockStmt*>(stmt));
            break;
            
        case ASTNode::NodeType::RETURN:
            checkReturnStmt(static_cast<ReturnStmt*>(stmt));
            break;

        case ASTNode::NodeType::PASS:
            checkPassStmt(static_cast<PassStmt*>(stmt));
            break;

        case ASTNode::NodeType::DEFER:
            checkDeferStmt(static_cast<DeferStmt*>(stmt));
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
            
        case ASTNode::NodeType::MOVE:
            checkMoveExpr(static_cast<MoveExpr*>(expr));
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

    // Set scope depth for Appendage Theory tracking
    stmt->scope_depth = ctx.getScopeDepth();

    // Register the variable first
    registerVariable(stmt->varName, stmt);

    // Check initializer if present
    if (stmt->initializer) {
        checkExpression(stmt->initializer.get());

        // ====================================================================
        // v0.2.35: $$i/$$m qualifier-based borrows
        // ====================================================================
        if (stmt->isBorrowImm || stmt->isBorrowMut) {
            // Extract borrow target from initializer (must be an identifier —
            // validated by type checker, but guard defensively)
            if (stmt->initializer->type == ASTNode::NodeType::IDENTIFIER) {
                std::string borrow_target = 
                    static_cast<IdentifierExpr*>(stmt->initializer.get())->name;
                bool is_mutable = stmt->isBorrowMut;
                recordBorrow(borrow_target, stmt->varName, is_mutable, stmt);
            }
            // Early return — $$i/$$m variables don't need the legacy $ checks
            return;
        }

        // Check if initializer is a borrow or pin operation
        if (stmt->initializer->type == ASTNode::NodeType::UNARY_OP) {
            UnaryExpr* unary = static_cast<UnaryExpr*>(stmt->initializer.get());

            // Determine mutability from variable type name
            // int32$mut -> mutable, int32$ -> immutable
            bool is_mutable_type = stmt->typeName.find("$mut") != std::string::npos;
            bool is_ref_type = stmt->typeName.find("$") != std::string::npos;

            // Check for !$x pattern (immutable borrow - alternative syntax)
            bool is_negated_borrow = false;
            if (unary->op.type == frontend::TokenType::TOKEN_BANG &&
                unary->operand && unary->operand->type == ASTNode::NodeType::UNARY_OP) {
                UnaryExpr* inner = static_cast<UnaryExpr*>(unary->operand.get());
                // Detect borrow by AST flags OR by token type
                bool inner_is_borrow = inner->creates_loan ||
                                       inner->op.type == frontend::TokenType::TOKEN_DOLLAR;
                std::string inner_target = inner->loan_target;

                // If loan_target not set, extract from operand
                if (inner_target.empty() && inner->operand &&
                    inner->operand->type == ASTNode::NodeType::IDENTIFIER) {
                    inner_target = static_cast<IdentifierExpr*>(inner->operand.get())->name;
                }

                if (inner_is_borrow && !inner_target.empty()) {
                    // This is !$x - immutable borrow
                    is_negated_borrow = true;
                    recordBorrow(inner_target, stmt->varName, false, stmt);
                }
            }

            // Detect borrow by AST flags OR by token type (TOKEN_DOLLAR)
            bool is_borrow = unary->creates_loan ||
                             unary->op.type == frontend::TokenType::TOKEN_DOLLAR;
            std::string borrow_target = unary->loan_target;

            // If loan_target not set, extract from operand
            if (borrow_target.empty() && unary->operand &&
                unary->operand->type == ASTNode::NodeType::IDENTIFIER) {
                borrow_target = static_cast<IdentifierExpr*>(unary->operand.get())->name;
            }

            if (!is_negated_borrow && is_borrow && !borrow_target.empty()) {
                // Borrow operation: determine mutability from type name
                // If type contains $mut -> mutable borrow
                // If type contains $ but not $mut -> immutable borrow
                // Default (plain $ operator without type annotation) -> mutable borrow
                bool is_mutable = is_mutable_type || (!is_ref_type);
                recordBorrow(borrow_target, stmt->varName, is_mutable, stmt);
            }

            // Detect pin by AST flags OR by token type (TOKEN_HASH)
            bool is_pin = unary->creates_pin ||
                          unary->op.type == frontend::TokenType::TOKEN_HASH;
            std::string pin_target = unary->pin_target;

            // If pin_target not set, extract from operand
            if (pin_target.empty() && unary->operand &&
                unary->operand->type == ASTNode::NodeType::IDENTIFIER) {
                pin_target = static_cast<IdentifierExpr*>(unary->operand.get())->name;
            }

            if (is_pin && !pin_target.empty()) {
                // Pin operation: wild int8@:ptr = #x
                stmt->is_pinned_shadow = true;
                stmt->pinned_target = pin_target;
                recordPin(pin_target, stmt->varName, stmt);
                // Don't record as wild allocation - it's just a pin
                return;
            }
        }
    }

    // Check if this is a wild allocation (only if NOT a pin operation)
    // A pin creates a wild pointer to GC memory, not a wild allocation
    // Use isWild flag set by parser when 'wild' keyword is present
    bool is_wild_type = stmt->isWild || stmt->typeName.find("wild") == 0;
    if (is_wild_type && stmt->initializer && !stmt->is_pinned_shadow) {
        // This is an actual wild allocation (e.g., wild int8@:ptr = alloc(...))
        stmt->requires_drop = true;
        recordWildAlloc(stmt->varName, stmt);
    }
}

void BorrowChecker::checkAssignment(BinaryExpr* expr) {
    if (!expr) return;

    std::string target_name;

    // Check left side (ensure it's not pinned or borrowed mutably)
    if (expr->left->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* target = static_cast<IdentifierExpr*>(expr->left.get());
        target_name = target->name;

        // Check if target is pinned
        if (isPinned(target->name)) {
            addErrorWithSuggestion("Cannot assign to pinned variable '" + target->name + "'", expr,
                "hint: unpin the variable first, or work with a copy");
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

    // ARIA-021: Handle borrow assignment (r = $x)
    // When assigning a borrow expression to a reference variable,
    // we need to create a loan and track the origin
    if (!target_name.empty() && expr->right->type == ASTNode::NodeType::UNARY_OP) {
        auto* unary = static_cast<UnaryExpr*>(expr->right.get());
        if (unary->creates_loan && !unary->loan_target.empty()) {
            const std::string& host = unary->loan_target;
            bool is_mutable = (unary->op.type != frontend::TokenType::TOKEN_BANG);  // !$ is immutable

            // Check XOR rule: 1 mutable OR N immutable (not both)
            auto loans_it = ctx.active_loans.find(host);
            if (loans_it != ctx.active_loans.end() && !loans_it->second.empty()) {
                bool has_mutable = std::any_of(loans_it->second.begin(), loans_it->second.end(),
                    [](const Loan& l) { return l.is_mutable; });

                if (has_mutable) {
                    const Loan& existing = loans_it->second.front();
                    addError("Cannot borrow '" + host + "' because it is already mutably borrowed by '" +
                             existing.borrower + "'",
                             expr->right.get(),
                             "Mutable borrow was created here", existing.creation_line, existing.creation_column);
                    return;
                } else if (is_mutable) {
                    const Loan& existing = loans_it->second.front();
                    addError("Cannot borrow '" + host + "' as mutable because it is already borrowed",
                             expr->right.get(),
                             "Existing borrow by '" + existing.borrower + "' created here",
                             existing.creation_line, existing.creation_column);
                    return;
                }
            }

            // Create the loan
            Loan loan(target_name, is_mutable, expr->line, expr->column);
            ctx.active_loans[host].push_back(loan);
            ctx.loan_origins[target_name].insert(host);
        }
    }
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

    // ========================================================================
    // FIXPOINT ITERATION FOR LOOP ANALYSIS
    // ========================================================================
    // The problem: A single pass cannot detect loop-carried dependencies where
    // a reference created in iteration N conflicts with one from iteration N-1.
    //
    // Solution: Iterate until the borrow state stabilizes (reaches a fixed point).
    // At each iteration, we merge the back-edge state into the loop header.
    // ========================================================================

    // 1. Snapshot entry state (state before entering loop)
    LifetimeContext entry_state = ctx.snapshot();
    LifetimeContext loop_head_state = entry_state;

    bool changed = true;
    int iterations = 0;

    // 2. Fixpoint loop
    while (changed && iterations < MAX_FIXPOINT_ITERATIONS) {
        changed = false;
        iterations++;

        // Start analysis with current loop header state
        ctx.restore(loop_head_state);

        // Analyze condition (may create borrows)
        checkExpression(stmt->condition.get());

        // Analyze body in its own scope
        ctx.enterScope();
        if (stmt->body) {
            checkStatement(stmt->body.get());
        }

        // Capture back-edge state (state at end of loop body, before scope exit cleanup)
        LifetimeContext back_edge_state = ctx.snapshot();
        ctx.exitScope();

        // 3. Merge back-edge into header (The Join Operation)
        // If back_edge_state adds new loans/states to loop_head_state, iterate again
        if (loop_head_state.mergeLoopBackEdge(back_edge_state)) {
            changed = true;
        }

        // 4. Widening safeguard: after threshold, force convergence
        if (iterations >= WIDENING_THRESHOLD && changed) {
            loop_head_state.widen();
            // Do one more iteration with widened state, then stop
            if (iterations >= MAX_FIXPOINT_ITERATIONS - 1) {
                changed = false;
            }
        }
    }

    // 5. Final state: the fixed point header state
    // This represents the conservative approximation of all possible loop executions
    ctx.restore(loop_head_state);

    // Check for loop-carried borrow conflicts
    // After fixpoint, if a variable has multiple mutable borrows, report error
    for (const auto& [host, loans] : ctx.active_loans) {
        int mutable_count = 0;
        const Loan* first_mutable = nullptr;
        for (const auto& loan : loans) {
            if (loan.is_mutable) {
                mutable_count++;
                if (!first_mutable) first_mutable = &loan;
            }
        }
        if (mutable_count > 1) {
            addError("Loop-carried borrow conflict: '" + host +
                    "' is mutably borrowed multiple times across loop iterations",
                    stmt,
                    "First mutable borrow here",
                    first_mutable->creation_line, first_mutable->creation_column);
        }
    }
}

void BorrowChecker::checkForStmt(ForStmt* stmt) {
    if (!stmt) return;

    // ========================================================================
    // FIXPOINT ITERATION FOR FOR-LOOP ANALYSIS
    // ========================================================================
    // For loops have: initializer; condition; update; body
    // The initializer runs once, then (condition, body, update) repeats.
    // We need fixpoint iteration on the repeating part.
    // ========================================================================

    // Enter loop scope (initializer variables are in loop scope)
    ctx.enterScope();

    // Check initializer (runs once before loop)
    if (stmt->initializer) {
        checkStatement(stmt->initializer.get());
    }

    // Snapshot state after initializer (this is the entry to the repeating part)
    LifetimeContext entry_state = ctx.snapshot();
    LifetimeContext loop_head_state = entry_state;

    bool changed = true;
    int iterations = 0;

    // Fixpoint loop for the repeating part
    while (changed && iterations < MAX_FIXPOINT_ITERATIONS) {
        changed = false;
        iterations++;

        // Start with current loop header state
        ctx.restore(loop_head_state);

        // Analyze condition
        if (stmt->condition) {
            checkExpression(stmt->condition.get());
        }

        // Analyze body in its own nested scope
        ctx.enterScope();
        if (stmt->body) {
            checkStatement(stmt->body.get());
        }
        ctx.exitScope();

        // Analyze update expression
        if (stmt->update) {
            checkExpression(stmt->update.get());
        }

        // Capture back-edge state
        LifetimeContext back_edge_state = ctx.snapshot();

        // Merge back-edge into header
        if (loop_head_state.mergeLoopBackEdge(back_edge_state)) {
            changed = true;
        }

        // Widening safeguard
        if (iterations >= WIDENING_THRESHOLD && changed) {
            loop_head_state.widen();
            if (iterations >= MAX_FIXPOINT_ITERATIONS - 1) {
                changed = false;
            }
        }
    }

    // Final state
    ctx.restore(loop_head_state);

    // Check for loop-carried borrow conflicts
    for (const auto& [host, loans] : ctx.active_loans) {
        int mutable_count = 0;
        const Loan* first_mutable = nullptr;
        for (const auto& loan : loans) {
            if (loan.is_mutable) {
                mutable_count++;
                if (!first_mutable) first_mutable = &loan;
            }
        }
        if (mutable_count > 1) {
            addError("Loop-carried borrow conflict: '" + host +
                    "' is mutably borrowed multiple times across loop iterations",
                    stmt,
                    "First mutable borrow here",
                    first_mutable->creation_line, first_mutable->creation_column);
        }
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
            
            // Remove moved state for variables going out of scope
            ctx.moved_variables.erase(var);
            
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

        // ARIA-017: Check that returned references don't point to local variables
        // This prevents dangling references from escaping function scope
        checkReferenceEscape(stmt->value.get(), stmt);
    }
}

void BorrowChecker::checkPassStmt(PassStmt* stmt) {
    if (!stmt) return;

    // Check passed value
    if (stmt->value) {
        checkExpression(stmt->value.get());

        // ARIA-017: Check that passed references don't point to local variables
        // This prevents dangling references from escaping function scope
        checkReferenceEscape(stmt->value.get(), stmt);
    }
}

/**
 * ARIA-017: Check if a reference/borrow expression would escape its scope.
 * Called when returning or passing values that might be references to locals.
 *
 * Detects patterns like:
 *   func:get_ref = int32$() {
 *       int32:x = 10;
 *       pass($x);  // ERROR: Reference to local would escape
 *   }
 */
void BorrowChecker::checkReferenceEscape(ASTNode* value, ASTNode* context) {
    if (!value) return;

    // Check if this is a borrow expression ($x or !$x)
    if (value->type == ASTNode::NodeType::UNARY_OP) {
        UnaryExpr* unary = static_cast<UnaryExpr*>(value);

        // Detect borrow by token type or AST flag
        bool is_borrow = unary->creates_loan ||
                         unary->op.type == frontend::TokenType::TOKEN_DOLLAR;

        // Also check for negated borrow (!$x)
        if (!is_borrow && unary->op.type == frontend::TokenType::TOKEN_BANG &&
            unary->operand && unary->operand->type == ASTNode::NodeType::UNARY_OP) {
            UnaryExpr* inner = static_cast<UnaryExpr*>(unary->operand.get());
            is_borrow = inner->creates_loan ||
                        inner->op.type == frontend::TokenType::TOKEN_DOLLAR;
            if (is_borrow) {
                unary = inner;  // Use inner expression for target extraction
            }
        }

        if (is_borrow) {
            // Extract the borrowed variable name
            std::string target;
            if (!unary->loan_target.empty()) {
                target = unary->loan_target;
            } else if (unary->operand &&
                       unary->operand->type == ASTNode::NodeType::IDENTIFIER) {
                target = static_cast<IdentifierExpr*>(unary->operand.get())->name;
            }

            if (!target.empty()) {
                // Check if target is a local variable (exists in any scope)
                bool is_local = false;
                for (const auto& scope_vars : ctx.scope_stack) {
                    for (const auto& var : scope_vars) {
                        if (var == target) {
                            is_local = true;
                            break;
                        }
                    }
                    if (is_local) break;
                }

                if (is_local) {
                    addErrorWithSuggestion("Cannot return reference to local variable '" + target +
                             "'. The reference would become invalid when the function returns.",
                             context,
                             "hint: return the value by copy instead, or move it to the caller's scope");
                }
            }
        }
    }
}

/**
 * Helper: Check if a code block contains control flow structures that could
 * make execution paths ambiguous (e.g., if, while, for, pick).
 *
 * Used to detect when defer blocks have complex control flow that might
 * cause free() calls to not execute on all paths.
 */
static bool hasComplexControlFlow(ASTNode* node) {
    if (!node) return false;

    switch (node->type) {
        // Direct Control Flow Nodes - these create branching
        case ASTNode::NodeType::IF:
        case ASTNode::NodeType::WHILE:
        case ASTNode::NodeType::FOR:
        case ASTNode::NodeType::LOOP:
        case ASTNode::NodeType::TILL:
        case ASTNode::NodeType::WHEN:
        case ASTNode::NodeType::PICK:
        case ASTNode::NodeType::BREAK:
        case ASTNode::NodeType::CONTINUE:
        case ASTNode::NodeType::RETURN:  // Early return changes flow
            return true;

        // Container Nodes - Recurse deeply
        case ASTNode::NodeType::BLOCK: {
            BlockStmt* block = static_cast<BlockStmt*>(node);
            for (const auto& s : block->statements) {
                if (hasComplexControlFlow(s.get())) return true;
            }
            return false;
        }

        case ASTNode::NodeType::EXPRESSION_STMT: {
            // Check for ternary expressions which also branch
            ExpressionStmt* exprStmt = static_cast<ExpressionStmt*>(node);
            if (exprStmt->expression &&
                exprStmt->expression->type == ASTNode::NodeType::TERNARY) {
                return true;
            }
            return false;
        }

        default:
            return false;
    }
}

void BorrowChecker::checkDeferStmt(DeferStmt* stmt) {
    if (!stmt) return;

    // ARIA-014: Deep scan the defer block for free() calls
    // Handles: direct free(), wrapper functions, conditionals, method cleanup
    if (stmt->block) {
        // Step 1: Identify ALL variables that MIGHT be freed (May-Analysis)
        std::unordered_set<std::string> freed_vars =
            scanDeferBlockForFree(stmt->block.get(), stmt->line, false);

        // Step 2: Safety Analysis for Complex Control Flow
        // If there are freed variables and the block has complex control flow,
        // check if the frees are GUARANTEED on all paths
        if (!freed_vars.empty()) {
            if (hasComplexControlFlow(stmt->block.get())) {
                // We have complexity. Verify if frees are guaranteed (Must-Analysis)
                std::unordered_set<std::string> definitely_freed =
                    scanDeferBlockForFree(stmt->block.get(), stmt->line, true);

                // Check for discrepancies between May-Set and Must-Set
                // If a variable is in freed_vars but NOT in definitely_freed,
                // it means the free is conditional
                for (const auto& var : freed_vars) {
                    if (definitely_freed.find(var) == definitely_freed.end()) {
                        // Found a conditionally freed variable in a complex block
                        std::string warningMsg =
                            "Defer block contains conditional/loop - free of '" +
                            var + "' may not execute on all paths";

                        // Emit warning using proper severity
                        addWarning(warningMsg, stmt,
                            "hint: ensure the free() call is unconditional within the defer block");

                        // One warning per defer block is sufficient
                        break;
                    }
                }
            }
        }

        // Record the defer obligations (for leak detection at scope exit)
        for (const auto& var : freed_vars) {
            recordDeferFree(var, stmt->line);
        }
    }
}

// ============================================================================
// Deep AST Scanning for Defer Blocks
// ============================================================================

// Known deallocator function names
static const std::unordered_set<std::string> KNOWN_DEALLOCATORS = {
    "free", "aria.free", "aria_free",
    "deallocate", "dealloc",
    "release", "destroy",
    "close", "aria.close",
    "dispose", "cleanup",
    "arena_reset", "arena_destroy",
    "buffer_free", "string_free",
    "wildx_free",
    "drop"  // Wild pointer drop function
};

// Known cleanup method names (for x.method() style)
static const std::unordered_set<std::string> CLEANUP_METHODS = {
    "dispose", "close", "release", "destroy", "free", "cleanup", "drop"
};

bool BorrowChecker::isDeallocator(const std::string& funcName) const {
    // Check against known deallocators
    if (KNOWN_DEALLOCATORS.count(funcName) > 0) {
        return true;
    }

    // Check for common patterns in function names
    // Functions ending with "_free", "_close", "_destroy", "_release"
    if (funcName.length() > 5) {
        std::string suffix = funcName.substr(funcName.length() - 5);
        if (suffix == "_free") return true;
    }
    if (funcName.length() > 6) {
        std::string suffix = funcName.substr(funcName.length() - 6);
        if (suffix == "_close") return true;
    }
    if (funcName.length() > 8) {
        std::string suffix = funcName.substr(funcName.length() - 8);
        if (suffix == "_destroy" || suffix == "_release") return true;
    }

    return false;
}

std::string BorrowChecker::checkMethodCleanup(CallExpr* call) const {
    // Check for method-style cleanup: obj.dispose(), ptr.close(), etc.
    if (!call || !call->callee) return "";

    // Check for member access pattern: obj.method()
    if (call->callee->type == ASTNode::NodeType::MEMBER_ACCESS) {
        auto* memberAccess = static_cast<MemberAccessExpr*>(call->callee.get());
        if (memberAccess->object && memberAccess->object->type == ASTNode::NodeType::IDENTIFIER) {
            // Check if the method name is a known cleanup method
            if (CLEANUP_METHODS.count(memberAccess->member) > 0) {
                auto* obj = static_cast<IdentifierExpr*>(memberAccess->object.get());
                return obj->name;  // Return the object being cleaned up
            }
        }
    }

    return "";
}

std::unordered_set<std::string> BorrowChecker::scanDeferBlockForFree(ASTNode* node, int deferLine, bool must_free) {
    // Deep AST scanning for free() calls in defer blocks
    // Returns the set of variables that are freed in this subtree
    //
    // Handles:
    // - Block statements (recursive)
    // - Conditional statements (if/else)
    // - Loop statements (while/for) - conservative: assume may execute
    // - Direct free() calls
    // - Wrapper function calls
    // - Method-style cleanup (x.dispose())

    std::unordered_set<std::string> freed_vars;

    if (!node) return freed_vars;

    switch (node->type) {
        case ASTNode::NodeType::BLOCK: {
            // Recursively scan all statements in the block
            auto* block = static_cast<BlockStmt*>(node);
            for (const auto& s : block->statements) {
                auto sub_freed = scanDeferBlockForFree(s.get(), deferLine, must_free);
                freed_vars.insert(sub_freed.begin(), sub_freed.end());
            }
            break;
        }

        case ASTNode::NodeType::EXPRESSION_STMT: {
            // Unwrap expression statement
            auto* exprStmt = static_cast<ExpressionStmt*>(node);
            if (exprStmt->expression) {
                auto sub_freed = scanDeferBlockForFree(exprStmt->expression.get(), deferLine, must_free);
                freed_vars.insert(sub_freed.begin(), sub_freed.end());
            }
            break;
        }

        case ASTNode::NodeType::IF: {
            // Handle conditional free: if (cond) { free(x); }
            // For defer blocks, we use may-analysis: free on ANY path counts
            auto* ifStmt = static_cast<IfStmt*>(node);

            // Scan then branch
            std::unordered_set<std::string> then_freed;
            if (ifStmt->thenBranch) {
                then_freed = scanDeferBlockForFree(ifStmt->thenBranch.get(), deferLine, must_free);
            }

            // Scan else branch
            std::unordered_set<std::string> else_freed;
            if (ifStmt->elseBranch) {
                else_freed = scanDeferBlockForFree(ifStmt->elseBranch.get(), deferLine, must_free);
            }

            if (must_free) {
                // Must-analysis: only count variables freed on ALL paths
                // Intersection of then and else
                for (const auto& var : then_freed) {
                    if (else_freed.count(var) > 0 || !ifStmt->elseBranch) {
                        // Freed in both branches, or no else branch (then is the only path that frees)
                        freed_vars.insert(var);
                    }
                }
            } else {
                // May-analysis: count variables freed on ANY path
                // Union of then and else
                freed_vars.insert(then_freed.begin(), then_freed.end());
                freed_vars.insert(else_freed.begin(), else_freed.end());
            }
            break;
        }

        case ASTNode::NodeType::WHILE:
        case ASTNode::NodeType::FOR: {
            // Loops in defer blocks: conservative - assume the loop may execute
            // Scan the body for frees
            ASTNode* body = nullptr;
            if (node->type == ASTNode::NodeType::WHILE) {
                body = static_cast<WhileStmt*>(node)->body.get();
            } else {
                body = static_cast<ForStmt*>(node)->body.get();
            }
            if (body) {
                auto sub_freed = scanDeferBlockForFree(body, deferLine, must_free);
                freed_vars.insert(sub_freed.begin(), sub_freed.end());
            }
            break;
        }

        case ASTNode::NodeType::CALL: {
            auto* call = static_cast<CallExpr*>(node);

            // Check for method-style cleanup: obj.dispose()
            std::string method_obj = checkMethodCleanup(call);
            if (!method_obj.empty()) {
                freed_vars.insert(method_obj);
                recordDeferFree(method_obj, deferLine);
                break;
            }

            // Check for direct or wrapper function calls
            if (call->callee && call->callee->type == ASTNode::NodeType::IDENTIFIER) {
                auto* callee = static_cast<IdentifierExpr*>(call->callee.get());

                if (isDeallocator(callee->name)) {
                    // Found a deallocator call - extract the freed variable
                    // Only track if it's actually a wild-allocated variable
                    if (!call->arguments.empty()) {
                        ASTNode* arg = call->arguments[0].get();

                        // Handle direct variable: free(ptr)
                        if (arg->type == ASTNode::NodeType::IDENTIFIER) {
                            auto* argIdent = static_cast<IdentifierExpr*>(arg);
                            if (ctx.wild_states.find(argIdent->name) != ctx.wild_states.end()) {
                                freed_vars.insert(argIdent->name);
                                recordDeferFree(argIdent->name, deferLine);
                            }
                        }
                        // Handle member access: free(obj.ptr)
                        else if (arg->type == ASTNode::NodeType::MEMBER_ACCESS) {
                            auto* member = static_cast<MemberAccessExpr*>(arg);
                            if (member->object && member->object->type == ASTNode::NodeType::IDENTIFIER) {
                                auto* obj = static_cast<IdentifierExpr*>(member->object.get());
                                // Record the base object as having cleanup
                                freed_vars.insert(obj->name);
                                recordDeferFree(obj->name, deferLine);
                            }
                        }
                        // Handle unary dereference: free(*ptrToPtr) - record the pointer
                        else if (arg->type == ASTNode::NodeType::UNARY_OP) {
                            auto* unary = static_cast<UnaryExpr*>(arg);
                            if (unary->operand && unary->operand->type == ASTNode::NodeType::IDENTIFIER) {
                                auto* innerIdent = static_cast<IdentifierExpr*>(unary->operand.get());
                                freed_vars.insert(innerIdent->name);
                                recordDeferFree(innerIdent->name, deferLine);
                            }
                        }
                    }
                }
            }
            break;
        }

        default:
            // Other node types don't contain free calls
            break;
    }

    return freed_vars;
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
    
    // Check for use-after-move
    if (ctx.moved_variables.find(expr->name) != ctx.moved_variables.end()) {
        addErrorWithSuggestion("Use after move: variable '" + expr->name + "' was moved", expr,
                "hint: clone the value before moving if you need it afterward");
        return;
    }
    
    // Check for use-after-free on wild pointers
    checkWildUse(expr->name, expr);
}

void BorrowChecker::checkCallExpr(CallExpr* expr) {
    if (!expr) return;

    // ARIA-020: Track borrow targets in this call to detect aliasing
    // Maps variable name -> first argument index where it was borrowed
    std::unordered_map<std::string, size_t> borrow_targets;

    // Check all arguments
    for (size_t i = 0; i < expr->arguments.size(); ++i) {
        const auto& arg = expr->arguments[i];
        checkExpression(arg.get());

        // ARIA-016: Check if argument is a pinned variable being passed by value
        // Pinned variables cannot be moved (passed by value to functions)
        // Passing by value would potentially relocate or copy the pinned memory,
        // violating the address stability guarantee that pins provide
        if (arg && arg->type == ASTNode::NodeType::IDENTIFIER) {
            auto* ident = static_cast<IdentifierExpr*>(arg.get());
            if (isPinned(ident->name)) {
                addError("Cannot pass pinned variable '" + ident->name +
                         "' by value. Pinned objects must remain at a stable address. "
                         "Use a reference ($" + ident->name + ") instead.",
                         arg.get());
            }

            // ARIA-019: Check if argument is a borrowed variable being passed by value
            // Cannot move a variable while it has active borrows
            auto loans_it = ctx.active_loans.find(ident->name);
            if (loans_it != ctx.active_loans.end() && !loans_it->second.empty()) {
                const Loan& loan = loans_it->second.front();
                addError("Cannot move variable '" + ident->name +
                         "' because it is currently borrowed by '" + loan.borrower + "'",
                         arg.get(),
                         "Variable was borrowed here", loan.creation_line, loan.creation_column);
            }
        }

        // ARIA-020: Check for borrow expressions in arguments
        // Detects aliasing (same variable borrowed multiple times) and
        // reborrowing (borrowing while existing borrow exists)
        if (arg && arg->type == ASTNode::NodeType::UNARY_OP) {
            auto* unary = static_cast<UnaryExpr*>(arg.get());
            if (unary->creates_loan && !unary->loan_target.empty()) {
                const std::string& target = unary->loan_target;

                // ARIA-020a: Check for argument aliasing (Bug #6)
                // Same variable borrowed multiple times in same call
                auto existing = borrow_targets.find(target);
                if (existing != borrow_targets.end()) {
                    addError("Cannot borrow '" + target + "' multiple times in the same function call. "
                             "This would create aliasing references to the same variable.",
                             arg.get());
                } else {
                    borrow_targets[target] = i;
                }

                // ARIA-020b: Check for reborrow while existing borrow (Bug #7)
                // Cannot create new borrow if variable already has active loans
                auto loans_it = ctx.active_loans.find(target);
                if (loans_it != ctx.active_loans.end() && !loans_it->second.empty()) {
                    const Loan& loan = loans_it->second.front();
                    addError("Cannot borrow '" + target + "' because it is already borrowed by '" +
                             loan.borrower + "'",
                             arg.get(),
                             "Previous borrow was created here", loan.creation_line, loan.creation_column);
                }
            }
        }
    }

    // ARIA-022: Check for wild memory deallocation calls
    // If this is a free() call, record it for double-free and use-after-free detection
    // Only applies to wild-tracked variables (pointers), not plain value types like int32
    if (expr->callee && expr->callee->type == ASTNode::NodeType::IDENTIFIER) {
        auto* callee = static_cast<IdentifierExpr*>(expr->callee.get());
        if (isDeallocator(callee->name) && !expr->arguments.empty()) {
            ASTNode* arg = expr->arguments[0].get();
            if (arg && arg->type == ASTNode::NodeType::IDENTIFIER) {
                auto* argIdent = static_cast<IdentifierExpr*>(arg);
                // Only record deallocation if the variable is actually a wild pointer.
                // Non-wild variables (int32, string, etc.) passed to functions whose
                // names happen to end in _close/_free/_destroy are not deallocations.
                if (ctx.wild_states.find(argIdent->name) != ctx.wild_states.end()) {
                    recordWildFree(argIdent->name, expr);
                }
            }
        }
    }

    // TODO: Check function signature for ownership transfer
    // Some functions may take ownership (move) of arguments
    // This requires function signature analysis
}

void BorrowChecker::checkMoveExpr(MoveExpr* expr) {
    if (!expr) return;

    const std::string& varName = expr->variableName;

    // Check if the variable can be used (not already moved or freed)
    if (ctx.moved_variables.find(varName) != ctx.moved_variables.end()) {
        addErrorWithSuggestion("Cannot move variable '" + varName + "' (already moved)", expr,
                "hint: a variable can only be moved once — clone it before the first move if needed");
        return;
    }

    // ARIA-016: Check for active pins - cannot move pinned objects
    // Pinning locks the object's memory address for FFI/DMA stability
    if (isPinned(varName)) {
        addErrorWithSuggestion("Cannot move variable '" + varName +
                 "' because it is currently pinned. Pinned objects must remain "
                 "at a stable address while wild pointers reference them.", expr,
                 "hint: unpin the variable before moving, or work with a copy");
        return;
    }

    // Check for active borrows - cannot move borrowed objects
    auto loans_it = ctx.active_loans.find(varName);
    if (loans_it != ctx.active_loans.end() && !loans_it->second.empty()) {
        const Loan& loan = loans_it->second.front();
        addErrorWithSuggestion("Cannot move variable '" + varName +
                 "' because it is currently borrowed by '" + loan.borrower + "'", expr,
                 "Variable was borrowed here", loan.creation_line, loan.creation_column,
                 "hint: ensure all borrows of '" + varName + "' have ended before moving");
        return;
    }

    checkWildUse(varName, expr);
    
    // Mark the variable as moved (invalidate it for all types)
    ctx.moved_variables.insert(varName);
    
    // For wild pointers, also update the wild state
    auto it = ctx.wild_states.find(varName);
    if (it != ctx.wild_states.end()) {
        ctx.wild_states[varName] = WildState::MOVED;
        // Remove from pending frees since ownership transferred
        ctx.pending_wild_frees.erase(varName);
    }
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

void BorrowChecker::addErrorWithSuggestion(const std::string& message, ASTNode* node,
                                            const std::string& suggestion) {
    if (node) {
        errors.emplace_back(node->line, node->column, message, suggestion);
    } else {
        errors.emplace_back(0, 0, message, suggestion);
    }
}

void BorrowChecker::addErrorWithSuggestion(const std::string& message, ASTNode* node,
                                            const std::string& related_msg, int related_line, int related_col,
                                            const std::string& suggestion) {
    BorrowError err = node 
        ? BorrowError(node->line, node->column, message, related_line, related_col, related_msg)
        : BorrowError(0, 0, message, related_line, related_col, related_msg);
    err.suggestion = suggestion;
    errors.push_back(err);
}

void BorrowChecker::addWarning(const std::string& message, ASTNode* node) {
    BorrowError err = node 
        ? BorrowError(node->line, node->column, message)
        : BorrowError(0, 0, message);
    err.severity = BorrowSeverity::WARNING;
    errors.push_back(err);
}

void BorrowChecker::addWarning(const std::string& message, ASTNode* node,
                               const std::string& suggestion) {
    BorrowError err = node 
        ? BorrowError(node->line, node->column, message, suggestion)
        : BorrowError(0, 0, message, suggestion);
    err.severity = BorrowSeverity::WARNING;
    errors.push_back(err);
}

// ============================================================================
// TBB Value State Methods (Gemini Report: TBB ERR → Release Borrows)
// ============================================================================

void LifetimeContext::markTBBError(const std::string& var) {
    tbb_value_states[var] = TBBValueState::ERROR;
    // When a TBB variable enters ERR state, release loans that originated from it
    releaseLoansForTBBError(var);
}

void LifetimeContext::markTBBValid(const std::string& var) {
    tbb_value_states[var] = TBBValueState::VALID;
}

bool LifetimeContext::isTBBError(const std::string& var) const {
    auto it = tbb_value_states.find(var);
    return it != tbb_value_states.end() && it->second == TBBValueState::ERROR;
}

void LifetimeContext::releaseLoansForTBBError(const std::string& var) {
    // Release all loans where the host is the ERR variable
    // This enables error recovery patterns like:
    //   let result = torus.compute();
    //   if result == ERR { torus.reset(); }  // Allowed: ERR releases borrow

    // Remove from active_loans where this var is the host
    active_loans.erase(var);

    // Also remove from path_loans if paths start with this variable
    std::vector<AccessPath> paths_to_remove;
    for (const auto& [path, loans] : path_loans) {
        if (path.base_var == var) {
            paths_to_remove.push_back(path);
        }
    }
    for (const auto& path : paths_to_remove) {
        path_loans.erase(path);
    }

    // Remove from loan_origins where this var is an origin
    for (auto& [ref, origins] : loan_origins) {
        origins.erase(var);
    }
}

// ============================================================================
// Split Borrow Methods (Gemini Report: Access Path Tracking)
// ============================================================================

const Loan* LifetimeContext::checkPathConflict(const AccessPath& path, bool is_mutable) const {
    // Check if borrowing 'path' would conflict with existing loans
    // Uses disjointness analysis to allow split borrows

    // First check path_loans for precise path-based analysis
    for (const auto& [existing_path, loans] : path_loans) {
        // Skip if paths are disjoint (e.g., x.a vs x.b)
        if (path.isDisjointFrom(existing_path)) {
            continue;
        }

        // Paths overlap - check for borrow conflicts
        for (const auto& loan : loans) {
            // Reserved loans only block mutable borrows
            if (loan.phase == LoanPhase::RESERVED) {
                if (is_mutable) {
                    return &loan;
                }
                continue;
            }

            // Active loans: mutable blocks all, immutable blocks mutable
            if (loan.is_mutable || is_mutable) {
                return &loan;
            }
        }
    }

    // Also check legacy active_loans for base variable conflicts
    auto it = active_loans.find(path.base_var);
    if (it != active_loans.end()) {
        for (const auto& loan : it->second) {
            // If existing loan has no path info, assume it conflicts
            if (loan.path.fields.empty() && loan.path.base_var == path.base_var) {
                if (loan.phase == LoanPhase::RESERVED && !is_mutable) {
                    continue;  // Reserved allows shared reads
                }
                if (loan.is_mutable || is_mutable) {
                    return &loan;
                }
            }
        }
    }

    return nullptr;  // No conflict
}

void LifetimeContext::recordPathLoan(const AccessPath& path, const Loan& loan) {
    path_loans[path].push_back(loan);

    // Also record in legacy active_loans for backwards compatibility
    active_loans[path.base_var].push_back(loan);
}

void LifetimeContext::releasePathLoans(const AccessPath& path) {
    // Remove exact path matches
    path_loans.erase(path);

    // Also remove from active_loans where borrower matches
    auto it = active_loans.find(path.base_var);
    if (it != active_loans.end()) {
        it->second.erase(
            std::remove_if(it->second.begin(), it->second.end(),
                [&path](const Loan& loan) { return loan.path == path; }),
            it->second.end()
        );
    }
}

// ============================================================================
// Liveness Analysis (Gemini Report: Loop-Carried Dependencies)
// ============================================================================

LivenessAnalysis BorrowChecker::computeLoopLiveness(ASTNode* loopBody, ASTNode* loopCondition) {
    LivenessAnalysis liveness;

    // Collect variables used in the loop condition
    // These are definitely live at the loop header
    if (loopCondition) {
        collectUsedVariables(loopCondition, liveness.live_at_header);
    }

    // Collect variables used in the loop body
    // A variable is live at the header if it's used before being redefined
    if (loopBody) {
        collectUsedVariables(loopBody, liveness.live_at_header);
    }

    return liveness;
}

void BorrowChecker::collectUsedVariables(ASTNode* expr, std::unordered_set<std::string>& used) {
    if (!expr) return;

    switch (expr->type) {
        case ASTNode::NodeType::IDENTIFIER: {
            auto* ident = static_cast<IdentifierExpr*>(expr);
            used.insert(ident->name);
            break;
        }

        case ASTNode::NodeType::BINARY_OP: {
            auto* binary = static_cast<BinaryExpr*>(expr);
            collectUsedVariables(binary->left.get(), used);
            collectUsedVariables(binary->right.get(), used);
            break;
        }

        case ASTNode::NodeType::UNARY_OP: {
            auto* unary = static_cast<UnaryExpr*>(expr);
            collectUsedVariables(unary->operand.get(), used);
            break;
        }

        case ASTNode::NodeType::CALL: {
            auto* call = static_cast<CallExpr*>(expr);
            collectUsedVariables(call->callee.get(), used);
            for (const auto& arg : call->arguments) {
                collectUsedVariables(arg.get(), used);
            }
            break;
        }

        case ASTNode::NodeType::MEMBER_ACCESS: {
            auto* member = static_cast<MemberAccessExpr*>(expr);
            collectUsedVariables(member->object.get(), used);
            break;
        }

        case ASTNode::NodeType::INDEX: {
            auto* index = static_cast<IndexExpr*>(expr);
            collectUsedVariables(index->array.get(), used);
            collectUsedVariables(index->index.get(), used);
            break;
        }

        case ASTNode::NodeType::BLOCK: {
            auto* block = static_cast<BlockStmt*>(expr);
            for (const auto& s : block->statements) {
                collectUsedVariables(s.get(), used);
            }
            break;
        }

        case ASTNode::NodeType::VAR_DECL: {
            auto* varDecl = static_cast<VarDeclStmt*>(expr);
            if (varDecl->initializer) {
                collectUsedVariables(varDecl->initializer.get(), used);
            }
            break;
        }

        case ASTNode::NodeType::EXPRESSION_STMT: {
            auto* exprStmt = static_cast<ExpressionStmt*>(expr);
            collectUsedVariables(exprStmt->expression.get(), used);
            break;
        }

        case ASTNode::NodeType::IF: {
            auto* ifStmt = static_cast<IfStmt*>(expr);
            collectUsedVariables(ifStmt->condition.get(), used);
            collectUsedVariables(ifStmt->thenBranch.get(), used);
            if (ifStmt->elseBranch) {
                collectUsedVariables(ifStmt->elseBranch.get(), used);
            }
            break;
        }

        case ASTNode::NodeType::WHILE: {
            auto* whileStmt = static_cast<WhileStmt*>(expr);
            collectUsedVariables(whileStmt->condition.get(), used);
            collectUsedVariables(whileStmt->body.get(), used);
            break;
        }

        case ASTNode::NodeType::FOR: {
            auto* forStmt = static_cast<ForStmt*>(expr);
            collectUsedVariables(forStmt->initializer.get(), used);
            collectUsedVariables(forStmt->condition.get(), used);
            collectUsedVariables(forStmt->update.get(), used);
            collectUsedVariables(forStmt->body.get(), used);
            break;
        }

        case ASTNode::NodeType::RETURN: {
            auto* ret = static_cast<ReturnStmt*>(expr);
            collectUsedVariables(ret->value.get(), used);
            break;
        }

        default:
            // Other node types don't use variables directly
            break;
    }
}

// ============================================================================
// Two-Phase Borrowing (Gemini Report: Method Call Patterns)
// ============================================================================

Loan BorrowChecker::createReservedLoan(const std::string& host, const std::string& reference, ASTNode* node) {
    // Create a loan in RESERVED phase - acts as shared borrow during argument evaluation
    // This enables patterns like vec.push(vec.len())

    AccessPath path(host);
    Loan loan(reference, true /* is_mutable */, node->line, node->column, path, LoanPhase::RESERVED);

    // Check for conflicts with existing loans
    // Reserved loans only conflict with active mutable borrows
    const Loan* conflict = ctx.checkPathConflict(path, false);  // Check as shared
    if (conflict && conflict->blocksMutableAccess()) {
        addError("Cannot create reserved borrow of '" + host +
                 "' - already mutably borrowed by '" + conflict->borrower + "'",
                 node, "Previous mutable borrow here",
                 conflict->creation_line, conflict->creation_column);
    }

    // Record the reserved loan
    ctx.recordPathLoan(path, loan);
    ctx.loan_origins[reference].insert(host);

    return loan;
}

bool BorrowChecker::activateLoan(Loan& loan, ASTNode* node) {
    // Activate a reserved loan to full mutable
    // Called after method arguments are evaluated

    if (loan.phase == LoanPhase::ACTIVE) {
        return true;  // Already active
    }

    // Check for conflicts - now we need exclusive access
    const Loan* conflict = ctx.checkPathConflict(loan.path, true);  // Check as mutable
    if (conflict && conflict != &loan) {
        addError("Cannot activate mutable borrow of '" + loan.path.toString() +
                 "' - conflicts with borrow by '" + conflict->borrower + "'",
                 node, "Conflicting borrow here",
                 conflict->creation_line, conflict->creation_column);
        return false;
    }

    // Update phase in path_loans
    auto it = ctx.path_loans.find(loan.path);
    if (it != ctx.path_loans.end()) {
        for (auto& existing : it->second) {
            if (existing.borrower == loan.borrower &&
                existing.creation_line == loan.creation_line) {
                existing.phase = LoanPhase::ACTIVE;
                break;
            }
        }
    }

    // Update the loan parameter
    loan.phase = LoanPhase::ACTIVE;
    return true;
}

// ============================================================================
// TBB Error Detection (Gemini Report)
// ============================================================================

std::string BorrowChecker::detectTBBErrorCheck(ASTNode* condition) {
    // Detect patterns that check for TBB ERR state:
    //   x == ERR
    //   x == TBB8_ERR
    //   x.is_err()
    //   !x.is_valid()

    if (!condition) return "";

    if (condition->type == ASTNode::NodeType::BINARY_OP) {
        auto* binary = static_cast<BinaryExpr*>(condition);

        // Check for x == ERR or x == TBB*_ERR patterns
        if (binary->op.type == frontend::TokenType::TOKEN_EQUAL_EQUAL) {
            // Check left == ERR
            if (binary->left->type == ASTNode::NodeType::IDENTIFIER &&
                binary->right->type == ASTNode::NodeType::IDENTIFIER) {
                auto* left = static_cast<IdentifierExpr*>(binary->left.get());
                auto* right = static_cast<IdentifierExpr*>(binary->right.get());

                // Check if right side is an ERR constant
                if (right->name == "ERR" ||
                    right->name.find("_ERR") != std::string::npos ||
                    (right->name.find("TBB") != std::string::npos &&
                     right->name.find("ERR") != std::string::npos)) {
                    return left->name;
                }

                // Also check left side for ERR
                if (left->name == "ERR" ||
                    left->name.find("_ERR") != std::string::npos) {
                    return right->name;
                }
            }
        }
    }
    else if (condition->type == ASTNode::NodeType::CALL) {
        auto* call = static_cast<CallExpr*>(condition);

        // Check for x.is_err() pattern
        if (call->callee && call->callee->type == ASTNode::NodeType::MEMBER_ACCESS) {
            auto* member = static_cast<MemberAccessExpr*>(call->callee.get());
            if (member->member == "is_err" || member->member == "is_error") {
                if (member->object && member->object->type == ASTNode::NodeType::IDENTIFIER) {
                    return static_cast<IdentifierExpr*>(member->object.get())->name;
                }
            }
        }
    }
    else if (condition->type == ASTNode::NodeType::UNARY_OP) {
        auto* unary = static_cast<UnaryExpr*>(condition);

        // Check for !x.is_valid() pattern
        if (unary->op.type == frontend::TokenType::TOKEN_BANG) {
            if (unary->operand && unary->operand->type == ASTNode::NodeType::CALL) {
                auto* call = static_cast<CallExpr*>(unary->operand.get());
                if (call->callee && call->callee->type == ASTNode::NodeType::MEMBER_ACCESS) {
                    auto* member = static_cast<MemberAccessExpr*>(call->callee.get());
                    if (member->member == "is_valid" || member->member == "is_ok") {
                        if (member->object && member->object->type == ASTNode::NodeType::IDENTIFIER) {
                            return static_cast<IdentifierExpr*>(member->object.get())->name;
                        }
                    }
                }
            }
        }
    }

    return "";
}

bool BorrowChecker::isTBBType(const std::string& typeName) const {
    // Check if the type name indicates a TBB (Ternary Balanced Binary) type
    // TBB types: tbb8, tbb16, tbb32, tbb64, TBB8, TBB16, etc.

    std::string lower = typeName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    return lower.find("tbb") != std::string::npos ||
           lower.find("ternary") != std::string::npos ||
           lower.find("balanced") != std::string::npos;
}

// ============================================================================
// Access Path Extraction (Gemini Report: Split Borrows)
// ============================================================================

AccessPath BorrowChecker::extractAccessPath(ASTNode* expr) {
    // Extract the full access path from an expression
    // Handles: identifier, member access, array subscript

    if (!expr) return AccessPath();

    switch (expr->type) {
        case ASTNode::NodeType::IDENTIFIER: {
            auto* ident = static_cast<IdentifierExpr*>(expr);
            return AccessPath(ident->name);
        }

        case ASTNode::NodeType::MEMBER_ACCESS: {
            auto* member = static_cast<MemberAccessExpr*>(expr);
            AccessPath base = extractAccessPath(member->object.get());
            base.fields.push_back(member->member);
            return base;
        }

        case ASTNode::NodeType::INDEX: {
            auto* index = static_cast<IndexExpr*>(expr);
            AccessPath base = extractAccessPath(index->array.get());

            // Try to extract constant index
            if (index->index && index->index->type == ASTNode::NodeType::LITERAL) {
                auto* lit = static_cast<LiteralExpr*>(index->index.get());
                // Try to extract integer value from the variant
                if (std::holds_alternative<int64_t>(lit->value)) {
                    base.fields.push_back("[" + std::to_string(std::get<int64_t>(lit->value)) + "]");
                } else {
                    // Non-integer literal - use placeholder
                    base.fields.push_back("[*]");
                }
            } else {
                // Non-constant index - use placeholder
                base.fields.push_back("[*]");
            }
            return base;
        }

        case ASTNode::NodeType::UNARY_OP: {
            // Handle dereference and borrow operators
            auto* unary = static_cast<UnaryExpr*>(expr);
            if (unary->op.type == frontend::TokenType::TOKEN_DOLLAR ||
                unary->op.type == frontend::TokenType::TOKEN_STAR ||
                unary->op.type == frontend::TokenType::TOKEN_AT) {
                return extractAccessPath(unary->operand.get());
            }
            break;
        }

        default:
            break;
    }

    return AccessPath();  // Empty path for unrecognized expressions
}

void BorrowChecker::recordBorrowWithPath(const AccessPath& path, const std::string& reference,
                                          bool is_mutable, ASTNode* node) {
    // Record a borrow with full access path tracking
    // This enables split borrowing of disjoint fields

    if (path.base_var.empty()) {
        addError("Cannot borrow from invalid access path", node);
        return;
    }

    // Validate lifetime
    if (!validateLifetime(path.base_var, reference, node)) {
        return;
    }

    // Check borrow rules with path awareness
    if (!checkBorrowRulesWithPath(path, is_mutable, node)) {
        return;
    }

    // Create and record the loan
    Loan loan(reference, is_mutable, node->line, node->column, path);
    ctx.recordPathLoan(path, loan);
    ctx.loan_origins[reference].insert(path.base_var);
}

bool BorrowChecker::checkBorrowRulesWithPath(const AccessPath& path, bool is_mutable, ASTNode* node) {
    // Check borrow rules with access path awareness
    // Allows disjoint paths to be borrowed simultaneously

    // Check for pinning on the base variable
    if (isPinned(path.base_var)) {
        if (is_mutable) {
            addError("Cannot borrow pinned variable '" + path.base_var + "' as mutable", node);
            return false;
        }
    }

    // Check for conflicts using path-based analysis
    const Loan* conflict = ctx.checkPathConflict(path, is_mutable);
    if (conflict) {
        if (is_mutable) {
            addError("Cannot borrow '" + path.toString() + "' as mutable because it conflicts with existing borrow",
                    node,
                    "Existing " + std::string(conflict->is_mutable ? "mutable" : "immutable") +
                    " borrow by '" + conflict->borrower + "' of '" + conflict->path.toString() + "'",
                    conflict->creation_line, conflict->creation_column);
        } else {
            addError("Cannot borrow '" + path.toString() + "' as immutable because it is already borrowed as mutable",
                    node,
                    "Mutable borrow by '" + conflict->borrower + "' of '" + conflict->path.toString() + "'",
                    conflict->creation_line, conflict->creation_column);
        }
        return false;
    }

    return true;
}

} // namespace sema
} // namespace aria

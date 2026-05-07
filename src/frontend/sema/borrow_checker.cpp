#include "frontend/sema/borrow_checker.h"
#include <sstream>
#include <algorithm>
#include <iostream>

namespace npk {
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
    snap.path_loans = this->path_loans;
    snap.active_pins = this->active_pins;
    snap.pin_derived_aliases = this->pin_derived_aliases;
    snap.pointer_vars = this->pointer_vars;
    snap.pending_wild_frees = this->pending_wild_frees;
    snap.wild_states = this->wild_states;
    snap.wild_alloc_sizes = this->wild_alloc_sizes;
    snap.moved_variables = this->moved_variables;
    snap.current_depth = this->current_depth;
    snap.scope_stack = this->scope_stack;
    return snap;
}

void LifetimeContext::restore(const LifetimeContext& snap) {
    this->var_depths = snap.var_depths;
    this->loan_origins = snap.loan_origins;
    this->active_loans = snap.active_loans;
    this->path_loans = snap.path_loans;
    this->active_pins = snap.active_pins;
    this->pin_derived_aliases = snap.pin_derived_aliases;
    this->pointer_vars = snap.pointer_vars;
    this->pending_wild_frees = snap.pending_wild_frees;
    this->wild_states = snap.wild_states;
    this->wild_alloc_sizes = snap.wild_alloc_sizes;
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
    // This is critical for conditional borrow merge: if branches create
    // different $$i/$$m aliases, all possible hosts remain conservatively
    // borrowed after the merge.
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

    auto merge_path_loans = [this](const auto& source_path_loans) {
        for (const auto& [path, loans] : source_path_loans) {
            auto& our_loans = path_loans[path];
            for (const auto& loan : loans) {
                bool found = false;
                for (const auto& existing : our_loans) {
                    if (existing.borrower == loan.borrower &&
                        existing.is_mutable == loan.is_mutable &&
                        existing.creation_line == loan.creation_line &&
                        existing.creation_column == loan.creation_column &&
                        existing.path == loan.path &&
                        existing.phase == loan.phase) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    our_loans.push_back(loan);
                }
            }
        }
    };
    merge_path_loans(then_state.path_loans);
    merge_path_loans(else_state.path_loans);

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

    // Merge pin-derived aliases conservatively: if an alias is pin-derived on
    // either branch, retain the read-only pin provenance after the merge.
    for (const auto& [alias, info] : then_state.pin_derived_aliases) {
        pin_derived_aliases.emplace(alias, info);
    }
    for (const auto& [alias, info] : else_state.pin_derived_aliases) {
        pin_derived_aliases.emplace(alias, info);
    }

    pointer_vars.insert(then_state.pointer_vars.begin(), then_state.pointer_vars.end());
    pointer_vars.insert(else_state.pointer_vars.begin(), else_state.pointer_vars.end());
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

    // Merge path-sensitive loans with the same liveness filter used for the
    // legacy active_loans map. These power split borrows such as p.a vs p.b.
    for (const auto& [path, loans] : back_edge_state.path_loans) {
        if (!liveness.isLiveAtHeader(path.base_var)) {
            continue;
        }

        auto& our_loans = path_loans[path];
        for (const auto& loan : loans) {
            if (!liveness.isLiveAtHeader(loan.borrower)) {
                continue;
            }

            bool found = false;
            for (const auto& existing : our_loans) {
                if (existing.borrower == loan.borrower &&
                    existing.is_mutable == loan.is_mutable &&
                    existing.creation_line == loan.creation_line &&
                    existing.creation_column == loan.creation_column &&
                    existing.path == loan.path &&
                    existing.phase == loan.phase) {
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

    // Merge pin-derived aliases using the same liveness filter as loan origins.
    for (const auto& [alias, info] : back_edge_state.pin_derived_aliases) {
        if (!liveness.isLiveAtHeader(alias)) {
            continue;
        }
        auto [_, inserted] = pin_derived_aliases.emplace(alias, info);
        if (inserted) {
            changed = true;
        }
    }

    for (const auto& var : back_edge_state.pointer_vars) {
        if (liveness.isLiveAtHeader(var) && pointer_vars.insert(var).second) {
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
    if (!(a.path == b.path)) return false;
    if (a.phase != b.phase) return false;

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
    // 2b. Compare Path-Sensitive Loans
    // ==========================================================
    if (path_loans.size() != other.path_loans.size()) return false;
    for (const auto& [path, my_loans] : path_loans) {
        auto it = other.path_loans.find(path);
        if (it == other.path_loans.end()) return false;

        const auto& other_loans = it->second;
        if (my_loans.size() != other_loans.size()) return false;

        for (size_t i = 0; i < my_loans.size(); ++i) {
            if (!are_loans_equal(my_loans[i], other_loans[i])) {
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
    if (pin_derived_aliases != other.pin_derived_aliases) return false;
    if (pointer_vars != other.pointer_vars) return false;

    // ==========================================================
    // 5. Compare Pending Frees (Leak Detection)
    // ==========================================================
    if (pending_wild_frees != other.pending_wild_frees) return false;

    // ==========================================================
    // 6. Compare Moved Variables (Use-After-Move)
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
    func_summaries.clear();
    current_function.clear();
    trait_methods.clear();
    type_traits.clear();
    copyable_types.clear();
    droppable_types.clear();
    
    // Phase 1: Collect function borrow summaries (signatures only)
    collectFunctionSummaries(ast);
    
    // Phase 2: Full borrow analysis with inter-procedural awareness
    checkStatement(ast);
    
    return errors;
}

// ============================================================================
// Inter-Procedural Analysis (v0.6.0)
// ============================================================================

void BorrowChecker::collectFunctionSummaries(ASTNode* ast) {
    if (!ast) return;
    
    if (ast->type == ASTNode::NodeType::PROGRAM) {
        auto* program = static_cast<ProgramNode*>(ast);
        for (const auto& decl : program->declarations) {
            if (decl && decl->type == ASTNode::NodeType::FUNC_DECL) {
                auto* func = static_cast<FuncDeclStmt*>(decl.get());
                FunctionBorrowSummary summary = buildSummary(func);
                func_summaries[summary.func_name] = std::move(summary);
            }
            // v0.6.2: Collect trait method signatures and impl method summaries
            else if (decl && decl->type == ASTNode::NodeType::TRAIT_DECL) {
                auto* traitDecl = static_cast<TraitDeclStmt*>(decl.get());
                trait_methods[traitDecl->traitName] = traitDecl->methods;
            }
            else if (decl && decl->type == ASTNode::NodeType::IMPL_DECL) {
                auto* implDecl = static_cast<ImplDeclStmt*>(decl.get());
                // Register trait implementation
                type_traits[implDecl->typeName].insert(implDecl->traitName);
                // Track Copyable/Droppable types
                if (implDecl->traitName == "Copyable") {
                    copyable_types.insert(implDecl->typeName);
                } else if (implDecl->traitName == "Droppable") {
                    droppable_types.insert(implDecl->typeName);
                }
                // Collect summaries for each impl method (mangled name: Type_method)
                for (const auto& methodNode : implDecl->methods) {
                    if (methodNode && methodNode->type == ASTNode::NodeType::FUNC_DECL) {
                        auto* func = static_cast<FuncDeclStmt*>(methodNode.get());
                        FunctionBorrowSummary summary = buildSummary(func);
                        // Mangle the name: TypeName_methodName
                        std::string mangledName = implDecl->typeName + "_" + func->funcName;
                        summary.func_name = mangledName;
                        summary.is_trait_method = true;
                        summary.impl_type = implDecl->typeName;
                        summary.trait_name = implDecl->traitName;
                        // Identify self parameter
                        for (size_t i = 0; i < summary.param_names.size(); ++i) {
                            if (summary.param_names[i] == "self") {
                                summary.self_param_index = static_cast<int>(i);
                                summary.self_ownership = summary.param_ownership[i];
                                break;
                            }
                        }
                        func_summaries[mangledName] = std::move(summary);
                    }
                }
            }
        }
    } else if (ast->type == ASTNode::NodeType::FUNC_DECL) {
        auto* func = static_cast<FuncDeclStmt*>(ast);
        FunctionBorrowSummary summary = buildSummary(func);
        func_summaries[summary.func_name] = std::move(summary);
    }
}

FunctionBorrowSummary BorrowChecker::buildSummary(FuncDeclStmt* func) {
    FunctionBorrowSummary summary;
    summary.func_name = func->funcName;
    summary.decl_line = func->line;
    summary.decl_column = func->column;
    summary.returns_borrow_imm = func->returnIsBorrowImm;
    summary.returns_borrow_mut = func->returnIsBorrowMut;
    summary.returns_wild = func->returnIsWild;
    
    for (const auto& param : func->parameters) {
        if (!param || param->type != ASTNode::NodeType::PARAMETER) continue;
        auto* p = static_cast<ParameterNode*>(param.get());
        
        ParamOwnership ownership = ParamOwnership::COPY;
        if (p->isBorrowImm) {
            ownership = ParamOwnership::BORROW_IMM;
        } else if (p->isBorrowMut) {
            ownership = ParamOwnership::BORROW_MUT;
        } else if (p->isWild && !func->isExtern) {
            // Wild pointer passed by value = copy (raw pointers are copyable)
            // Lifecycle tracking (alloc/free) still handled via wild_states
            ownership = ParamOwnership::COPY;
        }
        
        summary.param_ownership.push_back(ownership);
        summary.param_names.push_back(p->paramName);
    }
    
    return summary;
}

void BorrowChecker::registerFunctionParams(FuncDeclStmt* func) {
    for (const auto& param : func->parameters) {
        if (!param || param->type != ASTNode::NodeType::PARAMETER) continue;
        auto* p = static_cast<ParameterNode*>(param.get());
        
        // Register the parameter variable in the current scope
        registerVariable(p->paramName, param.get());
        
        // If the parameter is a borrow, record an active loan
        // The "host" is an implicit caller variable (we use a synthetic name)
        if (p->isBorrowImm || p->isBorrowMut) {
            std::string synthetic_host = "__caller_" + p->paramName;
            ctx.var_depths[synthetic_host] = ctx.current_depth - 1;  // Caller scope
            recordBorrow(synthetic_host, p->paramName, p->isBorrowMut, param.get());
        }
        
        // If the parameter is wild, track it
        if (p->isWild || p->isWildx) {
            ctx.wild_states[p->paramName] = WildState::ALLOCATED;
        }
    }
}

void BorrowChecker::checkCallOwnership(CallExpr* expr, const FunctionBorrowSummary& summary) {
    size_t num_params = summary.param_ownership.size();
    std::unordered_map<std::string, size_t> mutable_borrow_targets;
    
    for (size_t i = 0; i < expr->arguments.size() && i < num_params; ++i) {
        const auto& arg = expr->arguments[i];
        if (!arg) continue;
        
        ParamOwnership expected = summary.param_ownership[i];
        const std::string& param_name = summary.param_names[i];
        
        // Extract the argument variable name if it's an identifier
        std::string arg_var;
        if (arg->type == ASTNode::NodeType::IDENTIFIER) {
            arg_var = static_cast<IdentifierExpr*>(arg.get())->name;
        }
        
        switch (expected) {
            case ParamOwnership::MOVE: {
                // Callee takes ownership — argument must be movable
                if (!arg_var.empty()) {
                    // Check not already moved
                    if (ctx.moved_variables.count(arg_var)) {
                        addErrorWithSuggestion(
                            "Cannot pass '" + arg_var + "' to '" + summary.func_name +
                            "' parameter '" + param_name + "' (already moved)",
                            arg.get(),
                            "hint: a variable can only be moved once — clone it before the first move if needed");
                        tagCode("ARIA-019");
                        break;
                    }
                    // Check not borrowed
                    auto loans_it = ctx.active_loans.find(arg_var);
                    if (loans_it != ctx.active_loans.end() && !loans_it->second.empty()) {
                        const Loan& loan = loans_it->second.front();
                        addError("Cannot move '" + arg_var + "' into '" + summary.func_name +
                                 "' parameter '" + param_name + "' — it is currently borrowed by '" +
                                 loan.borrower + "'",
                                 arg.get(),
                                 "Variable was borrowed here", loan.creation_line, loan.creation_column);
                        tagCode("ARIA-019");
                        break;
                    }
                    // Mark as moved
                    ctx.moved_variables.insert(arg_var);
                    auto wit = ctx.wild_states.find(arg_var);
                    if (wit != ctx.wild_states.end()) {
                        ctx.wild_states[arg_var] = WildState::MOVED;
                        ctx.pending_wild_frees.erase(arg_var);
                    }
                }
                break;
            }
                
            case ParamOwnership::BORROW_MUT: {
                // Callee expects $$m. The parameter declaration carries the
                // mutable-borrow contract; the call site must pass a normal,
                // addressable identifier. Dollar-prefixed borrow expressions are not Nitpick syntax.
                if (arg->type == ASTNode::NodeType::UNARY_OP) {
                    addErrorWithSuggestion(
                        "Function '" + summary.func_name + "' parameter '" +
                        param_name + "' expects a mutable borrow ($$m), but dollar-borrow syntax is not Nitpick syntax",
                        arg.get(),
                        "hint: pass the addressable variable directly; keep $$m on the parameter declaration");
                    tagCode("ARIA-020");
                } else if (arg_var.empty()) {
                    addErrorWithSuggestion(
                        "Function '" + summary.func_name + "' parameter '" +
                        param_name + "' expects a mutable borrow ($$m), but the argument is not an addressable variable",
                        arg.get(),
                        "hint: pass a local variable name directly");
                    tagCode("ARIA-020");
                } else {
                    auto existing = mutable_borrow_targets.find(arg_var);
                    if (existing != mutable_borrow_targets.end()) {
                        addError("Cannot mutably borrow '" + arg_var +
                                 "' multiple times in the same function call",
                                 arg.get());
                        tagCode("ARIA-020");
                        break;
                    }
                    mutable_borrow_targets[arg_var] = i;

                    if (ctx.moved_variables.count(arg_var)) {
                        addErrorWithSuggestion(
                            "Cannot mutably borrow '" + arg_var + "' for '" + summary.func_name +
                            "' parameter '" + param_name + "' because it was already moved",
                            arg.get(),
                            "hint: clone the value before moving if you need to borrow it later");
                        tagCode("ARIA-019");
                        break;
                    }

                    auto loans_it = ctx.active_loans.find(arg_var);
                    if (loans_it != ctx.active_loans.end() && !loans_it->second.empty()) {
                        const Loan& loan = loans_it->second.front();
                        addError("Cannot mutably borrow '" + arg_var + "' for '" + summary.func_name +
                                 "' parameter '" + param_name + "' because it is already borrowed by '" +
                                 loan.borrower + "'",
                                 arg.get(),
                                 "Existing borrow was created here", loan.creation_line, loan.creation_column);
                        tagCode("ARIA-023");
                    }
                }
                break;
            }
                
            case ParamOwnership::BORROW_IMM: {
                // Callee expects $$i. As with $$m, the parameter declaration
                // carries the borrow contract; dollar-borrow expressions are
                // intentionally rejected as non-Aria syntax.
                if (arg->type == ASTNode::NodeType::UNARY_OP) {
                    addErrorWithSuggestion(
                        "Function '" + summary.func_name + "' parameter '" +
                        param_name + "' expects immutable borrow ($$i), but dollar-borrow syntax is not Nitpick syntax",
                        arg.get(),
                        "hint: pass the value directly; keep $$i on the parameter declaration");
                    tagCode("ARIA-020");
                }
                break;
            }
                
            case ParamOwnership::COPY:
                // Default — no special ownership requirements
                break;
        }
    }
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
        tagCode("ARIA-017");
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
            tagCode("ARIA-016");
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
        tagCode("ARIA-023");
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
                tagCode("ARIA-023");
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

    for (auto it = ctx.path_loans.begin(); it != ctx.path_loans.end(); ) {
        auto& loans = it->second;
        loans.erase(
            std::remove_if(loans.begin(), loans.end(),
                [&var](const Loan& loan) { return loan.borrower == var; }),
            loans.end()
        );
        if (loans.empty()) {
            it = ctx.path_loans.erase(it);
        } else {
            ++it;
        }
    }
    
    // Remove from loan origins
    ctx.loan_origins.erase(var);
    ctx.pin_derived_aliases.erase(var);
    ctx.pointer_vars.erase(var);
    
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

bool BorrowChecker::findPinnedRootedPath(ASTNode* node,
                                         std::string& pin_ref,
                                         std::string& host,
                                         std::string& path) const {
    if (!node) return false;

    if (node->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* pointer = static_cast<IdentifierExpr*>(node);

        auto aliasIt = ctx.pin_derived_aliases.find(pointer->name);
        if (aliasIt != ctx.pin_derived_aliases.end()) {
            pin_ref = aliasIt->second.pin_ref;
            host = aliasIt->second.host;
            path = pointer->name + " (derived from " + aliasIt->second.source_path + ")";
            return true;
        }

        auto originsIt = ctx.loan_origins.find(pointer->name);
        if (originsIt != ctx.loan_origins.end()) {
            for (const auto& origin : originsIt->second) {
                auto pinIt = ctx.active_pins.find(origin);
                if (pinIt != ctx.active_pins.end() && pinIt->second == pointer->name) {
                    pin_ref = pointer->name;
                    host = origin;
                    path = pointer->name;
                    return true;
                }
            }
        }

        return false;
    }

    if (node->type == ASTNode::NodeType::POINTER_MEMBER ||
        node->type == ASTNode::NodeType::MEMBER_ACCESS) {
        MemberAccessExpr* member = static_cast<MemberAccessExpr*>(node);
        if (findPinnedRootedPath(member->object.get(), pin_ref, host, path)) {
            path += (node->type == ASTNode::NodeType::POINTER_MEMBER ? "->" : ".");
            path += member->member;
            return true;
        }
    }

    return false;
}

void BorrowChecker::recordPinDerivedAlias(const std::string& alias,
                                          ASTNode* initializer,
                                          ASTNode* node) {
    std::string pin_ref;
    std::string host;
    std::string path;

    if (!findPinnedRootedPath(initializer, pin_ref, host, path)) {
        ctx.pin_derived_aliases.erase(alias);
        return;
    }

    if (!validateLifetime(host, alias, node)) {
        return;
    }

    ctx.pin_derived_aliases[alias] = PinAliasInfo{pin_ref, host, path};
    ctx.loan_origins[alias].insert(host);
}

void BorrowChecker::recordPin(const std::string& host, const std::string& pin_ref, ASTNode* node) {
    // Check if already pinned
    if (isPinned(host)) {
        addError("Variable '" + host + "' is already pinned by '" + 
                ctx.active_pins[host] + "'", node);
        tagCode("ARIA-016");
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

    for (auto it = ctx.pin_derived_aliases.begin(); it != ctx.pin_derived_aliases.end(); ) {
        if (it->second.pin_ref == var) {
            it = ctx.pin_derived_aliases.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// Wild Memory Safety (Phase 3.3.3)
// ============================================================================

void BorrowChecker::recordWildAlloc(const std::string& var, ASTNode* node, const std::string& size_expr) {
    ctx.pending_wild_frees.insert(var);
    ctx.wild_states[var] = WildState::ALLOCATED;
    if (!size_expr.empty()) {
        ctx.wild_alloc_sizes[var] = size_expr;
    }
    if (borrow_debug) {
        int line = node ? node->line : 0;
        fprintf(stderr, "[borrow-debug] ALLOC '%s' size=%s at line %d\n",
                var.c_str(), size_expr.empty() ? "unknown" : size_expr.c_str(), line);
    }
}

void BorrowChecker::recordWildFree(const std::string& var, ASTNode* node) {
    // Check if variable is allocated
    auto it = ctx.wild_states.find(var);
    if (it == ctx.wild_states.end()) {
        addError("Cannot free undefined variable '" + var + "'", node);
        tagCode("ARIA-022");
        return;
    }
    
    if (it->second == WildState::FREED) {
        addErrorWithSuggestion("Double free of variable '" + var + "' (already freed)", node,
                "hint: remove the duplicate free(), or set '" + var + "' to null after the first free");
        tagCode("ARIA-022");
        return;
    }
    
    if (it->second == WildState::MOVED) {
        addError("Cannot free moved variable '" + var + "'", node);
        tagCode("ARIA-022");
        return;
    }
    
    // Mark as freed
    ctx.wild_states[var] = WildState::FREED;
    ctx.pending_wild_frees.erase(var);
    ctx.wild_alloc_sizes.erase(var);
    if (borrow_debug) {
        int line = node ? node->line : 0;
        fprintf(stderr, "[borrow-debug] FREE '%s' at line %d\n", var.c_str(), line);
    }
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
            tagCode("ARIA-022");
            return false;

        case WildState::MOVED:
            addErrorWithSuggestion("Use after move: variable '" + var + "' was moved", node,
                "hint: clone the value before moving if you need it afterward");
            tagCode("ARIA-019");
            return false;

        case WildState::UNINITIALIZED:
            addErrorWithSuggestion("Use of uninitialized wild pointer '" + var + "'", node,
                "hint: allocate memory for '" + var + "' before using it");
            tagCode("ARIA-022");
            return false;

        case WildState::UNKNOWN:
            addError("Use of wild pointer '" + var + "' with unknown state " +
                    "(may have been freed or moved in a branch)", node);
            tagCode("ARIA-022");
            return false;

        case WildState::MAY_FREED:
            addError("Use of wild pointer '" + var + "' that may have been freed " +
                    "in a previous loop iteration", node);
            tagCode("ARIA-022");
            return false;

        case WildState::ALLOCATED:
            return true;
    }

    return true;
}

// v0.6.3: Realloc invalidation — realloc may move memory, invalidating all borrows
void BorrowChecker::recordWildRealloc(const std::string& var, ASTNode* node, const std::string& new_size_expr) {
    auto it = ctx.wild_states.find(var);
    if (it == ctx.wild_states.end()) {
        addError("Cannot realloc undefined variable '" + var + "'", node);
        tagCode("ARIA-022");
        return;
    }

    if (it->second == WildState::FREED) {
        addErrorWithSuggestion("Cannot realloc freed variable '" + var + "'", node,
                "hint: allocate new memory with alloc() instead of realloc on freed pointer");
        tagCode("ARIA-022");
        return;
    }

    if (it->second == WildState::MOVED) {
        addError("Cannot realloc moved variable '" + var + "'", node);
        tagCode("ARIA-022");
        return;
    }

    // realloc may move the allocation, making all existing pointers dangling
    auto loans_it = ctx.active_loans.find(var);
    if (loans_it != ctx.active_loans.end() && !loans_it->second.empty()) {
        const Loan& loan = loans_it->second.front();
        addError("Cannot realloc '" + var + "' because it has active borrows. "
                 "realloc may move memory, invalidating all references",
                 node,
                 "Active borrow by '" + loan.borrower + "' was created here",
                 loan.creation_line, loan.creation_column);
        tagCode("ARIA-022");
        return;
    }

    // Also check path-based loans
    for (const auto& [path, loans] : ctx.path_loans) {
        if (!loans.empty() && path.base_var == var) {
            const Loan& loan = loans.front();
            addError("Cannot realloc '" + var + "' because a sub-path is borrowed by '" +
                     loan.borrower + "'",
                     node,
                     "Path borrow was created here",
                     loan.creation_line, loan.creation_column);
            tagCode("ARIA-022");
            return;
        }
    }

    // realloc() consumes the old pointer — mark it as moved/consumed
    // so the caller is NOT responsible for freeing the old pointer.
    // The new pointer (return value) will be tracked as a fresh wild allocation.
    ctx.wild_states[var] = WildState::MOVED;
    ctx.pending_wild_frees.erase(var);

    // Update allocation size for diagnostic purposes
    if (!new_size_expr.empty()) {
        ctx.wild_alloc_sizes[var] = new_size_expr;
    }

    if (borrow_debug) {
        int line = node ? node->line : 0;
        fprintf(stderr, "[borrow-debug] REALLOC '%s' new_size=%s at line %d\n",
                var.c_str(), new_size_expr.empty() ? "unknown" : new_size_expr.c_str(), line);
    }
}

void BorrowChecker::checkForLeaks() {
    // Check for wild memory that wasn't freed
    if (!ctx.scope_stack.empty()) {
        const auto& current_scope_vars = ctx.scope_stack.back();

        for (const auto& var : current_scope_vars) {
            if (ctx.pending_wild_frees.count(var) > 0) {
                BorrowError err(1, 1, "Memory leak: wild variable '" + var + "' was not freed before scope exit",
                        "hint: add 'defer { free(" + var + "); }' after allocation, or free before end of scope");
                err.code = "ARIA-014";
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
                    tagCode("ARIA-014");
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
            // Check function body with parameter awareness
            // Each function gets an isolated borrow context (no cross-function leakage)
            auto* funcDecl = static_cast<FuncDeclStmt*>(stmt);
            std::string prev_function = current_function;
            current_function = funcDecl->funcName;
            if (funcDecl->body) {
                LifetimeContext saved_ctx = ctx.snapshot();
                ctx = LifetimeContext();  // Fresh context for this function
                ctx.enterScope();
                // Register parameters so borrows of params are tracked
                registerFunctionParams(funcDecl);
                checkStatement(funcDecl->body.get());
                if (borrow_dump_enabled) {
                    dumpBorrowState(std::cerr, funcDecl->funcName);
                }
                ctx.exitScope();
                ctx.restore(saved_ctx);
            }
            current_function = prev_function;
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

        case ASTNode::NodeType::LOOP:
            checkLoopStmt(static_cast<LoopStmt*>(stmt));
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

        case ASTNode::NodeType::PICK:
            checkPickStmt(static_cast<PickStmt*>(stmt));
            break;

        case ASTNode::NodeType::WHEN:
            checkWhenStmt(static_cast<WhenStmt*>(stmt));
            break;

        case ASTNode::NodeType::TRAIT_DECL:
            checkTraitDeclStmt(static_cast<TraitDeclStmt*>(stmt));
            break;

        case ASTNode::NodeType::IMPL_DECL:
            checkImplDeclStmt(static_cast<ImplDeclStmt*>(stmt));
            break;

        case ASTNode::NodeType::STRUCT_DECL:
            // Struct declarations don't need borrow checking themselves,
            // but we acknowledge them so they don't fall through to default
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

    const std::string type_text = stmt->typeNode ? stmt->typeNode->toString() : stmt->typeName;
    const bool is_pointer_decl = stmt->typeName.find("->") != std::string::npos ||
        stmt->typeName.find("@") != std::string::npos ||
        type_text.find("->") != std::string::npos ||
        type_text.find("@") != std::string::npos;
    if (is_pointer_decl) {
        ctx.pointer_vars.insert(stmt->varName);
    }

    // Check initializer if present
    if (stmt->initializer) {
        checkExpression(stmt->initializer.get());

        // ====================================================================
        // v0.2.35: $$i/$$m qualifier-based borrows
        // ====================================================================
        if (stmt->isBorrowImm || stmt->isBorrowMut) {
            // Extract borrow target from initializer (must be an identifier or
            // member access — validated by type checker, but guard defensively)
            AccessPath borrow_path = extractAccessPath(stmt->initializer.get());
            if (!borrow_path.base_var.empty()) {
                bool is_mutable = stmt->isBorrowMut;
                recordBorrowWithPath(borrow_path, stmt->varName, is_mutable, stmt);
            }
            // Early return — $$i/$$m variables fully express borrow intent.
            return;
        }

        // Check if initializer is a pin operation.
        if (stmt->initializer->type == ASTNode::NodeType::UNARY_OP) {
            UnaryExpr* unary = static_cast<UnaryExpr*>(stmt->initializer.get());

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

        if (is_pointer_decl) {
            recordPinDerivedAlias(stmt->varName, stmt->initializer.get(), stmt);
        }
    }

    // Check if this is a wild allocation (only if NOT a pin operation)
    // A pin creates a wild pointer to GC memory, not a wild allocation
    // Use isWild flag set by parser when 'wild' keyword is present
    bool is_wild_type = stmt->isWild || stmt->typeName.find("wild") == 0;
    if (is_wild_type && stmt->initializer && !stmt->is_pinned_shadow) {
        // This is an actual wild allocation (e.g., wild int8@:ptr = alloc(...))
        stmt->requires_drop = true;

        // v0.6.3: Extract allocation size from alloc(size) call
        std::string alloc_size_expr;
        if (stmt->initializer->type == ASTNode::NodeType::CALL) {
            auto* call = static_cast<CallExpr*>(stmt->initializer.get());
            if (call->callee && call->callee->type == ASTNode::NodeType::IDENTIFIER) {
                auto* callee = static_cast<IdentifierExpr*>(call->callee.get());
                if ((callee->name == "alloc" || callee->name == "npk_alloc" ||
                     callee->name == "malloc") &&
                    !call->arguments.empty()) {
                    ASTNode* sizeArg = call->arguments[0].get();
                    if (sizeArg && sizeArg->type == ASTNode::NodeType::LITERAL) {
                        auto* lit = static_cast<LiteralExpr*>(sizeArg);
                        alloc_size_expr = lit->raw_value_string.empty()
                            ? std::to_string(std::get<int64_t>(lit->value))
                            : lit->raw_value_string;
                    } else if (sizeArg && sizeArg->type == ASTNode::NodeType::IDENTIFIER) {
                        alloc_size_expr = static_cast<IdentifierExpr*>(sizeArg)->name;
                    }
                }
                // v0.6.3: Also extract size from realloc(ptr, new_size)
                if ((callee->name == "realloc" || callee->name == "npk_realloc") &&
                    call->arguments.size() >= 2) {
                    ASTNode* sizeArg = call->arguments[1].get();
                    if (sizeArg && sizeArg->type == ASTNode::NodeType::LITERAL) {
                        auto* lit = static_cast<LiteralExpr*>(sizeArg);
                        alloc_size_expr = lit->raw_value_string.empty()
                            ? std::to_string(std::get<int64_t>(lit->value))
                            : lit->raw_value_string;
                    } else if (sizeArg && sizeArg->type == ASTNode::NodeType::IDENTIFIER) {
                        alloc_size_expr = static_cast<IdentifierExpr*>(sizeArg)->name;
                    }
                }
            }
        }
        recordWildAlloc(stmt->varName, stmt, alloc_size_expr);
    }
}

void BorrowChecker::checkAssignment(BinaryExpr* expr) {
    if (!expr) return;

    std::string target_name;

    auto rootIdentifierName = [&](auto&& self, ASTNode* node) -> std::string {
        if (!node) return "";
        if (node->type == ASTNode::NodeType::IDENTIFIER) {
            return static_cast<IdentifierExpr*>(node)->name;
        }
        if (node->type == ASTNode::NodeType::MEMBER_ACCESS ||
            node->type == ASTNode::NodeType::POINTER_MEMBER) {
            MemberAccessExpr* member = static_cast<MemberAccessExpr*>(node);
            return self(self, member->object.get());
        }
        if (node->type == ASTNode::NodeType::INDEX) {
            IndexExpr* index = static_cast<IndexExpr*>(node);
            return self(self, index->array.get());
        }
        return "";
    };

    // Check left side (ensure it's not pinned or borrowed mutably)
    if (expr->left->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* target = static_cast<IdentifierExpr*>(expr->left.get());
        target_name = target->name;

        // Check if target is pinned
        if (isPinned(target->name)) {
            addErrorWithSuggestion("Cannot assign to pinned variable '" + target->name + "'", expr,
                "hint: unpin the variable first, or work with a copy");
            tagCode("ARIA-016");
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
            tagCode("ARIA-026");
            return;
        }

        // Check for use-after-free
        checkWildUse(target->name, expr);
    } else if (expr->left->type == ASTNode::NodeType::UNARY_OP) {
        UnaryExpr* unary = static_cast<UnaryExpr*>(expr->left.get());
        if (unary->op.type == frontend::TokenType::TOKEN_LEFT_ARROW ||
            unary->op.type == frontend::TokenType::TOKEN_STAR) {
            std::string pin_ref, host, path;
            if (findPinnedRootedPath(unary->operand.get(), pin_ref, host, path)) {
                addErrorWithSuggestion("Cannot assign through pin reference '" + pin_ref +
                        "' via path '" + path + "' rooted at pinned variable '" + host + "'", expr,
                        "hint: pins provide stable read access; unpin or use a non-pinned pointer before mutation");
                tagCode("ARIA-016");
                return;
            }
        }
    } else if (expr->left->type == ASTNode::NodeType::POINTER_MEMBER) {
        MemberAccessExpr* member = static_cast<MemberAccessExpr*>(expr->left.get());
        std::string pin_ref, host, path;
        if (findPinnedRootedPath(expr->left.get(), pin_ref, host, path)) {
            addErrorWithSuggestion("Cannot assign through pin reference '" + pin_ref +
                    "' via path '" + path + "' rooted at pinned variable '" + host + "'", expr,
                    "hint: pins provide stable read access; unpin or use a non-pinned pointer before mutation");
            tagCode("ARIA-016");
            return;
        }
    } else if (expr->left->type == ASTNode::NodeType::INDEX) {
        AccessPath target_path = extractAccessPath(expr->left.get());
        if (!target_path.base_var.empty()) {
            const Loan* conflict = ctx.checkPathConflict(target_path, true);
            if (conflict) {
                addError("Cannot assign to '" + target_path.toString() + "' because it is borrowed",
                        expr,
                        "Borrowed by '" + conflict->borrower + "' here",
                        conflict->creation_line, conflict->creation_column);
                tagCode("ARIA-026");
                return;
            }
        }

        std::string root = rootIdentifierName(rootIdentifierName, expr->left.get());
        if (!root.empty() && isPinned(root)) {
            addErrorWithSuggestion("Cannot assign to indexed element of pinned variable '" + root + "'", expr,
                    "hint: pins provide stable read access; unpin before mutating elements");
            tagCode("ARIA-016");
            return;
        }
    } else if (expr->left->type == ASTNode::NodeType::MEMBER_ACCESS) {
        MemberAccessExpr* member = static_cast<MemberAccessExpr*>(expr->left.get());
        AccessPath target_path = extractAccessPath(expr->left.get());
        if (!target_path.base_var.empty()) {
            const Loan* conflict = ctx.checkPathConflict(target_path, true);
            if (conflict) {
                addError("Cannot assign to '" + target_path.toString() + "' because it is borrowed",
                        expr,
                        "Borrowed by '" + conflict->borrower + "' here",
                        conflict->creation_line, conflict->creation_column);
                tagCode("ARIA-026");
                return;
            }
        }

        std::string root = rootIdentifierName(rootIdentifierName, expr->left.get());
        if (!root.empty() && isPinned(root)) {
            addErrorWithSuggestion("Cannot assign to field '" + member->member +
                    "' of pinned variable '" + root + "'", expr,
                    "hint: pins provide stable read access; unpin before mutating fields");
            tagCode("ARIA-016");
            return;
        }
    }

    // Check right side
    checkExpression(expr->right.get());

    if (!target_name.empty() && ctx.pointer_vars.count(target_name) > 0) {
        recordPinDerivedAlias(target_name, expr->right.get(), expr);
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

// ============================================================================
// loop() Statement Borrow Checking (v0.19.2)
// ============================================================================
void BorrowChecker::checkLoopStmt(LoopStmt* stmt) {
    if (!stmt) return;

    // Analyze the bound expressions (evaluated once, may create borrows)
    if (stmt->start)  checkExpression(stmt->start.get());
    if (stmt->limit)  checkExpression(stmt->limit.get());
    if (stmt->step)   checkExpression(stmt->step.get());

    // Snapshot state before entering the repeating body
    LifetimeContext entry_state = ctx.snapshot();
    LifetimeContext loop_head_state = entry_state;

    bool changed = true;
    int iterations = 0;

    while (changed && iterations < MAX_FIXPOINT_ITERATIONS) {
        changed = false;
        iterations++;

        ctx.restore(loop_head_state);

        // Analyze body in its own scope
        ctx.enterScope();
        if (stmt->body) {
            checkStatement(stmt->body.get());
        }

        // Capture back-edge state before exiting iteration scope
        LifetimeContext back_edge_state = ctx.snapshot();
        ctx.exitScope();

        if (loop_head_state.mergeLoopBackEdge(back_edge_state)) {
            changed = true;
        }

        if (iterations >= WIDENING_THRESHOLD && changed) {
            loop_head_state.widen();
            if (iterations >= MAX_FIXPOINT_ITERATIONS - 1) {
                changed = false;
            }
        }
    }

    // Settle on the fixed-point header state
    ctx.restore(loop_head_state);

    // Check for loop-carried mutable borrow conflicts
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
            tagCode("ARIA-025");
        }
    }
}

void BorrowChecker::checkForStmt(ForStmt* stmt) {
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
            tagCode("ARIA-025");
        }
    }

    // Exit loop scope
    ctx.exitScope();
}

// ============================================================================
// v0.6.1: Pick Statement Borrow Flow
// ============================================================================

void BorrowChecker::checkPickStmt(PickStmt* stmt) {
    if (!stmt) return;

    // Check selector expression (may create borrows)
    checkExpression(stmt->selector.get());

    // Snapshot state before branching into cases
    auto pre_pick_state = ctx.snapshot();

    // Track states from each case arm
    std::vector<LifetimeContext> arm_states;
    bool has_wildcard = false;

    for (const auto& case_node : stmt->cases) {
        auto* pick_case = static_cast<PickCase*>(case_node.get());
        if (!pick_case) continue;

        // Skip unreachable arms
        if (pick_case->is_unreachable) continue;

        // Check if this is the wildcard (*) case
        if (!pick_case->pattern) {
            has_wildcard = true;
        }

        // Restore to pre-branch state for each arm
        ctx.restore(pre_pick_state);

        // Check pattern expression (may reference variables)
        if (pick_case->pattern) {
            checkExpression(pick_case->pattern.get());
        }

        // Check arm body in its own scope
        ctx.enterScope();
        if (pick_case->body) {
            checkStatement(pick_case->body.get());
        }
        ctx.exitScope();

        // Capture post-arm state
        arm_states.push_back(ctx.snapshot());
    }

    // Merge all arm states (conservative: union of all possible borrow states)
    if (!arm_states.empty()) {
        // Start with the first arm state
        ctx.restore(arm_states[0]);

        // Merge remaining arms pairwise
        for (size_t i = 1; i < arm_states.size(); ++i) {
            auto current = ctx.snapshot();
            ctx.merge(current, arm_states[i]);
        }
    }

    // If no wildcard and no cases, restore pre-pick state
    // (the pick might not execute any arm)
    if (!has_wildcard && !arm_states.empty()) {
        auto merged = ctx.snapshot();
        ctx.merge(merged, pre_pick_state);
    }
}

// ============================================================================
// v0.6.1: When Statement (Loop) Borrow Flow
// ============================================================================

void BorrowChecker::checkWhenStmt(WhenStmt* stmt) {
    if (!stmt) return;

    // when(condition) { body } then { } end { }
    // 'when' is a conditional loop: body executes while condition is true
    // Uses fixpoint iteration like while/for

    LifetimeContext entry_state = ctx.snapshot();
    LifetimeContext loop_head_state = entry_state;

    bool changed = true;
    int iterations = 0;

    // Fixpoint loop
    while (changed && iterations < MAX_FIXPOINT_ITERATIONS) {
        changed = false;
        iterations++;

        ctx.restore(loop_head_state);

        // Check condition
        checkExpression(stmt->condition.get());

        // Check body in its own scope
        ctx.enterScope();
        if (stmt->body) {
            checkStatement(stmt->body.get());
        }
        LifetimeContext back_edge_state = ctx.snapshot();
        ctx.exitScope();

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

    // Final state: the fixed point
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
                    "' is mutably borrowed multiple times across when-loop iterations",
                    stmt,
                    "First mutable borrow here",
                    first_mutable->creation_line, first_mutable->creation_column);
            tagCode("ARIA-025");
        }
    }

    // Check 'then' block (executes on normal completion)
    if (stmt->then_block) {
        ctx.enterScope();
        checkStatement(stmt->then_block.get());
        ctx.exitScope();
    }

    // Check 'end' block (executes on break or no execution)
    // For borrow analysis, merge then-path and end-path
    if (stmt->end_block) {
        auto then_state = ctx.snapshot();
        ctx.restore(loop_head_state);
        ctx.enterScope();
        checkStatement(stmt->end_block.get());
        ctx.exitScope();
        auto end_state = ctx.snapshot();
        ctx.merge(then_state, end_state);
    }
}

// ============================================================================
// v0.6.2: Trait Integration — Trait and Impl Borrow Checking
// ============================================================================

void BorrowChecker::checkTraitDeclStmt(TraitDeclStmt* stmt) {
    if (!stmt) return;

    // Validate trait method signatures for borrow consistency.
    // Each method that takes Self must declare consistent borrow qualifiers:
    //   - Self:self      = takes ownership (move)
    //   - $$i Self:self  = immutable borrow (reads only)
    //   - $$m Self:self  = mutable borrow (can modify)
    // A method with no self parameter is a static/associated function.

    for (const auto& method : stmt->methods) {
        bool has_self = false;
        bool self_is_borrow_imm = false;
        bool self_is_borrow_mut = false;

        for (const auto& param : method.parameters) {
            if (param.paramName == "self") {
                has_self = true;
                self_is_borrow_imm = param.isBorrowImm;
                self_is_borrow_mut = param.isBorrowMut;

                // Cannot be both $$i and $$m
                if (self_is_borrow_imm && self_is_borrow_mut) {
                    addError("Trait '" + stmt->traitName + "' method '" + method.name +
                             "': self parameter cannot be both $$i (immutable) and $$m (mutable)",
                             stmt);
                    tagCode("ARIA-024");
                }
                break;
            }
        }

        // Methods that mutate should use $$m self, read-only methods should use $$i self.
        // Not enforced as error — just tracked for impl validation.
        (void)has_self;
    }
}

void BorrowChecker::checkImplDeclStmt(ImplDeclStmt* stmt) {
    if (!stmt) return;

    // Validate trait method signatures match if this implements a known trait
    auto trait_it = trait_methods.find(stmt->traitName);

    // Check each method in the impl block
    for (const auto& methodNode : stmt->methods) {
        if (!methodNode || methodNode->type != ASTNode::NodeType::FUNC_DECL) continue;
        auto* func = static_cast<FuncDeclStmt*>(methodNode.get());

        // v0.6.2: Validate self parameter borrow qualifiers match trait declaration
        if (trait_it != trait_methods.end()) {
            for (const auto& traitMethod : trait_it->second) {
                if (traitMethod.name != func->funcName) continue;

                // Find self in trait method signature
                bool trait_self_imm = false;
                bool trait_self_mut = false;
                bool trait_has_self = false;
                for (const auto& tp : traitMethod.parameters) {
                    if (tp.paramName == "self") {
                        trait_has_self = true;
                        trait_self_imm = tp.isBorrowImm;
                        trait_self_mut = tp.isBorrowMut;
                        break;
                    }
                }

                // Find self in impl method signature
                bool impl_self_imm = false;
                bool impl_self_mut = false;
                bool impl_has_self = false;
                for (const auto& param : func->parameters) {
                    if (!param || param->type != ASTNode::NodeType::PARAMETER) continue;
                    auto* p = static_cast<ParameterNode*>(param.get());
                    if (p->paramName == "self") {
                        impl_has_self = true;
                        impl_self_imm = p->isBorrowImm;
                        impl_self_mut = p->isBorrowMut;
                        break;
                    }
                }

                // Mismatch: trait says $$i, impl says $$m (or other combinations)
                if (trait_has_self && impl_has_self) {
                    if (trait_self_imm != impl_self_imm || trait_self_mut != impl_self_mut) {
                        std::string trait_qual = trait_self_mut ? "$$m" : (trait_self_imm ? "$$i" : "owned");
                        std::string impl_qual = impl_self_mut ? "$$m" : (impl_self_imm ? "$$i" : "owned");
                        addError("Method '" + func->funcName + "' in impl " + stmt->traitName +
                                 " for " + stmt->typeName + ": self borrow qualifier mismatch — "
                                 "trait declares " + trait_qual + " but impl uses " + impl_qual,
                                 methodNode.get());
                        tagCode("ARIA-024");
                    }
                } else if (trait_has_self && !impl_has_self) {
                    addError("Method '" + func->funcName + "' in impl " + stmt->traitName +
                             " for " + stmt->typeName + ": trait requires self parameter but impl omits it",
                             methodNode.get());
                    tagCode("ARIA-024");
                }
                break;
            }
        }

        // Borrow-check the method body with self-awareness
        // We analyze each impl method like a normal function, but register
        // 'self' as a variable of the impl type with appropriate borrow mode
        std::string prev_function = current_function;
        std::string mangledName = stmt->typeName + "_" + func->funcName;
        current_function = mangledName;

        if (func->body) {
            LifetimeContext saved_ctx = ctx.snapshot();
            ctx = LifetimeContext();  // Fresh context for this method
            ctx.enterScope();

            // Register parameters, with special handling for 'self'
            for (const auto& param : func->parameters) {
                if (!param || param->type != ASTNode::NodeType::PARAMETER) continue;
                auto* p = static_cast<ParameterNode*>(param.get());

                registerVariable(p->paramName, param.get());

                if (p->paramName == "self") {
                    // 'self' is the receiver — track borrow mode
                    if (p->isBorrowImm) {
                        // $$i self: immutable borrow from caller
                        std::string synthetic_host = "__caller_self";
                        ctx.var_depths[synthetic_host] = ctx.current_depth - 1;
                        recordBorrow(synthetic_host, "self", false, param.get());
                    } else if (p->isBorrowMut) {
                        // $$m self: mutable borrow from caller
                        std::string synthetic_host = "__caller_self";
                        ctx.var_depths[synthetic_host] = ctx.current_depth - 1;
                        recordBorrow(synthetic_host, "self", true, param.get());
                    }
                    // else: owned self — move semantics, no borrow recorded
                } else if (p->isBorrowImm || p->isBorrowMut) {
                    std::string synthetic_host = "__caller_" + p->paramName;
                    ctx.var_depths[synthetic_host] = ctx.current_depth - 1;
                    recordBorrow(synthetic_host, p->paramName, p->isBorrowMut, param.get());
                }

                if (p->isWild || p->isWildx) {
                    ctx.wild_states[p->paramName] = WildState::ALLOCATED;
                }
            }

            checkStatement(func->body.get());
            ctx.exitScope();
            ctx.restore(saved_ctx);
        }

        current_function = prev_function;
    }

    // v0.6.2: If this type implements Droppable, validate the drop/finalize method
    if (stmt->traitName == "Droppable") {
        bool has_finalize = false;
        for (const auto& methodNode : stmt->methods) {
            if (!methodNode || methodNode->type != ASTNode::NodeType::FUNC_DECL) continue;
            auto* func = static_cast<FuncDeclStmt*>(methodNode.get());
            if (func->funcName == "finalize" || func->funcName == "drop") {
                has_finalize = true;
                // The drop/finalize method should take $$m self (mutable borrow)
                // so it can clean up internal state
                for (const auto& param : func->parameters) {
                    if (!param || param->type != ASTNode::NodeType::PARAMETER) continue;
                    auto* p = static_cast<ParameterNode*>(param.get());
                    if (p->paramName == "self" && !p->isBorrowMut) {
                        addWarning("Droppable finalize/drop method for '" + stmt->typeName +
                                   "' should take $$m self (mutable borrow) to clean up internal state",
                                   methodNode.get(),
                                   "Change to: func:finalize = NIL($$m " + stmt->typeName + ":self)");
                        tagCode("ARIA-024");
                    }
                }
            }
        }
        if (!has_finalize && !stmt->methods.empty()) {
            addWarning("Type '" + stmt->typeName + "' implements Droppable but has no "
                       "finalize/drop method — custom destructor will not run",
                       stmt);
            tagCode("ARIA-024");
        }
    }
}

// ============================================================================
// v0.6.1: Return Type Borrow Validation
// ============================================================================

void BorrowChecker::checkReturnBorrowEscape(ASTNode* returnValue, ASTNode* context) {
    (void)returnValue;
    (void)context;
    // Dollar-prefixed borrow-return expression syntax is intentionally invalid in Aria.
    // $$i/$$m qualifier escape checks are handled by declaration/path tracking.

    // Also check if returning a variable that's itself a borrow
    if (returnValue->type == ASTNode::NodeType::IDENTIFIER) {
        auto* ident = static_cast<IdentifierExpr*>(returnValue);
        // Check if this variable holds a borrow that would escape
        auto it = ctx.loan_origins.find(ident->name);
        if (it != ctx.loan_origins.end()) {
            for (const auto& origin : it->second) {
                int origin_depth = getVariableDepth(origin);
                if (origin_depth > 1) {
                    addError("Cannot return '" + ident->name +
                            "' — it borrows from local variable '" + origin +
                            "' which will be destroyed when the function returns",
                            context,
                            "'" + origin + "' is local to this function",
                            returnValue->line, returnValue->column);
                    tagCode("ARIA-017");
                }
            }
        }
    }
}

// ============================================================================
// v0.6.1: Z3-Backed Index Disjointness
// ============================================================================

bool BorrowChecker::proveIndexDisjoint(ASTNode* idx1, ASTNode* idx2) {
    if (!idx1 || !idx2) return false;

    // Fast path: literal comparison
    if (idx1->type == ASTNode::NodeType::LITERAL &&
        idx2->type == ASTNode::NodeType::LITERAL) {
        auto* lit1 = static_cast<LiteralExpr*>(idx1);
        auto* lit2 = static_cast<LiteralExpr*>(idx2);
        if (std::holds_alternative<int64_t>(lit1->value) &&
            std::holds_alternative<int64_t>(lit2->value)) {
            return std::get<int64_t>(lit1->value) != std::get<int64_t>(lit2->value);
        }
    }

    // Identifier comparison: if both are identifiers referring to different
    // variables, check if they have disjoint known ranges from Rules constraints
    if (idx1->type == ASTNode::NodeType::IDENTIFIER &&
        idx2->type == ASTNode::NodeType::IDENTIFIER) {
        auto* id1 = static_cast<IdentifierExpr*>(idx1);
        auto* id2 = static_cast<IdentifierExpr*>(idx2);
        
        // Same variable name → cannot prove disjoint
        if (id1->name == id2->name) {
            return false;
        }
        
        // If Z3 verifier is available, attempt range-based disjointness proof.
        // Look up both variables' Rules constraints; if their ranges are
        // non-overlapping, they're provably disjoint.
        if (z3_verifier) {
            // Check if both variables have limit<Rules> constraints that
            // define non-overlapping integer ranges. The Z3 verifier's
            // verifyLimitRange can determine if a value in [lo1, hi1]
            // can equal a value in [lo2, hi2] — if the ranges don't
            // overlap, indices are provably different.
            // This requires Rules constraint lookup from the enclosing scope,
            // which is not yet wired in. When Rules constraints are available
            // on BorrowChecker's context, this path activates.
        }
    }

    // Binary expression with literal offset: arr[i+1] vs arr[i] are disjoint
    // when the offset difference is non-zero
    if (idx1->type == ASTNode::NodeType::BINARY_OP &&
        idx2->type == ASTNode::NodeType::IDENTIFIER) {
        auto* binop = static_cast<BinaryExpr*>(idx1);
        if (binop->left && binop->left->type == ASTNode::NodeType::IDENTIFIER &&
            binop->right && binop->right->type == ASTNode::NodeType::LITERAL) {
            auto* base = static_cast<IdentifierExpr*>(binop->left.get());
            auto* id2 = static_cast<IdentifierExpr*>(idx2);
            auto* offset = static_cast<LiteralExpr*>(binop->right.get());
            if (base->name == id2->name &&
                std::holds_alternative<int64_t>(offset->value) &&
                std::get<int64_t>(offset->value) != 0) {
                return true;  // i+K vs i are disjoint when K != 0
            }
        }
    }
    // Symmetric case: idx1 is identifier, idx2 is binary
    if (idx2->type == ASTNode::NodeType::BINARY_OP &&
        idx1->type == ASTNode::NodeType::IDENTIFIER) {
        return proveIndexDisjoint(idx2, idx1);
    }

    return false;
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
                        tagCode("ARIA-017");
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

        // v0.6.1: Validate return borrow doesn't escape local lifetime
        if (!current_function.empty()) {
            checkReturnBorrowEscape(stmt->value.get(), stmt);
        }
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

        // v0.6.1: Validate pass borrow doesn't escape local lifetime
        if (!current_function.empty()) {
            checkReturnBorrowEscape(stmt->value.get(), stmt);
        }
    }
}

/**
 * ARIA-017: Check if a reference/borrow expression would escape its scope.
 * Called when returning or passing values that might be references to locals.
 *
 * Dollar-prefixed borrow expressions are rejected by the parser; current Aria
 * borrow intent is declared with $$i / $$m qualifiers.
 */
void BorrowChecker::checkReferenceEscape(ASTNode* value, ASTNode* context) {
    (void)value;
    (void)context;
    // No dollar-borrow expression syntax exists in current Aria.
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
    "free", "aria.free", "npk_free",
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
    
    // Pin operations are handled at variable declaration. Dollar-borrow
    // expressions are intentionally not Nitpick syntax.
}

void BorrowChecker::checkIdentifier(IdentifierExpr* expr) {
    if (!expr) return;
    
    // Check for use-after-move
    if (ctx.moved_variables.find(expr->name) != ctx.moved_variables.end()) {
        addErrorWithSuggestion("Use after move: variable '" + expr->name + "' was moved", expr,
                "hint: clone the value before moving if you need it afterward");
        tagCode("ARIA-019");
        return;
    }
    checkWildUse(expr->name, expr);
}

void BorrowChecker::checkCallExpr(CallExpr* expr) {
    if (!expr) return;

    // Check all arguments
    for (const auto& arg : expr->arguments) {
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
                         "Use an explicit $$i/$$m borrow-compatible API instead.",
                         arg.get());
                tagCode("ARIA-016");
            }

            auto aliasIt = ctx.pin_derived_aliases.find(ident->name);
            if (aliasIt != ctx.pin_derived_aliases.end()) {
                addError("Cannot pass pin-derived pointer alias '" + ident->name +
                         "' by value. Alias derives from pin reference '" + aliasIt->second.pin_ref +
                         "' rooted at pinned variable '" + aliasIt->second.host + "'. "
                         "Use an explicit $$i borrow-compatible API instead.",
                         arg.get());
                tagCode("ARIA-016");
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
                tagCode("ARIA-019");
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

        // v0.6.3: Realloc borrow invalidation
        // realloc(ptr, new_size) may move memory — invalidate all borrows of ptr
        if ((callee->name == "realloc" || callee->name == "npk_realloc") &&
            expr->arguments.size() >= 2) {
            ASTNode* ptrArg = expr->arguments[0].get();
            if (ptrArg && ptrArg->type == ASTNode::NodeType::IDENTIFIER) {
                auto* ptrIdent = static_cast<IdentifierExpr*>(ptrArg);
                if (ctx.wild_states.find(ptrIdent->name) != ctx.wild_states.end()) {
                    // Extract new size expression for tracking
                    std::string new_size_expr;
                    ASTNode* sizeArg = expr->arguments[1].get();
                    if (sizeArg && sizeArg->type == ASTNode::NodeType::LITERAL) {
                        auto* lit = static_cast<LiteralExpr*>(sizeArg);
                        new_size_expr = lit->raw_value_string.empty()
                            ? std::to_string(std::get<int64_t>(lit->value))
                            : lit->raw_value_string;
                    } else if (sizeArg && sizeArg->type == ASTNode::NodeType::IDENTIFIER) {
                        new_size_expr = static_cast<IdentifierExpr*>(sizeArg)->name;
                    }
                    recordWildRealloc(ptrIdent->name, expr, new_size_expr);
                }
            }
        }
    }

    // v0.6.0: Check function signature for ownership transfer
    // Look up callee summary and enforce move/borrow semantics at call site
    if (expr->callee && expr->callee->type == ASTNode::NodeType::IDENTIFIER) {
        auto* callee = static_cast<IdentifierExpr*>(expr->callee.get());
        auto summary_it = func_summaries.find(callee->name);
        if (summary_it != func_summaries.end()) {
            checkCallOwnership(expr, summary_it->second);
        }
    }

    // v0.6.2: UFCS method call — obj.method(args) desugars to Type_method(obj, args)
    // Look up the mangled name and enforce self borrow semantics
    if (expr->callee && expr->callee->type == ASTNode::NodeType::MEMBER_ACCESS) {
        auto* memberExpr = static_cast<MemberAccessExpr*>(expr->callee.get());
        const std::string& methodName = memberExpr->member;

        // Extract object name for borrow checking
        std::string obj_name;
        if (memberExpr->object && memberExpr->object->type == ASTNode::NodeType::IDENTIFIER) {
            obj_name = static_cast<IdentifierExpr*>(memberExpr->object.get())->name;
        }

        // Try to find a matching Type_method summary
        // We search all summaries for trait methods matching this method name
        for (const auto& [name, summary] : func_summaries) {
            if (!summary.is_trait_method) continue;
            // Check if this summary's un-mangled method name matches
            std::string expected_suffix = "_" + methodName;
            if (name.length() > expected_suffix.length() &&
                name.substr(name.length() - expected_suffix.length()) == expected_suffix) {

                // Found a trait method — enforce self borrow rules
                if (summary.self_param_index >= 0 && !obj_name.empty()) {
                    switch (summary.self_ownership) {
                        case ParamOwnership::BORROW_IMM: {
                            // Method takes $$i self — auto-borrow obj immutably
                            // Check: must not be already mutably borrowed
                            auto loans_it = ctx.active_loans.find(obj_name);
                            if (loans_it != ctx.active_loans.end()) {
                                for (const auto& loan : loans_it->second) {
                                    if (loan.is_mutable && loan.isActive()) {
                                        addError("Cannot call method '" + methodName +
                                                 "' on '" + obj_name +
                                                 "' — it is currently mutably borrowed by '" +
                                                 loan.borrower + "'",
                                                 expr,
                                                 "Mutable borrow was created here",
                                                 loan.creation_line, loan.creation_column);
                                        tagCode("ARIA-020");
                                    }
                                }
                            }
                            break;
                        }
                        case ParamOwnership::BORROW_MUT: {
                            // Method takes $$m self — auto-borrow obj mutably
                            // Check: must not have ANY active borrows
                            auto loans_it = ctx.active_loans.find(obj_name);
                            if (loans_it != ctx.active_loans.end() &&
                                !loans_it->second.empty()) {
                                const Loan& loan = loans_it->second.front();
                                addError("Cannot call mutating method '" + methodName +
                                         "' on '" + obj_name +
                                         "' — it is currently borrowed by '" +
                                         loan.borrower + "'",
                                         expr,
                                         "Active borrow was created here",
                                         loan.creation_line, loan.creation_column);
                                tagCode("ARIA-020");
                            }
                            // Check: must not be already moved
                            if (ctx.moved_variables.count(obj_name)) {
                                addError("Cannot call method '" + methodName +
                                         "' on '" + obj_name + "' — it has been moved",
                                         expr);
                                tagCode("ARIA-019");
                            }
                            break;
                        }
                        case ParamOwnership::MOVE: {
                            // Method takes owned self — consume the object
                            if (ctx.moved_variables.count(obj_name)) {
                                addError("Cannot call consuming method '" + methodName +
                                         "' on '" + obj_name + "' — it has already been moved",
                                         expr);
                                tagCode("ARIA-019");
                            }
                            auto loans_it = ctx.active_loans.find(obj_name);
                            if (loans_it != ctx.active_loans.end() &&
                                !loans_it->second.empty()) {
                                const Loan& loan = loans_it->second.front();
                                addError("Cannot consume '" + obj_name +
                                         "' in method '" + methodName +
                                         "' — it is currently borrowed by '" +
                                         loan.borrower + "'",
                                         expr,
                                         "Active borrow was created here",
                                         loan.creation_line, loan.creation_column);
                                tagCode("ARIA-019");
                            }
                            ctx.moved_variables.insert(obj_name);
                            break;
                        }
                        default:
                            break;
                    }
                }

                // Also check non-self arguments via normal ownership checking
                checkCallOwnership(expr, summary);
                break;  // Found matching method
            }
        }
    }
}

void BorrowChecker::checkMoveExpr(MoveExpr* expr) {
    if (!expr) return;

    const std::string& varName = expr->variableName;

    // Check if the variable can be used (not already moved or freed)
    if (ctx.moved_variables.find(varName) != ctx.moved_variables.end()) {
        addErrorWithSuggestion("Cannot move variable '" + varName + "' (already moved)", expr,
                "hint: a variable can only be moved once — clone it before the first move if needed");
        tagCode("ARIA-019");
        return;
    }

    // ARIA-016: Check for active pins - cannot move pinned objects
    // Pinning locks the object's memory address for FFI/DMA stability
    if (isPinned(varName)) {
        addErrorWithSuggestion("Cannot move variable '" + varName +
                 "' because it is currently pinned. Pinned objects must remain "
                 "at a stable address while wild pointers reference them.", expr,
                 "hint: unpin the variable before moving, or work with a copy");
        tagCode("ARIA-016");
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
        tagCode("ARIA-019");
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

void BorrowChecker::tagCode(const std::string& code) {
    if (!errors.empty()) {
        errors.back().code = code;
    }
}

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
                    base.index_exprs.push_back(index->index.get());
                } else {
                    // Non-integer literal - use placeholder
                    base.fields.push_back("[*]");
                    base.index_exprs.push_back(index->index.get());
                }
            } else {
                // Non-constant index - use placeholder, store expr for Z3
                base.fields.push_back("[*]");
                base.index_exprs.push_back(index->index ? index->index.get() : nullptr);
            }
            return base;
        }

        case ASTNode::NodeType::UNARY_OP: {
            // Handle dereference/address operators that preserve the base path.
            auto* unary = static_cast<UnaryExpr*>(expr);
            if (unary->op.type == frontend::TokenType::TOKEN_STAR ||
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
            tagCode("ARIA-016");
            return false;
        }
    }

    const Loan* conflict = ctx.checkPathConflict(path, is_mutable);
    if (conflict) {
        if (is_mutable) {
            addError("Cannot borrow '" + path.toString() + "' as mutable because it conflicts with existing borrow",
                    node,
                    "Existing " + std::string(conflict->is_mutable ? "mutable" : "immutable") +
                    " borrow by '" + conflict->borrower + "' of '" + conflict->path.toString() + "'",
                    conflict->creation_line, conflict->creation_column);
            tagCode("ARIA-023");
        } else {
            addError("Cannot borrow '" + path.toString() + "' as immutable because it is already borrowed as mutable",
                    node,
                    "Mutable borrow by '" + conflict->borrower + "' of '" + conflict->path.toString() + "'",
                    conflict->creation_line, conflict->creation_column);
            tagCode("ARIA-023");
        }
        return false;
    }

    return true;
}

// v0.6.4: --borrow-dump visualization
void BorrowChecker::dumpBorrowState(std::ostream& out, const std::string& func_name) const {
    out << "=== Borrow State Dump";
    if (!func_name.empty()) {
        out << " [" << func_name << "]";
    }
    out << " ===\n";

    // Active loans
    if (!ctx.active_loans.empty()) {
        out << "Active Loans:\n";
        for (const auto& [host, loans] : ctx.active_loans) {
            for (const auto& loan : loans) {
                out << "  " << host << " -> " << loan.borrower
                    << " (" << (loan.is_mutable ? "mut" : "immut") << ")"
                    << " [line " << loan.creation_line << "]\n";
            }
        }
        out << "\n";
    }

    // Path-based loans
    if (!ctx.path_loans.empty()) {
        out << "Path Loans:\n";
        for (const auto& [path, loans] : ctx.path_loans) {
            for (const auto& loan : loans) {
                out << "  " << path.toString() << " -> " << loan.borrower
                    << " (" << (loan.is_mutable ? "mut" : "immut") << ")"
                    << " [line " << loan.creation_line << "]\n";
            }
        }
        out << "\n";
    }

    // Pinned variables
    if (!ctx.active_pins.empty()) {
        out << "Pinned Variables:\n";
        for (const auto& [host, pin_ref] : ctx.active_pins) {
            out << "  " << host << " pinned by " << pin_ref << "\n";
        }
        out << "\n";
    }

    // Wild pointer states
    if (!ctx.wild_states.empty()) {
        out << "Wild Pointer States:\n";
        for (const auto& [var, state] : ctx.wild_states) {
            out << "  " << var << " = ";
            switch (state) {
                case WildState::ALLOCATED: out << "ALLOCATED"; break;
                case WildState::FREED: out << "FREED"; break;
                case WildState::MOVED: out << "MOVED"; break;
                case WildState::UNINITIALIZED: out << "UNINITIALIZED"; break;
                case WildState::UNKNOWN: out << "UNKNOWN"; break;
                case WildState::MAY_FREED: out << "MAY_FREED"; break;
            }
            auto size_it = ctx.wild_alloc_sizes.find(var);
            if (size_it != ctx.wild_alloc_sizes.end()) {
                out << " (size: " << size_it->second << ")";
            }
            out << "\n";
        }
        out << "\n";
    }

    // Moved variables
    if (!ctx.moved_variables.empty()) {
        out << "Moved Variables:\n";
        for (const auto& var : ctx.moved_variables) {
            out << "  " << var << "\n";
        }
        out << "\n";
    }

    // Pending wild frees (leak candidates)
    if (!ctx.pending_wild_frees.empty()) {
        out << "Pending Wild Frees (leak candidates):\n";
        for (const auto& var : ctx.pending_wild_frees) {
            out << "  " << var << "\n";
        }
        out << "\n";
    }

    // Loan origins
    if (!ctx.loan_origins.empty()) {
        out << "Loan Origins:\n";
        for (const auto& [ref, origins] : ctx.loan_origins) {
            out << "  " << ref << " <- {";
            bool first = true;
            for (const auto& origin : origins) {
                if (!first) out << ", ";
                out << origin;
                first = false;
            }
            out << "}\n";
        }
        out << "\n";
    }

    // Variable depths
    if (!ctx.var_depths.empty()) {
        out << "Variable Depths:\n";
        for (const auto& [var, depth] : ctx.var_depths) {
            out << "  " << var << " @ depth " << depth << "\n";
        }
        out << "\n";
    }

    out << "=== End Borrow Dump ===\n";
}

} // namespace sema
} // namespace npk

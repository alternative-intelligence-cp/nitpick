#include "lifetime_context.h"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace aria {

// ============================================================================
// Constructor
// ============================================================================

LifetimeContext::LifetimeContext()
    : current_depth_(0), next_loan_id_(1) {
    // Start with global scope (depth 0)
    scope_stack_.emplace_back(0);
}

// ============================================================================
// Scope Management
// ============================================================================

void LifetimeContext::enter_scope() {
    current_depth_++;
    scope_stack_.emplace_back(current_depth_);
}

void LifetimeContext::exit_scope() {
    if (scope_stack_.empty()) {
        std::cerr << "ERROR: Attempting to exit scope with empty stack!\n";
        return;
    }
    
    // End all borrows for variables leaving scope
    end_borrows_for_scope(current_depth_);
    
    // Remove variables declared in this scope from global tracking
    Scope& exiting_scope = scope_stack_.back();
    for (const auto& [name, var_info] : exiting_scope.variables) {
        global_var_map_.erase(name);
    }
    
    scope_stack_.pop_back();
    current_depth_--;
}

Scope& LifetimeContext::current_scope() {
    if (scope_stack_.empty()) {
        throw std::runtime_error("No current scope!");
    }
    return scope_stack_.back();
}

const Scope& LifetimeContext::current_scope() const {
    if (scope_stack_.empty()) {
        throw std::runtime_error("No current scope!");
    }
    return scope_stack_.back();
}

// ============================================================================
// Variable Management
// ============================================================================

VarInfo* LifetimeContext::declare_variable(const std::string& name,
                                           MemoryRegion region,
                                           VarDecl* decl) {
    // Check for shadowing in same scope
    Scope& scope = current_scope();
    if (scope.variables.find(name) != scope.variables.end()) {
        std::cerr << "WARNING: Variable '" << name << "' shadowed in same scope\n";
    }
    
    // Create variable info
    auto result = scope.variables.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(name),
        std::forward_as_tuple(name, current_depth_, region, decl)
    );
    
    VarInfo* var_info = &result.first->second;
    
    // Add to global tracking
    global_var_map_[name] = var_info;
    
    return var_info;
}

void LifetimeContext::initialize_variable(const std::string& name) {
    VarInfo* var = lookup_variable(name);
    if (!var) {
        std::cerr << "ERROR: Cannot initialize unknown variable '" << name << "'\n";
        return;
    }
    
    var->state = VarState::INITIALIZED;
}

void LifetimeContext::move_variable(const std::string& name) {
    VarInfo* var = lookup_variable(name);
    if (!var) {
        std::cerr << "ERROR: Cannot move unknown variable '" << name << "'\n";
        return;
    }
    
    // For wild types, move invalidates the variable
    // For GC types, move is a copy (no invalidation)
    if (var->region == MemoryRegion::WILD_HEAP) {
        var->state = VarState::MOVED;
        current_scope().moved_vars.insert(name);
    }
}

VarInfo* LifetimeContext::lookup_variable(const std::string& name) {
    auto it = global_var_map_.find(name);
    return (it != global_var_map_.end()) ? it->second : nullptr;
}

const VarInfo* LifetimeContext::lookup_variable(const std::string& name) const {
    auto it = global_var_map_.find(name);
    return (it != global_var_map_.end()) ? it->second : nullptr;
}

bool LifetimeContext::variable_exists(const std::string& name) const {
    return lookup_variable(name) != nullptr;
}

// ============================================================================
// Borrow Tracking (Core Appendage Theory)
// ============================================================================

bool LifetimeContext::create_borrow(const std::string& var_name,
                                   BorrowKind kind,
                                   const std::string& ref_var_name,
                                   int ref_scope_depth,
                                   ASTNode* borrow_site) {
    VarInfo* var = lookup_variable(var_name);
    if (!var) {
        std::cerr << "ERROR: Cannot borrow unknown variable '" << var_name << "'\n";
        return false;
    }
    
    // Check variable is in valid state
    if (!is_valid_for_borrow(var_name)) {
        return false;
    }
    
    // RULE 1: Check Appendage Inequality - Depth(Host) ≤ Depth(Reference)
    if (!validate_depth_inequality(var->scope_depth, ref_scope_depth)) {
        std::cerr << "ERROR: Appendage Inequality violated!\n";
        std::cerr << "  Host '" << var_name << "' at depth " << var->scope_depth << "\n";
        std::cerr << "  Reference '" << ref_var_name << "' at depth " << ref_scope_depth << "\n";
        std::cerr << "  Requirement: Depth(Host) <= Depth(Reference)\n";
        return false;
    }
    
    // RULE 2: Check Mutability XOR Aliasing
    if (kind == BorrowKind::MUTABLE) {
        if (!can_borrow_mutably(var_name)) {
            std::cerr << get_borrow_error_message(var_name, kind) << "\n";
            return false;
        }
        var->state = VarState::BORROWED_MUT;
    } else {
        if (!can_borrow_immutably(var_name)) {
            std::cerr << get_borrow_error_message(var_name, kind) << "\n";
            return false;
        }
        if (var->state == VarState::INITIALIZED) {
            var->state = VarState::BORROWED_IMM;
        }
    }
    
    // Create the loan
    var->active_loans.emplace_back(
        var_name, kind, current_depth_, ref_scope_depth, borrow_site, ref_var_name
    );
    
    return true;
}

void LifetimeContext::end_borrow(const std::string& var_name,
                                const std::string& ref_var_name) {
    VarInfo* var = lookup_variable(var_name);
    if (!var) {
        return; // Variable already out of scope
    }
    
    // Remove the loan
    auto& loans = var->active_loans;
    loans.erase(
        std::remove_if(loans.begin(), loans.end(),
            [&ref_var_name](const Loan& loan) {
                return loan.ref_var_name == ref_var_name;
            }),
        loans.end()
    );
    
    // Update state if no more borrows
    if (loans.empty() && (var->state == VarState::BORROWED_IMM ||
                         var->state == VarState::BORROWED_MUT)) {
        var->state = VarState::INITIALIZED;
    }
}

void LifetimeContext::end_borrows_for_scope(int scope_depth) {
    // Find all borrows where the reference is at this scope depth
    for (auto& [name, var_info] : global_var_map_) {
        auto& loans = var_info->active_loans;
        
        // Remove loans where reference is leaving scope
        loans.erase(
            std::remove_if(loans.begin(), loans.end(),
                [scope_depth](const Loan& loan) {
                    return loan.ref_scope_depth == scope_depth;
                }),
            loans.end()
        );
        
        // Update state if no more borrows
        if (loans.empty() && (var_info->state == VarState::BORROWED_IMM ||
                             var_info->state == VarState::BORROWED_MUT)) {
            var_info->state = VarState::INITIALIZED;
        }
    }
}

bool LifetimeContext::can_borrow_mutably(const std::string& var_name) const {
    const VarInfo* var = lookup_variable(var_name);
    if (!var) return false;
    
    // Cannot borrow mutably if ANY borrows exist
    return var->active_loans.empty();
}

bool LifetimeContext::can_borrow_immutably(const std::string& var_name) const {
    const VarInfo* var = lookup_variable(var_name);
    if (!var) return false;
    
    // Cannot borrow immutably if mutable borrow exists
    return !var->has_mutable_borrow();
}

const std::vector<Loan>& LifetimeContext::get_active_loans(const std::string& var_name) const {
    static std::vector<Loan> empty;
    const VarInfo* var = lookup_variable(var_name);
    return var ? var->active_loans : empty;
}

// ============================================================================
// Pinning Operations (# operator)
// ============================================================================

bool LifetimeContext::pin_variable(const std::string& var_name) {
    VarInfo* var = lookup_variable(var_name);
    if (!var) {
        std::cerr << "ERROR: Cannot pin unknown variable '" << var_name << "'\n";
        return false;
    }
    
    // Can only pin GC objects
    if (var->region != MemoryRegion::GC_HEAP) {
        std::cerr << "ERROR: Can only pin GC heap objects ('" << var_name
                  << "' is not GC)\n";
        return false;
    }
    
    if (var->is_pinned) {
        std::cerr << "WARNING: Variable '" << var_name << "' already pinned\n";
    }
    
    var->is_pinned = true;
    return true;
}

void LifetimeContext::unpin_variable(const std::string& var_name) {
    VarInfo* var = lookup_variable(var_name);
    if (var) {
        var->is_pinned = false;
    }
}

bool LifetimeContext::is_pinned(const std::string& var_name) const {
    const VarInfo* var = lookup_variable(var_name);
    return var && var->is_pinned;
}

// ============================================================================
// Validation Helpers
// ============================================================================

bool LifetimeContext::validate_depth_inequality(int host_depth, int ref_depth) const {
    // Appendage Inequality: Depth(Host) ≤ Depth(Reference)
    // Host must be in outer or equal scope compared to reference
    return host_depth <= ref_depth;
}

bool LifetimeContext::is_valid_for_use(const std::string& var_name) const {
    const VarInfo* var = lookup_variable(var_name);
    if (!var) return false;
    
    // Cannot use if moved or uninitialized
    return var->state != VarState::MOVED &&
           var->state != VarState::UNINITIALIZED;
}

bool LifetimeContext::is_valid_for_borrow(const std::string& var_name) const {
    const VarInfo* var = lookup_variable(var_name);
    if (!var) return false;
    
    // Cannot borrow if moved or uninitialized
    if (var->state == VarState::MOVED) {
        std::cerr << "ERROR: Cannot borrow moved variable '" << var_name << "'\n";
        return false;
    }
    if (var->state == VarState::UNINITIALIZED) {
        std::cerr << "ERROR: Cannot borrow uninitialized variable '" << var_name << "'\n";
        return false;
    }
    
    return true;
}

// ============================================================================
// Debugging and Diagnostics
// ============================================================================

void LifetimeContext::dump_state() const {
    std::cout << "\n=== Lifetime Context State ===\n";
    std::cout << "Current depth: " << current_depth_ << "\n";
    std::cout << "Scope stack size: " << scope_stack_.size() << "\n\n";
    
    for (int i = static_cast<int>(scope_stack_.size()) - 1; i >= 0; i--) {
        const Scope& scope = scope_stack_[i];
        std::cout << "Scope depth " << scope.depth << ":\n";
        
        if (scope.variables.empty()) {
            std::cout << "  (no variables)\n";
        } else {
            for (const auto& [name, var] : scope.variables) {
                std::cout << "  " << name << ": ";
                
                // Memory region
                switch (var.region) {
                    case MemoryRegion::STACK: std::cout << "stack"; break;
                    case MemoryRegion::GC_HEAP: std::cout << "gc"; break;
                    case MemoryRegion::WILD_HEAP: std::cout << "wild"; break;
                    case MemoryRegion::UNKNOWN: std::cout << "unknown"; break;
                }
                
                // State
                std::cout << ", state=";
                switch (var.state) {
                    case VarState::UNINITIALIZED: std::cout << "uninit"; break;
                    case VarState::INITIALIZED: std::cout << "init"; break;
                    case VarState::MOVED: std::cout << "moved"; break;
                    case VarState::BORROWED_IMM: std::cout << "borrowed(imm)"; break;
                    case VarState::BORROWED_MUT: std::cout << "borrowed(mut)"; break;
                }
                
                // Pinned
                if (var.is_pinned) {
                    std::cout << ", PINNED";
                }
                
                // Active loans
                if (!var.active_loans.empty()) {
                    std::cout << ", loans=" << var.active_loans.size();
                }
                
                std::cout << "\n";
            }
        }
        std::cout << "\n";
    }
}

std::string LifetimeContext::get_borrow_error_message(const std::string& var_name,
                                                      BorrowKind attempted_kind) const {
    const VarInfo* var = lookup_variable(var_name);
    if (!var) {
        return "ERROR: Unknown variable '" + var_name + "'";
    }
    
    std::ostringstream oss;
    oss << "ERROR: Cannot borrow '" << var_name << "' as ";
    oss << (attempted_kind == BorrowKind::MUTABLE ? "mutable" : "immutable");
    oss << "\n";
    
    if (var->has_mutable_borrow()) {
        oss << "  Variable already has a mutable borrow\n";
    } else if (var->has_immutable_borrows()) {
        oss << "  Variable already has " << var->active_borrow_count()
            << " immutable borrow(s)\n";
    }
    
    // Show existing borrows
    if (!var->active_loans.empty()) {
        oss << "  Existing borrows:\n";
        for (const auto& loan : var->active_loans) {
            oss << "    - " << (loan.kind == BorrowKind::MUTABLE ? "mutable" : "immutable")
                << " borrow by '" << loan.ref_var_name << "' at depth " << loan.ref_scope_depth << "\n";
        }
    }
    
    return oss.str();
}

// ============================================================================
// Wild Memory Leak Detection (Rule 2)
// ============================================================================

void LifetimeContext::track_wild_allocation(const std::string& var_name) {
    pending_wild_frees_.insert(var_name);
}

void LifetimeContext::track_wild_free(const std::string& var_name) {
    // Remove from pending (no longer needs free)
    pending_wild_frees_.erase(var_name);
    
    // Add to freed set (for use-after-free detection)
    freed_wild_vars_.insert(var_name);
}

bool LifetimeContext::is_freed(const std::string& var_name) const {
    return freed_wild_vars_.find(var_name) != freed_wild_vars_.end();
}

std::vector<std::string> LifetimeContext::get_pending_wild_frees() const {
    return std::vector<std::string>(pending_wild_frees_.begin(), pending_wild_frees_.end());
}

void LifetimeContext::clear_pending_wild_frees_for_scope() {
    // This would need more sophisticated tracking per scope
    // For now, just clear all (assumes defer was used properly)
    pending_wild_frees_.clear();
}

} // namespace aria

# Borrow Checker Implementation Plan
**Based on**: research_001_borrow_checker.txt (407 lines)  
**Date**: December 11, 2025  
**Status**: Research complete, ready for Phase 2 implementation  
**Timeline**: 8-10 weeks

---

## Executive Summary

The Aria Borrow Checker (ABC) implements a **hybrid memory safety model** that bridges Rust-style ownership with garbage collection. The core innovation is **"Appendage Theory"** - a formal framework ensuring that references (Appendages) never outlive their referents (Hosts).

### Key Features

1. **Hybrid Memory Model**: Three memory regions (stack, GC heap, wild heap) with safe interaction
2. **Scope Depth Analysis**: Mathematical proof via depth comparison (Depth(Host) ≤ Depth(Ref))
3. **Pinning Bridge**: `#` operator allows wild pointers to safely reference GC objects
4. **Flow-Sensitive Checking**: Tracks variable state through branches, loops, and control flow
5. **Zero Runtime Cost**: All checks at compile time (Sema phase)

---

## Memory Model Overview

### Three Memory Regions

| Region | Keyword | Allocation | Deallocation | Stability | Use Case |
|--------|---------|------------|--------------|-----------|----------|
| Stack | `stack` | Automatic (lexical) | Block exit | Fixed | Local variables |
| GC Heap | (default) | `aria.gc_alloc` | GC sweep | **Moving** | Objects, strings |
| Wild Heap | `wild` | `aria.alloc` | `aria.free` / `defer` | Fixed | Performance-critical |

### Critical Operators

```aria
@   // Address-of: creates raw pointer (restricted)
$   // Safe reference: creates tracked borrow (Appendage)
#   // Pin: prevents GC movement, returns wild pointer
```

---

## Appendage Theory (Core Algorithm)

### Mathematical Foundation

**The Appendage Inequality**:
```
Depth(Host) ≤ Depth(Reference)
```

This ensures the Host is declared in an outer (or equal) scope to the Reference, preventing the classic "dangling pointer to dead stack variable" error.

### Example

```aria
func:outer = void() {                    // Depth 1
    int32:host = 42;                      // host: Depth 2
    
    if (condition) {                      // Enter Depth 3
        int32$:ref = $host;                // ref: Depth 3
        // ✅ Valid: Depth(host)=2 ≤ Depth(ref)=3
    }
    // ref dies at end of if block (Depth 3)
    // host still alive (Depth 2)
}

// ❌ INVALID EXAMPLE:
func:invalid = void() {                  // Depth 1
    int32$:ref;                           // ref: Depth 2
    
    if (condition) {                      // Enter Depth 3
        int32:inner_host = 42;             // inner_host: Depth 3
        ref = $inner_host;                 // ❌ ERROR!
        // Depth(inner_host)=3 > Depth(ref)=2
    }
    // inner_host dies, but ref (Depth 2) is still alive
    // ref now dangles!
}
```

---

## Borrow Checking Rules

### Rule 1: Mutability XOR Aliasing (RW-Lock)

**At any point, for a Host H**:
- **N** immutable references (`$H`), OR
- **1** mutable reference (`$mut H`), OR  
- **0** references

```aria
int32:x = 10;
int32$:r1 = $x;      // ✅ Immutable borrow 1
int32$:r2 = $x;      // ✅ Immutable borrow 2 (ok)
int32$:r3 = $mut x;  // ❌ ERROR: cannot borrow mutably while immutable borrows exist
```

### Rule 2: Wild Memory Hygiene

**Every allocation must be freed on all exit paths**:

```aria
wild int8*:ptr = aria.alloc<int8>(100);  // Tracked in pending_wild_frees
defer aria.free(ptr);                     // Removed from pending_wild_frees
// ✅ Valid: all paths free ptr

// ❌ INVALID:
if (condition) {
    wild int8*:leak = aria.alloc<int8>(50);
    // No defer, no free → ERROR: Memory leak
}
```

**Use-after-free detection**:

```aria
wild int8*:ptr = aria.alloc<int8>(100);
aria.free(ptr);       // ptr transitions to "Dead" state
int8:val = *ptr;      // ❌ ERROR: Use-after-free
```

### Rule 3: The Pinning Contract

**A pinned object cannot be moved, reassigned, or mutated while the pin is alive**:

```aria
string:data = "hello";
wild int8*:ptr = #data;   // data is now PINNED_BY(ptr)

data = "world";            // ❌ ERROR: Cannot assign to pinned variable 'data'

// ptr goes out of scope → pin released → data can be reassigned
```

### Rule 4: Wild/GC Boundary

**Wild memory cannot hold unpinned GC references** (GC won't scan wild memory):

```aria
struct:Node = {
    wild int32*:next;  // Wild pointer field
};

gc string:s = "data";
wild Node*:node = aria.alloc<Node>(1);

node->next = s;            // ❌ ERROR: Cannot store unpinned GC reference in wild memory

// ✅ VALID with pinning:
wild string*:pinned_s = #s;
node->next = pinned_s;     // Now safe (s is pinned)
```

---

## Implementation Architecture

### Phase 2.1: Data Structures (2 weeks)

#### LifetimeContext

```cpp
struct LifetimeContext {
    // Maps variable name -> declaration scope depth
    std::unordered_map<std::string, int> var_depths;
    
    // Maps reference -> set of possible hosts (for phi nodes)
    std::unordered_map<std::string, std::set<std::string>> loan_origins;
    
    // Maps host -> list of active loans (enforces RW-Lock)
    std::unordered_map<std::string, std::vector<Loan>> active_loans;
    
    // Tracks variables currently pinned by # operator
    // Key: Host variable, Value: Pinning reference name
    std::unordered_map<std::string, std::string> active_pins;
    
    // Tracks wild allocations requiring cleanup (leak detection)
    std::unordered_set<std::string> pending_wild_frees;
    
    // Current traversal depth
    int current_depth = 0;
    
    // Methods
    void enterScope();
    void exitScope();
    LifetimeContext snapshot();
    void restore(const LifetimeContext& state);
    void merge(const LifetimeContext& then_state, const LifetimeContext& else_state);
};

struct Loan {
    std::string borrower;  // Name of reference variable
    bool is_mutable;       // Type of borrow ($ vs $mut)
    int creation_line;     // For error reporting
};
```

#### AST Annotations

Extend existing AST nodes:

```cpp
// In src/frontend/ast.h

class VarDecl : public Statement {
public:
    // ... existing fields ...
    
    // NEW: Lifetime tracking
    int scope_depth = -1;           // Assigned by borrow checker
    bool requires_drop = false;      // true if wild/stack needs cleanup
    bool is_pinned_shadow = false;   // true if this var pins another
    std::string pinned_target;       // Name of variable this one pins
};

class UnaryOp : public Expression {
public:
    // ... existing fields ...
    
    // NEW: Borrow tracking
    bool creates_loan = false;       // true for $, #, @
    std::string loan_target;         // The variable being borrowed
};
```

### Phase 2.2: Core Algorithm (3 weeks)

#### Scope Tracking

```cpp
void BorrowChecker::enterScope() {
    ctx.current_depth++;
    scope_stack.push({});  // New scope frame
}

void BorrowChecker::exitScope() {
    // 1. Identify dying variables at current depth
    auto& dying = scope_stack.top();
    
    // 2. Leak check
    for (auto& var : dying) {
        if (ctx.pending_wild_frees.count(var)) {
            reportError("Memory leak: wild variable '" + var + "' not freed");
        }
    }
    
    // 3. Dangling reference check
    for (auto& [ref, origins] : ctx.loan_origins) {
        int ref_depth = ctx.var_depths[ref];
        
        // If ref is in outer scope but depends on dying variable
        if (ref_depth < ctx.current_depth) {
            for (auto& origin : origins) {
                if (dying.count(origin)) {
                    reportError("Reference '" + ref + "' outlives host '" + origin + "'");
                }
            }
        }
    }
    
    // 4. Release pins
    for (auto& var : dying) {
        if (ctx.active_pins.count(var)) {
            std::string host = ctx.active_pins[var];
            // Remove pin status from host
            ctx.active_pins.erase(var);
        }
    }
    
    // 5. Cleanup
    for (auto& var : dying) {
        ctx.var_depths.erase(var);
        ctx.loan_origins.erase(var);
        ctx.active_loans.erase(var);
    }
    
    scope_stack.pop();
    ctx.current_depth--;
}
```

#### Variable Declaration

```cpp
void BorrowChecker::visit(VarDecl* node) {
    // 1. Register variable
    ctx.var_depths[node->name] = ctx.current_depth;
    scope_stack.top().insert(node->name);
    
    // 2. Track wild allocations
    if (node->type->isWild()) {
        ctx.pending_wild_frees.insert(node->name);
    }
    
    // 3. Analyze initializer
    if (node->initializer) {
        checkExpression(node->initializer.get());
        
        // If initializer is a borrow ($x or #x)
        if (auto* unary = dynamic_cast<UnaryOp*>(node->initializer.get())) {
            if (unary->op == "$" || unary->op == "#") {
                std::string host = extractHostVariable(unary);
                
                // APPENDAGE INEQUALITY CHECK
                int host_depth = ctx.var_depths[host];
                if (host_depth > ctx.current_depth) {
                    reportError("Host '" + host + "' declared in inner scope than reference '" + node->name + "'");
                }
                
                // Record loan
                ctx.loan_origins[node->name].insert(host);
                
                // If pinning
                if (unary->op == "#") {
                    if (ctx.active_pins.count(host)) {
                        reportError("Variable '" + host + "' is already pinned");
                    }
                    ctx.active_pins[host] = node->name;
                    node->is_pinned_shadow = true;
                    node->pinned_target = host;
                }
                
                // If borrowing
                if (unary->op == "$") {
                    Loan loan{node->name, unary->is_mutable, node->line};
                    
                    // MUTABILITY XOR ALIASING CHECK
                    auto& loans = ctx.active_loans[host];
                    if (unary->is_mutable) {
                        if (!loans.empty()) {
                            reportError("Cannot borrow '" + host + "' mutably while other borrows exist");
                        }
                    } else {
                        for (auto& existing : loans) {
                            if (existing.is_mutable) {
                                reportError("Cannot borrow '" + host + "' immutably while mutable borrow exists");
                            }
                        }
                    }
                    
                    loans.push_back(loan);
                }
            }
        }
    }
    
    // 4. Annotate AST
    node->scope_depth = ctx.current_depth;
}
```

### Phase 2.3: Control Flow (2 weeks)

#### If Statements

```cpp
void BorrowChecker::visit(IfStmt* node) {
    // 1. Check condition
    checkExpression(node->condition.get());
    
    // 2. Snapshot pre-branch state
    auto pre_branch = ctx.snapshot();
    
    // 3. Analyze THEN branch
    ctx.enterScope();
    node->then_block->accept(*this);
    ctx.exitScope();
    auto then_state = ctx.snapshot();
    
    // 4. Restore and analyze ELSE branch
    ctx.restore(pre_branch);
    if (node->else_block) {
        ctx.enterScope();
        node->else_block->accept(*this);
        ctx.exitScope();
    }
    auto else_state = ctx.snapshot();
    
    // 5. Merge states (conservative)
    // Variable is freed only if freed in BOTH branches
    // Variable is invalidated if invalidated in EITHER branch
    ctx.merge(then_state, else_state);
}
```

#### While Loops

```cpp
void BorrowChecker::visit(WhileLoop* node) {
    checkExpression(node->condition.get());
    
    ctx.enterScope();  // Loop scope
    
    // Mark external borrows as locked during loop
    auto external_borrows = getExternalBorrows();
    
    // Analyze body
    node->body->accept(*this);
    
    // Loop invariant validation
    // Check if any reference created in loop escapes but its referent doesn't
    for (auto& [ref, origins] : ctx.loan_origins) {
        int ref_depth = ctx.var_depths[ref];
        
        // If ref escapes to outer scope
        if (ref_depth < ctx.current_depth) {
            for (auto& origin : origins) {
                int origin_depth = ctx.var_depths[origin];
                
                // If origin is local to loop (will die)
                if (origin_depth >= ctx.current_depth) {
                    reportError("Reference '" + ref + "' escapes loop but referent '" + origin + "' does not");
                }
            }
        }
    }
    
    ctx.exitScope();
}
```

### Phase 2.4: Special Cases (1 week)

#### Defer Handling

```cpp
void BorrowChecker::visit(DeferStmt* node) {
    // Analyze deferred statement for free() calls
    if (auto* call = extractFreeCall(node->statement.get())) {
        std::string freed_var = call->args[0];
        
        // Remove from leak tracking
        ctx.pending_wild_frees.erase(freed_var);
        
        // Mark as "freed" for use-after-free detection
        ctx.freed_vars.insert(freed_var);
    }
}
```

#### WildX (Executable Memory)

```cpp
void BorrowChecker::checkWildXWrite(const std::string& var) {
    if (ctx.wildx_state[var] == WildXState::Executable) {
        reportError("Cannot write to executable memory '" + var + "'");
    }
}
```

### Phase 2.5: Error Reporting (1 week)

#### Diagnostic Architecture

```cpp
struct BorrowError {
    SourceLocation location;      // Where error occurred
    std::string message;          // "Cannot move 'x'..."
    SourceLocation loan_loc;      // Where blocking loan started
    std::string loan_reason;      // "...because borrowed by 'r' here"
    std::vector<std::string> help_messages;
};

void BorrowChecker::reportError(const std::string& msg) {
    BorrowError err;
    err.location = current_location;
    err.message = msg;
    
    // Reconstruct chain of custody
    // ... add contextual information ...
    
    errors.push_back(err);
}
```

#### Example Error Message

```
Error: Pinning Violation
  --> src/main.aria:12:5
   |
10 | wild int8*:ptr = #data;
   |                  ----- 'data' is pinned here by reference 'ptr'
...
12 | data = "new value";
   | ^^^^ cannot assign to 'data' because it is currently pinned
   |
   = Help: The pin is released when 'ptr' goes out of scope at line 15.
```

### Phase 2.6: Testing (1 week)

#### Test Categories

1. **Basic Lifetime Tests** (`tests/borrow/test_basic_lifetime.cpp`)
   - Simple borrows
   - Scope depth violations
   - Reference outlives host

2. **Mutability Tests** (`tests/borrow/test_mutability.cpp`)
   - Multiple immutable borrows
   - Single mutable borrow
   - Mutable/immutable conflicts

3. **Wild Memory Tests** (`tests/borrow/test_wild_memory.cpp`)
   - Leak detection
   - Use-after-free
   - Double-free

4. **Pinning Tests** (`tests/borrow/test_pinning.cpp`)
   - Basic pinning
   - Pin violations (reassignment, move)
   - Multiple pins

5. **Control Flow Tests** (`tests/borrow/test_control_flow.cpp`)
   - If/else merging
   - Loop invariants
   - Early returns

6. **Wild/GC Boundary Tests** (`tests/borrow/test_wild_gc.cpp`)
   - Storing GC refs in wild memory
   - Pinned GC refs in wild memory

---

## Compiler Pipeline Integration

### Current Pipeline

1. Lexer → Tokens
2. Parser → AST
3. Type Checker → Type-annotated AST
4. ⭐ **[NEW] Borrow Checker** → Lifetime-validated AST
5. Codegen → LLVM IR

### Placement: Phase 5b (Semantic Analysis)

The borrow checker runs **after type checking** but **before codegen**. It acts as a gatekeeper - invalid code never reaches LLVM.

**Input**: Type-annotated AST  
**Output**: Success or list of BorrowErrors  
**Side Effect**: Annotates AST with lifetime metadata for optimization

---

## Comparison: Aria vs Rust

| Feature | Rust (NLL/Polonius) | Aria (Appendage Theory) |
|---------|---------------------|-------------------------|
| Analysis Basis | Control Flow Graph (MIR) | AST + Scope Depth |
| GC Interaction | None (pure manual/RAII) | Core feature (Pinning) |
| Pinning | Type-based (`Pin<P>`) | Operator-based (`#var`) |
| Wild Memory | `unsafe` raw pointers | First-class `wild` keyword |
| Complexity | High (lifetime inference) | Medium (depth + rules) |

**Aria's Pragmatic Approach**: Less theoretically pure than Rust's Polonius, but more practical for a GC language. Explicit scope depth simplifies the mental model while maintaining strong safety.

---

## Implementation Timeline

### Total: 10 weeks

| Phase | Duration | Description |
|-------|----------|-------------|
| 2.1 Data Structures | 2 weeks | LifetimeContext, AST annotations, error types |
| 2.2 Core Algorithm | 3 weeks | Scope tracking, Appendage Inequality, loan recording |
| 2.3 Control Flow | 2 weeks | If/else, loops, phi merging |
| 2.4 Special Cases | 1 week | Defer, WildX, closures |
| 2.5 Error Reporting | 1 week | Diagnostics, chain of custody |
| 2.6 Testing | 1 week | Comprehensive test suite (50+ tests) |

---

## Success Criteria

1. ✅ All basic lifetime violations caught at compile time
2. ✅ Mutability XOR Aliasing enforced (no data races)
3. ✅ Wild memory leaks detected (100% coverage)
4. ✅ Use-after-free prevented (flow-sensitive)
5. ✅ Pinning contract enforced (no GC violations)
6. ✅ Control flow merging correct (conservative safety)
7. ✅ Error messages actionable (show loan origin)
8. ✅ Zero false positives on Phase 1 test suite

---

## Dependencies

**Before Starting**:
- Phase 1 complete (all type systems) ✅
- Type checker functional (assigns types to AST nodes)

**Parallel Work**:
- Can proceed independently of Phase 3 (Module System)
- Does NOT depend on stdlib implementation
- research_007 (Threading) may inform concurrent borrow rules (future)

---

## Open Questions

1. **Closures**: How do closures capture variables? By-value or by-reference? Needs lifetime analysis.
2. **Async/Await**: Do async blocks extend lifetimes? May need special handling.
3. **Trait Objects**: Dynamic dispatch with borrows - needs investigation.
4. **Interior Mutability**: Should Aria have `Cell<T>`/`RefCell<T>` equivalents?

---

## References

- **Research Document**: `/docs/gemini/responses/research_001_borrow_checker.txt` (407 lines)
- **Aria Specification**: `/docs/info/aria_specs.txt` (memory model, operators)
- **Rust Borrow Checker**: [Rustc Dev Guide](https://rustc-dev-guide.rust-lang.org/borrow_check.html)

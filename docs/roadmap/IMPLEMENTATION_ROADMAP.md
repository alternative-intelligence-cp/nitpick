# ARIA COMPILER IMPLEMENTATION ROADMAP
**Date:** 2025-01-24  
**Based On:** research_016, research_019 specifications  
**Status:** Ready for Implementation

---

## PRIORITY LEVELS

- 🔴 **P0** - Critical path blockers (must complete for basic functionality)
- 🟡 **P1** - High value features (significant user impact)
- 🟢 **P2** - Nice to have (can defer)
- ⚪ **P3** - Future work (low priority)

---

## MILESTONE 1: RESULT TYPE FOUNDATION (Week 1-2)

### 🔴 P0.1: Result Type Parsing (2-3 days)
**Goal:** Parse `result<T, E>` type syntax

**Tasks:**
1. Add result keyword to lexer (if not present)
2. Extend type parser to handle `result<T, E>` with two type parameters
3. Create AST representation for parameterized Result types
4. Update type resolver to construct Result Type objects
5. Add tests for parsing result types in various contexts

**Files:**
- `src/frontend/lexer/lexer.cpp` - Add RESULT token
- `src/frontend/parser/type_parser.cpp` - Parse result<T,E>
- `src/frontend/ast/types.h` - ResultTypeRef AST node
- `tests/parser/test_result_types.aria` - Test cases

**Estimated:** 2-3 days  
**Blockers:** None  
**Dependency:** Type system (already complete)

---

### 🔴 P0.2: Result Struct Literal Syntax (1 day)
**Goal:** Parse `{ err: expr, val: expr }` literals

**Tasks:**
1. Extend struct literal parser to recognize err/val pattern
2. Add type checking to ensure literal matches result<T,E>
3. Generate appropriate Result construction code

**Files:**
- `src/frontend/parser/expr_parser.cpp` - Struct literals
- `src/frontend/sema/type_checker.cpp` - Result literal validation

**Estimated:** 1 day  
**Blockers:** None (can parallel with P0.1)

---

### 🔴 P0.3: Unwrap Operator AST (1 day)
**Goal:** Create AST node for ? operator

**Tasks:**
1. Add UnwrapExpr AST node class
2. Support two variants:
   - `expr?` - early return
   - `expr? default` - default coalescing
3. Parser integration for postfix ? operator
4. Type checking: ensure expr returns result<T,E>

**Files:**
- `src/frontend/ast/expr.h` - UnwrapExpr class
- `src/frontend/parser/expr_parser.cpp` - ? operator parsing
- `src/frontend/sema/type_checker.cpp` - Unwrap validation

**Estimated:** 1 day  
**Blockers:** None

---

### 🔴 P0.4: Unwrap Operator Codegen (1-2 days)
**Goal:** Generate LLVM IR for ? operator

**Tasks:**
1. Implement early return variant:
   - Check `result.err == 0`
   - Branch: success extracts val, failure returns result
2. Implement default coalescing:
   - Check `result.err == 0`
   - Branch: success extracts val, failure evaluates default
3. Optimize register usage for small Results
4. Add tests for both variants

**Files:**
- `src/backend/codegen/expr_codegen.cpp` - UnwrapExpr lowering
- `tests/codegen/test_unwrap_operator.aria` - Tests

**Estimated:** 1-2 days  
**Blockers:** P0.3  
**Critical:** Must implement ? for ergonomic error handling

---

### 🟡 P1.1: Result LLVM Layout Optimization (1 day)
**Goal:** Generate optimized result<T> structs

**Tasks:**
1. Implement size-specific layouts:
   - result<void> → i8
   - result<int64> → { i8, [7 x i8], i64 }
   - result<bool> → { i8, i8 }
2. Ensure register-friendly alignment
3. Test return value passing in registers

**Files:**
- `src/backend/codegen/type_lowering.cpp` - Result layout
- `src/backend/codegen/abi_lowering.cpp` - Register passing

**Estimated:** 1 day  
**Blockers:** P0.1  
**Benefit:** Performance optimization

---

## MILESTONE 2: CONDITIONAL OPERATORS (Week 2)

### 🔴 P0.5: is Ternary Operator (1 day)
**Goal:** Implement `is cond : true_val : false_val`

**Tasks:**
1. Add is keyword to lexer
2. Create TernaryExpr AST node
3. Parser for is expression
4. Type unification for both branches
5. Codegen with conditional branching

**Files:**
- `src/frontend/lexer/lexer.cpp` - is token
- `src/frontend/ast/expr.h` - TernaryExpr
- `src/frontend/parser/expr_parser.cpp` - is parsing
- `src/backend/codegen/expr_codegen.cpp` - is codegen

**Estimated:** 1 day  
**Blockers:** None  
**Value:** Ergonomic ternary for error handling

---

### 🟡 P1.2: TBB Sentinel Checking in Conditionals (2 days)
**Goal:** Ensure if statements check for ERR sentinel

**Tasks:**
1. Add is_err() utility function
2. Modify condition lowering to check sentinel first
3. Generate composite boolean: `!is_err(val) && is_true(val)`
4. Integrate with TBB arithmetic overflow checks
5. Update all conditional contexts (if, while, etc.)

**Files:**
- `src/backend/codegen/tbb_lowering.cpp` - is_err() implementation
- `src/backend/codegen/control_flow_codegen.cpp` - Conditional lowering
- `src/frontend/sema/type_checker.cpp` - TBB awareness

**Estimated:** 2 days  
**Blockers:** None  
**Benefit:** Prevents phantom branching from corrupted data

---

## MILESTONE 3: WHEN LOOP (Week 3)

### 🟡 P1.3: when Loop AST and Parser (1-2 days)
**Goal:** Parse when/then/end construct

**Tasks:**
1. Add when, then, end keywords to lexer
2. Create WhenLoop AST node with:
   - condition expr
   - body block
   - then_block (optional)
   - end_block (optional)
3. Parser logic for three-block structure
4. Semantic validation

**Files:**
- `src/frontend/lexer/lexer.cpp` - Keywords
- `src/frontend/ast/stmt.h` - WhenLoop class
- `src/frontend/parser/stmt_parser.cpp` - when parsing
- `src/frontend/sema/type_checker.cpp` - Loop validation

**Estimated:** 1-2 days  
**Blockers:** None  
**Value:** Unique Aria feature, well-specified

---

### 🟡 P1.4: when Loop State Machine Codegen (2 days)
**Goal:** Generate state machine with implicit flags

**Tasks:**
1. Implement lowering strategy:
   - `run_once` flag for initial execution
   - `broke_early` flag for break detection
2. Generate control flow graph:
   - Start → check condition
   - Body with break/continue handling
   - Then block (natural completion)
   - End block (never ran or broke)
3. Optimize flag elimination where possible
4. Comprehensive testing

**Files:**
- `src/backend/codegen/control_flow_codegen.cpp` - when lowering
- `tests/codegen/test_when_loop.aria` - Test suite

**Estimated:** 2 days  
**Blockers:** P1.3  
**Complexity:** State machine requires careful CFG construction

---

## MILESTONE 4: CLOSURE FOUNDATIONS (Week 4-5)

### 🟡 P1.5: Closure Environment Struct Generation (2-3 days)
**Goal:** Generate environment for captured variables

**Tasks:**
1. Capture analysis during semantic pass:
   - Identify captured variables
   - Determine capture mode (by-value, by-ref, by-move)
2. Synthesize environment struct:
   ```cpp
   struct ClosureEnv_<id> {
       T1 captured_var1;
       T2* captured_ref2;
   };
   ```
3. Allocate environment (stack only for now)
4. Apply Appendage Theory depth constraints

**Files:**
- `src/frontend/sema/closure_analyzer.cpp` - NEW: Capture analysis
- `src/backend/codegen/closure_codegen.cpp` - NEW: Environment generation
- `src/frontend/sema/borrow_checker.cpp` - Depth validation

**Estimated:** 2-3 days  
**Blockers:** None (heap closures require research_021)  
**Scope:** Stack closures only initially

---

### 🟡 P1.6: Func Fat Pointer Implementation (2 days)
**Goal:** Implement `{ method_ptr, env_ptr }` model

**Tasks:**
1. Generate FuncFatPtr struct for func variables
2. Implement calling convention:
   - Load method_ptr into temp register
   - Load env_ptr into dedicated register (R10/x18)
   - Call method_ptr
3. Static functions: env_ptr = NULL
4. Closures: env_ptr = &environment

**Files:**
- `src/backend/codegen/func_codegen.cpp` - NEW: Fat pointer handling
- `src/backend/codegen/call_codegen.cpp` - Modified calling convention

**Estimated:** 2 days  
**Blockers:** P1.5  
**Critical:** Unified func representation

---

## MILESTONE 5: ARRAY SLICING (Week 5-6)

### 🟢 P2.1: Slice Type and Syntax (2 days)
**Goal:** Implement `T` dynamic array type

**Tasks:**
1. Parse slice type `T`
2. Generate 3-word struct:
   ```cpp
   struct Slice_T {
       T* ptr;
       int64 len;
       int64 cap;
   };
   ```
3. Implement len and cap properties
4. Zero-copy semantics

**Files:**
- `src/frontend/parser/type_parser.cpp` - T syntax
- `src/backend/codegen/type_lowering.cpp` - Slice layout
- `src/backend/codegen/slice_codegen.cpp` - NEW: Operations

**Estimated:** 2 days  
**Blockers:** None  
**Benefit:** High-performance data structures

---

### 🟢 P2.2: Range Slicing Operators (1-2 days)
**Goal:** Implement `arr[2..5]` and `arr[2...5]`

**Tasks:**
1. Add .. and ... operators to lexer
2. Parse range expressions
3. Generate slicing code:
   - Bounds checking
   - Pointer offset: `new_ptr = ptr + start * sizeof(T)`
   - Length calculation: `new_len = end - start`
   - Capacity adjustment
4. Appendage Theory: slice lifetime ≤ source lifetime

**Files:**
- `src/frontend/lexer/lexer.cpp` - Range operators
- `src/frontend/parser/expr_parser.cpp` - Range parsing
- `src/backend/codegen/slice_codegen.cpp` - Slicing operations
- `src/frontend/sema/borrow_checker.cpp` - Lifetime validation

**Estimated:** 1-2 days  
**Blockers:** P2.1

---

## MILESTONE 6: PATTERN MATCHING (Week 6-7)

### 🟡 P1.7: pick Statement for Results (3-4 days)
**Goal:** Pattern match on result<T,E>

**Tasks:**
1. Extend existing pick implementation for Result destructuring
2. Parse `{ err: pattern, val: pattern }` patterns
3. Generate switch on err discriminator
4. Extract val field in success arm
5. Optimize with jump tables

**Files:**
- `src/frontend/parser/stmt_parser.cpp` - pick patterns
- `src/frontend/ast/pattern.h` - Result pattern nodes
- `src/backend/codegen/pattern_match_codegen.cpp` - Decision tree

**Estimated:** 3-4 days  
**Blockers:** P0.1 (Result type)  
**Value:** Ergonomic error handling + pattern matching

---

## FUTURE WORK (Post Phase 1)

### ⚪ P3.1: Heap Closures (requires research_021)
- Wild heap allocation for closures
- GC heap with Shadow Stack registration
- Reference counting for environments

### ⚪ P3.2: Async Functions (requires research_029)
- Coroutine frame layout
- State machine for await points
- M:N scheduler integration
- Spill slot management

### ⚪ P3.3: Comptime Functions (requires research_010)
- CTFE interpreter
- Virtual heap for comptime execution
- TBB emulation in software ALU

### ⚪ P3.4: Monadic Operations
- map(), flatMap(), and_then() for Result
- Standard library integration
- Composition operators

### ⚪ P3.5: Multi-Dimensional Arrays
- Tensor type with stride array
- SIMD integration
- Matrix operations

---

## IMPLEMENTATION TIMELINE

### Week 1-2: Result Foundation
- ✅ Type system (complete)
- 🔴 Parser + ? operator + is ternary
- 🔴 Basic codegen
**Milestone:** Can write and use result<T,E> types with error handling

### Week 3: Control Flow
- 🟡 when loop (unique feature)
- 🟡 TBB sentinel checking
**Milestone:** Complete conditional construct support

### Week 4-5: Closures (Stack Only)
- 🟡 Environment generation
- 🟡 Fat pointer model
- 🟡 Capture analysis
**Milestone:** Stack-allocated closures working

### Week 6-7: Arrays & Pattern Matching
- 🟢 Slice types + operations
- 🟡 pick for Results
**Milestone:** High-performance data structures + ergonomic error handling

### Week 8+: Advanced Features
- ⚪ Heap closures (needs GC)
- ⚪ Async functions
- ⚪ Monadic operations

---

## DEPENDENCIES

### External Research:
- ✅ research_001 (Borrow Checker) - Appendage Theory
- ✅ research_016 (Functional Types) - Complete spec
- ✅ research_019 (Conditional Constructs) - Control flow
- ⏳ research_021 (GC System) - Heap closures
- ⏳ research_029 (Async/Await) - Async functions

### Internal Dependencies:
```
Type System (✅)
    ↓
Parser (Week 1-2)
    ↓
Codegen (Week 1-7)
    ├─→ Result + ? operator (P0) → pick matching (P1)
    ├─→ is ternary (P0)
    ├─→ when loop (P1)
    ├─→ Closures (P1) → Heap closures (P3)
    └─→ Array slicing (P2)
```

---

## TESTING STRATEGY

### Unit Tests (Per Feature):
- Parser tests for new syntax
- Type checker tests for validation
- Codegen tests for LLVM IR correctness
- Runtime tests for behavior verification

### Integration Tests:
- Result type with ? operator in complex functions
- when loop with break/continue in various scenarios
- Closures capturing multiple variables
- Array slicing with bounds checking

### Performance Tests:
- Result register passing optimization
- Closure call overhead vs function pointers
- Array slice zero-copy verification

---

## SUCCESS CRITERIA

### Phase 1 Complete When:
1. ✅ Result<T,E> type fully functional (parse, check, codegen)
2. ✅ ? unwrap operator working (both variants)
3. ✅ is ternary operator
4. ✅ when loop with then/end blocks
5. ✅ Stack closures with capture analysis
6. ✅ Basic array slicing
7. ✅ pick pattern matching for Results
8. ✅ All tests passing
9. ✅ Documentation complete

### Deferred to Phase 2:
- Heap closures (needs research_021 GC integration)
- Async functions (needs research_029 spec)
- Advanced array operations (tensors, SIMD)
- Monadic operations library

---

## RISK ASSESSMENT

### Low Risk (Well-Specified):
- ✅ Result type layout (exact spec provided)
- ✅ ? operator semantics (detailed in research_019)
- ✅ when loop (complete state machine defined)

### Medium Risk (Requires Careful Integration):
- ⚠️ TBB sentinel checking (complex condition lowering)
- ⚠️ Closure capture analysis (needs borrow checker coordination)
- ⚠️ pick pattern matching (decision tree optimization)

### High Risk (Missing Specifications):
- ⚠️ Heap closures (awaiting research_021)
- ⚠️ Async functions (awaiting research_029)

---

## CURRENT STATUS

- **Phase 1 Progress:** ~50% (Type system complete, research available)
- **Immediate Next:** P0.1 - Result type parsing (no blockers)
- **Team Capacity:** 1 developer
- **Timeline:** 6-8 weeks for Phase 1 milestones

---

**Status:** ✅ ROADMAP COMPLETE AND APPROVED  
**Ready to Begin:** Result Type Parsing (P0.1)  
**Estimated Completion:** Phase 1 in 6-8 weeks

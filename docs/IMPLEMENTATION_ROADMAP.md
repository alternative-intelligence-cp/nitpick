# Aria Compiler Implementation Roadmap
**Date:** December 13, 2025  
**Goal:** Complete core compiler with minimal stdlib for self-hosting

---

## Philosophy: Dependency-Ordered Implementation

This roadmap orders features by **dependency chains** rather than research numbers. Each phase builds on the previous, ensuring we always have a working compiler.

**Key Principle:** Implement foundational features first, then build upward.

---

## Phase 0: Foundation Audit ✅ (COMPLETE)

**Status:** Already done
- ✅ Type system basics (int, uint, flt, tbb, bool, string)
- ✅ Basic expressions (binary ops, literals, variables)
- ✅ Function declarations
- ✅ Simple control flow (while, for loops)
- ✅ LLVM IR generation pipeline
- ✅ Modular architecture (v0.0.9)

---

## Phase 1: Core Control Flow (IMMEDIATE PRIORITY)

**Why First:** Control flow is needed for EVERYTHING. Can't test features without if/else.

### 1.1 - Basic Conditionals (research_019)
**Dependencies:** None (uses existing expression evaluation)

**Tasks:**
- [ ] Add `is` ternary operator token (TOKEN_KW_IS)
- [ ] Implement TernaryExpr AST node
- [ ] Parser: `parseIsExpression()` - precedence after `??`
- [ ] Codegen: `visitTernaryExpr()` - basic block branching
- [ ] Tests: is/then/else with all types

**Stdlib Needed:** None (uses existing print)

**Estimated:** 2-3 hours

### 1.2 - If/Else with TBB Safety (research_019)
**Dependencies:** 1.1 (shares branching logic)

**Tasks:**
- [ ] Enhance existing IfStmt codegen
- [ ] Add TBB validity checks (!is_err && is_true)
- [ ] Implement else-if chains
- [ ] Tests: TBB conditionals, nested if/else

**Stdlib Needed:** None

**Estimated:** 2-3 hours

### 1.3 - Unwrap Operator (research_019)
**Dependencies:** result type (already exists)

**Tasks:**
- [ ] Add ? unwrap operator (already tokenized)
- [ ] Implement UnwrapExpr AST node
- [ ] Parser: `parseUnwrapExpr()` - postfix operator
- [ ] Codegen: Check err field, return val or default
- [ ] Tests: unwrap with result types

**Stdlib Needed:** None

**Estimated:** 2 hours

**Phase 1 Total:** 6-8 hours, **NO STDLIB DEPENDENCIES**

---

## Phase 2: Pattern Matching (HIGH VALUE)

**Why Second:** Enables cleaner tests and compiler internals.

### 2.1 - Pick Statement Core (research_019)
**Dependencies:** Phase 1 (uses if/else internally)

**Tasks:**
- [ ] Implement PickStmt codegen (already has AST)
- [ ] Handle exact match cases
- [ ] Handle comparison cases (<, >, <=, >=)
- [ ] Handle wildcard (*)
- [ ] Tests: basic pick with integers

**Stdlib Needed:** None

**Estimated:** 3-4 hours

### 2.2 - Pick Ranges (research_019)
**Dependencies:** 2.1

**Tasks:**
- [ ] Add .. and ... range operators to parser
- [ ] Implement range matching in pick
- [ ] Tests: inclusive/exclusive ranges

**Stdlib Needed:** None

**Estimated:** 2 hours

### 2.3 - Pick Fallthrough (research_019)
**Dependencies:** 2.1

**Tasks:**
- [ ] Implement fall(label) statement
- [ ] Add label support to PickCase
- [ ] Generate proper branch targets
- [ ] Tests: explicit fallthrough chains

**Stdlib Needed:** None

**Estimated:** 2-3 hours

### 2.4 - Pick Destructuring (research_019) [OPTIONAL]
**Dependencies:** 2.1, composite types

**Tasks:**
- [ ] Implement DestructurePattern codegen (AST exists)
- [ ] Object destructuring: { key: var }
- [ ] Array destructuring: [a, b, c]
- [ ] Tests: pattern matching with structs/arrays

**Stdlib Needed:** None (can defer)

**Estimated:** 4-5 hours (DEFER until structs more mature)

**Phase 2 Total:** 7-9 hours core (11-14 with destructuring)

---

## Phase 3: Advanced Loops (NICE TO HAVE)

**Why Third:** Improves iteration but not critical for bootstrapping.

### 3.1 - When Loop (research_019)
**Dependencies:** Phase 1 (uses if/else)

**Tasks:**
- [ ] Implement WhenLoop codegen (AST exists)
- [ ] Generate body/then/end blocks
- [ ] Track loop state (run_once, broke_early flags)
- [ ] Tests: when/then/end combinations

**Stdlib Needed:** None

**Estimated:** 3-4 hours

### 3.2 - Till Loop (research_018)
**Dependencies:** None (separate from when)

**Tasks:**
- [ ] Add TOKEN_KW_TILL (already exists)
- [ ] Implement TillLoop AST node
- [ ] Parser: `parseTillLoop()`
- [ ] Codegen: $ variable, direction inference
- [ ] Tests: positive/negative step

**Stdlib Needed:** None

**Estimated:** 2-3 hours

### 3.3 - Loop Construct (research_018)
**Dependencies:** 3.2 (similar to till)

**Tasks:**
- [ ] Add TOKEN_KW_LOOP
- [ ] Implement LoopStmt AST node
- [ ] Parser: `parseLoopStmt()`
- [ ] Codegen: start/limit/step with direction
- [ ] Tests: up/down counting

**Stdlib Needed:** None

**Estimated:** 2-3 hours

**Phase 3 Total:** 7-10 hours

---

## Phase 4: Type System Completion (FOUNDATIONAL)

**Why Fourth:** Need solid types before memory management.

### 4.1 - Complete Integer Types (research_012)
**Dependencies:** None (partial implementation exists)

**Tasks:**
- [ ] Add literal suffixes (42u8, 100i16)
- [ ] Implement @pack directive for sub-byte structs
- [ ] Add checked arithmetic intrinsics
- [ ] Tests: suffixes, packing, overflow

**Stdlib Needed:** None

**Estimated:** 3-4 hours

### 4.2 - Floating-Point Types (research_013)
**Dependencies:** 4.1 (shares type system code)

**Tasks:**
- [ ] Add flt32, flt64, flt128 to type system
- [ ] Implement flt256, flt512 (software emulation stubs)
- [ ] Add floating literal parsing
- [ ] LLVM IR generation for float ops
- [ ] Tests: float arithmetic, all widths

**Stdlib Needed:** None (print already handles floats)

**Estimated:** 4-5 hours

### 4.3 - Vector Types (research_014)
**Dependencies:** 4.2 (uses float types)

**Tasks:**
- [ ] Add vec2, vec3, vec4, dvec2-4, ivec2-4
- [ ] Add vec9 for 3x3 matrices
- [ ] SIMD lowering (AVX/SSE)
- [ ] Vector arithmetic operators
- [ ] Tests: vector ops, swizzling

**Stdlib Needed:** None

**Estimated:** 5-6 hours

### 4.4 - Composite Types (research_015)
**Dependencies:** 4.1-4.3 (need base types)

**Tasks:**
- [ ] Enhance struct support (already partial)
- [ ] Add tuple types
- [ ] Implement union types
- [ ] Memory layout and alignment
- [ ] Tests: composite construction, member access

**Stdlib Needed:** None

**Estimated:** 6-8 hours

**Phase 4 Total:** 18-23 hours

---

## Phase 5: Memory Management (CRITICAL FOR STDLIB)

**Why Fifth:** Need before building any significant stdlib.

### 5.1 - Wild Memory System (research_022)
**Dependencies:** Phase 4 (need types)

**Tasks:**
- [ ] Implement @ address-of operator codegen
- [ ] Implement -> pointer dereference operator
- [ ] Add aria.alloc/free runtime functions
- [ ] Implement wild type qualifier
- [ ] Tests: manual memory management

**Stdlib Needed:** aria.alloc, aria.free (C++ runtime)

**Estimated:** 4-5 hours

### 5.2 - Memory Pinning (research_022)
**Dependencies:** 5.1, GC stubs

**Tasks:**
- [ ] Implement # pin operator
- [ ] Add pinned_bit to object headers
- [ ] Create pinning runtime bridge
- [ ] Tests: pin/unpin operations

**Stdlib Needed:** GC runtime stubs

**Estimated:** 3-4 hours

### 5.3 - GC Basics (research_021)
**Dependencies:** 5.1 (need wild for comparison)

**Tasks:**
- [ ] Implement ObjHeader (64-bit)
- [ ] Create shadow stack mechanism
- [ ] Add GC allocation stubs
- [ ] Implement mark phase (no sweep yet)
- [ ] Tests: allocation, roots

**Stdlib Needed:** std.mem (minimal)

**Estimated:** 8-10 hours (complex!)

### 5.4 - Borrow Checker (research_001)
**Dependencies:** 5.1, 5.3

**Tasks:**
- [ ] Implement lifetime analysis
- [ ] Add borrow checking to type checker
- [ ] Mutable/immutable borrow rules
- [ ] Tests: borrow violations

**Stdlib Needed:** None

**Estimated:** 10-12 hours (VERY complex!)

**Phase 5 Total:** 25-31 hours

---

## Phase 6: Generics & Monomorphization (NEEDED FOR STDLIB)

**Why Sixth:** Stdlib needs generic collections.

### 6.1 - Generic Functions (research_027)
**Dependencies:** Phase 4 (type system)

**Tasks:**
- [ ] Add <T> generic parameter syntax
- [ ] Implement *T dereference in bodies
- [ ] Parse generic function declarations
- [ ] Type inference for generic calls
- [ ] Tests: generic identity, max, swap

**Stdlib Needed:** None

**Estimated:** 5-6 hours

### 6.2 - Monomorphization (research_027)
**Dependencies:** 6.1

**Tasks:**
- [ ] Implement demand-driven instantiation
- [ ] Name mangling (_Aria_M_<func>_<types>)
- [ ] Deduplication cache
- [ ] Tests: multiple instantiations

**Stdlib Needed:** None

**Estimated:** 6-8 hours

### 6.3 - Const Generics (research_027, research_030)
**Dependencies:** 6.2

**Tasks:**
- [ ] Add const generic parameters (const N: int)
- [ ] Implement compile-time evaluation
- [ ] matrix<T, R, C> support
- [ ] Tests: fixed-size arrays, matrices

**Stdlib Needed:** None

**Estimated:** 7-9 hours

**Phase 6 Total:** 18-23 hours

---

## Phase 7: Essential Stdlib (FOR COMPILER USE)

**Why Seventh:** Compiler needs these to self-host.

### 7.1 - Platform Abstraction (research_031)
**Dependencies:** Phase 5 (memory)

**Tasks:**
- [ ] Create std.sys module
- [ ] Implement spawn/exec/wait
- [ ] Add platform detection (Linux/Windows/macOS)
- [ ] File path operations
- [ ] Tests: process spawning

**Files:** lib/stdlib/sys.aria

**Estimated:** 6-8 hours

### 7.2 - File I/O (research_031)
**Dependencies:** 7.1

**Tasks:**
- [ ] Enhance existing I/O runtime
- [ ] Add std.io module (Aria wrapper)
- [ ] Implement File struct with RAII
- [ ] TBB-safe seeking
- [ ] Tests: read/write files

**Files:** lib/stdlib/io.aria

**Estimated:** 5-6 hours

### 7.3 - String Processing (research_031)
**Dependencies:** Phase 4 (string type)

**Tasks:**
- [ ] Expand existing string.aria
- [ ] Add UTF-8 validation
- [ ] Implement split/join/trim
- [ ] Lexical analysis helpers
- [ ] Tests: string operations

**Files:** lib/stdlib/string.aria (expand)

**Estimated:** 4-5 hours

### 7.4 - Collections (for compiler)
**Dependencies:** Phase 6 (generics)

**Tasks:**
- [ ] Implement generic array operations
- [ ] Add Vector<T> dynamic array
- [ ] Implement HashMap<K, V> (simple)
- [ ] Tests: collection operations

**Files:** lib/stdlib/collections.aria

**Estimated:** 8-10 hours

**Phase 7 Total:** 23-29 hours

---

## Phase 8: Module System (FOR ORGANIZATION)

**Why Eighth:** Needed to organize compiler code.

### 8.1 - Use/Mod Keywords (research_028)
**Dependencies:** Phase 7 (file I/O)

**Tasks:**
- [ ] Implement use statement parsing
- [ ] Add mod keyword for modules
- [ ] File-based module resolution
- [ ] Import/export logic
- [ ] Tests: multi-file projects

**Stdlib Needed:** std.sys (file operations)

**Estimated:** 8-10 hours

### 8.2 - Visibility (research_028)
**Dependencies:** 8.1

**Tasks:**
- [ ] Implement pub keyword
- [ ] Add pub(package), pub(super)
- [ ] Visibility checks in type checker
- [ ] Tests: public/private access

**Stdlib Needed:** None

**Estimated:** 4-5 hours

### 8.3 - External C FFI (research_028)
**Dependencies:** 8.1

**Tasks:**
- [ ] Implement extern "libc" blocks
- [ ] C type mapping (string -> char*)
- [ ] Calling convention handling
- [ ] Tests: C library calls

**Stdlib Needed:** None

**Estimated:** 6-8 hours

**Phase 8 Total:** 18-23 hours

---

## Phase 9: Advanced Features (NICE TO HAVE)

**Why Ninth:** Improves quality but not critical for bootstrap.

### 9.1 - Async/Await (research_029) [DEFER]
### 9.2 - Const/CTFE (research_030) [DEFER]
### 9.3 - Matrix/Tensor Types (research_017) [DEFER]
### 9.4 - Runtime Assembler (research_023) [DEFER]

**Note:** These can wait until after self-hosting.

---

## Summary: Critical Path to Self-Hosting

**MUST HAVE (in order):**
1. ✅ Phase 0: Foundation (done)
2. **Phase 1:** Core Control Flow (6-8 hrs)
3. **Phase 2:** Pattern Matching (7-9 hrs)
4. **Phase 4:** Type System (18-23 hrs)
5. **Phase 5:** Memory Management (25-31 hrs)
6. **Phase 6:** Generics (18-23 hrs)
7. **Phase 7:** Essential Stdlib (23-29 hrs)
8. **Phase 8:** Module System (18-23 hrs)

**Total Estimated:** 115-146 hours (3-4 weeks full-time)

**CAN DEFER:**
- Phase 3: Advanced loops (use while/for for now)
- Phase 9: Async, CTFE, tensors (post-bootstrap)

---

## Implementation Strategy

### Week 1: Control Flow & Types
- Days 1-2: Phase 1 (if/is/unwrap)
- Days 3-4: Phase 2 (pick/ranges/fall)
- Day 5: Phase 4.1-4.2 (integers/floats)

### Week 2: Types & Memory
- Days 6-7: Phase 4.3-4.4 (vectors/composites)
- Days 8-10: Phase 5.1-5.3 (wild/pin/GC)

### Week 3: Advanced Memory & Generics
- Days 11-12: Phase 5.4 (borrow checker)
- Days 13-15: Phase 6 (generics/monomorphization)

### Week 4: Stdlib & Modules
- Days 16-18: Phase 7 (stdlib essentials)
- Days 19-21: Phase 8 (module system)

### Week 5: Polish & Test
- Days 22-24: Integration testing
- Days 25-26: Bug fixes
- Day 27: Self-hosting attempt
- Day 28: Success! 🎉

---

## Next Steps

**IMMEDIATE ACTION:**
1. Start Phase 1.1: Implement `is` ternary operator
2. This unblocks all conditional logic
3. No stdlib dependencies - can start NOW

**Command to begin:**
```bash
cd /home/randy/._____RANDY_____/REPOS/aria
# Ready to implement!
```

---

*"The journey of a thousand miles begins with a single if statement."* 🚀

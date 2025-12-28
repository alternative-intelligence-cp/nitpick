# Aria Compiler Deep Research Audit - Gemini Deep Research Prompt

## Context

You are performing a comprehensive audit of the **Aria programming language compiler**. Aria is a systems programming language with:

- **Memory Safety**: Multiple allocation strategies (GC, wild, wildx, stack, arena, pool, slab)
- **Borrow Checker**: Rust-inspired lifetime tracking with defer obligations
- **Type System**: Generics, traits, opaque types, high-precision numerics (flt256/flt512, tbb64, balanced ternary)
- **Advanced Features**: Async/await with io_uring, six-stream I/O topology, Result types with pass/fail sugar, compile-time evaluation
- **Runtime**: LLVM-based JIT compilation, zero-copy I/O, coroutine support

## Your Mission

Conduct a **detailed technical audit** of the Aria compiler implementation by:

1. **Cross-referencing specifications against implementation**
2. **Identifying incomplete features, TODOs, and stub implementations**
3. **Detecting bugs, edge cases, and potential security issues**
4. **Verifying correctness of complex subsystems** (borrow checker, type checker, code generation)
5. **Generating a prioritized todo list** for remaining work

---

## Files Provided

1. **aria_source_full.txt** (13MB, 410,916 lines)
   - Complete source code: C++ compiler + Aria stdlib
   - Directories: `src/`, `include/`, `lib/`, `stdlib/`, `vendor/`, `third_party/`, `editors/`
   - File types: `.cpp`, `.hpp`, `.h`, `.c`, `.aria`, `CMakeLists.txt`

2. **Specification Documents** (from docs/gemini/responses/)
   - Implementation specs for ARIA-010 through ARIA-021
   - Architecture documents, design decisions, API contracts

---

## Audit Focus Areas

### 1. **Feature Completeness**

For each major subsystem, verify:

- ✅ **Fully Implemented**: Feature works, has tests, handles edge cases
- ⚠️ **Partially Implemented**: Core logic exists but missing edge cases, error handling, or optimizations
- ❌ **Not Implemented**: Stub/placeholder code, TODOs, or missing entirely

**Key Subsystems to Check:**

- **Frontend**: Lexer, Parser, Preprocessor
- **Type System**: Type checker, generic resolver, trait system
- **Semantic Analysis**: Borrow checker, definite assignment, exhaustiveness checking, const evaluation
- **Code Generation**: LLVM IR generation, TBB arithmetic, ternary ops, high-precision floats
- **Runtime**: GC, async executor, io_uring integration, process spawning, six-stream I/O
- **Standard Library**: Collections (Vec, HashMap), Result types, async primitives

### 2. **Bug Detection**

Look for:

- **Memory Safety Issues**: Use-after-free, double-free, leaks in wild/wildx allocators
- **Type System Holes**: Unsound casts, missing lifetime checks, generic instantiation bugs
- **Borrow Checker Gaps**: Missing move/borrow checks, incorrect defer obligation tracking
- **Code Generation Errors**: Incorrect LLVM IR, mismatched calling conventions, alignment issues
- **Concurrency Bugs**: Race conditions in async runtime, GC pauses during I/O
- **Edge Cases**: Integer overflow, divide-by-zero, null dereference, UTF-8 validation

### 3. **Specification Compliance**

Cross-reference implementation against specs:

- Do all ARIA-010 through ARIA-021 tasks match their specifications?
- Are APIs implemented as documented?
- Are error messages clear and helpful?
- Are performance characteristics as expected (e.g., O(1) HashMap lookup)?

### 4. **Code Quality**

Assess:

- **Error Handling**: Proper Result propagation, TBB sticky errors, panic modes
- **Documentation**: Missing comments on complex algorithms (especially borrow checker, const eval)
- **Test Coverage**: Are critical paths tested? (Look for test files in context)
- **Performance**: Obvious inefficiencies (excessive copying, redundant allocations)

---

## Output Format

Provide your findings in this structure:

### Executive Summary
- Total features audited: X
- Fully implemented: X (%)
- Partially implemented: X (%)
- Not implemented: X (%)
- Critical bugs found: X
- Medium severity bugs: X
- Low severity issues: X

### Critical Issues (P0 - Must Fix Before Release)
1. **[CATEGORY] Issue Title**
   - **Location**: `file.cpp:line` or subsystem name
   - **Description**: What's wrong
   - **Impact**: Why it matters (crash, memory corruption, incorrect behavior)
   - **Recommendation**: How to fix

### High Priority (P1 - Should Fix Soon)
[Same format as P0]

### Medium Priority (P2 - Should Fix Eventually)
[Same format as P0]

### Incomplete Features (By Subsystem)
**Borrow Checker**
- [ ] Feature X - Status: Stub, needs full implementation
- [ ] Feature Y - Status: Partially done, missing edge case Z

**Type System**
[etc.]

### TODO List (Actionable Items)
Prioritized list of specific tasks:

**Phase 1: Critical Fixes (Must complete before any release)**
1. Fix use-after-free in wild allocator defer tracking (src/runtime/allocators/wild_alloc.cpp:234)
2. Implement missing null check in HashMap rehashing (lib/std/collections.aria:398)
[etc.]

**Phase 2: Feature Completion**
[etc.]

**Phase 3: Quality & Performance**
[etc.]

### Positive Findings
What's implemented well:
- Excellent X implementation with proper Y
- Good test coverage for Z
[etc.]

---

## Special Attention Areas

### Borrow Checker (src/frontend/sema/borrow_checker.cpp)
This is **critical for memory safety**. Verify:
- All move semantics correctly prevent use-after-move
- Defer obligations properly track wild/wildx cleanup
- Pinning operator (#) prevents moves as specified
- No false positives that block valid code

### High-Precision Floats (src/runtime/highprec_float.cpp)
Verify:
- flt256/flt512 arithmetic is IEEE-compliant
- NaN propagation for TBB integration works correctly
- Alignment requirements (32-byte/64-byte) are enforced

### Async Runtime (src/runtime/async/)
Check for:
- Race conditions in work-stealing executor
- Proper io_uring integration (fallback on older kernels?)
- GC safepoints during async operations
- Coroutine state machine correctness

### Six-Stream I/O (src/runtime/process/process.cpp, lib/std/process.aria)
Verify:
- All 6 FDs (stdin, stdout, stderr, stddbg, stddati, stddato) properly set up
- FD inheritance (O_CLOEXEC) correctly handled
- close_range() syscall with fallback for kernels < 5.9

### Standard Library HashMap (lib/std/collections.aria)
Recent changes made it generic - verify:
- Hash function parameter works for all key types
- Linear probing handles collisions correctly
- Rehashing on removal fixes probe chains
- Load factor (0.7) prevents clustering

---

## Methodology

1. **Read specifications first** to understand intended behavior
2. **Trace implementation paths** for each major feature
3. **Look for defensive programming**: null checks, bounds checks, error handling
4. **Check invariants**: Does code maintain documented contracts?
5. **Think like an attacker**: Where could unsafe code cause exploits?
6. **Verify test coverage**: Are edge cases tested?

---

## Constraints

- **No false positives**: Only report real issues with evidence
- **Be specific**: Always cite file:line or describe precisely where issue exists
- **Explain impact**: Don't just say "bug", explain what breaks
- **Actionable recommendations**: "Add null check" not just "missing validation"
- **Prioritize correctly**: Memory corruption > incorrect output > performance

---

## Begin Audit

Start your deep research audit now. Be thorough, methodical, and precise. The Aria compiler will be used for production systems programming - correctness and safety are paramount.

# Aria Programming Guide - Documentation Status

**Last Updated**: February 13, 2026  
**Total Documented**: 18 files, ~20,517 lines  
**Sessions Completed**: 29  

This document tracks what features from `aria_specs.txt` have been documented in the programming guide vs what remains to be documented.

---

## 📊 Overall Progress

| Category | Documented | Total | % Complete |
|----------|-----------|-------|------------|
| **Fundamental Types** | 14 | 40+ | ~35% |
| **Advanced Types** | 1 | 12+ | ~8% |
| **Built-in Facilities** | 1 | 10+ | ~10% |
| **Language Features** | 0 | 20+ | 0% |

---

## ✅ COMPLETED Documentation

### Fundamental Types (14/40+)

#### Boolean & Character
- [x] **bool.md** (784 lines) - Logical true/false type
- [x] **char.md** (697 lines) - UTF-8 Unicode code point
- [x] **string.md** (935 lines) - UTF-8 string type

#### Signed Integers (Standard)
- [x] **i8.md** (1456 lines) - 8-bit signed integer (-128 to +127)
- [x] **i16.md** (1517 lines) - 16-bit signed integer (-32,768 to +32,767)
- [x] **i32.md** (989 lines) - 32-bit signed integer (default, ±2.1B)
- [x] **i64.md** (991 lines) - 64-bit signed integer (±9.2 quintillion)

#### Unsigned Integers (Standard)
- [x] **u8.md** (1121 lines) - 8-bit unsigned byte (0-255)
- [x] **u16.md** (1167 lines) - 16-bit unsigned (0-65,535, ports)
- [x] **u32.md** (1131 lines) - 32-bit unsigned (0-4.3B)
- [x] **u64.md** (1091 lines) - 64-bit unsigned (0-18.4 quintillion)

#### Floating Point (Standard)
- [x] **f32.md** (1373 lines) - 32-bit IEEE 754 float (~7 digits)
- [x] **f64.md** (915 lines) - 64-bit IEEE 754 float (~15-16 digits)

#### Twisted Balanced Binary (TBB) Integers
- [x] **tbb8.md** (1217 lines) - 8-bit symmetric ±127, ERR=-128
- [x] **tbb16.md** (952 lines) - 16-bit symmetric ±32,767, ERR=-32768
- [x] **tbb32.md** (1011 lines) - 32-bit symmetric, general-purpose
- [x] **tbb64.md** (980 lines) - 64-bit symmetric, Y2038-proof

### Advanced Types (1/12+)

#### Quantum/Speculative Types
- [x] **quantum.md** (1147 lines) - Q3<T> and Q9<T> superposition types

### Built-in Facilities (1/10+)

#### Debug & Development
- [x] **dbug.md** (1039 lines) - Grouped debug printing and assertions

---

## 🔲 TODO: Not Yet Documented

### Fundamental Types (Remaining ~26 types)

#### Extended Precision Integers
- [ ] **u128.md** - 128-bit unsigned (exotic precision)
- [ ] **u256.md** - 256-bit unsigned (crypto operations)
- [ ] **u512.md** - 512-bit unsigned (large crypto keys)
- [ ] **u1024.md** - 1024-bit unsigned (RSA-1024 range)
- [ ] **u2048.md** - 2048-bit unsigned (RSA-2048, string parsing)
- [ ] **u4096.md** - 4096-bit unsigned (RSA-4096, maximum crypto)
- [ ] **i128.md** - 128-bit signed (exotic precision)
- [ ] **i256.md** - 256-bit signed (large calculations)
- [ ] **i512.md** - 512-bit signed (massive range)
- [ ] **i1024.md** - 1024-bit signed (crypto operations)
- [ ] **i2048.md** - 2048-bit signed (string parsing)
- [ ] **i4096.md** - 4096-bit signed (maximum range)

#### Floating Point (Extended)
- [ ] **f128.md** - 128-bit IEEE float (quadruple precision)
- [ ] **fix256.md** - 256-bit fixed-point (Q128.128, deterministic)

#### Exact Rational Arithmetic (frac types)
**HIGH PRIORITY** - Built on TBB foundation (documented)

- [ ] **frac8.md** - 8-bit exact rationals: `struct {tbb8:whole; tbb8:num; tbb8:denom}`
  - Mixed-fraction representation (e.g., 2 1/3)
  - Sticky ERR propagation from all components
  - No floating-point drift, mathematically exact
  - Use cases: Small exact fractions, music intervals, phase relationships

- [ ] **frac16.md** - 16-bit exact rationals: `struct {tbb16:whole; tbb16:num; tbb16:denom}`
  - Medium precision exact arithmetic
  - Use cases: Audio phase, precise timing, exact percentages

- [ ] **frac32.md** - 32-bit exact rationals: `struct {tbb32:whole; tbb32:num; tbb32:denom}`
  - **Most common exact rational type** (general-purpose)
  - Use cases: Financial calculations (exact percentages), scientific ratios, exact geometry

- [ ] **frac64.md** - 64-bit exact rationals: `struct {tbb64:whole; tbb64:num; tbb64:denom}`
  - High-precision exact arithmetic
  - Use cases: High-precision physics, astronomy calculations, exact fractions for large values

**Key Philosophy**: Exact rationals solve "0.1 + 0.2 != 0.3" problem. `{0,1,10} + {0,2,10} = {0,3,10}` EXACTLY!

#### Deterministic Floating Point (tfp types)
**HIGH PRIORITY** - Built on TBB foundation (documented)

- [ ] **tfp32.md** - 32-bit deterministic float: `tbb16:exponent + tbb16:mantissa`
  - No -0.0 (unified zero)
  - Unified ERR sentinel (replaces NaN/Infinity)
  - Bit-identical results across platforms
  - Use cases: Safety-critical systems, deterministic simulations, cross-platform networking

- [ ] **tfp64.md** - 64-bit deterministic float: `tbb16:exponent + tbb48:mantissa`
  - **Standard deterministic float** (general-purpose)
  - No special values (NaN/Inf → ERR)
  - Sticky ERR propagation
  - Use cases: Physics simulations (exact replay), networked games (sync), financial modeling

**Key Philosophy**: Determinism > IEEE 754 compliance. Safety-critical systems need bit-exact reproducibility.

#### Balanced Number Systems
**EXOTIC** - Alternative base systems for specific use cases

- [ ] **trit.md** - Balanced ternary digit: `-1, 0, +1` (stored in i8)
  - ERR sentinel at -128
  - Three-state logic (negative, neutral, positive)
  - Use cases: Three-valued logic, analog-to-digital, fuzzy systems

- [ ] **tryte.md** - 10 trits: `3^10 = 59,049` values (stored in u16)
  - Compact ternary representation
  - Use cases: Compact storage of ternary data

- [ ] **nit.md** - Balanced nonary digit: `-4, -3, -2, -1, 0, +1, +2, +3, +4` (stored in i8)
  - ERR sentinel at -128
  - Nine-state logic (Q9 quantum type uses this!)
  - Use cases: Fine-grained confidence levels, multi-state systems

- [ ] **nyte.md** - 5 nits: `9^5 = 59,049` values (stored in u16)
  - Compact nonary representation
  - Use cases: Compact storage of Q9 quantum states

### Advanced Types (Remaining ~11 types)

#### Error Handling
**CRITICAL** - Core to Aria's safety philosophy

- [ ] **result.md** - `Result<T>` type: `struct {bool:err; T:val}`
  - Hard errors that MUST be handled
  - Integration with `?`, `??`, `?!` unwrap operators
  - Sticky error propagation
  - Use cases: File I/O, network operations, parsing, all fallible operations

- [ ] **unknown.md** - Soft error value (undefined states)
  - For operations like division by zero, array out-of-bounds
  - vs Result<T>: Soft errors vs hard errors
  - Propagates through calculations
  - Use cases: Math edge cases, default values, optional computations

- [ ] **failsafe.md** - Mandatory last-resort error handler
  - `failsafe(ErrorCode:code) -> never`
  - Called when no other error handling
  - Integration with `?!` emphatic unwrap operator
  - Use cases: Unrecoverable errors, assertion failures, contract violations

#### Memory Management
**IMPORTANT** - Safe alternatives to raw pointers

- [ ] **handle.md** - `Handle<T>` generational index handles
  - Safe arena allocation: `struct {u64:index; u32:generation}`
  - Stale handle detection (no use-after-free)
  - Reallocation safety (generation mismatch on arena growth)
  - Use cases: Neural networks (SHVO), graph structures, game entities, any dynamic allocation

- [ ] **pointer.md** - Raw pointer types: `T->` (reference), `T->>` (mutable)
  - Blueprint-style syntax: `int64->:ptr` (declaration), `ptr->member` (access)
  - Dereference operator: `value <- ptr` (shows data flow)
  - Address-of operator: `@var`
  - When to use vs Handle<T>
  - FFI/C interop

- [ ] **reference.md** - Safe references (lifetime-checked)
  - Borrow checking (if implemented)
  - vs pointers vs Handle<T>

#### Collections
- [ ] **array.md** - Fixed-size arrays: `int32[10]:arr`
  - Stack-allocated, fixed size
  - Bounds checking
  - vs slice vs vector

- [ ] **slice.md** - Array views: `int32[]:slice`
  - Reference to array segment
  - Mutable vs immutable slices
  - Use cases: Function parameters, subarrays

- [ ] **vector.md** - Dynamic arrays: `Vec<T>`
  - Heap-allocated, growable
  - Capacity vs length
  - Use cases: Dynamic collections, lists

#### Concurrency
- [ ] **atomic.md** - Atomic types: `atomic<T>`
  - SeqCst (sequentially consistent) by default
  - Weaker orderings: Acquire/Release/Relaxed (marked unsafe)
  - ERR sentinel atomicity across threads
  - Use cases: Thread-safe counters, flags, lock-free data structures

#### Generics/Composition
- [ ] **struct.md** - Structure types
  - Named fields
  - Memory layout guarantees
  - Generic structs: `struct<T>:Box { T:value; }`

- [ ] **enum.md** - Enumeration types (if distinct from pick)
  - Tagged unions
  - Pattern matching with pick

- [ ] **tuple.md** - Product types (if supported)
  - Anonymous field aggregation

### Built-in Facilities (Remaining ~9 facilities)

#### Contracts & Assertions
- [ ] **contracts.md** - Design by Contract
  - `requires` - Preconditions (function entry)
  - `ensures` - Postconditions (function exit, `result` keyword)
  - `invariant` - Loop invariants (future)
  - Compile-time vs runtime checks
  - Integration with Result<T>, unknown, failsafe()

#### I/O & Formatting
- [ ] **print.md** - Standard output printing
  - `print()`, `println()`, `eprint()`, `eprintln()`
  - String interpolation: `"Value: &{x}"`
  - Type-safe formatting

- [ ] **file_io.md** - File operations
  - Reading, writing, seeking
  - Result<T> integration (I/O failures)
  - Binary vs text modes

#### Math & Intrinsics
- [ ] **math_functions.md** - Mathematical operations
  - Trigonometry: `sin()`, `cos()`, `tan()`
  - Roots: `sqrt()`, `cbrt()`
  - Logarithms: `log()`, `log10()`, `exp()`
  - TFP vs IEEE 754 implementations

- [ ] **intrinsics.md** - Compiler intrinsics
  - `tbb_widen<T>()` - TBB widening (ARIA-018 compliant)
  - SIMD operations (if exposed)
  - Platform-specific optimizations

#### Memory & Performance
- [ ] **arena_allocator.md** - Arena allocation pattern
  - Integration with Handle<T>
  - Bulk allocation/deallocation
  - Use cases: Game engines, compilers, neural networks

- [ ] **simd.md** - SIMD vectorization (if exposed to user)
  - Vector types, operations
  - Platform support (SSE, AVX, NEON)

#### Testing & Diagnostics
- [ ] **testing.md** - Test framework (if built-in)
  - Test functions, assertions
  - Test organization

- [ ] **profiling.md** - Performance profiling (if built-in)
  - Timing, instrumentation

### Language Features (0/20+ documented)

#### Control Flow
- [ ] **if_else.md** - Conditional branching
  - `if`, `else if`, `else`
  - Expression form (returns value)

- [ ] **pick.md** - Pattern matching / switch
  - `pick (expr) { case: ...; fall(); }`
  - Exhaustiveness checking
  - Explicit `fall()` for fallthrough

- [ ] **till_loops.md** - Iteration
  - `till` loops (while/for equivalent)
  - `$` iteration variable
  - Range operators: `..` (inclusive), `...` (exclusive)
  - Break, continue semantics

#### Functions
- [ ] **functions.md** - Function definitions
  - Signature: `func:name = ReturnType(params) { body }`
  - Implicit Result<T> return type
  - `pass()` / `fail()` for returns
  - Pure functions, side effects

- [ ] **lambdas.md** - Anonymous functions (if distinct)
  - Closure syntax
  - Capture semantics

#### Operators
- [ ] **operators.md** - All operators comprehensive guide
  - Arithmetic: `+`, `-`, `*`, `/`, `%`
  - Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`, `<=>`
  - Logical: `&&`, `||`, `!`, `^^` (XOR)
  - Bitwise: `&`, `|`, `^`, `<<`, `>>`
  - Assignment: `=`, `+=`, `-=`, `*=`, `/=`, `%=`, etc.
  - Unwrap: `?`, `??`, `?!`, `?.`
  - Pipeline: `|>`, `<|`
  - Pointer: `->`, `<-`, `@`, `#`
  - Range: `..`, `...`
  - Ternary: `is condition : value_if_true : value_if_false`
  - Failsafe: `!!!` (emergency termination)
  - Precedence table (complete reference)

#### Type System
- [ ] **type_system.md** - Overview of type system philosophy
  - Safety > Stability > Accuracy > Performance > Convenience
  - Zero implicit conversion
  - Explicit type suffixes: `42u32`, `3.14f64`, `100tbb32`

- [ ] **generics.md** - Generic programming
  - Generic functions: `func<T>:identity = T(T:value) { pass(value); }`
  - Generic structs: `struct<T>:Box { T:value; }`
  - Type constraints (if supported)

- [ ] **type_conversions.md** - Explicit conversions
  - `convert<T>()` for widening/narrowing
  - Safety checks, ERR on overflow
  - vs casts (if unsafe casts exist)

#### Modules & Organization
- [ ] **modules.md** - Module system
  - Imports, exports
  - Namespacing
  - Visibility (public/private)

- [ ] **packages.md** - Package management (aria_make integration)
  - Dependencies
  - Build system

#### FFI & Interop
- [ ] **c_ffi.md** - C foreign function interface
  - `extern` blocks
  - C types: `c_int`, `c_void`, `c_char`, etc.
  - Pointer interop
  - String conversion: `to_cstr()`, `from_cstr()`

- [ ] **abi.md** - Application Binary Interface
  - Calling conventions
  - Memory layout guarantees
  - Platform compatibility

#### Safety & Verification
- [ ] **marked_unsafe.md** - Unsafe operations
  - When unsafe is allowed
  - Unsafe blocks (if they exist)
  - Audit trail

- [ ] **memory_model.md** - Memory ordering & threading
  - SeqCst by default
  - Data race = UB
  - Atomic ordering models
  - Happens-before relationships

- [ ] **compiler_errors.md** - Error messages guide
  - Common errors and solutions
  - Type mismatch, missing type suffix, etc.

---

## 📋 Recommended Documentation Order (Next Sessions)

Based on dependencies and user needs:

### **Immediate Priority** (Sessions 30-33)
Built on TBB foundation (already documented):

1. **frac32.md** - Most common exact rational type (Session 30)
2. **frac64.md** - High-precision rationals (Session 31)
3. **tfp32.md** - Deterministic 32-bit float (Session 32)
4. **tfp64.md** - Deterministic 64-bit float (Session 33)

### **High Priority** (Sessions 34-40)
Core error handling & memory management:

5. **result.md** - Result<T> error type (Session 34)
6. **unknown.md** - Soft error value (Session 35)
7. **failsafe.md** - Last-resort error handler (Session 36)
8. **handle.md** - Generational handles (arena allocator) (Session 37)
9. **array.md** - Fixed-size arrays (Session 38)
10. **slice.md** - Array views (Session 39)
11. **vector.md** - Dynamic arrays (Session 40)

### **Medium Priority** (Sessions 41-50)
Remaining fundamental types:

12. **frac8.md** - Small exact rationals
13. **frac16.md** - Medium exact rationals
14. **trit.md**, **tryte.md**, **nit.md**, **nyte.md** - Balanced number systems
15. **u128.md** through **i4096.md** - Extended precision integers (as needed)
16. **f128.md**, **fix256.md** - Extended floating point
17. **pointer.md** - Raw pointers (FFI critical)
18. **struct.md** - Structure types
19. **atomic.md** - Atomic operations

### **Lower Priority** (Sessions 51+)
Language features & advanced topics:

20. **functions.md**, **if_else.md**, **pick.md**, **till_loops.md** - Control flow
21. **operators.md** - Comprehensive operator reference
22. **contracts.md** - Design by Contract
23. **generics.md**, **type_conversions.md** - Type system details
24. **modules.md**, **c_ffi.md** - Organization & interop
25. **math_functions.md**, **intrinsics.md** - Built-in functions
26. Remaining facilities and advanced topics

---

## 📈 Metrics

**Current Coverage**: ~18/200+ features (~9% of total language)

**Completion Estimates**:
- **Fundamental types complete**: ~50 sessions (Sessions 1-50)
- **All critical features**: ~100 sessions (Sessions 1-100)
- **Full reference guide**: ~200 sessions (Sessions 1-200)

**Rate**: ~1-4 files per session, ~1000-4000 lines per session

**Philosophy**: Document forward-looking features BEFORE implementation to guide development. "Blueprints don't lie; inspector won't pass sloppy work."

---

## 🎯 Goal

**Ultimate Goal**: Every feature from `aria_specs.txt` gets comprehensive documentation in the programming guide, serving as:
1. **User reference**: How to use Aria types and features
2. **Implementation guide**: Specifications for compiler developers
3. **Design record**: Philosophy and rationale for language decisions
4. **Tutorial material**: Learning path from beginner to expert

**Current State**: Strong foundation in fundamental types (integers, floats, TBB), ready to build higher-level types (frac, tfp) that depend on TBB infrastructure.

**Next Milestone**: Complete exact arithmetic types (frac family) and deterministic floating point (tfp family) - the "killer features" that make Aria unique for safety-critical and exact computation.

---

## ✨ Significance

TBB types (Session 29) were a **major milestone**: Not just "another integer variant" but **foundational safety infrastructure** for:
- Exact rational arithmetic (frac types)
- Deterministic floating point (tfp types)
- Safety-critical error propagation
- ARIA-018 sentinel discontinuity constraint

This foundation enables the next phase: exact arithmetic types that solve fundamental problems:
- **0.1 + 0.2 = 0.3** (exactly, via frac types)
- **Bit-identical floating point across platforms** (via tfp types)
- **No silent failures** (ERR propagation throughout type hierarchy)

The documentation journey continues: **29 sessions done, ~171 to go!** 🚀

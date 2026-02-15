# Aria Programming Guide

**Status**: ✅ Phase 1 Complete (Type System), 🔄 Phase 2 In Progress (Session 34: Loop Batch 4 ✅ DONE - HALFWAY!)  
**Total Topics**: 350 markdown files (9 legacy files removed)  
**Type Guides Complete**: 50 comprehensive guides (~41,273 lines)  
**Code Files Fixed**: 30 error + 3 lambda/closure + 5 stdlib lambda + 7 loop batch 1 + 15 loop batch 2 + 11 loop batch 3 + 7 loop batch 4 = 78 files  
**Legacy Files Removed**: 9 obsolete files (~2,482 lines of outdated docs)  
**Last Updated**: February 14, 2026 (Session 34 - Loop syntax batch 4 complete - control_flow/ directory clean - **51% HALFWAY POINT!**)

---

## Overview

This comprehensive programming guide covers every feature of the Aria programming language. Each topic has its own dedicated file for detailed documentation.

**Current Phase**: ✅ **Phase 1 COMPLETE** - All 50 type guides documented (Sessions 1-26). ✅ **Phase 2 Core Rewrites COMPLETE** - Lambda/closure docs + stdlib examples fixed + legacy cleanup + loop batches 1-4 (Sessions 29-34).

**Recent Work (Feb 14, 2026)**:
- ✅ Session 17: Ultra-large integers (int1024/2048/4096, uint1024/2048/4096 - RSA cryptography)
- ✅ Session 18: Quantum types & debug system (Q3/Q9 cognitive primitives, dbug infrastructure)
- ✅ Session 19: Handle<T> - Memory-safe arena references for dynamic graphs
- ✅ Session 20: tfp32/tfp64 - Deterministic floating-point for reproducible physics
- ✅ Session 21: frac8-frac64 - Exact rational arithmetic (zero drift mathematics)
- ✅ Session 22: complex<T> - Wave mechanics infrastructure for consciousness
- ✅ Session 23: atomic<T> - Thread-safety infrastructure (prevents catastrophic races)
- ✅ Session 24: simd<T,N> - Data-parallel vectorization (real-time consciousness)
- ✅ Session 25: fix256 - Deterministic 256-bit fixed-point (zero-drift arithmetic)
- ✅ Session 26: Result<T,E> - Explicit error handling (impossible to ignore at compile-time)
- ✅ Session 27: High-visibility syntax fixes (error_handling.md, filter.md, SYNTAX_REFERENCE.md)
- ✅ Session 28: **Error syntax batch complete!** (30 files: stdlib/, types/, functions/, operators/)
- ✅ Session 29: **Lambda/closure rewrites complete!** (3 core docs + specs pointer fix)
- ✅ Session 30: **Stdlib lambda examples complete!** (5 files: reduce, transform, print, readFile, writeFile)
- ✅ **Legacy cleanup complete!** (9 obsolete files removed: types/, control_flow/, standard_library/, memory_model/)
- ✅ **Session 31: Loop syntax batch 1 complete!** (57 loops fixed across 7 type guides)
- ✅ **Session 32: Loop syntax batch 2 complete!** (34 loops fixed across 15 operator guides - CRITICAL range operator docs corrected!)
- ✅ **Session 33: Loop syntax batch 3 complete!** (20 loops fixed across 11 memory_model guides)
- ✅ **Session 34: Loop syntax batch 4 complete!** (155 loops fixed across 7 control_flow guides - **CRITICAL TEACHING FILES** - **51% HALFWAY POINT!**)

See [UPDATE_PROGRESS.md](UPDATE_PROGRESS.md) for detailed status and [SYNTAX_AUDIT_FEB14_2026.md](SYNTAX_AUDIT_FEB14_2026.md) for Phase 2 roadmap.

---

## Phase 2: Syntax Cleanup (In Progress - Session 34: Loop Batch 4 ✅ DONE - **51% HALFWAY POINT!**)

**Completed Issues** ✅:
- ✅ **Loop syntax batch 4 - Control flow** (155 loops fixed across 7 files - **CRITICAL TEACHING FILES!**):
  - **HIGH PRIORITY** (101 loops):
    * control_flow/iteration_variable.md (39 loops) - Teaches iteration patterns, $ index access
    * control_flow/for.md (35 loops) - Teaches till loops (renamed from for loops)
    * control_flow/dollar_variable.md (27 loops) - **COMPLETELY REWROTE**: $ is index, not mutation marker!
  - **MEDIUM PRIORITY** (33 loops):
    * control_flow/continue.md (18 loops) - Skip iteration patterns
    * control_flow/for_syntax.md (15 loops) - Complete syntax reference (renamed to "Till Loop Syntax")
  - **LOWER PRIORITY** (21 loops):
    * control_flow/break.md (11 loops) - Exit loop patterns
    * control_flow/while.md (1 loop) - Comparison example
  - **PRESERVED**:
    * control_flow/loop.md: C-style for loops + Python/Rust/C comparisons
    * control_flow/till.md: C-style for loops + Python/Rust/C comparisons
  - **IMPACT**: CRITICAL - These files TEACH users how to write loops!
    - If wrong, users learn `for item in collection` (doesn't exist in Aria)
    - Similar to Session 32's range operators teaching wrong syntax
    - Teaching files must be accurate or users learn broken patterns
  - **266/517 total loops fixed (51% - HALFWAY POINT!)**
- ✅ **Loop syntax batch 3 - Memory model** (20 loops fixed across 11 files):
  - memory_model/gc.md, borrowing.md, stack.md (11 loops) - Garbage collection, borrow patterns, stack allocation
  - memory_model/borrow_operator.md, allocators.md, mutable_borrow.md, immutable_borrow.md (8 loops)
  - memory_model/aria_gc_alloc.md, address_operator.md, aria_alloc_array.md, pinning.md (4 loops)
  - **Preserved**: 7 C-style `for (;;)` loops (valid Aria syntax for performance code)
  - All converted from wrong `for i in 0..n` → correct `till(n-1, 1)` syntax
- ✅ **Loop syntax batch 2 - Operator guides** (34 loops fixed across 15 files):
  - **CRITICAL**: operators/range.md, range_exclusive.md, range_inclusive.md (19 loops)
    - **Issue**: Range operator reference docs were teaching Rust `for-in` syntax as Aria!
    - **Fix**: Clarified `..` and `..=` are for SLICING, not loops; added prominent warnings
    - **Impact**: HIGH - These are foundational operator docs users learn from
  - operators/iteration.md, add_assign.md, mul_assign.md (6 loops)
  - operators/add.md, div_assign.md, divide.md, increment.md, left_shift.md, lshift_assign.md, mod_assign.md, modulo.md, not_equal.md (9 loops)
  - All converted from wrong `for i in 0..n` → correct `till(n-1, 1)` syntax
- ✅ **Loop syntax batch 1 - Type guides** (57 loops fixed across 7 files):
  - types/int1024_int2048_int4096.md: 11 loops (cryptographic operations, cache patterns)
  - types/uint1024_uint2048_uint4096.md: 16 loops (Merkle trees, modular exponentiation)
  - types/tfp32_tfp64.md: 12 loops (deterministic physics, blockchain consensus)
  - types/frac8_frac16_frac32_frac64.md: 10 loops (exact financial math, zero drift)
  - types/Handle.md: 6 loops (neurogenesis, arena integrity)
  - types/Q3_Q9.md: 1 loop (quantum confidence accumulation)
  - debugging/dbug.md: 1 loop (debug group enumeration)
- ✅ **Legacy file cleanup** (9 files deleted, ~2,482 lines removed):
  - types/: array.md, array_declaration.md, array_operations.md, func.md, func_declaration.md
  - control_flow/: return.md
  - standard_library/: reduce.md (old version)
  - memory_model/: wild.md, wildx.md
  - Rationale: All used obsolete Rust-style Aria syntax from abandoned spec, fully superseded by modern guides
- ✅ **Stdlib lambda examples** (5 files, 77+ fixes): reduce, transform, print, readFile, writeFile
- ✅ **Pointer syntax in aria_specs.txt** (28 corrections): `int32@:` → `int32->:` (blueprint-style data flow)
- ✅ **Core lambda/closure documentation** (3 complete rewrites):
  - functions/lambda.md (~470 lines) - Fixed `|params|` to `returnType(params) { body }`
  - functions/anonymous_functions.md (~320 lines) - Same syntax as regular functions
  - functions/closure_capture.md (~480 lines) - Value vs pointer capture (no `$variable` magic)
- ✅ **284 error handling issues** (30 files): All `return Ok/Err` → `pass/fail`
  - stdlib/ (10 files): http_client, readFile, writeFile, exec, math, stream_io, wait, httpGet, readJSON, process_management
  - types/ (18 files): Handle, Result, ERR, NIL, all integer types, flt64, string, func_return, tensor
  - functions/ (1 file): function_return_type
  - operators/ (1 file): question_operator

**Remaining Issues**:
- ~251 loop syntax patterns remaining: C-style `for` → Aria `till` with `$`  
  - **51% COMPLETE** (266/517 loops fixed!)
  - Next targets: advanced_features/, modules/, io_system/, standard_library/ (remaining subdirs)
- 12 Result type signatures: `Result<T>` → `Result<T,E>`
  - NOT in active programming guide - orphaned legacy files

**Session 27-34 Achievements**:
- ✅ SYNTAX_REFERENCE.md - Canonical syntax examples
- ✅ aria_specs.txt - Corrected pointer syntax (28 instances)
- ✅ Lambda/closure core docs - Complete rewrites with correct syntax
- ✅ Stdlib lambda examples - All functional programming patterns corrected
- ✅ All error handling syntax - 30 files corrected
- ✅ Loop syntax batches 1-4 - 266 loops fixed (types + operators + memory_model + control_flow) - **51% HALFWAY!**

**Next: Session 35** - Continue loop syntax corrections in remaining directories

---

## Guide Structure

### 📦 Types (80+ files)
Comprehensive coverage of Aria's type system:
- **Standard Integers**: ✅ int1 (593 lines - 1-bit signed, educational), ✅ int2/int4 (703 lines - nibbles, BCD encoding), ✅ int8/int16 (895 lines - asymmetric ranges, wrapping overflow), ✅ int32/int64 (883 lines - production scale, Year 2038 awareness), ✅ int128/256/512 (723 lines - UUIDs, hashes, post-quantum)
- **Unsigned Integers**: ✅ uint8/uint16 (854 lines - double positive range, underflow traps!), ✅ uint32/uint64 (809 lines - production sizes, memory addresses), ✅ uint128/256/512 (834 lines - blockchain, bitmasks, extreme capacities)
- **Large Integers (Cryptography)**: ✅ int1024 (RSA-4096), ✅ int2048 (RSA-8192), ✅ int4096 (RSA-16384 **MAXIMUM**)
- **TBB (Twisted Balanced Binary)**: ✅ tbb8 (935 lines), ✅ tbb16 (1045 lines), ✅ tbb32 (1123 lines), ✅ tbb64 (1040 lines) - **COMPLETE FAMILY**
- **Special Values**: ✅ ERR (1016 lines - TBB error sentinel, sticky propagation), ✅ NIL (949 lines - optional types, safe navigation), NULL, comparisons
- **Safety-Critical**: ✅ fix256 (Q128.128 deterministic fixed-point - Phase 5.3 complete)
- **Generic Types**: ✅ complex<T> (generic complex numbers), atomic<T> (thread-safe), simd<T,N> (SIMD vectors)
- **Deterministic Floats (Twisted Floating Point)**: ✅ tfp32 (32-bit, ~4 digits), ✅ tfp64 (64-bit, ~14 digits)
- **Non-Deterministic Floats (IEEE 754)**: ✅ float (629 lines - 32-bit, ~7 digits, fast hardware), ✅ double (670 lines - 64-bit, ~15 digits, precision)
- **Exact Rationals**: ✅ frac8 (8-bit, ±127 range), ✅ frac16 (16-bit, ±32K range), ✅ frac32 (32-bit, ±2B production), ✅ frac64 (64-bit, ±9×10¹⁸ extreme precision)
- **Balanced Numbers**: ✅ trit/tryte (650 lines - balanced ternary, Q3 foundation), ✅ nit/nyte (709 lines - balanced nonary, Q9 foundation)
- **Extended Precision Floats**: ✅ flt128 (533 lines - quadruple, ~34 digits, 50-100× slow), ✅ flt256 (535 lines - octuple, ~70 digits, 100-500× slow), ✅ flt512 (407 lines - hexadecuple, ~150 digits, 500-2000× slow)
- **Vectors**: ✅ vec2 (540 lines - 2D graphics/physics, 8 bytes), ✅ vec3 (560 lines - 3D graphics/physics, 16 bytes), ✅ vec9 (580 lines - Nikola 9D consciousness, ATPM model)
- **Memory Safety**: ✅ Handle<T> (generational handles - P1-3 complete)
- **Dynamic/Object**: dyn, obj
- **Structured**: struct declarations, fields, generics, pointers
- **Collections**: string, array (declarations and operations)
- **Result/Function**: result type, error handling, function types
- **Advanced**: tensor, matrix, void (FFI only), pointers
- **Special Values**: NIL, NULL, ERR, comparisons
- **⚠️ CRITICAL**: **Zero Implicit Conversion Policy** - ALL literals require explicit type suffixes (safety requirement)

### 🧠 Memory Model (18 files)
Memory management and safety:
- **Allocation Strategies**: wild, gc, stack, wildx
- **RAII**: defer keyword
- **Safety Operators**: pinning (#), borrowing ($), immutable/mutable borrows
- **Pointer Operations**: @ operator, address-of, pointer syntax
- **Allocator Functions**: aria.alloc, aria.free, aria.gc_alloc, specialized allocators

### 🔄 Control Flow (21 files)
All control structures:
- **Conditionals**: if/else syntax
- **Loops**: while, for, loop (bidirectional), till (positive/negative step)
- **Advanced**: when/then/end, pick (pattern matching), fall (fallthrough)
- **Flow Control**: break, continue, return, pass, fail
- **Special**: $ iteration variable, dollar_variable syntax

### ⚙️ Operators (59 files)
Complete operator reference:
- **Assignment**: =, +=, -=, *=, /=, %=
- **Arithmetic**: +, -, *, /, %, ++, --
- **Comparison**: ==, !=, <, >, <=, >=, <=> (spaceship)
- **Logical**: &&, ||, !
- **Bitwise**: &, |, ^, ~, <<, >>
- **Special**: @ (address), # (pin), $ (iteration/borrow), ?. (safe nav), ?? (null coalesce)
- **Advanced**: ? (unwrap), |> <| (pipeline), .. ... (range), is (ternary), -> (pointer member)
- **Annotations**: : (type), . (member access), & (interpolation)
- **Template**: ` (backtick), string interpolation

### 📋 Functions (24 files)
Function system and advanced features:
- **Declarations**: func keyword, syntax, parameters, return types
- **Anonymous**: lambda syntax, closures, capture semantics
- **Higher-Order**: functions as arguments, immediate execution
- **Result Handling**: pass, fail keywords
- **Async**: async/await functions
- **Generics**: generic functions/structs, monomorphization, type inference, * prefix

### 📚 Modules (18 files)
Module system and FFI:
- **Import/Export**: use (import), mod (define), pub (visibility)
- **Paths**: relative, absolute, wildcard, selective imports
- **Advanced**: module aliases, nested modules
- **Conditional**: cfg, platform-specific compilation
- **FFI**: extern blocks, C interop, C pointers, libc integration

### 💾 I/O System (15 files) ✅ COMPLETE
6-stream topology (Hex-Stream): **4,270 lines**
- **Overview**: Six-stream architecture, separation of concerns
- **Standard Streams**: stdin, stdout, stderr (traditional)
- **Extended Streams**: stddbg (debug), stddati (data in), stddato (data out)
- **Stream Types**: text I/O, binary I/O, debug I/O
- **Architecture**: control plane vs data plane separation
- **Best Practices**: stream separation, deployment patterns

### 📖 Standard Library (23 files)
Built-in functionality:
- **I/O**: print, readFile, writeFile, readJSON, readCSV, openFile, streams
- **Process Management**: spawn, fork, exec, createPipe, wait
- **HTTP**: httpGet, HTTP client
- **Collections**: filter, transform, reduce, sort, reverse, unique
- **Math**: math operations, Math.round
- **System**: getMemoryUsage, getActiveConnections, diagnostics
- **Logging**: createLogger, structured logging, log levels

### 🚀 Advanced Features (26 files)
Language advanced capabilities:
- **Compile-Time**: const, comptime, compile-time computation
- **Concurrency**: async/await, coroutines, threading, atomics
- **Metaprogramming**: NASM-style macros, context stack
- **Patterns**: pattern matching, destructuring
- **Error Handling**: error propagation, Result<T> patterns
- **Syntax**: whitespace-insensitive, brace-delimited, semicolons, comments
- **Internals**: tokens, lexer, parser, AST
- **Best Practices**: code examples, idioms, common patterns

---

## Quick Reference

### File Organization

```
programming_guide/
├── types/               (74 files)  - Type system
├── memory_model/        (18 files)  - Memory management
├── control_flow/        (21 files)  - Control structures
├── operators/           (59 files)  - All operators
├── functions/           (24 files)  - Function system
├── modules/             (18 files)  - Module system & FFI
├── io_system/           (15 files)  - 6-stream I/O
├── standard_library/    (23 files)  - Built-in functions
├── advanced_features/   (26 files)  - Advanced topics
└── README.md           (this file)  - Master index
```

### Usage

Each `.md` file corresponds to a specific language feature and will contain:
- **Syntax**: Exact syntax with examples
- **Semantics**: What it does and how it works
- **Usage**: Common patterns and best practices
- **Gotchas**: Common mistakes and edge cases
- **Related**: Links to related topics
- **Examples**: Code snippets demonstrating usage

---

## Navigation Tips

1. **By Category**: Browse folders for related topics
2. **By Feature**: Use file names (self-documenting)
3. **Full-Text Search**: All files follow consistent naming
4. **Cross-References**: Files link to related topics

---

## ⚠️ CRITICAL: Type System Changes (January 9, 2026)

Aria now enforces **ZERO implicit type conversions**. ALL numeric literals MUST have explicit type suffixes.

**Quick Start:**
- See [Zero Implicit Conversion Policy](types/zero_implicit_conversion.md) for full explanation
- See [Type Suffix Reference](types/type_suffix_reference.md) for quick syntax guide

**Examples:**
```aria
// ❌ OLD (broken):
uint32:mask = 1;
int32:count = 0;

// ✅ NEW (required):
uint32:mask = 1u32;
int32:count = 0i32;
```

**Rationale:** Child-safety requirement for Nikola - no hidden assumptions or behavior.

---

## Development Status

- ✅ **Structure**: Complete (302 files created)
- 📝 **Content**: Pending (to be filled incrementally)
- 🔗 **Cross-Links**: Pending (after content)
- 📚 **Examples**: Pending (after content)

**Strategy**: Fill content file-by-file as needed during:
- Development breaks
- Feature implementation
- Testing and validation
- Documentation sessions

---

## Related Documentation

- [Aria Specifications](../../aria/docs/info/aria_specs.txt) - Language specification
- [Ecosystem Overview](../ECOSYSTEM_OVERVIEW.md) - System architecture
- [Type System](../specs/TYPE_SYSTEM.md) - Type theory details
- [FFI Design](../specs/FFI_DESIGN.md) - C interop patterns

---

**Status**: Ready for incremental content population  
**Repository**: https://github.com/alternative-intelligence-cp/aria_ecosystem  
**License**: AGPL-3.0

# Aria Programming Guide

**Status**: ✅ Phase 1 Complete (Type System), 🔄 Phase 2 Starting (Syntax Cleanup)  
**Total Topics**: 359 markdown files  
**Type Guides Complete**: 50 comprehensive guides (~41,273 lines)  
**Last Updated**: February 14, 2026 (Session 26 complete)

---

## Overview

This comprehensive programming guide covers every feature of the Aria programming language. Each topic has its own dedicated file for detailed documentation.

**Current Phase**: ✅ **Phase 1 COMPLETE** - All 50 type guides documented (Sessions 1-26). Starting Phase 2: Syntax corrections across remaining ~309 files.

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
- ✅ Session 27 (in progress): High-visibility syntax fixes (error_handling.md, README.md, filter.md)

See [UPDATE_PROGRESS.md](UPDATE_PROGRESS.md) for detailed status and [SYNTAX_AUDIT_FEB14_2026.md](SYNTAX_AUDIT_FEB14_2026.md) for Phase 2 roadmap.

---

## Phase 2: Syntax Cleanup (In Progress - Session 27)

**Identified Issues** (See [SYNTAX_AUDIT_FEB14_2026.md](SYNTAX_AUDIT_FEB14_2026.md) for details):
- 284 error handling syntax issues (55 files): `return Ok/Err` → `pass/fail`
- 495 lambda syntax issues (44 files): `=>` doesn't exist in Aria
- 517 loop syntax patterns: C-style `for` → Aria `till` with `$`
- 12 Result type signatures: `Result<T>` → `Result<T,E>`

**High-Visibility Fixes** (Session 27 - in progress):
- ✅ advanced_features/error_handling.md - Fixed all return Ok/Err → pass/fail
- ✅ README.md - Updated to reflect Session 26 completion
- 🔄 stdlib/filter.md - Converting => to proper Aria lambda syntax
- 🔄 SYNTAX_REFERENCE.md - Creating canonical syntax examples

**Next Steps** (Sessions 28-30):
- Batch fix remaining 54 files with error handling issues
- Convert all 44 files with lambda => syntax
- Document loop syntax patterns for till conversions

---

##Guide Structure

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

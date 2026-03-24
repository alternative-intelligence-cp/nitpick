# Aria 0.2.4 — Known Issues & Limitations

Last updated: July 2026

---

## Resolved Bugs (Bugs #1–#27 Fixed)

All 27 catalogued compiler bugs have been resolved for the 0.1.0 release.

| Bug | Description | Resolution |
|-----|-------------|------------|
| #16 | float64 function parameters crash SelectionDAG | Fixed: proper f64 ABI handling |
| #17 | ICmpInst assertion with flt64 + multiple if+pass | Fixed: correct type comparison |
| #18 | raw(pub_flt64()) + arithmetic → sint_to_fp crash | Fixed: float-aware raw extraction |
| #19 | extern float64 results overwritten by cvtsi2sd | Fixed: result register preservation |
| #20 | uint64 right-shift emits `ashr` instead of `lshr` | Fixed: unsigned shift codegen |
| #21 | uint64 literals ≥ 2^63 stored as 0 in LLVM IR | Fixed: APInt signedness handling |
| #24 | flt256/flt512 comparison crashes (anonymous structs) | Fixed: named structs + LBIM promotion |
| #25 | Spaceship operator assertion on LBIM types | Fixed: LBIM dispatch for `<=>` |
| Arrays-in-structs | Array fields stored pointer, not values | Fixed: element-by-element copy |
| Global arrays | Parser rejected arrays at module scope | Fixed: global scope support |
| Wild write-overhead | Result\<T\> not unwrapped in array stores | Fixed: extractvalue before store |
| Struct param fields | self.field returned 0 | Fixed: correct struct parameter access |
| BUG-001 | Result\<T\> comparison | Resolved |
| BUG-002 | flt64 compound assignment | Resolved |
| BUG-003 | Implicit Result\<T\>→T unwrap | Resolved |
| #26 | Assignment stores i64 literal into narrower alloca (type mismatch UB) | Fixed: type conversion before CreateStore |
| #27 | LBIM div/mod ABI mismatch with C runtime (sret/byval) | Fixed: correct sret+byval for by-value functions, sret+ptr for pointer functions |

---

## Resolved Bugs — Details

### Bug #26 — Assignment Type Mismatch (FIXED)

**Severity**: CRITICAL → RESOLVED  
**Discovered**: Safety-critical validation, May 2025  
**Fixed**: March 2026

**Root Cause**: Variable reassignment (`x = 5`) emitted `store i64 5, ptr %x` into an i32 alloca without type conversion. LLVM treated this as UB and optimized the store away, leaving the original value. This affected `pass(variable)` and `return variable` — they appeared to use "stale SSA" because the reassignment silently failed.

**Fix**: Added type conversion logic (trunc/sext/fptrunc/fpext/int↔float/LBIM promotion) before `CreateStore` in the assignment handler of ir_generator.cpp.

**Verification**: `int32:x = 0; x = 42; pass(x);` → EXIT=42 (was EXIT=0). All 10 safety-critical tests pass. All 27 ecosystem packages pass.

---

### Bug #27 — LBIM Division/Modulo ABI Mismatch (FIXED)

**Severity**: HIGH → RESOLVED  
**Discovered**: Safety-critical validation, May 2025  
**Fixed**: March 2026

**Root Cause**: The codegen for LBIM division and modulo declared C runtime functions with direct struct-to-struct LLVM IR signatures (`%struct.int1024 @func(%struct.int1024, %struct.int1024)`). For structs >64 bytes, the x86-64 SysV ABI requires MEMORY class passing (on the stack), but LLVM's backend tried to decompose the struct into registers — causing an ABI mismatch with the C++-compiled runtime.

Additionally, the runtime has two different calling conventions:
- int128-int1024 (lbim.cpp): `struct func(struct, struct)` — by value
- int2048-int4096 (lbim_extended.cpp): `struct func(const struct*, const struct*)` — by pointer

**Fix**: Changed LLVM IR declarations to use `void @func(ptr sret, ptr byval, ptr byval)` for by-value functions and `void @func(ptr sret, ptr, ptr)` for by-pointer functions. Also added scalar-to-struct promotion (e.g., `int4096:x / 3`).

**Verification**: `int1024: 1000000 / 7 = 142857` (was 0). `int4096: 3000000000 / 3 = 1000000000` (was segfault). All 10 safety-critical tests pass.

---

### int32 Signed Overflow — Safe Overflow Detection (By Design)

**Severity**: Informational  
**Status**: By design — documented behavior

**Description**: Aria's integer arithmetic uses `llvm.sadd.with.overflow` intrinsics (not raw `add nsw`). When overflow occurs, the result is replaced with the Unknown sentinel value (INT32_MAX for int32). This is intentional Layer 1 Safety behavior — overflow IS detected and converted to a known sentinel rather than producing undefined behavior.

**Behavior**:
```
int32:x = 2147483647;
int32:y = x + 1;
// y == 2147483647 (Unknown sentinel — overflow was detected)
```

**Impact on Nikola**: Use TBB (Ternary Balanced Binary) types for integer arithmetic where overflow detection with error propagation is needed — TBB types propagate overflow to ERR state via sticky error semantics.

---

## Current Limitations

### 1. Language Features Not Yet Implemented

- **Closures / Lambdas**: Not supported. Use named functions.
- **Traits / Interfaces**: `impl` blocks are parsed but not yet functional for user-defined traits.
- **Pattern matching**: No `match` / `switch` statement. Use `if`/`else if` chains.
- **Async / Await**: Basic async/await works (single-threaded coroutine model with promise-based error propagation). Task scheduling and async I/O with event loop not yet integrated.
- **Module system**: Single-file compilation only. No `import` or `use` statements across files. Multi-file programs require extern declarations or linking.
- **String type**: No built-in `string` type. Use `int8->` (pointer to byte array) with extern C functions, or the `aria-str` ecosystem package.

### 2. Type System

- **Generic types**: Limited to `Result<T>`, `struct<T>`, and standard library generics. User-defined generic functions/types are not supported.
- **Complex numbers**: The `complex` type is specified but not implemented in the compiler. Use the `aria-entangled` package for complex number operations.
- **SIMD types**: `vec2`, `vec3`, `vec4`, `vec9`, and SIMD intrinsics are specified but have limited compiler support.
- **TensorFloat types**: `tfp32`, `tfp64` are documented in the programming guide but not implemented.
- **Sub-byte integer types**: `int1`, `int2`, `int4` are specified but not supported as variable types.
- **Fractional types**: `frac8`, `frac16`, `frac32`, `frac64` are specified but not implemented.

### 3. Compiler Behavior

- **Compiler crash on optional types**: Using optional type syntax (e.g., `int64?:x = NIL;`) causes a compiler segfault. Optional types are specified but not yet safely handled. Workaround: Use `Result<T>` for fallible values instead.
- **Build warnings**: The compiler builds with 0 Aria-source warnings. ~23 warnings from LLVM headers remain (not actionable).
- **Error messages**: Some error messages include ANSI color codes. Pipe through `sed 's/\x1b\[[0-9;]*m//g'` to strip them for plain-text processing.
- **Void return type**: `void` is only valid in `extern` function declarations (for FFI). Use `NIL` for Aria functions that return no value.
- **Pointer syntax**: Use `->` for pointers in Aria code, not `*`. The `*` syntax is only valid in extern blocks for FFI compatibility.
- **Loop syntax**: Aria uses `till` loops (not `for-in`). Example: `till(int64:i = 0i64; i < 10i64; i = i + 1i64) { ... }`
- **Function closing**: Function bodies close with `};` (semicolon after brace), while `if`/`while`/`till` blocks close with `}` (no semicolon).

### 4. Standard Library

- **Minimal stdlib**: The standard library includes `core.aria` (Result, array generics), `print_utils.aria`, `string_builder.aria`, `string_convert.aria`, `number.aria`, `dbug.aria`, `atomic.aria`, `complex.aria`, `wave.aria`, `wavemech.aria`, and several hashmap/vector implementations.
- **No file I/O in stdlib**: File operations require extern C declarations (see `aria-json` package for an example).
- **No networking**: No built-in network support.

### 5. Tooling

- **aria-doc**: The API documentation generator (`build/aria-doc`) generates HTML output. Supports single-file and multi-file (package/directory) documentation generation. Generates unique pages for all functions, structs, enums, traits, type aliases, and constants. Index page lists all discovered items grouped by kind with signatures and links.
- **aria_make**: The build tool works correctly for ecosystem packages but must be run from the package directory.
- **Debugger**: No source-level debugger integration. Use `dbug` module for printf-style debugging.

### 6. Programming Guide

- **5,843 code examples**: The programming guide contains 5,843 `aria` code blocks across 357 files. All are fragments (no complete programs). Of those tested with auto-wrapping:
  - 217 (5%) compile successfully
  - The remaining 95% fail because they are contextual snippets referencing undefined identifiers, or demonstrate planned/future features (SIMD, Complex, tfp32, etc.)
- **Future-feature documentation**: Several guide sections document types and features that are specified but not yet implemented (Complex, SIMD, vec types, tfp32/64, fractional types, sub-byte integers). These are marked as part of the language specification but should not be expected to compile.

---

## Ecosystem Package Status

All 27 ecosystem packages pass their full test suites (540+ tests total, 0 failures):

| Package | Tests | Notes |
|---------|-------|-------|
| aria-args | 20/20 | — |
| aria-ascii | 20/20 | — |
| aria-audio | 20/20 | FFI shim for audio output |
| aria-bits | 20/20 | — |
| aria-buf | 20/20 | — |
| aria-clamp | 20/20 | — |
| aria-color | 20/20 | — |
| aria-console | 20/20 | — |
| aria-conv | 20/20 | — |
| aria-decision-t | 20/20 | — |
| aria-display | 20/20 | FFI shim |
| aria-endian | 20/20 | — |
| aria-entangled | 20/20 | — |
| aria-fixed | 20/20 | — |
| aria-freq | 20/20 | Overflow avoidance (idiomatic) |
| aria-gradient-field | 20/20 | — |
| aria-hash | 20/20 | — |
| aria-input | 20/20 | FFI shim |
| aria-json | 20/20 | — |
| aria-math | 23/23 | Includes fabs, acos, fmod |
| aria-mux | 20/20 | — |
| aria-rand | 20/20 | — |
| aria-resource-mem | 20/20 | — |
| aria-str | 20/20 | — |
| aria-uuid | 20/20 | — |
| aria-vec | 20/20 | — |
| aria-zigzag | 20/20 | — |

---

## Fuzzing Results

- **V2 fuzzer campaign**: 2,840 programs generated, 0 unresolved crashes
- 2 bugs found during fuzzing (#24, #25) — both fixed and verified
- Stress tests: 4 large stress test programs, all passing

---

## Safety-Critical Validation Results

A suite of 9 safety-critical test files (`tests/safety_critical/`) was created to validate Aria's numerical correctness for Nikola (9D Toroidal Waveform Intelligence). These tests specifically target the precision requirements of physics-based consciousness substrates where numerical drift can have catastrophic consequences.

| Test Suite | Assertions | Result | Notes |
|------------|-----------|--------|-------|
| IEEE 754 Compliance | 10 | ✅ PASS | Signed zero, infinity, subnormals, epsilon |
| Energy Conservation (Störmer-Verlet) | 3 | ✅ PASS | 10,000 steps @ 1ms, drift < 0.01% |
| Determinism Verification | 6 | ✅ PASS | Bit-identical across repeated runs |
| TBB Sticky Error Propagation | 6 | ✅ PASS | ERR never heals to valid value |
| Mathematical Field Identities | 10 | ✅ PASS | All field axioms hold |
| Type Casting Precision | 10 | ✅ PASS | All round-trips preserve values |
| Catastrophic Cancellation | 6 | ✅ PASS | Kahan summation verified, near-equal note |
| Large Integer (int1024) | 14 | ⚠️ KNOWN | Division returns 0 (Bug #27) |
| Overflow/Underflow Boundary | 10 | ⚠️ KNOWN | int32 overflow is UB (nsw flag) |

**Key findings**: The core floating-point arithmetic, energy conservation (critical for Nikola's Störmer-Verlet integration), determinism, and error propagation all pass. The two ⚠️ results are documented in the Open Bugs section above.

---

## Reporting Issues

Report issues at the project repository or contact the Aria development team.

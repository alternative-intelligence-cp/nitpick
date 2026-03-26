# Aria v0.2.13 — Known Issues & Limitations

Last updated: March 2026

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

- **Traits / Interfaces**: `impl` blocks are parsed but not yet functional for user-defined traits. RFC complete (monomorphized traits + `dyn Trait` opt-in, `$x`/`$mut x` borrows); implementation planned for a future release.
- **Async channels & actors**: Basic async/await works (coroutine model with promise-based error propagation). Task scheduling with actor model not yet integrated.

### 2. Type System

- **TensorFloat types**: `tfp32`, `tfp64` are documented in the programming guide but not implemented.
- **Sub-byte integer types**: `int1`, `int2`, `int4` are specified but not supported as variable types.
- **Fractional types**: `frac8`, `frac16`, `frac32`, `frac64` are specified but not implemented.

### 3. Compiler Behavior

- **Compiler crash on optional types**: Using optional type syntax (e.g., `int64?:x = NIL;`) causes a compiler segfault. Optional types are specified but not yet safely handled. Workaround: Use `Result<T>` for fallible values instead.
- **Build warnings**: The compiler builds with 0 Aria-source warnings. Some warnings from LLVM headers remain (not actionable).
- **Error messages**: Some error messages include ANSI color codes. Pipe through `sed 's/\x1b\[[0-9;]*m//g'` to strip them for plain-text processing.
- **88 TODO comments** in compiler source — mostly implementation placeholders for vector components, optional types, array indexing edge cases.
- **2 HACK comments** — `codegen_expr.cpp` (single-field struct assumption), `parser.cpp` (GPU/PTX testing).

### 4. WebAssembly Target

- **No threading**: WASM target is single-threaded (no async, thread pools, mutexes).
- **No process spawning**: WASM sandbox restriction — no fork/exec.
- **No signals or mmap**: WASM uses linear memory model.
- **No native FFI**: WASM has its own import model; native `.so`/`.dll` FFI not available.
- **File I/O requires WASI**: File operations only work in WASI-compatible runtimes, not bare WASM.

### 5. Programming Guide

- **Code examples are fragments**: The programming guide contains thousands of code blocks, most of which are contextual snippets (not complete programs). Some document future/planned features (SIMD vectors, Complex, tfp32, fractionals).

### 6. Tooling

- **aria_make**: The build tool works correctly for ecosystem packages but must be run from the package directory.

---

## Ecosystem Package Status

All 74 ecosystem packages in [`aria-packages`](https://github.com/alternative-intelligence-cp/aria-packages) pass their test suites.

**Package tiers:**
- **39 utility/GUI packages** — math, strings, crypto, compression, graphics (raylib, SDL2, GTK4, OpenGL), games (Tetris), editor
- **4 database clients** — SQLite, PostgreSQL, MySQL, Redis (all with parameterized queries)
- **7 server/middleware** — HTTP server, Express-style router, body-parser, CORS, cookie, session, rate-limit, static file serving
- **8 AI/ML** — tensor, CUDA, Transformer, Mamba, Jamba, Looping, UACP protocol
- **16 additional** — CLI, env, semver, template, URL, websocket, XML, YAML, and more

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
| Large Integer (int1024) | 14 | ✅ PASS | Bug #27 fixed (LBIM ABI) |
| Overflow/Underflow Boundary | 10 | ✅ PASS | Overflow detected via intrinsics |

**Key findings**: The core floating-point arithmetic, energy conservation (critical for Nikola's Störmer-Verlet integration), determinism, and error propagation all pass. All 9 safety-critical test suites pass.

---

## Reporting Issues

Report issues at the project repository or contact the Aria development team.

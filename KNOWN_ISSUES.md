# Aria v0.13.7 — Known Issues & Limitations

Last updated: April 4, 2026

> **Note:** The canonical KNOWN_ISSUES.md is in the [`aria`](https://github.com/alternative-intelligence-cp/aria) repo. This is a working copy for internal tracking.

---

## Resolved Bugs (by version)

### Resolved in v0.13.x

| ID | Description | Resolution | Version |
|----|-------------|------------|---------|
| BUG-06 | `_test` filename segfault | Fixed module name generation | v0.13.0 |
| BUG-09 | Computed `fixed` constants zeroed on import | Fixed constant folding codegen | v0.13.0 |
| STUBS | 12 stub implementations across compiler/runtime | All replaced with real implementations | v0.13.1 |
| FEAT-06 | Multi-file linking not supported | Implemented | v0.13.2 |
| FEAT-10 | Extern block ≤7 limit | Removed limit | v0.13.2 |
| — | `@func_name` function pointer syntax broken | Trampoline wrapper generation | v0.13.6 |
| — | ahash missing `ahdelete`, `ahhas`, `ahclear`, `ahkeys` | Full implementation with tombstone support | v0.13.5 |
| — | Variadic/rest/spread not implemented | `..?` (variadic/rest) and `..^` (spread) operators | v0.13.5 |

### Resolved in v0.4.6

| ID | Description | Resolution |
|----|-------------|------------|
| BUG-14 | Float `!=` returned false when comparing NaN | Changed `CreateFCmpONE` → `CreateFCmpUNE` |
| BUG-17 | LLVM module verifier was commented out | Re-enabled as hard error |
| FEAT-03 | `ahkeys` misleading name (returns count, not keys) | Renamed to `ahcount` |
| FEAT-04 | astack builtins only accepted keyword syntax | All 8 builtins now accept optional parentheses |

### Resolved in v0.1.0–v0.4.x

| Bug | Description | Fixed In |
|-----|-------------|----------|
| #16 | float64 function parameters crash SelectionDAG | v0.1.0 |
| #17 | ICmpInst assertion with flt64 + multiple if+pass | v0.1.0 |
| #18 | raw(pub_flt64()) + arithmetic → sint_to_fp crash | v0.1.0 |
| #19 | extern float64 results overwritten by cvtsi2sd | v0.1.0 |
| #20 | uint64 right-shift emits `ashr` instead of `lshr` | v0.1.0 |
| #21 | uint64 literals ≥ 2^63 stored as 0 in LLVM IR | v0.1.0 |
| #24 | flt256/flt512 comparison crashes (anonymous structs) | v0.1.0 |
| #25 | Spaceship operator assertion on LBIM types | v0.1.0 |
| #26 | Assignment stores i64 literal into narrower alloca | v0.2.x |
| #27 | LBIM div/mod ABI mismatch with C runtime | v0.2.x |
| BUG-008 | `?!` requires i32-suffixed literals | v0.4.x |
| BUG-009 | `.error` returns int64, not int32 | v0.4.x |
| BUG-010 | tbb32 cannot compare with int32 literals | v0.4.x |
| BUG-011 | Cannot borrow from struct field expression | v0.4.x |

---

## Current Limitations (v0.13.7)

### 1. ABI & FFI

- **flt32 ABI**: Aria's `flt32` passes as `double` at C ABI boundary. C shims MUST use `double` params, then cast internally.
- **String ABI** (asymmetric): `string` params → `const char*` in C. `string` returns → `AriaString {char*, int64_t}` by value.
- **Extern pointer returns**: Create `{ i1, ptr }` optional wrapper — can corrupt struct fields. Workaround: use `int64` for handle types.
- **`pass(extern_returning_struct())`**: Store in variable first, then `pass(variable)`. Safe for primitives.

### 2. Codegen Quirks

- **`fixed` with arithmetic**: `fixed int64:NEG1 = 0i64 - 1i64` may evaluate to `0`. Use variable initialization instead.
- **Negative constants imported via `use` are zeroed**: Compute negative values inline as `0i64 - value`.
- **Non-pub helper function calls crash codegen**: `func:_helper` called from `pub func:` crashes. Workaround: inline helper logic.
- **Large functions with many if-blocks**: Can cause segfaults. Workaround: split into smaller functions.
- **Lambda closures don't capture locals or globals**: Can only call extern functions. Design improvement planned.
- **`@func_name` as call argument**: Type checker mismatch when passing as function argument (as opposed to initializer). Workaround: assign to variable first.

### 3. Reserved Words

Cannot use as variable/parameter names: `max`, `end`, `raw`, `ok`, `is`, `stream`, `limit`, `binary`, `pipe`, `process`, `debug`, `write`.

### 4. Syntax Rules

- `if` requires parentheses: `if (cond) { }`
- Control flow has NO trailing semicolons: `loop(...){}`, `pick(...){}`, `when(...){}`, `if(...){}` 
- Bitwise operators (`&`, `|`, `^`) require unsigned types
- `pick()` requires `(*)` wildcard case for infinite domains
- `NIL` for non-extern return types; `void` only in extern blocks
- `loop`/`till` iterator variable is `$` (dollar sign)
- `fixed` not `const` for immutable bindings; `const` only in extern blocks

### 5. WebAssembly Target

- No threading, process spawning, signals, mmap, or native FFI
- File I/O requires WASI-compatible runtime

### 6. Runtime

- **`ahget` on missing key segfaults**: Always check `ahtype(h,k) >= 0` first.
- **`ahset` silently returns -1 on capacity overflow**: Check return value.
- **Nested module function calls (A→B, both pub)**: Can trigger GC OOM in some cases.
- **Variables defined before `loop()`**: May become inaccessible after loop exits in some cases.

---

## Test Suite Status (v0.13.7)

959 test files:
- **827 PASS** (86.2%)
- **78 expected compile errors** (negative tests — borrow checker, type mismatch, enforcement)
- **33 adversarial tests** (designed to test error detection)
- **13 fuzz-generated tests** (designed to fail compilation)
- **6 @skip-test** (helper modules, no main)
- **2 Z3 verification tests** (require --verify flags)
- **0 genuine failures**

### Fuzzer Status
- v0.13.0 campaign: ~17h running, 4600+ tests, **0 crashes, 0 timeouts**
- v0.13.6 binary frozen for continued fuzzing

---

## Ecosystem Package Status

102+ ecosystem packages in aria-packages. Standard library: 57 modules (13 pure, 20 aria-libc, 24 FFI).

---

## Safety-By-Design Notes

### int32 Signed Overflow
Aria's integer arithmetic uses `llvm.sadd.with.overflow` intrinsics. Overflow is replaced with Unknown sentinel (INT32_MAX for int32). This is intentional Layer 1 Safety — overflow detected, not UB.

Use TBB types for arithmetic where overflow detection with error propagation is needed — TBB types propagate to ERR state via sticky error semantics.

### Safety-Critical Validation (9 suites, all passing)
IEEE 754 compliance, energy conservation (Störmer-Verlet), determinism, TBB sticky errors, field identities, type casting precision, catastrophic cancellation, large integers (int1024), overflow/underflow boundaries.

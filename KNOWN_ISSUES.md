# Nitpick v0.20.3 — Known Issues & Limitations

Last updated: v0.20.3

> **Note:** The canonical KNOWN_ISSUES.md is in the [`nitpick`](https://github.com/alternative-intelligence-cp/nitpick) repo. This is a working copy for internal tracking.

---

## Resolved Bugs (by version)

### Resolved in v0.19.x

| ID | Description | Resolution | Version |
|----|-------------|------------|---------|
| bug072 | Non-pub helper called from `pub func:` crashes codegen | Intra-module calls work regardless of visibility | v0.19.3 |
| bug073 | `ahget` on missing key segfaults | Returns zero; always check `ahtype(h,k) >= 0` | v0.19.3 |
| bug074 | `@func_name` as call argument: type-checker mismatch | Assign to lambda variable first, then pass | v0.19.3 |
| bug075 | Variables declared before `loop()` inaccessible after exit | Pre-loop variables remain in scope after loop | v0.19.3 |
| bug076 | `Result<T>` in arithmetic emitted misleading error | Error message improved | v0.19.3 |
| bug077 | Large pick (30+ arms) triggers codegen segfault | Chunked codegen for large pick statements | v0.19.3 |

### Resolved in v0.19.1

| ID | Description | Resolution | Version |
|----|-------------|------------|---------|
| — | `pass(extern_returning_struct())` required temp variable | Direct pass of extern-returned struct now works | v0.19.1 |

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

### Resolved in v0.4.6 and earlier

| ID | Description | Fixed In |
|----|-------------|----------|
| BUG-14 | Float `!=` returned false when comparing NaN | v0.4.6 |
| BUG-17 | LLVM module verifier was commented out | v0.4.6 |
| #16–#27 | Various float/integer/ABI crashes | v0.1.0–v0.2.x |
| BUG-008–011 | `?!` literals, `.error` type, tbb32 compare, field borrow | v0.4.x |

---

## Current Limitations (v0.20.3)

### High Severity

*(none — all high-severity issues resolved as of v0.20.3)*

### Medium Severity

- **Lambda closures do not capture scope** (Medium): Lambda function pointer
  variables (`(T)(U):f = T(U:x) { ... }`) cannot capture variables from the
  enclosing scope. All values must be passed as explicit parameters. Captured-
  environment closures are planned for a future release.

- **`ahset` silently returns -1 on capacity overflow** (Medium): Check the
  return value of `ahset` — a return of -1 means the table is full and the
  key was not inserted.

- **Nested module function calls (A→B, both pub)** (Medium): May trigger GC
  OOM in pathological cases. Avoid deep pub-pub chains across modules in
  tight memory environments.

- **`pick` range arms not exhaustiveness-checked for `int32`/`int64`** (Medium):
  The exhaustiveness checker treats `int32` and `int64` as infinite domains
  (their value spaces are 2³² and 2⁶⁴ respectively). Range arms like `0..100:`
  do not contribute to exhaustiveness analysis for these types — you must always
  include a `(*):` wildcard arm. For `int8`, `int16`, and all `tbb*` types,
  range arms are fully tracked and a wildcard is not required when all values
  are covered.

### Low Severity

- **flt32 ABI** (Low): `flt32` passes as `double` at the C ABI boundary. C
  shims must accept `double` params and cast internally.

- **String ABI is asymmetric** (Low): `string` params → `const char*` in C.
  `string` returns → `AriaString {char*, int64_t}` by value (not pointer).

- **Extern pointer returns** (Low): `{ i1, ptr }` optional wrapper can corrupt
  struct fields. Use `int64` for handle types in extern blocks.

- **`fixed` with compile-time arithmetic** (Low): `fixed int64:NEG1 = 0i64 - 1i64`
  may evaluate to `0`. Compute negative values at runtime instead.

- **Negative constants imported via `use` are zeroed** (Low): Compute negative
  values inline as `0i64 - value` rather than importing from another module.

### Syntax & Design Notes

- `if` requires parentheses: `if (cond) { }`
- Control flow blocks have no trailing semicolons: `loop(...){}`, `pick(...){}`, `if(...){}` etc.
- Bitwise operators (`&`, `|`, `^`) require unsigned types
- `pick()` requires a `(*)` wildcard case for types with infinite domains
- `NIL` for non-extern void returns; `void` only in extern blocks
- `loop`/`till` iteration variable is `$` (reserved — cannot be reused in loop body)
- `fixed` not `const` for immutable bindings; `const` only in extern blocks
- Reserved names (cannot use as variable/parameter): `max`, `end`, `raw`, `ok`, `is`, `stream`, `limit`, `binary`, `pipe`, `process`, `debug`, `write`

### WebAssembly Target

- No threading, process spawning, signals, mmap, or native FFI
- File I/O requires WASI-compatible runtime

---

## Test Suite Status (v0.19.5)

- **831 passing** test files
- **78 expected compile errors** (negative tests — borrow checker, type mismatch)
- **33 adversarial tests** (designed to test error detection)
- **13 fuzz-generated tests** (designed to fail compilation)
- **6 `@skip-test`** (helper modules, no `main`)
- **2 Z3 verification tests** (require `--verify` flags)
- **0 genuine failures**
- **K core**: 127/127 (K Framework v7.1.320)
- **K proofs**: 10/10

### Fuzzer Status
- v0.19.x campaign (EMI, 500 variants): **0 miscompiles**
- v0.13.6 binary frozen for continued fuzzing; 0 crashes, 0 timeouts

---

## Ecosystem Package Status

103+ ecosystem packages in nitpick-packages. Standard library: 57 modules (13 pure, 20 aria-libc, 24 FFI).

---

## Safety-By-Design Notes

### Integer Overflow
Nitpick's integer arithmetic uses `llvm.sadd.with.overflow` intrinsics. Overflow is replaced with an Unknown sentinel (INT32_MAX for int32). This is intentional Layer 1 Safety — overflow is detected, not undefined behavior.

Use TBB types for arithmetic where overflow detection with error propagation is needed — TBB types propagate to ERR state via sticky error semantics.

### Safety-Critical Validation (9 suites, all passing)
IEEE 754 compliance, energy conservation (Störmer-Verlet), determinism, TBB sticky errors, field identities, type casting precision, catastrophic cancellation, large integers (int1024), overflow/underflow boundaries.

# Nitpick v0.20.5 — Known Issues & Limitations

Last updated: v0.20.5

> **Note:** The canonical KNOWN_ISSUES.md is in the [`nitpick`](https://github.com/alternative-intelligence-cp/nitpick) repo. This is a working copy for internal tracking.

---

## Resolved Bugs (by version)

### Resolved in v0.20.x

| ID | Description | Resolution | Version |
|----|-------------|------------|---------|
| — | `vec9` dynamic indexing throws `std::runtime_error` at runtime | ICmpEQ type mismatch fixed in `ir_generator.cpp`; read and write paths now cast loop index to i32 before select loop | v0.20.5 |
| — | `UNUSED_FUNCTION` and `EMPTY_BLOCK` warnings not emitted | Warning pass implemented | v0.20.0 |
| — | `%error`/`%warning` directives not implemented | Preprocessor directives implemented | v0.20.1 |
| — | Struct interpolation in template literals not supported | `Display` trait and struct interpolation implemented | v0.20.2 |
| — | `comptime` evaluator Step 7 (struct comparison) stub | Const evaluator Step 7 complete | v0.20.3 |
| — | Closure `validateLifetimes` stub (no escape detection) | Closure lifetime validation implemented | v0.20.4 |
| — | `optional<T>` not in type system | `optional<T>` with safe navigation (`?.`) and null coalescing (`?|`) implemented | v0.20.4 |

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

- **`ahset` silently returns -1 on capacity overflow** (Medium): Check the
  return value of `ahset` — a return of -1 means the table is full and the
  key was not inserted.

- **Nested module function calls (A→B, both pub)** (Medium): May trigger GC
  OOM in pathological cases. Avoid deep pub-pub chains across modules in
  tight memory environments.

- **`pick` range exhaustiveness for `int32`/`uint32`/`int64`** — Fixed in v0.21.3:
  The exhaustiveness checker now routes `int32`, `uint32`, and `int64` through
  INTEGER_RANGE analysis. Range arms contribute to coverage and the compiler
  lists uncovered intervals in the diagnostic. A `(*)` wildcard is still
  required unless all values are covered by range arms. `uint64` remains
  treated as an infinite domain (its max value exceeds `int64_t`).
  Fixed: commit tagged v0.21.3.

### Low Severity

- **flt32 ABI** (Low): `flt32` passes as `double` at the C ABI boundary. C
  shims must accept `double` params and cast internally.

- **String ABI is asymmetric** (Low): `string` params → `const char*` in C.
  `string` returns → `AriaString {char*, int64_t}` by value (not pointer).

- **Extern pointer returns** (Low): `{ i1, ptr }` optional wrapper can corrupt
  struct fields. Use `int64` for handle types in extern blocks.

- **`fixed` with compile-time arithmetic** -- Fixed in v0.13.0 (BUG-09):
  `fixed int64:NEG1 = 0i64 - 1i64` now correctly evaluates to `-1`.
  Verified in v0.21.3. Negative `fixed` values are valid and supported.

- **Negative constants imported via `use` are zeroed** (Low): Compute negative
  values inline as `0i64 - value` rather than importing from another module.

### Syntax & Design Notes

- `if` requires parentheses: `if (cond) { }`
- Control flow blocks have no trailing semicolons: `loop(...){}`, `pick(...){}`, `if(...){}` etc.
- Bitwise operators (`&`, `|`, `^`) require unsigned types
- `pick()` requires a `(*)` wildcard case for types with infinite domains (e.g. `string`, `uint64`); for `int32`, `uint32`, `int64`, `int8`, `int16`, and all `tbb*` types, range arms are tracked and `(*)` is only needed if the range arms do not cover all values
- `NIL` for non-extern void returns; `void` only in extern blocks
- `loop`/`till` iteration variable is `$` (reserved — cannot be reused in loop body)
- `fixed` not `const` for immutable bindings; `const` only in extern blocks
- Reserved names (cannot use as variable/parameter): `max`, `end`, `raw`, `ok`, `is`, `stream`, `limit`, `binary`, `pipe`, `process`, `debug`, `write`

### WebAssembly Target

- No threading, process spawning, signals, mmap, or native FFI
- File I/O requires WASI-compatible runtime

---

## Test Suite Status (v0.20.5)

- **18/18 CTest** suites passing (all CTest registered tests pass)
- **K core**: passes (k_semantics_core test)
- **K proofs**: passes (k_semantics_proofs test)
- **vec9 dynamic indexing**: 1/1 (new in v0.20.5)
- **Closure lifetime tests**: 1/1 (v0.20.4)
- **Optional safe navigation**: 1/1 (v0.20.4)
- **0 genuine failures**

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

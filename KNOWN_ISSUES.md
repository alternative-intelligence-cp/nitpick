# Nitpick v0.31.1.10 — Known Issues & Limitations

Last updated: v0.31.1.10

> **Note:** The canonical KNOWN_ISSUES.md is in the [`nitpick`](https://github.com/alternative-intelligence-cp/nitpick) repo. This is a working copy for internal tracking.

---

## Deferred — v0.31.1.x (Trait / dyn surface)

Closed in Phase 2 of the v0.31.x cycle but explicitly carried forward:

- **Re-borrow from a `$$m dyn T` parameter** — passing `b` onward as `$$m`
  to another call is treated conservatively: the callee binds the fat ptr
  to a struct alloca without setting the `__borrow_param_mut` marker for
  `b`. No current fixture exercises this path; revisit if a real program
  demands it.
- **Field-level borrows of a `dyn T` struct field** — not supported;
  borrow the whole containing struct.
- **Trait inheritance / super-traits** — out of scope for Phase 2.
- **Associated types / associated constants** — out of scope for Phase 2.
- **Generic trait bounds** (`func:f = T($$i T:x) where T: Trait`) — out of
  scope for Phase 2.
- **`derive` for arbitrary traits** — only `derive(Display)` is supported
  (v0.21.1).
- **`obj` (owned trait object)** — distinguishing semantics from `dyn`
  was deferred at v0.31.1.0 (D-9 follow-on).

---

## Deferred — v0.25.x (Borrow Checker Hardening)

The items below are explicitly out of scope for the v0.25.x cycle and
carried forward to v0.26.x triage:

- **Flow-sensitive borrow release across `if` / `else` arms** — releasing in
  one arm does not free the host in the join.
- **Implicit two-phase borrows** — methods take their receiver loan up
  front for the whole call; explicit scope-release is required.
- **Closure capture for non-primitive types** — defaults to BY_VALUE;
  capture-by-reference is currently supported only for primitive scalars.
- **IR-gen path-arg lowering for `$$m` parameters** — the borrow checker
  enforces the rules but codegen still uses the existing pointer-passing
  convention.
- **Cross-module return-borrow inference** — summary-based, not global;
  novel cross-module shapes may require explicit scoping.
- **K model field-path depth** — capped at 3 levels (`a.b.c`); deeper
  paths exist in the parser but are not yet expressible in the K model.
- **Out-of-bounds array access** — not borrow-checked; lives in the
  verification stack (KLEE / CBMC).

---

## Deferred — v0.24.x

### Comptime — Planned But Not Yet Implemented

- **`--comptime-budget` flag** — CTFE step cap is documented as configurable
  but the CLI flag is not exposed; the cap is a fixed compile-time constant.
- **`--trace-comptime=<funcname>` flag** — documented in
  `aria-docs/guide/comptime/debug.md` as planned; not implemented.
- **Comptime arrays of struct values** evaluate correctly inside comptime
  but cannot yet be embedded directly into IR globals; large lookup tables
  should be built inside a `comptime { ... }` returning the final scalar
  or small array.

---

## Deferred — v0.23.x

### MACRO-007 — Complex Code-Generating Macros (Deferred)
Macros that generate code requiring a C-shim bridge (heavy FFI body generation)
are deferred. Current macros support all pure-Nitpick body patterns. C-bridging
code generation macros require additional codegen work planned for v0.25.x+.

---

## Resolved Bugs (by version)

### Resolved in v0.31.1.x (Phase 2 — trait / impl / dyn)

| Slice | Bugs | Theme |
|-------|------|-------|
| v0.31.1.0 – v0.31.1.6 | bug363 – bug370 | `dyn T` surface scaffolding (local var, struct field, fn arg, heterogeneous branch, no-impl diagnostic ARIA-043, two-method dispatch) |
| v0.31.1.7 | bug371 | Probe C `$$i dyn T` local borrow + dispatch |
| v0.31.1.8 | bug372, bug373 | impl-method `$$m self` lowering; `$$m dyn T` local borrow mutation |
| v0.31.1.9 | bug374 | `$$m dyn T` parameter coercion (call-site + callee-side ABI) |
| v0.31.1.10 | bug375 – bug378 | Probe D regression slice: dyn-borrow source-side conflict rules (ARIA-019/023/026 fire uniformly for `dyn T` borrows) |

### Resolved in v0.25.x

| Slice | Bugs | Theme |
|-------|------|-------|
| v0.25.0 | bug173 – bug174 | `TILL` body borrow checking, `FAIL` dispatch |
| v0.25.1 | bug175 – bug178 | `defer` body borrow tracking; early-exit leak audit |
| v0.25.2 | bug179 – bug183 | Multi-dim and nested array borrow paths |
| v0.25.3 | bug184 – bug189 | Deep struct field paths and `ptr->field` |
| v0.25.4 | bug190 – bug194 | Inter-procedural parameter intent and return-borrow lifetime |
| v0.25.5 | bug195 – bug199 | Two-phase borrows; `$$m self` in trait impls |
| v0.25.6 | bug200 – bug204 | Closure capture borrows; multi-await polish; ARIA-023/026 secondary spans |
| v0.25.7 | (no new bugs) | K core tests 143/144/145; `guide/borrow/` cookbook; cycle audit |

### Resolved in v0.24.x

| ID | Description | Resolution | Version |
|----|-------------|------------|---------|
| (unnamed) | `string:x = comptime("...")` segfaulted at runtime — `COMPTIME_EXPR` codegen emitted raw `i8*` instead of `AriaString` struct | Construct `struct.NpkString {data, length}` global mirroring string-literal codegen | v0.24.7 |
| (unnamed) | `inferComptimeExpr()` failed on intrinsic chains like `@typeInfo(T).fields.x.type_name` | Refactored to evaluate-first; only fall back to `inferType()` when evaluation fails | v0.24.7 |

### Resolved in v0.23.x

| ID | Description | Resolution | Version |
|----|-------------|------------|---------|
| MACRO-003 | Macro variable hygiene | gensym/cloneAST per call site | v0.23.2 |
| (unnamed) | `case RETURN:` dropped from `checkStatement()` in v0.23.5 | Restored in `ba66d8a` | v0.23.6 |


### Resolved in v0.22.x

| ID | Description | Resolution | Version |
|----|-------------|------------|---------|
| POLISH-001 | `eprint`/`eprintln` not wired in type checker/codegen | Implemented; stdlib builtins | v0.22.2 |
| POLISH-002 | `.aria` extension rejected by module resolver | `isValidSourceFile()` accepts both `.npk` and `.aria` | v0.22.2 |
| POLISH-003 | `npk_arg(i)` broken ABI | Replaced with `get_argc`/`get_argv` builtins | v0.22.3 |
| POLISH-004 | File extension conventions undocumented | `use_import.md` File Extension Conventions section | v0.22.6 |
| POLISH-006 | `pick` on integer values untested | Regression tests added; already worked | v0.22.4 |
| POLISH-007 | Bitwise ops on int32 variables untested | Regression tests added; already worked | v0.22.4 |
| POLISH-008 | Reserved keyword as var name → cryptic error | Parser emits friendly message with suggestions | v0.22.6 |
| POLISH-009 | `\xNN` / `\u{NNNN}` escape sequences unsupported | Added to all three string scanners in lexer | v0.22.5 |
| POLISH-010 | "type X but expects X" multi-module import clash | `checkFuncDecl()` in-place symbol update | v0.22.2 |
| POLISH-011 | `break`/`continue` not in loop bodies | Already implemented end-to-end; tests added | v0.22.3 |
| POLISH-012 | `pass n;` not counted as variable use | `collectIdentifiers()` PASS case added | v0.22.1 |
| POLISH-013 | `print()` vs C stdio buffering interop | `ffi_advanced.md` Stdio Buffering section | v0.22.1 |
| POLISH-014 | `while` body not scanned by unused-var checker | `collectIdentifiers()` WHILE+6 cases added | v0.22.1 |
| — | `isValidAriaFile()` pre-rebrand name | Renamed to `isValidSourceFile()` | v0.22.7 |

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

- **`ahset` capacity overflow** — Fixed in v0.4.5 (B7) and verified in
  v0.21.5: `npk_uhash_set` auto-grows the hash table on capacity overflow
  rather than returning `-1`. The runtime now always returns `0` from a
  successful insert; the previously documented `-1` overflow return is
  unreachable. Regression test: `tests/bugs/bug103_ahset_autogrow_pass.npk`.

- **Nested module function calls (A→B, both pub)** (Medium): May trigger GC
  OOM in pathological cases. Avoid deep pub-pub chains across modules in
  tight memory environments. v0.21.5 investigation: 4-level chains and
  16-leaf hub graphs compile and run cleanly under 70 MB; the failure mode
  appears only on much larger or cyclic graphs and could not be reproduced
  on small/medium fixtures. Floor regression test:
  `tests/bugs/bug104_pub_pub_chain_pass.npk` (4-level chain).

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

## Test Suite Status (v0.21.6)

- **24/24 CTest** suites passing (all CTest registered tests pass)
- **K core**: 139/139 (k_semantics_core test) — up from 127 (v0.21.5: 127, v0.21.6 adds tbb8/16/64 + async/await tests)
- **K proofs**: 10/10 (k_semantics_proofs test)
- **bug_tests_v0216**: 2/2 (bug105 tbb variants, bug106 async/await)
- **0 genuine failures**

### A-009 — tbb8/tbb16/tbb64 K Semantics (Partially Addressed — v0.21.6)
K now formalizes all four TBB types (tbb8, tbb16, tbb32, tbb64) with arithmetic
and ERR sentinels. Remaining type lattice gaps (int128, flt arithmetic, etc.) are
documented in `k-semantics/SEMANTIC_GAPS.md`.

### A-010 — async/await K Semantics (Partially Addressed — v0.21.6)
K now models `async func:` declarations and `await` (synchronous model). True
coroutine frame semantics (suspension/resume) remain unformalized. See
`k-semantics/SEMANTIC_GAPS.md`.

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

# Aria v0.18.0 K Semantics Status

Date: April 30, 2026
Branch: `dev-0.18.x`

## Completed in this slice

- Created `k-semantics/aria.k` as the first K Framework executable-semantics seed.
- Added core tests for exit, arithmetic, mutable/fixed binding, loops, Result
  unwrapping/fallback, sticky `ERR`, failsafe routing, and `if/else`.
- Added `run_k_tests.sh` with CTest-friendly skip behavior when K is absent.
- Added install notes capturing the current local toolchain state.
- Installed K Framework v7.1.320 through `kup` and fixed the runner for the
  current `kompile --output-definition` CLI.
- Added typed internal numeric values for `int32`, `int64`, and `tbb32`.
- Modeled two's-complement `int32`/`int64` bounds and `tbb32` min-sentinel
  overflow/underflow to sticky `ERR`.
- Added zero-argument helper functions returning through `pass`/`fail`, callable
  through expressions such as `raw answer()` and `broken() defaults 43`.
- Added one- and two-argument helper functions with isolated call frames that
  restore caller locals, types, store, fixed bindings, and allocator state.
- Added canonical one-, two-, and three-field struct declarations, typed struct
  literals, field reads, direct field writes, and struct helper parameters.
- Added `pick`/`fall` executable semantics for first-match value dispatch,
  `(*)` wildcards, optional labeled arms, and `fall label;` jumps.
- Added top-level untyped `Rules:` declarations, integer `$` rule conditions,
  cascaded `limit<OtherRules>` references, and `limit<Rules>` declaration and
  reassignment checks for numeric values.
- Added failsafe routing for violated `limit<Rules>` constraints while
  preserving and restoring the previous `$` / loop-index value after checks.
- Added `run_k_proofs.sh`, a Haskell-backend `kprove` proof runner with
  CTest-friendly skip behavior when K is absent.
- Added `proofs/core-proofs.k` with three concrete executable-core claims for
  sticky `ERR`, bounded `int32` wrapping, and `tbb32` overflow-to-`ERR` behavior.
- Added string literals plus `print`/`println` stdout modeling with optional
  `// expect-stdout:` assertions in the K runner.
- Added first memory allocation qualifier slice: `stack`, `gc`, and `wild`
  declaration syntax, memory classification cells, minimal pointer/`alloc`
  values, `free(id)`, and wild-leak failsafe routing.
- Compiled `aria.k` and passed all 40 core K tests under `krun`.
- Proved the first `kprove` proof module under K Framework v7.1.320.
- Ignored generated K build output at `/k-semantics/.build/`.

## Local toolchain state

- `kompile`: K Framework v7.1.320 via `kup`
- `krun`: K Framework v7.1.320 via `kup`
- `kprove`: K Framework v7.1.320 via `kup`
- Java: OpenJDK 21 available
- Z3: 4.16.0 available
- Docker: installed, but current user lacks Docker socket permission

## Validation performed

- `./k-semantics/run_k_tests.sh --require-k`: 40 passed, 0 failed.
- `bash ./k-semantics/run_k_proofs.sh --require-k`: 1 proof module passed, 0 failed.
- Cross-checked the new `Rules` / `limit<Rules>` K tests with `build/ariac`;
  expected exits matched actual process exits for all four new programs.
- Cross-checked the new `stack`/`gc` and `wild`/`free` pass cases with
  `build/ariac`; expected exits matched actual process exits.
- `git diff --check`: passed.
- `ctest --test-dir build -R '^k_semantics_core$' --output-on-failure -V`:
  `k_semantics_core` passed with K enabled.
- `ctest --test-dir build --output-on-failure`: 8/8 tests passed.
- Existing Aria generated formal-tool artifacts remain untracked and ignored where generated.

## Semantic gaps intentionally left open

- Remaining integer families (`int8`/`int16`, unsigned ints, `tbb8`/`tbb16`/`tbb64`)
- stderr/stddbg output cells
- Struct arrays, nested field paths, generic structs, and legacy `struct Name { ... }` shorthand
- Rich `pick` patterns beyond value equality and `(*)` wildcard dispatch
- Typed `Rules<T>`, non-integer rule values, struct-field rules, arrays,
  modulo expressions, and SMT/proof integration for `limit<Rules>`
- richer memory semantics beyond the initial allocation qualifier slice:
  `defer { free(...) }`, pointer dereference/addressing, pinning (`#`), and
  `wildx`
- borrow permissions (`$i`, `$m`)
- modules/imports and extern/FFI
- concurrency primitives

## Next recommended slice

Expand semantic coverage in the next small slice. Recommended order: borrow
permissions (`$i`, `$m`), then richer memory behavior and broader symbolic
`kprove` lemmas.

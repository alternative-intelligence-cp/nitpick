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
- Added first borrow permission slice: `$$i`/`$$m` local aliases, borrow
  parameter metadata, `$x`/`!$x` borrow values, immutable/mutable conflict
  checks, immutable-borrow assignment blocking, and `$$m` bad-argument failsafe
  routing.
- Added first `defer { ... }` cleanup slice: per-block defer stacks, LIFO
  deferred block execution at scope exit, pending cleanup before terminal
  `exit`, and `pick`/`fall` interaction with scoped block cleanup.
- Added first pointer address/dereference slice: `@id` produces a local-store
  pointer value and `<-ptr` reads the current value behind that pointer, with
  failsafe routing for invalid non-pointer dereferences. Pointer store-through
  is intentionally not modeled yet.
- Added first pin-safety slice: `#id` registers a pinned host, records it in a
  dedicated `<pinned-hosts>` cell, returns a distinct pin pointer value, and
  routes double pins, pinned-host reassignment, and pinned-host mutable borrows
  to failsafe while allowing immutable aliases.
- Threaded pinned-host state through isolated helper call frames so callee-local
  pins do not leak into callers and caller pins are restored after return.
- Raised the `k_semantics_core` CTest timeout to 180 seconds so the expanded
  K corpus can complete reliably after a fresh `kompile`.
- Compiled `aria.k` and passed all 55 core K tests under `krun`.
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

- `./k-semantics/run_k_tests.sh --require-k`: 55 passed, 0 failed.
- `bash ./k-semantics/run_k_proofs.sh --require-k`: 1 proof module passed, 0 failed.
- Cross-checked the new `Rules` / `limit<Rules>` K tests with `build/ariac`;
  expected exits matched actual process exits for all four new programs.
- Cross-checked the new `stack`/`gc` and `wild`/`free` pass cases with
  `build/ariac`; expected exits matched actual process exits.
- Cross-checked the new borrow pass cases with `build/ariac`; expected exits
  matched actual process exits. Cross-checked borrow negative cases against
  `ariac` static rejections for double mutable borrow, plain value passed to
  `$$m`, and assignment to a borrowed host.
- Cross-checked focused `defer { free(...) }` probes with `build/ariac`: the
  defer-cleaned wild allocation compiled and ran with the requested exit, while
  the no-defer wild allocation was statically rejected with ARIA-014.
- Cross-checked focused pointer probes with `build/ariac`: `@value` plus
  `<-ptr` compiled and returned the pointed-to value, read-after-reassignment
  returned the updated value, and invalid `<-value` was statically rejected.
- Cross-checked focused pin probes with `build/ariac`: non-wild `int32->:pin =
  #value` compiled and ran, immutable aliasing of a pinned host compiled and
  returned the expected value, and reassignment, double pin, and mutable alias
  attempts were statically rejected with ARIA-016.
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
- richer memory semantics beyond the allocation/defer/pointer-read slices:
  pointer store-through, fuller runtime pin dereference/release behavior, and
  `wildx`
- richer borrow semantics beyond the initial permission qualifier slice:
  positive `$$m` call-by-reference mutation, scope-based borrow release,
  field/path-sensitive borrows, and pin-aware field/path edge cases
- modules/imports and extern/FFI
- concurrency primitives

## Next recommended slice

Expand semantic coverage in the next small slice. Recommended order: richer
pointer/borrow behavior, then broader symbolic `kprove` lemmas.

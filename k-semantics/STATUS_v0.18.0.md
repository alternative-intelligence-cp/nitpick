# Aria v0.18.0 K Semantics Status

Date: April 29, 2026
Branch: `dev-0.18.x`

## Completed in this slice

- Created `k-semantics/aria.k` as the first K Framework executable-semantics seed.
- Added core tests for exit, arithmetic, mutable/fixed binding, loops, Result
  unwrapping/fallback, sticky `ERR`, failsafe routing, and `if/else`.
- Added `run_k_tests.sh` with CTest-friendly skip behavior when K is absent.
- Added install notes capturing the current local toolchain state.
- Installed K Framework v7.1.320 through `kup` and fixed the runner for the
  current `kompile --output-definition` CLI.
- Compiled `aria.k` and passed all 10 core K tests under `krun`.
- Ignored generated K build output at `/k-semantics/.build/`.

## Local toolchain state

- `kompile`: K Framework v7.1.320 via `kup`
- `krun`: K Framework v7.1.320 via `kup`
- `kprove`: K Framework v7.1.320 via `kup`
- Java: OpenJDK 21 available
- Z3: 4.16.0 available
- Docker: installed, but current user lacks Docker socket permission

## Validation performed

- `./k-semantics/run_k_tests.sh --require-k`: 10 passed, 0 failed.
- `ctest --test-dir build -R '^k_semantics_core$' --output-on-failure -V`:
  `k_semantics_core` passed with K enabled.
- `ctest --test-dir build --output-on-failure`: 7/7 tests passed.
- Existing Aria generated formal-tool artifacts remain untracked and ignored where generated.

## Semantic gaps intentionally left open

- Full function declarations/calls
- Type-accurate int32/int64 bounds and tbb32 min-sentinel encoding
- Strings and stdout/stderr/stddbg output cells
- Structs, arrays, and field access
- `pick`/`fall`
- `limit<Rules>` SMT/proof integration
- memory contexts (`stack`, `gc`, `wild`)
- borrow permissions (`$i`, `$m`)
- modules/imports and extern/FFI
- concurrency primitives

## Next recommended slice

Expand semantic coverage in the next small slice. Recommended order: bounded
`int32`/`int64`, explicit `tbb32` range/min-sentinel behavior, then function
declarations/calls beyond the fixed `main`/`failsafe` envelope.

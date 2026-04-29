# Aria v0.18.0 K Semantics Status

Date: April 29, 2026
Branch: `dev-0.18.x`

## Completed in this slice

- Created `k-semantics/aria.k` as the first K Framework executable-semantics seed.
- Added core tests for exit, arithmetic, mutable/fixed binding, loops, Result
  unwrapping/fallback, sticky `ERR`, failsafe routing, and `if/else`.
- Added `run_k_tests.sh` with CTest-friendly skip behavior when K is absent.
- Added install notes capturing the current local toolchain state.
- Ignored generated K build output at `/k-semantics/.build/`.

## Local toolchain state

- `kompile`: not installed
- `krun`: not installed
- `kprove`: not installed
- Java: OpenJDK 21 available
- Z3: 4.16.0 available
- Docker: installed, but current user lacks Docker socket permission

## Validation performed

- Static file/content checks can run immediately.
- K compilation/execution is blocked until K tools are installed.
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

Install K, run `./k-semantics/run_k_tests.sh --require-k`, and fix any concrete
K syntax/backend issues before expanding semantic coverage.
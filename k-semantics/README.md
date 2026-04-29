# Aria K Semantics — v0.18.0 Seed

This directory contains the first executable-semantics nucleus for Aria's
v0.18.x validation series. The goal is to build a formal K Framework oracle that
can eventually answer: “what should this Aria program do?” independently of
`ariac`.

## Current scope

`aria.k` currently models a deliberately small core subset:

- mandatory `func:main` / `func:failsafe` program envelope
- `int32`, `int64`, `tbb32`, `flt32`, `flt64`, and `NIL` type tokens
- mutable and `fixed` variable bindings
- integer arithmetic and comparisons
- sticky `ERR` propagation for arithmetic
- `Unknown` propagation through arithmetic
- `pass`, `fail`, `raw`, `drop`, `defaults`, and `?!`
- `if` / `else`
- `loop(start, end, step) { ... }` with implicit `$` iterator
- terminal `exit` / `exit(...)`

This is not yet a complete Aria semantics. It is the first golden-oracle seed
for incremental validation.

## Run the K tests

Install K first (see `INSTALL.md`), then run:

```bash
./k-semantics/run_k_tests.sh --require-k
```

Without `--require-k`, the script exits `77` when K is missing so CTest can mark
the test as skipped instead of failing unrelated builds.

## Test corpus

Core seed tests live in `tests/core/`. Each test includes an expected terminal
state header:

```aria
// expect-exit: 10
```

The runner checks the final K configuration for the matching `<exit-code>` cell.

## Roadmap

Next increments should add, in order:

1. int32/int64 bounded overflow behavior and explicit tbb32 range semantics
2. function declarations/calls beyond the fixed `main`/`failsafe` envelope
3. string values and `println` output modeling
4. struct definitions and field access
5. `pick`/`fall` semantics
6. `limit<Rules>` and proof-oriented `kprove` lemmas
7. memory contexts: `stack`, `gc`, `wild`
8. borrow permissions: `$i` / `$m`
9. module/import and extern/FFI boundaries

Keep each step small enough to compare against real `ariac` output.
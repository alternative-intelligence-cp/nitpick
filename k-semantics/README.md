# Aria K Semantics — v0.18.0 Seed

This directory contains the first executable-semantics nucleus for Aria's
v0.18.x validation series. The goal is to build a formal K Framework oracle that
can eventually answer: “what should this Aria program do?” independently of
`ariac`.

## Current scope

`aria.k` currently models a deliberately small core subset:

- mandatory `func:main` / `func:failsafe` program envelope
- zero-argument helper functions returning through `pass` / `fail`
- `int32`, `int64`, `tbb32`, `flt32`, `flt64`, and `string` type tokens
- mutable and `fixed` variable bindings
- typed internal numeric values for declared `int32`, `int64`, and `tbb32`
- two's-complement bounded `int32`/`int64` arithmetic
- `tbb32` min-sentinel (`-2147483648`) and overflow/underflow to sticky `ERR`
- integer arithmetic and comparisons across raw and typed numeric values
- sticky `ERR` propagation for arithmetic
- `Unknown` propagation through arithmetic
- `pass`, `fail`, `raw`, `drop`, `defaults`, and `?!`
- helper calls such as `raw answer()` and `broken() defaults 43`
- string literals plus `print` / `println` writes to the `<stdout>` cell
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
Tests that model terminal output can also include an optional stdout assertion:

```aria
// expect-stdout: "hello\n"
```

## Roadmap

Next increments should add, in order:

1. parameterized function calls and isolated call frames
2. struct definitions and field access
3. `pick`/`fall` semantics
4. `limit<Rules>` and proof-oriented `kprove` lemmas
5. memory contexts: `stack`, `gc`, `wild`
6. borrow permissions: `$i` / `$m`
7. module/import and extern/FFI boundaries

Keep each step small enough to compare against real `ariac` output.

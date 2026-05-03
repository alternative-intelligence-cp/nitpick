# Aria K Semantics — v0.18.0 Seed

This directory contains the first executable-semantics nucleus for Aria's
v0.18.x validation series. The goal is to build a formal K Framework oracle that
can eventually answer: “what should this Aria program do?” independently of
`ariac`.

## Current scope

`aria.k` currently models a deliberately small core subset:

- mandatory `func:main` / `func:failsafe` program envelope
- zero-, one-, and two-argument helper functions returning through `pass` / `fail`
- isolated helper call frames for parameters, local bindings, and pinned-host
  preservation
- canonical `struct:Name = { Type:field; ... };` declarations for one-, two-,
  and three-field structs
- struct literals such as `Point{x: 10, y: 20}`, field reads such as `p.x`, and
  direct field writes such as `p.x = 42`
- top-level untyped `Rules:Name = { ... };` declarations with integer `$`
  conditions and cascaded `limit<OtherRules>` references
- `limit<RulesName> Type:x = expr;` declarations and reassignment checks for
  numeric values, including failsafe routing on violated constraints
- `stack`, `gc`, `wild`, and `wildx` declaration qualifiers with live allocation-class
  tracking cells
- `wild int8->:p = alloc(size);` / `wildx int8->:p = alloc(size);`
  declarations, `free(p);`, and failsafe routing for leaked or invalid
  wild-family frees
- `defer { ... }` cleanup blocks, registered per block scope and executed in
  LIFO order on scope exit or before terminal `exit`
- standalone nested `{ ... }` statement blocks with block-exit restoration of
  local borrow aliases, immutable/mutable borrowed-host tracking, and pinned
  host tracking
- local pointer operations: `@value` captures a local binding address,
  `<-ptr` reads the current value from that address, and `<-ptr = value`
  writes back through the captured location
- pointer-member operations: `ptr->field` reads through a local pointer to a
  struct binding, `ptr->field = value` stores back through that field path, and
  nested pointer-valued paths such as `ptr->leaf->x` read/store through the
  selected inner pointee
- pin registration via `#value`, with pinned-host tracking, pin dereference,
  pin store-through and pin-member store-through blocking, double-pin blocking,
  same-scope reassignment blocking, pinned-host field mutation blocking,
  pin-derived nested path mutation blocking, block-scoped pin release, and
  pinned-host by-value helper-call / terminal-exit blocking, plus mutable-borrow
  blocking while immutable aliases remain allowed
- `$$i` / `$$m` borrow qualifiers on local aliases and helper parameters, with
  direct one-level field path tracking for aliases such as `pair.a`,
  immutable-vs-mutable conflict checks, disjoint-field split borrows,
  exact borrowed-field assignment blocking, nested two-level struct-field path
  tracking for aliases such as `box.leaf.x`, nested sibling split borrows,
  parent/child borrow conflict checks, nested-field mutation, parent-field
  mutation blocking while a child field is borrowed, mutable field-alias
  writeback for local direct and two-level struct-field aliases, `$$m`
  argument-shape enforcement, and positive `$$m` call-by-reference writeback
  for ordinary variable borrows
- `int8`, pointer (`Type->`), `int32`, `int64`, `tbb32`, `flt32`, `flt64`, and
  `string` type tokens
- mutable and `fixed` variable bindings
- typed internal numeric values for declared `int32`, `int64`, and `tbb32`
- two's-complement bounded `int32`/`int64` arithmetic
- `tbb32` min-sentinel (`-2147483648`) and overflow/underflow to sticky `ERR`
- integer arithmetic and comparisons across raw and typed numeric values
- sticky `ERR` propagation for arithmetic
- `Unknown` propagation through arithmetic
- `pass`, `fail`, `raw`, `drop`, `defaults`, and `?!`
- helper calls such as `raw answer()`, `raw inc(8)`, and `broken(17) defaults 23`
- string literals plus `print` / `println` writes to the `<stdout>` cell
- `if` / `else`
- `pick(selector) { ... }` first-match dispatch over value patterns, `(*)`
  wildcards, optional labeled arms, and `fall label;` jumps to a labeled arm
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

## Run the K proofs

The first proof hook compiles `aria.k` with the Haskell backend required by
`kprove`, then proves every `*.k` claim module under `proofs/`:

```bash
bash ./k-semantics/run_k_proofs.sh --require-k
```

The initial proof corpus includes `proofs/core-proofs.k` with three concrete
claims for sticky `ERR`, bounded `int32` wrapping, and `tbb32`
overflow-to-`ERR` behavior, plus `proofs/field-alias-proofs.k` with concrete
claims for direct field-alias writeback, nested field-alias writeback, and
immutable field-alias assignment failsafe routing, plus `proofs/pin-proofs.k`
with concrete claims for pin registration, pin store-through rejection,
pin-member mutation rejection, pin-path mutation rejection, and pinned-host
reassignment failsafe routing, plus `proofs/pin-by-value-proofs.k` with
concrete claims for pinned-host rejection in one-argument calls, immutable
parameter calls, both two-argument positions, and both direct and parenthesized
terminal `exit` values, plus `proofs/pointer-proofs.k` with concrete claims
for local address-of, pointer dereference, pointer store-through, invalid
non-pointer dereference routing, pointer-member reads, and pointer-member
store-through, plus `proofs/pointer-path-proofs.k` with concrete claims for
nested pointer-valued field reads, nested pointer-valued store-through,
pin-derived nested path reads, and pin-derived nested path mutation rejection,
plus `proofs/borrow-path-proofs.k` with concrete claims for direct sibling
field assignment while a different field is borrowed, exact borrowed-field
assignment rejection, nested sibling-field assignment while a different nested
field is borrowed, and exact nested borrowed-field assignment rejection.
Like the test runner, the proof runner exits `77` without `--require-k` when K is
not installed so CTest can skip it cleanly.

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

1. richer memory and borrow behavior: remaining concrete deeper pin path edge cases
  and array/index field borrow paths once accepted by the compiler surface
2. richer `Rules<T>` coverage: floats, strings, arrays, struct fields, and SMT
3. broader proof-oriented `kprove` lemmas for helper calls, `Rules`, memory, and
  borrow permissions
4. module/import and extern/FFI boundaries

Keep each step small enough to compare against real `ariac` output.

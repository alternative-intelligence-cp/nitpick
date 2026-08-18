# Analysis rejection

Whole programs that **load, resolve and type-check** and are refused by a **static
analysis**. Driven by `tools/check.npk`, which is the real frontend.

## Why this is its own suite

Four rejection suites, four stages, and collapsing any two would make "correctly
refused" mean less — a file that stopped earlier would satisfy a test written about
a later stage.

| Suite | Refused by | Gets that far? |
|---|---|---|
| `tests/modules/rejection/` | the **loader** | no |
| `tests/types/rejection/` | the **type checker** | loads and resolves |
| `tests/analysis/rejection/` | a **static analysis** — this suite | and type-checks |
| `tests/rejection/` | the **backend**, at a rung it cannot lower (D-085) | and passes every analysis |

The split is load-bearing here rather than tidy: **the analyses run only over a
program the type checker accepted**, so a case written for `NITPICK-BORROW-001`
that failed to type would never reach the rule it was written for. That is not
hypothetical — 0.5.3 had to move four cases into `types/rejection/places.npk` for
exactly that reason, because a `NITPICK-TYPE-024` in the same file stopped the
program before the move rules ran.

The counterweight is `tests/accept/`, which is **one** suite for all four stages.
Silence has no stage.

## What every code is tested by

| Code | Rule | Case |
|---|---|---|
| `BORROW-001` | D-004 rule 2 — a borrow may not travel up | `borrows.npk`, five forms |
| `BORROW-002` | D-004 rule 3 — nor be stored in something longer-lived | `borrows.npk` |
| `BORROW-003` | D-004 rule 4 — nor cross an `extern` call | `borrows.npk`, `path_shapes.npk` |
| `BORROW-007` | the derivation walk ran out of fuel | `too_deep.npk` |
| `BORROW-009` | a borrow reaches a binding the analysis cannot follow | `borrows.npk` |
| `BORROW-010` | the marking fixpoint did not settle | `unsettled.npk` |
| `ASSIGN-001` | D-010 — read before written | `definite_assignment.npk`, ten forms |
| `ASSIGN-002` | `fixed` / `const` assigned twice | `definite_assignment.npk` |
| `ASSIGN-003` | the walk ran out of depth | `too_deep.npk` |
| `MOVE-001` | D-065 — use after move | `moves.npk`, `path_shapes.npk` |
| `MOVE-002` | use after free, and double free | `moves.npk`, `path_shapes.npk` |
| `MOVE-003` | `nodrop` with no drop to suppress | `moves.npk` |
| `PICK-001` | D-008 — a case left uncovered | `exhaustiveness.npk` |
| `PICK-002` | D-008 §5.1 — a `tbb` with no `ERR:` arm | `exhaustiveness.npk` |
| `PICK-003` | a selector that cannot be covered by listing | `exhaustiveness.npk` |
| `PICK-004` | an arm nothing can reach | `exhaustiveness.npk` |
| `PICK-005` | the walk ran out of depth | `too_deep.npk` |
| `TAINT-001` | D-007 — `.value` of an unchecked `Result` | `taint.npk`, `path_shapes.npk` |
| `LOCK-001` | D-056 — acquiring downward | `lock_levels.npk`, `path_shapes.npk` |
| `LOCK-002` | an implementation above its trait's bound | `lock_levels.npk` |
| `LOCK-003` | an `acquires` level that is not constant | `lock_levels.npk` |
| `LOCK-004` | the walk ran out of depth | `too_deep.npk` |

### The five with no test, and why they cannot have one

**Three are rules whose construct does not exist.** They are written, walked and
unreachable — deliberately, because a rule added after the analysis is verified is
a re-verification this project cannot afford:

- `BORROW-004` — a borrow crossing a **thread spawn**. There is no spawn;
  concurrency lowers in cycle 1.1.
- `BORROW-005` — a borrow held across an **`await`**. Same cycle.
- `BORROW-006` — a **closure** capturing a borrow. Closures are removed (D-018), so
  there is no capture list to inspect.

**Two fire only if the compiler is broken.** `ASSIGN-004` and `BORROW-008` report a
node kind the walk has no entry for — there is nothing a programmer can write to
produce one, and a test would have to break the compiler to reach it. Both were hit
during development and behaved correctly, which is the only evidence available.

## Path-shape coverage

An analysis can visit every node and still be wrong about a **path**.
`path_shapes.npk` is the cross-product, worked through by hand because no script
can tell whether a merge was handled *correctly* — only whether it was reached.

| Analysis | branch | zero-iteration loop | `pick` arm | early exit | `defer` |
|---|---|---|---|---|---|
| escape (D-004) | `path_shapes` | `borrows` | `path_shapes` | `borrows` | `path_shapes` |
| assignment (D-010) | `definite_assignment` | `definite_assignment` | `definite_assignment` | `definite_assignment` | `definite_assignment` |
| moves (D-065) | `moves` | `moves` | `path_shapes` | — *(see below)* | `path_shapes` |
| taint (D-007) | `taint` | `path_shapes` | `taint` | `taint` | — *(see below)* |
| locks (D-056) | `path_shapes` | `path_shapes` | `path_shapes` | `path_shapes` | — *(see below)* |
| exhaustiveness (D-008) | *(not applicable — the shape is the `pick`)* | | | | |

Three cells are empty and each is a judgement rather than an omission:

- **moves × early exit** — an exit removes an arm from the merge, and `moved`
  unions, so an exit can only ever *reduce* what is considered moved. There is no
  case where an early `pass` makes a use-after-move appear, which is what a test
  would have to show.
- **taint × `defer`** and **locks × `defer`** — a `defer` body is walked with the
  state at its registration, so both analyses see it exactly as they see any other
  block. `escape` and `moves` have `defer` cases because for them the deferred body
  runs at a point that *matters*; for these two it does not.

**The early-exit case in `path_shapes.npk` is a positive one**, and deliberately:
an early `pass` before a lower acquisition is *correct* code, and an analysis
treating an exit as ordinary flow would refuse it. It is in this file rather than
`tests/accept/` because it belongs beside the shapes it is contrasted with.

## Writing one

Expectations live in the file, assert on **codes and spans, never on message text**
(`BUILD_REFERENCE.md` §7.1), and a file with no `// expect-error:` is a fixture.

**Every case here must type-check.** A type error stops the program before any
analysis runs, so the test would fail for a reason unrelated to what it guards —
and if the case you want genuinely does not type, it belongs in
`tests/types/rejection/`.

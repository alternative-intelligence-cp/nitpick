# `tests/accept/` — programs the frontend must accept

A file here loads, resolves, type-checks and passes every static analysis, and
`tools/check.npk` says **nothing at all** about it. Any diagnostic is a failure,
whatever its code.

## Why this suite exists

The other three suites are all rejections:

| Suite | Refused by |
|---|---|
| `tests/modules/rejection/` | the **loader** — it never reaches a checker |
| `tests/types/rejection/` | the **type checker** |
| `tests/analysis/rejection/` | a **static analysis**, on a program that type-checks |
| `tests/rejection/` | the **backend**, at a rung it cannot lower yet (D-085) |

**There is one acceptance suite and four rejection suites, and that asymmetry is
deliberate.** A rejection names the stage that refused, and the stages have to stay
distinguishable or a file that stopped early would satisfy a test written about a
later one. Silence has no stage: a program the whole frontend accepts is accepted
by every part of it, so splitting this directory four ways would be four
directories asserting the same thing.

**None of them can tell a correct frontend from one that refuses everything.** An
escape analysis that answered "yes, that is a borrow" to every expression passes
all nine cases in `rejection/borrows.npk`. So does one that reports every program
as internally broken. A negative suite establishes that a rule *fires*; only a
positive one establishes that it fires *when it should*.

That distinction became load-bearing in cycle 0.5. A type rule that is too strict
gets found quickly, because somebody's correct program stops compiling. The
analyses in this cycle are **deliberately conservative in a way type rules are
not** — they fail closed on fuel exhaustion, on an expression kind with no entry
in the shape table, on anything they cannot decide — and every one of those
choices trades a false refusal for soundness. That trade is right, and this suite
is what keeps it bounded.

## What belongs here

Programs that exercise the **accepting** side of a rule that has a rejection file
next door: the shapes a reader would expect to be refused and which are not, and
the reason they are not. `accept/borrows.npk` returns the value behind a borrow,
passes borrows down, and rebinds one, because each of those is a step away from
something that *is* refused.

A case that merely compiles adds nothing. A case that a plausible wrong
implementation would reject is the whole point.

## What does not belong here

- **Anything that should be refused.** It goes in a rejection suite, with its
  code.
- **Runtime behaviour.** Nothing here is compiled or run; `tests/conformance/`
  and `tests/frontend/` are where a program's *answer* is asserted. This suite
  asserts silence.

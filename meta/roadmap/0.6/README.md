# Cycle 0.6 — Macros and comptime

**The last cycle of Phase A.** At the end of it the frontend is complete: a
checker that validates `.npk` sources completely and emits nothing.

## What makes this cycle different from the five before it

**Every earlier cycle added a pass that READS the AST. This one adds a pass that
REWRITES it** — and every pass already built runs downstream of the result.

That is a failure mode none of the previous cycles had. A defect in the lexer
produces a bad token and something complains. A defect in an analysis produces a
wrong diagnostic. **A defect in expansion produces a well-formed AST that means
something other than what was written**, and every pass after it then resolves,
type-checks and analyses that AST faithfully. The diagnostics will be about code
nobody wrote, pointing at spans that came from a macro body.

So the measure here is neither node coverage (0.2, 0.3) nor rule coverage (0.4)
nor path coverage (0.5), but a fourth: **what the expansion produced is what the
macro said.** That is only checkable by comparing expanded output against an
expectation, which is why this cycle needs a test shape the others did not have.

## The specification does not exist yet, and that is the first subcycle

D-057 is unusually blunt about this:

> The macro system is far larger than either document describes, and **its
> specification exists only in regression tests** — 49 `.npk` files under
> `nitpick/tests/`, carrying the semantics in comments keyed to decision codes.
> … It is not unspecified — it is **specified in the wrong artifact**, one that
> ships with no prose and cannot be read as a whole.

`meta/specs/` holds twenty reference documents and **`MACRO_REFERENCE.md` is not
one of them.** Building against 31 test files nobody has read as a whole is how
0.4 lost five defects to constructs that parsed and were never read downstream.

**So 0.6.0 recovers the specification and writes it**, and every later subcycle is
built against that document rather than against the corpus. It is also a
deliverable in its own right: `meta/specs/` becomes the contents of `nitpick-docs`
at the switch (`meta/SWITCH.md`), and a language whose macro system is documented
only by its own regression tests is not one anybody else can use.

## The three rules that shape everything else

### Expansion precedes evaluation, and runs to a fixed point first

One rule covering three findings D-057 recovered from the corpus: module-level
expansion iterates until no invocation remains; `comptime` folding happens after
expansion; and `comptime(double_it!(3))` expands before it evaluates, nesting
arbitrarily.

### Expansion is bounded, and that is new

`macro:m = () { m!(); };` iterates forever. Nothing in the corpus bounds it, so
**the prototype fails to terminate** — unacceptable in a compiler under formal
verification, where termination is itself a property to establish.

A depth and iteration limit, exceeding it an ordinary compile error naming the
macro and the chain that reached the bound. Every walk in this compiler that
follows program structure is already bounded; this is the same rule applied to a
loop that rewrites rather than reads.

### An identifier in a macro body resolves in the DEFINING scope. Always.

The prototype resolves it in the defining scope, notices when the call site
disagrees, emits `NITPICK-061`, and then **keeps the caller's binding anyway** —
a back-compat path that D-057 calls out as "precisely the failure the blueprint
philosophy exists to prevent: the macro means something different depending on
where it is invoked, with a warning as the only guard."

Flipped: defining scope always, failure to resolve there is a **compile error**
rather than a silent fallback, and **`#caller(NAME)` is the sole way to reach the
call site** — explicit, greppable, and already spelled with the compiler-directive
sigil. `NITPICK-061` disappears, because the hazard becomes structurally absent
rather than detected.

## What already parses and nothing reads

The same table 0.5 opened with, and it is again the work list:

| Parses today | Read by | Lands in |
|---|---|---|
| `macro:name = (params) { … };` | nothing | 0.6.1 |
| a macro **invocation** | nothing — there is no expansion pass at all | 0.6.1 |
| `comptime(expr)` | typed as its operand | 0.6.4 |
| `#[derive(Copy, Clone)]` | parsed as a generic attribute, never inspected | 0.6.5 |
| `#caller(NAME)` | **does not lex as a builtin** — `caller` is not in `builtins.npk` | 0.6.2 |

## Six debts fall due in 0.6.4

`comptime` folding is owed to six places that today refuse anything but an integer
literal and say so by name. Each carries the same sentence pointing here:

| Site | Refuses |
|---|---|
| `resolve_type.npk:667` | an array size `int32[N]` |
| `resolve_type.npk:471` | a `comptime` generic argument `Mutex<Config, 2>` |
| `analysis/locks.npk:176` | an `acquires N` level |
| `analysis/bindings.npk:414` | a `const` global's initialiser |
| `parse_decl.npk:917` | *(macro body shape, fixed at parse)* |

**And one tripwire.** `tests/frontend/expr_types.npk` asserts that **exactly one**
expression in its program is unrecorded — the `int32[4]` size, which nothing types
today. When 0.6.4 starts folding array sizes that count goes to zero and the
assertion fails, deliberately, until somebody confirms it was intended. It was
written in 0.5.0 for this moment.

## Subcycles

| | Topic |
|---|---|
| **0.6.0** | The macro semantics, recovered from the corpus and written as `MACRO_REFERENCE.md` |
| **0.6.1** | Expansion — the fixed-point loop, bounded, and module-level invocation |
| **0.6.2** | Hygiene — defining-scope resolution, and `#caller(NAME)` as the sole opt-out |
| **0.6.3** | Splicing — multiple declarations, struct fields, `impl` methods |
| **0.6.4** | `comptime` evaluation, and the six debts it pays |
| **0.6.5** | `#[derive]` |
| **0.6.6** | Diagnostics, the suites, and closing Phase A |

## What "done" looks like

`tools/check.npk` expands a program's macros to a fixed point, folds its `comptime`
expressions, generates its derived implementations, and then does everything cycles
0.1–0.5 already do — refusing a macro that recurses without bound, one whose body
names something absent from its defining scope, a `comptime` expression that cannot
be folded, and a `derive` of a trait the type cannot satisfy.

**And Phase A is over.** The artifact is a checker; the emitter is cycle 0.7.

## Two things to decide early rather than discover

### What bounds expansion, in numbers

D-057 settles that there **is** a depth and iteration limit and does not say what
they are. `--comptime-budget <N>` is named as the precedent for the shape, so the
mechanism is familiar — but a default that is too low refuses working programs and
one too high is a termination guarantee in name only.

**Recommendation: bound both, separately, and make the diagnostic name the chain.**
Depth bounds one invocation's nesting; iterations bound the fixed-point loop. They
fail differently — a deeply nested single expansion is not the same mistake as two
macros expanding into each other — and a single combined budget would report both
as the same thing.

### What `#[derive]` may derive, and what it generates

`TRAITS_REFERENCE.md` §131 lists eight: `Default`, `PartialOrd`, `ToString`, `Eq`,
`Hash`, `Clone`, `Debug`, `Ord`. **What each generates is not written down**, and
several are not obviously mechanical — `Ord` on a struct needs a field order,
`Hash` needs a combining function, `Debug` needs a format and D-053 moved
formatting to `&{ }` interpolation.

This is frontend-blocking in the same way D-113 was: derive generates
declarations, so the frontend must accept whatever shape they take. It should be
settled in 0.6.0 alongside the macro semantics, not discovered in 0.6.5.

## Scheduled into this cycle from outside it: D-127 — **settled in 0.6.7**

Recorded in 0.6.2 as *"something writes past the end of its own allocation"*, on the
evidence that adding a size header to the allocator broke three tests while shifting
every address by 16 with no write was harmless.

**It was not a memory bug at all.** `etyper_init` takes ten parameters and three
test helpers passed nine; the seed never checked call arity, so the call emitted one
operand too few and the callee read an unwritten register. What junk sits in that
register depends on the binary's layout — which is why every size change moved the
symptom and why the allocator looked guilty for two days.

`valgrind` named it in a single run, on the binary that was **passing**. The entry in
`DECISIONS.md` keeps the whole wrong theory, because the instructive part is not the
bug — it is that a semantic no-op crashing the program should have ended the
allocator hypothesis on the spot, and did not.

## Inserted during the cycle: 0.6.7

**Eight expression kinds are never typed** (D-129) — struct literals, array and
vector literals, pipe, dyn cast, expression-`pick`, and the two `comptime`-shaped
ones 0.6.4 owns. All eight fall through `type_of_expr_inner` to the INVALID type,
which is silent by design, so each is a construct the checker accepts without
looking at it.

Two were found by accident while doing something else, in consecutive subcycles.
That is what makes it a cycle-level item rather than a cleanup: `check_kinds_typed`
now diffs the kind list against the checker the way `check_kinds_reachable` has
diffed it against the parser since 0.2, and **0.6.7 empties the allow-list** before
0.6.6 closes Phase A. A checker that accepts `Point{ zzz: 1i32 }` does not validate
completely, which is the whole of what Phase A's artifact claims to be.

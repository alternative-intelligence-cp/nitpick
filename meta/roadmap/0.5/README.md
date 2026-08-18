# Cycle 0.5 — Static analyses

The analyses that make the safety claims true, written in Nitpick.

`SAFETY_ARCHITECTURE.md` states the position this cycle has to deliver:

> **Memory safety is a Layer 1 property, not a runtime one.** The mechanisms that
> deliver it are all **compile-time and structural**, with no runtime checks.

Cycle 0.4 taught the compiler what things **are**. This cycle is where it learns
**what happens to them** — which values escape, which bindings are assigned, which
have been moved out of, which arms a `pick` leaves uncovered, which levels a lock
acquisition may already be holding.

## What makes this cycle different from the last three

**0.2 and 0.3 were structural.** Visit every node, do the obvious thing; the hard
part was not forgetting a case, which is why `check_kinds_reachable` and
`resolve_audit` were worth building.

**0.4 was about rules.** A rule subtly wrong produces a program that compiles and
misbehaves, so negative coverage replaced mechanical completeness as the measure.

**0.5 is about paths**, and it has a third failure mode: an analysis that is
merely *incomplete* is silent, and what it silently fails to reject is a property
somebody was told they had. `--verify-memory` promising use-after-free freedom
while the checker skips a case is worse than promising nothing, because it invites
reliance. D-056 says exactly this about deadlock freedom, and the whole decision
exists because a flag claimed a property nothing delivered.

So the measure here is neither "every node visited" nor "every rule refused
something" but both, plus a third: **every path shape reached**. A branch, a loop,
a `pick` arm, a `defer`, an early `pass` — an analysis that is right on straight-line
code and wrong after a merge is the shape this cycle has to keep catching.

## What already parses and nothing reads

Cycle 0.4 found five defects that all dated to 0.2 — the cycle that *parsed* the
construct — because D-085 makes the parser accept the whole grammar and, by
itself, makes the checker's silence invisible. **The same list exists for this
cycle, and it is the work list**, so it is written down first rather than
discovered one subcycle at a time:

| Parses today | Read by | Lands in |
|---|---|---|
| `stack` / `wild` / `wildx` qualifiers | ~~nothing~~ **done in 0.5.1 and 0.5.3** | 0.5.3 |
| `fixed` / `const` qualifiers | ~~nothing~~ **done in 0.5.2** — the real frontend enforces both now, where only the seed did | 0.5.2 |
| `nodrop` qualifier | ~~nothing~~ **done in 0.5.3** — it requires `wild` or `wildx` | 0.5.3 |
| `$$i` / `$$m` borrow operators | ~~typed as plain pointers~~ **done in 0.5.1** | 0.5.1 |
| `move(place)` | ~~typed as its operand~~ **done in 0.5.3** | 0.5.3 |
| `pick` patterns | ~~typed, never checked for coverage~~ **done in 0.5.4** | 0.5.4 |
| `defer` bodies | ~~walked, never ordered against exits~~ **done in 0.5.2** — checked where registered | 0.5.2 |
| `Result.value` after `fail` | ~~untainted~~ **done in 0.5.5** | 0.5.5 |

Every row is a construct the frontend accepts and means nothing by. That is not a
backlog — it is the definition of this cycle.

## The three rules that shape everything else

### A borrow is a property of an expression, not a type

`$$i x` and `@x` are **pointers at the type level**, and the borrow checker is what
makes them second-class (D-004). Making borrow-ness a distinct *type* would force
every pointer operation to accept two kinds of pointer, which is the
context-dependence the blueprint philosophy forbids — and D-004's whole argument
for second-class borrows is that validity is **bounded by the callee frame,
structurally**, with no lifetime variables at all.

So the analysis answers "does this expression yield a borrow" from the expression's
shape, and the type system is left alone.

### Second-class means down, never up

A borrow may be passed **down** the call stack and may never travel **up**: not
returned, not stored into anything outliving the frame, not captured, not carried
across an `extern` call, a thread spawn, or an `await` (D-004's five rules).

Passing a borrow downward needs no annotation, because the callee's frame is
strictly inner. That is the entire lifetime system.

### An analysis says what it proves, and no more

`--verify-concurrency` verifies **data-race freedom and lock-order freedom**, and
deliberately does *not* claim deadlock freedom, because the deadline backstop is
containment rather than proof (D-056). That honesty is load-bearing: a narrow
guarantee that holds is worth more than a broad one that does not, and the flag
naming the wrong thing is how the gap arose in the first place.

## Subcycles

| | Topic |
|---|---|
| ~~**0.5.0**~~ | ~~The substrate~~ — **DONE**. An expression's type recorded once, plus the borrow classifier. D-113 settled. |
| ~~**0.5.1**~~ | ~~Second-class borrows and escape (D-004)~~ — **DONE**. All five rules, marked to a fixpoint, with the two caller-side rules that make "passing down needs no annotation" hold. D-114 through D-117 settled. |
| ~~**0.5.2**~~ | ~~Definite assignment, `fixed`, and `defer` ordering~~ — **DONE**. Two sets, merging in opposite directions; `$$m` fills where `$$i` reads. D-118 settled. |
| ~~**0.5.3**~~ | ~~`move`, moved-from bindings, and the manual-memory qualifiers (D-065)~~ — **DONE**. A free is a move; four sets merging in three directions. D-119 settled. |
| ~~**0.5.4**~~ | ~~Exhaustiveness — `pick` coverage, and the `tbb` ERR arm (D-008 §5.1)~~ — **DONE**. Coverage answered once and consumed twice; 0.5.2's debt paid. D-120 settled. |
| ~~**0.5.5**~~ | ~~`unknown` taint on `Result.value` (D-007)~~ — **DONE**. The branch decides, not the mention; `ok()` was already gone. D-121 settled. |
| **0.5.6** | Lock levels — the call-graph acquisition analysis (D-056) |
| **0.5.7** | Diagnostics, the suites, and closing the cycle |

## What "done" looks like

`tools/check.npk` refuses a program that returns a borrow, reads an unassigned
binding, uses a moved-from one, leaves a `pick` arm uncovered, lets a tainted value
steer a branch, or acquires a lock downward — each with its own code, its own span,
and a test that shows it refusing.

## Two things to decide early rather than discover

Both block a subcycle if left, and neither is settled in the spec set. **Both are
now settled** — D-113 and the expression-type table — and 0.5.1 raised two more of
exactly this kind, recorded in `0.5.1.md`:

- ~~**A parameter cannot be declared second-class.**~~ **SETTLED as D-117 — it does
  not need to be.** A callee could return a borrow it was passed, and the escape
  rules were defeated by one function call. The fix is caller-side and needs no
  syntax: a pointer parameter never points into the callee's own frame, so the
  escape is always one level up. Making the parameter second-class was the first
  recommendation and would have **stopped the compiler compiling itself** — every
  context struct here is built from pointer parameters and handed back.
- **A C variadic tail has no type.** `..*` in an `extern` names nothing, and the
  obvious filler `any[]` is the bare `any` the type system refuses on purpose.
  Refused explicitly (`NITPICK-TYPE-023`) rather than guessed at, and it must be
  settled before Phase B — `nlibc` is `extern` declarations all the way down.

Both are **language-surface** decisions and both are frontend-blocking under the
capability ladder, which is what puts them here rather than in a subcycle.

### How a dynamically dispatched method declares its acquisition level

**SETTLED as D-113** — `acquires <= N`, a third contract kind beside `requires`
and `ensures`. The reasoning below is what settled it.

**D-056 requires a spelling that does not exist.** The lock-level analysis is
whole-program, and its one hole is dynamic dispatch — a call through a trait object
can reach anything, so its acquisition set is unbounded. The decision says:

> a dynamically dispatched call **declares its maximum acquisition level** as part
> of the trait method's contract, and implementations are checked against it. An
> undeclared method may not acquire at all.

`VERIFICATION_REFERENCE.md` §248 restates the requirement. **No document gives the
syntax.** This is frontend-blocking under the capability ladder — the frontend is
built once, so a declaration form it must accept cannot arrive after it is
finished.

**Recommendation: reuse the contract slot the language already has.** A function
already carries `requires` / `ensures` clauses in a contract window
(`VerifyContractNode`), and a maximum acquisition level is a *precondition on the
caller* in exactly the sense `requires` is. Spelling it as a third contract kind —
`acquires <= N` — adds a `VerifyKind` and no new syntax shape, keeps the
declaration where every other obligation on a signature already lives, and makes an
undeclared method's "may not acquire at all" the natural reading of an absent
clause rather than a special case.

### Whether an expression's type is recorded or recomputed

**SETTLED in 0.5.0** — recorded, in `src/frontend/expr_types.npk`. It earned its
keep immediately: 0.5.1's `<-` rule needs an expression's type to tell a borrow of
a pointer from the plain value behind a borrow, and removing it refuses
`pass (<-p);` in four places.

Several analyses need types: exhaustiveness needs the selector's, taint needs to
know a `Result`, lock levels need `Mutex<T, LEVEL>`'s argument. Cycle 0.4 computes
an expression's type and **keeps nothing** — `type_of_expr` returns it and the
result is discarded.

**Recommendation: record it, once, in a side table indexed by `ExprId`.** Two
computations of one fact is the exact shape that has cost this compiler defects
repeatedly — `is_error` stored beside `error` (D-069), `expr_shape` read two ways,
a type name read as a token kind (D-104). An analysis that recomputes types agrees
with the checker only as long as both are updated together; an analysis that reads
what the checker recorded agrees with it **by construction**.

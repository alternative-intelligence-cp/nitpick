# Open decisions and unwritten specs

The work queue, opened at D-061 and **fully closed at D-079** — all ten
decisions settled and all three specs written. Retained as the record of what was
open and how each item closed. Every item here blocks something concrete; the
"blocks" column says what. Nothing in this file is optional and nothing in it is
deferrable — per the standing constraint, anything going into the language has to
be in before the Astrée trial starts, because re-verification is unaffordable.

Ordering rationale is in §4. Items move to `meta/roadmap/done/` as they close,
with the deciding D-number recorded.

---

## 1. Decisions

### Safety-critical

| # | Item | Blocks | Source |
|---|---|---|---|
| ~~1~~ | ~~**Task cancellation**~~ — **settled by D-062.** Task lifetime is lexical; a spawned task cannot outlive its spawning scope, so frame lifetimes nest and D-034's arena is doing the job arenas are for. Scope exit joins under a mandatory deadline; expiry traps. Preemptive `Executor::cancel` is not ported. | — | `CONCURRENCY_REFERENCE.md` §6 |
| ~~2~~ | ~~**`async` + `failsafe`**~~ — **settled by D-063.** A trap is a whole-program event: no coroutine resumes on any thread, no `defer` runs, frames freeze. Other threads stop before the handler runs. `failsafe` may not be `async`. | — | `CONCURRENCY_REFERENCE.md` §6 |

Both closed. The finding that shaped them: the prototype implements cancellation
**twice** — preemptively via `coro.destroy()` (no `defer`, admitted dangling
handle) and cooperatively via a token polled at `await` points (defers run). Two
disciplines for one job; the unsafe one is removed and the safe one survives as
the join mechanism rather than as surface syntax.

### Frontend-blocking

The bootstrap strategy builds the frontend **once, in full**. Anything that
changes the parser, the AST, or the builtin-type table has to be settled before
that work starts, or the strategy fails in exactly the way it was designed to
avoid.

| # | Item | Blocks | Source |
|---|---|---|---|
| ~~3~~ | ~~**Generics**~~ — **settled by D-064.** Bodies checked at their definition against bounds alone; turbofish is the only expression-position form, which confines `>>`-splitting to a delimited context; `comptime` value parameters added for D-056's lock levels; monomorphization depth-capped at 64, deduplicated, reversibly mangled with no hash. No specialization, no variance, no duck-typed bodies. | — | `PRE_PLANNING_REVIEW.md` §4; `SPEC_GAPS` §3 |
| ~~4~~ | ~~**`move` memory qualifier**~~ — **settled by D-065: it is not a qualifier.** `move(place)` is a keyword operator with a parenthesized operand, the same shape as `comptime(expr)`. Moved to `ControlFlow`. Explicit only, no implicit moves; moved-from bindings are invalid until reinitialized. | — | `LEXICAL_REFERENCE.md` open items |
| ~~5~~ | ~~**`opaque` declaration form**~~ — **settled by D-066.** `opaque struct:Name;` wins on evidence: the standalone form has zero prototype usage. `extern`-only, and no value semantics (`OPAQUE-COPY-001`). | — | conflict 49 |
| ~~6~~ | ~~**LBIM sticky ERR**~~ — **was already settled by D-037**, which resolves Part R by name and strikes the §2.2.1 sentinel. Listing it here was my error: Part R's heading still said "Open question" and I trusted the marker over the decision log. D-037 also corrects the premise — plain integers **wrap** rather than trap, which is what `ncrypto`'s `uint4096_shl` already relies on. | — | Part R |

### ABI and lowering

| # | Item | Blocks | Source |
|---|---|---|---|
| ~~7~~ | ~~**Fat vs thin pointers**~~ — **was already settled by D-038: thin.** Part W's heading still said "Open question" and conflict 52 still said "a real open question, not stale text"; both predate D-038. Second stale marker of the day. **But the question was aimed at the wrong type** — if pointers carry no bounds, something must, and `T[]` was entirely unspecified. **D-070** settles that: `T[]` is a slice, `{ptr, i64 len}`, a non-owning second-class view, bounds-checked against the runtime length. | — | Part W |
| ~~8~~ | ~~**`tbb32` ERR encoding**~~ — **settled by D-069.** Nothing was undocumented: `INT32_MIN` *is* `tbb32`'s published sentinel. What was missing was its meaning in that field — an error whose identity was lost — now **unconstructible**, trapping where it would be built. The larger find: `is_error` stored the same fact as `error != 0` with no invariant relating them, and both `{error: 0, is_error: true}` and `{error: 5, is_error: false}` were constructible. Stored field removed; `r.is_error` survives as a derived accessor. | — | D-005 follow-up |

### Policy

| # | Item | Blocks | Source |
|---|---|---|---|
| ~~9~~ | ~~**LLVM / Z3 dependency boundary**~~ — **settled by D-067: invoked, never linked.** There is no exception to the zero-dependency rule, because neither is a dependency in the sense the rule means. D-055's argument applied to the toolchain. D-067 also records what it does *not* claim — LLVM's IR-to-machine-code translation stays outside the verified boundary. | — | Part X |
| ~~10~~ | ~~**Runtime `limit<Rules>` violation**~~ — **settled by D-068.** It traps to `failsafe`. More importantly, constraints are enforced in **every** build; `--verify` decides only whether a check is discharged statically and therefore elided. A safety property must not depend on a compiler flag. | — | N5 |

---

## 2. Specs to write

| Spec | Why it is needed | Depends on |
|---|---|---|
| ~~**Channels, actors, thread pools**~~ — **written.** `CONCURRENCY_REFERENCE.md` §§6–9, backed by D-071/D-072/D-073 and the defect catalogue in `meta/CONCURRENCY_STDLIB_AUDIT.md`. The premise was wrong: the implementations could not serve as the specification, because three of the four do not work. | — | decisions 1, 2, 3, 4 |
| ~~**Streams / IO**~~ — **written:** `meta/specs/IO_REFERENCE.md`, backed by D-074/D-075/D-076. `Reader`/`Writer` traits so diagnostics are capturable; every operation `async` per D-071; end-of-input is an error code, not a sentinel; buffering fixed and never inferred from `isatty`; stream lifetime lexical. | — | decision 3 |
| ~~**Build system**~~ — **written:** `meta/specs/BUILD_REFERENCE.md`, backed by D-077/D-078/D-079. One manifest schema (two were in use for one filename), no network during a build, byte-reproducible output, three-stage bootstrap with a stage-1/stage-2 fixpoint check. | — | — |

---

## 3. Carried, not blocking

- **Build the concurrency stdlib in dependency order — build, not port.**
  `mutex`, `rwlock`, and `condvar` are genuinely C-free and go first, though
  D-056 changes the `mutex` API to `Mutex<T, LEVEL>` and removes the untimed
  `CondVar.wait`. `channel`, `actor`, `thread_pool`, and `thread` are **written
  against `CONCURRENCY_REFERENCE.md` §§6–9 rather than ported** — see
  `meta/CONCURRENCY_STDLIB_AUDIT.md`. `barrier` is reimplemented natively;
  `lockfree` and `atomic.npk` are not carried across at all (D-073).
- **Confirm Astrée's accepted input format with AbsInt**, well before the trial
  clock starts. The trial is a single non-renewable 30 days.

---

## 4. Ordering

1. **Decisions 1 and 2 together.** They are one question asked twice — what owns
   a task frame, and who tears it down — and answering either alone risks an
   answer the other contradicts. Safety-critical, and they close
   `CONCURRENCY_REFERENCE.md` §6 apart from the port.
2. **Decision 3, generics.** The largest item and the one the whole frontend
   waits on. Everything spelled `<T>` in the stdlib inherits its answer.
3. **Decisions 4, 5, 10, 9.** Contained decisions, batched — each is small on its
   own and they touch unrelated parts of the language.
4. **Decisions 6 and 8.** Both are the same shape of question — where a sticky
   ERR lives inside a representation — so they are decided together.
5. **Decision 7, pointers.** ABI, and the most expensive to revisit.
6. **The three specs**, concurrency last since it consumes 1, 2, 3, and 4.

This is more than one day's work. That is expected and is not a reason to
compress it.

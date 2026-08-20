# Cycle 1.4 — Astrée preparation

**Phase C, the one that cannot be retried.** A single non-renewable 30-day Astrée
run. Everything about this cycle is shaped by that fact: the work is preparation so
thorough that the 30 days are spent analyzing, not discovering that the input format
was wrong.

> Detailed **map**, and deliberately short — most of 1.4's content is *gates that
> must close earlier*, not subcycles that run here. The audit's central finding is
> that the most important gate has no owner and points at an unexamined assumption.

## The gate that must close before 1.3 exits (C-19)

The docs assume Astrée analyzes "monomorphized output" (0.4.7 notes,
TRAITS_REFERENCE §351). But **the compiler emits LLVM IR, and Astrée accepts C** —
not LLVM IR, not binaries. If AbsInt confirms C-only:

- an entire **C-emission path** becomes unplanned Phase-C work, and discovering it at
  the start of the 30 days is the failure this whole cycle exists to prevent;
- that path must be scheduled *now* (a 1.3-or-earlier subcycle), not at 1.4.

So C-19 is promoted from the carried "confirm with AbsInt" note to a **numbered gate
answered before 1.3 exits**, with the full question list (below) written down. This
is the single most important item in the post-0.8 plan's tail, because it is the one
whose late discovery is unrecoverable.

## The AbsInt question list (settle during 1.3, at the latest)

1. **Accepted input format** — C? which C standard/subset? Any LLVM-IR or binary
   path at all? (Decides whether a C-emitter is Phase-C work.)
2. **Analysis entry points** — how the entry set is specified for a program whose
   `main` is `async` over an executor.
3. **The concurrency-model mapping** — how the D-071 executor/coroutine model is
   presented to a tool that reasons about threads (Astrée has concurrency support, but
   the mapping from pinned tasks + futex parking to its model is non-trivial).
4. **Runtime-floor stubbing policy** — what Astrée is told about `sys`, the allocator,
   and the hand-written IR routines it cannot see the source of.
5. **The evidence package** — whether the SMT elimination manifest and z3 unsat cores
   (D-040 anticipates this) are part of what Astrée or the certification consumes.

## Subcycle shape (to be filled when reached)

| # | Topic | Gated on |
|---|---|---|
| 1.4.0 | **The confirmed input path** — whatever C-19 settled (a C-emitter if C-only, or the direct path if not); proven on a small program end-to-end into Astrée before the clock starts | C-19 |
| 1.4.1 | **Entry points, stubs, concurrency mapping** — the analysis harness Astrée needs, per the question list | C-19 |
| 1.4.2 | **The dry run** — a full pass on a representative program *before* the 30-day clock, so the real run finds analysis results, not setup errors | 1.4.0, 1.4.1 |

## Watch for

- **The clock starts once, and does not stop.** Every setup question answered on the
  30-day clock is a day not spent on the analysis that clock is for. 1.4.2's dry run
  is the whole risk-reduction of the cycle — treat a failed dry run as a schedule
  input, not a surprise.
- **The switch (`meta/SWITCH.md`) waits on 1.4** and inherits three audit
  corrections: the stale ship-list (`MACRO_REFERENCE.md`, added at 0.6, is in no
  list), the unowned prototype-coverage pass, and `meta/specs/`'s completeness
  (it inherits every Theme-F doc-staleness gap before it can replace `nitpick-docs`).
  These are folded into `OPEN_DECISIONS` and should be swept during 1.3/1.4 while both
  doc sets are still live — not after the archive.
- **Nothing about the switch happens until 1.4 is finished.** It is written down
  because a plan that lives only in conversation evaporates (SWITCH.md's own reason),
  not because it is imminent.

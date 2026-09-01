# HANDOFF — for the executor picking up cycle 1.4

> Written 2026-08-28 by the planning session (Fable), for a fresh
> session (Opus) to execute from. CLAUDE.md carries the standing
> context and is current; this file is the runway: where you are, what
> to do next, the rules that bite, and when to stop and escalate.
> Retire this file at the 1.4 cycle close.

## Where you are

**1.4.7 steps 1 and 2 are both underway and both green at HEAD.** Everything
below is committed; the tree is clean and nothing is in flight.

- **STEP 1 IS COMPLETE** (`e2a835c`). `dyn Writer` diagnostics: FIVE copies of
  the diagnostic walk (two `report` functions, three inline loops) became ONE
  `diag_report(dyn Writer, …)`, all six driver one-liners moved onto the same
  sink, and all four drivers' `main` are coroutines. The async surface lives in
  `src/frontend/diag_writer.npk`, NOT in diagnostics.npk — see the REACH note
  below, it is the interesting decision.
- **It needed two things first**: **D-224** (`c2db47c`) — `exit` means process
  exit in every body, async included — and a **snapshot refresh** (`e0871ab`).
- **D-225 landed mid-step** (`a2f2032` + snapshot `e310185`): declared-
  uninitialised managed storage holds its canonical vacant value. A real
  memory-safety defect, latent since 1.1.12.
- **STEP 2: all TWELVE single-array families are converted**, and **EIGHT of
  the ten parallel-array families** — `FoldEnv`, `tt_unit`, `vtmap`, `reach`,
  the resolver's error table, `irw_site`, `Suspend`, `Ast`. Each committed with
  its own full harness run.
- **REMAINING IN STEP 2: `TypeTable`'s five and `FnEmitter`'s seven.** The two
  wide ones; `fnem_pick_push` grows eight arrays in lockstep and
  `ir_func.npk` has 36 `ralloc` sites. Then step 3 (form upgrades), which has
  not been started.

## The decisions settled this cycle, and where they are

- **D-224** — `exit` is process exit in every body. The root task's frame is a
  STACK ALLOCA (not a heap block), and the async arm of `emit_exit` calls
  `@npk_exit` directly. Its own record notes that "B subsumes A" was WRONG:
  the scope-exit unwind is frame-resident, so both halves were needed.
- **D-225** — every `type_drops`-true kind has a stated canonical vacant value,
  its drop body is a no-op on it, and a declaration without an initialiser
  writes it. **`OwnedFd`'s vacant is −1, not zero** — a zeroed descriptor slot
  is fd 0, and dropping it would close stdin silently. Enums are fixed only
  along the TAG-0 projection. Instrumented in the walkers-total table.
- **THE INDEX RULE** (user-ratified, in 1.4.7.md, no D-number): when a table's
  count becomes `int64`, **the index type FOLLOWS THE COUNT** unless an
  external contract pins `int32`, and then it is a guarded narrow reusing an
  error the module ALREADY declares. A newly declared error is the last resort
  — it is the one option whose cost lands on every `failsafe` in the tree
  (family 6 paid 38 arms). `Ast` is the contract case; `Suspend` was not.

## Four things this stretch cost that you should not re-learn

- **The snapshot refresh procedure in `bootstrap/seed/README.md` WAS WRONG and
  is now fixed.** It compared stage1.new with stage2 and installed stage1.new.
  For any change that alters what the compiler EMITS those differ BY
  CONSTRUCTION, and stage1.new's BODY predates the change. The criterion is
  **stage2 == stage3, and stage 2 is what gets installed.** D-202's lesson in
  a second place.
- **A compiler FIX does not reach the tools until the snapshot carries it.**
  `build_tool` compiles `tools/` with the SNAPSHOT, not the npkc just built
  from `src/`. Measured: npkc-built checker rc=0, snapshot-built rc=3, same
  sources. D-205's rule in its mirror direction.
- **REACH is IMPORT-scoped.** An async function anywhere in a module makes
  `DeadlineExceeded` reachable in every program that imports it — twelve of the
  compiler's own unit tests failed REACH-002 because diagnostics.npk gained
  async functions. Twelve arms would have acknowledged a failure that cannot
  occur in those programs; the module split is why `diag_writer.npk` exists.
  An exhaustive `failsafe` earns its keep by being TRUE.
- **`async` may not be `never fails`** (TYPE-037): a suspended task can be
  cancelled. That also takes `drop` off the path, since D-163 licenses it by a
  checked `never fails` callee — use `?|`.

## Converting the last two families

`meta/roadmap/1.4/convert_family.py` handles SINGLE-array families only; every
parallel-array family so far was converted BY HAND, which is safe only because
the compiler enumerates what a sweep misses. The discipline that works:

1. **Sweep `src/`, `tools/` AND `tests/`** — not just the defining file. Two of
   my eight hand conversions were caught by the compiler for a site in another
   file (`r.env.count` on FoldEnv; `emit_program.npk` on irw_site).
2. **Apply, `quickemit`, self-compile (`npkc src/main.npk`), THEN the harness.**
   The self-compile is the cheap check for the family-10 class — a small test
   program has too few instantiations to overflow anything.
3. **Read the id BEFORE the push.** `list_push` performs the increment the old
   code did explicitly, so an id computed after the push is off by one — it
   compiles perfectly and corrupts every reference. This nearly shipped in all
   six of `Ast`'s add functions.
4. **A bulk append takes `list_reserve` before its loop**, not a push per
   element with no reserve (`Sink`'s shape; `Suspend`'s `lsflat`).
5. **N counts must be reset together.** `Suspend`'s per-function reset clears
   seven; they are listed rather than folded, on purpose.

## Running harnesses in parallel — THE WIDTH IS 2

The harness costs ~1.3 cores but ~9.5 GB, so MEMORY bounds concurrency on this
48-core / 157 GB machine. Convert family N in the main tree; in a worktree
apply N then N+1 as a CUMULATIVE PREFIX and run both at once, so each run
validates exactly the tree state that will be committed — verify with `diff`
before staging, every time.

**Going to 3-wide produced the migration's only red**, and the width is back at
2 until it is characterised: `extern_c_driver.npk` gave exit 29 once in 40 runs
under -O2 while three harnesses shared the machine. **Owed**: is that test's
5-second deadline simply too tight to be load-independent, or is there a race
that load makes reachable? ORCHESTRATION §4 asks for a calibration run at the
intended width first; I skipped it and this is what it would have caught.

**When a red appears, the IR is often the cheap decisive test.** That red's
signature (correct at -O0, wrong at -O2) is 1.3.8's defect exactly, and the
conversion under suspicion was the SUSPEND WALK. Emitting the failing program
from both trees and `cmp`-ing settled it in two minutes: byte-identical, so the
walk's crossing decisions never changed. A harness run would have taken fifty.

## OWED — open items, none of them lost

Work, in no particular order. Each is real, each was found by measurement, and
none is blocking step 2's remaining two families.

1. **`extern_c_driver.npk` under load** (above) — deadline too tight, or a race
   load makes reachable? The parallel width stays at 2 until this is answered.
2. **The `escape.npk` `ident_holds` belt** — line ~605 indexes `holds` where
   its siblings guard. In-range TODAY by construction (line 604 gates on
   `SYM_STMT`), but the belt is owed and **must fail CLOSED, which here is
   `pass true`**, not the siblings' `pass false`: `place_vouchable` inverts the
   answer, so a false-on-OOB guard would make an unseeable symbol vouchable.
3. **`check_instantiations` runs BEFORE the escape walk can still record** —
   `pipeline.npk:345` decides, and the escape walk's deep field walk can record
   a first-seen instance after that. Reachable shape: a bound-violating generic
   field type in a struct no checked expression touches. Move the decide pass
   after the last pass that can record, or sweep the tail and assert the table
   did not grow.
4. **`npkg test`'s capture-and-compare is BLOCKED by design, not by effort** —
   a `dyn` coercion MOVES, so a capture buffer handed to `diag_report` is
   consumed and cannot be read back. Needs a borrowing Writer, or a capture
   sink the walk does not own. `diaglist_render(Sink->)` remains a second walk
   until it is answered. USER-FACING DESIGN QUESTION.
5. **Should diagnostics print span-sorted?** `diaglist_render` sorts; the
   driver path never did, and I kept driver behaviour identical rather than
   change output ordering inside a receiver migration. D-204's reproducibility
   argues yes. USER DECISION.
6. **ORCHESTRATION.md's four §8 questions** are proposed as **D-226** and
   unanswered: does it become a numbered decision, who plays orchestrator, what
   width to calibrate at, and is "never work around a compiler defect"
   absolute.
7. **CLAUDE.md's quickcheck example cites `tests/types/rejection/borrows.npk`,
   which does not exist** — the file is `tests/analysis/rejection/borrows.npk`
   (a borrow rule is refused by an analysis, not the type checker).
8. **`chan_elem_ok`'s three refusals report as `NITPICK-RUNG-001`** but are
   PERMANENT language rules, in a suite whose README says its contents graduate
   as rungs land. These never will. Fix is D-215's shape: a TYPE code with a
   span, cases moving to `tests/types/rejection/`. (Carried from 1.4.4.)

## RETIRED: 1.4.6 — the builder switch (D-203, D-205)

`meta/roadmap/1.4/1.4.6.md` is the plan. This is the cycle's hinge: the
committed `bootstrap/seed/stage1.ll` becomes the builder, `npkrt.ll`
re-homes to `runtime/`, and D-205's normative rule changes meaning —
`src/` stops being bounded by subset 1 and becomes bounded by what the
SNAPSHOT can compile.

**1.4.5 left two hooks it will need.** The `repro` stage already asserts
that `bootstrap/seed/stage1.ll` matches a fresh emission, guarded on the
file existing — creating that file arms the assertion, and the STAMP
sha256 half is still owed. And the harness's toolchain flags now come from
`nitpick.toml`, so a switch that changes how anything is assembled changes
the manifest, not fifteen call sites.

**Read 1.4.4's execution record too.** It carries three things you would
otherwise rediscover: the D-151 leak check runs only on `exit 0` (a test
reporting success as 42 measures NOTHING — that is how a shared-arena leak
test passed against a build with the drop disabled); the return seam is
two halves, `fnem_ret_agg` then `fnem_ret_done`, with the unwind between
them; and every scope exit emits join → defers → drops → that scope's
channel reclaims.

**Owed, and named so it does not evaporate**: all three of
`chan_elem_ok`'s refusals in `ir_types.npk` — a `dyn` element, a borrow
element, an `OwnedFd` element — are PERMANENT language rules reporting as
`NITPICK-RUNG-001`, out of a suite whose README says its contents graduate
to `tests/conformance/` as each rung lands. These never will. The fix is
D-215's shape: a TYPE code with a span, refused in the checker, cases
moving to `tests/types/rejection/`. Schedule it.

## What the 1.4 subcycles should change about how you read a plan

**Every plan file whose diagnosis could be tested was wrong about the
diagnosis, while right about the goal.** Test the reported symptom before
implementing the reported fix:

- D-208 said the move analysis was straight-line and asked for
  loop-carried states. The loop rule had held since 0.5.3; PARAMETERS were
  invisible to the analysis entirely, and the audit's own evidence
  (`modmap_members`) was a `move` PARAMETER, not a loop.
- D-216 said TYPE-046 already refused a pick arm binding an owning
  payload. It did not — that was a live use-after-free, confirmed by an
  executed probe reading `0xAA` poison.
- D-210 predicted no deliberate-wrap sites because "the hash mixers ride
  tbb/wide". They rode plain `uint64`; FNV's multiply IS the wraparound.
- §26 of TYPE_REFERENCE promised `fixed` on struct fields was enforced. It
  was enforced nowhere.

- D-207 asked for a second list head per scope. The list is a LIFO stack,
  so a saved MARK does the same job with one pointer and leaves
  `emit_spawn` untouched — the goal was right, the mechanism was not.
- D-223's own landing note predicted four tests would pass unmodified. Two
  did; one was a REAL DANGLING POINTER and one hit a different rule. And
  the three sites the note expected the analysis to keep refusing were
  refused by a fixed-size BUFFER OVERFLOWING, not by the analysis — a
  ratified decision's own predictions are worth testing too.
- D-209's step 1 said "the drivers construct `std_err()`'s writer once".
  A `dyn` is move-only and a trait receiver is `Self->`, so the sink passes
  by value and CANNOT be constructed once — the three report sites each
  build their own. Right about the adoption, wrong about the mechanism.
- D-204 said `npkseed.py` embedded its argv path in the ModuleID. It
  emitted `"?"`: `_path` is the Node base's LOCATION attribute and a
  module node never gets one, while the file path sits in a `path` FIELD
  one character away. Reproducible by accident — and the accident was
  worth converting into a decision, which is the shape of most of these.

The pattern: **a rule believed in force because a document says so.**
Making a dormant rule apply is how six live defects surfaced across these
subcycles, including two memory-safety holes and — at 1.4.4 — a five-second
sleep on EVERY thread join since 1.1.9, hidden because the answer it
eventually returned was correct. Measure the symptom. A number that lands
exactly on a configured timeout is never a coincidence.

## The rules that bite (each has already cost a debugging cycle)

- **`--only` iterates, never concludes.** Nothing commits without a
  FULL harness run. A subcycle ends with the full run green and one
  commit in the house style (read `git log` for the voice; end commit
  messages with the Co-Authored-By/Claude-Session trailer only if your
  harness instructs it — otherwise match the repo's existing style).
- **The reserved-words table in CLAUDE.md is real** — `any` as a local
  name cost even the planning session an edit-build-fail cycle. Check
  it before naming anything.
- **quickcheck watches nothing** — rebuild after every `src/` edit;
  quickemit rebuilds itself. Neither substitutes for the harness.
- **Strings are move-only owners**: no binding-to-binding copies; pass
  as plain args freely; rebuild names per use in emitter code rather
  than holding one binding across lines. Pick arms cannot bind owning
  payloads until 1.4.3b lands — that refusal is correct, not a bug.
- **The walkers-total instrument refuses half-done type-kind changes**
  (excuse tables in `bootstrap/harness/harness.py`). When your change
  makes it fire, the fix is to complete the change or update the
  excuse WITH A TRUE REASON — never to silence it.
- **Never rewrite `done/` archives or settled DECISIONS text** —
  annotate with dated notes (the D-085/D-202 pattern).
- **The snapshot builds `src/` now** — a construct it cannot compile fails
  before any test runs. Refresh it (`bootstrap/seed/README.md`) at the
  PREVIOUS commit when a step genuinely needs a new feature.
- **`src/`'s own code is checked like everyone else's since the switch** —
  overflow traps, the escape analysis, move-only owners. A trap inside the
  compiler is a `src/` bug, not a test bug; `gdb -ex "break npk_trap" -ex
  run -ex bt` names it in one shot and beat two rounds of my reasoning.

## What you must NOT do

- **Do not re-open or re-litigate settled decisions** — implement them.
  If a plan step is WRONG on contact with the code (not merely harder
  than expected — wrong), stop and escalate rather than improvising a
  different design.
- **Do not invent grammar, operators, spellings, or semantic rules.**
  Language design is the user's domain. If a step seems to need a new
  spelling, that is an escalation, full stop.
- **Do not defer**: no "revisit later", no TODO-shaped exits. The
  standing rule (see the pinned memories) is that everything lands
  before the Astrée trial or is decided out by name.
- Do not optimise for a small diff; optimise for landing once,
  correctly. Time is not the constraint; correctness is.

## Escalation (the user holds a Fable reserve for exactly this)

The user keeps a slice of Fable quota for debugging help (~10% as of
2026-08-31, resetting Wednesday; a typical escalation costs 1-2%, so
budget three or four). An escalation this cycle -- the family-10
STOP-THE-LINE -- caught a landmine an executor had missed (`OwnedFd`'s
vacant is -1, and a plain memset would have closed stdin silently), so it
is worth spending when the question is decision-shaped.
**A NEW FABLE SESSION MUST WORK IN ITS OWN WORKTREE**: on 2026-08-30 two
sessions shared this tree and one commit swept up the other's in-flight
edits: **`fc609a6`'s message and its diff disagree**, and the commit carries a
`git note` saying so and pointing at 1.4.7.md's RESOLVED section (`git log`
shows notes by default; `refs/notes/commits` needs its own push).

Escalate when (a) a plan step is wrong on contact, (b) a miscompile/fixpoint-drift/nondeterminism hunt has
survived two serious hypotheses, or (c) anything decision-shaped
appears. BEFORE escalating, write what you found into the subcycle's
file (symptoms, ruled-out causes, the exact failing command) so the
escalation session starts warm instead of re-deriving your steps.
Remember the 0.6 lesson recorded in ROADMAP: a symptom that moves when
unrelated things change size is a value never written; reach for the
debugger (valgrind is fine — zero-dependency governs the artifact, not
the workbench) before building instrumentation.

## Where things live

- Plans: `meta/roadmap/1.4/*.md` (this cycle), `1.5/README.md` (the
  ratified verification architecture), `1.6/README.md` (Astrée prep).
- Decisions: `meta/specs/DECISIONS.md` (through D-225); the live queue
  `meta/roadmap/OPEN_DECISIONS.md` (only C-19 remains externally
  gated).
- Research: `meta/roadmap/research/` — read the `digests/`, not the
  2MB primaries; the audit is `research/COVERAGE_AUDIT.md`.
- The auto-memory directory is shared with the planning sessions —
  the pinned memories there are standing user preferences; honor them.

## First action

Read `meta/roadmap/1.4/1.4.7.md` end to end — it is long now and the
execution record is most of it, in chronological order: step 1's first
attempt and revert, step 2's twelve single-array families, the
STOP-THE-LINE and its RESOLVED section, the D-224 settlement, step 1's
landing, and the parallel-array families with the index rule. Re-verify every anchor before editing
(lines drift); an anchor says what to look for, not a blind offset.
Announce the item you are on; commit per the file's acceptance section.

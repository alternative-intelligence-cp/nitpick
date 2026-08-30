# HANDOFF — for the executor picking up cycle 1.4

> Written 2026-08-28 by the planning session (Fable), for a fresh
> session (Opus) to execute from. CLAUDE.md carries the standing
> context and is current; this file is the runway: where you are, what
> to do next, the rules that bite, and when to stop and escalate.
> Retire this file at the 1.4 cycle close.

## Where you are

- **1.4.6 IS COMPLETE AND PUSHED** (`499bebf..2ee4a2f`). The committed snapshot
  builds `src/`; the Python seed builds nothing.
- **1.4.7 step 2 is UNDERWAY: nine of twelve single-array families are
  committed and green** (`RootList`, `Sink`, `StrTable`, `SiteTable`,
  `InternTable`, `SourceManager`, `ModuleGraph`, `BoundTable`, `ImplTable`),
  each with its own full harness run and a byte-identical fixpoint.
- **THE FAMILY-10 STOP-THE-LINE IS RESOLVED — it was never a use-after-free,
  and family 10 is LANDED and harness-green (`ok 60`, fixpoint byte-identical).**
  The defect was `convert_family.py` itself: its step-4 guard deletion ran
  unscoped and unboundaried, so family 10's `inst_grow\(t\)` matched inside
  `fninst_record`'s `drop fninst_grow(t);` and stripped a live guard from a
  table it was not converting; the un-grown table's push then overflowed its
  768-byte block into the neighbouring slot's header. Read 1.4.7.md,
  "RESOLVED" — the gdb evidence, the four measurements re-explained, the tool
  fix with its regression test, and two new owed items (the `ident_holds`
  belt direction, and `check_instantiations` running before the escape walk
  can record). The corrected conversion is in `src/frontend/types.npk` at
  HEAD (fc609a6 swept it in while its message still described the old
  theory — trust RESOLVED, not that message or this file's earlier text).
  `1.4.7-family10-trigger.patch` stays as the committed reproduction;
  `1.4.7-escalation.md` is answered.
- **1.4.7 step 1 (`dyn Writer` diagnostics) is BLOCKED ON A USER DECISION**,
  unrelated to the above: what `exit` means inside an async body. Three options
  with a recommendation are in 1.4.7.md. Do not improvise a fourth.

## Two things not to rediscover

- **`meta/roadmap/1.4/convert_family.py`** does the mechanical conversion, with
  the SEVEN ways a site hides from a text search closed by construction — the
  seventh (the tool editing a site it does not own) is the one that caused the
  STOP-THE-LINE, and the fixed tool now scopes and boundaries its guard
  deletion and refuses a deleted guard without its converted push. Rehearse
  with `dry=True` first; it refuses to write an incomplete conversion. The
  remaining families' parameters are in 1.4.7.md.
- **Text search has been wrong about "who touches this representation" six
  times in this step; the compiler has been wrong zero times.** Apply,
  `quickemit` (NOT `quickcheck` — it builds only the frontend), then the full
  harness. Skipping the `quickemit` step is what let one broken family reach a
  harness run.

## Running harnesses in parallel (proven, and it earned its keep)

The harness is fully serial and costs ~1.3 cores but ~9.5 GB, so on this
48-core / 157 GB machine MEMORY bounds concurrency to roughly a dozen runs, not
CPU. Convert family N in the main tree; in a worktree apply N then N+1 as a
CUMULATIVE PREFIX and run both harnesses at once. Each run then validates
exactly the tree state that will be committed, so no rigor is given up — verify
the prefix with `diff` before trusting it. This is how the use-after-free was
separated from a green family 9 in one wall-clock window. Watch for spurious
DEADLINE failures (`channel_deadline`, `driver_deadline`, `executor_sleep`, the
five-second thread joins); none appeared at 2 concurrent, and one would be a
stop sign rather than noise.

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

The user keeps ~12% Fable quota until Wednesday's reset for
debugging help. Spend it well: escalate when (a) a plan step is wrong
on contact, (b) a miscompile/fixpoint-drift/nondeterminism hunt has
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
- Decisions: `meta/specs/DECISIONS.md` (through D-221); the live queue
  `meta/roadmap/OPEN_DECISIONS.md` (only C-19 remains externally
  gated).
- Research: `meta/roadmap/research/` — read the `digests/`, not the
  2MB primaries; the audit is `research/COVERAGE_AUDIT.md`.
- The auto-memory directory is shared with the planning sessions —
  the pinned memories there are standing user preferences; honor them.

## First action

Read `meta/roadmap/1.4/1.4.7.md` end to end — INCLUDING its execution
record, which is where step 1 stopped and why — then 1.4.6's, which is long,
and the long part is what the switch exposed. Re-verify every anchor before editing
(lines drift); an anchor says what to look for, not a blind offset.
Announce the item you are on; commit per the file's acceptance section.

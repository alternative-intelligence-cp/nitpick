# HANDOFF — for the executor picking up cycle 1.4

> Written 2026-08-28 by the planning session (Fable), for a fresh
> session (Opus) to execute from. CLAUDE.md carries the standing
> context and is current; this file is the runway: where you are, what
> to do next, the rules that bite, and when to stop and escalate.
> Retire this file at the 1.4 cycle close.

## Where you are

> **2026-09-02: 1.4.8 is UNDERWAY in the main tree** (the fresh session the
> 1.4.7b close asked for). Its execution record — order, decisions taken on
> contact, the user's answers, and every step's landing — is the tail of
> `meta/roadmap/1.4/1.4.8.md`; read that before this file's older stretch.
>
> **LANDED so far:** Part A (`clone_exec` / `environ` / `lib/nproc.npk`),
> step 2 (`TY_FLAGS`), step 2b (`range<T>` spellable), step 3 (mid-cycle
> snapshot refresh), step 4 (every `open` caller crosses typed flags to the
> floor), step 5 (`lib/nfs.npk` — the file-system surface — plus `sys_cwd`),
> step 6 (D-236 manifest-root paths; the `selfhost` stage now measures H9
> green). Steps 3–6 were validated under D-228's cumulative-prefix protocol
> and landed after a UI freeze interrupted the executing session — the
> recovery lost nothing, main is byte-identical to the fully-merged `w456`,
> and a confirmatory harness on committed main followed.
>
> **Part D LANDED (2026-09-02): `npkg/` exists** — `npkg build` (byte-identical
> to the harness's compiler), `npkg test` (908 verdicts on its first full run,
> every suite count the harness's, the §7.1 self-check inside it), the
> closed-world link with an in-house ELF reader, and the harness's new
> `parity` stage diffing the two runners' verdicts on every full run. The
> execution record (decisions on contact, the three finds — S-9, the
> `resolve_check` prelude defect, the quadratic capture) is the tail of
> `meta/roadmap/1.4/1.4.8.md`; the questions for the user are **S-9** and
> **S-10** in `OPEN_DECISIONS.md` §2e.
>
> **1.4.8 IS CLOSED (2026-09-02)**: the concluding harness run on the final
> tree was green in every stage, 58/58, with the `parity` stage's first
> result — 902 verdicts agree between the two runners, npkc byte-identical.
> S-9 and S-10 were ratified the same day as **D-237** and **D-238**.
> **NEXT: 1.4.8b** (`meta/roadmap/1.4/1.4.8b.md`, execution-grade: the eight
> files resolved as pre-settled, exact matching in both runners with the
> self-check's new case, then the manifest-declared suites with the parity
> stage proving the move verdict-identical), then 1.4.9, the close.

**1.4.7 IS COMPLETE (closed 2026-09-01)**: steps 1–3, D-229, OWED-8 and
OWED-1 landed; the fixpoint is declared under D-202 and the snapshot is
refreshed FROM THE ADOPTED TREE (stage2 == stage3, 15,450,688 bytes, STAMP
source-commit `43379b0`); SUBSET_1 §4 has its closing edit. The last two
commits before the close: the FNV step took the one copy's `uint128` spelling
(1.4.6's owed item — and `bridge_stubs.npk`'s second copy of the trio, which
1.4.2b's collapse never saw, is gone), and four `src/` comments the adoption
had made false came current. **1.4.7b (the pre-1.4.8 batch) IS COMPLETE**
(`meta/roadmap/1.4/1.4.7b.md`): D-234/D-235/D-236 recorded and S-8 raised; D-235
in the compiler (the `hasshared` bit and every-kind-decided channel table, the
last S-6 rung gone); D-231 (the sub-byte widths struck, the wide ladder pinned
by `wide_ladder.npk`); the tfp fold respelled over one `uint512`; the provenance
headers re-spelled; and D-228's width calibration run clean at 6. **Next: 1.4.8
(`npkg`, D-206) in a fresh session, and it OPENS with two re-homed items —
D-236's implementation (manifest-root paths, which needs the filesystem surface
D-206/D-213 build) and D-230's `TY_FLAGS` (whose consumer is `lib/nfs.npk`, and
which waits on the user's families answer).** See 1.4.7b.md's "RE-HOMED" section
and its two user questions. Everything below is committed; the tree is clean and
nothing is in flight.

- **STEP 1 IS COMPLETE** (`e2a835c`). Five copies of the diagnostic walk became
  one `diag_report`. It needed **D-224** (`exit` is process exit in every body)
  and a snapshot refresh first, and **D-225** landed mid-step (declared-
  uninitialised managed storage holds its canonical vacant value — a real
  memory-safety defect, latent since 1.1.12).
- **STEP 2 IS COMPLETE.** Every growable array in `src/` is a `List<T>`;
  **`ralloc` appears nowhere in `src/` outside `list.npk`**. Twenty-two
  families — the twelve single-array, the ten parallel-array, and FOUR the
  original enumeration had missed, three of them named in its grower list but
  given no landing row and one (`MacroTable`) never counted at all, because the
  enumeration keyed on `ralloc` and that family `alloc`s-and-copies.
- **STEP 3 IS COMPLETE.** 268 of 600 counter loops became
  `for (intN:i in 0iN...b)` — three dots: `..` is INCLUSIVE, `...` exclusive
  — and the 332 that stay do so by rule, not by omission. **A `for` captures
  its bound at entry and a `while` re-reads it**, so a loop bounded by a
  container's live count stays a `while` (175 of them); the spelling says
  which. That rule is PROPOSED in 1.4.7.md's step-3 record and wants
  ratification — D-226's shape, an engineering rule for `src/`. Five
  match-shaped unwraps became `?|`/`?!`; the other twenty are not operator
  shapes and the record says why, one by one.
- **D-229 IS COMPLETE.** Stage 1 (`5d56959`): the walk generic, borrowing and
  span-sorting. Stage 2: `diaglist_render` retired into it through
  `impl:Sink:Writer`, which lives in `diag_writer.npk` beside the walk — NOT
  beside its type, because REACH is import-scoped and the impl's methods are
  async; the impl's own header says so. The walk is tested through the capture
  it exists for (`tests/backend/programs/diag_capture.npk`, an async `main`
  lending it a `Sink`, overflow arm included); the frontend test stays
  synchronous. Both negative-controlled.
- **An impl MAY live in any module** — the impl table is program-wide and each
  entry resolves its names in its own scope (TRAITS_REFERENCE §4.1, D-171) —
  and the first one to do so found that the SYMBOL SCHEME had never been asked
  which module qualifies a method: definitions and three call paths disagreed
  the moment an impl left its trait's module. **Fixed at `fb22bb6`**: the
  impl's module, on both ends (`ExprEmitter.cur_impl` / `impl_decl_for`),
  byte-identical IR for every pre-existing program; `impl_foreign.npk` pins
  every shape. Read 1.4.7.md's record before touching method symbols.

## The decisions settled this cycle, and where they are

All in `meta/specs/DECISIONS.md`. **D-224…D-233.**

- **D-224** `exit` is process exit in every body; the async arm calls
  `@npk_exit` directly. Its own record notes "B subsumes A" was WRONG.
- **D-225** every `type_drops`-true kind has a stated canonical vacant value.
  **`OwnedFd`'s vacant is −1, not zero** — a zeroed descriptor slot is fd 0,
  and dropping it would close stdin silently.
- **D-226** the index type FOLLOWS THE COUNT unless an external contract pins
  `int32`, and then it is a guarded narrow reusing an error the module already
  declares. Ordering: **no narrow > reuse the module's own error > declare a
  new one**; the third costs every `failsafe` in the tree an arm.
- **D-227** a memoised layout fact is never read before it is computed. The
  QUERY ensures; the caller does not remember. `_recorded` is the explicit,
  greppable opt-out, and the unqualified name is the safe one.
- **D-228** ORCHESTRATION's R1–R9 and its cumulative-prefix protocol are
  normative. Fable orchestrates, Opus executes; width calibration is sequenced
  BEHIND OWED-1; R6 is absolute.
- **D-229** the diagnostic walk is generic and borrowing, and prints
  span-sorted. Stage 1 committed; stage 2 owed (above).
- **D-230** D-044's flag types get implemented as one `TY_FLAGS` kind, before
  1.4.8, because that subcycle's `lib/nfs.npk` grows the flag-taking surface.
- **D-231** the sub-byte integer widths are STRUCK; the wide ladder is pinned
  with layout rows and one EXECUTED conformance case.
- **D-232 → superseded by D-233.**
- **D-233** the verification evidence moves to the emitted IR: LLVM-native
  analyzers supersede Astrée and the C emitter is struck. Three legs — abstract
  interpretation over our own IR (engine chosen at 1.6.0's measured gate),
  the D-218 Z3 spine untouched, and Alive2 translation validation. **1.6 is no
  longer a one-shot**; what is scarce now is proof invalidation, not trial
  attempts.

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

## Converting a collection family — RETIRED, kept for the method

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

Ratified 2026-09-01 as **D-228…D-232** after a Fable analysis pass; what
remains below is execution, not deliberation.

1. ~~**`extern_c_driver.npk` under load**~~ — **CLOSED 2026-09-01**, the
   method followed in order and every step measured: a RACE in the C FIXTURE.
   Kernel 1 stored its hostile tail AFTER its completion, so under load the
   driver could poison the ring after the next dispatch had validated an
   honest tail, then die on its own poison — and the Bridge reported
   `EDriverFault`, correctly, which exit 29 collapsed. 11 of 120 under 48 CPU
   hogs at -O2, every failure within five milliseconds (no wait ever ran out);
   identical at -O0 with the window widened. Fixed at the source (store before
   complete; the test names r4's wrong error). **D-228's width calibration is
   unblocked**; the width stays at 2 until that run is made.
2. ~~The `escape.npk` `ident_holds` belt~~ — **CLOSED 2026-09-01** (`ea2faea`).
   Fails closed as `pass true`, the opposite of its siblings, because both
   callers invert the answer.
3. ~~`check_instantiations` runs before the escape walk can still record~~ —
   **CLOSED 2026-09-01** (`6ca1ea2`). The diagnosis was wrong (measured: the
   table never grows after the decide pass, and the named shape is already
   caught); the goal was right, so the tail is now swept by construction.
4. ~~`npkg test`'s capture-and-compare is blocked~~ — **CLOSED 2026-09-01**
   (D-229 stage 2): the walk is generic and borrowing, `diaglist_render` is
   gone, and `diag_capture.npk` reads the walk's bytes back through a `Sink`.
5. ~~Should diagnostics print span-sorted?~~ — **CLOSED 2026-09-01** (D-229
   stage 1, `5d56959`): yes, in the one walk, and the sort's own use-after-free
   went with it.
6. ~~ORCHESTRATION.md's four §8 questions~~ — **RATIFIED as D-228.**
7. ~~CLAUDE.md's quickcheck example~~ — CLOSED 2026-09-01.
8. ~~`chan_elem_ok`'s three refusals report as `NITPICK-RUNG-001`~~ —
   **CLOSED 2026-09-01** as ratified: `NITPICK-TYPE-057`, one table in
   types.npk (all 47 kinds named, walkers-total registered), D-227's ensuring
   entry in type_layout.npk, one helper in resolve_type.npk raising from the
   spelling and from `type_subst`'s channel arm, the belt kept and the
   undecided kinds still a rung. One thing the plan did not see: the spelling
   site reaches layout only through a `use` cycle (type_layout imports
   resolve_type), declared with its reason at the `use` — legal by D-086, the
   third such cycle. 1.4.7.md's record has the alternative and why not.

### New at the 1.4.7 close (2026-09-01)

- **S-7 (OPEN_DECISIONS §2e, the user's)** — the emission records each source
  path AS GIVEN, so an absolute `src/main.npk` embeds the machine's path in
  the site table (1,489 of 1,647 rows in a dry-run refresh) and nothing but
  the README's relative spelling kept the committed snapshot clean. The
  `repro` stage now refuses an absolute site path in `stage1.ll`; the
  recommendation is manifest-root-relative paths in the source manager.
- **The loop-bound rule** still wants ratification (step 3's record), and
  **S-6** still wants its answer; **D-228's width calibration** is unblocked
  and is the orchestrator's run.
- **Two candidates recorded, not acted on**: `numeric.npk`'s limb arithmetic
  respelled over `uint256` (a simplification outside D-209's scope), and the
  55 `// Subset 1 … Cycle 0.x.y.` provenance headers (SUBSET_1 §4 says what
  they mean; a one-line sweep if the user prefers they stop saying it).

### New, from the same ratification batch

- **D-230 — implement D-044's flag types** as one `TY_FLAGS` kind before
  1.4.8, because that subcycle's `lib/nfs.npk` grows the flag-taking surface.
- **D-231 — strike the sub-byte integer widths, pin the wide ladder** with
  layout rows and one executed conformance case.
- ~~**D-232 — C-only is the working default for Astrée**~~ — **SUPERSEDED the
  same day by D-233**: the evidence moves to the emitted IR, Astrée exits, and
  the C emitter is struck. The reason is worth carrying: Astrée ingests C, so
  it would have analysed an AST→C sibling lowering that never ships, while the
  binary comes from the LLVM path — evidence about a model rather than about
  the artifact.

### Checked and NOT a defect (2026-09-01)

A borrow-carrying concrete coerced to `dyn` is not refused at the coercion, and
that is fine: the ESCAPE ANALYSIS catches the hazard through another door.
Measured with four probes — the coercion compiles, a `dyn` outliving a BLOCK is
safe because D-191 extends an address-taken local to the function's end, and
the shape that actually dangles (a `dyn` carrying a borrow to its own frame,
returned upward) is refused by `NITPICK-BORROW-001` at the coercion itself.
D-215 needed its own rule because a channel endpoint is a handle the escape
analysis has no reason to follow; a borrow is precisely what it does follow.
Recorded because the first three probes each supported the opposite conclusion.

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
  before the EVIDENCE CAMPAIGN closes or is decided out by name — D-233
  restated the basis (proof invalidation, not a one-shot trial) and the rule
  survived unchanged. Tools are the exception the decision names: adopting a
  tool later adds evidence and invalidates none, so SEQUENCING a tool is a
  decision rather than a deferral.
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
  ratified verification architecture), `1.6/README.md` (the analyzer-evidence
  cycle, rewritten at D-233).
- Decisions: `meta/specs/DECISIONS.md` (through D-233); the live queue
  `meta/roadmap/OPEN_DECISIONS.md` — **nothing is externally gated any more**:
  C-19 was the last, and D-233 closed it by removing the external dependency
  rather than by answering it.
- Research: `meta/roadmap/research/` — read the `digests/`, not the
  2MB primaries; the audit is `research/COVERAGE_AUDIT.md`.
- The auto-memory directory is shared with the planning sessions —
  the pinned memories there are standing user preferences; honor them.

## First action

**Read `meta/roadmap/1.4/1.4.7.md` end to end.** It is long and the execution
record is most of it, in chronological order — step 1's attempt and revert, the
twelve single-array families, the family-10 STOP-THE-LINE and its RESOLVED
section, D-224, step 1's landing, the parallel-array families and the index
rule, then the four families the enumeration had missed and the D-227
neighbourhood. Re-verify every anchor before editing (lines drift); an anchor
says what to look for, not a blind offset.

1.4.7 is closed. Next are **D-230** (`TY_FLAGS`, one kind — read D-230 and
D-044; B-7's walkers-total instrument enumerates the sweep up front, the way
1.3.1 ran it for `TY_SIMD`) and **D-231** (strike the sub-byte widths from
LEXICAL_REFERENCE §6.2's marked region and regenerate; pin the wide ladder
with layout rows and one executed conformance case), then **1.4.8**
(`meta/roadmap/1.4/1.4.8.md`). A new type kind means a snapshot refresh
BEFORE `src/` may spell it (D-205). Announce the item you are on; one full
harness run per commit, no exceptions.

## What this cycle proved about how to work here, in one place

Every one of these cost something to learn and each is now load-bearing:

1. **Test the reported symptom before implementing the reported fix.** Every
   plan file whose diagnosis could be tested was wrong about the diagnosis
   while right about the goal. 1.4.7 added three more: OWED-3's premise
   (measured false — the table never grows after the decide pass), OWED-8's
   "three refusals" (one has no test at all), and D-232's whole route.
2. **A count that comes out right can still be a wrong edit.** The family-10
   converter deleted a guard it did not own while its own count read a correct
   9; only a separate push-preservation assertion caught it. It happened TWICE
   in this cycle's tooling.
3. **Silence is not success.** `rg -oh` parses as `-o -h` and `-h` is HELP in
   ripgrep, so a sweep printed the help banner and its filtered form printed
   nothing — which reads exactly like "no matches". Ask any "none" that a
   decision rests on a second way.
4. **Check an instrument against failure before trusting it.** This repo has
   shipped a dead assertion (`check_runtime_sigs_agree`'s derived-inner leg).
   Both instruments added this cycle were negative-controlled, and the control
   caught a real bug in one of them.
5. **The compiler is the completeness check, not grep.** Text search has been
   wrong about "who touches this representation" in eight structurally
   different ways; the compiler has been wrong zero times. But it cannot catch
   an UNSTARTED conversion, and it cannot see an asymmetry that type-checks —
   the counts-move-together break on FnEmitter's pop side was invisible to it.
6. **"Correct by accident" is the shape to hunt.** Four defects this cycle
   returned the right answer from an uninitialised, uncomputed, or freed read.
   Poisoning the value — making the wrong answer wrong — is what exposed every
   one of them, and is now the `absent-fact` harness stage.

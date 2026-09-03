# Cycle 1.4 — Self-hosting

**Phase C, the milestone that matters.** Everything before 1.4 is validated
against the seed's output; after it, the compiler validates itself. This is
where the builder switches from the regenerated Python seed to committed stage
IR, the fixpoint is re-closed after 0.9–1.3's additions, `src/` finally adopts
the language it implements, builds become byte-reproducible beyond one process
on one machine, and `npkg` — the permanent build/test/verify runner — is born
and proven against the throwaway Python harness.

> **CLOSED 2026-09-02 (1.4.9).** Every row below landed; self-hosting is
> declared under D-202 (`1.4.9.md` has the numbers), the snapshot is refreshed
> from the final tree, and "What cycle 1.4 taught" at the end of this file is
> the cycle's account of itself. The Python harness did NOT retire — D-206:
> both runners run through 1.5, the harness's result is the one that means
> the suite is green, and retirement is `meta/SWITCH.md`'s.

> This file was internally titled "Cycle 1.3" until 1.4.0 — the 1.1-close
> renumbering swept the folder name but not the file body. Corrected at cycle
> open, along with `SUBSET_1.md:266`'s "(1.3)" and the harness's "1.2
> fixpoint" comment.

## The state this cycle starts from (the 1.4.0 survey)

Measured at cycle open, superseding the 0.8-close audit's version of this
section where they differ:

- **The fixpoint stage already measures the right thing.** The audit's C-10
  finding was about the SPEC texts (BUILD_REFERENCE:188, D-085's carried
  D-079 sentence): "stage 1 and stage 2 must be byte-identical" —
  unsatisfiable as written, since the seed and the real backend are two
  independent emitters. The harness's operative check
  (`harness.py:2246-2334`) compares the seed-built compiler's **emission of
  the compiler** against stage 1's re-emission, byte for byte — two `.ll`
  texts, both from `src/backend`, exactly the criterion C-10 proposes. The
  code is right; the spec is what 1.4.0 corrects (D-202).
- **`src/` is still fully subset-1.** Zero generic declarations, zero
  `trait:`/`impl`, zero `async func:` across the 71 modules the seed
  compiles. SUBSET_1 §4's adoption rows (0.9 floats/tbb, 1.0 generics, 1.1
  async, 1.2 RAII spelling) were never executed for `src/` — the seed is the
  sole builder and lowers only subset 1, so the C-13 constraint silently
  overruled the adoption table every cycle. `src/prelude/prelude.npk` is the
  standing escape valve: it uses traits, generics and async freely because
  it is **data** (scraped into `prelude_source.npk` by the generator,
  compiled by npkc at runtime, never by the seed). The whole adoption debt
  lands in this cycle, after the builder switch.
- **`bootstrap/seed/` is empty** (one `.gitkeep`). Four documents assert the
  committed seed IR exists; `.gitignore` even carries the carve-out comment
  for `bootstrap/seed/*.ll`; `LAYOUT.md:68` and `npkseed.py:9` name the
  artifact `seed/stage1.ll`. Nothing has ever written it. The artifact that
  WOULD be committed exists as `.internal/quickemit/npkc.ll` — 11.2 MB,
  264,685 lines, and already path-independent on the harness path
  (`module_id="test"`, relative source paths, zero absolute paths in the
  text). Only the standalone `npkseed.py` embeds its argv path as the
  ModuleID (C-12's fix).
- **`bootstrap/runtime/npkrt.ll` is mis-homed and mis-labeled.** Its header
  says THROWAWAY; it is 6,187 lines, 157 defines, and linked into every
  artifact the project produces — including the one that ships. LAYOUT's
  "all of it is deleted once self-hosting closes" would delete the runtime
  floor and the only rebuild-from-LLVM path (C-11/D-203 fixes the survival
  map; D-015's "replaced at a later rung" gets its disposition in the same
  decision).
- **The toolchain invocation is already uniform**: `llc -O0 -filetype=obj
  -relocation-model=static` and `ld.lld -static` at every site, plus the
  1.3.8 opt-O2 leg (`opt -O2 -S`, `llc -O2`). Nothing pins the version
  (20.1.2) as a build input anywhere a build reads (C-12).
- **The harness runs 14 whole-suite instruments plus the per-artifact
  checks** — the full inventory is in 1.4.0 §B-4 — and two of them import
  the seed (`check_ll_types_agree` via `ntypes`, `check_runtime_sigs_agree`
  via `emit.RUNTIME`), so they cannot survive seed retirement as written.
  Two more exist but are wired to nothing (`selfcheck.py`,
  `spec_coverage.py`), and `check_allocas_hoisted` never runs on stage 1 —
  the binary D-173's defect actually crashed. 1.4.1 closes those gaps
  before anything else moves.
- **The B-7 instrument does not exist.** `check_drops_total` is the shape
  applied to exactly one walker (`type_drops`, with the `DROPS_DEFAULT_OK`
  excuse table); `src/frontend/types.npk` alone has ~35 `TY_`-switching
  walkers no check enumerates.
- **The floor cannot spawn a tool.** `npk_driver_clone_exec` is the only
  spawn: fork-shape clone with an allocation-free child (the copied
  allocator futex bars running ANY Nitpick code there), but its child
  sequence hardcodes stdout→`/dev/null`, a mandatory fd-3 control channel,
  and the 16-slot driver registry. Waiting is already done from Nitpick via
  `sys(SYS_WAITID, P_PIDFD, …)` (nbridge). Nothing anywhere lists a
  directory — `getdents64` appears in no floor symbol, no `sys` call, no
  lib. `npkg` needs a general spawn with caller-directed stdio and a
  directory-listing surface (D-206).
- **The bare-builtin surface for P-3**: 54 bare builtins (38 `never
  fails`), 13 already typed by bespoke checker arms whose settled
  convention is never-fails → **bare T, no `raw`** (the
  `own_fd`/`buffer_new` precedent), may-fail → `Result<T>`. Everything else
  types UNKNOWN, the emitter's 45-entry `rt_sig` table is the coercion
  authority (always `zext`, signedness-blind), and the generator scrapes
  BUILTIN_REFERENCE for names and never-fails only. ~2,470 call sites in
  `src/` (1,679 of them `raw string_concat`), 118 in `lib/`, 663 in
  `tests/`. The full map is in 1.4.0 §P-3.

## Decisions in (SETTLED at 1.4.0 — see `1.4.0.md`, user-ratified in full)

The batch is D-201…D-209, proposed with recommendations at cycle open and
ratified whole:
**P-3** → D-201 (the generated builtin signature table and the typing rule),
**C-10** → D-202 (the fixpoint criterion restated), **C-11** → D-203 (the
committed bootstrap IR, the `bootstrap/` survival map, npkrt.ll's home and
D-015's disposition), **C-12** → D-204 (byte-reproducibility defined and
checked), **C-13** → D-205 (the builder rule and the switch), **B-4** →
D-206 (`npkg`, the spawn primitive, the closed-world link), **C-22** → D-207
(per-scope joins), **S-2** → D-208 (loop-carried moved-from), and the
adoption scope → D-209. **B-7** is an instrument, no decision — built at
1.4.1. The old OPEN_DECISIONS suggestions D-153…D-157 collide with settled
numbers and are superseded by these.

## Subcycle map

| # | Topic | Gated on |
|---|---|---|
| 1.4.0 | **Cycle open — DONE** — the decision batch ratified; the renumbering/staleness doc sweep (this file, SUBSET_1 §4 + :266, the C-10 spec texts, C-17's stale floor count, BUILTIN_REFERENCE's stale site count, `nitpick.toml`'s "1.3" note) | ratification |
| 1.4.1 | **The instruments — DONE** — B-7's `check_type_kinds_total`; wire `selfcheck.py` + `spec_coverage.py` into the run; `check_allocas_hoisted` on stage 1; fix whatever their first runs find | 1.4.0 |
| 1.4.2 | **P-3 — DONE** — the generated signature table, builtin calls typed for real, the emitter's parallel authority retired, the tree-wide re-spell under the transitional rule | D-201 |
| 1.4.2b | **The audit repairs — DONE** — plain-int overflow TRAPS (D-210, the Therac class off the default type) and module bindings `const`/`fixed`-only (D-211, the spellable global race closed) | D-210, D-211 |
| 1.4.3 | **S-2 — DONE** — loop-carried moved-from states in the 0.5 move analysis (the read-before-assign fixed point extended, not a new walk) | D-208 |
| 1.4.3b | **The consuming `pick` — DONE** — `pick (move(v))`, ownership into the matched arm; owning enum payloads stop being write-only (D-216) | D-216, 1.4.3 |
| 1.4.4 | **C-22 — DONE** — per-scope join machinery; channel-in-loop and `shared_arena` drops lifted; the `dyn`-element refusal permanent; PLUS D-215's coercion refusal (a channel-carrying concrete may not erase to `dyn`) | D-207, D-215 |
| 1.4.5 | **Reproducibility mechanics — DONE** — `npkseed.py` ModuleID fix, the `repro` build-twice-cross-cwd stage, the toolchain pinned in the manifest | D-204 |
| 1.4.6 | **The switch — DONE** — the fixpoint IR committed as `bootstrap/seed/stage1.ll` with its STAMP, the harness builds from it, the seed retires (its two coupled instruments re-based), npkrt.ll re-homed, LAYOUT amended | D-203, D-205 |
| 1.4.7 | **Adoption — DONE** — `src/` adopts per D-209's ratified scope (generic collections, `dyn Writer` diagnostics per D-075, the checked-scope list); the fixpoint re-closed under the new builder | D-209, 1.4.6 |
| 1.4.7b | **The pre-1.4.8 batch — DONE** — the close's ratified items (D-234 the loop-bound rule, D-235 every kind decided as a channel element, D-236 manifest-root-relative paths), D-230's `TY_FLAGS`, D-231's strike-and-pin, D-228's width calibration | D-230, D-231, D-234–D-236 |
| 1.4.8 | **`npkg` — DONE** — the spawn primitive and directory listing, `npkg build`/`npkg test`, the closed-world link, parity proven against the harness run-for-run (a harness stage, on every full run) | D-206 |
| 1.4.8b | **The post-parity batch — DONE** — D-237 exact diagnostic matching in both runners (the eight files resolved first, the self-check's new case), D-238 every suite declared in the manifest and read from one table by both runners, the move proven verdict-identical | D-237, D-238 |
| 1.4.8c | **The ratified pair — DONE** — D-239 a name the compiler (`Error`) or the prelude owns refused at every type-namespace declaration, `assoc` and generic parameters included (the loader, RESOLVE-001); D-240 one mistake, one report at the three sites D-237 surfaced | D-239, D-240 |
| 1.4.9 | **Close — DONE** — self-hosting declared under D-202 (stage2 == stage3 at 15,631,627 bytes from `80784f3`; `selfhost`, `repro`, `parity` green on the same tree), the snapshot refreshed from the final tree, docs synced (the D-233 batch drained, G-2 settled by D-231 recorded, the width into ORCHESTRATION), HANDOFF.md retired with every part re-homed, the cycle to `done/`, the per-cycle push | all |

## Watch for

- **The harness-to-`npkg` handoff is the risky moment.** `npkg` must run
  every whole-suite instrument the harness does before the harness goes —
  and per D-206 the harness does NOT go in 1.4: both run through 1.5, npkg
  authoritative, the harness as the belt, retirement folded into
  `meta/SWITCH.md`'s coordinated operation. Never a gap where neither runs.
- **The builder switch inverts the failure mode.** Today a `src/` construct
  the seed cannot compile fails loudly at build. After the switch, the
  committed snapshot compiles ALMOST everything — the failure mode becomes a
  snapshot too stale for a new language feature, which surfaces as a
  mysterious stage-1 refusal. D-205's rule names the discipline: a language
  feature `src/` wants to use must be in the snapshot first, and the
  snapshot refreshes at cycle closes.
- **The adoption sweep is the largest planned diff since 1.1.1** and it is
  the first one validated by the NEW builder. Sequence inside 1.4.7:
  smallest adoption first, fixpoint green between steps, never two idiom
  migrations in one commit.
- **P-3's sweep and the seed must move together** (1.4.2 precedes the
  switch, so the seed still builds `src/`): the seed's `check.BUILTINS`
  wrapped-flags flip and its emitter learns the envelope extraction in the
  same commit as the `src/` re-spell, or stage 1 refuses to build.
- **The user's planned bug/vuln-statistics review** (OPEN_DECISIONS §6c)
  has its natural slot open from this cycle onward — the compiler is
  self-hosting and the library tier grows next. It must land before 1.6
  pulls the trigger; flagging here so the slot is not silently passed.

## What cycle 1.4 taught

Recorded at the close (1.4.9, 2026-09-02), gathered from the subcycle records
and from the executor HANDOFF this section retires. Each entry names the
incident behind it. The rules of working that generalise are repeated
compactly in `ROADMAP.md` ("What cycle 1.4 taught") and, where they are
protocol, in `ORCHESTRATION.md`.

1. **Every plan whose diagnosis could be tested was wrong about the
   diagnosis while right about the goal.** D-208 asked for loop-carried
   moved-from states; the loop rule had held since 0.5.3, and PARAMETERS were
   invisible to the analysis — 26 findings in `src/`, one a live double free
   (1.4.3). D-216 said TYPE-046 already refused a `pick` arm binding an owning
   payload; it was a live use-after-free, confirmed by an executed probe
   reading `0xAA` (1.4.3b). D-210 predicted no deliberate-wrap sites because
   the hash mixers rode `tbb`; they rode plain `uint64`, and FNV's multiply IS
   the wraparound (1.4.2b). TYPE_REFERENCE §26 promised `fixed` on struct
   fields was enforced; it was enforced nowhere. D-207 asked for a second list
   head per scope; the list is a LIFO stack, so a saved MARK does it with one
   pointer (1.4.4). D-223's landing note predicted four tests would pass
   unmodified; one was a real dangling pointer and one hit a different rule.
   D-209's step 1 said the drivers construct the writer once; a `dyn` is
   move-only, so each site builds its own (1.4.7). D-204 said `npkseed.py`
   embedded its argv path in the ModuleID; it emitted `"?"` by accident
   (1.4.5). Two of D-237's eight pre-settlements contradicted the files on
   reading (1.4.8b). **Test the reported symptom before implementing the
   reported fix.**

2. **A rule believed in force because a document says so.** D-078's path
   independence was held by one README line — a dry-run refresh with an
   absolute argument embedded the machine's path in 1,489 of 1,647 site rows
   (1.4.7's close; closed by D-236). BUILD_REFERENCE §7.1's "unexpected
   diagnostics fail a test" was enforced by neither runner since 0.8 (S-9 →
   D-237). D-151's leak check runs only on `exit 0`, so a test reporting
   success as 42 measures nothing (1.4.4). `check_runtime_sigs_agree`'s
   derived-inner leg had never run — it read its flag out of a leaked loop
   variable (1.4.2). `check_allocas_hoisted` never ran on stage 1, the one
   binary D-173's defect actually crashed (1.4.1). SUBSET_1 §4's
   gradual-adoption table never happened, because the seed was the sole
   builder (1.4.0). Making a dormant rule apply is how six live defects
   surfaced, and 1.4.8's answer is structural: one table both runners read
   (D-238), one signature authority (D-201), one toolchain pin every
   invocation is built from (D-204).

3. **"Correct by accident" is the shape to hunt.** Four defects returned the
   right answer from an uninitialised, uncomputed or freed read: `tt_grow`
   never zeroed two of its four side arrays (latent since 1.2.5), the three
   memoised layout bits were read before computation (disabling TYPE-046,
   D-215 and `gives` wherever the window was open), a payload-less enum's
   bits were never written at all, and the family-10 push overflowed its
   768-byte block into the neighbouring slot's header. Every thread join
   slept its whole five-second deadline since 1.1.9 and then reported
   success — `FUTEX_PRIVATE_FLAG` against the kernel's shared wake (1.4.4).
   Poisoning the value — making the wrong answer wrong — exposed each, and
   the `absent-fact` stage keeps that experiment (D-227). A number that lands
   exactly on a configured timeout is never a coincidence.

4. **A count that comes out right can still be a wrong edit.** The family-10
   converter deleted a guard it did not own while its own count read a
   correct 9; only a separate push-preservation assertion caught it, and it
   happened twice in the cycle's tooling. (`convert_family.py` archives with
   this folder; item 10 is the discipline that worked.)

5. **Silence is not success.** `rg -oh` parses as `-o -h`, and `-h` is HELP
   in ripgrep, so a sweep printed the help banner and its filtered form
   printed nothing — which reads exactly like "no matches". Ask any "none" a
   decision rests on a second way. The harness holds the same rule from the
   other side: a filter that matches nothing is an error, never
   `ok 0 test(s) passed`.

6. **Check an instrument against failure before trusting it.** The repo had
   shipped a dead assertion (item 2). Both instruments 1.4.1 added, the
   `absent-fact` stage, the `repro` stage's site-path guard and the runners'
   D-237 rule were negative-controlled, and a control caught a real bug in
   one of them: the absent-fact check's copied tree could not resolve
   `src/main.npk`'s one import outside `src/` (`../lib/nio.npk`), found by
   its own negative control failing to build for the wrong reason.

7. **The compiler is the completeness check, not grep.** Across the
   twenty-two collection families, text search was wrong about "who touches
   this representation" in eight structurally different ways; the compiler
   was wrong zero times — three of 1.4.2's four step-3 defects were caught by
   the compiler checking ITSELF, none by a test. It cannot catch an UNSTARTED
   conversion, and it cannot see an asymmetry that type-checks (FnEmitter's
   push and pop counts moving apart).

8. **Two implementations that must agree are an instrument.** The parity
   stage — both runners over one manifest table, verdicts diffed unit for
   unit, `build/npkc` byte-compared — found three things neither runner
   found alone: the never-enforced §7.1 rule (S-9), `tools/resolve_check.npk`
   never naming the prelude module, and a quadratic capture (`npkg test`'s
   first full run spent 17 of 56 minutes in the kernel). And a second
   implementation of a LIST is a document nobody can trust — the suite list
   lived in both runners as code until D-238 made the manifest the one table.

9. **Four things the stretch cost, none to re-learn.** The refresh criterion
   is stage2 == stage3 and STAGE 2 is what gets installed — stage1.new (the
   OLD builder's emission) differs by construction whenever a change alters
   what the compiler emits, and its body predates the change (D-202's lesson
   in a second place; `bootstrap/seed/README.md`). A fix in the compiler's
   BACKEND does not reach the tools until the snapshot carries it — the
   harness compiles `tools/` with the SNAPSHOT, so the tools' own source (the
   frontend) is always current while their binaries are emitted by the old
   backend (measured at 1.4.7 under D-225: npkc-built checker rc=0,
   snapshot-built rc=3, same sources). REACH is IMPORT-scoped — an async
   function anywhere in a module makes `DeadlineExceeded` reachable in every
   importer, which is why `diag_writer.npk` exists as its own module. `async`
   may not be `never fails` (TYPE-037), which takes `drop` off the path: use
   `?|`.

10. **The method for converting a collection family**, kept because the
    compiler enumerates what a sweep misses only after the sweep starts:
    sweep `src/`, `tools/` AND `tests/`, not just the defining file (two of
    eight hand conversions were caught for a site in another file); apply,
    `quickemit`, self-compile (`npkc src/main.npk`), THEN the harness — the
    self-compile is the cheap check for the family-10 class, since a small
    program has too few instantiations to overflow anything; read the id
    BEFORE the push (`list_push` performs the increment the old code did
    explicitly, so an id computed after the push is off by one — it compiles
    perfectly and corrupts every reference); a bulk append takes
    `list_reserve` before its loop; N counts are reset together, listed
    rather than folded.

11. **The width, and the one red.** A harness run costs ~1.3 cores and
    ~9.5 GB, so memory bounds concurrency (D-228 R4). The cumulative-prefix
    protocol (ORCHESTRATION §4) was measured clean at six concurrent full
    runs on the 1.4.7 close commit (1.4.7b step 4); twelve is unmeasured and
    owed before the first 12-wide window. Going to three wide before OWED-1
    was diagnosed produced the cycle's only red — `extern_c_driver.npk`, exit
    29 once in 40 under load — and it was a race in the C FIXTURE (the
    hostile tail stored after the completion), reproduced 11 times in 120
    under contention and fixed at its source; the Bridge, the reactor and the
    optimiser were exonerated by measurement, never retried. When a red
    appears, the IR is often the cheap decisive test: emitting the failing
    program from both trees and `cmp`-ing them settled in two minutes what a
    harness run would have taken fifty to say.

12. **Checked and NOT a defect** (kept so it is not re-derived): a
    borrow-carrying concrete coerced to `dyn` is not refused at the coercion,
    and that is fine — the escape analysis catches the hazard through another
    door. Four probes: the coercion compiles; a `dyn` outliving a BLOCK is
    safe because D-191 extends an address-taken local to the function's end;
    the shape that actually dangles (a `dyn` carrying a borrow to its own
    frame, returned upward) is refused by `NITPICK-BORROW-001` at the coercion
    itself. D-215 needed its own rule because a channel endpoint is a handle
    the escape analysis has no reason to follow; a borrow is precisely what it
    does follow. The first three probes each supported the opposite
    conclusion.

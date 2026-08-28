# Cycle 1.4 — Self-hosting

**Phase C, the milestone that matters.** Everything before 1.4 is validated
against the seed's output; after it, the compiler validates itself. This is
where the builder switches from the regenerated Python seed to committed stage
IR, the fixpoint is re-closed after 0.9–1.3's additions, `src/` finally adopts
the language it implements, builds become byte-reproducible beyond one process
on one machine, and `npkg` — the permanent build/test/verify runner — is born
and proven against the throwaway Python harness.

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
| 1.4.0 | **Cycle open** — the decision batch ratified; the renumbering/staleness doc sweep (this file, SUBSET_1 §4 + :266, the C-10 spec texts, C-17's stale floor count, BUILTIN_REFERENCE's stale site count, `nitpick.toml`'s "1.3" note) | ratification |
| 1.4.1 | **The instruments** — B-7's `check_type_kinds_total`; wire `selfcheck.py` + `spec_coverage.py` into the run; `check_allocas_hoisted` on stage 1; fix whatever their first runs find | 1.4.0 |
| 1.4.2 | **P-3** — the generated signature table, builtin calls typed for real, the emitter's parallel authority retired, the tree-wide re-spell under the transitional rule | D-201 |
| 1.4.3 | **S-2** — loop-carried moved-from states in the 0.5 move analysis (the read-before-assign fixed point extended, not a new walk) | D-208 |
| 1.4.4 | **C-22** — per-scope join machinery; channel-in-loop and `shared_arena` drops lifted; the `dyn`-element refusal made permanent | D-207 |
| 1.4.5 | **Reproducibility mechanics** — `npkseed.py` ModuleID fix, the `repro` build-twice-cross-cwd stage, the toolchain pinned in the manifest | D-204 |
| 1.4.6 | **The switch** — the fixpoint IR committed as `bootstrap/seed/stage1.ll` with its STAMP, the harness builds from it, the seed retires (its two coupled instruments re-based), npkrt.ll re-homed, LAYOUT amended | D-203, D-205 |
| 1.4.7 | **Adoption** — `src/` adopts per D-209's ratified scope (generic collections, `dyn Writer` diagnostics per D-075, the checked-scope list); the fixpoint re-closed under the new builder | D-209, 1.4.6 |
| 1.4.8 | **`npkg`** — the spawn primitive and directory listing, `npkg build`/`npkg test`, the closed-world link, parity proven against the harness run-for-run | D-206 |
| 1.4.9 | **Close** — self-hosting declared against D-202's criterion, the snapshot refreshed, docs synced, the cycle to `done/` | all |

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

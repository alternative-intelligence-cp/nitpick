# HANDOFF — for the executor picking up cycle 1.4

> Written 2026-08-28 by the planning session (Fable), for a fresh
> session (Opus) to execute from. CLAUDE.md carries the standing
> context and is current; this file is the runway: where you are, what
> to do next, the rules that bite, and when to stop and escalate.
> Retire this file at the 1.4 cycle close.

## Where you are

- HEAD is the 1.4 stretch: `41cad00` (1.4.0 — the cycle opened, D-201…
  D-209 ratified), `cf55611` (1.4.1 — the walkers-total instrument and
  its finds, including the enum-drop silent leak fixed), `fec4cfa`
  (the research digests, the ratified coverage audit, D-210…D-221, and
  every plan file), plus the handoff commit carrying this file. All
  pushed.
- The harness is GREEN: 60 suites, 173 real-backend programs (each
  also through `opt -O2`), fixpoint byte-identical, all instruments
  passing. `python3 bootstrap/harness/harness.py` reproduces it
  (~20 min idle, 35+ under load — it is not hung, check
  `/tmp/npk-harness-*/` for progress).
- **Every decision through the end of cycle 1.5 is SETTLED and
  recorded** (D-201…D-221 in `meta/specs/DECISIONS.md`). The one
  externally-gated item is C-19 (the AbsInt contact), due by 1.5's
  exit — the question list is in `meta/roadmap/1.6/README.md`.

## The queue (each has an execution-grade file in `meta/roadmap/1.4/`)

1. **1.4.2** — P-3: the builtin signature table (D-201). THREE steps in
   order, fixpoint green after each; never combine them.
2. **1.4.2b** — overflow traps (D-210) + const/fixed-only module state
   (D-211). Two commits.
3. **1.4.3** — loop-carried moved-from states (D-208).
4. **1.4.3b** — the consuming `pick (move(v))` (D-216).
5. **1.4.4** — per-scope joins (D-207) + the dyn coercion refusal
   (D-215).
6. **1.4.5** — reproducibility mechanics (D-204).
7. **1.4.6** — THE SWITCH (D-203/D-205): committed builder, seed
   retires, npkrt.ll re-homes. Push at this one.
8. **1.4.7** — adoption (D-209), smallest step first, one idiom per
   commit.
9. **1.4.8** — `npkg` + `npk_spawn` + the nfs riders (D-206/D-213).
10. **1.4.9** — close: fixpoint declared per D-202, snapshot refreshed,
    docs synced, cycle folder to `done/`.

The plan files carry file:line anchors verified at planning time —
**re-verify each anchor before editing** (lines drift as work lands);
the anchor tells you what to look for, not a blind offset.

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
- **The seed still builds `src/` until 1.4.6** — src/ stays subset-1
  until the switch; 1.4.2's Step 2 is the one place the seed itself is
  edited (its file says exactly how).

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

Read `meta/roadmap/1.4/1.4.2.md` end to end, re-verify its anchors
against the current tree, and begin Step 0. Announce the step you are
on as you work; commit per the file's acceptance section.

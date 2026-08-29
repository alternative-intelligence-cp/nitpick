# HANDOFF — for the executor picking up cycle 1.4

> Written 2026-08-28 by the planning session (Fable), for a fresh
> session (Opus) to execute from. CLAUDE.md carries the standing
> context and is current; this file is the runway: where you are, what
> to do next, the rules that bite, and when to stop and escalate.
> Retire this file at the 1.4 cycle close.

## Where you are

- HEAD is `ad965fc`, pushed. Since the 1.4.0–1.4.1 stretch, SIX subcycles
  landed, each with a full green harness run and its own commit:
  **1.4.2** (D-201, four commits: the generated builtin signature table),
  **1.4.2b** (D-210 overflow traps, D-211 module state), **1.4.2c**
  (D-222 — `const` retired), **1.4.3** (D-208), **1.4.3b** (D-216), and
  **1.4.4 item 6** (D-215).
- The harness is GREEN: 60 suites, 179 real-backend programs (each also
  through `opt -O2`), fixpoint byte-identical.
  `python3 bootstrap/harness/harness.py` reproduces it (~25–35 min; it is
  not hung, check `/tmp/npk-harness-*/`).

## NEXT: 1.4.4 items 1–5 — the per-scope join machinery

`meta/roadmap/1.4/1.4.4.md` is the plan; its **Execution record** carries
every anchor measured on 2026-08-29 (frame layout, spawn link, join drive,
scope extents, chfin reclaim, the C-22 rung to lift, the dyn rung that
STAYS, the three harness excuse rows) so you do not re-derive them.

Item 6 (D-215) is done. Items 1–5 are the deep half — frame layout, scope
exit across four exit kinds, channel reclaim re-homed, `TY_SHARED_ARENA`
gaining a drop — and they were deliberately left for a fresh session. Read
the record's ordering note before writing any emission, and give the
concurrency tests `// stress: 40`.

## What today's six subcycles should change about how you read a plan

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

The pattern: **a rule believed in force because a document says so.**
Making a dormant rule apply is how four live defects surfaced today,
including two memory-safety holes. Expect the same from items 1–5.

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

Read `meta/roadmap/1.4/1.4.4.md` end to end — the plan AND its execution
record — then start on items 1–5. Re-verify each anchor before editing
(lines drift); the anchor says what to look for, not a blind offset.
Announce the item you are on; commit per the file's acceptance section.

# Cycle 1.6 — the LLVM-native analyzer evidence (D-233)

**Phase C.** Abstract interpretation over the emitted IR, translation
validation of the optimizer, and the assembled evidence package. **This file
was the Astrée preparation handbook until D-233 (2026-09-01) superseded
D-232 and moved the evidence to the IR itself** — the old content survives
in git history and in `../research/digests/r3-digest.md`, and the parts of
it that transfer are folded into the subcycles below. There is no external
clock anywhere in this cycle: every engine is open, pinned by commit, built
on the workbench, and run as a standing instrument.

> The deciding survey is
> `../research/LLVM_Formal_Verification_Tool_Options.md`; its
> decision-grade digest with reliability notes is
> `../research/digests/llvm-tools-digest.md`. Per the r5/r8 rule, never
> cite the survey's prose without the notes — its most-cited source is an
> empty citation row, and its SMACK "complete path coverage" claim is
> wrong for this language (digest note 4).

## The three legs (D-233's architecture)

| Leg | Evidence class | Owner |
|---|---|---|
| **A** | Whole-program runtime-error absence — OOB, div-by-zero, null/uninitialized reads — by sound abstract interpretation over OUR emitted IR | The engine 1.6.0's gate names: **Clam/Crab** vs **IKOS** |
| **B** | Per-obligation proof — overflow, bounds, casts, contracts, `limit`, termination, ERR-exit | **Z3 under D-218** — 1.5's spine, untouched by this cycle |
| **C** | Optimizer integrity — the pinned `opt -O2` pipeline does not remove a guarantee | **Alive2** beside the opt-O2 harness leg (which stays) |

Depth tools are NAMED ESCALATIONS with entry criteria, not adoptions:
SeaHorn (CHC/Spacer) if leg A's triage meets invariants its numeric domains
cannot close — the D-150 chunk-bitmap class is the expected tenant;
SAW/Crux if the Bridge wire marshaling wants extensional-equality proof
beyond 1.5.6's Z3 leg. Tool adoption is monotone (a later analyzer adds
evidence and invalidates none), which is why this sequencing is a decision
and not a deferral — D-233 states the rule.

## Subcycle map

| # | Topic | Gated on |
|---|---|---|
| 1.6.0 | **The bring-up gate** — both leg-A candidates built at pinned commits and run against the same three emissions: `dyn_slots.npk` (small; aggregates, `dyn`), `extern_c_driver.npk` (the richest idiom mix — bridge, coroutines, deadlines), and `src/npkc.npk`'s own emission (scale). Measured per engine: (1) does it INGEST our LLVM-20 opaque-pointer IR at all, and if not, what is the port distance — IKOS sits at LLVM 14 today, across the opaque-pointer break, which is the NIKOS-shaped port; Clam targets 15 with support to 18, and our conservative instruction vocabulary may cross 18→20 textually for free; (2) alarm count and quality on those three programs; (3) wall-clock and memory; (4) determinism controllability — fixed seeds/options, verdicts stable across runs and cwds (the D-204 discipline applied to a new tool). **One engine wins by these measurements; the other is decided out.** Alive2 is built at the 20.1.2-matching commit in the same subcycle and smoke-run on one program's pre/post-opt pair. If the winning engine needs a port, the port is THIS cycle's work and starts here — scoped by the gate's measurements, not assumed. | D-233 |
| 1.6.1 | **Leg A as a standing stage** — the winning engine wired into the harness beside opt-O2, over every real-backend program's emission; `nitpick.toml` grows the pinned-tool table (commit hash + options, read by the invocation, the D-204 "pinned AND READ" rule); the **alarm ledger** is born: a committed baseline of known alarms with per-alarm dispositions (true-defect / imprecision-accepted / fact-missing), where a NEW alarm on an unchanged tree fails the stage — runs diff, never restart. The emission grows its analyzer-visible facts where triage shows they pay: the D-218.9 `llvm.assume` range rows carrying what the type system, D-148 envelopes and REACH already know (the old data-dictionary idea, landed as IR facts instead of pragmas). The analyzer's model of the npkrt bottom (sys trampolines, futex park, clone/execve) consumes the TCB.md list 1.5.6 wrote — same enumeration, new consumer. | 1.6.0 |
| 1.6.2 | **Leg C as a standing stage** — Alive2 over the opt-O2 leg's pre/post pairs, per-pass where the whole-module diff is outside its competence; its own ledger with the inter-procedural blind spot (inlining) recorded per program; the exit-code opt-O2 leg RETAINED as the end-to-end net. A refinement failure here is a stop-the-line miscompile finding, the 1.3.8 class with a proof attached. | 1.6.0 |
| 1.6.3 | **The dry run and the evidence package** — a full analyzer pass over npkc's own emission (its cadence decided here by measured runtime, not assumed into every harness run), then the package assembled: leg-A verdicts + alarm ledger, the D-218 obligation manifest and elision rows, leg-C's ledger, TCB.md's enumerated floor, and the toolchain/tool pins — the verified-middle-end-plus-validated-floor claim (C-17.11) restated over the new evidence set. Escalation criteria for the depth tools evaluated against the ledger ONCE, with the answer recorded either way. Docs synced; cycle to `done/`. | 1.6.1, 1.6.2 |

## Watch for

- **Version drift between the two pins.** The LLVM toolchain pin (20.1.2)
  and each tool's commit pin move independently; a toolchain upgrade
  re-opens the gate's ingestion question. Any change to either pin is a
  manifest change with a full run behind it, never a quiet bump.
- **The alarm ledger is the discipline that makes leg A an instrument.**
  An analyzer whose alarms are re-triaged from scratch each run decays
  into noise nobody reads — the ledger's diff-not-restart rule is what
  the stage's green means. A new alarm on an unchanged tree is a stop
  sign (the R5 shape, applied to analysis).
- **Determinism extends to every verdict source.** A verdict is a function
  of (input, tool build, budget) — D-218.2's law, generalized by D-233.
  An engine option that trades determinism for speed is refused, not
  tuned.
- **The old one-shot anxieties do not transfer, and the scheduling rule
  does.** Trial attempts stopped being scarce; proof invalidation did not.
  Everything entering the LANGUAGE still lands before the evidence
  campaign closes — D-233 restates the standing rule with its new basis.
- **The switch (`meta/SWITCH.md`) still waits on 1.6** and inherits the
  three audit corrections (stale ship-list, unowned prototype-coverage
  pass, `meta/specs/` completeness) — swept during 1.4/1.5 while both doc
  sets are live. Nothing about the switch happens until this cycle is
  finished.

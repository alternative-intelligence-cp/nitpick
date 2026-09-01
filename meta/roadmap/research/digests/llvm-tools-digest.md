# Digest of LLVM_Formal_Verification_Tool_Options.md — the Astrée replacement survey (produced at the D-233 replan, 2026-09-01; source: ../LLVM_Formal_Verification_Tool_Options.md)

> Extraction from the deep-research report "Formal Verification Strategies for
> LLVM-Native Toolchains: An Evaluation of Alternatives to Astrée",
> commissioned by the user when the Astrée route was reconsidered. **Read the
> reliability notes (§7) before citing the report's prose** — the house rule
> the r5/r8 digests established. The verified-by-hand rows in §6 were measured
> on 2026-09-01 against the projects' own repositories and OVERRIDE the
> report's prose where they disagree.

## 1. The report's core argument (§1–§2)

Verifying a C rendering instead of the shipped IR attaches the evidence to
the wrong artifact. The report frames this as the IR→C "semantic chasm"
(poison/undef and the two-phase memory model have no C equivalent, so a
translator must distort them — false positives and false negatives both).
**Scope correction for OUR case**: D-232's emitter design was AST→C from the
same recorded-type authority as the IR emitter — a sibling lowering, not an
IR→C translator — so the poison-chasm argument does not apply verbatim. The
objection that DOES apply, and decides the matter, is the sibling-artifact
one: Astrée would have analyzed a lowering that never ships, while every
LLVM-native tool analyzes the very IR that becomes the binary. Evidence about
the artifact beats evidence about a model of it.

The report also answers the eleven C-19 AbsInt questions one by one with
"the LLVM-native ecosystem makes the question moot" (§2's table) — mostly
fair, with the §7 caveats below.

## 2. Abstract interpretation on LLVM (§3) — the Astrée seat

- **IKOS** (NASA Ames; NOSA 1.3 license): interval/congruence/gauge domains
  over an LLVM-bitcode frontend (ARBOS/AR normalizes away SSA φ-nodes);
  Bourdoncle fixpoint iteration; the Mikos work cut peak memory up to ~95%.
  Proves absence of OOB, div-by-zero, null deref, uninitialized reads. Known
  weak on multithreading (fine for us: per-task sequential logic, message
  passing between).
- **Crab/Clam** (Apache-2.0): abstract interpretation with a REGION-BASED
  memory model — sea-dsa partitions the heap into proven-disjoint regions
  with allocation-site + recency abstraction, enabling strong updates. The
  report singles it out as the better fit for verifying D-150's allocator
  metadata (headers vs payloads as distinct abstract fields).

## 3. Symbolic simulation and ITP (§4) — depth tools

- **SAW / Crux-LLVM** (Galois; BSD-3): symbolic execution of LLVM bitcode
  through Crucible; proves extensional equality against Cryptol/SAWScript
  specs; compositional overrides prevent path explosion. Report's suggested
  target: the D-149 Bridge wire marshaling (prove malformed IPC data always
  routes to the error path).
- **Heapster** (SAW extension): type-checks LLVM bitcode with a
  separation-logic type system and EXTRACTS pure Coq specifications; the
  report maps it onto the borrow rules. Heavy adoption (Haskell + Coq).

## 4. CHC and BMC (§5)

- **SeaHorn** (Spacer/Z3-PDR over Constrained Horn Clauses, sea-dsa memory):
  infers universally quantified invariants — the report's candidate for the
  D-150 chunk-bitmap consistency class.
- **SMACK** (LLVM→Boogie), **Symbiotic** (slicing + KLEE fork; SV-COMP
  winner): bounded model checking. **See §7 note 4 — the report's "complete
  path coverage" claim for SMACK is wrong for this project.**

## 5. Foundational semantics (§6)

- **Vellvm**: mechanized LLVM IR semantics in Coq (ITrees) — the ground-truth
  reference, not an operational tool for us.
- **Alive2** (MIT): translation validation of LLVM optimization passes —
  proves the post-`opt` IR refines the pre-`opt` IR, with full poison/undef
  semantics. `alive-tv` (two-file mode) and an `opt` plugin. **Stated limit:
  no inter-procedural transformations** (spurious counterexamples possible)
  — inlining-heavy whole-module -O2 must be scoped accordingly.

## 6. Verified by hand, 2026-09-01 (fetched from the projects' repos — these rows override the report)

| Tool | License | LLVM support TODAY | Fit against our pinned 20.1.2 |
|---|---|---|---|
| IKOS v3.5 | NOSA 1.3 | **LLVM/Clang 14.0.x** | Six majors behind, across the opaque-pointer break — adoption means a real port (the NIKOS shape D-217 parked) |
| Clam/Crab | Apache-2.0 | master targets **LLVM 15**, support **up to 18** | Closest AI engine; 18→20 textual-IR gap for our conservative instruction vocabulary is plausibly nil — measure, don't assume |
| Alive2 | MIT | **tracks LLVM main**; older LLVM = matching Alive2 commit | A 20.1-era commit exists by construction; zero version gap |

Clam's own README states the governing fact the report never mentions:
**"LLVM bitcode is not compatible across major releases."** Tool-version
bring-up is therefore a real, measured gate — not an assumption — and it is
1.6.0's job under D-233.

## 7. Reliability notes (the r5/r8 discipline applied)

1. **Citation 14 — the report's most-cited source — is an EMPTY row** in its
   works-cited list (it is the uploaded project context; citations 1 and 3
   are `nitpick-specs.md` and `questions.md`). Every Nitpick-specific claim
   traces to what the report was fed, not to the repo.
2. **"the Staunton team"** appears twice — a hallucinated project name.
   Harmless, but it calibrates how carefully the prose was assembled.
3. Prototype-era spellings ride throughout: `@local` for borrows,
   `MAX_INSTANTIATION_DEPTH`. Directionally right, textually stale.
4. **The SMACK claim is wrong for us and must not enter the plan**: "SMACK
   can be configured with unroll bound 64 … guaranteed complete path
   coverage." The 64-cap is on MACRO recursion, generic instantiation depth
   and comptime fuel — compile-time machinery. Ordinary runtime loops and
   recursion are NOT bounded by 64, so a bound-64 BMC run is a bounded
   search, not coverage. (1.5's termination obligations exist precisely
   because runtime iteration is unbounded.)
5. Two arXiv IDs (2605.26169, 2607.07126) could not be pattern-matched to
   real entries with confidence; treat those two citations as unverified.
6. The LLVM-version currency of the tool ecosystem — the single most
   plan-shaping fact — is absent from the report entirely; §6 above supplies
   it.

## 8. What the replan took (see D-233 for the ratified form)

Three legs: (A) abstract interpretation over the emitted IR — engine chosen
at a measured bring-up gate between Clam/Crab and IKOS; (B) the D-218 Z3
obligation architecture, untouched; (C) Alive2 translation validation beside
the opt-O2 harness leg. SeaHorn and SAW/Heapster recorded as named
escalation candidates with entry criteria, not adopted now. The C emitter
struck with D-232. The five-tier "adopt everything" table was deliberately
NOT taken whole — five heavyweight toolchains at once is breadth-first
effort spend, and tool adoption is monotone (a later analyzer adds evidence
without invalidating any), so sequencing by measured need costs nothing.

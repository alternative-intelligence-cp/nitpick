# Digest of r3.md — Astrée preparation (produced at 1.4.x planning; source: meta/roadmap/research/r3.md)

> Extraction from the deep-research report "Strategic Preparation for Astrée
> Static Analysis: A Comprehensive Handbook for Zero-False-Alarm
> Verification". Bracketed numbers are the report's own citation indices.
> **Standing caveat: the report cites NO AbsInt first-party documentation**
> (its 40 sources are the 2005 ESOP paper, academic pages, reseller pages,
> and release videocasts) — its claims are consistent with everything known
> but the C-19 contact must confirm them first-hand.

## 1. Input formats (§1)

**C and C++ source only; no LLVM-IR/assembly/binary path.** "The primary
input constraint for the Astrée static analyzer is absolute: the system
operates exclusively on C and C++ source code. There is no native ingestion
pathway for LLVM Intermediate Representation (IR), compiled object code,
assembly language, or raw machine binaries" [1]. Binary/WCET analysis is
AbsInt's separate aiT toolsuite [6][7].

- C standards: C90, C99, C11, C18 fully supported.
- C++: "high maturity in recent release cycles" — C++98/11/14/17, OO
  semantic checks, state-machine domains, standard-container stubs [8].
  MISRA C:2004/2012(+A1/A2), MISRA C++:2008, AUTOSAR C++14, MISRA
  C++:2023 (C++17) [9].
- Subset restrictions ("the primary cause of failed 30-day trials" is
  ignoring them):
  - **Dynamic allocation: strictly excluded** (malloc/free/new/delete break
    the static heap model; all memory static or stack) [1].
  - **Recursion: prohibited** (semantic expansion inlines abstract state at
    call sites); even bounded recursion "highly discouraged and generally
    flagged", flatten it [1].
  - **Function pointers: fully supported** — exhaustive points-to sets,
    multiple targets analyzed as the union [1].
  - **Unions: historically excluded/highly restricted**; newer versions
    handle "specific union overlays"; type punning yields imprecision [1].
  - Variadic functions: generally restricted (MISRA mandates removal) [11].
  - Backward goto / setjmp/longjmp: heavily restricted or excluded;
    forward goto/break/switch/continue fully supported [1].
  - libc and POSIX threads: not analyzable directly — rigorously stubbed
    [1]. RTOS-level tasking is different (see §3): OSEK/AUTOSAR native.

## 2. The __ASTREE_ directive system (§2)

Injected as pragmas in source or via the external **--config-sem** file.
Unconstrained inputs are the noise source: the interval domain assumes full
range and every downstream division/index alarms. "Configuring these
directives is the single most time-consuming task of the preparation
phase" [5].

| Directive | Purpose |
|---|---|
| `__ASTREE_volatile_input((var, [min, max]));` | hardware-fed variable: nondeterministic between reads, never outside bounds (12-bit ADC: `[0, 4095]`) |
| `__ASTREE_known_fact((cond));` | unconditional axiom; prunes contradicting paths |
| `__ASTREE_assert((cond));` | prove-or-alarm a safety property in all states |
| `__ASTREE_modify((var));` | variable changed unknown-ly (DMA) — back to full range |
| `__ASTREE_wait_for_clock(());` | ends one reactive-loop iteration; forces the per-tick fixpoint |
| `__ASTREE_max_clock((N));` | bounds total ticks (10-hour flight at 100 Hz = 3600000) — stops float bounds degrading under "runs forever" |
| `__ASTREE_log_vars((var));` | print the abstract bounds at this CFG point (alarm debugging) |

## 3. Entry points, tasks, stubbing (§3)

- Entry point: **`--exec-fn main`**. Bare-metal shape: init → read volatile
  inputs → compute → write outputs → `__ASTREE_wait_for_clock()` per loop.
- Concurrency: **MultiSSE** computes all interleavings for race/deadlock
  absence [20]. AUTOSAR ARXML / OSEK OIL files parsed directly — tasks,
  priorities, ceiling protocols, alarms, resource locks extracted; "no
  manual changes to the software under analysis" [23]. ISRs = highest-
  priority tasks, preempt anywhere, run to completion [21]. Races proven
  absent via thread-modular "digests" [22]. (ARINC 653 named in the
  heading, never detailed in the body.)
- Stubbing: inline asm, intrinsics, drivers, closed binaries → "simplified,
  highly abstracted C function replicating the side effects": timer-read
  stub = bounded `__ASTREE_volatile_input`; math-intrinsic stub = bounded
  via `__ASTREE_known_fact`. "A comprehensive, robust stub library is a
  mandatory prerequisite" [5].

## 4. Machine-generated code (§4) — the sections about OUR situation

"Astrée was originally conceived, designed, and optimized to analyze
exactly this type of code: real-time, synchronous control-command
applications generated directly from synchronous data-flow
specifications" [1].

- Generators (SCADE, TargetLink, Embedded Coder) emit subset-respecting C:
  no dynamic allocation, no recursion, flat single-entry/single-exit
  control flow ("a massive sequence of assignments, simple conditionals,
  and bounded loops") — "maps perfectly to Astrée's trace partitioning and
  loop unrolling", analyzing FASTER than hand-written code.
- **Regular naming** maps variables back to model-level origins.
- **The decisive advantage: automated annotation.** The generator's data
  dictionary holds physical min/max for every signal; TargetLink
  auto-exports ranges/scaling/fixed-point constraints as Astrée
  annotations [16] — "eliminates weeks of manual preparation work."
- Compute-Through-Overflow: dedicated domains recognize intentional
  intermediate wrapping in generated fixed-point code [3].
- **The transferable conclusion**: every advantage credited to
  SCADE/TargetLink — subset-respecting emission, regular naming, flat
  control flow, mechanically emitted range directives — is a property a
  compiler backend can deliberately provide. The "data dictionary" is
  information npkc's type system and REACH analysis already hold.

## 5. Case-study numbers (§5)

| Program | Size | Time / resources | Alarms |
|---|---|---|---|
| Airbus A340 fly-by-wire | 132,000 LOC | 1h20m (2.8 GHz PC, 300 MB RAM) | **0 false alarms** [2] |
| Airbus A380 electric flight control | ~350k–1M LOC | ~6h–overnight per module | **0 false alarms** [2] |

- What dominated the schedule: "not configuring the analyzer tool itself,
  but analyzing the physical environment" — specifying every sensor
  bound, modeling the runtime environment, tuning variable packing [1].
  No numeric team-size/calendar figures are given.
- Precision machinery: Interval alone diverges on relational code;
  **Octagon** (±x±y bounds) kills rate-limiter false alarms; **Ellipsoid**
  bounds IIR filter state; **packing** (3–4 variables per pack, syntactic
  proximity) keeps cost linear in code size [1].
- Alarm taxonomy: Type A (fatal for that context) vs Type B (continue
  under worst-case; the cascade source) [3].

## 6. Commercial/trial mechanics (§6)

- 30-day evaluation license, node-locked or floating [3]; commercial:
  node-locked, floating, subscription. No pricing in the report.
- Trials include **Field Application Engineer** access: helping import
  `compile_commands.json` (mirroring the production compiler's
  preprocessor config exactly) and set up ARXML/TargetLink bridges [9].
- Qualification kits: DO-178C to DAL-A, ISO 26262 ASIL D, EN-50128,
  FDA [10].

## 7. The day-1 readiness checklist (§7)

Governing rule: the 30 days are for reviewing legitimate alarms and tuning
(`--inner-unroll` etc.) — "not writing C stubs or hunting through
documentation for sensor specifications" [5].

- **Phase 1, weeks −4..−3 (sanitization)**: eliminate dynamic memory;
  flatten recursion; isolate hardware dependencies into stub-ready headers.
- **Phase 2, weeks −3..−2 (semantic modeling)**: build the stub library;
  compile the DATA DICTIONARY (every external input with physical min/max)
  — "the most critical step"; draft the config-sem directives file +
  `__ASTREE_max_clock`.
- **Phase 3, week −1 (build integration)**: emit `compile_commands.json`;
  prepare ARXML/OIL task artifacts.
- **Phase 4, day 1**: baseline run; triage Type A first, then burn down
  Type B by refining bounds.

Named time sinks: directive configuration (the single biggest); one
unconstrained input cascades noise everywhere; missing/wrong stubs = fatal
parse errors or massive imprecision.

## 8. What the C-19 AbsInt contact must confirm (the report's gaps)

The report flags nothing as unknown and cites no first-party docs — so:

1. Confirm no LLVM-IR/assembly path exists in the CURRENT product, and
   whether any roadmap changes that. If confirmed, C-19 resolves to
   "Astrée requires emitted C", with §4's generated-code practice the model.
2. Real-world C++ front-end maturity; whether generated C remains the
   recommended input over generated C++.
3. Current union/variadic admission specifics.
4. Whether ANY bounded recursion is acceptable or all must be flattened
   (relevant to emitted runtime code).
5. Trial terms: FAE support for first-time evaluators, whether the clock
   start can follow a readiness review, extension policy, pricing.
6. The config-sem file grammar (user manual needed).
7. Current alarm classification/triage tooling.
8. RuleChecker bundling vs separate license.
9. ARINC 653 support depth (only if ever relevant).
10. Typical preparation-effort norms for a codebase of npkc's scale.

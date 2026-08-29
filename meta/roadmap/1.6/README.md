# Cycle 1.6 — Astrée preparation

**Phase C, the one that cannot be retried.** A single non-renewable 30-day Astrée
run. Everything about this cycle is shaped by that fact: the work is preparation so
thorough that the 30 days are spent analyzing, not discovering that the input format
was wrong.

> Detailed **map**, and deliberately short — most of 1.6's content is *gates that
> must close earlier*, not subcycles that run here. (This file was internally
> titled "Cycle 1.5" until the 1.4.0-era doc sweep — the renumbering that made
> Astrée 1.6 swept the folder, not the body. Its "before 1.4 exits" gate language
> meant "before self-hosting's successor exits" under the old map and now reads
> "before 1.5 exits", matching ROADMAP and C-19's row.)

## What the R-3 research settled and what it could not (1.4-era; full digest: `../research/digests/r3-digest.md`)

The deep-research pass (r3.md) confirms the working assumption with one
caveat that keeps C-19 open:

- **Input is C and C++ source only** — "no native ingestion pathway for
  LLVM IR, compiled object code, assembly, or binaries" (binary analysis
  is AbsInt's separate aiT product). C90–C18; the C++ front-end is
  described as mature through C++17. **Caveat: the report cites no
  AbsInt first-party documentation** — the claim matches everything
  known, but first-party confirmation IS the C-19 contact.
- **The analyzable subset**: no dynamic allocation (static/stack only),
  no recursion (even bounded is flagged), function pointers fully
  supported (exhaustive points-to), unions restricted, variadics
  restricted, forward-only unstructured jumps, everything external
  stubbed.
- **The generated-code sections describe OUR situation**: Astrée "was
  originally conceived, designed, and optimized" for machine-generated
  synchronous control code, and everything it credits SCADE/TargetLink
  for — subset-respecting emission, regular naming that maps back to
  source origins, flat control flow, and mechanically emitted
  `__ASTREE_volatile_input`/`known_fact` range directives from the
  generator's data dictionary — **is a property a C-emitting npkc
  backend can deliberately provide**. The "data dictionary" (physical
  min/max per external input) corresponds to information the type
  system, the D-148 literal envelopes, and REACH already hold.
- **Benchmarks**: A340 132 kLOC → 1h20m, zero false alarms; A380
  350k–1M LOC → ~6h–overnight per module, zero false alarms. The
  preparation effort was dominated by ENVIRONMENT analysis (sensor
  bounds, stubs, packing), not analyzer configuration.
- **Trial mechanics**: 30-day evaluation, node-locked or floating, with
  Field Application Engineer support; project setup ingests
  `compile_commands.json`; qualification kits to DO-178C DAL-A /
  ISO 26262 ASIL D.

## The gate that must close before 1.5 exits (C-19)

The docs assumed Astrée analyzes "monomorphized output"; the research
confirms it accepts C — so **if AbsInt confirms C-only, a C-emission
path is Phase-C work that must be scheduled before 1.6**, designed to
the generated-code playbook above (subset-respecting emission is the
easy half for us: no dynamic allocation in the analyzed artifact means
the emitted C models the D-150 allocator explicitly or the analysis
scopes around it — one of the questions below).

## The AbsInt question list (the C-19 contact; sharpened by R-3's gaps)

1. **Input format, first-party**: confirm no LLVM-IR/assembly path in
   the CURRENT product and none on the roadmap. (The report's "absolute"
   claim rests on the 2005 paper and reseller pages.)
2. **C++ front-end reality**: what C++ subsets analyze well in practice;
   whether generated C remains the recommended input over generated C++.
3. **Unions and variadics**: exactly what is admitted today.
4. **Recursion**: is ANY bounded recursion acceptable, or must all be
   flattened (bears on emitted runtime code).
5. **Dynamic allocation policy for a language runtime**: how projects
   with a real allocator present it — analyzed as static pools, stubbed,
   or scoped out; what the D-150 slab allocator should look like to the
   analysis.
6. **Entry points and the executor**: how an `async main` over a
   per-thread executor maps to the task model — MultiSSE with a hand
   -declared task set, or the synchronous `--exec-fn` shape per thread;
   what of AUTOSAR/OSEK-style config import applies to a non-OSEK
   runtime.
7. **Runtime-floor stubbing**: policy for `sys` trampolines, futex
   parking, clone/execve — the enumerated TCB bottom (r8's Lesson 2).
8. **The evidence package**: whether the SMT elision manifest and unsat
   cores are consumable alongside, or parallel evidence only.
9. **Trial terms**: FAE support scope for a first-time evaluator;
   whether the 30-day clock can start after a readiness review; license
   form; extension policy; pricing.
10. **config-sem grammar and current alarm-classification/triage
    tooling** (the user manual, which the public research never reaches).
11. **Preparation-effort norms** for a codebase of npkc's scale (the
    Airbus effort is described only qualitatively in public sources).

## The preparation checklist (adapted from R-3's "Day-1 Readiness"; work owed to earlier cycles)

- **Sanitization is a code-generation property here, not an audit**: the
  C emitter (if C-19 confirms) emits no dynamic allocation in analyzed
  code, no recursion, isolated hardware/syscall boundaries — designed
  in, per the SCADE observation, not cleaned up after.
- **The data dictionary is generated**: every external input's physical
  bounds emitted as `__ASTREE_volatile_input` from the types + declared
  envelopes; `__ASTREE_known_fact` from proven invariants (the Z3
  synergy — r5/r8 digest: Z3 prunes Astrée's alarms, Astrée's invariants
  seed Z3's context).
- **The stub library** for the runtime floor's quarantined bottom
  (syscalls, futex, clone/execve), each stub bounded.
- **Build integration**: `compile_commands.json` emission from `npkg`.
- **The dry run** (1.6's own subcycle): a full pass on a representative
  program BEFORE the clock starts; a failed dry run is a schedule input.
- **Triage doctrine**: Type A alarms first, then Type B burn-down by
  bound refinement; `--inner-unroll` and packing tuning are on-clock
  work by design.

## Subcycle shape (to be filled when reached)

| # | Topic | Gated on |
|---|---|---|
| 1.6.0 | **The confirmed input path** — whatever C-19 settled (the C-emitter proven end-to-end on a small program, or the direct path if AbsInt surprises) | C-19 |
| 1.6.1 | **Entry points, stubs, concurrency mapping, the generated data dictionary** — the analysis harness per the question list | C-19 |
| 1.6.2 | **The dry run** — a full pass on a representative program *before* the 30-day clock | 1.6.0, 1.6.1 |

## Watch for

- **The clock starts once, and does not stop.** Every setup question
  answered on the 30-day clock is a day not spent on the analysis the
  clock is for.
- **The switch (`meta/SWITCH.md`) waits on 1.6** and inherits the three
  audit corrections (stale ship-list, unowned prototype-coverage pass,
  `meta/specs/` completeness) — swept during 1.4/1.5 while both doc sets
  are live.
- **Nothing about the switch happens until this cycle is finished.**

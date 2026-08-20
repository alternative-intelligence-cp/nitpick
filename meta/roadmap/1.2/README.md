# Cycle 1.2 — Self-hosting

**Phase C, the milestone that matters.** Everything before 1.2 is validated against
the seed's output; after it, the compiler validates itself. This is where the seed
is retired, the fixpoint is re-closed after 0.9–1.1's additions, builds become
byte-reproducible across environments, and `npkg` — the permanent build/test/verify
runner — replaces the throwaway Python harness.

> Detailed **map**. Its subcycles are written when reached. The audit found this
> cycle's *acceptance criteria* are the part most in need of correction before it
> starts — four of its gating items are about measuring the right thing.

## The state this cycle starts from (the audit's finding)

The stage-1/stage-2 fixpoint **already runs** as a harness stage and passes today
(closed early, 0.8.1). So 1.2 is not inventing the fixpoint — it is **re-closing it
after 0.9–1.1 change the compiler**, retiring the seed as the builder, and making the
reproducibility claims true beyond one machine. But the audit found the *acceptance
criterion as written is unsatisfiable* and the *committed seed it assumes does not
exist* — so the cycle must first correct what it is measuring.

## Decisions in (see `../OPEN_DECISIONS.md` §3)

- **C-10 — correct the fixpoint criterion.** BUILD_REFERENCE/D-085 say "stage 1 and
  stage 2 byte-identical" — unsatisfiable (two independent emitters). The real check
  is "stage-N's emission of the compiler equals stage-N+1's." An implementer
  following the spec literally concludes self-hosting is broken. *Fix the spec first.*
- **C-11 — commit the seed IR and fix the deletion plan.** `bootstrap/seed/` is empty
  though four docs claim it holds committed IR; `LAYOUT.md` would delete the only
  rebuild-from-LLVM path and `npkrt.o` (linked into stage 1). Also schedule D-015's
  npkrt-replacement, currently unscheduled.
- **C-12 — define byte-reproducibility cross-environment** (pin the toolchain as an
  input; build-twice-from-different-cwd; make the seed path-independent).
- **C-13 — the seed-retirement schedule** (the constraint 0.9–1.1 have been obeying:
  `src/` may not use a construct the current builder can't compile; name the switch
  point — which is *here*).
- **B-4 — schedule `npkg`** — the permanent runner; without it, "the day self-hosting
  closes is the day the project has no test runner."

## Subcycle shape (to be filled when reached)

| # | Topic | Gated on |
|---|---|---|
| 1.2.0 | **Correct the criterion & commit the seed** — C-10 spec fix; C-11 seed commit (path-independent) + LAYOUT amendment | C-10, C-11 |
| 1.2.1 | **The builder switch** — `src/` adopts the 0.9–1.1 features it deferred; the builder moves from regenerated seed to committed stage IR (C-13) | C-13 |
| 1.2.2 | **Re-close the fixpoint** — stage-1/stage-2 emission equality after the source adopts generics/async; the concrete collections become generic | C-1 (1.0), all rungs |
| 1.2.3 | **Byte-reproducibility** — toolchain pinned in the lock/manifest; build-twice-cross-cwd check; a pinned hash (C-12) | C-12 |
| 1.2.4 | **`npkg`** — a minimal build/test/verify runner; the D-011 undefined-symbol scan written into BUILD_REFERENCE §4 as a permanent step; the Python harness's succession | B-4 |

## Watch for

- **The harness-to-`npkg` handoff is the risky moment.** `LAYOUT.md` deletes the
  Python harness at 1.2; `npkg` must exist and run every whole-suite check the harness
  did (the five instruments, the real-parser sweep, the fixpoint, the zero-dependency
  scan) *before* the harness goes. Sequence 1.2.4 so `npkg` is proven against the
  harness's results before the harness is retired — never a gap where neither runs.
- **`npkg` needs a process-spawn primitive** the language does not have (it must
  invoke `llc`/`ld.lld`, and 1.3 will need it for z3). This is the same primitive C-17
  needs; build it once, here, and 1.3 inherits it. Today Python spawns the tools;
  `npkg` in Nitpick cannot until the floor grows a spawn builtin.
- **Reproducibility is a deliverable, not a hope** — C-12's build-twice check is what
  turns D-078 from a claim into a tested property. Without it, "byte-reproducible"
  means "byte-identical in one process on one machine," which is not the guarantee.

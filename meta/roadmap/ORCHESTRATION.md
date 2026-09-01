# Orchestration — one agent directing many, after the core language

> **RATIFIED as D-228** (user decision, 2026-09-01). R1–R9 and §4's
> cumulative-prefix protocol are NORMATIVE; this file stays as their rationale,
> the split D-218 and `1.5/README.md` already use. §8's four questions are
> answered in the decision: Fable orchestrates and Opus executes, calibration
> is sequenced behind OWED-1 rather than run before it, and R6 is absolute over
> the shipped artifact and its gates. Written 2026-08-30 during the 1.4.7
> family-10 stand-down; everything here carries the measurement or the incident
> behind it. Nothing here authorises spawning an agent — that is the user's
> call, per task, as always.
>
> **D-233 note (2026-09-01)**: where this file says "the Astrée
> preparation", read "the 1.6 analyzer evidence" — Astrée left the plan the
> same day this one was ratified. R9's stated basis ("the Astrée trial is a
> single non-renewable 30 days") is restated by D-233 as proof-invalidation
> scarcity; R9's rule is unchanged either way — obligations discovered in a
> branch and never collected are still the cheapest way to lose the
> campaign.

## 1. What this is, and what it is not

This is **not** parallelising the harness. That is a throughput fix, it was
done during 1.4.7 for its own reasons, and it earned its keep immediately —
running two harnesses concurrently is what separated a green family 9 from a
trapping 9+10 in one wall-clock window instead of two.

This is **one agent orchestrating many** on the work that follows the core
language: the standard library, further libraries, test authorship, the K
framework grammar, documentation, and the Astrée preparation. Dependency
chains still exist there. They are not the chains that bind work on the
compiler itself, and §2 is why.

## 2. Why core-language work is the serial case

It is not mainly the dependency graph. It is that **the compiler is one
artifact with a whole-tree invariant.** Every subcycle ends with npkc
rebuilding itself byte-identically, and that fixpoint is a property of the
entire tree, not of any diff. Two agents editing `src/` concurrently cannot
each validate it, because neither is testing the tree that will actually
exist. The twelve collection families of 1.4.7 barely touch each other and
still had to land one at a time, for exactly this reason.

After the core is done the shape changes. A standard-library module is
*compiled by* the compiler rather than being part of it, so an interface
appears between them. The work becomes a fan-out from a frozen root, and a
fan-out from a frozen root is the shape that parallelises.

Hence the governing rule:

> **Parallelism is licensed by a frozen interface, never by a schedule.**

An agent may work in parallel only against an interface that is written down,
committed, and under a no-change freeze for the duration of the window. A task
that needs the interface changed is an escalation, and it serialises.

## 3. The rules

**R1 — One worktree and one branch per agent. Never a shared index.**
A git worktree has one index and one HEAD. On 2026-08-30 two sessions shared
this tree; one ran an add/commit for its own documentation change and swept up
the other's in-flight work, producing `fc609a6` — a commit whose message
described an unfixed use-after-free while its diff contained the applied fix.
Nothing was lost, but custody was wrong and the record contradicted itself.
Costs one command; removes the class.

**R2 — `src/` has exactly one writer at a time. Permanently, including
post-core.** It is the root of the fan-out: one change there invalidates every
parallel branch simultaneously.

**R3 — Independently green is not green. The gate is a full harness on the
MERGED set.** Family 9 was green. Family 10 was green. Together they trapped
`npkc` compiling itself. That is precisely the artifact N parallel branches
manufacture, and this project has already paid for the lesson once. The
orchestrator's job is not collecting green branches; it is merging them and
re-running. This is `--only` iterates, never concludes, one level up.

**R4 — Concurrency is bounded by measured memory, not by agent count.**
A harness run costs ~1.3 cores and ~9.5 GB, so on this 48-core / 157 GB
machine memory bounds concurrency to roughly a dozen runs. More agents than
that do not run in parallel; they queue.

**R5 — A red under parallel load is a stop sign, never a retry.**
Every timing-shaped defect this project has found looked like flakiness first:
`npk_exit` calling `exit` rather than `exit_group` so a threaded program's
status was whichever thread finished last; a channel wake landing between
registering and sleeping; a five-second futex sleep on every thread join,
latent since 1.1.9 and hidden because the answer it eventually returned was
correct. A retry-on-red policy would have concealed all three. Red under load
→ serialise and reproduce.

**R6 — A parallel agent that finds a compiler defect must not work around
it.** It will happen constantly: the text layer (1.1.12c) produced three
compiler fixes, the library tier (1.3.7) three more. The path is: record the
repro, stop, escalate. The orchestrator freezes the window, one writer fixes
`src/`, full harness, and every branch rebases onto the new HEAD and re-runs
its own gate. This is expensive on purpose — a workaround buried in library
code outlives the bug, is never removed, and is indefensible at verification
time.

**R7 — Assign FILES, not tasks.** Two agents told "write tests for X" and
"write tests for Y" can both edit one suite file. Ownership by path makes the
conflict impossible by construction instead of by discipline.

**R8 — The orchestrator owns the composed execution record, and does not
write code.** This project's execution records are load-bearing: the tally of
six structurally different ways a text search misses a site, and the running
"text search 0, compiler 6" score, exist only because one writer kept them.
N agents each keeping their own record loses every cross-agent pattern. An
orchestrator that also writes code cannot hold the merge and gate roles
cleanly, so it holds freeze management, assignment, integration, record
composition and escalation routing — and nothing else.

**R9 — Every branch records its own verification obligations, and the
orchestrator merges them into 1.5's list at integration.** Parallel authorship
of library code is parallel authorship of proof obligations. The Astrée trial
is a single non-renewable 30 days; obligations discovered in a branch and
never collected are the cheapest possible way to lose it.

## 4. The integration protocol — parallel bisect by cumulative prefix

The scheme 1.4.7 used for two branches generalises, and it is the reason the
gate does not cost N × 50 minutes.

Order the N green branches. Build the cumulative prefixes — branch 1; branches
1+2; 1+2+3; … — and run **one harness per prefix, concurrently**, up to the
R4 memory bound. Each run validates exactly a tree state that will exist, so
no rigor is given up; verify each prefix with `diff` before trusting it.

- All green → the whole set lands, in that order.
- Red first appears at prefix *k* → branch *k* is the culprit, given
  prefix *k−1* is green. The bisect has already happened, in one wall-clock
  window instead of N.

Beyond the memory bound, chunk it: gate a dozen, land them, gate the next.

**Calibration owed before the first wide window.** At n=2 no spurious deadline
failures appeared. n=12 is unmeasured, and under R5 a flake and a real
concurrency defect are indistinguishable — which for this language is the
worst possible ambiguity. So: run a known-green tree at the intended width
first and require all green. The tests whose failure mode is a deadline are
`channel_deadline`, `driver_deadline`, `executor_sleep`, and anything that
joins a thread.

## 5. What splits, and what does not

**Parallelisable** — leaf work against a frozen npkc, disjoint by path:

- individual `lib/*.npk` modules and their tests;
- conformance and rejection test authorship;
- the K framework grammar and semantics;
- documentation and spec sync (`check_decisions_current` reports a backlog by
  design);
- the 1.6 Astrée preparation questions and research digests.

**Must stay serial** — this is the more useful half:

- anything in `src/`, by R2;
- **anything declaring a new `error:`.** Family 6's `source.BoundsIndex` grew
  38 `failsafe` arms across the tree, because REACH-002 is exhaustive over what
  can reach `failsafe`. Two of those in parallel conflict everywhere;
- **anything changing a prelude type's representation.** The whole 1.4.7
  stop-the-line was a representation change, and its blast radius was a
  neighbouring allocation;
- anything touching the builtin signature table, which is deliberately one
  authority (D-201) and generated from.

## 6. Mechanism

**Separate sessions with their own worktrees** for anything that runs a
harness: long-lived, own tool budget, survives independently, and proven here.
**Subagents inside one session** only for read-only fan-out — searching,
auditing, reading the research corpus. A subagent cannot sensibly own a
50-minute gate.

Cross-session messages are the coordination channel and they work; the
2026-08-30 exchange is the worked example. A peer cannot grant permission,
cannot approve another session's pending prompt, and cannot authorise an
action a session's own settings refuse.

## 7. When this starts

The natural point is the **1.4 close or 1.5's open**. 1.5 is itself partly
parallelisable — the obligation catalogue, the encodings, and the solver
determinism profile are largely disjoint — and it is the first cycle where
this would pay. Nothing here applies to the remainder of 1.4: families 11, 12
and the parallel-array ten are `src/` work and stay serial by R2.

## 8. What needs ratifying

The rules above are recommendations with evidence. These are decisions:

1. ~~Does this become a decision?~~ **ANSWERED — D-228.** R1–R9 and §4 are
   normative; this file is their rationale. The reason stands as written: R2,
   R5 and R6 constrain future sessions' behaviour, and a document nobody is
   bound by is the next stale document.
2. **Who plays orchestrator — a Fable session, an Opus session, or the
   user?** R8 makes it a real role with real judgement (freeze calls, red
   triage, record composition) and no code output. Recommendation: Fable,
   since the role is judgement-dense and output-light, which is the shape the
   budget already favours.
3. **The width to calibrate at** (§4). Recommendation: 6 first, then 12 only
   if 6 is clean, since the deadline-flake risk is the one unmeasured hazard.
4. **Whether R6's "never work around a compiler defect" is absolute.** It is
   written as absolute. If there is a case for a recorded, time-boxed
   exception the user wants available, it has to be named now — under the
   standing rule, not later.

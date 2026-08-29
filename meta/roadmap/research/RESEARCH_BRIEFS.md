# Research briefs — deep-research prompts, paste-ready

Owner: the user (Gemini deep research). Opened at 1.4.0, when the §6c
review began. Each brief names the decision it feeds and carries a
self-contained prompt — paste the block verbatim; nothing in it assumes
access to this repository. **Suggested priority order is the section
order.** Finished reports go in `meta/research/` (one file per report,
named after the brief) so findings are auditable when the coverage
decisions cite them.

A standing note for reading the results: these reports INFORM decisions,
they do not make them. Anything a report motivates must still land before
the Astrée trial (the D-183-era rule), and mid-build scope growth stays
gated on the §6c "within reason" judgement.

---

## R-1 — Bug and vulnerability class statistics (feeds §6c, the coverage audit)

The §6c review's foundation: what actually goes wrong, ranked by
real-world frequency AND real-world exploitation, so Nitpick's shipped
checks can be audited class-by-class against reality rather than
intuition.

**Prompt:**

> I am designing the safety audit for a new memory-safe systems
> programming language intended for safety-critical software (robotics,
> embedded, long-running services). I need a comprehensive, data-driven
> ranking of software defect and vulnerability classes as they occur in
> the real world, current through 2026. Please compile:
>
> 1. The MITRE CWE Top 25 lists for 2021 through the most recent edition,
>    as a single table showing each CWE's rank trajectory across years,
>    with a plain-language description of the defect class and a concrete
>    example of each.
> 2. CISA Known Exploited Vulnerabilities (KEV) catalog analyses: which
>    CWE classes are most frequently EXPLOITED in the wild (as opposed to
>    most frequently reported), and how the exploited-class ranking
>    differs from the reported-class ranking.
> 3. The published memory-safety statistics from major vendors: Microsoft
>    (~70% of CVEs memory safety), Google Chrome and Android (including
>    the Android team's published data on how moving new code to
>    memory-safe languages changed vulnerability rates over time), Mozilla,
>    and any comparable public datasets. Include the actual percentages,
>    what population each measures, and the trend over time.
> 4. Root-cause taxonomies for SYSTEMS software specifically (C/C++
>    codebases: kernels, browsers, embedded firmware): what fraction of
>    serious defects are memory safety vs. integer/arithmetic errors vs.
>    concurrency (races, deadlocks, TOCTOU) vs. injection/parsing vs.
>    logic errors vs. configuration/deployment. Cite studies (academic or
>    industry) with their methodology and population.
> 5. Crash/reliability data as distinct from security data: what defect
>    classes dominate field CRASHES in deployed software (telemetry
>    studies, e.g. from OS vendors), since uncontrolled crashes matter as
>    much as exploits for my use case.
>
> Present a final synthesized ranking: the top ~25 defect classes for
> systems software, each with (a) reported frequency, (b) exploitation
> frequency, (c) whether it is primarily a language-level, library-level,
> or design-level defect, and (d) which language mechanisms are known to
> eliminate or mitigate it. Cite every data source with enough precision
> that I can verify the numbers.

## R-2 — What still goes wrong in memory-safe languages (feeds §6c)

The audit's sharpest question is not "does Nitpick match C's failure
modes" but "what leaks through a memory-safe language anyway" — that
residue is exactly where Nitpick's additional checks (taint, forced error
handling, exhaustiveness, contracts, controlled shutdown) must be
auditable.

**Prompt:**

> I am auditing the defect coverage of a new memory-safe systems language.
> Memory safety is the floor, not the goal — I need to know what STILL
> goes wrong in languages that already have it, current through 2026.
> Please compile:
>
> 1. Analyses of the RustSec advisory database and Rust CVEs: taxonomies
>    of vulnerability classes in the Rust ecosystem (e.g. studies like
>    "how many Rust CVEs require unsafe code"), the split between
>    unsafe-block defects, logic errors, panics/denial-of-service,
>    integer overflow, injection, and dependency/supply-chain issues.
> 2. The same for Go (nil dereferences, data races despite the race
>    detector, goroutine leaks) and for Java/C# where instructive
>    (deserialization, injection) — briefly, as contrast cases.
> 3. Studies on PANIC/abort behavior as a failure class in Rust services
>    (unwrap culture, denial of service via reachable panics) — since my
>    language routes all failures through mandatory Result handling and a
>    controlled-shutdown path, I need to know how often "the program
>    stopped uncontrolled" is the actual field failure in Rust systems.
> 4. Concurrency defects that type systems do NOT catch: lost wakeups,
>    deadlocks, priority inversion, async executor bugs — with real
>    incident examples from Rust async (Tokio) or similar runtimes.
> 5. Logic/specification errors: what fraction of serious defects in
>    memory-safe codebases are "the code did what it said, the spec was
>    wrong" — and what techniques (contracts, property testing, formal
>    verification) measurably reduce them.
> 6. Supply-chain and build-system compromise as a class (xz-utils style):
>    prevalence and the mitigations that worked.
>
> Synthesize: a ranked list of the defect classes that REMAIN once memory
> safety is guaranteed, each with frequency evidence, and for each, which
> additional language-level mechanism (if any) is known to close it —
> e.g. exhaustive matching, checked arithmetic, taint tracking, forced
> error handling, effect systems, contracts, deterministic
> shutdown. Cite sources precisely.

## R-3 — Astrée: input formats, preparation, and the trial (feeds C-19; the 1.6 one-shot)

The single non-renewable 30-day trial must not start with a discovery.
Everything publicly knowable about what Astrée accepts and how projects
prepare for it, before any AbsInt contact.

**Prompt:**

> I am preparing a codebase for analysis with AbsInt's Astrée static
> analyzer, and I get one shot: a single 30-day trial. I need everything
> publicly known, current through 2026, about preparing for Astrée.
> Please compile:
>
> 1. INPUT: exactly what Astrée accepts. Which C standards (C99? C11?),
>    which subset restrictions (recursion, dynamic allocation, function
>    pointers, unions, variadic functions, setjmp, threads), whether
>    C++ is supported (Astrée for C++ — maturity, restrictions), and
>    whether there is ANY path for other inputs (LLVM IR, assembly,
>    binaries) — I believe the answer is C source only, but I need that
>    confirmed with sources.
> 2. The directive/annotation system: __ASTREE_ directives (assert,
>    known_fact, volatile ranges, modify, etc.) — what exists, what each
>    is for, and how real projects use them to model the environment.
> 3. Entry points and environment modeling: how an analysis run is
>    scoped (single entry? task model?), how interrupt handlers and RTOS
>    tasks are declared, how Astrée's OS-awareness works (OSEK/ARINC
>    integrations), and what a project does about code the analyzer
>    should stub (drivers, syscalls, hand-written assembly).
> 4. GENERATED code: Astrée is routinely run on model-generated C
>    (SCADE, TargetLink, Embedded Coder). What is known about how code
>    generators shape their C output for analyzability — naming,
>    control-flow restrictions, annotations emitted alongside — since my
>    input would also be machine-generated C.
> 5. Precision and alarm management: published case studies (Airbus
>    A340/A380, automotive) with numbers — code size, analysis time,
>    alarm counts, how zero-false-alarm was reached, how long preparation
>    took, team size. What preparation work dominated the schedule.
> 6. The commercial mechanics: what an evaluation/trial typically
>    includes, license model, whether AbsInt provides preparation
>    support during trials, and anything published about typical
>    evaluation timelines.
>
> Deliver as a preparation handbook: what to have ready BEFORE day 1 of
> a 30-day trial, the known pitfalls that consume trial time, and a
> checklist mapping each Astrée requirement to a preparation task. Cite
> AbsInt documentation, papers by Cousot/Kästner/Ferdinand et al., and
> user reports precisely.

## R-4 — LLVM toolchain output determinism (feeds D-204; lands at 1.4.5)

Reproducibility is now a tested property; the test is only as good as our
knowledge of where `opt`/`llc`/`lld` can be nondeterministic.

**Prompt:**

> I need a precise, sourced account of output determinism in the LLVM
> toolchain, current through 2026, for a reproducible-builds pipeline
> that compiles LLVM IR text with `llc` and links with `ld.lld` (no
> clang involved — the IR is generated by my own compiler).
>
> 1. Given IDENTICAL .ll input, a fixed LLVM version, and fixed flags:
>    is `llc` output (object file) guaranteed byte-identical across
>    runs, across machines of the same target triple, and across host
>    operating systems? What known sources of nondeterminism exist
>    (hash-map iteration order history, host-CPU-dependent decisions,
>    parallelism, ASLR-dependent pointer ordering bugs), and which have
>    been fixed vs. still open? Same questions for `opt -O2`.
> 2. `ld.lld` determinism: is output byte-identical for identical
>    inputs? Effects of `--threads`, archive ordering, build-id
>    computation, and any timestamp/path embedding in ELF output.
> 3. What the reproducible-builds community (Debian reproducible-builds
>    project, NixOS) has documented about LLVM-toolchain
>    nondeterminism: actual bugs found, workarounds/flags adopted
>    (-frandom-seed equivalents at the IR level, path remapping,
>    ZERO_AR_DATE analogues), and current status.
> 4. Version sensitivity: how much does output change across LLVM patch
>    releases (20.1.x)? Is pinning to a minor version sufficient for
>    byte-identical output, or must the exact patch release be pinned?
> 5. Anything embedded in llc-produced ELF objects beyond the code:
>    metadata sections, producer strings, build attributes — what is in
>    them and which flags control them.
>
> Deliver: a checklist of every known determinism hazard for an
> llc+lld pipeline, each with status (fixed in which version / open /
> needs a flag), and the minimal flag set for byte-reproducible output.
> Cite LLVM bug tracker entries, commits, and reproducible-builds
> documentation precisely.

## R-5 — SMT encoding architecture for program verifiers (feeds C-17; 1.5 opens on it)

1.5's biggest unknown: the obligation catalogue and the encoding choices
that make Z3 tractable AND deterministic — determinism being a
correctness property here (D-040: a timeout-dependent binary is the
hazard).

**Prompt:**

> I am designing the SMT backend of a program verifier: a compiler emits
> proof obligations over SMT-LIB2 to Z3 for a systems language with
> fixed-width integers, IEEE floats, fixed-point types, structs, slices
> with bounds, and a Result-based error model. Compile the state of the
> art, current through 2026:
>
> 1. INTEGER encoding: bitvector theory vs. unbounded integers with
>    range axioms — how CBMC, ESBMC, Kani, Dafny, Why3/Frama-C-WP, Verus
>    and Creusot each chose, the performance and completeness tradeoffs,
>    and specifically how overflow-check obligations are encoded
>    efficiently.
> 2. FLOAT encoding: the SMT FP theory's real-world tractability in Z3
>    (and cvc5) — what verifiers actually do (FP theory vs. reals
>    abstraction vs. interval axioms), with benchmark evidence.
> 3. MEMORY: how verifiers encode structs/arrays/slices (theory of
>    arrays vs. field-as-function vs. separation-logic-derived
>    encodings), and how bounds-check obligations are discharged.
> 4. The OBLIGATION CATALOGUE: what classes of checks mature verifiers
>    emit (overflow, div-by-zero, bounds, exhaustiveness, contract
>    pre/post, termination/variants, aliasing/disjointness), how
>    obligations are named/tracked for stable reporting across builds,
>    and how verified-check ELISION is recorded so an optimized build
>    can prove it removed only proven checks.
> 5. COUNTEREXAMPLE mapping: how model values get mapped back to source
>    spans and variable names — the symbol-naming contracts verifiers
>    use between the obligation emitter and the model parser.
> 6. DETERMINISM: Z3's reproducibility controls — random seeds,
>    resource limits (rlimit) vs. wall-clock timeouts, version pinning —
>    and what verification tools do so that "proved" vs "timeout" is a
>    deterministic function of the input across machines. This matters
>    to me more than raw speed: a proof result that varies by machine
>    poisons a reproducible build.
> 7. Incrementality and obligation batching: push/pop vs. independent
>    queries, per-function vs. per-obligation solver instances, and what
>    scales.
>
> Deliver: an architecture recommendation matrix (encoding choice per
> type family × verifier precedent × evidence), the obligation-catalogue
> checklist, and the determinism configuration. Cite tool documentation,
> papers, and benchmark studies precisely.

## R-6 — Async-runtime defect classes and their verification (feeds §6c and 1.5's executor modeling)

We built an executor, reactor, channels, and actors in 1.1 and found two
lost-wakeup-class defects ourselves (both needed ~20+ stress runs to
reproduce). The question: what is the KNOWN defect taxonomy for this kind
of runtime, and what do mature projects do beyond stress loops.

**Prompt:**

> I built a cooperative async runtime for a systems language: per-thread
> executors, epoll-based reactor, eventfd cross-thread wakes, futex-based
> parking, bounded channels with generation-tagged slots, structured
> concurrency with scope-exit joins. I need the field's accumulated
> knowledge about what goes wrong in such runtimes and how it is caught,
> current through 2026:
>
> 1. A defect taxonomy for async executors/reactors with real incident
>    examples: lost wakeups, spurious wakeup mishandling, wake-before-
>    sleep races, poll-after-completion, waker contract violations
>    (Tokio/async-std/smol bug histories are good sources), starvation
>    and unfair scheduling, shutdown/cancellation races, timer-wheel vs
>    deadline bugs.
> 2. Systematic-testing tools and what they catch: loom (Rust),
>    shuttle, Coyote (formerly P#), rr chaos mode, antithesis-style
>    deterministic hypervisors — how each explores schedules
>    (exhaustive with bounds? probabilistic?), their real-world catch
>    records, and the effort to adopt them.
> 3. Model-checking precedents: TLA+/PlusCal specs of executors,
>    channels, or work-stealing schedulers that found real bugs
>    (published cases), and how much of a runtime is practically
>    modelable.
> 4. The specific literature on LOST WAKEUPS: the standard protocol
>    patterns that make register-then-sleep safe (the "prepare to park /
>    recheck / park" discipline), formalizations of futex protocols, and
>    known-correct reference implementations.
> 5. What fraction of concurrency defects escape stress testing:
>    any published data comparing stress-loop detection rates vs.
>    schedule-exploration detection rates for the same bug corpus.
>
> Deliver: the taxonomy with incident citations, and an assessment of
> which detection techniques would be worth building into a test harness
> that currently uses N-times stress repetition — including what a
> minimal deterministic-schedule-exploration harness for a HAND-WRITTEN
> executor (not a library) entails.

## R-7 — Software failure in deployed safety-critical and robotic systems (feeds §6c framing)

Grounds the audit in the deployment domain: a companion robot around
children. What actually causes harm or recall in fielded systems.

**Prompt:**

> I need a sourced survey of what actually goes wrong in SOFTWARE for
> deployed safety-critical and robotic systems, current through 2026 —
> not vulnerability counts, but field failures, recalls, and incidents:
>
> 1. FDA medical-device recall data: what fraction of recalls are
>    software-caused, the recurring root-cause categories, and any
>    class-level analyses (infusion pumps, surgical robots).
> 2. Automotive: published analyses of software-related recalls and
>    field incidents (NHTSA data), the defect classes ISO 26262 field
>    experience reports name, and unintended-acceleration-class case
>    studies with their root-cause findings (Toyota 2013 trial expert
>    analysis included).
> 3. Robotics specifically: incident taxonomies for industrial and
>    service robots (OSHA data, academic incident surveys), and any
>    published analyses of consumer/companion robot failures.
> 4. Aerospace lessons that transfer: the classic case studies (Ariane
>    5, Mars climate orbiter, Boeing 737 MAX MCAS) reduced to their
>    software-engineering root causes — specification vs. implementation
>    vs. integration vs. numeric.
> 5. Across all of the above: how often the root cause is (a) memory
>    corruption, (b) numeric/unit/overflow error, (c) unhandled error
>    path or crash-under-fault, (d) concurrency/timing, (e) requirement/
>    logic error, (f) configuration/deployment — a synthesized
>    distribution with the caveats stated.
> 6. What the standards' own data says works: any published evidence on
>    which assurance techniques (MC/DC testing, static analysis, formal
>    methods, runtime monitoring, watchdog/degraded-mode design)
>    correlate with lower field-failure rates.
>
> Deliver: the synthesized root-cause distribution for safety-critical
> software failures with per-domain tables, and the ten most instructive
> incident case studies for a designer of a language whose stated goal
> is that uncontrolled shutdown is impossible. Cite primary sources.

## R-8 (optional, if time remains) — Trusted-base architecture in verified toolchains (feeds 1.5 architecture)

**Prompt:**

> Survey how verification-oriented compilers and systems bound and
> justify their TRUSTED BASE, current through 2026: CompCert (what is
> proved vs. trusted — the printer, the assembler), CakeML (down to
> machine code), seL4's toolchain (the proof chain, translation
> validation of the compiler output), translation validation as an
> alternative to compiler verification (Sewell's seL4 binary
> verification, LLVM translation-validation efforts like Alive2), and
> certifying compilation (proof-carrying artifacts). For each: what is
> in the trusted base, what the verification consumes as input (source,
> IR, binary), how runtime/primitive layers written in assembly or IR
> are handled by the verification story, and the published lessons about
> where trusted-base bugs were actually found. Deliver: a comparison
> table and the transferable lessons for a self-hosted compiler that
> emits LLVM IR, keeps a hand-written-IR runtime floor, and plans
> abstract-interpretation (Astrée) plus SMT (Z3) evidence rather than a
> Coq-style end-to-end proof.

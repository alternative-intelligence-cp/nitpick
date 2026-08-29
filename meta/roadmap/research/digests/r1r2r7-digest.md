# Digest of r1.md + r2.md + r7.md — the coverage-audit raw material (sources: meta/roadmap/research/r1.md, r2.md, r7.md)

> Extraction from "Defect Taxonomies and Vulnerability Trajectories in
> Systems Software" (r1), "Beyond Memory Safety: An Empirical Taxonomy of
> Residual Defect Classes in Modern Systems Languages" (r2), and
> "Systemic Software Failures in Deployed Safety-Critical and Robotic
> Systems" (r7). Bracketed numbers are the reports' citation indices.
> This digest is the input to the §6c coverage audit
> (`COVERAGE_AUDIT.md`), which maps these classes against Nitpick's
> shipped mechanisms.

## r1 — statistics

### CWE Top 25 trajectory 2021→2025 (ranks by year; N/A* = methodology remap)

| CWE | Class | 21 | 22 | 23 | 24 | 25 |
|---|---|---|---|---|---|---|
| 787 | OOB Write | 1 | 1 | 1 | N/A* | 5 |
| 79 | XSS | 2 | 2 | 2 | N/A* | 1 |
| 89 | SQLi | 6 | 3 | 3 | 3 | 2 |
| 416 | Use After Free | 7 | 7 | 4 | N/A* | 7 |
| 78 | OS Command Injection | 5 | 6 | 5 | N/A* | 9 |
| 22 | Path Traversal | 8 | 8 | 8 | N/A* | 6 |
| 20 | Input Validation | 4 | 4 | 6 | 12 | N/A* |
| 94 | Code Injection | 25 | 25 | 23 | 11 | 10 |
| 122 | Heap Overflow | — | — | — | — | 16 |
| 400 | Resource Consumption | 27 | 23 | 27 | 24 | N/A* |
| 476 | NULL Deref | 15 | 11 | 12 | 21 | N/A* |
| 190 | Integer Overflow | 12 | 13 | 14 | 23 | N/A* |

Reporting bias: XSS/SQLi inflated by automatable discovery; races
(CWE-362) and deep UAF underrepresented relative to true prevalence [13].

### KEV exploitation (182 actively exploited CVEs, Jun 2024–Jun 2025)

Top 10 EXPLOITED: **#1 CWE-78, #2 CWE-416, #3 CWE-787, #4 CWE-306
(Missing Authentication), #5 CWE-502 (Deserialization), #6 CWE-22,
#7 CWE-94, #8 CWE-288 (Auth Bypass), #9 CWE-122, #10 CWE-79.**

Inversions: XSS #1 reported → #10 exploited; CWE-78 #9 → #1; CWE-306
#21 reported → #4 exploited; SQLi and CSRF (#2/#3 reported) ABSENT from
the exploited Top 10. Macro pillars: memory safety 30%, command/code
injection 30%, access control 20%, deserialization 10%, path traversal
10%. Claim: memory safety + type-safe subprocess boundaries obsolete
">60% of active zero-day vectors."

### Vendor telemetry

- Microsoft: ~70% of CVEs memory safety (decade of patch telemetry) [16].
- Chromium: ~70% of 912 high/critical bugs since 2015 memory-unsafety;
  **36% of the whole population strictly UAF**; spread evenly [16][19].
- Mozilla: 32/34 (94%) critical/high in core components memory-related [18].
- **75% of actively weaponized zero-day CVEs globally are memory
  safety** (Google Threat Intelligence) [21].
- Mitigation ceilings: MiraclePtr stops 57% of privileged-process UAF at
  continuous cost [20]; MPX up to 50% overhead, zero temporal coverage
  [18]; mobile sandboxing at its memory limit [19].
- **Android new-code transition: memory-safety vulns >220 (2019) → ~36
  projected (2024), −83%, WITHOUT rewriting legacy code** — discovery
  concentrates in new code [21].

### Systems root causes

- Crypto libraries: 48.4% of vulns memory safety; +19.4% side channels
  (constant-time execution named required) [15].
- Syzbot finds a new kernel bug per 0.4 days; maintainers patch in 38–51
  days; **>15% of "low-risk" fuzzer findings escalate to arbitrary
  write/control-flow hijack** (SyzScope) [14].
- **The stated law: a language permitting data races implicitly permits
  UAF** (race→UAF: CVE-2021-0920, CVE-2020-6819) [25][26].

### Crash/reliability side

- Crash-dominant: NULL deref (CWE-476), overflow→undersized-buffer→heap
  chains, resource exhaustion (CWE-400).
- **110 real-world Rust panics: predominantly `unwrap()`/`expect()` and
  OOB indexing — deterministic aborts, availability violations** [25].
  Prescription: total functions, exhaustive handling, static bounds.

### r1's final Top-25 for systems software (web-only classes excluded)

1 UAF (416) · 2 OOB Write (787) · 3 OS Command Injection (78) · 4 Heap
Overflow (122) · 5 Deserialization (502) · 6 Missing Authentication
(306) · 7 Path Traversal (22) · 8 Code Injection (94) · 9 Auth Bypass
(288) · 10 NULL Deref (476) · 11 Integer Overflow (190) · 12 Input
Validation (20) · 13 Race Condition (362) · 14 OOB Read (125) · 15
Resource Consumption (400) · 16 Buffer Restriction (119) · 17 Double
Free (415) · 18 Privilege Management (269) · 19 Hard-coded Credentials
(798) · 20 Default Permissions (276) · 21 Signal Handler Race (364) ·
22 Type Confusion (843) · 23 Divide By Zero (369) · 24 Expired Pointer
Deref (825) · 25 Improper Synchronization (662).
(Per-class eliminate/mitigate mechanisms are in the source table, r1
§6.1 — ownership/borrows, fat pointers, argument-array subprocess APIs,
no-NULL Option types, panic-on-overflow, mutex-wraps-data, etc.)

Four architectural invariants (r1 §6.2): aliasing constraints are
prerequisite to memory safety; "panics are better than corruptions, but
verification is best" (crash-as-DoS still unacceptable for robotics);
type-safe APIs over sanitization; absence of NULL.

## r2 — what survives memory safety (each with its closing mechanism)

1. **Safe/unsafe boundary breaches** — 63.6% of 433 Rust-ecosystem vulns
   still memory+concurrency [7]; ALL 70 studied memory-safety issues
   involved unsafe code but the defect was usually the SAFE caller
   violating invariants — fixes touch 3.85 safe functions vs 0.16 unsafe
   (96%/4%) [2][7]; 17/21 buffer overflows from safe-side size math.
   → *formal verification of the boundary (pre/postconditions checked
   across it).*
2. **Panic-driven DoS** — the 110-panic study: almost all in safe code,
   unwrap culture; reachable panics get DoS CVEs; Rust's debug-panic /
   release-wrap overflow bifurcation is "a source of profound defect
   generation" [2][10][11].
   → *forced error handling + checked arithmetic, no implicit aborts.*
3. **Async cancellation & task deadlocks** — drop-cancellation abandons
   partial effects; `select!` drops losers mid-operation; sync lock held
   across `.await` → "futurelock"; Kotlin: 55 coroutine bugs, bridging/
   nested-blocking → pool exhaustion [22][23][24][29].
   → *linear types + effect systems (run-to-completion or deterministic
   teardown; reject sync locks across yields).*
4. **Message-passing leaks** — Go, 171 bugs: **~58% of blocking bugs
   caused by CHANNEL misuse** ("message passing is safer" falsified);
   goroutine leaks evade the deadlock detector (fires only on whole-
   program block). Rust: 55/59 blocking bugs from Mutex/Condvar misuse,
   ALL in safe code; the double-lock-across-`match` guard-lifetime trap
   [19][2][9].
   → *structured concurrency + deterministic shutdown.*
5. **Build-time supply chain** — build.rs runs arbitrary code with full
   privileges at compile time; auditing fatigues (cargo-vet); real
   compromise: arrayref 0.3.10 [44][46][47].
   → *sandboxed deterministic build execution.*
6. **Semantic/logic errors** — Rust-for-Linux vs 240 driver vulns:
   **34.17% auto-eliminated / 47.08% only with disciplined idioms /
   18.75% untouched** [32]; SPARK-style contracts named "the only proven
   mechanism" for the residue.
   → *first-class DbC + property testing + taint tracking.*

## r7 — field failures in the deployment domain

### Medical/FDA
- 2024: 1,059 recall events (+8.6%), 440.4M units (+55.4%), Class I at a
  15-year high; **pure software defects +31% YoY → 3rd leading cause
  (8.2%)**; PMA-supplement (iterative-update) approvals carry **+30%
  recall risk** [1][5].
- Infusion pumps: 89% of Class-I direct causes device-driven; software
  26% of those; **under-delivery 55% vs over-delivery 27%** [6].
- Therac-25: the 8-second race (UI vs hardware config task) AND the
  uint8 counter 255→0 bypassing the interlock [7–10].
- da Vinci (10,624 MAUDE reports): 144 deaths, 1,391 injuries; **3.1% of
  adverse events = mid-operation manual reboots, 7.3% conversion to open
  surgery** — unrecoverable fault states with instruments inside
  patients [11][12].
- **UI software = ~half of all software recalls 2012–2015 (423)**:
  missing bounds on energy inputs, unit conversions, silent overrides [15].

### Automotive
- Toyota UA (Barr analysis): >11,000 globals; 81,514 MISRA violations;
  monolithic "Task X" (throttle + its own failsafe in one failure
  domain); stack claimed 41% / measured 94% + forbidden recursion →
  silent overflow into RTOS TCBs; single-bit corruption kills Task X;
  **watchdog fed by a timer-tick ISR stayed happy while the throttle
  task was dead**; no mirroring of the throttle target [20–26].
- Cruise robotaxi 2023: braked in 460 ms (fast path worked), then the
  collision-detection subsystem **misclassified frontal run-over as side
  impact** → the don't-block-traffic pull-over rule OVERRODE the
  emergency stop → pedestrian dragged 20 feet; 950-vehicle recall,
  $1.5M fine [30–32].

### Robotics (OSHA 2015–2022)
77 severe accidents: 54 stationary (upper-extremity amputations), 23
mobile (lower-extremity fractures); prevailing root cause: **collision
avoidance failing to model individual human extremities entering the
kinematic envelope** [35].

### Aerospace (the classics, reduced)
- Ariane 5: 64-bit float → 16-bit int cast; handler INTENTIONALLY
  disabled to save cycles; identical backup = zero redundancy against
  systematic software fault.
- Mars Climate Orbiter: no unit-bearing typed interface.
- 737 MAX MCAS: "software flawlessly executed a fundamentally flawed
  specification" — single sensor, unbounded iterative trim.

### Root-cause taxonomy (six categories; NO numeric split given — the
report flags survivorship/reporting bias explicitly)
(a) memory corruption · (b) numeric/unit/overflow · (c) unhandled error
path & crash-under-fault · (d) concurrency/timing · (e) requirement/
logic error (dominates modern autonomous incidents) · (f) configuration/
deployment (dominates medical UI recalls).

### Assurance-technique efficacy
- MC/DC: 95.6–98.8% mutant kill on avionics models — but structurally
  blind to UNWRITTEN requirements (the class-(e) failures) [43][44].
- Astrée: A340/A380 proven RTE-free (see r3 digest for numbers) [45][47].
- SPARK Ada: **~0.04 defects/KLOC vs 1–5 industry** [51][52].
- seL4: full functional correctness at **~20 person-years / 9.3 KLOC** —
  does not scale; the scalable pattern is architectural [55][57].
- **Simplex/Runtime Assurance** (Sha): unverified advanced controller
  wrapped by a verified baseline controller + decision module enforcing
  a declarative safety envelope; "would have prevented the Cruise
  dragging" [61][63].

### The ten case studies → language-mechanism table (r7 §8)
1 Toyota Task X → provable stack bounds, no recursion · 2 Therac-25
overflow → panic-free overflow semantics · 3 Therac-25 race → data-race
freedom · 4 Cruise → exhaustive state-machine matching with proven safe
default · 5 Ariane 5 → casts return Result, forced handling · 6 Toyota
watchdog → liveness tokens from the monitored logic itself · 7 da Vinci
→ graceful degradation, no global panics, deterministic degraded-mode
supervisor · 8 Toyota globals → no mutable global state · 9 FDA UI
bounds → refinement types (`Int[0,100]`) · 10 UI silent override →
linear types for state transitions (must be explicitly consumed).

## The 22 flagged surprises (abbreviated; full list in the source digest)

Reported≠exploited (XSS #1→#10; Missing Auth #21→#4; SQLi/CSRF absent);
message passing NOT safer than locks (58% of Go blocking bugs are
channels); Rust memory bugs CAUSED in safe code (3.85:0.16 fix ratio);
all 59 Rust blocking-concurrency bugs in safe code (guard-lifetime
double-lock); deterministic aborts are their own failure class; Rust's
overflow bifurcation is itself a defect generator; Rust-for-Linux
auto-eliminates only 34% of real driver vulns; >15% of "low-risk"
fuzzer bugs escalate; races ARE memory unsafety (the law); UI software
is a top medical killer; updates are a regression vector in certified
devices (+30%); under-delivery beats over-delivery; robots get rebooted
inside patients; a watchdog can be alive while the system is dead; the
Cruise failure was a correctly executing business rule; the Ariane
handler was disabled on purpose; mitigation ceilings are real
(MiraclePtr 57%); formal proof works but doesn't scale — the scalable
answer is architectural (Simplex); MC/DC can't catch unwritten
requirements; security and stability are the same list; supply-chain
compromise enters at compile time; memory+concurrency is STILL the top
Rust-ecosystem category (63.6%), entering through the boundary.

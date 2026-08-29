# The §6c coverage audit — defect classes vs. Nitpick's shipped mechanisms

> The review OPEN_DECISIONS §6c mandates: the researched defect classes
> (digests: `digests/r1r2r7-digest.md`, `digests/r6-digest.md`) mapped
> against the language as it ships at 1.4.1. **Verdicts**: ELIMINATED
> (cannot be spelled, or refuses at compile time), GUARDED (runtime trap
> through the controlled-shutdown path), OPT-OUT (expressible only
> through a greppable TOS spelling — wild/raw/`=>!`/`sys`), PARTIAL,
> GAP, ARRIVES-1.5 (closed by the verification cycle's planned
> machinery), OUT-OF-SCOPE (design/application level, with the language's
> contribution noted). The proposals at the end (G-1…G-10) are the
> "within reason" additions batch for the user's ratification.
> Two facts below were verified against the code during the audit and
> are marked ⚡; everything else cites shipped decisions.
>
> **RATIFIED WHOLE by the user ("lets go with your recommendations")
> and recorded as D-210…D-214**: G-1 → D-210 (overflow traps, 1.4.2b),
> G-7 → D-211 (const/fixed-only module bindings, 1.4.2b), G-5 → D-212
> (the schedule-exploration harness, 1.5.7), G-2/3/4 → D-213 (the nfs
> riders, 1.4.8), G-8/9/10 → D-214 (the decided-outs). G-6 rides
> D-218's obligation catalogue. The same ratification settled S-4 →
> D-215, S-5 → D-216, and the 1.5 batch → D-217…D-221.

## Part 1 — r1's exploit-weighted Top 25 for systems software

| # | Class (CWE) | Verdict | The mechanism |
|---|---|---|---|
| 1 | Use After Free (416) — #2 exploited | **ELIMINATED** default / OPT-OUT wild | static ownership + move analysis + scope-exit drops (1.2); borrows second-class, never escape up (D-004); overwrite drops old (D-186); channel slots generation-tagged (StaleHandle); allocator validate-before-dereference (D-150); wild is the greppable opt-out with `defer` discipline + quarantine/0xAA instruments |
| 2 | OOB Write (787) — #3 exploited | **ELIMINATED** default / OPT-OUT | length-carrying slices, every index bounds-checked (OutOfBounds trap); pointer arithmetic only via `#ptr_add` in wild context |
| 3 | OS Command Injection (78) — **#1 exploited** | **ELIMINATED BY DESIGN** | the language HAS no shell/string-exec API at all; every spawn is argv-array by construction (`spawn_driver`, D-206's `npk_spawn`) — precisely r1 §6.2's prescription |
| 4 | Heap Overflow (122) | **ELIMINATED** default | as #2; no unbounded-copy functions exist (`mcpy` takes n; string ops length-carrying) |
| 5 | Deserialization (502) | structural core **ELIMINATED** | no reflection, no instantiate-by-name — the gadget-chain mechanism cannot be spelled; Bridge input is [UNTRUSTED]-validated with kill-on-protocol; residual data-validation discipline = class 12 |
| 6 | Missing Authentication (306) — #4 exploited | OUT-OF-SCOPE (design) | language contribution: unforgeable capability-shaped values are natural here (move-only tokens, `OwnedFd` precedent) — Nikola-level design guidance, not a language mechanism |
| 7 | Path Traversal (22) | **GAP → G-2** | `Path`/`path_parse` exist; no canonicalization/containment story yet — lands with D-206's `lib/nfs.npk` |
| 8 | Code Injection (94) | **ELIMINATED** default / OPT-OUT wildx | no eval, no dynamic code — except the JIT surface, which is the acknowledged opt-out under the W^X state machine (D-155: write-seal-call, violations refused) |
| 9 | Auth Bypass (288) | OUT-OF-SCOPE (design) | as #6 |
| 10 | NULL Deref (476) | **ELIMINATED** | no user-facing null; absence is `Optional<T>` with exhaustive `pick`/`?.` — r1 §6.2's "absence of NULL" verbatim |
| 11 | Integer Overflow (190) | **FINDING → G-1** ⚡ | plain `intN + - *` emit bare wrapping `add/sub/mul` (verified in `emit_arith_value`); the CHECKED family is `tbb` (saturate-to-ERR, sticky, D-144). The split is BY TYPE — visible in source, no debug/release variance (r2's bifurcation lesson does NOT apply) — but silent wrap on the DEFAULT int is the Therac-255→0 shape. Decision needed: see G-1 |
| 12 | Input Validation (20) | PARTIAL → **ARRIVES-1.5** | "parse don't validate" is the type system's grain (nominal types, Result-returning constructors, D-148 literal envelopes); refinement (`limit<Rules>` — r7 case 9's `Int[0,100]` exactly) is built surface, checked at 1.5 |
| 13 | Race Condition (362) | **ELIMINATED** default, one hole → G-7 ⚡ | spawn crossings restricted to sanctioned forms (D-180: borrows of sync primitives only); channel elements move-only across; `atomic<T>` for shared scalars; lock ORDER statically checked (D-056 levels — the deadlock half). ⚡ HOLE: a PLAIN (non-const) module global is a mutable LLVM global and NOTHING governs a spawned task writing it — a spellable data race. See G-7. TOCTOU: library item → G-4 |
| 14 | OOB Read (125) | **ELIMINATED** default | as #2 (the Heartbleed class) |
| 15 | Resource Consumption (400) | STRONG PARTIAL → 1.5 | bounded channels (cap in the TYPE), deadlines on every wait (`within`, D-176/DEADLINE_EXCEEDED), fuel on macros/comptime/analyses, arena caps, D-151 leak registry, OOM traps; unbounded-loop termination obligations arrive with 1.5's catalogue |
| 16 | Buffer Restriction (119) | **ELIMINATED** default | the #2/#4 family |
| 17 | Double Free (415) | **ELIMINATED** default | move-only owners, drop flags, loop-carried moves at 1.4.3 (D-208); allocator bitmap + quarantine catch the wild residue |
| 18 | Privilege Management (269) | OUT-OF-SCOPE (design) | note: every spawn sets NO_NEW_PRIVS + PDEATHSIG (D-188/D-206) — the floor is already conservative |
| 19 | Hard-coded Credentials (798) | proposed DECIDED-OUT → G-8 | a lint, not a mechanism; weigh noise vs value |
| 20 | Default Permissions (276) | **GAP (library) → G-3** | file-creation modes default restrictive in nfs/nio |
| 21 | Signal Handler Race (364) | **ELIMINATED BY ABSENCE** | user-facing signal handlers do not exist; the trap route + failsafe is the one asynchronous-failure path, and it runs the driver-registry kill first (D-188) |
| 22 | Type Confusion (843) | **ELIMINATED** default | nominal identity-by-declaration (D-090), checked casts `=>` / acknowledged `=>!`, vtable-typed `dyn` dispatch, no void* |
| 23 | Divide By Zero (369) | **GUARDED** | D-007 guards on every `/ %` — trap −4097/−4098, or ERR in the twisted families; any-lane simd guards (D-194) |
| 24 | Expired Pointer Deref (825) | **ELIMINATED** default | escape analysis + second-class borrows; the suspend walk frames address-taken locals across awaits (the 1.3.8 repair closed the last known instance) |
| 25 | Improper Synchronization (662) | **ELIMINATED** default | the Mutex WRAPS its data (D-056) — access only through acquire; r1's row names this exact mechanism |

KEV pillar arithmetic: memory safety (30%) + injection (30%, via no-shell
+ no-eval) + deserialization gadgets (10%) + traversal (10%, after G-2)
are language-covered — matching r1's ">60% of active zero-day vectors"
claim for a memory-safe + typed-subprocess language, with the remainder
(access control, 20%) design-level.

## Part 2 — r2's residual classes (what survives memory safety elsewhere)

| Residual class | Verdict | The mechanism |
|---|---|---|
| Safe/unsafe boundary breaches (the #1 residual: fixes touch 3.85 safe fns per 0.16 unsafe) | STRONG now, closes at 1.5 | our boundary is LICENSED, not ambient: `raw`/`drop` need a checked `never fails` (D-163); `sys` argument-typed (D-192); builtins fully typed (D-201); `=>!`/wild greppable. The r2 prescription — verify the boundary's pre/postconditions — is exactly 1.5's contracts |
| Panic-driven DoS (unwrap culture; overflow-check ambivalence) | **ELIMINATED as designed** | no unwrap without licence; a trap is not an abort — it routes through the driver-kill then `failsafe` with REACH-checked exhaustive arms (D-179): the "deterministic degraded-mode supervisor" r7 case 7 prescribes IS the design. The overflow half is G-1 |
| Async cancellation & futurelock | **ELIMINATED by construction** | no drop-cancellation exists: structured concurrency, scope-exit joins relaying child errors (D-163 r4), cooperative wind-up token; no select-drops-losers; locks are EXECUTOR-INTEGRATED waiters (the task-identity rule), so a guard held across an await never wedges the thread — the futurelock mechanism has no substrate |
| Message-passing leaks (58% of Go blocking bugs) | **ELIMINATED/GUARDED** | bounded channels with deadline sends (`within` → DEADLINE_EXCEEDED), reclaim at creating scope (D-183, per-scope at 1.4.4/D-207), StaleHandle generations, structural joins — the detector-evading orphan cannot accumulate |
| Build-time supply chain | **ELIMINATED BY DESIGN** | there is nothing to sandbox: no build scripts, no dependencies, no network at build (D-078), comptime is pure and fuel-bounded, the closed-world link (D-206) refuses foreign objects |
| Semantic/logic errors (the 18.75% no type system touches) | best-in-class primitives, closes at 1.5 | exhaustive `pick` (the Cruise class — unhandled semantic states refuse at compile time), `dim256` units (the Mars Orbiter class), casts that fail into `Result` (the Ariane class — r7 case 5 verbatim), move-only tokens for typestate (r7 case 10); contracts/`limit` complete it at 1.5 |

## Part 3 — r7's field-failure classes and the ten cases

Cases 2 (Therac overflow) → G-1. Case 3 (Therac race) → covered + G-7's
global hole. Case 4 (Cruise) → exhaustive pick, shipped. Case 5 (Ariane)
→ `=>`/`=>!` with Result-shaped failure, shipped; note the D-144-as-
amended rule (ERR traps under BOTH spellings) closed the "acknowledged
cast launders taint" corner. Case 7 (da Vinci) → failsafe/controlled
shutdown, shipped. Case 8 (Toyota globals) → G-7. Case 9 (FDA UI bounds)
→ `limit` at 1.5. Case 10 (silent override) → move-only consumption,
shipped. Case 1 (provable stack bounds) → **G-6**: async frames are
already exact-sized by construction (D-153/D-177 — one exact frame per
async fn); the SYNC side has no stack-depth story and recursion is legal
— and Astrée's no-recursion input constraint converges on the same
question (r3 digest). Case 6 (watchdog liveness) → Nikola-level design
guidance; record in the architecture notes, not the language.

The Simplex/Runtime-Assurance pattern (r7 §7 — "would have prevented the
Cruise dragging") is an ARCHITECTURE for Nikola: a verified baseline
controller + envelope monitor around the unverifiable advanced stack.
The language's contribution exists (Bridge supervision, failsafe,
deadline waits); the pattern itself belongs in Nikola's engineering
docs. Recorded here so the audit's trail shows it was seen.

## Part 4 — the r6 verdict: the schedule-exploration instrument (G-5)

The stress-repetition harness (`// stress: 40`) is the tool the data
says finds only shallow races: 95/4/0 (exploration/random/stress) on the
Raft corpus; defects evading 7-day stress runs; PCT's floor
1/(n·k^(d−1)) vs stress's "near 0% for specific interleavings". Both of
our own lost-wakeup finds were shallow enough for N≈20–40 — the class
the data warns about is the one we have NOT caught this way. The
minimal-harness recipe (mock the primitives, centralize the scheduler,
PCT-seed it, virtualize the reactor) maps onto us unusually well:
**npkc owns every primitive the mock layer must intercept** (futex park,
eventfd wake, channel CAS, waker state — all in npkrt.ll/emitted IR),
with no third-party boundary. Reactor virtualization has a half-built
precedent (`mock_driver.npk` fixtures). **Recommendation: build it, as
a 1.5 subcycle** beside the executor-modeling work (model the
primitives — AtomicWaker-class state machines — per the r6 digest's
TLA+ verdict; BPOR-style preemption bounds if any model spins).

## Part 5 — the ratification batch (G-1…G-10)

**G-1 — plain-int overflow semantics (the audit's biggest question).** ⚡
Verified: `intN + - *` wrap silently; `tbb` is the checked family. The
split is principled (semantics in the TYPE, one rule, no mode variance —
blueprint-compliant) but the DEFAULT is the wrapping one, and the
default is what gets written (Therac's counter was nobody's deliberate
choice). Options: **(a)** default ints TRAP on overflow (llvm
`*.with.overflow` + the D-142 trap route; tbb remains the
saturating-ERR family; wild-context or a stated spelling for deliberate
modular arithmetic — hashes, RNG mixers); **(b)** keep wrap, document
the discipline "state that must not wrap uses tbb"; **(c)** keep wrap
now, add 1.5 overflow OBLIGATIONS on plain ints (prove-or-refuse at
verification, zero runtime cost — the r1 §6.2 "verification is best"
row). **Recommendation: (a) now AND (c) at 1.5** — trap semantics
by default (safety > performance, the standing order), with 1.5 proving
traps away where `nsw`-style elision is sound (r5 digest §4). Cost: an
emitter change + REACH arm (IntOverflow) + sweep of any deliberate-wrap
sites in src/ (expected: none — the compiler does no intentional
wrapping outside hash mixing, which is in the prelude's Hash impls and
already spelled on tbb? verify at implementation).

**G-2 — path containment** (r1 #7): `lib/nfs.npk` gets canonicalization
+ open-beneath (openat2/RESOLVE_BENEATH-shaped) as the default opening
API. Rider on D-206's 1.4.8. Recommend: yes.

**G-3 — restrictive file-creation defaults** (r1 #20): 0600-equivalent
unless widened explicitly, in nio/nfs. Rider on 1.4.8. Recommend: yes.

**G-4 — at-style TOCTOU-resistant file APIs** (r1 #13's file half):
dirfd-relative operations in nfs. Rider on 1.4.8. Recommend: yes.

**G-5 — the deterministic schedule-exploration harness** (Part 4).
Recommend: yes, a 1.5 subcycle.

**G-6 — stack-depth/recursion obligations** (r7 case 1 + Astrée's
input constraint): 1.5's obligation catalogue gains a bounded-call-depth
/ recursion-termination row; the C-emission path (if C-19 confirms)
needs the recursion answer anyway. Recommend: yes, folded into 1.5.0's
catalogue decision.

**G-7 — mutable module globals across spawns.** ⚡ Verified: plain
(non-`const`/non-`fixed`) module bindings are mutable LLVM globals;
initializers are compile-time constants (D-165) but nothing restricts
MUTATION, including from spawned tasks — a spellable data race and the
Toyota-globals shape (r7 case 8). Options: **(a)** module bindings
become `const`/`fixed`-only (mutable process state must live in `main`'s
scope and flow explicitly — the r7-case-8 prescription; strongest, may
fight real needs like interned tables… which npkc itself builds at
startup INTO locals, so likely tolerable); **(b)** plain globals remain
but a WRITE outside `main`'s thread refuses (a spawn-visibility
analysis); **(c)** plain globals must be `atomic<T>` to be written after
spawn. **Recommendation: (a)**, with the audit of src/'s own usage as
the implementation's first step (the compiler compiles itself — if npkc
needs no mutable global, the language doesn't either). Needs its own
subcycle slot (1.4.x or 1.5-open).

**G-8 — hard-coded-credential lint**: recommend DECIDED-OUT for the
language (noise-prone, application-domain); revisit as an `npkg`
optional lint post-1.5 if wanted.

**G-9 — constant-time/side-channel** (crypto's 19.4%): no crypto
surface ships in the language today; recommend DECIDED-OUT-FOR-NOW,
recorded as a gating requirement for any future crypto library work
(constant-time discipline would need language support — a real future
decision, not a silent omission).

**G-10 — general taint tracking** (r2's rank-6 rider): D-007's
Result-taint + the Bridge's [UNTRUSTED] boundary exist; a general
untrusted-data taint type discipline would be a large addition
duplicating much of what `limit`+contracts deliver at 1.5. Recommend:
DECIDED-OUT as a separate mechanism; revisit only if the 1.5 machinery
leaves a demonstrated residue.

## Part 6 — what the audit confirms (the positive findings)

r1 §6.2's four invariants are all shipped design: aliasing-XOR-mutability
(second-class borrows + move-only owners), no-NULL (Optional),
type-safe subprocess APIs (argv-only spawns), and panics-route-to-
controlled-shutdown (failsafe + REACH). Of r7's ten case-study
mitigations, seven are shipped mechanisms, two are 1.5 machinery
(refinement, stack bounds), one is G-7. Of r2's six residual classes,
four are eliminated by construction, one is the licensed-boundary story
1.5 completes, one (G-1) is the open decision. The exploit-weighted view
(KEV) puts the language's coverage exactly where its thesis claims:
**the classes attackers actually use are the classes the default regime
cannot spell.**

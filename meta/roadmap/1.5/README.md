# Cycle 1.5 — Verification

**Phase C.** `prove`, `limit<Rules>`, contracts (`requires`/`ensures`), Z3 over
SMT-LIB2, and NIKOS's disposition. The cycle that makes Layer 1 real — the
mathematical prevention of invalid states that is the first line of the safety
architecture.

> Detailed **map**, upgraded at the 1.4-era research pass, evidence-backed
> by the R-5/R-8 deep research (`../research/digests/r5r8-digest.md` —
> read its reliability notes before citing further). **The decision batch
> below was RATIFIED WHOLE by the user during 1.4 and recorded early for
> the model handoff: C-17 → D-218, C-14 → D-219, C-15 → D-220, C-16 →
> D-221, B-5 → D-217 (NIKOS struck).** The cycle's opening act is
> implementation, not ratification. (This file was titled "Cycle 1.4"
> until the 1.4-era sweep — two renumberings ago.)

## The state this cycle starts from

The **surface is built**: grammar, AST, and resolution for contracts, `limit`,
invariants, and `Rules` bodies parse and bind, grammar-tested. Everything from
**typing through Z3 is not**, and 0.9.0 made the gap honest: the five
verification carriers refuse with `NITPICK-RUNG-001` naming this cycle. The
process-spawn primitive Z3 needs arrives with 1.4.8's `npk_spawn` (D-206), and
`npkg` owns the invocation. The typed-builtin world (D-201) means new floor
entries are table-typed from birth.

## The decision batch (RATIFIED — D-217…D-221; the text below is the normative detail the records cite)

### C-17 — the SMT emitter and invocation architecture (the cycle's spine)

1. **One solver: Z3, exact version pinned by SHA-256** in the manifest,
   invoked as a spawned subprocess over SMT-LIB2 TEXT (D-067), never linked.
   cvc5/Bitwuzla as a second engine is decided OUT (one determinism profile,
   one pinned artifact — the reconciliation of r5's own tension is in the
   float row below).
2. **The determinism profile is law** (the r5 doctrine, corroborated by the
   Z3 parameter docs and Verus's shipped practice): `smt.random_seed=0`,
   `sat.random_seed=0`, **wall-clock timeout DISABLED**, `rlimit` as the sole
   budget — a verdict is a function of (obligation, solver build, budget),
   never of machine load. "Proofs become a function of budget, not speed."
   All four values recorded in the manifest; a mismatched solver hash refuses
   the verify build loudly.
3. **Process model: one fresh Z3 process per FUNCTION** (the Verus-buckets
   precedent — kills learned-clause pollution, makes verdicts independent of
   query order, parallelizes cleanly under `npkg` later); `push`/`pop` only
   for micro-scoping obligations INSIDE one function's process.
4. **Integer encoding: partitioned by operation class.** Ordinary arithmetic
   on `intN`/`uintN` encodes as unbounded `Int` with explicit range axioms
   (`0 <= x < 2^N`-shaped) and overflow as inequality obligations; BITWISE
   operations encode QF_BV with an explicit theory-crossing cast at the site
   (the Verus/Dafny bridge). The `tbb`/`tfp` families encode as **scaled
   unbounded Int with their ERR sentinels as explicit range rows — never the
   FP theory** (r5 is unequivocal on fixed-point). `dim256` erases to tfp256
   as everywhere.
5. **Floats: two tiers, one solver.** Tier-1 obligations (no-NaN, no-div-0,
   comparisons, bounded simple arithmetic) go to Z3's QF_FP; heavy non-linear
   float reasoning (accumulating loops, deep division chains) is abstracted
   to Real intervals (the KeY practice) — precision sacrificed knowingly, and
   the obligation manifest records which tier discharged each. What neither
   tier proves becomes a retained runtime guard, never a silent assumption.
6. **Memory: the ownership-trusting encoding.** No global heap, no McCarthy
   arrays, no MBQI-triggering axioms: second-class borrows and move-only
   owners mean obligations see VALUES — aggregates as per-field values,
   mutations as functional updates (the Verus/Creusot family, which our
   model fits even more tightly than Rust's since borrows cannot escape).
   **Slices encode as (value, integer length) with plain arithmetic bounds
   obligations — the Seq theory is decided OUT** (young, quantifier-adjacent,
   a determinism risk; r5's reliability note 3 asked for exactly this
   comparison and determinism settles it).
7. **The obligation catalogue** (the manifest's `kind` column, exhaustively):
   overflow/underflow (per G-1's ratified outcome), div-by-zero and
   INT_MIN/−1 (the D-007 carried set), bounds (slice/array/buffer), cast
   range (D-148's envelope discipline at runtime boundaries), exhaustiveness
   (pick coverage — usually discharged by the checker, recorded as
   checker-discharged rows), contract pre/post, `limit` rule adherence,
   termination (`decreases`-style variants on recursion and unbounded loops
   — also G-6's stack-depth row), twisted-family ERR-exit obligations, and
   the D-014 failsafe postcondition. Every 0.9.0-era carried obligation
   appears or the manifest has holes.
8. **Obligation identity is a CONTENT HASH** of the canonical SMT text plus
   the module-qualified symbol and kind — the cross-build key neither report
   covered, ours by design: stable across builds when the code is unchanged
   (enables the elision manifest, proof caching later, and diffable verify
   runs). Human-facing tags ride the SMT `(! … :named …)` attribute derived
   from (symbol, span, kind); the model/unsat-core parser reverse-maps tags
   to spans for counterexample reporting (`get-model` + `get-unsat-core`).
9. **Elision mechanics**: a discharged check's runtime guard is removed in
   emission and the removal RECORDED in the manifest (kind, site, obligation
   hash, solver verdict row). IR-side, proven facts may be stated as
   `llvm.assume`; **`nsw`/`nuw` flags are NOT emitted** — poison semantics
   are a refinement-checking hazard (r8 Lesson 1) and the assume form
   carries the same optimizer value without minting poison.
10. **The `undef` ban becomes a checked rule.** r8: `undef` breaks SMT
    refinement checking (~18% of Alive2-detected miscompiles). ⚡ The emitter
    TODAY seeds aggregate construction with `insertvalue … undef` (verified
    in emitted IR at 1.4.1) — those seeds become `poison` (LLVM 20 literal)
    or zeroinitializer, and a harness check greps emitted IR for `undef `
    thereafter. Small, mechanical; land at 1.5.0 (or fold into a 1.4.7
    adoption step if convenient).
11. **The TCB is stated in r8's terms**: the honest claim is **verified
    middle-end plus validated floor** — `llc` and `ld.lld` are named TRUSTED
    components (the opt-O2 harness leg is a testing instrument on that
    boundary, not a proof); Astrée + Z3 + the IR-semantics encoding are the
    evidence tools; and the floor's unverifiable residue (syscall
    trampolines, futex paths, clone/execve — the volatile bottom) is
    **enumerated and documented as the TCB floor**, the seL4 precedent,
    in a `meta/specs/TCB.md` this cycle writes.

### C-14 — elision ownership

Elision is a property of the VERIFIED BUILD, recorded in the manifest — never
a flag. `--smt-opt` is struck; `[verify]` in the manifest governs; the
artifact Astrée reads is the verified build with its elision manifest beside
it. A timeout-dependent binary is impossible by construction: verdicts are
rlimit-deterministic (C-17.2), and an undischarged obligation RETAINS its
runtime guard — the binary differs only with the manifest saying so.

### C-15 — `limit<Rules>` placement, typing, subsumption

Checks inject at the three write points: initialization, every assignment to
the limited binding, and parameter entry (a `limit`ed parameter checks at the
callee's entry — caller-side discharge via the caller's own knowledge is the
optimization, recorded like any elision). Rule names in `limit<R>` RESOLVE
(closing the audit's typo hole); `Rules` bodies TYPE (`$` = the subject's
type; clauses `bool`). Subsumption (one Rules implying another at a
boundary) is a Z3 implication obligation, not a syntactic rule. The runtime
residue traps through the D-142 route with its own code in the D-141 space.

### C-16 — contract runtime semantics

A contract violation is a PROGRAM-INVALID state, not a value: the violation
channel is the trap route (reserved codes in the D-141 space, distinct rows
for requires/ensures/invariant), reaching `failsafe` like every trap — never
a `Result` (the wrap-in-Result framing predates D-084 and is dead). In
`ensures`, `result` denotes the SUCCESS value (type T); `old(expr)` is
admitted for COPYABLE values only (scalars, sizes — snapshot at entry),
refused for owning types by name. D-014's compiler-injected
`ensures result > 0` on `failsafe` plus the non-empty-body check are
implemented here (currently nowhere). Purity: contract expressions admit no
calls except `never fails` PURE functions (no allocation, no I/O — the
checker's question), because a contract that can fail or suspend is a
contradiction in terms.

### B-5 — NIKOS: the disposition

**Recommend: strike NIKOS from 1.5's deliverables by decision** (not
deferral-by-silence): Astrée IS the abstract-interpretation evidence for the
one-shot trial; an in-house IKOS fork before 1.6 duplicates that class of
evidence while consuming the scarcest resource (time before the trial), and
the manifest's `[verify.nikos]` table stays syntactically honored with the
tooling refusing by name until a post-1.6 cycle picks it up. If the user
prefers NIKOS alive pre-Astrée, the alternative shape is a 1.5.8 spike
scoped to interval-domain-only over the emitted IR — but the recommendation
is the clean strike.

## Subcycle map

| # | Topic | Gated on |
|---|---|---|
| 1.5.0 | **Ratify + the skeleton** — the batch above recorded; the SMT-LIB2 writer; z3 spawned via `npk_spawn` under the determinism profile; the manifest schema (obligation hash, kind, verdict, elision); the `undef→poison` sweep + harness grep; `meta/specs/TCB.md` drafted | the batch |
| 1.5.1 | **Type the verification surface** — `limit<R>` names resolve, `Rules` bodies type, contract expressions type (`result`, `old`, purity) | C-15, C-16 |
| 1.5.2 | **`limit<Rules>` live** — static discharge, runtime residue, subsumption; 0.9.0's rung refusal replaced | 1.5.1 |
| 1.5.3 | **Contracts live** — requires/ensures static + runtime; the D-014 failsafe injection + non-empty-body | 1.5.1 |
| 1.5.4 | **`prove` / `assert_static`** — path-condition obligations; `assert_static` folds at the frontend (0.6's comptime evaluator) | 1.5.0 |
| 1.5.5 | **The aliasing/disjointness analysis** VERIFICATION §2.1 presupposes — the conservative refusal Z3 then relaxes (the 0.5 analyses don't contain it; this cycle creates the error it suppresses) | — |
| 1.5.6 | **The floor's spec + the executor primitives** — npkrt.ll's verifiable parts specified and Z3-checked where feasible (r8 Lesson 2); the TCB.md residue list finalized; the AtomicWaker-class primitive models (park/unpark, channel slot, waker states — the r6 verdict: model primitives, never the whole executor; BPOR-style bounds if a model spins) | 1.5.0 |
| 1.5.7 | **The G-5 schedule-exploration harness** — mocked-primitive build of the runtime (we own every primitive), PCT-seeded central scheduler, virtualized reactor (synthetic EPOLLIN), seed-replay; wired as a harness stage beside `// stress:` | G-5 ratified |
| 1.5.8 | **Overflow obligations (G-1's static leg) + close-out** — prove-or-retain on plain-int arithmetic per G-1's ratified semantics; NIKOS disposition executed; **C-19 answered before this cycle exits** (the AbsInt contact — the question list is in `../1.6/README.md`); docs synced, cycle to done/ | G-1, B-5, C-19 |

## Watch for

- **C-19 is a 1.5-exit gate, not a 1.6 discovery.** If AbsInt confirms
  C-only, the C-emission path gets scheduled BEFORE 1.6 with the
  generated-code playbook (1.6 README) as its spec — and the recursion
  question (G-6/Astrée's subset) answered in its design.
- **A verification pass that changes the artifact by the solver's mood is
  the one thing this cycle must not ship** — that is what C-17.2 + C-14
  exist to make impossible; any deviation from the determinism profile is a
  stop-the-line defect, not a tuning knob.
- **The r5/r8 digests carry reliability notes** — two of the reports'
  citations are wrong even though their conclusions check out, and one
  claim (polynomial-time Gröbner) is false outright. The digest's notes say
  which; verify against the pinned Z3's documentation during 1.5.0, and
  never cite the reports' prose without them.
- **The obligation catalogue must absorb every carried obligation** from
  0.9.0 onward or the manifest's `kind` column has holes — the carried list
  lives in this README's catalogue row and OPEN_DECISIONS' history.

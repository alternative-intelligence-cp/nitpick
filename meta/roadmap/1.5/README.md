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

> **D-233 (2026-09-01) superseded D-232: Astrée left the plan**, and with it
> the C emitter and the AbsInt contact. Every Astrée/C-19 reference below
> carries a dated annotation rather than a rewrite (the ratified batch's
> text is settled); the Z3/SMT spine of this cycle — legs, encodings,
> catalogue and all — is UNTOUCHED by that decision. The
> abstract-interpretation evidence class now lands in 1.6 as LLVM-native
> analysis over the emitted IR; the new cycle map is `../1.6/README.md`.

## The state this cycle starts from

The **surface is built**: grammar, AST, and resolution for contracts, `limit`,
invariants, and `Rules` bodies parse and bind, grammar-tested. Everything from
**typing through Z3 is not**, and 0.9.0 made the gap honest: the five
verification carriers refuse with `NITPICK-RUNG-001` naming this cycle. The
process-spawn primitive Z3 needs arrives with 1.4.8's `npk_spawn` (D-206), and
`npkg` owns the invocation. The typed-builtin world (D-201) means new floor
entries are table-typed from birth.

> **At the 1.4 close (1.4.9, 2026-09-02) — what the paragraph above now
> means, for the executor who starts here.** Cycle 1.4 is archived
> (`../done/1.4/`); the compiler is self-hosting under D-202 and the
> snapshot is refreshed from the final tree. The spawn primitive LANDED: the
> floor's `clone_exec` (one ten-word block for every supervised child; the
> every-child-bound-fd-≥-4 rule CHECKED by the runtime) and `lib/nproc.npk`'s
> `proc_spawn`/`proc_wait`/`proc_reap` — both pipes captured, every wait
> bounded, a deadline kills-reaps-retires, `proc_wait` consuming its `Proc`
> (`tests/backend/programs/proc_tool.npk` is the worked example;
> `lib/nsys.npk` holds the syscall vocabulary; `lib/nfs.npk` the file-system
> surface). That is what 1.5.0 spawns z3 through. `npkg verify` refuses by
> name today (`npkg/main.npk`) and is the command this cycle gives a body;
> `[verify]` in `nitpick.toml` is already switched on (D-068), and
> `[verify.nikos]` is D-217's — refused by name until a post-1.6 cycle.
> A ONE-TIME `HANDOFF-1.5.0.md` sits beside this file (the folder rename at
> the 1.4 close's end made the message-based handoff impossible at that one
> boundary; it retires when 1.5.0 closes). Read, in this order, `CLAUDE.md`'s
> status, `../done/1.4/README.md`'s "What cycle 1.4 taught",
> `../ORCHESTRATION.md` (D-228, normative), `bootstrap/seed/README.md` (a
> feature enters `src/` only after a snapshot that understands it — 1.5
> turns rung refusals into checker work, so plan the refreshes), this file's
> batch, then write `1.5.0.md` execution-grade before touching code. The
> harness and `npkg` both run until `meta/SWITCH.md`; the harness's result
> is the one that means the suite is green.

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

    > **[D-252, 2026-09-04]** `limit-subsume` HAS a guard: the callee's
    > entry check, at that call — a discharged row lets a direct call name
    > the callee's body. The catalogue's column reads `yes` since 1.5.2 step
    > 4; the note under the table in VERIFICATION §7b says why.
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

    > **[D-233, 2026-09-01]** The evidence-tool list reads "the D-233
    > analyzer leg + Z3 + the IR-semantics encoding" now: Astrée's seat is
    > taken by LLVM-native abstract interpretation over the emitted IR,
    > and the opt-O2 boundary gains Alive2 translation validation beside
    > the testing leg (1.6.2). The TCB claim itself — verified middle-end
    > plus validated floor, the enumerated bottom — is unchanged, and
    > TCB.md gains a second consumer: the leg-A analyzer's model of the
    > floor reads the same list.

### C-14 — elision ownership

Elision is a property of the VERIFIED BUILD, recorded in the manifest — never
a flag. `--smt-opt` is struck; `[verify]` in the manifest governs; the
artifact Astrée reads is the verified build with its elision manifest beside
it *(D-233 note, 2026-09-01: read "the leg-A analyzer" for "Astrée" — the
analyzed artifact is now the emitted IR of that same verified build, and
the elision-manifest-beside-it rule is unchanged)*. A timeout-dependent binary is impossible by construction: verdicts are
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

> **[D-233, 2026-09-01]** This section's premise — "Astrée IS the
> abstract-interpretation evidence" — is withdrawn; see D-217's annotation.
> The strike's effect on THIS cycle stands (nothing lands in 1.5 that the
> campaign does not need), and the evidence class re-homes to 1.6's leg A,
> whose bring-up gate weighs IKOS — the fork this section parked as NIKOS —
> against Clam/Crab, by measurement. The "interval-domain-only spike over
> the emitted IR" alternative this section declined is, in hindsight, the
> direction D-233 adopted in full strength.

## Subcycle map

| # | Topic | Gated on |
|---|---|---|
| 1.5.0 | **Ratify + the skeleton — DONE** (`1.5.0.md`; closed 2026-09-03). The `undef` ban (seeds `zeroinitializer`, the check in both runners, the snapshot refreshed), SHA-256 and the solver pin (the workbench's own z3 4.16.0 build, hashed; the profile READ), the SMT-LIB2 writer and the obligation walk (`src/backend/smt/`), z3 spawned one process per function through `lib/nproc.npk`, `nitpick.obligations` (P-10) written only by `npkg verify --record`, elision through `llvm.assume`, the `verify` test stage in both runners with nine tracer programs, the compiler's own set decided (141 obligations, 116 discharged) and the verified compiler rebuilding itself, `TCB.md` drafted with its floor table generated and instrumented. The tracer kind is the D-007 division pair; every later kind reuses the path. | the batch |
| 1.5.1 | **Type the verification surface — DONE** (`1.5.1.md`; closed 2026-09-03). `limit<R>` names resolve at all three sites (RESOLVE-002/-011, cycles RESOLVE-006), `Rules` bodies type over `$` and a limited binding's type is the subject by identity (TYPE-059), every proposition is a `bool`, a contract admits only what a proposition can evaluate anywhere and calls only NAMED `never fails` `pure` functions (TYPE-060), purity is a declared clause with a `Pure` column on every builtin (TYPE-061), `old(expr)` and `result` are keywords with their own nodes, expansion clones its verify nodes, S-13 closed; ratified as D-241…D-245. No obligation, no rung retired. | C-15, C-16 |
| 1.5.1b | **The workbench's findings, fixed before planned work — DONE** (`1.5.1b.md`; closed 2026-09-04). Nine landings, each under a full harness on a cumulative prefix (D-228): `NPK_HEAP_STATS` and the `cost` stage in both runners, the 1.5.1-close baseline recorded (step 0); every file's first declaration is its header and the entry points are the root's — `main`/`failsafe` outside the root refuse, 243 files gain `mod:<basename>;` (D-248, DEF-2; step 1); a root with `main` and no `failsafe` refuses at `main` naming what the absent handler owes, REACH-003 (DEF-5; 1b); a view-maker's result borrows its operand — `string_bytes`, `string_from_bytes`, `arr[lo...hi]` are `@` of what they view, by the reference's `Views` column, BORROW-012 for a view of a temporary (D-249, DEF-3; 2); the builders write into a `Sink`, each byte once, the emission byte-identical, the three cost axes bounded (DEF-1; 3); derived comparisons follow the operand's spelling, DERIVE-006 (D-250; 3b); a unit without `failsafe` declares `@npk_failsafe` and compiles to an object `llc` accepts — the `object` stage in both runners — and `pub use` re-exports after a plain `use` (DEF-6/7; 3c); an owning value no place takes is a temporary of its statement, dropped when it ends and on every exit, frame-resident across `await` (D-246; 4); `List<T>` is compiler-known and owning, move-only, the compiler's own tables die with their pass (D-247; 5) — and step 5 found `pass h.n` clearing the root's drop flag for a COPYABLE field (every owning local returned by a copyable field leaked since 1.2.3; DEF-8) and that every descriptor-exhaustion proof depended on the session's soft limit (DEF-9), so both runners stand under `nitpick.toml`'s `[limits] nofile` and `fd_ceiling.npk` measures it. O-N12…O-N15 taken (`expect-exit` bounded in both runners). Owed: step 5b (`List` into the prelude after the refresh), the snapshot refresh, a ceiling for `self.toml`. | 1.5.1 |
| 1.5.1b | **The workbench's three defects and the two leaks under them** (`1.5.1b.md`, PLANNED 2026-09-03; the user: before 1.5.2) — DEF-2 the mandatory header + root-only entry points (D-248), DEF-3 view-makers as borrows (D-249), DEF-1's three builders through a `Sink`, statement-end temporaries (D-246, D-183's recorded item), `List<T>` owning (D-247); the `NPK_HEAP_STATS` instrument and a `cost` stage first, so every fix is a number | 1.5.1 |
| 1.5.2 | **`limit<Rules>` live — DONE** (`1.5.2.md`; planned and closed 2026-09-04; D-251, D-252). Six landings under D-228's cumulative-prefix protocol. **Step 0 was DEF-14**, a soundness defect found by planning: the 1.5.0 encoder kept a stable symbol for an address-taken local, so a definition of another local in terms of it outlived a call that wrote through the pointer, and a `div-zero` row was DISCHARGED that the program then defeats — the elided build died with SIGFPE where the plain build reaches `failsafe`; an escaped name is never NAMED now, the compiler's own 141 rows re-decided identically. Then: TYPE-063 (a limited binding has no address) and TYPE-064 (a `limit` where no write point exists), `LimitViolated` (−4111) armed by REACH (step 1); the check in EVERY build — one generated `i8` predicate per `Rules`, the check AFTER every write over the binding's whole value (initialiser, every assignment to it or any part of it, the callee's entry, sync and coroutine), the site key's SPACE, the rung retired (step 2); the `limit` rows with the rule as a HYPOTHESIS on every later version — `limit_loop.npk`'s `div-zero` after a loop is the first cross-kind discharge — `limit-subsume` rows at every direct call of a sync callee, elision into ONE `llvm.assume` over the rule's range clauses, both runners kind-aware with `none` (step 3); the caller-side bypass under a belt (step 4); the docs (step 5). Unscheduled and raised as S-30 (settled: 1.5.4b): the QF_BV crossing, the twisted scaled-Int rows and the float tiers. | 1.5.1, 1.5.1b |
| 1.5.2b | **Derived impls over generic subjects — DONE** (`1.5.2b.md`, planned, ratified and closed 2026-09-05; D-253, D-256…D-259). Six landings, each a cumulative prefix under a full harness (D-228): `DECL_THREAD`'s own bit with `check_decl_flags_unique`; a family impl applies only when its bounds hold, decided where it is used — one unifier for `find_method`, `type_implements`, `bind_blanket` and the emitter's instance recording (DEF-15 closed; the non-positional target shapes positional binding never served now run); the prelude's generated scalar region — 348 impls in thirteen families, `dim256` per distinct unit vector, `string`'s three by hand, `gen_tables.py --check` and `check_generated_current` — with the tier scalars' method intercepts gated on their own names in checker AND emitter (they refused every impl method); one rule for every member of a derived body with the synthesized bound, member-wise `Clone`, `Debug` through `debug` (DEF-16, DEF-17, DEF-18 closed), the emitter's `to_string` lookup taught to see a bound parameter and a family impl; every derived diagnostic re-homed to the derive with the `<derived-` belt in both runners, and D-240 widened to every `raw` over a refused operand. Recorded for the user: DEF-19 (a `pick` over an `Optional` enum admitted then refused by the emitter) and DEF-20 (a generic enum parses and means nothing). `nitpick.obligations` never moved | 1.5.2 |
| 1.5.3 | **Contracts live** — requires/ensures static + runtime; the D-014 failsafe injection + non-empty-body | 1.5.1 |
| 1.5.4 | **`prove` / `assert_static`** — path-condition obligations; `assert_static` folds at the frontend (0.6's comptime evaluator) | 1.5.0 |
| 1.5.4b | **The remaining theories (S-30, ratified 2026-09-04)** — D-218.4/5's QF_BV crossing for bitwise operations, the scaled unbounded-Int encoding with ERR-sentinel rows for `tbb`/`tfp` (and `dim256` through it), and the two float tiers: none had a subcycle, and 1.5.8's `err-exit` rows presuppose the twisted encoding. Until it lands, a `limit` over such a subject and every division or overflow in those families is `unencoded` or absent (guard retained — safe, unproven). Planned when 1.5.4 closes, so the path-condition machinery exists first | 1.5.4 |
| 1.5.5 | **The aliasing/disjointness analysis** VERIFICATION §2.1 presupposes — the conservative refusal Z3 then relaxes (the 0.5 analyses don't contain it; this cycle creates the error it suppresses) | — |
| 1.5.6 | **The floor's spec + the executor primitives** — npkrt.ll's verifiable parts specified and Z3-checked where feasible (r8 Lesson 2); the TCB.md residue list finalized; the AtomicWaker-class primitive models (park/unpark, channel slot, waker states — the r6 verdict: model primitives, never the whole executor; BPOR-style bounds if a model spins) | 1.5.0 |
| 1.5.7 | **The G-5 schedule-exploration harness** — mocked-primitive build of the runtime (we own every primitive), PCT-seeded central scheduler, virtualized reactor (synthetic EPOLLIN), seed-replay; wired as a harness stage beside `// stress:` | G-5 ratified |
| 1.5.8 | **Overflow obligations (G-1's static leg) + close-out** — prove-or-retain on plain-int arithmetic per G-1's ratified semantics; NIKOS disposition executed per D-217-as-annotated; ~~C-19 answered before this cycle exits (the AbsInt contact)~~ **C-19 CLOSED by D-233** — no external gate remains; 1.6's bring-up gate (its README) is ordinary scheduled work; docs synced, cycle to done/ | G-1, B-5 |

## Watch for

- ~~C-19 is a 1.5-exit gate~~ **C-19 is CLOSED (D-233, 2026-09-01)** — no
  AbsInt contact, no C-emission path, no external gate on this cycle's
  exit. What 1.6 now needs from 1.5 is ordinary and internal: TCB.md's
  enumerated floor (1.5.6) feeds leg A's analyzer model, and the D-218.9
  `llvm.assume` discipline is the channel leg A's range facts ride.
- **A verification pass that changes the artifact by the solver's mood is
  the one thing this cycle must not ship** — that is what C-17.2 + C-14
  exist to make impossible; any deviation from the determinism profile is a
  stop-the-line defect, not a tuning knob.
- **The r5/r8 digests carry reliability notes** — two of the reports'
  citations are wrong even though their conclusions check out, and one
  claim (polynomial-time Gröbner) is false outright. The digest's notes say
  which; verify against the pinned Z3's documentation during 1.5.0, and
  never cite the reports' prose without them.
- **A builtin kind with a fixed method set has that set in TWO places** — the
  checker's intercept and the emitter's — and 1.5.2b found both refusing
  every impl method on `tfp`, the ternary family, `complex` and `simd`, the
  emitter's by lowering `to_string` as `floor`. The rule since: the type's
  own names go to the intercept, every other name to the impl table, and ONE
  predicate per kind (`types.npk`) is read by both sides. A new builtin kind
  with methods of its own owes the same gate on both.
- **Every prelude impl body is emitted into every program whether reached or
  not** (measured at 1.5.2b step 2: +2.2% on the compiler's own IR for 348
  rows nothing in it calls). The emitter's reachability of impl bodies is a
  question for the cycle that owns emission policy, not a bug to fix in a
  prelude step; 1.6's evidence campaign reads the emitted IR and will meet it.
- **The obligation catalogue must absorb every carried obligation** from
  0.9.0 onward or the manifest's `kind` column has holes — the carried list
  lives in this README's catalogue row and OPEN_DECISIONS' history.

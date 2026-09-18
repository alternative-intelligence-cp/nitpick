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

    > **[S-71, 1.5.6 step 4 (2026-09-11); user-ratified 2026-09-17 — D-218 (2)
    > as amended.]** The profile also carries `lp.dio=false`: z3 4.16.0's
    > Diophantine sub-solver undoes its terms at every `(pop)`, so a row that
    > had ANSWERED did not return — a wedged solver under P-13 with no clock
    > to cut it. A solver STRATEGY, deterministic, in the manifest like the
    > rest; no verdict moves with it on or off. Fixed seeds, no wall clock and
    > `rlimit` as the sole budget all stand.
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
| 1.5.2f | **A bare type parameter is move-only in the body that names it — DONE (2026-09-05)** (`1.5.2f.md`; S-40, the workbench's O-N19, ratified as **D-264**; two landings, each a cumulative prefix under a full harness). `type_owns_for_move` answers true for a `T` and for `Self` beside what drops; `require_move_if_owning`, the lending `pick` and the derive generator ask it (TYPE-046 with the parameter's own reason; DERIVE-006 for a `T` payload under the four binding traits, as for a `string`'s); the seven sites in the tree that stored a by-value `T` into an owning slot say `move`; nothing else in the compiler, the tools or the suites refuses anew. **Left open:** S-41, a borrowing `pick` binding form, which would let a generic enum with payloads derive the four again. **Watch for:** `move(x)` spends `x` at a scalar too (MOVE-001 on a later read) | 1.5.2e |
| 1.5.2g | **The ladder reports its digests; the emission is the cross-machine identity claim — DONE (2026-09-06)** (`1.5.2g.md`; S-42, the library workbench's first CI finding, ratified as **D-265**; two landings, each a cumulative prefix under a full harness). Every `npkg` ladder run prints the SHA-256 and byte count of each intermediate it produced (`lib/nhash.npk`), the harness's `parity` stage cross-checks the printed lines against an independent digest of the same files, BUILD_REFERENCE §5 says which claim holds across machines (the emission's) and which is the toolchain's (the object's, the binary's), and every pin notice carries the emission's digest. **Not built:** a tool-binary digest pin (refused by D-265: it would refuse every machine but one). **Watch for:** an emitted `.ll`'s byte count is path-dependent (every site row carries the source path relative to the manifest root, D-236), so a size comparison across directories quotes the OBJECT; the emission's DIGEST compares only between builds of the same tree at the same relative path. | 1.5.2f |
| 1.5.2h | **A lending `pick` binds views; the selector is frozen while a view is live — DONE (2026-09-06)** (`1.5.2h.md`; S-41 ratified as **D-266**; DEF-24, found by planning, fixed at its step 0; three landings, each a cumulative prefix under a full harness). A binding of a lending `pick` is a read-only VIEW of the payload in place — typed as the payload, read by value, no copy at the bind, no address, no move, no drop — the consuming form keeps ownership transfer, the selector's root is frozen inside an arm that binds (TYPE-067), a view is refused every address a limited binding is refused, the IMPLICIT receiver address included (which TYPE-063 had missed: DEF-24, a limited struct written through a pointer receiver with no trap), and the derive generator's `Eq`/`Ord`/`PartialOrd`/`Clone` lift for `string` and `T` payloads. `drop` over a refused operand gained D-240's short-circuit on the way. |
| 1.5.2i | **`string_concat` of two empties allocates nothing — DONE (2026-09-06)** (`1.5.2i.md`; DEF-25, the library workbench's report; two landings, each a cumulative prefix under a full harness: `@npk_string_concat` allocated a real block for a length-0 result and returned it with cap 0, so its drop freed nothing — 16 bytes per empty call, the prelude's `string:Clone` of an empty string and the compiler's own `string_concat(x, "")` copy idiom included). The slice's empty branch in the concat, a cost unit holding the empty loop's peak to the one-byte loop's. |
| 1.5.2e | **Two small defects from 1.5.2d's close — DONE (2026-09-05)** (`1.5.2e.md`; S-39 ratified as **D-263**; three landings, each a cumulative prefix under a full harness). Step 1: the prelude's `List<T>` stores through `alloc_managed`, the managed heap's untracked entry, PRELUDE-ONLY by the reference's `**Prelude-only**` marker (generated into `builtin_prelude_only`, TYPE-054 elsewhere), `ralloc` keeping a block's role — a `List` alive in `main` at `exit 0` exits 0 where it exited 94, and D-151 keeps counting every `wild` block. Step 2 (DEF-22, the workbench's O-N18): `.len` on a fixed-size array lowers to its constant. Step 3: the docs. **Watch for:** `exit` runs no drops (D-183's amendment) — a program test with an owning local in `main` measures the storage's REGIME, not the drop; D-010 refuses reading a local declared without an initialiser, arrays included | 1.5.2d |
| 1.5.2d | **The fixed per-program cost — DONE (2026-09-05)** (`1.5.2d.md`; S-38, the library workbench's measurement at the 1.5.2c close, ratified the same day as **D-262**; four landings, each a cumulative prefix under a full harness, D-228). Step 0 measured first, as the recommendation said: the frontend held 88% of a floor-only program's 0.82 s, and a profile put that in three scaling defects. Step 1: the bindings analysis sizes its state by the function (per-function slot ordinals at resolution) and the type table and the string interner carry hash indexes — the probe's frontend 0.72 s → 0.07 s and 96 MB → 4.4 MB, the compiler's own build 242 s → 20 s with its peak 13.4 GB → 113 MB; no verdict, type id or byte of unchanged IR moved. Step 2: an unreferenced prelude item is not emitted, decided by a fixpoint over the emitted text — the probe's IR 845,283 → 50,561 bytes and 608 → 14 functions, `llc` 0.46 s → 0.02 s, the compiler's own IR keeping 101 of 685 prelude functions, all referenced; a belt in both runners; found on the way: the head-to-tail item, the prelude's generic instances, a compiler module sharing the prelude's qualifier (renamed), the elision cross-check's rows. Step 2b (DEF-21, the workbench's finding): the undefined-symbol allowlist is the runtime's EXPORTS — internal defines out, the `module asm`'s `.globl` names in. Step 3: the docs, `self.toml`'s ceiling tightened, the workbench notified. **Watch for:** a belt on emitted IR must know WHICH compiler emitted it (the snapshot carries a change only after a refresh); an instrument that counts sites must count only the functions the emission holds | 1.5.2c |
| 1.5.2c | **Generic enums, and a `pick` over an `Optional` refused by name — DONE (2026-09-05)** (`1.5.2c.md`; planned, ratified — **D-260**, **D-261** — and closed the same day; three landings, each a cumulative prefix under a full harness, D-228). Step 0: a `pick`'s selector may not be an `Optional` (TYPE-065 at the selector, both forms; `??` and `== NIL` are the spellings). Step 1: a generic enum is a family as a generic struct is — every read of a variant's payload type binds the instance's arguments through `bind_instance` (the layout, the pattern bindings, the constructor's fit, the emitter's payload slots), a constructor's or a bare variant's instance is the expected type, else inferred from the payload by the generic call's own unifier, else TYPE-022 naming the parameter; an inferred instance is recorded and judged like an annotated one; the emitter substitutes a generic body's enum at construction and at every `pick` reader; the derive test 1.5.2b dropped returned. **Found on the way and fixed in step 1:** the `pick` EXPRESSION form typed no arm binding at all — a bound payload's member read was accepted unchecked and died as EMIT-002 on a PLAIN enum, and an owning payload could be copied out of a lending pick expression — so both spellings now run one `type_pick_rules` (`pick_expr_bindings.npk` pins the four rules that became live). Step 2: the docs. `nitpick.obligations` never moved. **Watch for:** a payload-less variant of a generic enum needs the annotation (`Opt<int32>:o = Opt.None;`); a rule written for one spelling of a construct is owed to the other — grep for the twin before calling a rule landed | 1.5.2b |
| 1.5.3 | **Contracts live — DONE (2026-09-06)** (`1.5.3.md`; D-221 and D-014 §3 over the surface 1.5.1 typed and the shapes 1.5.2 froze; S-43 and S-44 ratified the same day as **D-267** and **D-268**; four landings, each a cumulative prefix under a full harness, D-228; LIVE-1 closes). `requires` checked at the callee's entry as a predicate function `<sym>.req` (the `Rules` shape) in the checked entry and at state 0, D-252's bypass covering it; `ensures` at the return seam with `result` in register and `old` snapshots in slots; `invariant` at every loop head; `failsafe`'s injected postcondition (REACH-004 for a literal, the guard per S-43); rows at every call site and function entry (`requires`), return point (`ensures`), loop entry, body end and `continue` (`invariant`), `exit` in `failsafe` (`failsafe-post`) and impl method (conformance); a `pure never fails` call an uninterpreted function; a callee's `ensures` a hypothesis where continuing means success. Four steps, each under a full harness on a cumulative prefix. | 1.5.1 |
| 1.5.4 | **Path conditions, the counters, `prove` / `assert_static` — DONE (2026-09-06)** (`1.5.4.md`; six landings, each a cumulative prefix under a full harness, D-228; S-45…S-48 landed under their recommendations and were ratified 2026-09-07 as D-269…D-272). The encoder becomes path-sensitive: a branch condition inside its arm, the negations of the `pick` arms before an arm and of a loop's condition after it, `when`'s two blocks on "did the body run", an arm that never falls through, the guarded re-push of an arm's facts and an `ite` merge after `if`/`when`/`pick`, `&&`/`||`/ternary arms; `$` and the `for` binding as terms (the counter's entry, body, incremented and exit values, the direction as the emitter computes it once); `prove` a guard-less row the VERIFIED build refuses when undischarged (`NITPICK-VERIFY-001`) and a lemma after its site; `assert_static` folded at the frontend (TYPE-069); `exhaustive` and `assert-static` as `checker` rows end to end; a literal step the checker's (DEF-28, TYPE-068) and a computed step's guard the `loop-step` row (S-46); the rung suite retires (S-47). Found by planning: DEF-26 (a division inside a pick-expression arm has no row), DEF-27 (`enc_under` binds a colliding escaped name unnamed), DEF-28. Six steps, each a cumulative prefix under a full harness (D-228) | 1.5.3 |
| 1.5.4c | **A module symbol means one thing (D-273, DEF-31) — DONE** (`1.5.4c.md`; planned 2026-09-07, closed 2026-09-09; three landings, each a cumulative prefix under a full harness, D-228): the qualified call `m.f(x)` is a DIRECT call of the member (inline module, alias, nested path — one walk, `namespace_path`; one direct-call typer and one direct-call emitter for `f(x)` and `m.f(x)`; every reader of a method call's receiver skips a namespace by the recorded `ns_decl`), `use m.*`/`{…}`/`.f`/`as` bind a module symbol's public names through the file forms' own binders (the only way an inline module's types are named from outside), a private member is RESOLVE-003 from either spelling (the decision text's "-004" is the two-files code), an alias and a `pub mod` carry their scope in the one `inner` field, `std` is owned (RESOLVE-001); R-1 stands, R-2 holds by the fixed-point loop with no second pass. Found and fixed on the way: `Trait.method(recv, s)` never demanded `move(s)` (a double free), an async METHOD spawn armed no `DeadlineExceeded`, a struct read through a module was silently invalid, an inline module's struct had no type header and its bodies resolved type names in the file's scope. S-49/S-50 recorded for the user. `nitpick.obligations` never moved | 1.5.4 |
| 1.5.4d | **The file-module import's scope and the inline module's error identities (D-274, D-275; S-49/S-50 ratified 2026-09-10) — DONE (2026-09-10)** (`1.5.4d.md`; planned and closed the same day; four landings, each a cumulative prefix under a full harness, D-228; proceeded under R-3, a PAIR check of a qualified arm against the constants the program declares, and R-4, every arm over an `Error` selector one of `(*)`, `(Name)` bound to a constant or `(file.Name)`; found and fixed on the way: DEF-32, the reach analysis's qualifier from the first SITE's module rather than the declaring file's, latent since 1.1.6; S-51 — imports inside an inline module, a silent no-op — ratified the same day as D-276 and landed as step 2, the plan's step 0b run after step 1 because both harness windows had opened when the answer came). The plan said: a member-less `mod:name;` import carries the loaded file's scope so `name.f()` resolves as `use "./name.npk" as name;` does (one `inner` field, D-273 §4); an error constant declared inside an inline module stays the FILE's, and a `failsafe` arm's qualifier naming no module the program knows is RESOLVE-002 at the arm rather than a silent non-match. Small: the loader's `graph_load_file_module` and the arm checks; both measured at 1.5.4c as having no user in the tree | 1.5.4 |
| 1.5.4b | **The remaining theories (D-218 items 4 and 5; S-52…S-57 ratified 2026-09-10 as D-277…D-282) — DONE (2026-09-10)** (`1.5.4b.md`; planned execution-grade and closed the same day; seven landings — steps 0–4, 4b and 5 — each a cumulative prefix under a full harness, D-228). A shift's amount is checked (D-277: TYPE-070 for a known amount outside `0..width−1`, `ShiftRange` −4115 and the `shift-range` row for a computed one — the probe had shown poison reaching LLVM; the snapshot refreshed by the bridging variant so forty roots could name the constant); every bitwise operation is encoded — the Int forms wherever an operand is a numeral, at any width, and the bit-vector crossing at ≤ 64 bits (D-279, D-280: `BV_CROSS_MAX_BITS`, measured — free at 64, 18% of the budget at 256, the budget at 2048; the gate that no discharged row regresses held at every step); the twisted kinds are scaled `Int` with ERR a value and `err-exit` rows live at every `TbbErr` guard (D-278: the compiler's manifest 147 → 368 rows), with the proposition rule DEF-33 found and fixed (a proposition holds only where its evaluation does not trap) beside DEF-34 (`smt_int`'s negative numeral) and DEF-35 (a negated literal pattern); floats in two tiers (D-281: QF_FP terms, the tier column fed by the encoder — `int`/`bv`/`fp`/`-`, `real` for a tier-2 discharge — and the Real-interval twin under three soundness conditions, `flt_tier2.npk` the shape D-218 (5) was written for); `simd` values per-lane terms with one row per any-lane guard (D-282; the binding-lanes reading recorded as S-58; the first harness caught the vector guards ignoring the manifest, fixed in the step); step 4b: a float division arms no `DivByZero` (DEF-37). Recorded for the user: S-58, S-59/DEF-38 (a `simd` integer lane wraps where its scalar traps), DEF-36 (the `?!` raise and a guard's trap share one text). The compiler's own set: 368 rows in 197 function files, decided in 3.9 s. | 1.5.4 |
| 1.5.4e | **The 1.5.4b close's three, landed (D-283…D-285; ratified 2026-09-10) — DONE (2026-09-11)** (`1.5.4e.md`; planned 2026-09-10 on `18b93e1`; three landings, each a cumulative prefix under a full harness, D-228): `simd` integer lanes carry D-210 (the vector overflow intrinsics any-lane, `IntOverflow`; an integer `.sum()` traps per fold step; the compound spelling lowers through the vector path — DEF-39 found by planning), `npk_raise` as the program's entry to the one trap route (`?!` and `!!!`; DEF-40 found by planning: `!!!` bypassed the trap entry), the binding-lanes reading of D-282 confirmed; found on the way: DEF-41, a compound shift through a field with a guard and no row, fixed at step 0; `npkrt.o` moved for the first time since DEF-25 and the notice named both digests | 1.5.4b |
| 1.5.5 | **The aliasing half of D-004 — DONE (2026-09-11)** (`1.5.5.md`; planned execution-grade on `cb8cbb0`, S-60…S-62 ratified the same day as **D-286** and **D-287**; four landings, each a cumulative prefix under a full harness, D-228). Measured first: nothing decided aliasing (two `$$m` of one element ran), the spec's qualifier example never parsed (the operator form is the language's); found by planning: DEF-42 (a borrow assigned to an outer holder accepted — rule 3 unchecked for local holders) and DEF-43 (a write through a pointer to a `fixed` module binding a SIGSEGV with no `failsafe`). Step 0: DEF-42 (`check_holder_scope`). Step 1: `alias.npk` — `$$m` excludes, `$$i` shares, `@` claims nothing but is a write-capable access; lexical lifetimes (NLL out); a claim only as a whole call argument or a pointer local's value, a holder used not copied (BORROW-014); the conflict table by the paths' common prefix (BORROW-013); TYPE-071 (a `fixed` binding has no address); five tree sites, one a real conflict (`diaglist_sort`). Step 2: the computed-index overlap a RUNTIME GUARD in every build (the byte-range compare at `addr_of`, `BorrowOverlap` −4116) elided by the verified build through the `disjoint` row (kind 20) — the spec's own example discharges under the rules. Step 3: the docs. Found on the way: step 0's collector skipped the custom-shaped kinds; the compiler's own `failsafe` cannot name a new identity until a snapshot refresh (D-205). `nitpick.obligations` never moved. The original row: VERIFICATION §2.1 presupposes the analysis — the conservative refusal Z3 then relaxes (the 0.5 analyses don't contain it; this cycle creates the error it suppresses) | 1.5.4e |
| 1.5.6 | **The floor's spec + the executor primitives — DONE (2026-09-12)** (`1.5.6.md`; planned execution-grade on `149dbf6`, S-63…S-70 ratified whole as D-288…D-292 the day they were asked, DEF-44…DEF-48 found by planning and DEF-50/DEF-51 by the work; six landings under D-228, the last two merged). **What the floor now has: a specification, protocol models and an enumerated syscall boundary.** `runtime/npkrt.spec` states what 137 symbols do (`requires`/`ensures`/`frame`/`objects`/loop invariants and unrollings, `(summary)` calls, `(boundary "…")` promises at the kernel), `npkg/floor_smt.npk` translates each against the floor's own IR and `runtime/models/` carries six bounded protocol models with sixteen controls; 370 rows in `runtime/npkrt.obligations`, 363 discharged, 7 residue, none refuted, and TCB.md's three generated regions (the membership table, the syscall boundary, the residue list) cannot go stale without a red run. **Four floor defects fixed on the way** — the whole-program stop (D-291), the failsafe region (D-292), four racing plain accesses promoted to atomics (D-290), and a leak in the two file readers that no test could see (DEF-51). The profile gained `lp.dio=false` (S-71, ratified by the user 2026-09-17 as an amendment to D-218 (2)). Measured first: the floor is 167 defines (3 `asm`, 21 `atomic`, 99 `syscall`, 44 `pure`) and 69 of them are translatable (no atomic, inline asm reached only through `npk_exec` or the trap route); four plain accesses race in LLVM's model (a frame's `windup`, a channel's generation, `@npk_frozen`, the join's tid word) and the trap route's arbitration is a plain load-then-store — two concurrent traps run two `failsafe`s, and nothing stops a task running on another thread (D-063's promise unimplemented; S-69 builds the stop; S-70 the failsafe region a `failsafe` allocates from); the park/unpark protocol as a bounded model decides `unsat` in 0.76 s under the profile and its no-re-check control `sat` (the r6 verdict holds); QF_BV/QF_ABV cannot close the division identity or a 16-byte `memcpy` at the budget while the compiler's own Int partition with a UF memory decides the division's inductive step at rlimit 3,093 and a 64-byte `memcpy` at 166,810 — the floor's theory is the program's (S-64). The design: a Nitpick translator in `npkg/` reading the floor's text and writing the compiler's obligation-directory shapes so both runners' z3 loops decide the floor unchanged; `runtime/npkrt.spec` (contracts, invariants or unwinding bounds, trap outcomes, frames, boundary promises, the shared-state classification), `runtime/models/*.model` (six protocols with negative controls and a correspondence belt), `runtime/npkrt.obligations` (the floor's manifest; `open` a red run); the syscall-boundary table and the kernel-effect table; TCB.md's disposition and residue columns generated. The original row: npkrt.ll's verifiable parts specified and Z3-checked where feasible (r8 Lesson 2); the TCB.md residue list finalized; the AtomicWaker-class primitive models (park/unpark, channel slot, waker states — the r6 verdict: model primitives, never the whole executor; BPOR-style bounds if a model spins) | 1.5.0 |
| 1.5.6b | **The floor's stack rule, and the kernel-effect table held to the kernel** (`1.5.6b.md`; planned execution-grade 2026-09-17 on `b7d60dc`, S-72…S-74 ratified the same day as **D-293**, **D-294** and an amendment to **D-288**). Born at the s6→s7 hand-off: the outgoing seat named four places it would try to prove 1.5.6's evidence wrong (OPEN_DECISIONS §2g, E-1…E-4), and E-4 -- "the kernel-effect rows I wrote from knowledge" -- paid within the hour, BY MEASUREMENT: two rows wrong (one the unsound direction, latent), one of them hiding **DEF-52** (`npk_hardware_concurrency` popcounting 120 bytes of uninitialised stack: 1008 hardware threads on a 48-thread machine), and the audit of that defect's class found **DEF-53** (D-173's entry-block rule never extended to the hand-written floor: eight of fifteen allocas outside their entry blocks, two in loops -- a loop-body alloca SIGSEGVs at the pinned `-O0` and `-O2`). Steps: the stack rule (D-173's belt over the floor; a new defined-before-use belt, stores where a function has rows or runs hot -- a memset cost `npk_hs_put_dec` 8 of its 23 rows), the kernel-effect table as one generated authority with (number, option) keys and a probe program that holds it to the running kernel in both runners, E-3's faithful `epwait` and a seventh model `reactor-io` for the I/O wake path the big model names and does not model, the `hardware_concurrency` builtin, the refusal of a builtin's name, the docs. 1.5.7's virtual kernel stands on the same facts, so this runs first. **COMPLETE (2026-09-17): nine landings — steps 0–4, 4b, 4c, 4d, 5 — each a cumulative prefix under a full harness (D-228).** It GREW mid-execution by what its steps found, ratified the day it was asked ("go with your recommendations on all three"): **D-295** (S-75, raised by step 2: the floor's models read a SECOND way, exhaustive explicit-state search as a belt in both runners beside the solver's rows — three of seven models' depths were smaller than their diameters, a model's meaning had had ONE reader, and the belt's first finding was the runner self-check's own toy model, commented safe and unsafe in two steps; TCB.md §5's thirteenth acceptance narrowed to fidelity and liveness), **D-296** (S-76, raised by step 4: a CALLABLE binding — a parameter, a local, a `for` or a `pick` pattern binding of function type — may not take a builtin's name, decided by the binding's TYPE), and three defects of the function-value corner found by writing D-296's test and fixed at step 4c (**DEF-54**: a call through a pattern-bound function value emitted a symbol that does not exist and `llc` rejected the module; **DEF-55**: `raw o.f(x)` over a `never fails` field refused where `raw (o.f)(x)` was accepted; **DEF-56**: a trait method with a function-typed parameter could never be implemented). Step 2 also found `park-unpark` missing a whole step (the epoll wait's empty return: the model's reachable states went 263 → 358, no verdict moved). `runtime/npkrt.obligations`: 379 rows, 372 discharged, 7 `budget`, 0 open; `nitpick.obligations` never moved (368); the floor's bytes moved once (step 0) | 1.5.6 |
| 1.5.6c | **The floor spec's caller assumptions (leads E-1 and E-2) — DONE (2026-09-17)** (`1.5.6c.md`; approved by the user 2026-09-17 to run BEFORE 1.5.7's plan; planned and closed the same day; five landings, each a cumulative prefix under a full harness, D-228; no question asked but S-77, raised). A section's `requires` and `(objects …)` are HYPOTHESES of its rows, and nothing checks the callers no row covers — a boundary symbol, a symbol with no section, emitted code. **TWO CLAUSES WERE FALSE FOR A LEGAL CALLER**, neither a behavioural defect, both evidence that said nothing where it looked like it spoke: `npk_string_concat` assumed its two INPUT strings apart (`string_concat(s, s)` is in the tree twice; the proof never needed it) and `npk_small_free` assumed a chunk apart from the head of the list it was on (the ordinary LIFO free). Step 0: **`(lo len apart-when COND)`** — a range in the address space always and set apart only where the body reads it; the `ite` spelling first planned took the file from 202 s to 357–908 s in the runners' own mode, past its 250 s hang net, and `apart-when` takes 194 (a cost is measured in the runner's mode, in seconds AND rlimit). Step 1: **`(views (lo len) …)`** — ranges a symbol is handed to READ: apart from objects, never from each other, and read-only PROVEN (the frame row exempts no byte of a view); and two silences refused by name in both runners — a clause head the translator reads ONCE, written twice, was dropped without a word (for `frame`/`ensures-trap`/`ensures-fresh` a claim never proven), and a loop sub-clause it does not read likewise. Step 2: every other apartness ARGUED in the spec beside the clause, each argument read against the IR (three sentences of the first walk did not survive that), on six named invariants. Step 3: **TCB.md §4d, GENERATED in both runners** — per section, the callers a row covers and the callers nothing does (31 sections: 2 fully covered, 18 with a floor caller no row covers, 15 exported) — and §5's sixteenth acceptance. No verdict moved anywhere (25 hashes over two symbols); no floor byte, nothing under `src/`: no ladder row moved. Measured and recorded for what a solver CAN see: no vacuous row that should not be, every section's assumptions have a model, which pair facts each proof needs. **S-77 raised (the user's)**: the solver's hang net leaves `npk_small_free` at 81% of its bound — settled as **D-297** and landed 2026-09-17 as its own landing before 1.5.7 step 0 (the net counts the committed manifest's `budget` rows: `120 + 10·checks + 60·B`; `npk_small_free` under 610 s where it was 250) | 1.5.6b |
| 1.5.7 | **The G-5 schedule-exploration harness — UNDERWAY (planned 2026-09-17 by s7, measured with a throw-away prototype, approved by the user the same day: S-77…S-83 → D-297…D-303; `1.5.7.md`)** — the REAL floor transformed (a scheduling point before each of its 78 atomic step lines, each of its 63 `@npk_sys6(` calls routed to a hand-written IR shim — X-1: a transform, never a second floor), real clone'd threads under a baton, virtual futex/epoll/clock, PCT with ordered priority bands and a fairness bound, exact replay by seed, quiescence ORACLES that see a lost wakeup an exit code cannot (`LOST-WAKE`/`LOST-FUTEX-WAKE` are red, D-301), negative controls from the models, the spec's caller hypotheses EXECUTED at every call (D-302), `// explore: N` markers with a belt (D-299), 1,000 seeds per unit (D-300). **Step 0 LANDED 2026-09-18**: the one transformer (`npkg/explore.npk`, `tools/explored.npk`) and the totality belt in both runners, the prototype kept outside every gate (D-303). **Step 1 LANDED 2026-09-18**: the shim in hand-written IR (`runtime/explore/npkx.ll`), held to the C reference schedule hash for schedule hash on all 30 signal-free programs under load; X-13 (the address space is virtual: the floor's `mmap` trims had made a step count depend on an address, found by the port in both shims); the `explore` stage in both runners; the markers on all 44 `// stress:` programs and the marker belt. **Step 2 LANDED 2026-09-18**: virtual signals — the four trap-route programs join, 34 of 34 explorable programs agreeing with the reference hash for hash. **Step 3 LANDED 2026-09-18**: the quiescence oracles live (`LOST-WAKE`/`LOST-FUTEX-WAKE` red, D-301; the offsets held to the floor's types), the control mechanism with `store-release` and `no-rouse` (both found at seed 1; 680 clean runs, no false positive). Before step 0, S-77's own landing (D-297, `fa7b5a9`). | G-5 ratified; 1.5.6c |
| 1.5.8 | **Overflow obligations (G-1's static leg) + close-out** — prove-or-retain on plain-int arithmetic per G-1's ratified semantics; NIKOS disposition executed per D-217-as-annotated; ~~C-19 answered before this cycle exits (the AbsInt contact)~~ **C-19 CLOSED by D-233** — no external gate remains; 1.6's bring-up gate (its README) is ordinary scheduled work; docs synced, cycle to done/. **[2026-09-17, read against the catalogue.]** This row undersells the subcycle: VERIFICATION_REFERENCE §7b assigns 1.5.8 FIVE kinds, not one — `overflow`, `bounds`, `cast-range` (each a guard to elide) and `terminate`, `stack-depth` (no guard) — and none has a row in `nitpick.obligations` yet. Sized at `b7d60dc` over the compiler's own emission: 2,248 of the 2,503 guard-trap call sites are `IntOverflow` and 13 are `OutOfBounds`, against the 273 guards the verified build elides today (202 manifest rows, de-duplicated by hash) — this is where D-210 §4's "1.5 proves the traps away" is kept or not. `terminate` has NO surface syntax (`decreases` appears nowhere in the lexer, the parser, the grammar or a spec): planning opens with a language question for the user, and the library listener is owed the keyword-or-refusal answer, named with its code, before a re-pin could surprise it. The close-out also owns OPEN_DECISIONS §2g's E-1 and E-2 | G-1, B-5 |

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

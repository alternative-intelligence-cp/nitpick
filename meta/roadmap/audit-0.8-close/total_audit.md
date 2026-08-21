# Nitpick-native — Total Spec/Plan/Implementation Audit

Consolidated findings from a full-set review at **0.8 closed** (commit `267c329`).
Sources: two hands-on deep dives ([borrow checker](deep_dive_borrow_checker.md),
[allocator](deep_dive_allocator.md)) and five parallel slice audits (concurrency/
async, modules/build/self-hosting, macros/verification, grammar/traits/generics,
type-system/operators). Findings that could be exercised against the live compiler
were — probes are archived in [probes/](probes/), and the most severe finding was
confirmed by compiling through npkc and reading the emitted IR.

**Severity uses the project's own priority order: safety > correctness >
performance > developer comfort.** Nothing in `/home/randy/Workspace/REPOS/nitpick-native`
was modified.

This document is organized by **cross-cutting theme**, because the strongest
findings recurred across independent audits — the same structural problem seen
from two angles is higher-confidence than any single reviewer's catch. A
slice-by-slice index is at the end (§9) for anyone tracing a specific document.

---

## 0. The two things to fix this week (live, safety, cheap)

Everything else in this document is a plan-time correction. These two are
**shipped-behavior violations of the language's own safety doctrine**, both
confirmed by direct observation, both fixable in a single mid-cycle subcycle:

> **CLOSED at 0.9.0.** Both repaired as the opening subcycle of cycle 0.9:
> the five verification carriers refuse with `NITPICK-RUNG-001` naming 1.3
> (`tests/rejection/verification.npk` pins all five spans), and division/
> remainder emit the D-007 guard trapping to `failsafe` through D-142's
> `npk_trap` route — DIV_BY_ZERO −4097, INT_MIN_OVERFLOW −4098, executed-exit
> tests `div_guard`/`rem_guard`/`div_min`/`div_ok`. The probe below now
> refuses instead of compiling to unguarded IR.

### LIVE-1 — `limit<Rules>`, `requires`/`ensures`, and loop invariants compile to nothing, with no check and no rung refusal — **SAFETY, CONFIRMED**

The backend never reads the limit node, the contract clauses, or the invariant.
`prove` and `assert_static` correctly refuse with `NITPICK-RUNG-001` naming cycle
1.3 (`src/backend/ir/ir_stmt.npk:182-187`); the other three verification carriers
are silently dropped. **Confirmed by compiling through npkc** ([probes/limit_drop.npk](probes/limit_drop.npk)):
a `requires b != 0i32` function emitted a bare `sdiv i32` with no guard, and a
`limit<r_positive> int32:x` emitted a bare `alloca`+`store` — zero `icmp`, zero
`br`, the rule absent from the IR. This is exactly what D-068 forbids: "a safety
property must not depend on a compiler flag." **Fix:** add rung refusals for the
three carriers (same shape `prove` has), so they cannot compile-to-nothing before
1.3's real checks exist. *(macros/verification F1; independently reachable from
the type-system audit's division finding.)*

### LIVE-2 — integer division lowers to an unguarded `sdiv`/`udiv`; div-by-zero is LLVM UB in the artifact — **SAFETY, CONFIRMED**

`TYPE_REFERENCE.md:56,71-76` and D-007 mandate a zero-check branch to `failsafe`;
`src/backend/ir/ir_expr.npk:521-528,455-456` emit raw `sdiv`/`udiv`/`srem`/`urem`
with no guard and no rung refusal. **Observed in the same npkc run** — `checked`'s
body was `%t4 = sdiv i32 %t2, %t3` with nothing in front of it. `int32/0` at
runtime is a hardware fault (SIGFPE), an uncontrolled crash with actuators
potentially live — the precise event Layer 3 exists to prevent. Separately,
`INT32_MIN / -1` (also LLVM UB) has no specified result for plain integers.
**Fix:** emit the guard branch now, or — if div-hardening is deliberately 0.9 —
add a rung refusal so `/` cannot silently ship UB in the interim. Either way it
must not be silent. *(type-system #1.)*

> These two share a root cause with a planning lesson (Theme A): the rung-refusal
> discipline — "the parser accepts everything, the backend refuses what it cannot
> yet lower" (D-085) — has **holes where a construct is neither lowered correctly
> nor refused**. That is the one failure mode D-085 exists to make impossible, and
> it is worth a standing whole-tree check (§8).

---

## Theme A — Rung-refusal holes: constructs that neither lower nor refuse

D-085's contract is that every construct outside the current rung produces
`NITPICK-RUNG-001`, never silence and never a miscompile. Four confirmed holes:

| Construct | What happens today | Severity | Source |
|---|---|---|---|
| `limit`/`requires`/`ensures`/`invariant` | dropped, no check, no refusal | **safety, LIVE** | LIVE-1 |
| integer `/` `%` | unguarded `sdiv`/`urem`, no refusal | **safety, LIVE** | LIVE-2 |
| int⇄tbb same-width cast | no-op fast path; sentinel check never emitted → taint forgeable | **safety, LIVE** | type-sys #2 |
| inline `mod` members in an emitted program | `emit_all` neither descends nor refuses — code silently shed | correctness, LIVE | grammar #14d |

**The fix is uniform and cheap:** each needs a rung refusal until its real lowering
lands. The deeper fix is the instrument in §8 — a whole-tree check that every
parseable construct is either lowered or rung-refused, diffed against the AST kind
list, in the same family as `check_kinds_typed` (0.6) and `check_kinds_reachable`
(0.2). Every hole above is a construct that *parses* and that the backend walk has
no honest answer for; a diff would have named all four.

---

## Theme B — Unscheduled load-bearing subsystems (the roadmap stops at 0.9)

The roadmap is a detailed plan through 0.9 and a one-line "map" after that
(`ROADMAP.md:274-280`). Five subsystems that later cycles hard-depend on are named
nowhere concrete, and three of them form a single dependency chain that blocks 1.1
entirely. **This theme is the core of what the roadmap-authoring work must close.**

### B-1 — The real memory allocator (the keystone). *(allocator deep dive; concurrency F2.)*
Only the throwaway bump allocator exists; `dalloc` is a no-op. The dependency
chain nobody drew: **1.1 async → per-thread executor arena for coroutine frames
(D-034) → a real heap with real `free` → the exit-time `<wild-live>` leak registry
the K-semantics guarantee rests on** (presently vacuous — nothing is tracked, so
nothing is checked). Sharpened by the concurrency audit: the *surface* `arena<T>`
is a fixed-slot, `Handle`-returning allocator (`MEMORY_REFERENCE.md:122-141`) that
**cannot allocate variably-sized coroutine frames** — D-034's "executor arena" is
a distinct bump-per-task frame allocator that needs its own spec, not a
cross-reference. → **Proposed cycle 0.10**, before 1.1.

### B-2 — `Duration`, a monotonic clock, and executor timers. *(concurrency F1; driver plan §3.4; allocator note.)*
Used by every deadline API (D-056/D-062/D-071/D-083 and all of IO/concurrency),
**defined in no spec** — the word appears only as a parameter type in three I/O
signatures. Deadlines are the language's entire residual-deadlock containment
("a hang is worse than a crash"), so this is safety, not comfort. Needs: the
`Duration` type + layout + arithmetic + the relative-span-vs-absolute-timepoint
decision, `CLOCK_MONOTONIC` through the floor, futex-timeout executor integration,
and a pinned `DEADLINE_EXCEEDED` code in the D-141 error space. **Hard blocker for
1.1.** → decision owed before 1.1; drafted into the 1.1 cycle plan.

### B-3 — `arena` / `shared_arena` / `Handle` / `atomic` lowering. *(concurrency F2/F4; grammar #13; allocator.)*
All parse as keywords; all resolve to "not available at this rung yet" with **no
cycle named** (`resolve_type.npk:1392-1399`) — violating the very D-085 standard
the backend refusals meet. `atomic<T>` additionally has no enumerated permitted-`T`
set, no method return types (`compare_exchange`'s `{T,i1}`?), and no stated
Result-exemption. → `arena`/`Handle`/`atomic` scheduled in 0.10; each refusal
amended to name its rung.

### B-4 — `npkg`, the permanent build/test/fixpoint runner. *(modules #7.)*
`BUILD_REFERENCE.md` assigns the permanent fixpoint to `npkg verify` and the test
harness to `npkg test`, but **no Phase-C cycle builds npkg**, while `LAYOUT.md:71`
deletes the Python harness at 1.2. As planned, "the day self-hosting closes is the
day the project has no test runner." Also: the D-011 undefined-symbol scan lives
only in the throwaway harness, absent from the permanent build spec — it evaporates
when `bootstrap/` is deleted. → a minimal `npkg` (build/test/verify) scheduled in
or before 1.2; the D-011 scan written into `BUILD_REFERENCE §4` as a permanent
pipeline step.

### B-5 — NIKOS. *(macros/verification F7.)*
A named 1.3 deliverable (`ROADMAP.md:279`) with **zero specification** anywhere —
one flag mention, one manifest example, one toolchain sentence. An agent reaching
1.3 stops cold. → either write a NIKOS reference (domains, checks, port-vs-rebuild)
before 1.3 or strike it from the 1.3 line and schedule separately.

---

## Theme C — Decisions that must be settled before their cycle can start

Each blocks a specific cycle. These become the new `OPEN_DECISIONS.md` (§7 lists
them as a table with proposed resolutions). The pattern the audits kept finding:
the **surface** (grammar, AST, resolution) is built and tested, but the **decision
that lowering needs** was deferred and then forgotten.

### Blocks 1.0 (generics/traits/dyn) — six decisions. *(grammar #1-8; modules #5.)*
1. **`%Name` / symbol mangling** — requirements settled (reversible, hash-free),
   scheme absent; must cover module-canonical-name (files differ per importer),
   generic-arg encoding, LLVM quoting (`Container<int32>` isn't a legal bare
   identifier), and folding linkage. Confirmed by **two** audits. **Blocks 1.0
   start.**
2. **Object safety admits `Self` outside the receiver** — `bool(Self:self,
   Self:other)` passes the checker; behind a vtable the erased second arg is read
   at the wrong layout. **Safety.**
3. **`dyn` method dispatch has no checker path, no spec, no test** — you can
   assign a `dyn` value but not call through it.
4. **Multi-bound `dyn` ABI** contradicted three ways (16 bytes vs N+1 words); the
   `dyn A & B → dyn A` widening has no runtime mechanism.
5. **Associated types** parse and bind but can never be referenced — TRAITS_REFERENCE's
   own `Iterator` example does not typecheck (no `TY_ASSOC`, no projection syntax).
6. **Impls over a generic type family** are grammatically inexpressible, and
   `#[derive]` on a generic struct emits a broken `impl:Container:Eq`. Plus:
   inherited default methods are uncallable on concrete receivers (#6), and
   object-safety rule 3 has three contradictory statements (#8).

### Blocks 1.1 (async/concurrency) — beyond B-1/B-2/B-3. *(concurrency F2/F3/F6.)*
7. **Coroutine lowering** is one sentence; needs the coro ABI choice, suspend/
   resume protocol, spawn/join bookkeeping, and the wind-up-token plumbing.
8. **The D-004 borrow-across-await rule contradicts the async I/O traits** (a
   slice param is a borrow held across the call's own await) **and** the channel-
   endpoint-across-spawn model — and the *shipped* escape check enforces a third,
   narrower variant. This is the same seam my borrow-checker deep dive flagged
   (observation #1); it needs a decision that narrows "no borrow across await" to
   "no borrow across a **spawn**," which the model since D-032/62/83 supports.
9. **Construction APIs** — no channel constructor, `Job` undefined (closures are
   removed, D-018), `Thread.spawn`/executor-creation unspecified (D-083 hangs the
   join deadline on "where the executor is created" — which is nowhere), actor
   definition syntax absent, CondVar mutex-handoff protocol unstated, async trait
   methods unaddressed in TRAITS_REFERENCE.

### Blocks 1.2 (self-hosting). *(modules #1-4.)*
10. **The fixpoint acceptance criterion compares the wrong artifacts** —
    `BUILD_REFERENCE:188`/`D-085:5747` say "stage 1 and stage 2 must be
    byte-identical," but two independent emitters (Python seed vs npkc) never
    produce identical whole-program output; the *correct* check (what the harness
    implements) is "stage-1's emission of the compiler equals stage-2's." An
    implementer following the spec literally concludes self-hosting is broken.
11. **The committed seed IR does not exist** (`bootstrap/seed/` is `.gitkeep`)
    though four docs assert it does, and `LAYOUT.md`'s "delete all of `bootstrap/`
    at self-hosting" would destroy the only rebuild-from-LLVM-alone path and
    delete `npkrt.o`, which is linked into every executable including stage 1.
12. **"Byte-reproducible" (D-078) has no cross-environment definition or check** —
    toolchain version unpinned in the manifest/lock, no build-twice test, and the
    seed embeds its invocation path in `ModuleID`.
13. **Seed-retirement schedule contradiction** — SUBSET_1 §4 says `src/` adopts
    each rung's features, but the seed (sole builder until 1.2) lowers only subset
    1, so the moment `src/` uses a 0.9 construct the seed cannot build stage 1.
    When does the builder switch from seed to committed IR? Unspecified.

### Blocks 1.3 (verification) — five decisions. *(macros/verification F2-F8.)*
14. **Elision-vs-manifest ownership** — VERIFICATION_REFERENCE says `--verify`
    elides checks; D-040 hangs all reproducibility on `--smt-opt`; both cannot
    hold without reintroducing D-039's timeout-dependent-binary hazard for the
    exact artifact Astrée analyses.
15. **limit-check placement/typing/subsumption** — where checks go, what error
    code, whether `limit` is part of the parameter type; plus the frontend holes
    (rule names in `limit<R>` are never resolved — a typo passes silently; Rules
    bodies are never typed).
16. **Contract runtime semantics under universal `Result`** — the "wrap in
    Result" framing is pre-D-084; what a runtime `requires` violation returns
    (and its collision with FORMAL_DRAFT's reserved *failsafe* codes 50/51) is
    unspecified; `result` is untyped; no `old()` for postconditions; **D-014's
    compiler-injected `ensures result > 0` on `failsafe` is implemented nowhere.**
17. **The SMT encoding + invocation architecture** — no theory choice, no
    obligation catalogue matching the manifest's `kind` column, no counterexample→
    span contract, and — load-bearing — **the language has no process-spawn
    primitive** to invoke z3 with, and `npkg` (which BUILD_REFERENCE says owns the
    invocation) does not exist (ties to B-4).
18. **NIKOS scope or deferral** (= B-5).

### Blocks 1.4 (Astrée). *(macros/verification F8.)*
19. **The Astrée input-format question is recorded but not actionable** — the
    docs assume Astrée analyses "monomorphized output," but the compiler emits
    LLVM IR and **Astrée accepts C**. If AbsInt confirms C-only, an entire
    C-emission path becomes unplanned Phase-C work discovered at the start of a
    single non-renewable 30-day trial. Promote to a numbered gate answered before
    1.3 exits.

---

## Theme D — 0.9 lowering is under-specified where it lands next

Cycle 0.9 ("full type lowering") is the *current* next cycle, so its gaps are
near-term. Beyond LIVE-2, the type-system audit found the 0.9 surface riddled with
contradictions that would each stop an implementer:

- **Wide integers: LBIM vs native `i128`/`i256` — a three-way contradiction
  landing exactly on 0.9.** `TYPE_REFERENCE §4` says LBIM limb-structs "work around
  LLVM bugs"; `§6` (same doc) and D-011 (settled, with measurements: `add i128` = 5
  instructions, no libcall) say native `iN`. The 0.9 scope line and an
  `ir_types.npk` comment follow the dead LBIM reading. **Resolution:** native `iN`
  ≤256 bits; LBIM only above; restate in TR §4 and the 0.9 plan.
- **128/256-bit alignment: spec says 8, implementation computes bits/8, LLVM says
  16** — a memory-corruption class disagreement, since frontend struct layout uses
  the wrong aligns. Publish one alignment column per width matching the target
  datalayout.
- **Wide-division runtime symbols** (`__divti3` family) required by D-011, provided
  by nothing; the 0.8 undefined-symbol scan will fail the first `sdiv i128`. Add
  the four symbols with signatures to 0.9 scope.
- **Floats: `flt256`/`flt512` resolve but have no layout and no LLVM type; `flt128`
  arithmetic and `%`/`frem` need compiler-rt/libm libcalls** the floor doesn't
  provide (same D-011 class). Float div-by-zero: D-007 says trap, OP_REFERENCE says
  IEEE — decide.
- **Enum tag casts (D-140)** absent from both reference docs; general tagged-enum
  layout unstated and the one example is misaligned. **Range values** (`range<T>`)
  have nowhere to store their bound. **Ternary `is`** lowers to `select` (both arms
  evaluated — a guarded `a/b` still faults) with no stated lazy-evaluation.
- **`?` fallback on tbb**: OP_REFERENCE + D-008 promise it; D-099 + the checker
  refuse it (`?` takes only a `Result`). Pick one.

These are catalogued with exact line citations in the type-system slice report;
§7's decision table folds the load-bearing ones in.

---

## Theme E — Borrow-checker soundness holes (hands-on, confirmed)

From the [deep dive](deep_dive_borrow_checker.md), all six verified against the
live checker ([probes/](probes/)). The analyses are well-built (fail-closed, D-116
fixpoint correct); the holes are the "wrong-after-a-merge" class the 0.5
retrospective predicted:

- **F-1 (HIGH, live):** rule B's single-borrow exemption is defeated by storing a
  borrow through a non-borrow pointer destination — a dangling pointer into a dead
  local, accepted. Proven: two borrows → correctly refused; one borrow + one
  pointer dest → accepted.
- **F-2 (HIGH):** expression-`pick` `give` launders a borrow (arm verdict dropped).
- **F-3 (HIGH):** a borrow of an inner-block local, deref'd after the block, isn't
  caught (no scope-depth notion) — benign only until stack-coloring lands.
- **F-4 (MED-HIGH, live):** the `unknown` taint doesn't track param-rooted or
  field-path `Result`s — checking one field licenses reading every field's `.value`.
- **F-5 (MED-HIGH):** a `defer`-named binding can be moved/freed after registration
  → double-free the day `dalloc` really frees.
- **F-6 (MED):** `pick` arms leak `may`/`moved`/`freed` into siblings, wrongly
  refusing per-case `fixed` init (the idiom `pick` exists for).

These land as analysis-repair subcycles in **0.9.x** (the live ones first); F-2/F-3
gate on the rungs that make them reachable. The await-clause narrowing (deep dive
obs. #1) = decision C-8, owed at 1.1.

---

## Theme F — Spec staleness (numerous, low-per-item, corrosive in aggregate)

Every audit found reference docs describing removed constructs or superseded
layouts as current. Individually low severity (developer comfort), collectively a
real hazard: an implementing agent trusts the reference doc and builds the dead
design. A **doc-sync pass** is owed; the notable instances:

- `TYPE_REFERENCE §16` still shows the removed 3-field `Result` IR (`{i32,i32,i8}`);
  `§27` says `fail` stores `unknown` (impl zeroes it); `§28` operator table stale on
  four counts (`ptr->field`, `=>` "bounds checked", signed-only icmp, `?.|` typo);
  `§2.4` is a stale duplicate contradicting `§7`/D-135; `§19` `int32<Meters>`
  contradicts D-036; error-code examples use `0i32` against the doc's own tbb32 rule.
- `OP_REFERENCE` precedence table omits `?`/`?!`; documents pre-`++` and `**`/`>>>`
  that don't lex; `is` shown without required parens.
- `AST_REFERENCE` stale at four points the generator trusts (`CallExpr` implicit
  generics, `MethodCallExpr` type args with no working spelling, `ContractNode`
  missing `acquires`, `VectorCtorExpr` vs D-135).
- `DECISIONS.md`: D-129 still "OPEN" though landed; D-114 says "settle before Phase
  B" while OPEN_DECISIONS silently demoted it (two docs disagree, neither annotated);
  D-030 shows superseded `impl:...:for:` syntax; D-008 §5 tables still list removed
  `ok(x)`; D-028 heading superseded by D-088 unmarked.
- `GRAMMAR_ADOPTION_CONFLICTS` Part Q: the tryte/nyte trit-count correction (6→10)
  was ruled and never applied — TYPE_REFERENCE §7 still says 6.
- `src/prelude/prelude.npk:13-15` explains itself with the D-132-rejected model;
  several backend rung strings say "0.8" for now-complete work.

---

## Theme G — Frontend-stability items (the "built once, in full" clock is ticking)

The frontend is meant to be frozen so no backend rung forces a rewrite. Two items
would force token-table renumbering *after* that freeze if added late:

- **D-044's seven bitflag types** (`oflags`, `prot`, `mflags`, `fmode`, `fcmd`,
  `advice`, `whence`) were "settled now to avoid re-verification," are listed in
  `AST_REFERENCE` among builtins the parser must know, and **exist nowhere**. A
  user type named `oflags` silently shadows a decided builtin. Either run the
  generator to add them now, or supersede D-044 with a library-enum design — but
  decide before the frontend is called frozen. *(grammar #9.)*
- **The full integer-width set** (`int1/2/4`, `int512`–`int4096`) is accepted by
  the lexer/impl but defined by no layout in TYPE_REFERENCE; `tt_int` computes
  size/align 0 for sub-byte widths (garbage). Enumerate with a stored-as-byte rule
  or trim the grammar. *(type-sys #18.)*

---

## Theme H — Driver architecture (this folder's original task)

Fully treated in [driver_architecture_plan_v3.md](driver_architecture_plan_v3.md)
and [review_v2_findings.md](review_v2_findings.md); kernel mechanisms validated in
[poc/](poc/) (18/18 on the deployment kernel). It intersects the compiler audit at
three points, all now cross-referenced: it is the concrete implementation of D-055
(out-of-process GPU/GUI); it depends on B-2 (`Duration`) and B-3 (`atomic<int64>`);
and its borrow-across-await concern is the same seam as C-8/Theme E obs. #1. No new
driver findings; it is the most implementation-ready piece and waits only on 1.1.

---

## 8. The instrument this audit argues for

Cycle 0.6's lesson was "every hole was found by a check that diffs two lists, and
none by a test." This audit is a manual version of exactly that diff, run once, by
hand. Its findings should not need finding twice. Two standing checks would have
caught most of Themes A, D, and F automatically:

1. **`check_kinds_lowered_or_refused`** — diff the AST kind list against the backend
   walk; every kind must either lower or emit `NITPICK-RUNG-001`. Would have named
   all four Theme-A holes (limit, division, tbb-cast, inline-mod) on its first run,
   the way `check_kinds_typed` named eight untyped kinds in 0.6.3.
2. **`check_decisions_current`** — a lint that flags a `## D-NNN` marked SETTLED
   whose spec-doc cross-references still show the superseded text, and any decision
   heading whose "blocks/settle-by" clause names a cycle already in `done/`. Would
   have surfaced most of Theme F.

Both are in the house style (diff the compiler against the thing that describes it)
and both belong in the 0.9 plan as instruments, before the constructs they guard
multiply.

---

## 9. Slice index (for tracing a specific document)

| Slice audit | Findings | Heaviest hits |
|---|---|---|
| Borrow checker (deep dive) | F-1…F-6 + 8 obs | Theme E; C-8 |
| Allocator (deep dive) | C-1…C-3, 0.10 cycle, 5 notes | B-1; LIVE-adjacent |
| Concurrency / async | F1…F6 | B-2, B-3, C-7/8/9, LIVE-adjacent |
| Modules / build / self-hosting | 9 findings | B-4, C-10…13, Theme F |
| Macros / verification | F1…F11 | **LIVE-1**, B-5, C-14…18, Theme F |
| Grammar / traits / generics | 14 findings | C-1…6, Theme G, Theme F |
| Type system / operators | 31 findings | **LIVE-2**, Theme D, Theme F |

**Bottom line:** the specification set is unusually rigorous and the built
frontend is genuinely solid — but the *plan* thins to a single line past 0.9 while
five load-bearing subsystems (allocator, time, arenas/atomics, npkg, NIKOS) and
~20 gating decisions live in that thin region, and the rung-refusal discipline has
four live holes, two of them safety. The next document — the authored roadmap —
turns the thin region into per-subcycle plans and moves every gating decision into
an `OPEN_DECISIONS.md` with a named cycle, so an implementing agent works a
verified plan instead of rediscovering these at the worst possible time.

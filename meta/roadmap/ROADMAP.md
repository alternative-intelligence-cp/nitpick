# Roadmap — cycle map

The specification set is closed at **D-085**. This is the plan built on it.

## How this is organised

- **A cycle is a folder** — `0.0/`, `0.1/`, … — focused on **one topic**.
- **A subcycle is a file inside it** — `0.0.0.md`, `0.0.1.md`, … — one workable
  chunk of that topic.
- **A finished cycle moves to `done/`**, so the active work stays easy to find.
- **Commit after every subcycle. Push at minimum at the end of every cycle.**

Only the current cycle is planned in detail. Later cycles are a **map, not a
plan**: their subcycles get written when we reach them, because writing them now
would mean planning against specs that later cycles will have taught us
something about. That is the same reason the specs came first.

---

## The constraint that shapes everything

**We write in our own language from day one**, against a backend that starts
small and grows (D-085).

The bootstrap does **not** seed from the prototype compiler. The prototype
implements the language Nitpick *used to be* — its lexer has no `relay` and no
`cstring`, verified directly — so seeding from it would force our sources into a
foreign dialect and create a migration debt to undo later. It stays what it always
was: a **behavioural oracle**.

Instead, a **throwaway generator emits a seed compiler in LLVM IR** for
**subset 1**, and the seed's IR is committed. Rebuilding needs only the LLVM
toolchain; the generator is needed to regenerate the seed, never to build.

The rule that makes this work, and the one that ends the failure that killed
`nitpick-bootstrap`:

> **The parser never restricts. The backend does.**

The frontend accepts the **whole grammar from day one**. A construct the current
rung cannot lower produces a *backend* diagnostic — *not supported at this rung* —
never a parse error. The grammar is therefore never partial, never re-widened,
never rewritten.

"Subset 1" is consequently an honest statement about **what our own backend can
lower yet**, not a workaround for someone else's compiler. It shrinks to nothing
as the rungs are climbed, rather than needing a migration phase to escape.

---

## Phase A — the frontend, built once and in full

`CLAUDE.md`'s capability ladder: the frontend does not get rewritten at each
bootstrap stage. That is the failure mode the predecessors hit, and avoiding it
is why the whole frontend precedes any backend work.

| Cycle | Topic |
|---|---|
| ~~**0.0**~~ | ~~**Foundations**~~ — **DONE** (`done/0.0/`). Repo layout, subset 1, the seed (lexer/parser, checker/emitter, runtime floor), the test harness, and the diagnostics core. **Nitpick compiles, links, and runs.** |
| ~~**0.1**~~ | ~~**Lexer**~~ — **DONE** (`done/0.1/`). `LEXICAL_REFERENCE.md` in full: 238 token kinds, 154 keywords, every base and literal form, templates with `&{ }` interpolation, and the three interaction rules. |
| ~~**0.2**~~ | ~~**AST and parser**~~ — **DONE** (`done/0.2/`). `AST_REFERENCE.md` in full: 116 node kinds across six arrays, the 19-level precedence table, generics after the name, contracts, `pick` patterns, and a real parser that runs on real files. **Every node kind is reachable, and the harness re-checks that on every invocation** — the diff that proves it found sixteen defects across the cycle, none of which announced itself. |
| ~~**0.3**~~ | ~~**Modules, symbols, visibility**~~ — **DONE** (`done/0.3/`). The frontend opens files, loads a module graph, and binds every name. Three passes whose ORDER is the architecture: collect every declaration, bind imports **to a fixed point**, then resolve bodies — each needing the previous finished for every module, which is what makes D-086's legal `use` cycles resolvable rather than something to break. `resolve_audit` proves the walk has no holes, and was verified by making it fail. |
| ~~**0.4**~~ | ~~**Type system and checking**~~ — **DONE** (`done/0.4/`). `TYPE_REFERENCE.md` and `TRAITS_REFERENCE.md` in full: canonical interned types, `Result<T>` universal (D-013), casts, member access and UFCS, traits with coherence and object safety, and generics checked **at their definition** (D-064). `tools/check.npk` validates a whole program and emits nothing. **The largest cycle in Phase A, and the one where most of the work turned out to be repair** — see below. |
| ~~**0.5**~~ | ~~**Static analyses**~~ — **DONE** (`done/0.5/`). Second-class borrows and escape (D-004), definite assignment with `fixed`/`const` (D-010), `move` and use-after-free (D-065), `pick` exhaustiveness with the `tbb` ERR arm (D-008), the `unknown` taint (D-007), and lock levels (D-056). Eight subcycles, ten decisions, and **the cycle where an analysis's own bug was the recurring finding** — see below. |
| ~~**0.6**~~ | ~~**Macros and comptime**~~ — **DONE** (`done/0.6/`). Bounded expansion in all four positions with hygiene (D-057, D-124), splicing into structs and impls, `comptime` evaluation paying four debts, `#[derive]` with a prelude, and every expression kind typed. Eight subcycles, fourteen decisions, and **the cycle where the tooling found more than the tests did** — see below. |

**PHASE A IS COMPLETE.** The artifact is a **checker**: `tools/check.npk` loads a
program, expands its macros, resolves every name, types every expression, runs six
static analyses, and **emits nothing**. It exits 0 on a clean program and reports a
code and a span on anything it refuses.

What it refuses, as of the end of Phase A: a program that returns a borrow or
launders one through a call, reads an unassigned binding, writes a `fixed` binding
twice, uses a moved-from binding, double-frees, takes the address of a temporary,
leaves a `pick` arm uncovered, lets `(*)` swallow ERR, reads a tainted
`Result.value`, acquires a lock downward, expands a macro without bound, names
something in a macro body that its defining scope does not have, splices a body
where it does not fit, evaluates a `comptime` that never finishes, derives a trait
that cannot be derived, or writes a struct literal that omits a field — each with
its own code, its own span, and a case in one of the six rejection suites showing it
refuse.

### What cycle 0.4 taught, and what the later cycles should expect

**Most of the work was repair, not construction.** Four of 0.4.6's seven items and
five of 0.4.7's eight turned out to be fixes to things that already appeared to
work. That is not a comment on the earlier cycles' quality — it is a structural
consequence of D-085, and it will recur:

> The frontend accepts the whole grammar from day one. That is what makes the
> parser trustworthy, and **by itself it makes the checker's silence invisible.**
> A construct that parses is not a construct that works.

Every one of 0.4.7's five repairs dated to cycle **0.2** — the cycle that parsed
the construct. `Container<int32>` and `Container<string>` were one type;
`Mutex<Config, 2>` did not parse; a generic call ignored its type arguments; a
generic trait dropped its. In each case 0.2 recorded the source faithfully and
nothing downstream ever read what it recorded.

**The defence is the corpus and the sweep.** A construct missing from
`tests/grammar/whole_grammar.npk` is a construct nothing checks the parser
against, and the real-parser sweep over every source is what catches the seed and
the real frontend disagreeing. Both earned their keep repeatedly in 0.4 — the
sweep's most recent catch being that `dn` is a numeric literal and cannot be a
variable name.

**Write each test from the specification's own example.** Three of 0.4.6's four
defects survived because the thing meant to catch them tested a form the
specification does not use.

### What cycle 0.5 taught

**The analyses' own bugs outnumbered the ones they found.** 0.4's lesson was that
a construct which parses is not a construct that works; 0.5's is narrower and
sharper:

> **An analysis that is right on straight-line code and wrong after a merge passes
> every test written the easy way.**

That shape arrived four times. The borrow marking missed a binding assigned later
in a loop body (D-116). Definite assignment shipped the same bug in the same
week — *after* D-116 was written, and by the person who wrote it. The move
analysis inherited the fix for free because it shared the walk. And the escape
rules turned out to be defeated by one function call, which no amount of
single-function testing would have shown (D-117).

**Reading a decision does not inoculate you against it.** That is the most
transferable thing this cycle produced, and it is why 0.5.7 ends in a hand-worked
path-shape cross-product rather than a script: no checker can tell whether a merge
was handled correctly, only whether it was reached.

**Two rules were settled by checking them against this compiler.** The parameter
qualifier that would have closed the escape hole would also have stopped the
compiler compiling itself, because every context struct is built from pointer
parameters and handed back (D-117). And ownership is marked by **contract, never by
spelling** — the prototype decided it by matching `_free` suffixes, which gives
ownership semantics to any function someone names `window_close` (D-119, D-122).

**The acceptance suite is the half that was missing.** A rejection suite cannot
tell a correct analysis from one that refuses everything, and these analyses fail
closed by design — on fuel exhaustion, on an unclassified node, on anything
undecidable — so over-refusal is the failure they are most likely to have. Four of
the cycle's verifications are cases where breaking a rule refused *correct* code
rather than losing a finding.

**Five repairs to earlier cycles**, each dating to the cycle that had "finished"
the construct: a nested `mod` invisible to the type checker and to impl collection,
the resolver placing every nested module's members in the first one's scope,
`extern` functions with no type at all, and nothing anywhere checking that `@`,
`$$i`, `$$m` or `move` had an address to take.

### What cycle 0.6 taught

0.4's lesson was that a construct which parses is not one that works; 0.5's was that
an analysis right on straight-line code and wrong after a merge passes every test
written the easy way. 0.6's is about **instruments**:

> **Every hole this cycle found was found by a check that diffs two lists, and none
> of them by a test.**

Three of those checks were written here and all three found something immediately.
`check_kinds_typed` diffed the expression kinds against the type checker and found
**eight kinds never typed** — struct literals among them, so `Point{ zzz: 1i32 }`
compiled clean, and `Point{ x: 1i32 }` left a field with no value, which is the
undefined state D-010 refuses everywhere else. `check_codes_tested` found **twelve
codes with no case**. `check_codes_centralised` found nothing, which is the right
answer and not a reason to have skipped it.

The pattern each time: **the compiler and the thing that describes it have to be
diffed, because reading either alone never reveals the gap.** That is what
`check_kinds_reachable` has done for the parser since 0.2, and what nothing did for
the checker until a hole was stumbled into twice in consecutive subcycles.

**A slot that means two things cost this cycle two more defects**, bringing the
running count to seven. An `impl` read as an "item" — five header slots against
four, in a different order — segfaulted files containing no macros at all. And an
expression-bodied macro's window holds a `StmtId` where an `ExprId` was read, which
cloned whichever expression sat at that index.

**Four layering problems, and the fourth is a different kind.** `ensure_layout`,
`stmt_carries_expr` and `te_assign_mismatch` were each a function in the wrong
module, each found because a smaller compile group did not contain the module it had
been leaning on, and each moved to the layer whose question it answers. The fourth —
`type_expr` needing `check_stmt` — is a genuine cycle, because an expression-`pick`'s
arms *are* statement bodies, and it was **declared** rather than worked around. The
question that separates them: *is either direction wrong?*

**And the cycle's biggest single cost was a wrong theory held too long** (D-127). A
call passing nine arguments to a ten-parameter function read an unwritten register,
and *what junk sits in a register depends on the binary's layout* — so every size
change moved the symptom and the allocator looked guilty for two days. Adding 1.5 MB
of `.bss` with the instrumentation **disabled** crashed the program, which is a
semantic no-op and should have ended the hypothesis on the spot. `valgrind` named it
in one run, on the binary that was passing, and had been available throughout.

Two rules came out of that and both generalise past this cycle:

- **A symptom that moves when unrelated things change size is evidence of a value
  that was never written**, not only of memory that was overwritten.
- **The zero-dependency rule governs the artifact, not the workbench.** Reach for the
  debugger before building one.

**The seed now checks call arity**, which is what the whole episode was really about:
a tool that silently miscompiles the compiler is worse than no tool.

## What Phase A cost, and what it produced

**Seven cycles, 135 settled decisions, and a frontend of ~20,000 lines of Nitpick**
that validates Nitpick completely and emits nothing. The suite is 55 tests, 146
real-parser checks over every source in the tree, and six rejection suites named for
the stage that refuses.

### The three lessons, in the order they were learned

**0.4 — a construct that parses is not a construct that works.** Most of the largest
cycle turned out to be repair rather than construction, on constructs earlier cycles
had called finished.

**0.5 — an analysis right on straight-line code and wrong after a merge passes every
test written the easy way.** That shape arrived four times, including once *after*
the decision naming it was written, by the person who wrote it. **Reading a decision
does not inoculate you against it.**

**0.6 — every hole was found by a check that diffs two lists, and none by a test.**
A test proves the case somebody thought to write. A diff between the compiler and
the thing that describes it proves the cases nobody did.

### The instruments, which are the durable part

Five of them run on every invocation and each was written after something it would
have caught got through:

| check | diffs | first run found |
|---|---|---|
| `check_kinds_reachable` (0.2) | kind list vs. the parser | 16 defects across the cycle |
| node-kind reachability in tests | | |
| `check_kinds_typed` (0.6.3) | kind list vs. the type checker | 8 kinds never typed |
| `check_codes_tested` (0.6.6) | codes vs. the suites | 12 codes with no case |
| `check_codes_centralised` (0.6.6) | code literals vs. the codes modules | nothing — correctly |

Plus the **real-parser sweep**: every source in every suite is fed to
`tools/parse_check.npk` and must come back silent, which is what makes
`tests/rejection/` mean what D-085 says it means.

### The recurring defect, seven times

**A slot that means two things.** `{error, is_error}` where both could disagree
(D-069). `expr_shape` read two ways. A type name read as a token kind (D-104). A
builtin's generic count read out of a call's payload. A `StmtId` read as an `ExprId`.
An `impl` read as an "item". Each was one fact with two representations, and each
disagreed.

The fix has been the same every time and is now the house style: **name the
accessor for the kind it belongs to**, and split *whether* from *which* into two
fields rather than encoding both in one.

### What Phase A deliberately did not do

No emitter, no optimiser, no linker. The artifact refuses bad programs and produces
nothing from good ones, which is exactly what the capability ladder asks for: **the
frontend is built once, in full, so that no backend rung ever forces it to be
rewritten.** Whether that held is Phase B's answer to give.

## The 0.8-close replanning — what changed, and why

The roadmap as it stood at 0.8-close planned in detail through 0.9 and mapped the rest in one line each
(`1.0 generics · 1.1 async · 1.2 the managed lowering · 1.3 self-hosting ·
1.4 verification · 1.5 Astrée`).
The audit found that thin region hides **five unscheduled load-bearing subsystems**
and **~20 gating decisions**, and that the rung-refusal discipline has **four live
holes, two of them safety**. Four structural changes follow:

1. **A new cycle 0.10 — the memory allocator — is inserted between 0.9 and 1.0**,
   because 1.1's async executor cannot be built without arenas (D-034), arenas need
   a real heap, and the heap needs the exit-time leak registry the K-semantics
   guarantee currently only pretends to have. This is the single largest unplanned
   dependency; see [total_audit §B-1](audit-0.8-close/total_audit.md).

2. **0.9 opens with a safety-repair subcycle (0.9.0)** that closes the two live
   rung-refusal holes — dropped `limit`/contracts and unguarded division — before
   any new lowering. A construct that silently miscompiles is worse than one that
   is refused, and both of these ship today.

3. **Every cycle now carries an explicit "decisions in" section** naming the
   `OPEN_DECISIONS.md` items that must be settled before its first subcycle. A
   decision deferred and forgotten is exactly how the audit's gaps arose (the
   surface was built, the lowering decision was parked). No cycle starts against an
   open blocker.

4. **Two standing instruments are added in 0.9** (`check_kinds_lowered_or_refused`,
   `check_decisions_current`), in the house tradition of diffing the compiler
   against the thing that describes it — the mechanism that found every 0.6 hole and
   would have found most of this audit's automatically.

The planning principle is unchanged and is the reason this file still varies its
depth: **the current cycle is planned to the subcycle; later cycles are a map that
sharpens as we reach them.** What is new is that the map now names its blockers, so
the sharpening is bounded work rather than open discovery.

---

## Phase B — the backend, grown rung by rung

| Cycle | Topic |
|---|---|
| ~~**0.7**~~ | ~~**IR emission core**~~ — **DONE** (`done/0.7/`). The IR writer, types-to-LLVM diffed against the seed three ways, functions and expressions and control flow with `pick` and places, `NITPICK-RUNG-001` from the real backend, and `npkc` itself — `src/main.npk` over `src/driver/pipeline.npk`. **Subset 1 compiles, links and runs under this compiler**: 19 executed programs and 9 rung rejections on every harness run. Eight subcycles (0.7.3 inserted: the compiler could not parse itself), one runtime memory-safety fix (`ralloc`'s size header), D-136, and five Phase A holes found by the first programs the backend compiled. |
| ~~**0.8**~~ | ~~**`nlibc` core and runtime symbols**~~ — **DONE** (`done/0.8/`). The self-check debt paid: `src/` clean under the real checker, the selfhost harness stage, and **the fixpoint closed** — stage 1 rebuilds itself byte-identically on every run. The undefined-symbol scan turned zero-dependency into a checked invariant; `npkc -o`; the builtin floor measured down to what the compiler actually calls (then grown to the fd quartet, D-141) with the marker-scoped generator; the library tier born (`lib/nstr.npk`, `lib/nio.npk`) and made load-bearing — the compiler's own diagnostics go through it, to stderr, line-disciplined per D-050/D-076. Six subcycles, D-137–D-141, two ancient bugs out of the self-compile (resolution garbage edges, exponential cycle walk) and two more out of the new floor (the seed dropping `pass (relay …)` on Result<NIL>, `tbb` widening by zext). |
| ~~**0.9**~~ | ~~**Full type lowering**~~ — **DONE** (`done/0.9/`). The largest backend cycle, and the one where the type system started RUNNING: the trap route repaired first (0.9.0, D-141/D-142), the two instruments watching before the bulk landed (0.9.1 — whose first run out-found the audit), then aggregates with bounds guards, native wide integers to `i4096`, total IEEE floats (D-143), branch-free saturating `tbb` (D-144), the scalar/forms tail, and the control-flow/floor tail that ended with **zero rung strings naming the cycle**. Closed by D-146's four borrow-discipline repairs and — added at the user's request — D-147's leading-digit rule with D-148's exact literal envelope, which found that nothing had ever range-checked a literal against its type. **Ten subcycles, eight decisions (D-141–D-148), and the fixpoint held byte-identical through all of it.** |
| ~~**0.10**~~ | ~~**The memory allocator**~~ — **DONE** (`done/0.10/`). Six subcycles, D-150–D-155. The bump floor became a real slab+large allocator with out-of-band metadata and validate-before-dereference (D-150); the `<wild-live>` exit check made the K-semantics leak guarantee non-vacuous (D-151); `arena<T>`/`Handle<T>` (D-152) and `shared_arena<T>` (D-154, alloc-carries-the-value) gave 1.1 both arenas; the executor frame allocator (D-153) is the distinct thing D-034 needs; and the `wildx` W^X state machine (D-155) closed D-035 — a JIT that writes, seals, calls, and frees, with write-after-seal and execute-before-seal refused and the rest falling out of the move machinery. **A new `runtime` harness stage tests the surfaceless families in hand-written IR.** |

## Phase C — self-hosting and verification

| Cycle | Topic |
|---|---|
| ~~**1.0**~~ | ~~**Generics, traits, `dyn`**~~ — **DONE** (`done/1.0/`). The trait/`dyn` boundary the checker never had to answer, built rung by rung: reversible `%Name` mangling with a depth cap and dedup (D-156), monomorphization, traits and impls with `never fails` contract-checking (the D-163 hook), associated types that resolve, bind, and disqualify and their projections (D-160, D-164), and `dyn` — vtables, thunks, construction, dispatch, widening (D-158/D-159). The tail settled the library-facing decisions (D-165 module globals, D-166 `for` over an `Iterator`, D-168 `&{ }` via `ToString`, D-123 `Hash`/`Ord` derives) and the grammar/trait ones (D-170 parenthesised types, D-171 every impl names its target, D-172 `Trait.method`), struck two redundant operators (`?|` D-167, `++`/`--` D-174), and added `check_rung_names_open_cycle` — the instrument that makes a cycle's close a checked fact. **Closed with zero rung strings naming the cycle; the fixpoint held byte-identical throughout.** |
| **1.1** | **Async and concurrency** — **opens with D-163: `never fails` on every function, checked; `raw` and `drop` licensed only by it; a `Result` never discarded without a keyword** (three subcycles: the contract and the instrument, the `src/` sweep of ~7,900 sites, the `tests/` sweep and the refusal — the 0.8.0 shape), then coroutine lowering (D-177/D-178, 1.1.4 — landed), **the typed-error system (D-179, 1.1.5–1.1.7: `Error` as a nominal non-number, origin chains on `relay`, and the exhaustive `failsafe` — deliberately before the executor so its failures are born named)**, per-thread executors (1.1.8), channels, the D-071 suspension model. **Depends on 0.10's arenas** and **opens with the `Duration`/clock decision and the coroutine-ABI + borrow-across-await + construction-API decisions** (C-7…C-9, B-2). Map: `1.1/`. |
| **1.2** | **The managed lowering** — RAII at scope exit: the memory model's DEFAULT regime, of which the backend implements nothing. Nothing is dropped at a closing brace, so the regime every program gets unless it says otherwise is leak-until-exit, and D-151's own text records the interim as accepted ("managed-regime storage whose RAII arrives with the managed lowering"). What a drop IS per type, its ORDER against `defer` and against D-014's rule that a trap runs neither, `nodrop`, the move analysis deciding which paths still own a value at the brace, the early exits, and what a COPY of an owning value is. **Inserted here because it blocks 1.1.11**: a `Mutex` hands out a guard whose release IS scope exit, and closures are gone (D-018) so no scoped-callback form can stand in. Verifying (1.4) a compiler whose default regime is unimplemented, or handing Astrée (1.5) a program that leaks by design, are the other two reasons it cannot wait. Map: `1.2/`. **B-6.** |
| **1.3** | **Self-hosting** — the stage-1/stage-2 fixpoint (re-closed after 0.9–1.1), byte-reproducible builds, and **`npkg`** (the permanent build/test/verify runner that replaces the throwaway Python harness). **Opens by correcting the fixpoint criterion and committing the seed IR** (C-10…C-13). Map: `1.3/`. |
| **1.4** | **Verification** — `prove`, `limit<Rules>`, contracts, Z3 over SMT-LIB2, and NIKOS (or its deferral). **The least-built major subsystem; opens with five decisions** (C-14…C-18) and needs a process-spawn primitive the language does not yet have. Map: `1.4/`. |
| **1.5** | **Astrée preparation** — the single non-renewable 30-day run. **Gated on the input-format decision (C-19) answered before 1.4 exits**, because the docs assume monomorphized output while Astrée accepts C. Map: `1.5/`. |

**1.3 is the milestone that matters.** Everything before it is validated against the
seed's output; after it, the compiler validates itself.

**1.5 is the one that cannot be retried.** Confirm the accepted input format with
AbsInt long before the clock starts — promoted from a carried note to a numbered
gate (C-19) with a cycle deadline.

---

## After Phase C — the switch

Replacing the prototype is **one coordinated operation** across two repositories
and the website, and it is planned in **`meta/SWITCH.md`**: what moves where, why
`nitpick-docs` stays frozen until then, what `meta/specs/` owes before it can
replace it, the version restarting at `0.0`, and the one step that cannot be
undone cleanly.

Nothing there happens until 1.5 is finished. It is written down because the plan
was worked out in conversation, and a plan that lives only in a conversation
evaporates.

The audit folded three corrections into the switch's inputs (see `1.5/README.md`
and `OPEN_DECISIONS.md`): the ship-list is stale (`MACRO_REFERENCE.md`, added at
0.6, is in no list), the prototype-coverage pass is owned by no cycle, and
`meta/specs/` inherits every doc-staleness gap in the audit's Theme F before it
can replace `nitpick-docs`.

## Ordering notes

- **Diagnostics come first, in 0.0**, not last. They are how every later cycle is
  tested — D-075 routes them through `dyn Writer` precisely so the harness can
  capture and compare them.
- **0.7 precedes 0.8** deliberately. The first rung's programs only need to
  `exit`, so a runtime is not required to prove the emitter works; `nlibc` then
  makes those programs able to do something.


- **0.9.0 is a safety repair and comes first**, ahead of any new lowering. The two
  holes it closes are live in shipped behavior; new features can wait one subcycle,
  a divide-by-zero cannot.
- **0.10 precedes 1.1, non-negotiably.** If 1.1 is ever pulled forward, 0.10.0–0.10.3
  move with it — the executor-arena dependency is structural, only the cycle number
  is flexible.
- **Decisions precede their cycle.** Each cycle's "decisions in" section lists its
  blockers; a cycle whose blockers are open is not ready to start, and the plan says
  so rather than discovering it at the first subcycle.
- **Instruments precede the constructs they guard** (0.9's two new checks), the same
  reasoning that put diagnostics in 0.0 and the reachability check in 0.2.
- **D-163 precedes 1.1's code.** The executor and the Bridge are thousands of new
  lines of `raw` and `drop`; written under the licence they need no second sweep.
  1.1 retires the word's old home (D-149's Bridge) and owns the spawn form whose
  error channel D-163 requires, so it is the cycle that gives the word its new one.
- **Verification is 1.4, but its obligations are carried forward from every cycle** —
  0.9.0's rung refusals for `limit`/contracts are the first installment, so the
  constructs are honestly refused until 1.4 can honestly check them.


## The cycle-numbering convention, relaxed at 0.10

The old note here read "cycle numbers stay single-digit per major so the file
explorer sorts them." Inserting the allocator cycle broke that: there is no
single digit between 9 and 10, and renumbering Phase C instead would have
invalidated every "1.2 = self-hosting"-style cross-reference across
`DECISIONS.md`, `SUBSET_1.md`, and the cycle docs. By the project's own priority
order — correctness over comfort — the convention lost: **the cycle is `0.10`,
it sorts before `0.9` in a plain listing, and the map above is authoritative
over lexical order.** (Decided at the merge of the 0.8-close replanning; the
alternative and its cost are recorded in `audit-0.8-close/` provenance.)

**And at 1.1.10-close, Phase C WAS renumbered** — the thing the paragraph above
declined to do. The difference is the whole of it: there, renumbering would
have bought lexical sort order, which is comfort. Here a subsystem the memory
model has always specified turned out to have no cycle at all, and it blocks
the next subcycle (**B-6**): a `Mutex` guard releases at scope exit, and scope
exit does nothing. A missing half of the memory model is not a numbering
question.

| was | is | topic |
|---|---|---|
| 1.2 | **1.3** | Self-hosting |
| 1.3 | **1.4** | Verification |
| 1.4 | **1.5** | Astrée preparation |

**1.2 is now the managed lowering.** The cost the older note predicted is real
and was paid: the cycle folders, the nine rung strings naming the verification
cycle, `OPEN_DECISIONS.md`'s Blocks column and the forward references in
`DECISIONS.md` were all swept. **Archived notes under `done/` were NOT
rewritten** — they record what was true when written, so "scheduled for 1.3" in
an archive means verification under the old map, and this table is how to read
it.

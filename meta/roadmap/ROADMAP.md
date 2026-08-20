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

## Phase B — the backend, grown rung by rung

| Cycle | Topic |
|---|---|
| ~~**0.7**~~ | ~~**IR emission core**~~ — **DONE** (`done/0.7/`). The IR writer, types-to-LLVM diffed against the seed three ways, functions and expressions and control flow with `pick` and places, `NITPICK-RUNG-001` from the real backend, and `npkc` itself — `src/main.npk` over `src/driver/pipeline.npk`. **Subset 1 compiles, links and runs under this compiler**: 19 executed programs and 9 rung rejections on every harness run. Eight subcycles (0.7.3 inserted: the compiler could not parse itself), one runtime memory-safety fix (`ralloc`'s size header), D-136, and five Phase A holes found by the first programs the backend compiled. |
| **0.8** | **`nlibc` core and runtime symbols** — syscalls, memory, the symbols LLVM emits (D-011, D-015). Programs that can do something. |
| **0.9** | **Full type lowering** — `Result`, structs, enums, slices, arrays, LBIM, `tbb` |

## Phase C — self-hosting and verification

| Cycle | Topic |
|---|---|
| **1.0** | **Generics, traits, `dyn`** — monomorphization, depth cap, reversible mangling (D-064) |
| **1.1** | **Async and concurrency** — coroutine lowering, executors, channels, the D-071 suspension model |
| **1.2** | **Self-hosting** — stage 1, the stage-1/stage-2 fixpoint, byte-reproducible builds (D-078, D-085) |
| **1.3** | **Verification integration** — `prove`, `limit<Rules>`, contracts, Z3 over SMT-LIB2, NIKOS |
| **1.4** | **Astrée preparation** — the single non-renewable 30-day run |

**1.2 is the milestone that matters.** Everything before it is validated against
the seed's output; after it, the compiler validates itself.

**1.4 is the one that cannot be retried.** Confirm the accepted input format with
AbsInt long before the clock starts — that item has been carried since the spec
work began and does not belong at the end of a queue.

---

## After Phase C — the switch

Replacing the prototype is **one coordinated operation** across two repositories
and the website, and it is planned in **`meta/SWITCH.md`**: what moves where, why
`nitpick-docs` stays frozen until then, what `meta/specs/` owes before it can
replace it, the version restarting at `0.0`, and the one step that cannot be
undone cleanly.

Nothing there happens until 1.4 is finished. It is written down because the plan
was worked out in conversation, and a plan that lives only in a conversation
evaporates.

## Ordering notes

- **Diagnostics come first, in 0.0**, not last. They are how every later cycle is
  tested — D-075 routes them through `dyn Writer` precisely so the harness can
  capture and compare them.
- **0.7 precedes 0.8** deliberately. The first rung's programs only need to
  `exit`, so a runtime is not required to prove the emitter works; `nlibc` then
  makes those programs able to do something.
- **Verification is 1.3, but is not an afterthought.** Every cycle carries its own
  obligations forward; 1.3 is where the tooling is wired up, not where correctness
  starts being considered.
- **Cycle numbers stay single-digit per major** so the file explorer sorts them.
  Phase C rolls to `1.x` for that reason, not because it implies a release.

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
| **0.5** | **Static analyses** — second-class borrows and escape, definite assignment, exhaustiveness, `move`, lock levels, `unknown` taint. **Planned in detail** (`0.5/`), eight subcycles. |
| **0.6** | **Macros and comptime** — bounded expansion (D-057), `comptime` evaluation, derive |

At the end of Phase A the artifact is a **checker**: it validates `.npk` sources
completely and emits nothing. **`tools/check.npk` is that artifact's shape today**
— load, resolve, type-check, report codes and spans — with cycles 0.5 and 0.6
adding the analyses and macros on top of it.

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

## Phase B — the backend, grown rung by rung

| Cycle | Topic |
|---|---|
| **0.7** | **IR emission core** — the LLVM IR text emitter and the first rung: integer arithmetic, calls, `exit`. Programs that run and return a code. |
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

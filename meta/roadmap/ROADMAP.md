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
| **0.1** | **Lexer** — `LEXICAL_REFERENCE.md` in full, including `>>` splitting, positional `!`, the `#` sigil, and every literal form |
| **0.2** | **AST and parser** — `AST_REFERENCE.md` in full, the 19-level precedence table, generics, contracts, `pick` patterns |
| **0.3** | **Modules, symbols, visibility** — `MODULE_REFERENCE.md`, dependency roots, ambiguity-is-an-error resolution |
| **0.4** | **Type system and checking** — `TYPE_REFERENCE.md`, `Result<T>`, traits and coherence, generics checked **at their definition** (D-064) |
| **0.5** | **Static analyses** — second-class borrows and escape, definite assignment, exhaustiveness, `move`, lock levels, `unknown` taint |
| **0.6** | **Macros and comptime** — bounded expansion (D-057), `comptime` evaluation, derive |

At the end of Phase A the artifact is a **checker**: it validates `.npk` sources
completely and emits nothing.

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

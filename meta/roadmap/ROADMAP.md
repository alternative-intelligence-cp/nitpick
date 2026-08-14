# Roadmap — cycle map

The specification set is closed at **D-084**. This is the plan built on it.

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

**Stage 0 is the prototype compiler, and it does not speak the language we
specified.**

D-079 makes the prototype `npkc` the stage-0 bootstrap compiler. But the
prototype's lexer has **no `relay` and no `cstring`** — verified directly against
`src/frontend/lexer/lexer.cpp`. It does have `comptime` and turbofish.

So every source file in this repository must, until stage 1 exists, be written in
the **intersection** of what stage 0 accepts and what the new language means. That
is not a temporary inconvenience to work around ad hoc; it is a defined subset
that needs writing down, testing against, and eventually migrating off
(cycle **1.3**).

The practical shape of it: error propagation is spelled
`if (r.is_error) { fail r.error; }` rather than `relay` until we can compile
ourselves — and per D-080's own evidence, that idiom is written wrong often
enough to matter, so the conformance tests in **0.0.1** exist partly to catch it.

---

## Phase A — the frontend, built once and in full

`CLAUDE.md`'s capability ladder: the frontend does not get rewritten at each
bootstrap stage. That is the failure mode the predecessors hit, and avoiding it
is why the whole frontend precedes any backend work.

| Cycle | Topic |
|---|---|
| **0.0** | **Foundations** — repo layout, the bootstrap subset, diagnostics core, test harness |
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
| **1.2** | **Self-hosting** — stage 1, the stage-1/stage-2 fixpoint, byte-reproducible builds (D-078, D-079) |
| **1.3** | **Migration off the bootstrap subset** — rewrite our own sources into full Nitpick, re-establish the fixpoint |
| **1.4** | **Verification integration** — `prove`, `limit<Rules>`, contracts, Z3 over SMT-LIB2, NIKOS |
| **1.5** | **Astrée preparation** — the single non-renewable 30-day run |

**1.2 is the milestone that matters.** Everything before it is validated against
stage 0's output; after it, the compiler validates itself.

**1.5 is the one that cannot be retried.** Confirm the accepted input format with
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
- **Verification is 1.4, but is not an afterthought.** Every cycle carries its own
  obligations forward; 1.4 is where the tooling is wired up, not where correctness
  starts being considered.
- **Cycle numbers stay single-digit per major** so the file explorer sorts them.
  Phase C rolls to `1.x` for that reason, not because it implies a release.

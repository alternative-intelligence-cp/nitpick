# B-6 — the managed lowering: a design study

**The default memory regime has no implementation.** `CLAUDE.md`'s own table
says *(default)* → "Managed — static ownership, RAII at scope exit", and the
backend emits nothing at a closing brace. D-151 records the consequence as a
knowingly accepted interim: runtime-internal storage "is managed-regime storage
whose RAII arrives with the managed lowering, reclaimed wholesale meanwhile by
`wild_release_all()` or process death". So the regime a program gets unless it
says otherwise is **leak-until-exit**.

That was survivable while every owning value in the compiler was either `wild`
(manual, with `defer`), arena-allocated, or lived for the process. It stops
being survivable at 1.1.11: a `Mutex<T, LEVEL>` hands out a guard, and
`CONCURRENCY_REFERENCE` §9's own example ends `}   // guard drops here; the
lock is released`. **A guard is the first type whose entire meaning is its
scope.** Without a drop it never releases, and every `Mutex` deadlocks on its
second acquisition. Closures are gone (D-018), so there is no scoped-callback
form to fall back on.

Six questions have to be answered together, because the answers constrain each
other. Each carries a recommendation.

---

## 1. What a drop IS, per type

A drop is a generated function per type, `@"npk.drop.<type>"`, emitted once
alongside the type's other machinery and called at scope exit. Most types do
not need one, and **a type with no drop must generate no call at all** — the
cost of this feature must be zero for the scalars that dominate every program.

| Type | Drop |
|---|---|
| `int*`, `bool`, `char`, `float*`, `tbb*`, `Error`, enum tag, `NIL`, kernel ids | none |
| `Handle<T>`, channel endpoint | none — an index, not an owner (D-152/D-182) |
| `T->` (pointer) | **none.** A pointer is not an owner; `wild` memory is manual (`defer`/`dalloc`), which is the whole point of the regime being explicit |
| slice `T[]` | none — a borrow (D-070) |
| `string` | free the body |
| `T[N]` array | each element, ascending, if `T` drops |
| struct | each field in **reverse declaration order**, if it drops |
| enum with payload | the ACTIVE variant's payload, selected on the tag |
| `T?` | the inner, if present |
| `Result<T>` | the value when `err == 0`; an error case carries no value |
| `atomic<T>` | as `T` |
| `arena<T>`, `shared_arena<T>` | release the slabs |
| `Channel<T, LEVEL, CAP>` | **reclaim the slot and bump the generation** |
| `dyn Trait` | through a **drop slot in the vtable** — see §5 |

**The channel row is the one that closes a hole 1.1.10 had to leave open.**
D-182 makes an endpoint generation-checked so a stale one is `StaleHandle`
rather than a dangling read; but nothing reclaims a slot, so the generation
never moves and `StaleHandle` is currently **unreachable from source**. The
check is right and guards a reuse that does not happen yet. Reclamation is what
makes it real.

---

## 2. Ordering, and `defer`

Within a scope, drops run in **reverse declaration order** — the standard and
the only one that respects construction dependencies.

**`defer` blocks run BEFORE the drops of the same scope.** D-080 lists both on
the same exits and does not order them, and the order is forced: a `defer` body
can name the scope's bindings, so dropping first would hand it freed storage.
This also keeps the two mechanisms legible as a pair — `defer` is what the
author wrote, drops are what the regime owes, and the author's code runs while
its world is still intact.

**A trap runs neither** (D-014, already settled, unchanged): at trap time the
state is unknown, and running cleanup against possibly-corrupt state in an
order nobody chose is exactly what D-014 refuses.

---

## 3. Which exits, and which bindings

Every path that LEAVES a scope runs its drops: the closing brace, `break`,
`continue`, `pass`, `fail`, `relay`, `return`, and `exit`. A `pass` from an
inner scope runs the drops of every scope it unwinds through, innermost first.

**A suspension is not a scope exit** (D-177) and runs nothing — the frame lives
on, and its locals with it. This falls out correctly if drops are emitted at
scope-exit edges rather than at function-return edges, but it is the single
easiest thing to get wrong in a coroutine, and 1.1.4's crossing-locals walk
already knows which locals are frame-resident.

`exit` is the interesting one. D-014 already runs `defer` on `exit`, so drops
follow it; and D-151's leak check runs at exit and would otherwise report the
program's own managed storage. **Ordering matters: drops run, then the leak
check.** Getting this backwards turns the feature on and every clean program
starts trapping.

---

## 4. What a COPY is — the crux

D-065 settled that **nothing moves by being passed**: ownership transfers only
where `move` is written. That was consistent while nothing was dropped. The
moment drops exist it is a **double free**: `f(s)` copies a `string`'s body
pointer, and both the caller's binding and the callee's parameter drop it.

Two ways out:

- **(a) A type with a drop is move-only.** Passing or assigning it without
  `move` is a type error naming the type and the reason. The source is
  invalidated exactly as `move` already does (D-065's machinery, unchanged).
- **(b) Deep-copy on assignment.** An implicit allocation and an implicit copy
  at every binding.

**Recommendation: (a).** (b) is implicit expense and implicit behaviour, which
the blueprint philosophy refuses — and it makes the cost of a line depend on
the type of a name somewhere else. (a) is explicit, greppable, and it makes
something that is currently a *lie* into a *fact*: D-072 writes
`send(move(v), deadline)` in its own signature, and 1.1.10-D found that nothing
requires the `move`. Under (a) it is required, and the rung refusal 1.1.10-B
put on channels with owning elements is retired rather than made permanent.

The cost is honest and should be stated: **this is a real language rule**, and
it will make some existing code fail to compile until a `move` is written. That
is the correct direction — every one of those sites is a place where two names
believed they owned one thing.

---

## 5. `dyn`, and the vtable

Behind `dyn` the concrete type is erased, so the caller cannot know which drop
to call — **the vtable gains a drop slot**, filled with the concrete type's
drop, and `dyn` values drop through it. This is the same shape D-158/D-159
already use for methods and costs one pointer per vtable.

Note the contrast with 1.1.10-D5's async rule: an `async` method cannot go
behind `dyn` because the caller must know the FRAME SIZE *before* it calls. A
drop needs no size from the caller — it is one call with one pointer — so it
works where the coroutine does not.

---

## 6. Conditional moves, and drop flags

A binding moved on one branch and not another cannot be dropped
unconditionally, and cannot be skipped either. The bindings analysis (D-065)
already tracks moved-from state; where it can prove the answer statically, the
drop is emitted or elided outright. Where it cannot — a move inside an `if` —
the honest lowering is a **drop flag**: a one-bit local, set at initialisation,
cleared by the move, tested at the drop.

**Recommendation: emit flags only where the analysis cannot decide, and make
the analysis report how often that happens.** A flag per owning local would be
simple and uniformly wasteful; proving most of them away is most of the value,
and an instrument that counts the residue is how the proof stays honest. This
is the one part of the design where measurement should precede optimisation.

---

## What this buys, beyond the guard

- `StaleHandle` becomes reachable, and the channel generation check stops being
  a guard against a reuse nothing performs.
- The 1.1.10-B rung refusing channel elements that own heap storage is retired.
- `move` on a channel `send` becomes enforceable at the moment it becomes
  meaningful.
- D-151's leak check stops being scoped around the runtime's own storage.
- And the language's default regime becomes the thing the documentation has
  described since before any of this was built.

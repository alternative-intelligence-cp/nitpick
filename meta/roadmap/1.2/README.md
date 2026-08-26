# Cycle 1.2 — The managed lowering

**Phase C, inserted at the close of 1.1.10.** The memory model's DEFAULT regime
is "managed — static ownership, RAII at scope exit", and the backend implements
none of it. Nothing is dropped at a closing brace, so the regime every program
gets unless it says otherwise is leak-until-exit — an interim D-151 records as
knowingly accepted ("managed-regime storage whose RAII arrives with the managed
lowering").

**Why here and not later.** It blocks 1.1.11: a `Mutex<T, LEVEL>` hands out a
guard whose release IS scope exit, and closures are gone (D-018) so no
scoped-callback form can stand in. Verifying (1.4) a compiler whose default
regime is unimplemented, or handing Astrée (1.5) a program that leaks by
design, are the other two reasons it cannot wait. Phase C was renumbered to
make room; the mapping is in `ROADMAP.md`.

**Read `B6_MANAGED_LOWERING_STUDY.md` first** — six questions whose answers
constrain each other, each with a recommendation. **D-183** records what was
settled.

## The one that changes the language

§4 of the study. D-065 settled that nothing moves by being passed, which was
consistent while nothing was dropped and is a **double free** the moment
anything is. The answer is that **a type with a drop is move-only**: passing or
assigning it without `move` is a type error. This will make existing code fail
to compile until a `move` is written, and every such site is a place where two
names believed they owned one thing.

## Subcycles

| # | Topic | Gated on |
|---|---|---|
| 1.2.0 | **The drop table and the generated function** — `@"npk.drop.<T>"` per type that needs one, nothing emitted for types that do not; scalars, `string` (**with the `cap == 0` ownership bit, which means literals change to `cap = 0` first**), arrays, structs (reverse field order), enums (on the tag), `T?`, `Result<T>`, `atomic<T>`. A `check_drops_total` instrument: every type kind either drops, is stated not to, or fails the harness — the B-7 shape, applied where it is now load-bearing | D-183 |
| 1.2.1 | **Scope exits** — drops at the closing brace, `break`, `continue`, `pass`, `fail`, `relay`, `return`, `exit`; inner-to-outer on a multi-scope exit; `defer` BEFORE drops; a trap runs neither (D-014); **a suspension is not a scope exit** (D-177) | 1.2.0 |
| 1.2.2 | **Move-only types** — the type rule of study §4, the diagnostic that names the type and the reason, and the `src/`/`tests/` sweep it forces. The 0.8.0/1.1.0 shape: land the rule REPORTING, sweep, then flip it to refusing | 1.2.1 |
| 1.2.3 | **Drop flags** — the conditional-move residue: static proof where the bindings analysis can decide, a one-bit local where it cannot, and an instrument counting how often it cannot. Measurement before optimisation | 1.2.2 |
| 1.2.4 | **`dyn` drops** — the vtable's drop slot (D-158/D-159's shape, one pointer), and dropping through it | 1.2.0 |
| 1.2.5 | **Channels, arenas, and the leak check** — reclaim a channel slot and bump its generation, making `StaleHandle` reachable and testable for the first time; retire 1.1.10-B's rung on owning channel elements; arena and shared-arena release; drops before D-151's exit check, in that order | 1.2.1, 1.2.4 |

## Done when

A `Mutex` guard releases at the closing brace; a `string` local is freed once
and exactly once; `StaleHandle` has a test that provokes it from source; the
channel-element rung is gone; a program that copies an owning value without
`move` does not compile; and the fixpoint still holds byte-identical.

## Watch for

- **The zero-cost floor.** A type with no drop must generate no call. If a
  scalar-only function's IR changes at all, the design has slipped — and the
  `nf-inert` twins check is the existing instrument closest to noticing.
- **`exit` ordering.** Drops run, THEN D-151's leak check. Backwards, and every
  clean program starts trapping the day the feature lands.
- **The suspension boundary.** Drops belong on scope-exit edges, not
  function-return edges. A coroutine's locals outlive its suspensions, and
  1.1.4's crossing-locals walk already knows which ones.
- **The sweep is the cycle's bulk, as it was at 1.1.1.** Expect the move-only
  rule to touch far more sites than the drop machinery does, and expect the
  instrument to measure the real debt before the refusal flips.

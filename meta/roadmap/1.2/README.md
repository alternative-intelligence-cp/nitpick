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
| 1.2.1 | **Move-only types** — the type rule of study §4, the diagnostic that names the type and the reason, and the `src/`/`tests/` sweep it forces. The 0.8.0/1.1.0 shape: land the rule REPORTING, measure the real debt, sweep, then flip it to refusing. **Moved ahead of the call sites — see below** | 1.2.0 |
| 1.2.2 | **Turning the drops on** — uncomment the one call in `run_defers_down_to`; the machinery beneath it (scope frames, the unwind, the bodies) landed at 1.2.0b | 1.2.1 |
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

## Log

### 1.2.0a — the ownership bit, the predicate, and the instrument — DONE

**Literals now carry `cap = 0`.** The prerequisite for any string drop: a
literal's body is a module constant, and the capacity field — read by nothing —
is what tells an owned body from a static one. Both literal-emission sites
changed; the empty-template site already wrote 0.

**`type_drops` answers for every kind**, with the composite answers recorded by
LAYOUT the way `haspt` already is: a struct owns something exactly when one of
its fields does, an enum exactly when some variant's payload does, and layout
is the one pass that walks every field's resolved type. Storing it in a `Type`
slot would make two structurally identical structs intern as different types,
which is the mistake `tt_set_haspt`'s comment records having already been made
once.

`arena<T>`, `shared_arena<T>` and `Channel<T, …>` answer NO today and must
answer YES at 1.2.5. That is written into the predicate rather than left
implicit, because the channel one is load-bearing: reclaiming a slot is what
moves its generation, and until it does, D-182's `StaleHandle` cannot be
provoked from source.

**`check_drops_total`** is the instrument, and it is the B-7 shape applied where
it is now load-bearing. A type kind absent from `type_drops` falls to "owns
nothing" — which, if the kind owns something, is a leak that nothing reports,
since no test fails when a drop is merely not emitted. So a kind must be named
by the predicate or excused in a table WITH A REASON, and the check also catches
the two stale directions: an excuse for a kind that is now handled, and an
excuse for a kind that no longer exists. Verified by making all three fail.

Nothing is dropped yet — 1.2.0b emits the drop functions, 1.2.1 calls them.

### 1.2.0b — the machinery, and why the order in this table changed — DONE

Everything a drop needs, built and **not yet called**:

- A **scope frame** carries the locals mark it was pushed at, parallel to the
  defer frame and pushed with it — so a scope's drops are its locals above that
  mark, walked backwards, which is reverse declaration order for free.
- `run_defers_down_to` is the single seam. Every exit passes through it —
  normal block end, `break`, `continue`, and every function exit — so "defers
  first, then drops, innermost scope first" is a property of one loop rather
  than an ordering anybody has to maintain at each exit.
- `@"npk.drop.<T>"` bodies for `string` (conditional on the `cap == 0`
  ownership bit), structs (fields in reverse, only the owning ones), arrays,
  `Result<T>` (the value only when `err == 0`), `Optional<T>` (tag 1 means
  present), and `atomic<T>` (as its element). Registered on demand at the call
  sites and emitted after the module's functions, the loop re-reading its count
  because a struct's drop registers its fields'.

**Then the order in this table turned out to be wrong, and the compiler said
so.** Wiring the call before the MOVE-ONLY rule is not a leak fix, it is a
use-after-free: D-065 settled that nothing moves by being passed, so a `string`
handed to `strtab_add` is COPIED into the table and then freed at the caller's
closing brace, leaving the table pointing at released storage. The compiler
does exactly this, everywhere.

That was predicted from D-183 §4 and then **measured rather than argued**: with
the call in, `string_lib`, `fd_io` and `line_discipline` segfaulted, `file_io`
and `07_strings` failed, and npkc could no longer compile itself. With it out,
all five pass again.

So 1.2.1 is now the move-only rule and 1.2.2 turns the calls on — one
commented line, whose comment says exactly this. The sequencing error is worth
recording: the plan had the mechanism before the rule that makes the mechanism
sound, which reads naturally and is backwards. **A drop is only correct in a
language where ownership is unique**, and making ownership unique is the
larger half of this cycle.

### 1.2.1a — the move-only rule, measured — and the half of it that is missing

The rule of study §4 is implemented and wired to every value slot: var-decl,
assignment, call arguments, `pass`, and struct-literal fields. It fires when a
**place** of an owning type is read into a value position, and stays silent for
a temporary — `string:s = string_concat(a, b) ?! E;` has no second name to
invalidate, which is what keeps the rule from turning construction into
ceremony.

**The measurement, which is what this step was for:**

| | sites |
|---|---|
| `src/frontend/resolve_type.npk` | 69 |
| `src/backend/emit_program.npk` | 55 |
| `src/frontend/module_graph.npk` | 41 |
| `src/frontend/diagnostics.npk` | 25 |
| `src/backend/ir/ir_writer.npk` | 21 |
| eleven other files | 45 |
| **total** | **256** |

By type: `string` 200, `ConstVal` 32, `Diagnostic` 12, `FoldFlow` 9, and four
one-offs. For scale, D-163's comparable sweep was ~8,900 sites.

**Then reading them showed the rule is not yet right, and the rule is
unarmed until it is.** A by-value parameter of an owning type means two
different things in this compiler today:

- `strtab_add(t, data)` **stores** its string. The caller must transfer
  ownership, and `move` is exactly right.
- `string_eq(a, b)` only **reads** its arguments. Demanding `move` there would
  invalidate the caller's binding for a call that never took ownership —
  turning correct code into a use-after-move to satisfy a rule about an
  ownership transfer that did not happen.

The missing half is a convention for read-only parameters. Nitpick already has
the mechanism: `$$i string`, the second-class borrow (D-004), which **cannot
escape** — so a callee that borrows provably cannot store, and the two meanings
stop being one spelling. That is what makes `move` at the remaining sites both
correct and informative.

It is also an API change across the compiler and a decision about the
language's standard idiom for passing a string, so it is the user's rather than
one taken mid-sweep. The rule sits measured and gated at a single early return
whose comment says exactly this — the same shape as the drop calls held back at
1.2.0b, and for the same reason: the mechanism is right and the rule around it
is not settled.
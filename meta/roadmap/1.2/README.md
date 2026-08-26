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

### 1.2.1b — both candidate spellings tested, and both blocked

The read-only-parameter convention was ratified as `$$i string` — the
second-class borrow. **It does not work, and the reason is not a detail.**

**A borrow needs a place, and a literal has none.** `width($$i "klmno")` is
refused by TYPE-024, correctly: D-146 refuses taking the address of a
temporary. So a `string->` parameter cannot accept a string literal — and
`string_eq(name, "main")` is the shape this compiler is built out of. The
convention fails for exactly the functions it exists to serve. Measured, not
predicted: a borrow parameter works (`width($$i a)` runs, and `a` survives two
calls), and the same function refuses a literal.

**The scale, for the record.** `string_eq` alone has **721 call sites**; the
by-value owning parameters number 228 across ~146 functions. So this is not a
convention that can be adopted quietly at the edges.

**Then the alternative was tested too.** `move string:s` as a parameter
modifier — marking the rare CONSUMING parameter and leaving read-only ones
untouched — does not parse. It needs one new grammar production.

`nodrop string:s` does parse, and its existing meaning ("this binding is not
dropped") is very nearly right — but it is the wrong polarity. Which polarity
is available is forced, not chosen:

| default | what the caller writes | cost |
|---|---|---|
| callee does NOT own | nothing at read-only calls; `move(x)` at consuming ones | ~a dozen annotations — but the marker for "takes ownership" does not exist |
| callee OWNS | `move(x)` at **every** call | thousands of sites, and `string_eq(move(a), move(b))` **destroys the caller's bindings** — wrong, not merely verbose |

The second is not expensive, it is incorrect: a read-only call must not
invalidate its arguments. So the first is the only coherent design, and it
needs the one thing that does not exist yet — a parameter modifier meaning
*this parameter takes ownership*.

**Recommendation: spell it `move`.** The language already uses `move(x)` for
the transfer at the call site; `move T:p` at the declaration is the same word
for the same event at the other end of it, which is what the blueprint
philosophy asks of a notation. Default stays what D-065 already settled —
passing transfers nothing — so read-only code, literals included, is untouched.

**This is a grammar change, and grammar changes are the user's call**
(CLAUDE.md: adding a production because the language genuinely needs one is
ordinary work, but it must be raised). Cycle 1.2 is blocked on it: the drop
calls wait on the move-only rule, and the move-only rule waits on this.

*Noted in passing:* `nodrop` on a parameter is accepted although D-065 requires
it to sit on `wild`/`wildx` — a qualifier permitted where it says something
untrue. Small, real, and unrelated to this decision.

### 1.2.1e — `move T:p` lands, and the measurement is corrected

**`move T:p` is in.** One line in `p_qualifier_bit` plus one bit constant —
`move` was already a lexer keyword, so there is no new syntactic form. It
parses wherever a qualifier can and is refused (`NITPICK-MOVE-004`) anywhere
but a parameter, the same shape `nodrop` has. D-065's heading and
`LEXICAL_REFERENCE`'s row are amended rather than left to contradict it: the
operator form is untouched and `move` is still not a MEMORY qualifier — it
says how the binding was initialised, not how its storage is managed.

**`TYPE-047` closes the chain without touching the escape analysis.** Moving
out of an ordinary parameter is refused: it was lent, not given, so there is
no ownership here to pass on. A function that STORES its argument therefore
has exactly one way to be written — declare the parameter `move`, at which
point its callers must transfer too. Storing → needs `move` → refused on a
lent parameter → the parameter must be declared `move` → callers transfer.
No new ownership model in the escape walk.

**`pass` moves implicitly.** It always transfers, in every function, for every
type, and control leaves the frame so the binding cannot be read again. One
rule, no exceptions. Requiring `move(v)` on every return would have been 313
of the 459 sites, on the most common statement in the language, to say what
the construct already says.

**And the earlier measurement was wrong.** 1.2.1a recorded 256 sites across
five frontend files. `DiagList` holds **256** — that was the cap, so the
number was the cap and the distribution was just the first 256 diagnostics in
emission order, which walks the frontend first. Measured again with the cap
lifted:

| | sites |
|---|---|
| every slot wired | **459** |
| with `pass` moving implicitly | **146** |

and the 146 sit in `ir_expr` (74), `ir_stmt` (20), `ir_func` (17) —
the backend's IR emitters — with `resolve_path`, `diagnostics` and a few
others behind them. By type: `string` 132, `Diagnostic` 7, `ConstVal` 3.

They are the genuine transfers: `strtab_add` storing its string, `DiagList`
storing a diagnostic, values copied back OUT of a container. That last shape
is the one to watch in the sweep — reading a stored value copies it, and the
container still owns the original.

**The lesson for the next measurement**: count with an instrument, not with
diagnostics. D-163's sweep could report 8,921 because the harness counted;
this one reported the cap because the compiler did.

### 1.2.1f — the sweep: 146 → 28, and what the last 28 are waiting on

**The seed had to learn `move` first.** C-13's rule is that `src/` may use only
what the CURRENT BUILDER compiles, and the seed rung-refused `move(...)` and
did not parse `move T:p`. Both are three-line changes: ownership is a CHECKER
fact, so the seed types and emits `move(x)` as `x` — exactly what the real
backend's `ExprMoveExpr` arm does — and adds `move` to its qualifier set
without enforcing anything, the same way it parses `$$i` without enforcing
D-004.

**118 of the 146 are resolved.** 70 transfers written, 12 consuming parameters
declared, the rest restructured to read in place. `intern_add`, `strtab_add`,
`diaglist_push`, `irw_site`, `foldenv_set`, `fnem_bind` and `fnem_cross_add`
now say `move` on the parameter they keep — and marking `intern_add` cost
**nothing** at its 71 call sites, because callers pass freshly-built values,
not places. That is the shape of the whole sweep: consuming functions are rare
and their callers were already correct.

**The compiler checked the sweep, which is why it converged.** A `move` written
where the value is still used is use-after-move, and D-065's analysis has
caught that since 0.5.3. 122 wraps went in mechanically; **52 came back out**
because the compiler refused them, over a few rounds of "apply, ask, revert".
No judgement of mine was trusted where the analysis had an opinion.

**Two shapes needed a rule, not a fix:**

- **Permuting a container through a pointer.** `diaglist_sort` moves elements
  between slots of a `DiagList->`. Nothing leaves the caller's ownership — the
  array holds what it held — so TYPE-047 now fires only when the ROOT is a
  by-value parameter. A pointer parameter is a lent reference, and moving
  within the pointee is mutation, which is what a mutable reference is for.
- **Reading in place beats copying.** `irw_alloca(fe.w, slot, fe.plls[i])`
  hands the element straight to a lending parameter: no local, no copy, no
  transfer. Several sites dissolved rather than needing `move`.

### The 28 that remain, and the question under them

Mostly struct literals taking places — mechanical. But underneath them sits one
question with **no correct spelling in the language today**:

> A getter over a container of owning values. `fnem_iter_slot` returns a stored
> `string`. Returning a **borrow** is refused by D-004 rule 2 (a borrow may not
> travel up). Returning a **value** copies one the container still owns. And
> `pass` moving implicitly would take the element OUT of the container,
> leaving a hole.

Every option is wrong, so the rule stays gated: arming it would refuse working
code with nothing to replace it. This is the next real decision of the cycle,
and it is a language question rather than a sweep question.

*(Also recorded: moving out of a container leaves a hole the container still
believes it owns. That belongs with partial-move tracking, not with this rule.)*

**A measurement lesson, again.** Several "zero errors" readings during this
sweep were `quickcheck` timing out at 60 seconds on a self-check that takes 59
— and empty output greps as zero. The counts only became trustworthy when run
against the binary directly, with the diagnostics read from **stderr**, which
is where this compiler puts them.

### 1.2.2 — the rule is ARMED: the compiler is move-only clean, and TYPE-046 is an error

**The getter question answered itself, and better than the options I framed.**
The 0.8.1 escape rule already lets a borrow travel up one frame when it is
rooted at a PARAMETER — the constructor pattern the compiler is built out of —
so a getter over a container is `T->(Container->:c)` returning `$$i c.field`,
verified end-to-end by a running program. Where a sentinel return made the
borrow awkward (`fnem_iter_slot`'s `""`), the getter became an INDEX and the
caller reads the table in place, which also dissolved `fnem_lookup`: returning
a `LocalSlot` copied a string the table still owns, and a borrow cannot ride in
a struct field (D-004 rule 3), so it is `fnem_lookup_idx` now.

**`string` implements `Clone`** — the prelude trait existed with no impl. That
is the language's one spelling for "I genuinely want a second owner": explicit,
greppable, allocating. Inside `never fails` emitters the same idea is spelled
`raw string_concat(x, "")`, since `clone` may fail and an emitter may not.

**What the sweep settled, structurally rather than site-by-site:**

- Constructors CONSUME what they store: `iv`, `iv_rung`, `pv`, `pv_rung`,
  `ll_ok`, `ll_rung`, `rt`, `diag_make`, `cv_str`, `flow_ret`, `lexer_init`,
  `srcmgr_add`, `rootlist_add`, `graph_init` all take `move` on the values
  they keep, and their bodies move them into the literal.
- `Resolution.first` was STRUCK: it always equalled `path` when ambiguous and
  was read once — a field that duplicates another is two owners of one body
  the moment drops exist. The sweep found a redundant field, which is the
  design working.
- `ll_text_is_scalar_int` asks the scalar question of a bare type string;
  callers were building a throwaway `LlType` — copying a lent string into a
  local that existed only to be asked — at every cast site.
- `diaglist_sort` is move choreography now: `cur` moves out, the shifts move
  slot-to-slot, the insert moves back in; the pure-read walks hold borrows.

**Then the armed rule swept the test suite and found real things:**
`enum_payloads` copied a pick arm's payload binding out of an enum the enum
still owns (now an explicit copy — `move` would hollow the enum);
`file_io` copied `r.value` out of a `Result` it never read again (now a
move); and a `type_generic` probe copied a container field to prove a
substitution that `.len` proves without owning anything.

TYPE-046 is an ERROR, off `UNTESTED_CODES`, with its rejection case beside
TYPE-047's in `tests/types/rejection/move_rules.npk`.
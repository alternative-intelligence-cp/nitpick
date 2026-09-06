# Design Decisions Log

Running record of settled decisions and their rationale, so they do not have to
be re-litigated. Items marked **PROPOSED** are awaiting sign-off.

---

## D-001 — Drop `sys!!!` and `asm!!!` — **SETTLED**

The raw-tier syscall (`sys!!!`) and raw inline-assembly (`asm!!!<T>`) builtins
documented in `BUILTIN_REFERENCE.md` §3 and §5 returned **bare values**, bypassing
`Result<T>` entirely. They are removed.

Remaining, all `Result`-wrapped:

| Builtin | Returns | Tier |
|---|---|---|
| `sys(CONST, args...)` | `Result<int64>` | curated whitelist |
| `sys!!(CONST, args...)` | `Result<int64>` | any syscall |
| `asm!!<T>(arch, code, constraints, args)` | `Result<T>` | inline assembly |

**Rationale.** These were the two places in the language where an unchecked
failure was most likely to be catastrophic — raw syscalls and raw assembly — and
the two places with no error wrapper. `sys!!!` additionally permitted an
arbitrary expression as the syscall number.

Removing them also serves the **blueprint philosophy**: a construct should not
change meaning by context. With these gone, the rule "every function returns
`Result<T>` except `main` and `failsafe`" has no exceptions to remember.

**Follow-up:** `BUILTIN_REFERENCE.md` §3 and §5 need editing; `raw` / `_!`
remains the single explicit, greppable, auditable bypass.

---

## D-002 — FFI must map C failures into `Result.err` — **SETTLED; the contract half is superseded by D-149** (in-process C linkage no longer exists, so the per-function `fails on` contracts below are never written — the PRINCIPLE, that a foreign failure always arrives as an errored `Result` and never a silent success, survives and is delivered by the driver wire protocol's uniform status instead; D-163 then gives `never fails` a GENERAL home — a checked contract on any function, not just an `extern` (landed 1.1.0) — and licenses `raw`/`drop` by it (the refusal flips at 1.1.2))

`TYPE_REFERENCE.md` §11.2 currently specifies that when a C function "does not
provide error information, the result defaults to `Ok(val)`". **This is not the
intent and is removed.** A C call that fails must produce an errored `Result`,
never a silent success.

Intended behavior: because Nitpick has a matching or near-matching type for every
C primitive, `extern` declarations auto-wrap into the correct `Result<T>` **and
populate the error field when the call fails**.

### Mechanism — explicit error contracts on every `extern`

C has no universal failure convention; it is per-function, so the mapping cannot
be inferred from the type. Each `extern` declaration therefore states its own
failure condition, and **omitting it is a compile error** — silence must not
become a silent `Ok`.

```nitpick
extern "libc" {
    func:open   = int32(int8->:path, int32:flags)  fails on result < 0i32 with errno;
    func:malloc = wild any->(int64:size)           fails on result == NULL;
    func:strlen = int64(int8->:s)                  never fails;
}
```

- `fails on <expr>` — predicate over `result` that marks the call as failed.
- `with errno` — optional; source of the error code. Without it, a generic FFI
  error code is used.
- `never fails` — explicit, greppable assertion that the function cannot fail.
  Required rather than implied, so that "this C function is infallible" is a
  documented claim a reviewer can audit rather than an unstated default.

**On the keyword choice.** An earlier draft used `fails when`, which was rejected:
`when` is already the state-tracked loop construct (`when` / `then` / `end`), and
reusing it would give one keyword two meanings by context — a direct
blueprint-philosophy violation. `on` and `with` collide with nothing.

`fails` is a morphological variant of the existing `fail` keyword and denotes the
same concept (producing an errored `Result`), so the vocabulary stays coherent
rather than growing.

**Why keywords rather than symbols here.** Facet 2 of the blueprint philosophy
favors dense symbols, but Nitpick already draws the line by position: symbols for
expression-level operations (`?`, `?!`, `=>`, `@`, `<-`), keywords for
declaration-site contracts (`requires`, `ensures`, `invariant`, `limit`). An FFI
error contract is a declaration-site contract, so it takes keywords. Predicates
are unparenthesized, matching `requires` / `ensures` rather than the
parenthesized conditions of `if` / `while`.

**Rationale.** The reason `extern` was changed to return `Result<T>` at all was to
pull the FFI boundary inside the controlled-failure regime. Defaulting to `Ok`
pushed it back out while *looking* safe at the call site, which is worse than no
wrapper because it defeats review. Making the contract mandatory keeps one
uniform rule with no context-dependent default.

---

## D-003 — Memory model — **SETTLED**

**Recommendation: static ownership (borrow checker + RAII/NLL) for unique and
scoped data, arenas with `Handle<T>` for graph-shaped and cyclic data. Drop the
tracing garbage collector entirely, including the `gc` modifier.**

This supersedes the hybrid "keep `gc` as opt-in" suggestion in
`PRE_PLANNING_REVIEW.md` §1.5 — see "cycles" below for why that changed.

### Scoring against the stated criteria

| | Tracing GC | Borrow checker + arenas |
|---|---|---|
| **Verification** | Collector is *runtime* TCB code; concurrent mutator makes the heap global mutable state. Verifying user code also gets harder — object validity becomes a global reachability property. | Checker is *compile-time*; a bug is unsoundness, not runtime TCB. Produces lexical lifetime facts Z3 consumes directly. Prototype precedent exists (Z3 proving borrow index disjointness). |
| **Performance** | Card-table write barriers on every pointer store; safepoint polls at back-edges and calls; unbounded pauses; higher, less predictable peak memory. | **Zero runtime cost.** Deterministic scope-exit frees. No barriers, no safepoints, no pauses. |
| **Safety** | Eliminates use-after-free in safe code — but introduces an *unbounded pause* hazard with no trap and no handler, on the actuator path. | Eliminates use-after-free statically. No new runtime hazard. Cannot collect cycles — but see below. |

### Why the cycle argument does not save the GC

Cycle collection is the one thing static ownership genuinely cannot do, and it
was the strongest argument for keeping a collector. It does not survive contact
with the rest of the design:

1. **Arenas already handle cyclic graphs.** Allocate the graph in an arena and
   drop the arena wholesale. Individual nodes are never freed, so cycles among
   them are irrelevant.
2. **`Handle<T>` makes intra-arena edges safe, including cyclic ones.** Handles
   are indices with generation counters, not pointers. A stale handle fails
   safely through `Result<T>` instead of dangling. This is *already specified*.
3. **Leaks are already a controlled failure in Nitpick.** The K-semantics rule on
   `exit` traps to `failsafe` if unmanaged memory is still live. So the GC's
   remaining advantage addresses a condition the language already treats as a
   detected, controlled error rather than silent corruption.
4. Generation-counter validity is a **local, checkable** property — far easier for
   Z3 than global reachability.

Nikola's allocation profile reinforces this: manifold state, Mamba state,
tensors, and waveform buffers are bulk-numeric with batch lifetimes — arena
shaped. The genuinely cyclic part (knowledge graph, ingestion) is a minority of
traffic and is *not* on the real-time robotics path. Each mini-VM can own its
arena and drop it at teardown.

### Why drop the `gc` modifier rather than keep it as opt-in

Keeping `gc` "just in case" means writing, verifying, and maintaining a
generational collector anyway — the entire TCB cost — to serve a case arenas
already cover. It also leaves **two parallel lifetime disciplines**, so every
piece of code carries the question "which model governs this?" That is precisely
what the **blueprint philosophy** exists to prevent.

Retain: `stack`, `wild`, `wildx`, arenas + `Handle<T>`, `defer`, `nodrop`, and
RAII with NLL. Remove: `gc` and the collector.

### Consequences to accept

- **The borrow checker comes back**, and it is the hardest part of the frontend.
  Mitigation: prior art exists in `../nitpick/` (C++) and
  `../npkc-native/src/frontend/borrow_checker.npk`.
- **`SPEC_GAPS` §1 and `MEMORY_REFERENCE.md` §1.1 both need rewriting**, and the
  RAII-vs-GC contradiction (`PRE_PLANNING_REVIEW.md` §3.1) resolves toward RAII.
- **NLL is retained** — ergonomics at no safety cost, and the prototype had it.
- **Open sub-decision:** the exact escape rule for `@local` — see D-004.

### Spec propagation still owed

Deferred until D-004 lands, since both touch the same sections:
`MEMORY_REFERENCE.md` §1.1 and §1.3, `SPEC_GAPS_AND_AMBIGUITIES.md` §1,
`BUILTIN_REFERENCE.md` §1 ("untracked by the garbage collector"),
`AST_REFERENCE.md` §2 (`memory_modifier` enum), and `FORMAL_DRAFT` chapters 03,
07, and 09 if that draft is adopted.

---

## D-004 — Escape rule for `@local` — **SETTLED; rule 3 amended by D-223 (a borrow never enters a `wild`-qualified slot)**

**Recommendation: borrows are second-class. A borrow may be passed *down* the
call stack but may never travel *up* — not returned, not stored into anything
outliving the frame, not captured, not carried across a suspension.**

### This builds on machinery the language already has

The prototype's `FORMAL_DRAFT` already contains most of the pieces, which is why
this does not require inventing a lifetime system:

| Existing | Source | Role here |
|---|---|---|
| `$$i` / `$$m` borrow operators | `FORMAL_DRAFT/07` §7.3.1 | immutable (many) / mutable (exclusive) borrows |
| `borrow_imm` / `borrow_mut` qualifiers | `FORMAL_DRAFT/03` §3.3.4 | declaration-site borrow bindings |
| **Escape analysis** | `FORMAL_DRAFT/03` §3.3.1 | already used to choose stack vs heap placement |
| `stack` errors if the variable escapes | `FORMAL_DRAFT/03` §3.3.1 | precedent: escape is already a compile error for one qualifier |
| Move/copy by default | `FORMAL_DRAFT/07` §7.3.1 | borrows are the opt-in to avoid copies, not the default |
| Z3 borrow disjointness | `formal_verification_specs.txt` §5 | proves `$$m arr[i]` / `$$m arr[j]` disjoint |

The proposal is to **generalize `stack`'s existing escape rule to all borrows**,
and reuse the escape analysis that is already specified.

### The rule

1. `@local` yields a **borrow**, not a first-class pointer.
2. A borrow may not appear in the value of `pass`, `fail`, or `return`.
3. A borrow may not be stored into any aggregate that is not provably
   shorter-lived than the borrow.
4. A borrow may not cross an **`extern` call** (the callee may retain it and the
   compiler cannot see that), a **thread spawn**, or an **`await` point**.
5. A **closure may not capture a borrow**. Closures lower to
   `{ method_ptr, env_ptr }` fat pointers (`FORMAL_DRAFT/09` §9.7.3) whose
   environment can outlive the frame.

Passing a borrow downward as a function argument is always safe and needs no
annotation, because the callee's frame is strictly inner.

### What replaces `gc` for returning references

`FORMAL_DRAFT/09` §9.2.3 justifies `gc` as needed "when returning references that
outlive the current frame without moving ownership." With `gc` removed by D-003,
four mechanisms cover that case, all already in the language:

| Need | Mechanism |
|---|---|
| Return data the caller owns | **Move it** — already the default |
| Return a reference into a long-lived structure | **`Handle<T>`** into an arena outliving the frame |
| Return manually-managed memory | **`wild` pointer** — explicit, greppable, leak-checked at `exit` |
| Let the callee fill a caller-owned destination | Pass **`$$m`** of the destination downward |

### Why second-class rather than full lifetime inference

Full lifetime inference (lifetime parameters, variance, subtyping) is the single
largest source of complexity in a borrow checker and the hardest part to verify.
Second-class borrows need **no lifetime variables at all**: validity is bounded by
the callee frame, structurally. The checker reduces to escape analysis plus the
existing aliasing rules for `$$i` / `$$m`, both of which are already specified.

This directly serves the D-003 rationale — a smaller checker is a more verifiable
checker — and satisfies the **blueprint philosophy**: `@x` means exactly one
thing everywhere, "a borrow valid for this call," with no context-dependent
lifetime to track.

### Consequences to accept

- Rule 3 is deliberately strict to start. If real compiler code needs
  borrow-into-inner-aggregate, relax it then, with evidence.
- `#[lexical_drop]` (`FORMAL_DRAFT/03` §3.5) becomes redundant — it exists to force
  deterministic RAII "bypassing standard GC", which is the default under D-003.
- Rule 4's `await` clause matters because `async` lowers to `@llvm.coro` state
  machines (`FORMAL_DRAFT/07` §7.4); a borrow of the *caller's* frame held across a
  suspension can outlive it.

---

## D-005 — `Result<T>` layout — **SETTLED**

Two incompatible layouts existed. The **`{ T value, tbb32 error, bool is_error }`**
form (`TYPE_REFERENCE.md` §11.2) is correct.

```nitpick
struct<T>:Result = {
    T:value;        // Success value (zero-initialized on the error path)
    tbb32:error;    // Error code. Convention: < 0 system, > 0 user
    bool:is_error;  // Error flag
};
```

**Rejected:** `FORMAL_DRAFT/09` §9.7.1's `{ T value, void* err_payload, int8 is_error }`,
which stuffed integer error codes into a pointer field to avoid heap allocation.

Two independent reasons it is wrong:

1. **`void` and `void*` are not valid outside `extern` blocks.** Outside FFI they
   are replaced by `NIL` and `any->` respectively. A core language type cannot be
   defined in terms of a construct the language forbids in that position.
2. **`is_error` must be `bool`, not `int8`.** A `bool` permits exactly two values
   and **no arithmetic** — only logical operations (not / and / or). Typing the
   error flag as an integer would permit arithmetic on it. Underlying storage is
   still a byte; the restriction is at the type level, deliberately.

### Governing principle: semantic types are not their representations

Sharing a machine representation with an integer does not make something an
integer. The same reasoning produced `char` as a type separate from `int8`:
arithmetic on characters is disallowed, and `char` instead carries built-in
methods for what that arithmetic is normally used for (`toUpper`, `toLower`,
`isNumeric`, `isVisible`, …). A character is not an integer at the semantic
level, and neither is a boolean.

Apply this when specifying any new type: do not collapse a distinct semantic
concept into a primitive because the layouts match.

### Related, already consistent

- **Void functions do not exist.** A function producing no value returns `NIL`,
  and its `Result` must be handled like any other — a function that returns no
  value can still fail. (`TYPE_REFERENCE.md` §27; also a blueprint-philosophy
  application.)

### Follow-up

- `FORMAL_DRAFT/09` §9.7.1 must be corrected if that draft is adopted.
- ~~**Still open:** how `tbb32`'s sticky ERR state is encoded within the 4-byte
  error field.~~ — **settled by D-069.** Nothing is lost and nothing is
  undocumented: `INT32_MIN` *is* `tbb32`'s published ERR sentinel. What was
  missing was its *meaning* in this field — an error whose identity was lost —
  and it is now **unconstructible**, trapping where it would be built. D-069 also
  removes the stored `is_error` field, which encoded the same fact as
  `error != 0` with no invariant relating the two; `r.is_error` survives as a
  derived accessor. **The layout below is superseded accordingly.**

---

## D-006 — UFCS and method-call syntax are retained — **SETTLED**

**UFCS was never removed.** `SPEC_GAPS_AND_AMBIGUITIES.md` §3's resolution —
"Nitpick does NOT support object-oriented method calls (no `s.trim()`)... The `.`
operator is strictly for struct field access" — is **incorrect** and is struck.
It appears to have been introduced during that document's authoring rather than
reflecting an actual design change.

Method-call syntax stands as used throughout the specs and existing code:

```nitpick
Handle<int64>:h  = my_arena.alloc();
int64:val        = my_arena.get(h) ?! 0i64;
int32:prev       = counter.fetch_add(1i32);
Handle<int64>:h2 = app.my_arena.alloc();   // chained through an embedded field
```

### What actually changed

Only one thing: **`.` now handles all member access**, replacing the former split
where `->` meant dereferencing member access and `.` meant plain struct access.
`.` auto-dereferences when the operand is a pointer.

Two reasons, both from the **blueprint philosophy**:

1. **The arrow pointed the wrong way.** In Nitpick's notation, direction is
   semantic: `->` points *to* a target (pointer type), `<-` brings a value *back*
   (full dereference), `=>` goes *from* one type *to* another (cast). Member
   access brings data *toward* the reader, so an away-pointing arrow contradicted
   the operation it named.
2. **The distinction bought nothing.** "Is it `->` or `.` here?" was one more
   thing to remember with identical intent and identical outcome either way.

`->` is now **exclusively a type-position marker** (`int32->:ptr`).

### Consequences

- `PRE_PLANNING_REVIEW.md` §3.2 is resolved: arenas and `atomic<T>` keep method
  syntax; nothing needs respec.
- This matters more after D-003, which made arenas the primary safe mechanism for
  graph-shaped data. Their entire documented surface is method-based.
- The parser must support **chained member access through embedded struct fields**
  with automatic field-pointer offset computation (`FORMAL_DRAFT/09` §9.5.3).
- `SPEC_GAPS_AND_AMBIGUITIES.md` §3 needs correcting. Treat that document with
  suspicion generally — see the note in `PROTOTYPE_DELTA.md`.

---

## D-007 — Division by zero: taint or trap — **SETTLED**

**Recommendation: type-directed. `tbb` types enter sticky ERR and keep flowing
(fail-operational). Non-`tbb` types trap to `failsafe` (fail-stop).**

### Reframing the question

"Trap" does not mean "crash." `failsafe` is a **user-written controlled-shutdown
handler** — it receives the error code and is the only place besides `main` that
may call `exit`. For a robot, `failsafe` is where the arm parks, the gripper
releases safely, and *then* shutdown happens. So the trap path is already
graceful at the application level.

The genuine question is narrower: should the *arithmetic operation itself* yield
a value that keeps flowing, or should control transfer immediately?

### The two hazards

- **Taint's hazard is silent propagation into a decision.** A tainted value that
  reaches an actuator command produces a *wrong action*, which is worse than no
  action. `ok()` is supposed to prevent this, but that is a discipline, and
  disciplines fail under pressure.
- **Trap's hazard is losing the physical wind-down** — which `failsafe` already
  provides.

That asymmetry favors trapping by default.

### Why not a single global answer

Both behaviors are genuinely wanted, in different code. A control loop mid-motion
wants to degrade; a configuration parse wants to stop. Nitpick **already has the
mechanism to express that**: `tbb` types carry a sticky ERR state on overflow per
`SPEC_GAPS` §4. Extending sticky ERR to division by zero costs no new machinery
and gives the fail-operational path a *type*, rather than a convention.

| Operand type | Div-by-zero behavior | Intended for |
|---|---|---|
| `tbb8/16/32/64` | **sticky ERR**, propagates, checkable | control loops, actuator paths, anything that must degrade rather than stop |
| `int32`, `uint64`, … | **trap to `failsafe`** | ordinary code, parsing, setup, tooling |
| `flt32`, `flt64` | **±inf/nan, no trap** (IEEE; corrected by D-143 — this row once said trap, but float division is total by construction and D-037's overflow reasoning extends to it) | numerics |

### This is not a blueprint violation

The behavior is selected by **type**, not by context, and the type is written
explicitly at every declaration — `tbb32:x` versus `int32:x` is visible at a
glance. The operator `/` always means divide; what differs is the contract the
programmer deliberately chose, exactly as `wild` versus managed is an explicit,
visible opt-in. Nothing changes meaning based on surrounding circumstances.

It also matches an existing precedent: `SPEC_GAPS` §4 already states that
developers using plain `int32` "accept the risk of standard overflow," while
`tbb` types degrade safely. D-007 makes division consistent with overflow rather
than special-casing it.

### Supporting rule

Add an `--extra-picky` rule (alongside the prototype's `no-wild`) that **requires
`tbb` types for arithmetic in designated real-time / robotics code**. That makes
the fail-operational path a *compile-time guarantee* in the code that needs it,
rather than a coding convention. This is how the Nikola-priority requirement gets
enforced mechanically.

### Do not broaden `unknown`

The prototype's user-writable `unknown` (`int32:val = unknown;`) should **stay
removed**. `tbb` sticky ERR covers degraded computation better — it is typed and
scoped, rather than a universal taint that any value can carry — and keeping both
would leave two mechanisms for one job, which is the blueprint violation D-003
rejected for memory. `unknown` remains as `TYPE_REFERENCE.md` §27 defines it:
compiler-assigned taint on `Result.value` after `fail()`.

### Dependency

This **escalates the open `tbb` sticky-ERR encoding question**
(`PRE_PLANNING_REVIEW.md` §2.5, D-005 follow-up). Sticky ERR moves from an
overflow detail to the primary fail-operational safety mechanism for the robotics
path. How ERR is represented in `tbb8`/`16`/`32`/`64`, how it survives arithmetic,
and how it is tested must be nailed down before either `Result` lowering or
arithmetic lowering is implemented.

---

## D-008 — `tbb` sticky ERR encoding — **SETTLED**

Resolves the open item from D-005 and `PRE_PLANNING_REVIEW.md` §2.5. After D-007
this is the primary fail-operational mechanism on the robotics path, so it is
specified in full.

### 1. Representation

`tbbN` lowers to `iN`. The **most negative two's-complement value is reserved as
the ERR sentinel** and is excluded from the numeric range. What remains is
symmetric about zero — the "balanced" in Twisted Balanced Binary.

| Type | LLVM | ERR sentinel | Valid numeric range |
|---|---|---|---|
| `tbb8` | `i8` | `-128` | `-127 .. 127` |
| `tbb16` | `i16` | `-32768` | `-32767 .. 32767` |
| `tbb32` | `i32` | `-2147483648` | `-2147483647 .. 2147483647` |
| `tbb64` | `i64` | `INT64_MIN` | `-(2^63-1) .. 2^63-1` |
| `tbb128` | `{i64, i64}` | `INT128_MIN` | `-(2^127-1) .. 2^127-1` |
| `tbb256` | `{i64 x 4}` | `INT256_MIN` | `-(2^255-1) .. 2^255-1` |

> **Spec bug to fix:** `TYPE_REFERENCE.md` §6 lists `tbb8` as `-128..127
> (balanced)`. That range is the asymmetric two's-complement one and contradicts
> the sentinel rule stated two lines below it. Correct ranges are above.

### 2. Why balanced — the design pays for itself

Excluding the most negative value makes **negation and absolute value total**:

- `abs(x)` is representable for every valid `x`. In plain two's complement
  `abs(INT8_MIN)` overflows.
- `x * -1` is representable for every valid `x`. In plain two's complement
  `-128 * -1` overflows.
- `INT_MIN / -1` — which **faults in hardware** on x86 — cannot arise, because
  `INT_MIN` is ERR and is rejected by the operand pre-check.

An entire family of asymmetry bugs is eliminated structurally rather than by
runtime checks.

> **Amended at 1.0.9:** the checker refused unary `-` on a `tbb` (its
> `type_is_numeric` excluded the type) while this section makes negation
> total on it — found by the 1.0.8 audit. Unary minus on a `tbb` is admitted,
> typed as the same `tbb`, and lowered as `0 - x` under the sticky rule (an
> ERR operand yields ERR), which is what §1's encoding makes total.

### 3. Stickiness — ERR is absorbing, and it beats identities

**The total rule: any operation on an ERR value yields ERR.** No exceptions among
value-producing operations, and **this overrides mathematical identities**,
deliberately:

| Expression | Result | Note |
|---|---|---|
| `ERR + x` / `x + ERR` | `ERR` | |
| `ERR * 0` | **`ERR`** | *not* `0` — annihilation must not launder a taint |
| `ERR - ERR` | **`ERR`** | *not* `0` |
| `ERR / x`, `x / ERR` | `ERR` | |
| `x / 0` | `ERR` | per D-007 |
| `x % 0` | `ERR` | |

The rule to state in the spec: **once a value is ERR, no arithmetic can produce a
non-ERR result from it.** Only an explicit check (§5) or clearing operation can
leave the ERR state. If an identity could erase ERR, sticky propagation would be
defeated by ordinary algebra — which is exactly the silent-laundering failure
D-007 exists to prevent.

#### 3.1 Reconciling the total rule with comparison

"Any operation yields ERR" is well-defined only where the **result type can
represent ERR**. Comparison produces `bool`, which has exactly two values and no
third state (D-005), so the total rule cannot apply there — there is no ERR
`bool` to return.

The two rules therefore partition cleanly by result type, with no overlap and no
gap:

| Result type | Rule |
|---|---|
| `tbb` (can represent ERR) | §3 total rule — yields ERR |
| `bool` (cannot represent ERR) | §5 — traps to `failsafe` |

This is the only place the total rule needs a companion rule, and it is forced by
the type system rather than being an exception carved out for convenience.

### 4. Overflow

Any arithmetic result outside the valid range becomes ERR. `tbb` **saturates to
ERR rather than wrapping**.

Convenient consequence: out-of-range results land on the sentinel bit pattern
naturally (`-127 + -1 = -128 = ERR`), so detection and representation coincide.

### 5. Checking and control flow — ERR flows through data, traps at control

`bool` has exactly two values and no third state (D-005), so a comparison
**cannot** return an ERR-carrying boolean. That forces the question of what
`a < b` means when `a` is ERR.

**Rule: comparison or branching on an ERR value traps to `failsafe`.**

Rationale: a comparison is a *decision point* — precisely where a tainted value
would steer control flow and produce a wrong action, the hazard D-007 identified.
Trapping there gives the exact split wanted: ERR propagates freely through pure
arithmetic (fail-operational), but can never silently steer a decision.

Rejected alternative: NaN-style "all comparisons false." It breaks trichotomy
(`!(a < b)` would not imply `a >= b`), which is both a reasoning hazard for
developers and a serious obstacle for Z3.

Non-trapping ways to inspect an ERR value:

| Form | Returns | Behavior |
|---|---|---|
| `is_err(x)` | `bool` | never traps — the canonical guard |
| `x ? default` | `T` | safe unwrap; substitutes the default when ERR |
| ~~`ok(x)`~~ | — | REMOVED (see D-096's own retelling below): `ok()` tested a user-writable `unknown` that no longer exists; the taint discipline is `is_error` + `raw`. Row kept struck-through by the 0.9.8 doc-sync so a reader is not sent to build it. |

### 5.1 `pick` and the `ERR:` arm

`AST_REFERENCE.md` §2 documents an `ERR:` match label on `PICK_CASE` with no
explanation elsewhere. **This is that mechanism** — flagged as unexplained in
`PRE_PLANNING_REVIEW.md` §4.1 and now resolved:

```nitpick
pick (x) {
    ERR: { /* handle the tainted case */ },
    (0i32) { ... },
    (*)    { ... }
}
```

A `pick` on a `tbb` selector with an explicit `ERR:` arm handles the taint
without trapping. Without one, an ERR selector traps per §5. Given `pick` must be
exhaustive (`SPEC_GAPS` §4), **an `ERR:` arm should be required for `tbb`
selectors** — `(*)` must not silently absorb ERR, or the taint steers a branch
after all.

### 6. Casts — the width problem

**A cast involving `tbb` is never a straight bit operation.** ERR is a
*different bit pattern at every width*, so sign-extension and truncation both
give wrong answers.

| Cast | Rule |
|---|---|
| `tbbN => tbbM`, widening | if ERR → target's ERR sentinel. Otherwise sign-extend. **Never sign-extend the sentinel** — `tbb8` ERR (`-128`) sign-extended is a *valid* `tbb32` value. |
| `tbbN => tbbM`, narrowing | if ERR → target's ERR. Else if out of target's valid range → target's ERR. Else truncate. |
| `tbb => intN` | ERR has no representation in a plain integer. **Traps to `failsafe`** unless guarded by `is_err`/`ok`. |
| `intN => tbb` | a source value equal to the target's ERR sentinel would forge a taint. **Traps** on that value; otherwise range-check as above. |
| `tbb => flt` | ERR traps; otherwise convert. |

`=>` remains compile-error-on-possible-loss per `SPEC_GAPS` §2; `=>!` opts out
and, for `tbb`, must still be defined — recommend `=>!` preserve the ERR *state*
rather than the bit pattern, so unchecked casts cannot launder a taint either.

### 7. LLVM lowering sketch

Per operation on `tbbN`:

1. Pre-check both operands for the sentinel; if either matches, yield the
   sentinel and skip the operation. This also removes the `INT_MIN / -1`
   hardware fault by construction.
2. Perform the operation with the overflow-checked intrinsic
   (`llvm.sadd.with.overflow.iN`, `ssub`, `smul`).
3. If the overflow flag is set **or** the result equals the sentinel, yield the
   sentinel.
4. Division and modulo additionally check for a zero divisor first.

`tbb128` / `tbb256` are multi-limb; sentinel comparison and propagation operate
on the full multi-limb value, not per-limb.

### 8. Verification modeling

Model `tbbN` in Z3 as an `iN` bitvector with the predicate `valid(x) ⟺ x ≠ ERR_N`.
This gives contracts a clean vocabulary — `ensures !is_err(result)` becomes a
provable obligation, and `limit<Rules>` constraints compose with it.

Worth noting this is *easier* to verify than a wrapping integer, since the
overflow case has an explicit in-band representation instead of being undefined
or modular.

### 9. Open sub-questions

- **Default initialization.** `calloc` zeroes memory, and `0` is a valid `tbb`
  value. Should a declared-but-unassigned `tbb` default to `0` or to ERR?
  Definite-assignment analysis (`FORMAL_DRAFT/09` §9.6.4) already rejects reads
  before writes, so `0` is probably fine — but ERR-by-default would be the more
  paranoid choice and is worth a deliberate decision.
- **`tbb128` / `tbb256` arithmetic** has no hardware overflow intrinsic and needs
  a specified multi-limb algorithm.
### 10. Bitwise operators are banned on `tbb` — **SETTLED**

`&`, `|`, `^`, `~`, `<<`, and `>>` are **rejected by the type checker** on all
`tbb` widths. Applying them is a compile error, not a runtime condition.

Two reasons:

1. **They break the sentinel invariant in both directions.** Bit manipulation can
   *fabricate* the ERR pattern out of valid operands (`~127i8` is `-128`, the
   `tbb8` sentinel) and can *destroy* it out of an ERR operand (`ERR & 0` is
   `0`) — laundering a taint into a plausible number. Neither is detectable after
   the fact.
2. **A semantic type does not inherit every operation its representation
   permits** — the D-005 principle that gave `bool` and `char` restricted
   operation sets. A `tbb` is a balanced safe numeric, not a bag of bits, even
   though it is stored in an `iN`.

If bit manipulation is genuinely needed, cast to the corresponding plain integer
first (§6 — traps if ERR, so the taint cannot cross silently), manipulate there,
and cast back with the range check. That path is explicit and greppable, which is
the standard Nitpick treatment for an operation that suspends a guarantee.

---

## D-009 — `?!` takes exactly one argument — **SETTLED**

```nitpick
int32:val = read_file() ?! 99tbb32;   // on error: failsafe(99tbb32)
```

`?!` triggers `failsafe` on a failed unwrap, and `failsafe` has the fixed
signature `func:failsafe = int32(tbb32:err)` — exactly one argument. `?!`
therefore takes exactly one, and it is the error code handed to `failsafe`.
The argument is typed `tbb32` to match.

> The example was written `?! 99i32` until 1.0.8, and the checker refuses that
> spelling — the argument is a `tbb32`, as the paragraph above says, and an
> unsuffixed or `i32` literal is not one (D-092 has no conversion). The 1.0.8
> message audit found the checker's advice for it pointing at this example;
> the example is corrected rather than the rule relaxed.

Resolves `PRE_PLANNING_REVIEW.md` §3.3. Three sources already agreed
(`TYPE_REFERENCE.md` §11.2, `AST_REFERENCE.md` §3 `EmphaticUnwrapExpr`, prototype
`safety_systems_specs.txt` §2.2); the derivation from `failsafe`'s arity settles
it independently.

**Corrections owed:**

- `OP_REFERENCE.md` §5 shows the niladic form `val = fn() ?!;` — **wrong**, strike it.
- `MEMORY_REFERENCE.md` §5.2 shows `my_arena.get(h) ?! 0i64`. This is *syntactically*
  valid but almost certainly not the intent — in context it reads as a fallback
  value, which is `?`, not `?!`. As written it means "on error, call
  `failsafe(0)`". Change to `? 0i64` if a default was meant.

**Edge case, permitted:** passing a `tbb32` that is itself ERR to `?!` is legal
and meaningful — `failsafe` receives a tainted error code, i.e. "the failure code
could not itself be computed." Do not reject it.

---

## D-010 — `tbb` default initialization — **SETTLED**

**No implicit default value. Definite-assignment analysis is the guarantee.**

### Can an ERR taint be cleared? Yes — by assignment

The question that prompted this: if a `tbb` defaulted to ERR, could it ever
become useful again? It could, because **D-008's stickiness governs computation,
not storage**:

```nitpick
tbb32:x;            // no value yet
x = 5i32;           // assignment REPLACES — x is now 5, no taint
x = x + 1i32;       // read-then-compute — if x were ERR, stays ERR
```

ERR propagates through *data flow* (reads feeding operations), never through
*rebinding* (writes). So ERR-by-default would not be a trap door. Worth stating
explicitly in the spec, since "sticky" invites the opposite reading.

### Why no implicit default anyway

Definite-assignment analysis (`FORMAL_DRAFT/09` §9.6.4) already **rejects at
compile time any path that reads a variable before writing it**. That guarantee is
total for ordinary code, costs nothing at runtime, and is strictly stronger than
any default value — a default makes an uninitialized read *defined*, whereas the
analysis makes it *impossible*.

ERR-by-default was tempting as defense in depth, but it fails on cost and on
consistency:

- **Cost.** Zero pages come free from the OS via demand-zero mapping. Filling
  with the ERR pattern is an O(n) pass over memory. For Nikola's large numeric
  arrays that is a real, recurring expense for a case the compiler already
  proves cannot happen.
- **Consistency.** Making scalars default to ERR while large arrays default to
  zero (to keep the free zero pages) would be behavior that changes with context
  — the blueprint violation. One rule, applied everywhere, is: there is no
  implicit default.

### Where the guarantee does not reach, and the opt-in

Definite-assignment cannot see writes it does not control: memory from
`alloc()`, `wild` memory reinterpreted as `tbb`, or buffers filled by `extern`
calls. Those are already explicit opt-out territory (`wild`, `extern`, `raw`),
consistent with how every other Nitpick guarantee suspends.

For the case where ERR-poisoning *is* wanted — a buffer to be partially filled
where unwritten slots must be detectable — provide an explicit
`err_fill(buf, count)` builtin. Paying the O(n) cost becomes a deliberate,
greppable choice rather than a silent default.

**Note:** `calloc` on a `tbb` array yields `0`, a valid value, not ERR. That is
correct and intended — the programmer chose `calloc`, whose contract is
zero-initialization.

---

## D-011 — LLVM runtime-symbol policy — **SETTLED**

**Discovered while specifying `tbb128`/`tbb256`. This is broader than wide
arithmetic and affects the whole backend.**

### The finding

LLVM silently emits calls to **compiler-rt / libgcc / libc** for operations that
look purely native in IR. Measured with the installed LLVM 20.1.2 (`llc -O2`,
x86_64):

| IR operation | Emits | Dependency |
|---|---|---|
| `sdiv i128` | `__divti3` | compiler-rt |
| `udiv i128` | `__udivti3` | compiler-rt |
| `srem i128` | `__modti3` | compiler-rt |
| `llvm.memcpy` (large) | `memcpy` | libc |
| `llvm.memset` (large) | `memset` | libc |

**Every one of these is a zero-dependency violation** (see `CLAUDE.md`, "The hard
constraint"), and `memcpy`/`memset` arise from ordinary **struct copies and
zero-initialization** — which a compiler does constantly. This is not an exotic
corner.

### Counterintuitive detail worth knowing

Width does not predict the problem — **the presence of a libcall does**:

| Operation | Result |
|---|---|
| `add`/`mul i128` | 5 / 8 instructions, no call |
| `sdiv i128` | **libcall to `__divti3`** |
| `add`/`mul i256` | 10 / 58 instructions, no call |
| `sdiv i256` | **287 instructions, fully inline, no call** |

The *narrower* type is the problem case, because LLVM has a libcall for exactly
128-bit division and none for 256-bit, so it expands the wider one inline. Any
assumption that "wider is worse" is backwards here.

`llvm.sadd.with.overflow.i128` lowers natively in 6 instructions — so **D-008's
overflow-checked add/sub/mul work at 128 and 256 bits with no dependency**.

### Recommended policy

1. **Lower `tbb128`/`tbb256` to LLVM `i128`/`i256`.** The `{i64,i64}` and
   `{i64 x 4}` in `TYPE_REFERENCE.md` §6 describe storage layout, which LLVM's
   native wide integers already match. Do not hand-roll multi-limb add/sub/mul —
   the intrinsics are correct, dependency-free, and better than hand-written
   sequences.
2. **Provide a Nitpick-native runtime for the libcall set.** At minimum
   `__divti3`, `__udivti3`, `__modti3`, `__umodti3`, `memcpy`, `memset`. These
   are TCB code and need the same verification treatment as everything else.
   `libn` may already cover `memcpy`/`memset` — check before duplicating.
3. **Enforce it mechanically.** After codegen, scan every object for undefined
   external symbols and **fail the build** on anything outside an explicit
   allowlist of self-provided runtime symbols. This converts the zero-dependency
   rule from an aspiration into a checked invariant, and it will catch the next
   instance of this automatically instead of at link time on a target where the
   symbol happens to be missing.
4. **Measure `i256` division before shipping it.** 287 inline instructions is
   correct and dependency-free but expensive; a purpose-written routine may beat
   it. Do not assume either way.

### Why this matters beyond `tbb`

The same mechanism will fire for float↔int conversions on some targets, wide
shifts, and `__stack_chk_fail` under stack protectors. The policy above needs to
exist as **backend architecture**, not as a `tbb` footnote — recommend a
dedicated chapter in the eventual compiler spec.

---

## D-012 — `libn` pointer convention at the API boundary — **SETTLED**

Blocking decision for the `libn` port (`meta/ASSET_REVIEW.md`, concern 1). 514 of
~560 public functions are declared `= int64(...)`, using bare integers as
addresses, while the internals use typed pointers throughout.

### This is not a new convention — it is conformance

`BUILTIN_REFERENCE.md` §1 **already specifies** the allocation intrinsics:

| Builtin | Specified signature |
|---|---|
| `alloc` | `int64:size → wild int8->` |
| `calloc` | `(int64:count, int64:size) → wild int8->` |
| `ralloc` | `(wild any->:ptr, int64:new_size) → wild int8->` |
| `dalloc` | `wild any->:ptr → void` |

`libn`'s allocator is therefore already **out of spec**, not merely
unconventional. The port brings it into line rather than inventing a rule.

### Why bare `int64` addresses cannot survive the new design

This is not a style preference. Three of today's decisions are **actively
defeated** by address-as-integer:

1. **Leak checking stops working.** The K-semantics rule traps to `failsafe` if
   unmanaged memory is live at `exit`, which requires the compiler to know which
   values are `wild` allocations. An `int64` is indistinguishable from a count or
   an index — there is nothing to track. The safety mechanism is erased by the
   type erasure.
2. **D-004's escape rules cannot apply.** Second-class borrow analysis operates on
   pointer and borrow types. An integer carrying an address is invisible to it.
3. **Verification loses the vocabulary.** Z3 cannot reason about pointer validity
   for a value typed as an ordinary integer.

It is also a direct **blueprint-philosophy** violation: an `int64` that is
secretly an address means the type does not say what the thing is, and the reader
must recover that from context.

### Classification scheme for the port

Each `int64` return must be sorted into one of three categories. They are not
interchangeable and the distinction is currently invisible in the source.

| Category | Example (current) | Becomes |
|---|---|---|
| **Address** | `libn_mem_malloc = int64(int64:n)` | `wild int8->` (untyped memory) or a typed pointer where the type is known |
| **Size / count** | `mem_malloc_user_size = int64(int64:ptr)` | stays `int64` — genuinely a number |
| **Status / error** | `mem_free = int64(int64:ptr)` | `NIL`, with failure carried in `Result.err` |

Parameters take the same treatment: `mem_free = int64(int64:ptr)` becomes
`mem_free = NIL(wild int8->:ptr)`.

### The status category is a double encoding

Worth calling out separately. Every function already returns `Result<T>`
implicitly — the declared type is the *success* type. So a function whose `int64`
return value **is** a status code encodes failure twice: once in `Result.err`
and once in the value, with no rule saying which wins.

`mem_free` is the clearest case. Under the universal `Result` rule it should
return `NIL` — meaning `Result<NIL>` — with any failure in the error field.
Callers then use the same `?` / `?!` / `drop` vocabulary as everywhere else
instead of comparing a magic integer.

This affects more of the port than the address change does, and it is the part
most likely to be missed, since the current code *looks* correct.

### Sequencing

Do this **before** porting, mechanically, across the whole of `src/`. It touches
nearly every public signature; discovering it partway through means reworking
call sites twice.

### Related follow-ups from `ASSET_REVIEW.md`

- Add `wild` qualifiers to allocator returns (same underlying gap as above).
- Rename `fix256_t` / `tfp64_t` per `SPEC_GAPS` §3 (`tfp` plain, `dim` for
  dimensional analysis).
- Audit `tbb` cast sites against D-008 §6 — width changes and `tbb`↔integer
  conversions are no longer straight casts. No bitwise violations were found.

---

## D-013 — Exactly one `failsafe` per program, supplied by the executable — **SETTLED**

**Libraries do not define `failsafe`.** It is required only for executables and
must be provided by the end user. There is never more than one in a program.

### Why multiple handlers cannot work

Allowing a library to ship its own handler alongside the application's creates a
chain of responsibility, and every question it raises is unanswerable in a way
that matters here:

- **Ordering** — library first or application first? Either choice is wrong for
  some pairing.
- **Partial state** — if the library's handler frees its resources first, the
  application's handler may then reference freed state during the exact wind-down
  where correctness matters most.
- **Exit conflict** — `failsafe` and `main` are the only places `exit` may be
  called. Two handlers means two claimants on a single, unrepeatable action.
- **Re-entrancy** — a handler that itself traps has nowhere to go.
- **Verification** — "the emergency path" stops being a single analyzable
  function and becomes an ordering-dependent composition.

One handler, one meaning, no ordering to remember: the **blueprint philosophy**
answer and the safety answer coincide.

### How libraries clean up instead

The two mechanisms are not competitors — they operate at different scopes:

| Mechanism | Scope | Who uses it |
|---|---|---|
| `defer` | per-scope, composable, many per program | **libraries** and application code alike |
| `failsafe` | whole-program, singular | **executables only** |

A library manages its own resources with `defer`, which composes cleanly because
each block is bound to a lexical scope. `failsafe` is reserved for the
application's emergency *policy* — park the arm, release the gripper, then exit —
which is inherently application-specific and cannot be delegated to a library.

The K-semantics rule already supports this: `failsafe` is permitted to clean up,
and the global allocation registry (`<wildx-states>`) gives it what it needs to
do so without library cooperation.

### Consequence for `nlibc`

`ARCHIVE/libn/src/failsafe.npk` (4 lines) **is not ported**. If it exists to
satisfy a link requirement during standalone library testing, that belongs in the
test harness, not the library.

### Follow-on question this forces — **OPEN**

**Does `defer` run when control traps to `failsafe`?** The specs say `defer`
executes on "every exit path (including early `return`, `pass`, `fail`, or
`exit`)" but do not say whether a trap via `!!!` or `?!` counts.

Recommendation: **`defer` does not run on a trap.** Enter `failsafe` directly,
with the allocation registry intact.

- **Deterministic** — no arbitrary user code executes between the fault and the
  handler. `failsafe`'s precondition becomes "the registry is complete" rather
  than "some unknowable subset of defers ran."
- **Fast** — a robot needing to park does not want to unwind an arbitrarily deep
  stack of cleanup blocks first.
- **Safer** — the fault may have corrupted precisely the state a `defer` block
  depends on. Running cleanup in a known-bad state can worsen the situation.
- Cleanup remains possible: `failsafe` has the registry, and the K-semantics rule
  already anticipates it doing exactly this.

This needs settling before `defer` lowering is implemented.

---

> **Enforced at 1.5.1b step 1b (2026-09-03; DEF-5, the workbench's O-N11).**
> Until then a root that declared `main` and no `failsafe` compiled at exit 0:
> the emitter wrote every trap path as a call to `@npk_failsafe` into nothing
> and left `llc` to refuse the result — and the reach analysis RETURNED EARLY
> without a handler, so D-179's arm contract (REACH-002) was asked of programs
> that had one and of nothing that had none; deleting the handler discharged
> it. The reach analysis now refuses at `main` (`NITPICK-REACH-003`), naming
> the identities the absent handler would have to name — the set it had just
> computed at the line it used to return from. A root with neither `main` nor
> `failsafe` is a library checked alone and stays legal; a `failsafe` in any
> module but the root is D-248's `RESOLVE-013` (1.5.1b step 1).

## D-014 — `defer` does not run on a trap; `failsafe` requirements — **SETTLED; D-163 adds what a `defer` BODY may do (checked since 1.1.0)**: `fail`/`relay` are refused inside one (cleanup runs on an exit already decided), and a cleanup call that can fail is handled in the body with `?!`, an `is_err` branch, or an explicit `?| NIL`**

Resolves the open follow-on from D-013.

### `defer` does not execute when control traps to `failsafe`

`!!!` and `?!` transfer control **directly** to `failsafe`, without unwinding.
`defer` blocks run on normal exit paths only — scope exit, `return`, `pass`,
`fail`, `relay` *(added by D-080)*, and `exit` — never on a trap.

**Rationale.** At trap time the state of the system is unknown, including *how
degraded it is*. Running arbitrary cleanup code first means executing against
state the fault may have corrupted, in an order nobody chose, before the handler
that actually understands the situation gets control. The person who knows the
real conditions is the application author, and they must be able to specify
exactly what happens and when. Anything else is the compiler making assumptions
it has no basis for.

Secondary benefits: entry to `failsafe` is deterministic (no arbitrary code
between fault and handler), and it is fast — a robot that needs to park should
not unwind an arbitrarily deep stack of cleanup blocks first.

Cleanup remains possible: `failsafe` receives the global allocation registry
(`<wildx-states>`) intact, and the K-semantics `exit` rule already anticipates the
handler doing exactly this.

### `failsafe` must not assume a healthy system

Because the trap may fire in an arbitrarily degraded state, a `failsafe` handler
**cannot assume it has full system access**. It may not be able to allocate, open
files, or reach hardware it could reach moments earlier.

The guidance for Nitpick programs — particularly those driving robotics — is to
**preallocate whatever the shutdown path needs**, before the fault, and have it
standing ready. A handler that allocates during emergency shutdown may fail at
exactly the moment failure is least acceptable.

This cannot be enforced in general, since what a given program needs is
case-specific.

### What *can* be enforced

Three checkable requirements on `failsafe`:

1. **It must exist.** Already required for every executable (D-013).
2. **Its body must not be empty.** An empty handler is a stub that silently
   converts every trap into an unhandled exit. Compile error.
3. **It must return a positive value.** Reaching `failsafe` means something
   failed, so returning `0` — conventionally success — is a contradiction.
   Negative values are reserved for system errors by the `Result.err`
   convention.

Requirement 3 has a natural implementation: `failsafe` carries a
**compiler-injected `ensures result > 0i32` contract**, verified by Z3 through
the existing Design-by-Contract machinery. No new mechanism is needed — literal
returns are checked trivially, computed ones go to the solver, and a handler that
can return zero on some path fails verification with a counterexample.

### Suggested `--extra-picky` rule

Add a rule that **rejects allocation inside `failsafe`** — `alloc`, `calloc`,
`ralloc`, and arena growth. This partially enforces the preallocation discipline
above for code that opts in, in the same spirit as `no-wild`. It cannot catch
everything (a called function might allocate), but it catches the common and most
dangerous case directly.

---

## D-015 — Runtime symbols start as hand-written LLVM IR — **SETTLED**

The D-011 symbol set is implemented as **hand-written LLVM IR at an early rung of
the capability ladder**, then replaced with better implementations at a later rung.

| Rung | Implementation |
|---|---|
| early | hand-written LLVM IR — simple, correct, dependency-free |
| later | optimized Nitpick or tuned IR, once the compiler can express it |

### Both bootstrap gotchas dissolve

`PORT_PLAN.md` §4 raised two blockers. **Measured against LLVM 20.1.2, neither
survives this approach:**

1. **Self-reference — does not occur.** The concern was that a byte-copy loop
   would be pattern-matched back into a `memcpy` call, making the implementation
   call itself. Tested: a byte-copy loop inside a function literally named
   `memcpy` (and likewise `memset`) is **not** transformed at `-O2`. LLVM guards
   against converting a loop into a call to the function containing it. No
   `nobuiltin` attribute was required.
2. **Symbol binding — not needed.** The concern was that LLVM emits calls to
   literally `memcpy` while `nlibc` exports `mem_memcpy`. Writing the
   implementation directly in IR as `define ptr @memcpy(...)` **defines that
   symbol outright**, so no export-name attribute or intrinsic table is required
   at this rung.

Note the second gotcha *returns* at the later rung, when a Nitpick-authored
function needs to claim the symbol. Deferring it is exactly what the capability
ladder is for — it is not being solved now, and does not need to be.

### Performance is acceptable at the early rung

At `-O2` LLVM vectorizes the naive byte loop to 4-byte vector loads and stores on
its own. The simple implementation is not a byte-at-a-time crawl.

### Initial symbol set

`memcpy`, `memset`, `__divti3`, `__udivti3`, `__modti3`, `__umodti3`.

Pair with D-011's **undefined-symbol build check** so any newly-emitted runtime
call fails the build rather than surfacing at link time on some target.

### Borrowing from the prototype

The prototype (`../nitpick/`) is a legitimate source of *ideas and algorithms* for
these routines. **Anything taken from it must be rewritten C/C++-free** — the
prototype is heavily dependency-laden and its implementations frequently bottom
out in C. Consult it for approach, never copy through.

---

## D-016 — Atomics keep strict sequential consistency — **SETTLED**

The prototype's rule (`concurrency_specs.txt` §3.3) stands: high-level
`atomic<T>` methods enforce **SeqCst**, and weaker orderings (`relaxed`,
`acquire`, `release`, `acq_rel`) are reachable only through low-level compiler
intrinsics intended for core framework developers.

### Why this is right, not merely conservative

- **Weak orderings fail in the worst possible way for this project.** Misused
  relaxed atomics do not fail loudly or reproducibly — they produce intermittent
  corruption that appears on a different CPU, under load, months later. That is
  precisely the "small drift in numbers" failure class Nikola's safety case
  cannot tolerate, and it is undetectable by ordinary testing.
- **Verification tractability.** `--verify-concurrency` is a listed flag. Proving
  data-race freedom under SeqCst is feasible; under relaxed and acquire/release
  the set of permitted executions expands combinatorially. Committing to weak
  orderings in the safe API means committing to verify something dramatically
  harder.
- **Blueprint philosophy.** One ordering means `.load()` means the same thing
  everywhere, with no per-call-site ordering to remember or review.
- **The cost is bounded and local** — it falls on atomic operations specifically,
  not on all code. When profiling shows a specific atomic is a genuine
  bottleneck, the intrinsic escape hatch exists, and reaching for it is explicit
  and greppable.

### The more important concurrency question — **OPEN**

Memory ordering is not the main safety question; **whether data races are
possible at all** is. Two observations:

1. **D-004 already does much of the work.** Borrows are second-class and may not
   cross a thread spawn, so stack references cannot be shared between threads.
   That eliminates a large class of races structurally, at compile time.
2. **`Handle<T>` safety is specified for single-threaded arena access, and D-003
   made arenas load-bearing.** If two threads hold handles into the same arena,
   the generation-counter check becomes a read-check-use race, and the freelist
   and slot reuse need atomic updates. The use-after-free guarantee — which is
   the whole reason arenas replaced the collector — **does not currently extend
   to shared arenas.**

Point 2 is a genuine gap created by D-003 and needs deciding: either arenas are
single-threaded by construction (enforced how?), or arena operations become
atomic (cost on the primary allocation path), or shared arenas require explicit
synchronization at the type level. Recommend settling it alongside the
concurrency spec.

### Spec basis

Adopt `FORMAL_DRAFT/11_concurrency.md` as the base for the missing concurrency
specification rather than writing one from scratch — it exists, and the
carried-over set has no concurrency document at all despite `TYPE_REFERENCE.md`
specifying `atomic<T>` (§13) and `Future<T>` (§17).

---

## D-017 — Arenas and threads: two types, one discipline each — **SETTLED; the operation table is amended by D-152** (`put` joins `arena<T>`'s set, and `get` COPIES the element out — a borrow-returning `get` would be a returned borrow, which D-004 refuses everywhere else; the open sub-item this section left is thereby closed)

Closes the gap D-016 identified: `Handle<T>`'s use-after-free guarantee was
specified for single-threaded access, and D-003 made arenas load-bearing.

### Three races, not one

The generation counter is the obvious hazard but not the worst:

1. **Stale generation.** A reads generation and matches; B frees and the slot is
   reused; A uses the slot. Atomics fix this.
2. **Freelist contention.** A and B both pop the same freelist head. Atomics fix
   this.
3. **Growth invalidates pointers.** A holds a pointer from `get()`; B's `alloc()`
   grows the arena, reallocating the slab; A's pointer dangles. **Atomics do not
   fix this** — the memory moved. This is the one that decides the design.

### Decision

Two distinct types, each with exactly one discipline:

| | `arena<T>` | `shared_arena<T>` |
|---|---|---|
| Threading | single-threaded | multi-threaded |
| Operations | `alloc`, `get`, `free`, `reset`, `destroy` | **`alloc`, `get`, `destroy` only** |
| Per-slot `free` | yes | **no** |
| Storage | may reallocate on growth | **chunked, never moves** |
| Cost | zero | one atomic bump per allocation |
| Sharing | move-only; moving into a thread transfers ownership | shareable by reference |

### Why the shared variant is allocation-only

Removing per-slot `free` is what makes concurrency safe *without* a reclamation
scheme. If slots are never freed while the arena is live:

- generation counters never increment during concurrent access, so race 1 cannot
  arise;
- there is no freelist, so race 2 cannot arise;
- chunked storage means growth allocates a **new chunk** rather than moving
  existing ones, so race 3 cannot arise.

Allocation reduces to an atomic bump — a single `fetch_add` — with no epochs, no
hazard pointers, and no reference counting. All three of those are substantial
runtime TCB and precisely the kind of subtle concurrent code D-003 rejected a
collector to avoid.

The concurrent case therefore gets the **simpler** semantics, not the more
complex one.

### This matches the use case that justified arenas

D-003 justified arenas for cyclic graphs on the grounds that you "drop the arena
wholesale" rather than freeing individual nodes. `shared_arena<T>` makes that
justification into the type's actual contract. Nikola's knowledge graph and
ingestion layer — the concurrent, graph-shaped, cyclic workload — accumulates and
is torn down as a unit; it does not free individual nodes.

Where per-slot reuse is genuinely needed, that workload is single-threaded and
uses `arena<T>`.

### Not a blueprint violation

Two types is not two disciplines applied by context. Which discipline governs is
**written at the declaration** and visible at every use, exactly as D-007 resolved
divide-by-zero by type (`tbb` degrades, `int32` traps) rather than by
circumstance. `arena<T>` means one thing everywhere; `shared_arena<T>` means one
thing everywhere.

### Teardown

`destroy` on a `shared_arena<T>` requires that no thread still holds handles.
This is ownership, not synchronization: the owner destroys it after joining. The
same move-only discipline that keeps `arena<T>` single-threaded governs who may
destroy the shared variant.

### Open

- Chunk size policy for `shared_arena<T>` — fixed, or geometric growth.
- Whether `get()` on a shared arena returns a second-class borrow (D-004), which
  would prevent the returned reference from escaping the checking scope. Likely
  yes, and it composes well.

---

## D-018 — Closures are removed — **SETTLED**

Lambdas remain as **plain function values with no captured environment**. State
reaches them explicitly: as a parameter, or as a struct implementing a trait.

### Why the motivation is gone

Closures were added to pair with the garbage collector, so lambdas could carry
state around the way they do in JavaScript. D-003 removed the collector, and
`FORMAL_DRAFT` 6.4 shows exactly what that costs: closure environments were
allocated with `npk_gc_alloc`. With no collector, every escaping closure needs an
owner and an explicit lifetime — at which point the programmer is doing manual
lifetime management anyway, and passing context explicitly is more honest and no
more work.

### Nothing depends on them

Measured across the ecosystem:

| Source | Files mentioning lambda/closure |
|---|---|
| `ARCHIVE/libn/src` | **0** of 58 |
| `nitpick-posix/src` | **0** of 164 |
| `ARCHIVE/nstr`, `ARCHIVE/nmath` | **0** |
| `npkc-native/src` | 3 of 27 — all **compiler internals** (an AST node kind, a closure-capture analyzer) |

Closures appear in **none** of the ten carried-over topic specs — only in
`FORMAL_DRAFT` 6.4 and the prototype changelog. They are implemented but unused.

Removal therefore deletes compiler complexity (the prototype carries a 430-line
closure-capture analysis pass) and breaks no library code.

### Traits already provide stateful callbacks, explicitly

A closure is an anonymous struct with one method and a hidden lifetime. Nitpick
already has the explicit form, specified in `FORMAL_DRAFT` 13:

```nitpick
struct:Counter = { int32:count; };
impl:Handler:for:Counter = {
    func:on_event = NIL(Counter:self, int32:ev) { self.count = self.count + 1i32; };
};
```

Same capability, with a named type, a visible owner, a checkable lifetime, and a
call graph the verifier can follow.

### Additional reasons

- **Verification.** Closures obscure the control-flow graph exactly as `dyn` does
  — `FORMAL_DRAFT` 13.5.3 already warns that `dyn` "triggers compiler warnings
  when strictly auditing under `nitpick-safety` profiles". Closures carry the
  same cost without the same justification.
- **Blueprint philosophy.** A closure's environment lifetime is invisible at the
  call site: `f(x)` does not reveal whether `f` owns a heap environment.
- **TCB.** Environment allocation is one more allocation path requiring
  verification.
- **D-004 already restricts them** — rule 5 bans closures from capturing borrows.
  Removing closures makes that rule unnecessary rather than special.

### Consequences

- `FORMAL_DRAFT` 6.4 is struck; 9.7.3's fat-pointer layout keeps the `dyn Trait`
  half and drops the closure half.
- `npkc-native`'s `LAMBDA` AST node and `ClosureAnalyzer` are not carried forward.
- Function *pointers* remain — a lambda without capture is a function value and
  stays useful for callbacks, comparators, and dispatch tables.

---

## D-019 — Integer-to-pointer construction — **SETTLED**

Resolves `FORMAL_DRAFT_AUDIT.md` §5.2: 13.6.3 declares integer-to-pointer casting
illegal, which makes `nlibc` unwritable, since `mmap` returns an address that must
become a `wild int8->`.

**The general prohibition stands.** It is suspended by exactly one named,
greppable construct, legal only in `wild` context:

```nitpick
wild int8->:page = #wild_ptr<int8>(addr);
```

> **Corrected at 1.0.9:** this example was written `#wild_ptr<int8->>(addr)`
> — the pointer type as the argument — while the implementation, its
> diagnostics and `tests/grammar/whole_grammar.npk` take the POINTEE and the
> builtin adds the `->`, as its sibling `#wild_slice<int8>(p, n)` takes the
> element and adds the `[]` (D-070). One rule for the family: the type
> argument is what is pointed at or held, and the builtin supplies the shape.
> The example and `BUILTIN_REFERENCE.md`'s row are corrected; the code was
> right.

### Why a builtin rather than a cast operator

`=>>` was considered — staying in the cast family (`=>`, `=>!`) has real appeal.
It was not chosen for two reasons:

1. **This is not a cast.** `=>` converts a *value* between types; `=>!` does so
   without checking. Both preserve the thing being converted. Constructing a
   pointer from an integer **fabricates a reference to memory out of a number** —
   a categorically different operation, not a third severity level of the same
   one. Giving it a different syntactic category reflects that honestly. Note
   `!` already carries "unchecked" in this family; `=>>` would need to mean
   something other than danger while actually meaning *more* danger.
2. **Practical auditability.** A builtin is trivially restricted to `wild`
   context by the type checker and greps cleanly. `=>>` is two characters
   adjacent to `=>` on the keyboard and in the grammar.

If an operator is preferred after all, `=>>` is the right shape — but the
argument above is why the builtin is recommended.

### Naming

`#wild_ptr` reuses `wild`, which already means "unmanaged, unchecked, you are
responsible" throughout the language. It introduces no new concept, states the
danger tier in the name, and follows the `#name<T>(...)` builtin convention
(D-020).

---

## D-020 — `@` is address-of only; `#` is the compiler-directive sigil — **SETTLED**

**Every `@`-prefixed builtin in the specs is wrong.** `@` is the address-of
operator and nothing else. `@cast<T>(x)` reads as "the address of `cast<T>` of
x", which is both confusing and a direct blueprint violation — a symbol must not
change meaning by context.

This was introduced by mistake and partially corrected in the prototype; stale
usages likely survived for backwards compatibility with libraries of the time.

### Affected

`FORMAL_DRAFT` 8.1 and 13.6 carry the incorrect form throughout: `@sizeof`,
`@alignof`, `@offsetof`, `@len`, `@ptr_add`, `@ptr_sub`, `@typeof`, `@typeInfo`,
`@type_name`, `@fieldType`, `@has_field`, `@field_names`, `@is_comptime`,
`@cast`, `@cast_unchecked`, `@derive`. All must be rewritten.

*(LLVM intrinsic names such as `@llvm.coro.suspend` are LLVM's own syntax and are
not affected.)*

### The correct convention

`#` is the **compiler-directive sigil** — it marks something addressed to the
compiler rather than the runtime. Two syntactic positions, one meaning:

| Form | Purpose | Example |
|---|---|---|
| `#name<T>(...)` | builtin producing a value | `#size_of<T>`, `#wild_ptr<int8->>(addr)` |
| `#[name(...)]` | attribute annotating a declaration | `#[align(16)]`, `#[cfg(...)]` |

This matches `BUILTIN_REFERENCE.md` §4 (`#size_of<T>`) and `FORMAL_DRAFT` 8.3
(`#[align(N)]`), and mirrors how `.` handles all member access in D-006 — one
meaning, disambiguated by position rather than by context.

### `#` as the pin operator is obsolete

`OP_REFERENCE.md` §6 lists `#` as **pin** — "prevents the Garbage Collector from
moving the memory." **D-003 removed the collector**, so nothing moves memory
behind the programmer's back any more: `stack` and `wild` do not relocate, and
arena contents are reached through `Handle<T>`, which is growth-safe by
construction (D-017 additionally makes `shared_arena<T>` storage non-moving).

Pin therefore has no remaining purpose, and removing it leaves `#` with a single
coherent meaning. `MEMORY_REFERENCE.md` §2 (Pinned Memory) is struck.

---

## D-021 — One cast spelling: `=>` and `=>!` — **SETTLED**

`#cast<T>(expr)` and `#cast_unchecked<T>(expr)` are **removed from the language**.
`=>` and `=>!` are the only cast forms.

```nitpick
int32:n = large_val => int32;    // checked — compile error if data loss is possible
int8:b  = large_val =>! int8;    // unchecked — the explicit opt-out
```

### Why two spellings could not stand

`TYPE_REFERENCE.md` §28 and `FORMAL_DRAFT` 13.6.1 both documented the function
form as "semantically identical" to the operator. Two spellings for one operation
is precisely what the **blueprint philosophy** rejects: it is one more thing to
remember, one more variant to maintain, and one more inconsistency for a reviewer
to reconcile — with no benefit, since the intent and outcome are identical either
way. This is the same reasoning that collapsed `->` and `.` into a single member
access operator (D-006).

### Why the operator won rather than the builtin

1. **`#cast` was in the wrong family.** D-020 defines `#` as the
   **compiler-directive sigil** — something addressed to the compiler rather than
   the runtime. `#size_of<T>` is a genuine compile-time query; `#wild_ptr<T>` is a
   privileged construction. A cast is an ordinary operation on a value, so by
   D-020's own definition it does not belong under `#`.
2. **`=>` carries directional meaning.** Blueprint facet 2: direction is
   semantic. `=>` reads as *going from one type to another*, alongside `->`
   pointing *to* a target and `<-` bringing a value *back*. Deleting it would
   remove a member of a deliberately designed notation family.
3. **`!` already means unchecked**, consistently across `=>!`, `?!`, and `!!!`.
   The operator form gets that for free; the function form spells it out in a
   longer name.
4. **Casts are common.** Terse notation is right for frequent operations —
   the blueprint principle of maximum meaning in minimum space.

### Follow-up

- `TYPE_REFERENCE.md` §28 operator table — remove both `#cast` rows.
- `TYPE_REFERENCE.md` §1.2, §5a, §27 — rewrite remaining `#cast` usages as `=>`.
- `FORMAL_DRAFT` 13.6.1 and 13.6.2 — drop the function-style cast on adoption;
  8.1.3's bare `cast<T>` / `cast_unchecked<T>` go with them.

---

## D-022 — Counted loops: `till` and `loop` — **SETTLED**

Resolves `GRAMMAR_ADOPTION_CONFLICTS.md` A1. Both are **counted iteration**
constructs exposing the counter as `$`. `FORMAL_DRAFT` 05 §5.4.3–5.4.4, which
defines `till` as do-while and `loop` as infinite, is **wrong** and is struck.

### The two forms

| Form | Arguments | Range | Direction |
|---|---|---|---|
| `till(limit, step)` | 2 | `0` → `limit` | ascending only |
| `loop(start, limit, step)` | 3 | `start` → `limit` | **inferred** from `start` vs `limit` |

```nitpick
till(10i32, 1i32) {        // 0,1,2,…,9
    x += $;
}

loop(0i32, 10i32, 1i32) {  // 0,1,2,…,9   — ascending, inferred
    x += $;
}

loop(10i32, 0i32, 1i32) {  // 10,9,8,…,1  — descending, inferred
    x += $;
}
```

`till` is the simple, common case; `loop` is the controlled form. They are **not
redundant** — `till` counts up from zero only, while `loop` handles arbitrary
start points and both directions.

### `step` is always positive

**Direction is inferred from `start` and `limit`, so `step` only controls the size
of the jump and must be positive.** A negative step is a compile error.

This makes an entire bug class **unrepresentable**: in C-style loops, pairing an
ascending range with a negative step (or a descending range with a positive one)
produces an infinite loop, and the mistake is invisible at a glance because the
sign lives in a different clause from the bounds. Here the direction is not
something the programmer can get out of sync with the bounds, because it is not
something the programmer states at all.

It also serves the blueprint philosophy: `step` means exactly one thing — how far
to jump — everywhere it appears, rather than encoding direction in some contexts
and magnitude in others.

### Edge cases to specify

| Case | Behavior |
|---|---|
| `step` negative | **compile error** |
| `step` zero | **compile error** — the loop could not terminate. Where the step is not a literal, this becomes a verification obligation (`--verify` proves `step > 0`), falling back to a runtime check that traps to `failsafe`. |
| `start == limit` | zero iterations |
| `till` with `limit <= 0` | zero iterations — `till` ascends from `0` and can never reach a non-positive limit |
| bound is `tbb` and holds ERR | traps to `failsafe`, per D-008 §5 — a loop bound is a control-flow decision |

### Consequences

- `while (true)` is the idiom for an unbounded loop; there is no `loop { }` form.
- Nitpick has **no do-while construct**. If one is wanted it needs a distinct
  keyword — reusing `till` would give one keyword two meanings.
- `$` is well-defined in both forms, and counts *down* in a descending `loop`.
- Corroborated by `CONTROL_REFERENCE.md` §2.4, `AST_REFERENCE.md`'s
  `LOOP_STMT`/`TILL_STMT` operand slots, and `OP_REFERENCE.md` §8, which defines
  `$` as "bound inside `till` and `loop`" — `$` only has meaning for counted loops.

---

## D-023 — `for` is range-form only, with a typed binding — **SETTLED**

Resolves `GRAMMAR_ADOPTION_CONFLICTS.md` A3. The C-style three-clause form shown
in `FORMAL_DRAFT` 05 §5.4.2 is **not supported**.

```nitpick
for (int64:i in 1..3) { ... }     // the only form

// rejected:
// for (int32:i = 0; i < 10; i++)    C-style three-clause
// for (i in 0..10)                  untyped binding
```

**Rationale.** `loop` and `till` (D-022) already cover everything the three-clause
form provides, with better safety properties — so supporting both would mean two
constructs for one job, and the C-style one is the weaker of the pair. Keeping
`for` to ranges also draws a sharper line between the two: `for` iterates a
range or collection, `loop`/`till` count.

The **typed binding is required** because `FORMAL_DRAFT` 03 §3.1 forbids implicit
type inference outright — no `auto`, `var`, or `let`. The untyped `for (i in 0..10)`
form shown in 05 §5.4.2 contradicts the language's own rule.

---

## D-024 — Raw and multi-line string literals are retained — **SETTLED**

Resolves `GRAMMAR_ADOPTION_CONFLICTS.md` A4.

```nitpick
string:path  = r"C:\Users\dir";   // raw — no escape processing
string:block = """line one
line two""";                      // multi-line — preserves newlines
```

`FORMAL_DRAFT` 01 §1.6.3 states these were "intentionally omitted" because the
v0.61.82 parser errors on them. That is a statement about the **prototype's
implementation state**, not a design decision — `OP_REFERENCE.md` §9 lists both as
current. Raw strings in particular matter for regex patterns and paths, both of
which a compiler handles constantly.

The lexical grammar must therefore include `RawStringLiteral` and
`BlockStringLiteral` productions, which chapter 01 currently lacks.

---

## D-025 — No do-while construct — **SETTLED**

Nitpick has five loop forms — `while`, `for`, `loop`, `till`, `when` — and does
not add a sixth for do-while.

### `when` / `then` / `end` does **not** express it

This was considered and does not work. `when`'s body is gated on its condition:
if the condition is false initially the body never runs and `end` fires instead.
Producing "runs at least once" would mean duplicating the body into `end`, or
reintroducing a boolean flag — and eliminating exactly that kind of external
state-tracking flag is why `when` exists.

### The existing spelling is adequate

```nitpick
while (true) {
    body();
    if (!cond) { break; }
}
```

Explicit, greppable, and built from constructs that already exist. A sixth loop
form to save one line does not clear the bar the blueprint philosophy sets, and
each additional loop construct is one more thing a reviewer must know.

If a do-while is ever genuinely wanted it needs its **own keyword** — reusing
`till` would give one keyword two meanings, which D-022 already rejected.

### Related question — resolved by D-027

The `end` clause conflating "never ran" with "broke out early" turned out to be a
transcription error in the specs rather than the intended design. See **D-027**.

---

## D-026 — `..^` is the spread operator; `!!` is a modifier token — **SETTLED**

Both were listed in `FORMAL_DRAFT` 01 §1.5 with no definition anywhere. Resolved
against the prototype source rather than by inference.

### `..^` — spread

`..` `...` `..*` `..^` are **one family**, not four unrelated symbols:

| Token | Meaning | Site |
|---|---|---|
| `..` | inclusive range `[a, b]` | expression |
| `...` | exclusive range `[a, b)` | expression |
| `..*` | variadic rest marker — **collects** | declaration |
| `..^` | spread — **expands** | call |

`..*` and `..^` are inverses, which is why they share the dot prefix.

Evidence: `nitpick/src/frontend/parser/parser.cpp:2582`
(`// Check for spread operator: ..^expr`), `parser.cpp:2587`
(`"Expected expression after '..^' spread operator"`), and
`nitpick/src/frontend/ast/expr.cpp:248`. The token also exists in
`npkc-native/src/frontend/token.npk` and `nitpick-bootstrap/src/frontend/tokens.npk`.

This is why `FORMAL_DRAFT` 04 §4.2 names precedence level 8 "Range / **Spread**" —
the name was correct and the operator list simply never defined the second half.

### `!!` — a modifier token, not an operator

`nitpick/src/frontend/lexer/lexer.cpp:499` emits `TOKEN_BANG_BANG` with the
comment `// !! (sys!! modifier)`. It exists so `sys!!` and `asm!!` can be lexed
and has no standalone meaning. Listing it among operators was a category error.

`!!!` **is** a genuine operator — the failsafe abort. Note that after D-001
removed `sys!!!` and `asm!!!`, `!!!` again has exactly one meaning.

---

## D-027 — `when` / `then` / `end` semantics corrected — **SETTLED**

The specs had drifted from the intended design. Restored:

| Outcome | Clause |
|---|---|
| body ran ≥ 1 time, condition later became false | **`then`** |
| body ran ≥ 1 time, exited early via `break` | **`then`** |
| condition false initially — body never ran | **`end`** |

`then` and `end` **partition the outcomes exactly**: one always runs, never both,
and both are optional.

### What was wrong

`CONTROL_REFERENCE.md` §2.2 sent `break` to `end`, reserving `then` for normal
completion. That made `end` mean *either* "the body never ran" *or* "the loop
broke out" — two unrelated outcomes sharing one clause.

The consequence was self-defeating: distinguishing them required a boolean flag,
which is precisely the external state-tracking `when` exists to eliminate, in a
case squarely inside its remit. The construct failed at its own job.

### Why this grouping is the right one

The rule reduces to a single question — **did the body execute?** — and that is
the property which cannot otherwise be recovered without tracking it by hand.

Whether a loop *completed* versus *broke out* is normally evident from what the
body did, or from the condition that triggered the `break`. Whether it ran **at
all** is not evident from anything: on a zero-iteration loop the body leaves no
trace, which is exactly why the flag was needed.

So the split captures the information that is expensive to obtain and discards
the distinction that is cheap. It is also a clean binary partition rather than a
three-way classification with an ambiguous member — one rule, no exceptions.

### Follow-up

- `CONTROL_REFERENCE.md` §2.2 — corrected.
- `AST_REFERENCE.md`'s `WHEN_STMT` node is unaffected: `.c` remains a `GROUP_NODE`
  of up to two optional blocks (`then`, then `end`). Only the lowering changes —
  `break` must branch to the `then` block, not `end`.
- `FORMAL_DRAFT` 05 §5.4.5 mentions the clauses without defining their semantics,
  so it needs the definition added rather than corrected.

---

## D-028 — `assoc` declares associated types; `Type` is namespace-only — **SETTLED; the namespace half is superseded by D-088** (banner added by the 0.9.8 doc-sync — a targeted reader landing here must not build the dead half)

`Type` had two unrelated meanings distinguished only by position — a direct
blueprint violation, and genuinely ambiguous to parse.

| Construct | Keyword | Example |
|---|---|---|
| namespace / module grouping | **`Type`** *(unchanged)* | `Type:Counter = { … };` |
| associated type in a trait | **`assoc`** *(new)* | `assoc:Item;` / `assoc:Item = int32;` |

```nitpick
trait:Iterator = {
    assoc:Item;
    func:next = Item(Self:self);
};

impl:Iterator:for:Range = {
    assoc:Item = int32;
    func:next = int32(Range:self) { pass(self.current); };
};
```

Associated types may carry defaults — `assoc:Error = string;` — which an impl
inherits if it omits the binding.

**Why associated types moved rather than namespaces.** `Type` as a namespace is
the older and more visible construct, specified in `FORMAL_DRAFT` 02 §2.7.4 with
its own internal/interface/type struct discipline. Associated types are newer and
confined to trait and impl bodies, so renaming them disturbs less existing
material.

It also removes a real parsing ambiguity: inside a trait body, `Type:Foo = { … };`
could previously be read as either an associated type bound to an anonymous
struct or a nested namespace. `assoc:Foo = …;` cannot.

Add `assoc` to `TypeKeyword` in `LEXICAL_REFERENCE.md`.

---

## D-029 — `&` combines traits everywhere — **SETTLED**

Three sites meant "satisfies several traits" using two different symbols. `&` wins
in all of them; `+` is removed from trait combination.

```nitpick
trait:Ordered = Equatable & { … };                          // supertrait
func:process<T: Renderable & Serializable> = NIL(T:item) { … };  // generic bound
dyn Drawable & Serializable:obj = msg;                       // multi-bound dyn
```

Two of the three already used `&`, so this changes the fewest sites. `&` also
reads as logical conjunction — "satisfies A **and** B" — which is what a
constraint means, whereas `+` reads as addition.

`dyn A & B` remains assignable to `dyn A` (widening by dropping bounds), and each
trait must be object-safe.

---

## D-030 — Trait, impl, and generic declaration syntax — **SETTLED**

`FORMAL_DRAFT` chapters 06 and 13 disagreed on all three. **Chapter 13 wins
throughout**; chapter 06 is the outlier and is corrected.

### Canonical forms

```nitpick
trait:Serializable = { … };                    // declaration
impl:Serializable:for:Message = { … };         // trait impl
impl:for:Point = { … };                        // inherent impl
struct:Container<T> = { T:value; };            // generic struct
func:extract_value<T> = T(Container<T>:c) { … };            // generic function
func:process<T: Renderable & Serializable> = NIL(T:item) { … };  // bounded
```

| Construct | Chapter 06 (rejected) | Chapter 13 (adopted) |
|---|---|---|
| trait | `trait:Reader { … };` — no `=` | `trait:Reader = { … };` |
| impl | `impl Reader for FileStream { … }` | `impl:Reader:for:FileStream = { … };` |
| generic params | before the name — `func<T: …>:process` | **after** the name — `func:process<T: …>` |

Chapter 13's forms match the house style used by every other declaration —
`func:name = `, `struct:name = `, `Rules<T>:name = ` — and `SPEC_GAPS` §3
independently specifies after-the-name placement. Chapter 06 was alone.

Chapter 13 never shows a *bounded* generic, so the bound form above is written
rather than adopted: parameters go after the name, bounds attach with `:` inside
the angle brackets, and multiple bounds combine with `&` (D-029).

### `Self` becomes a keyword

Used six times in chapter 13 (`func:to_bytes = buffer(Self:self);`) but absent
from the keyword list. Add `Self` to `TypeKeyword` in `LEXICAL_REFERENCE.md`. It
denotes the implementing type inside a `trait` or `impl` body and is invalid
elsewhere.

### Blanket impls use the bound form

Chapter 13 spells blanket impls as `impl:Loggable:for:T:where:Printable = { … };`,
making `where` a colon-separated path segment. That is a second, unrelated
syntactic role for `where`, which otherwise guards `pick` arms as a parenthesized
expression — `MyMacro!(a, b) where (a > b)`.

Applying the bound syntax consistently removes the clash:

```nitpick
impl:Loggable:for:<T: Printable> = {
    func:log_str = string(T:self) { pass("[LOG]"); };
};
```

This is a **consequence** of D-029 and D-030 rather than a separate choice — the
same rule (`<T: Bound & Bound>`) now applies in every position that constrains a
type parameter. It was flagged for review because it changes a form chapter 13
states explicitly, and has been **confirmed**. Concrete impls continue to take
priority over blanket-generated ones.

### `>>` splitting

`Handle<Node<int64>>` requires the lexer to split `>>`, which is also the
right-shift operator. Chapter 13 §13.3.2 notes the behavior; it needs stating in
the lexical grammar as an explicit parser interaction rather than left implicit.

---

## D-031 — `impl` syntax: no connector, type first — **SETTLED; amended by D-171 (every impl now names its target, so the blanket examples below read `impl:<T: Bound>:T:Trait`)**

```ebnf
ImplDeclaration ::= "impl" ":" TypeOrParam (":" TraitName)? "=" "{" ImplBody "}" ";"
```

```nitpick
impl:Point = {                       // inherent methods on Point
    func:magnitude = flt64(Point:self) { … };
};

impl:Message:Serializable = {        // Message implements Serializable
    func:to_bytes = buffer(Message:self) { … };
};

impl:<T: Printable>:Loggable = {     // blanket: every Printable is Loggable
    func:log_str = string(T:self) { … };
};
```

Supersedes `FORMAL_DRAFT` 13's `impl:TraitName:for:TypeName`, `impl:for:TypeName`,
and `impl:Trait:for:T:where:Bound`.

### Why the connector goes entirely

`for` already means "iterate over" in `for (int64:i in 1..3)`. Reusing it for
"applied to" gives one keyword two meanings — the thing the blueprint philosophy
exists to prevent, and the same defect D-028 removed from `Type` and D-021 from
having two cast spellings.

Replacing it with a fresh keyword was considered and rejected. `with` was the
natural candidate but is **already taken** by the FFI error contract
(`fails on result < 0i32 with errno`, D-002), so it would recreate the same
overload. `on`, `as`, `in`, `is`, and `where` are likewise reserved. That left
`to`, `by`, and `via` — all free, but all **filler words**: they carry no
information that position does not already carry. Facet 2 of the blueprint
philosophy asks for maximum meaning in minimum space, and a connector adding zero
meaning fails that test. Dropping it is strictly better than any replacement:
same information, less to read, no vocabulary growth.

### Why the type comes first

The two forms share a rule: **slot 1 is always the type being implemented on.**

| Form | Slot 1 | Slot 2 |
|---|---|---|
| `impl:Point` | type | — |
| `impl:Message:Serializable` | type | trait |
| `impl:<T: Printable>:Loggable` | bounded type parameter | trait |

Trait-first ordering was considered and rejected for exactly this: with
`impl:Serializable:Message`, the one-segment inherent form `impl:Point` would make
slot 1 a *type* while the two-segment form made it a *trait* — the first slot
changing meaning by arity. Type-first has one rule with no exception.

It also reads as subject-predicate — "Message implements Serializable" — matching
the direction of the relationship rather than inverting it.

### Follow-up

- `TRAITS_REFERENCE.md` — all `impl` forms rewritten.
- `LEXICAL_REFERENCE.md` — `for` is no longer a path segment; it reverts to the
  loop keyword only, and the open item flagging its dual role is closed.
- `AST_REFERENCE.md` — the impl node carries an optional trait slot, not a fixed
  three-part path.

---

## D-032 — Async tasks are pinned to threads — **SETTLED**

A task resumes on the thread it suspended on. The async runtime does **not**
migrate tasks between threads, and does not work-steal.

### The framing this rejects

`FORMAL_DRAFT` 11 §11.2 has the runtime "multiplex coroutines over a configurable
pool of system threads", which permits migration. That looked like a
safety-versus-performance trade. It is not, for this workload.

**Work stealing optimizes the part of Nikola that is not the bottleneck.** The
manifold, Mamba state, tensor math, and waveform processing are CPU-bound bulk
numeric work that does not run through the coroutine scheduler at all — it
parallelizes by explicitly partitioning known-size work across threads, which
pinning does not affect. What *does* run through async is the ZMQ spine,
ingestion, and mini-VM coordination: I/O-bound and coordination work, where tasks
spend their time waiting rather than competing for cores.

### Why pinning is the safety answer

Migration does not corrupt data — a correct work-stealing runtime establishes a
happens-before edge at the suspend/resume boundary, so sequential cross-thread
access is sound. The cost is *"a correct runtime"*: it moves `arena<T>`'s
single-threaded guarantee (D-017) from a **compile-time structural property** into
**runtime TCB** that formal verification must then cover.

That is precisely the cost that decided D-003 against the collector. The same
answer follows.

Without pinning, D-017's contract would also have to be restated from
*single-threaded* to *no concurrent access*, since any task holding an arena
across an `await` would otherwise violate it — and that is the normal case, not
an edge one.

### Secondary benefits

- **Mini-VM isolation.** Each mini-VM can own one execution context plus its own
  arena, with memory that never leaves its thread. "This VM's memory" becomes a
  statement about a thread rather than a convention.
- **Cheaper coroutine frames.** Because a thread's tasks never migrate, its
  executor can allocate frames from a plain `arena<T>` — zero-cost — instead of
  `shared_arena<T>`'s atomic bump (D-034).
- **Reversible.** Pinning can be relaxed later against profiling evidence.
  Shipping migration and retrofitting pinning after a load-dependent race
  surfaces is the far worse ordering, and that failure class — intermittent,
  load-dependent, invisible in testing — is the one Nikola cannot tolerate.

### Rejected: pin by default, opt in to migration

Two scheduling disciplines means every piece of async code carries the question
"which one governs this?" The compute parallelism that would motivate opting in
is better served by explicit thread partitioning, which is a separate mechanism
rather than a second mode of this one.

---

## D-033 — `atomic_new` is removed — **SETTLED**

Atomics live in storage something else already owns. There is no allocating
constructor.

```nitpick
atomic<int32>:counter = 0i32;          // storage in the enclosing scope/struct/arena

struct:Stats = {
    atomic<int64>:hits;                // or as a field
};

atomic<int32>:lk = atomic_from_ptr<int32>(hdr_ptr);   // alias existing memory
```

`FORMAL_DRAFT` 11 §11.4.1 offers `atomic_new(0i32)` as "heap allocation". With no
collector, nothing says who frees it — a question that did not exist when the
chapter was written.

Removing the allocating form **eliminates** the question rather than answering
it: no new allocation path to verify, no ownership rule to remember, and one way
to obtain an atomic instead of two. `atomic_from_ptr<T>` already existed for
exactly the case that matters — placing an atomic over memory owned elsewhere.

Where the aliased address originates as an integer, it must be converted with
`#wild_ptr<T>(addr)` in `wild` context (D-019), and pointer arithmetic goes
through `#ptr_add<T>(ptr, offset)` — not the raw `hdr_ptr + 24i64` shown in
§11.4.1.

---

## D-034 — Coroutine frames are arena-allocated by the executor — **SETTLED**

`async` lowers to `@llvm.coro` state machines whose frames are heap-allocated so
they survive suspension (`FORMAL_DRAFT` 07 §7.4). With no collector, the frames
need an owner.

**Each thread's executor owns an `arena<T>` from which it allocates task frames,
released when the task completes.**

- Task completion is a well-defined free point — no reachability question.
- Arenas are already the mechanism for batch-lifetime data (D-003).
- Because tasks are pinned (D-032), the executor's arena is **single-threaded**,
  so frame allocation uses plain `arena<T>` at zero cost rather than
  `shared_arena<T>`'s atomic bump.

This keeps the async runtime's memory deterministic and introduces no discipline
that does not already exist.

---

## D-035 — `wildx` is retained; the verification boundary is stated, not removed — **SETTLED; the state machine is built by D-155** (0.10.5 makes the three open deliverables real — the lifecycle analysis, the W^X runtime, and `--extra-picky=no-wildx`)

### The question

Can a language with JIT-capable executable memory as a builtin pass formal
verification — and if not, could removing it be recovered by a user willing to
take the risk?

### What is and is not verifiable

The distinction that matters: **you can verify the container even when you cannot
verify the contents.**

| | Verifiable? | By what |
|---|---|---|
| The `wildx` **lifecycle** — `alloc` → write → `seal` → execute → `free` | ✅ **yes** | it is a state machine, which is precisely what Z3 and K handle well |
| The **W^X invariant** — no page is ever writable and executable at once | ✅ **yes** | `wildx_seal` is a *one-way* transition, so the invariant is structural rather than a runtime check |
| No execute-before-seal, no write-after-seal, no double-free, no leak at `exit` | ✅ **yes** | ordinary state-machine and ownership obligations |
| The **semantics of the generated bytes** | ❌ **no** | they do not exist at compile time; there is no AST for Z3 to translate and no term for K to reduce |

So a program using `wildx` is verifiable **up to the boundary**, and the
generated code lies outside both backends — exactly like the FFI barrier
(`VERIFICATION_REFERENCE.md` §6.3).

**This is inherent to runtime code generation, not to Nitpick's design.** No
language can verify code that does not exist when the verifier runs.

The guarantee `wildx` actually delivers is therefore *containment*: **the JIT
cannot corrupt the host program's memory safety.** That is the correct guarantee,
and it is the one Nikola's architecture already assumes — sandbox and oracle
rounds validate the *contents*, W^X and guard pages contain the *mechanism*.

### Removing it would be strictly worse

The capability does not depend on the keyword. Any user with the escape hatches
already in the language can reconstruct it:

```nitpick
// get a writable page
int64:p = raw sys!!(MMAP, 0i64, 4096i64, PROT_RW, MAP_ANON_PRIV, -1i64, 0i64);
// … write bytes …
drop sys!!(MPROTECT, p, 4096i64, PROT_EXEC);      // flip to executable
wild int8->:fn = #wild_ptr<int8->>(p);            // make it callable
```

Removing `wildx` therefore removes **none** of the capability and **all** of the
scaffolding:

- the W^X invariant is gone — nothing stops `PROT_WRITE|PROT_EXEC` in one call;
- ASLR and guard pages are gone;
- the sealed state machine is gone, so execute-before-seal becomes possible;
- leak tracking at `exit` no longer covers the pages;
- and the operation becomes **less visible** — one greppable keyword is replaced
  by a scatter of syscalls that read like ordinary memory work.

An auditor grepping for `wildx` finds every JIT site in the codebase. An auditor
grepping for a hand-rolled `mprotect` sequence finds them only if they think to
look.

### Certification

A program containing `wildx` will not reach the highest assurance levels of
DO-178C, IEC 61508, or ISO 26262 — those regimes require structural coverage
analysis over code that exists before execution. That is a property of the
*program*, not the language.

`--extra-picky`'s `wild` rule **already rejects both `wild` and `wildx`**
(`FORMAL_DRAFT` 12.7.4), so a high-assurance build mode that excludes runtime
code generation entirely already exists. The language serves both audiences
without changing.

### What actually needs doing

1. **State the verification boundary in the spec.** Nothing currently says that
   `wildx`-generated code is outside both backends. It should say so as plainly
   as the FFI barrier does.
2. **Specify the lifecycle as verifiable obligations** — seal-before-execute,
   no write-after-seal, no double-free, no live pages at `exit` — so the parts
   that *can* be proven are written down as things to prove.
3. Consider a `--extra-picky=no-wildx` rule separate from `no-wild`, so manual
   memory and runtime code generation can be banned independently. They are very
   different risks and currently share one switch.

### Bug found in the existing JIT helper

`nitpick/stdlib/jit.npk` `Jit.compile_add_i32()` frees the page on write failure
and then **continues to seal the freed page**:

```nitpick
if (write_rc != 0i32) {
    _? wildx_free(page);          // freed…
}
fixed int32:seal_rc = wildx_seal(page);   // …then used
```

There is no early return. A **use-after-free in the JIT helper itself**, and a
good regression test for the new compiler — this is exactly the class the
`wildx` state machine should reject statically.

*(That module is also C-dependent — `extern func:npk_jit_install_add_i32` binds
to `src/runtime/assembler/jit_smoke.cpp` — so it needs replacing regardless.)*

---

## D-036 — `tfp` and `dim` are distinct types; `fix256` is the obsolete name — **SETTLED**

Records the naming history, because the rename is still incomplete across the
specs and the reasoning is not recoverable from the files.

### The family as it stands

| Type | Meaning | Dimensional analysis |
|---|---|---|
| `tfp32` `tfp64` `tfp128` `tfp256` | **Twisted Fixed Point** — Q16.16, Q32.32, Q64.64, Q128.128 | **no** |
| `dim256<Unit>` | dimensional-analysis fixed point | **yes** — `<Joules>`, `<Meters>`, `<Seconds>`, `<Newtons>`, `<Kelvin>` |
| ~~`fix256`~~ | **obsolete name for `dim256`** | — |

`dim256<Unit>` is **identical to `tfp256` at the IR level** — unit annotations are
compile-time only and erased before lowering. Dimensional analysis is therefore
zero-cost, and it is `dim256`-exclusive: narrower fixed-point types do not accept
`<Unit>`.

### How the naming got here

1. `tfp` originally meant **Twisted Floating Point**.
2. That was redundant with `flt`, which already carries a NaN taint — a second
   floating-point family with sticky-error semantics bought nothing.
3. So `tfp` was repurposed to **Twisted Fixed Point**, which also matches `tbb`
   (Twisted Balanced Binary) — the "twisted" prefix consistently marking a family
   that reserves a value as a sticky error state.
4. That left `tfp256` and `fix256` overlapping: both Q128.128 fixed point,
   differing only in that `fix256` carried dimensional analysis.
5. `fix256` was renamed **`dim256`** to say what actually distinguishes it. The
   name now describes the feature rather than the representation.

### A second reason the rename was worth doing

`fix` and **`fixed`** are different things — `fixed` is Nitpick's immutability
qualifier (`fixed int32:MAX = 100i32;`). Having a `fix256` *type* alongside a
`fixed` *qualifier* invited exactly the kind of near-miss confusion the blueprint
philosophy exists to prevent. `dim256` removes it.

### Propagation still owed

`fix256` survives in:

- `FORMAL_DRAFT` 02 §2.3.3 — `FixedPointType ::= "fix256"`
- `FORMAL_DRAFT` 01 §1.4 `BuiltinType` and §1.6.2 `TypeSuffix`
- `TYPE_REFERENCE.md` — check both the type tables and the literal-suffix list

And **`tfp128` / `tfp256` are missing** from `LEXICAL_REFERENCE.md`'s `BuiltinType`
and `TypeSuffix` productions, which list only `tfp32` and `tfp64`.
`TYPE_REFERENCE.md` §5 defines all four.

---

## D-037 — No `int`, `uint`, or float type ever carries an ERR sentinel — **SETTLED**

Resolves `GRAMMAR_ADOPTION_CONFLICTS.md` Part R, and corrects a broader
contradiction it exposed.

### The rule

**`int*`, `uint*`, and `flt*` follow standard arithmetic behaviour at every
width.** None of them reserves a value as an error state. That includes the LBIM
types — `int1024` … `int4096` and unsigned counterparts — whose sticky-ERR
sentinel in `FORMAL_DRAFT` 02 §2.2.1 is **struck**.

The "twisted" families exist for the opposite case, and the two **complement
rather than replace** each other:

| Want | Use |
|---|---|
| over/underflow is **expected and sometimes desired** | `int*` / `uint*` — standard wrapping |
| over/underflow is **an error** | `tbb*` — sticky ERR |
| deterministic fractional arithmetic with a sticky error state | `tfp*` |
| exact rational values, no rounding at all | `frac*` |
| IEEE semantics including NaN/inf | `flt*` |

The choice is made **at the declaration, by type**, and is visible at every use —
the same shape as D-007's divide-by-zero resolution.

### Overflow on `int` is *wrapping*, not a trap — correcting `TYPE_REFERENCE.md`

`TYPE_REFERENCE.md` §1.2 currently specifies:

> Arithmetic: `+`, `-`, `*`, `/`, `%` — all use **checked (safe) variants** by
> default … `llvm.sadd.with.overflow.iN` → check overflow bit → **failsafe on
> overflow**

**This is wrong.** Wrapping is *defined* behaviour in two's complement, not
undefined behaviour, and it is routinely what is wanted — hashing, checksums,
PRNGs, modular arithmetic. Trapping there would make ordinary correct code
unrunnable, and it would leave no way to express "I want wrapping" at all.

`SPEC_GAPS` §4 already said as much: developers using plain types "accept the
risk of standard overflow/underflow". Two sources against one, and the intent is
explicit.

§1.3 (unsigned) carries the same error. §1.4 (floats) is already correct — "no
overflow checking, IEEE 754 handles inf/nan".

### Divide-by-zero is *not* over/underflow, and still traps

Worth stating because the distinction is easy to lose:

| Event | Has a defined result? | `int` / `uint` behaviour |
|---|---|---|
| overflow / underflow | **yes** — two's complement wrapping | wrap (this decision) |
| **divide by zero** | **no** — no value exists | **trap to `failsafe`** (D-007, unchanged) |

Nitpick rejects undefined behaviour, so division by zero must do something
defined. Trapping is the only option that does not invent a value out of nothing.
On `tbb` it yields ERR instead, per D-007.

### Why `tfp` is fixed point and not floating point

`tfp` originally meant Twisted *Floating* Point and appears to have been
introduced by the prototype's agent. It was redundant: `flt` already carries a
NaN taint, so a second floating-point family with sticky-error semantics added
nothing. Meanwhile fixed-point types and `frac` already cover the cases where
accuracy and absolute certainty about behaviour are required.

So `tfp` was repurposed to Twisted **Fixed** Point (D-036), leaving one coherent
rule: **"twisted" marks a family that reserves a value as a sticky error state**,
and it never appears on `int`, `uint`, or `flt`.

### Propagation owed

- `FORMAL_DRAFT` 02 §2.2.1 — strike the LBIM ERR sentinel paragraph.
- `TYPE_REFERENCE.md` §1.2 and §1.3 — overflow wraps; remove the failsafe claim.
- Check `ncrypto` (34,925 lines) for reliance on LBIM ERR propagation before
  porting.

---

## D-038 — Pointers are thin — **SETTLED**

Every pointer is a single machine word — LLVM's opaque `ptr`. There is **no
bounds metadata carried at runtime**.

`FORMAL_DRAFT` 15 §15.1.3's claim that "`int8->` is a Fat Pointer containing
bounds metadata" is **struck**. `TYPE_REFERENCE.md` §10 is correct: all pointer
kinds lower to identical IR, and the distinction between `wild` and borrow
pointers is enforced **entirely by the type checker**.

### Why thin is the right answer here

The usual argument for fat pointers is that they make out-of-bounds detectable on
any pointer at runtime. Nitpick closes most of that ground **statically and at
zero cost**, so the runtime metadata would be paying twice:

| Hazard | Already handled by |
|---|---|
| dangling references | **second-class borrows** — a borrow cannot outlive its frame (D-004) |
| use-after-free in arenas | **generation-counted `Handle<T>`** — a stale handle fails through `Result<T>` |
| out-of-bounds indexing | bounds checks on array access, plus `limit<Rules>` and Z3 proving indices in range |
| leaks | the K-semantics `exit` rule |

What fat pointers would add beyond that is bounds checking on **raw `wild`
pointers** — which are the explicit, greppable opt-out from safety in the first
place. Paying two-to-three words on every pointer in the language to partially
protect the construct that exists to say "I am taking responsibility here" is a
poor trade.

Three further costs:

- **C ABI incompatibility.** Fat pointers cannot be passed to C without
  conversion at every FFI boundary, which is exactly where the language is
  already most careful and least able to afford surprises.
- **Performance.** Two-to-three words per pointer on the numeric hot path, where
  performance is a first-class requirement.
- **Verification.** `--verify-memory` becomes partly a *runtime* guarantee rather
  than a static one — the wrong direction for Astrée, which reasons about what is
  provable before execution.

`--guard-pages` (`FORMAL_DRAFT` 14.5.3) remains available for aggressive
overrun detection around `wild` allocations, without changing the pointer
representation.

---

## D-039 — Z3 and LLVM are invoked as tools, never linked — **SETTLED**

Neither is compiled into the compiler binary or the produced binary. Both are
driven over **text interfaces**, as external processes:

| Tool | Interface | Role |
|---|---|---|
| **Z3** | SMT-LIB2 text | invoked during the build to discharge proof obligations |
| **LLVM** | LLVM IR text, via `llc` / `opt` / `llvm-as` | invoked to assemble, optimise, and lower |

This resolves `GRAMMAR_ADOPTION_CONFLICTS.md` Part X. `FORMAL_DRAFT` 00b's
"isolated exceptions being the LLVM IR generator and the Z3 SMT Subsystem"
described *linking*, and is corrected: they are exceptions in the sense that we
**use** them, not that C++ enters the trusted computing base.

Nothing about the zero-dependency rule is weakened. No C or C++ is linked into
either binary, and neither tool is present at runtime.

### Text interfaces make the solver replaceable

Beyond the dependency argument, this is the decision that keeps the door open.
**SMT-LIB2 is a standard**, so a text interface means Z3 can eventually be
swapped for a Nitpick-native solver without touching the compiler's structure.
Linking `libz3` would couple the compiler to Z3 permanently.

A Nitpick-native SMT solver is a long-term aspiration, not near-term work. That
mathematics is a specialist field and the intent is to leave it to people who
work in it. Retaining Z3 compatibility over a text interface is the bridge until
then. `--debug-z3` already dumps SMT-LIB2, so the interface exists.

### What Z3 changes in the output

Z3 runs during the build and verifies what it can. Its one effect on generated
code is **check elimination**, and the rule is conservative in the safe
direction:

> If Z3 **proves** a value cannot fall outside its range, the corresponding
> runtime check is removed. If it **cannot prove it**, the check stays and runs
> at runtime.

Proof can only ever *remove* a check that was provably unnecessary. Absence of
proof never removes anything. This is `--smt-opt`
(`VERIFICATION_REFERENCE.md` §5).

### ⚠️ `--smt-opt` must not make builds non-deterministic

Flagged rather than decided, because it matters for Astrée.

`--smt-timeout=N` defaults to 5000 ms. If a proof succeeds within the timeout on
one machine and times out on another — slower hardware, heavier load, a different
Z3 build — **the two compilations emit different binaries from identical
sources.** One retains a check the other elided.

That is non-deterministic compilation, which is a serious problem for
reproducible builds and for certification, where the artifact analysed must be
the artifact shipped.

Options, roughly in order of preference:

1. **Record the elimination set.** Emit a manifest of every check Z3 discharged;
   a build that does not reproduce it fails rather than silently differing.
2. **Off by default for release and certified builds.** Treat `--smt-opt` as a
   development-time optimisation only.
3. **Unbounded timeout in release builds**, so the result depends on the
   obligations rather than on the clock.

The optimisation is a by-product of the Z3 integration suggested by an agent
rather than a requirement, and there is no attachment to it if it proves
troublesome. Determinism should win where the two conflict.

---

## D-040 — `--smt-opt` records an elimination manifest — **SETTLED**

Closes the determinism concern raised in D-039. Where determinism and
optimisation conflict, **determinism wins**.

### The rule

When `--smt-opt` elides a runtime check because Z3 proved it unnecessary, the
compiler **records that decision in a manifest**. On any subsequent build the
manifest is authoritative:

| Situation | Outcome |
|---|---|
| no manifest present | one is generated; the build is marked *not reproducibility-verified* — **amended at 1.5.0 (P-9): nothing writes the manifest implicitly; an absent `nitpick.obligations` fails `npkg verify` by name and `npkg verify --record` writes it on purpose** |
| manifest present, every obligation matches | build proceeds; binary is reproducible |
| Z3 proves **more** than the manifest records (faster machine, warmer cache) | **build fails** — the binary would differ |
| Z3 proves **less** (timeout, slower host, different solver build) | **build fails** — the binary would differ |

Note that proving *less* also fails, even though the resulting binary would be
*safer* — it would retain a check the manifest says was removed. Reproducibility
is the property being protected, not conservatism, and a build that silently
differs is the thing to prevent regardless of which direction it differs in.

### What the manifest must contain

```
# nitpick-smt-manifest v1
# compiler: <version>   z3: <version>   target: <triple>   timeout-ms: <N>
<obligation-hash>  <function>  <kind>  discharged
```

- **Obligation identity must be stable across edits.** Source line numbers are
  too fragile — any change above a check renumbers everything below it. Identify
  each obligation by a **hash of its normalised SMT-LIB2 form**, so the same
  obligation yields the same identifier regardless of where it now sits.
- **Record the Z3 version and the timeout.** A different solver build can decide
  different things, and the timeout is precisely the variable that made this
  non-deterministic. Both belong in the header so a mismatch is diagnosable
  rather than mysterious.

### What this does and does not guarantee

It does **not** make Z3 deterministic. It makes **divergence detectable and
fatal** instead of silent. That is the achievable property, and it is the one
that matters: a build either reproduces the recorded reasoning exactly, or it
stops.

### It also turns a risk into an audit artifact

Worth noting for Astrée: the manifest is **evidence that every elided check had a
proof**. Rather than asking an auditor to accept that the compiler removed checks
safely, the build produces a list of exactly which checks were removed and on
what grounds, reproducible on demand.

For certification runs the manifest should be able to carry **full proof
certificates** (unsat cores) rather than just outcomes — larger, but maximally
auditable. Hash-and-outcome is the default; certificates are an opt-in.

### The asymmetry with the `--verify*` flags

Only `--smt-opt` changes generated code, so only it needs a manifest for
*reproducibility*.

The `--verify*` family can still behave non-deterministically under timeout — a
proof that succeeds on a fast machine may time out on a slow one — but it fails
**safe**: the outcome is a rejected build, never a silently different binary. You
cannot accidentally ship something unverified; you can only fail to build it.
Worth keeping the distinction in mind when reasoning about timeouts, and worth
considering an unbounded timeout for certification runs so that verification
depends on the obligations rather than on the clock.

---

## D-041 — The `a*` builtin collections are removed — **SETTLED**

`astack`, `alist`, `ahash`, and `astringlist`, together with their ~35 operation
keywords, are **not part of the language**. Collections belong in a library.

### What goes

```
astack  apush  apop   apeek  acap   asize  afits  atype
ahash   ahset  ahget  ahcount ahsize ahfits ahtype ahdelete ahhas ahclear ahkeys
alist   alpush alinsert alset alremove alpop alget alsize
astringlist aslpush aslinsert aslset aslremove aslpop aslget aslsize
```

That is **35 reserved words returned to userland** — a meaningful share of the
keyword space, and every one of them was unavailable as an identifier.

### Why they went

They were **scope-bound**: allocated against the enclosing scope and reclaimed
automatically at its end, so they never needed explicit cleanup. That was
genuinely useful in a few applications, but the scope-bound nature proved
confusing in practice — agents working on the prototype repeatedly misused them —
and the same capability is straightforward to provide as an ordinary library.

They are also the last significant **`aria` naming artifact** in the language
surface (D-036's `fix`/`fixed` cleanup and the `Aria`→`Nitpick` rename being the
others). The `a` prefix is a fossil of the old name.

### Already superseded in practice

`nitpick/stdlib/regex/alist.npk` **reimplements `alist` and `alpush` as ordinary
functions** — the library replacement already exists and is in use by the regex
engine. Only a stale comment in `nregx.npk` still describes `alist` as a builtin.

Measured across `libn/src`, `nitpick/stdlib`, `ncrypto/src`, and `nlists/src`:
**zero real uses of any builtin form.**

### This resolves three logged conflicts

`FORMAL_DRAFT` 15 §15.2.1–15.2.4 documented these builtins and is **struck
entirely** — which disposes of:

- **conflict 55** — collections "managed via opaque `int64` handles" (D-012 would
  have rejected this anyway);
- **conflict 56** — `0`/`-1` status returns and the `unknown` sentinel on
  underflow, violating the universal `Result<T>` rule twice over.

§15.2.5 (arenas and generational handles) is **retained** — it is correct and
consistent with D-017.

### Historical note: where `--smt-opt` came from

These builtins could hold **any type**. If the solver could prove every element
in a given collection had the same type, the per-access **type check** could be
eliminated. That is the origin of the SMT-guided optimisation now governed by
D-040 — the idea outlived the construct that motivated it.

---

## D-042 — Kernel identifiers are distinct types, not integers — **SETTLED**

### The rule

**A kernel-assigned identifier is not a number.** Each gets its own type,
permitting comparison and forbidding arithmetic — the same treatment that
separated `char8` from `uint8` and `bool` from `int8` (D-005).

| Type | Represents | LLVM | Replaces |
|---|---|---|---|
| `fd` | file descriptor | `i32` | 19 signatures returning `int64` |
| `pid` | process identifier | `i32` | |
| `tid` | thread identifier | `i32` | 26 signatures between them |
| `uid` | user identifier | `i32` | |
| `gid` | group identifier | `i32` | |

`uid` and `gid` are included because the same argument applies without
modification: comparing a `uid` to a `pid` is a bug that no amount of care
prevents while both are `int64`, and the compiler can simply refuse it.

### Permitted and forbidden operations

| | |
|---|---|
| **Permitted** | `==`, `!=`, `<`, `<=`, `>`, `>=` |
| **Forbidden** | `+`, `-`, `*`, `/`, `%`, `++`, `--`, and all bitwise operators |

Ordering is kept rather than restricted to equality because it has a real use —
computing the maximum descriptor for a `poll` or `select` bound. Arithmetic has
none: adding two file descriptors is not an operation, and `fd + 1` as a way to
reach "the next descriptor" is precisely the kind of guess the type exists to
prevent.

### The property this buys, beyond pedantry

Combined with the universal `Result<T>` rule, **an `fd` value is always valid.**

In POSIX, `open()` returns `-1` on failure, so every descriptor is potentially a
sentinel and every use site is responsible for remembering to check. Here the
failure goes to `Result.err` and never reaches the `fd` type at all. The
"did I check for `-1`?" bug class does not exist, because `-1` is not
representable as an `fd`.

The same holds for `pid`: `fork()` returning `-1` becomes an errored `Result`, and
the `0`-means-child convention stays a legitimate `pid` value that a `pick` can
match explicitly.

### Constants and conversions

Standard descriptors are named constants of type `fd`: **`STDIN`**, **`STDOUT`**,
**`STDERR`**.

Conversion is explicit in both directions:

- `fd => int32` — always safe, for logging or serialisation.
- `int32 => fd` — **requires `=>!`**. Fabricating a descriptor from an arbitrary
  integer is legitimate (inheriting fd 3 from a parent, for instance) but is an
  assertion that the integer really is an open descriptor, and it should be
  greppable like every other such assertion.

### Consequences

- `LEXICAL_REFERENCE.md` — add `fd`, `pid`, `tid`, `uid`, `gid` to `BuiltinType`.
- `SIGNATURE_LEDGER.md` — the FD (19) and PID (26) categories now target these
  types rather than `int32`.
- `nlibc`'s syscall layer is where they originate: `io_open` and friends return
  `Result<fd>`, `fork` returns `Result<pid>`, `libn_getuid` returns `Result<uid>`.
- Any function currently taking `int64:fd` takes `fd`, which also removes a class
  of argument-order mistakes — `dup2(oldfd, newfd)` cannot be called with a size
  in either position.

---

## D-043 — Signals are an enum — **SETTLED**

Signal numbers become a closed enumeration rather than bare integers or a scalar
`sig` type.

D-042's rule is "a kernel-assigned identifier is not a number", but a signal
number is not an identifier — it is a **constant drawn from a fixed, known set**.
That is enum-shaped. An enum additionally makes `pick` over signals
**exhaustive**, so a handler that forgets `SIGTERM` fails to compile rather than
silently ignoring it.

Affects the 16 signal-related parameters in `PARAMETER_LEDGER.md` and
`proc/signal.npk`'s public surface (`sigaction`, `signal`, `kill`, `raise`,
`killpg`, `sigaddset`, `sigdelset`, …).

Signal **sets** (`sigset_t` equivalents) remain a separate type — a set of enum
members, not an enum member.

---

## D-044 — Flags are bitflag types, not bare integers — **SETTLED**

Mode bits, open flags, protection bits, and `fcntl` commands become **distinct
bitflag types**, one per family. `int32` is not acceptable for any of them.

| Family | Type | Members |
|---|---|---|
| open flags | `oflags` | `O_RDONLY`, `O_WRONLY`, `O_CREAT`, `O_APPEND`, … |
| memory protection | `prot` | `PROT_READ`, `PROT_WRITE`, `PROT_EXEC`, `PROT_NONE` |
| mmap flags | `mflags` | `MAP_ANON`, `MAP_PRIVATE`, `MAP_SHARED`, … |
| file mode bits | `fmode` | `S_IRUSR`, `S_IWUSR`, … |
| `fcntl` commands | `fcmd` | `F_GETFL`, `F_SETFL`, `F_SETFD`, … |
| `madvise` advice | `advice` | `MADV_*` |
| seek origin | `whence` | `SEEK_SET`, `SEEK_CUR`, `SEEK_END` |

### Why this is not deferrable

As bare integers, nothing prevents `PROT_READ` being passed where an open flag
belongs, or `SEEK_END` where a `fcntl` command belongs. Both compile, both are
wrong, and both fail at runtime in ways that look like unrelated bugs. This is
the identical error class D-042 eliminated for descriptors, and the same argument
applies without modification.

**Adding it later would require re-verification**, which is not affordable. It
goes in now.

### Operations

| | |
|---|---|
| **Permitted** | `\|` (combine), `&` (test), `~` within the same family, `==`, `!=` |
| **Forbidden** | arithmetic; **mixing families** — `oflags \| prot` is a compile error |

Conversion to the underlying integer is explicit (`=>`), since the syscall layer
must eventually hand a raw value to the kernel. That conversion is confined to
`nlibc`'s syscall wrappers and is greppable.

### Precedent

`nlibc`'s `syscall/posix_constants.npk` (661 lines) already defines these as
constants; they need grouping into types rather than inventing from scratch.

---

## D-045 — Variadic functions: format strings are checked at compile time — **SUPERSEDED by D-053**

> **The `fmt` type described here no longer exists.** D-053 removed format
> strings from the language entirely — formatting is ordinary functions returning
> `string`, spliced by `&{ }` interpolation — so there is nothing for `fmt` to
> govern. The homogeneous `..*T[]` variadic below survives, with the `sys`
> builtin as its consumer. Retained as the record of why the format-directed form
> was introduced.


**This blocks the variadic collapse and needs settling before it starts.**

### The problem the collapse exposed

`PARAMETER_LEDGER.md` found that 153 of 608 functions are hand-expanded variadic
families, and proposed collapsing them with `..*`. But `..*` as specified is
**homogeneous** — `FORMAL_DRAFT` 06 §6.1.3 shows `..*string[]:args`, a typed
slice.

The printf and scanf families are **heterogeneous**. `libn` currently handles that
by erasing everything to `int64`:

```nitpick
pub func:io_printf2 = int64(int64:fmt, int64:a0, int64:a1) { … };
```

An `int64` argument might be a number or might be a pointer to a string, and the
**format string decides which**. That is precisely the C model, and it carries the
C consequences: `%s` against a non-pointer reads an integer as an address, and
argument count mismatches read past the end of the argument list. Format-string
handling is a long-standing CVE class for exactly this reason.

Collapsing these families with an erased `..*int64[]` would preserve the hazard.

### The split

| Families | Shape | Collapse |
|---|---|---|
| `execlN` (9), `execlpN` (8), `sysN` (5), `sys_fullN` (5) — **27 functions** | homogeneous — all `string`, all `int64` | trivial: `..*string[]`, `..*int64[]` |
| printf/fprintf/eprintf/asprintf/io_*printf/str_snprintf/strbuf_appendf (99) and scanf/fscanf/sscanf (27) — **126 functions** | heterogeneous, format-directed | needs a mechanism |

### Recommendation: format-checked, compile-time

Make the printf and scanf families **compiler-checked constructs** rather than
ordinary variadic functions:

- the format string must be a **compile-time constant**;
- the compiler **parses it and checks each specifier against the corresponding
  argument's type**;
- a mismatch — wrong type, too few arguments, too many — is a **compile error**.

This eliminates the entire class: no `%s` with an integer, no argument-count
overrun, no runtime format parsing on untrusted input. It is what Rust's
`format!` does, and it fits the language's posture — the check moves to compile
time where Astrée can see it, rather than becoming a runtime failure mode that
must be proven absent.

**Consequence:** a runtime-constructed format string cannot use these
constructs. That is the intended outcome — a runtime format string is the
vulnerability. Code needing dynamic output composes it explicitly with
`strbuf`, which is already present and type-safe.

**`scanf` deserves particular attention**: it writes through caller-supplied
pointers, so a type mismatch corrupts memory rather than merely printing
nonsense. If anything, the case for compile-time checking is stronger there.

### Settled

1. **Format-checked compile-time variadics** for the printf and scanf families.
2. **`..*` stays homogeneous-only** everywhere else — the remaining 27 functions
   need nothing more.
3. **No runtime-format escape hatch.** `strbuf` covers the legitimate dynamic
   cases type-safely, and an escape hatch here would reintroduce precisely the
   hazard being removed.

### The mechanism: a `fmt` parameter type

The requirement lives in the **type**, not in a new call syntax:

```nitpick
pub func:printf = int64(fmt:f, ..*);
```

- **`fmt` is inhabited only by compile-time string literals.** A runtime `string`
  cannot be passed, so a format cannot be constructed, stored, or received as
  data. This is what forecloses the vulnerability class rather than mitigating it.
- The compiler **parses the literal and checks each specifier against the
  corresponding argument's type**. Wrong type, too few arguments, or too many is a
  **compile error**.
- Call sites look ordinary — `printf("count: %d\n", n)` — with no sigil and no
  macro invocation.

Putting the constraint in the parameter type keeps it **visible in the signature**
and consistent with how the rest of the language works: `tbb` versus `int32`
selects overflow behaviour, `fd` versus `int32` selects what operations are legal,
and `fmt` versus `string` selects whether a value may be computed at runtime.

Two alternatives were considered and rejected. A compiler-directive spelling
(`#printf(…)`, consistent with D-020) puts a sigil on every print statement in the
language. A macro spelling (`printf!(…)`, per `FORMAL_DRAFT` 08.5) overloads `!`,
which already means unchecked or emphatic in `?!`, `=>!`, and `!!!`.

> **Pre-existing issue this surfaced:** `FORMAL_DRAFT` 08.5 specifies macro
> invocation as `name!(args)`, so `!` already carries two unrelated meanings in
> the language — macro invocation, and unchecked/emphatic. That is a blueprint
> violation independent of this decision and needs its own resolution.

---

## D-046 — Resolving `!`'s multiple meanings — **SETTLED**

`!` currently carries **three** unrelated meanings. Enumerated:

| Form | Meaning | Position |
|---|---|---|
| `!x` | logical NOT | leading, standalone |
| `!=` | inequality | leading, compound |
| `?!` | emphatic unwrap → `failsafe` | **trailing** |
| `=>!` | unchecked cast | **trailing** |
| `_!` | raw unwrap shorthand | **trailing** |
| `!!!` | failsafe abort | **repeated** |
| `sys!!`, `asm!!` | full-tier / supervised modifier | **repeated** |
| `name!(args)` | **macro invocation** | trailing on an identifier |

Three groups: **negation**, **unchecked/emphatic**, and **macro invocation**.

### 1. Macros move to `#name(args)` — the actual violation

`name!(args)` is the genuine collision: `foo!(x)` is visually indistinguishable
from an emphatic operation, and it is the only one of the three that has no
positional cue.

Macros become **`#name(args)`**, unified with builtins under D-020's
compiler-directive sigil:

```nitpick
macro:assert_positive = (x) { … };      // definition unchanged
#assert_positive(count);                 // invocation
```

This is correct rather than merely convenient. A macro **is** a compile-time
construct addressed to the compiler, which is exactly what `#` denotes — the same
category as `#size_of<T>` and `#[derive(…)]`. A caller does not need to know
whether `#foo(x)` is compiler-provided or user-defined; both expand at compile
time and both are type-checked after expansion.

**Cost: zero.** No macros are defined anywhere in the ecosystem — not in `libn`,
`stdlib`, `ncrypto`, or `nlists` — and there are no real call sites. This is
free to change now and would not be later.

### 2. `!!` is eliminated

After D-001 removed `sys!!!` and `asm!!!`, the `!!` tier marker is nearly
vestigial:

| Now | Becomes | Why |
|---|---|---|
| `asm!!<T>(…)` | **`asm<T>(…)`** | it is the *only* remaining assembly form, so the tier marker distinguishes nothing |
| `sys!!(…)` | **removed** | superseded by D-048 — the tiers collapse to a single `sys`, so there is no second spelling to name |

This removes `!!` from the language entirely.

> **Superseded in part.** This decision originally renamed `sys!!` to `sys_full`.
> D-048 then collapsed the syscall tiers altogether, so `sys_full` never enters
> the language — there is only `sys`. The `asm!!` → `asm` rename stands.

### 3. What remains, and the rule

After the above, `!` has **two** meanings, distinguished **lexically** rather than
by context:

> **Leading `!` negates** — `!x`, `!=`.
> **Trailing or repeated `!` marks unchecked or emphatic** — `?!`, `=>!`, `_!`, `!!!`.

This is a real rule and it should be **stated in `OP_REFERENCE.md`** rather than
left for a reader to infer. It is weaker than "one symbol, one meaning", and the
honest position is that it is a compromise rather than a clean result.

**Recommendation: accept it.** The alternatives are worse:

- Renaming negation to `not` breaks up the `&&` / `||` / `!` family, which hangs
  together as a set.
- Renaming the emphatic family loses genuinely good notation — `!` reads as
  urgency, and `?!` / `!!!` convey their severity at a glance, which is facet 2
  of the blueprint philosophy working as intended.

Crucially, the distinction is **lexical, not contextual**: a reader can tell which
meaning applies from the token itself, without knowing what surrounds it. That is
categorically different from the `->` problem D-006 fixed, where the *same* token
in the *same* position meant different things depending on the operand's type.

### Consequences

- `LEXICAL_REFERENCE.md` — remove `!!` from `ModifierToken`; macros use `#`.
- `OP_REFERENCE.md` — state the leading/trailing rule explicitly.
- `BUILTIN_REFERENCE.md` — `asm!!` becomes `asm`; `sys!!` becomes `sys_full`.
- `FORMAL_DRAFT` 08.5 (macro invocation) and 08.2 / 10.2 (syscall tiers) need
  updating on adoption.
- `nlibc`'s `sys_full` wrapper name now matches the language builtin — check for
  shadowing.

---

## D-047 — `nlibc` drops its syscall wrapper layer; the whitelist policy moves into the builtin — **SETTLED**

Surfaced by D-046's `sys!!` → `sys_full` rename, which collided with an existing
`libn` function of the same name. Investigating that turned up something larger.

### `libn`'s syscall foundation is built on a removed construct

```nitpick
pub func:sys_full = int64(int64:nr, int64:a1, …, int64:a6) {
    int64:ret = sys!!!(nr, a1, a2, a3, a4, a5, a6);   // D-001 removed this tier
    return err_from_syscall(ret);
};
```

**Every syscall in `libn` reaches the kernel through `sys!!!`**, the raw tier
D-001 deleted for returning a bare `int64` and accepting an arbitrary expression
as the syscall number. `sys_safe` sits on the same foundation.

### The wrapper layer becomes redundant

The language builtins now do what these wrappers were built to do:

| `libn` wrapper | Builtin | Verdict |
|---|---|---|
| `sys_full` (7-arg) + `sys_full1`…`sys_full5` | `sys_full(CONST, ..*int64[])` | **delete** — the builtin returns `Result<int64>` directly |
| `sys_safe` (whitelist check) + `sys1`…`sys5` | `sys(CONST, ..*int64[])` | **delete** — but see below |
| `err_from_syscall` | — | **delete** — the builtin already produces `Result.err`; converting a negative return by hand is exactly the double-encoding D-012 objected to |

So the syscall layer collapses to **nothing**: callers use the builtins directly.
That is a larger reduction than the variadic collapse alone implied — roughly a
dozen functions plus their error-conversion helper disappear rather than being
ported.

### But `sys_safe` holds policy the builtin leaves unspecified

`BUILTIN_REFERENCE.md` §3 describes `sys` as "restricted to a curated whitelist"
without ever saying **which** syscalls. `libn`'s `sys_safe` is that whitelist,
written out — and it is more specific than a list of numbers:

- an explicit enumeration covering read/write/open/close/stat/lseek, the epoll and
  select family, and the socket family;
- **per-argument filtering**: `SYS_IOCTL` is admitted only for three specific
  request codes (`TCGETS` among them), rejecting everything else with `-EINVAL`.

That second point is real safety policy. An `ioctl` whitelist that admits the call
but not arbitrary request codes is doing meaningful work, and it would be lost if
the wrapper were simply deleted.

**The policy is extracted before the wrapper is removed** — but D-048 changed
where it goes.

The **syscall list** is no longer needed: D-048 collapsed the tiers, so there is
no whitelist to specify, and restricting which syscalls a binary may make is
`--seccomp`'s job instead.

The **argument-level constraints** still matter and must be checked against
`libn`'s typed `io_*` wrappers — `io_set_cloexec`, `io_set_nonblocking`,
`io_set_append`, `io_fcntl_setown` — to confirm that everything `sys_safe`
enforced is already unreachable through the typed API. That check happens before
deletion, not after.

### Ordering

This precedes the variadic collapse for the `sys` families. `VARIADIC_COLLAPSE.md`
lists `sysN` (5) and `sys_fullN` (5) as collapsing to two functions; they instead
**collapse to zero**. Doing the collapse first would mean carefully rewriting
functions about to be deleted — the same trap the collapse itself was meant to
avoid.

### Consequences

- `VARIADIC_COLLAPSE.md` — `sys`/`sys_full` move from "collapse to 2" to "delete".
- `BUILTIN_REFERENCE.md` §3 — the `sys` whitelist must be specified in full,
  including per-argument constraints, sourced from `sys_safe`.
- `nlibc` — `syscall/syscall.npk` (680 lines, 63 public functions) shrinks
  substantially; audit what remains once the wrappers go.
- Every `libn` call site reaching a syscall through `sys_safe` / `sys_full` is
  rewritten to the builtin.

---

## D-048 — Collapse the syscall tiers to a single `sys` — **SETTLED**

Supersedes part of D-047, which assumed the two-tier split would survive.

### The original scheme, and why it no longer applies

The three tiers were a **danger-escalation ladder along two axes** — how many
syscalls are reachable, and whether the result is wrapped:

| Tier | Reachable | Wrapped |
|---|---|---|
| `sys` | curated whitelist | **no** |
| `sys!!` | everything | yes |
| `sys!!!` | everything | **no** |

The whitelist was small **because `sys` was unwrapped**. Restricting which calls
could be made compensated for the absence of error wrapping — a narrow set of
well-understood calls is tolerable to use raw; the full syscall table is not.

**D-001 removed `sys!!!` and made everything wrapped.** The whitelist's entire
justification went with it. What remains is two tiers distinguished only by an
arbitrary list of which syscalls are "common enough".

### Recommendation: one `sys`

```nitpick
Result<int64>:r = sys(READ, fd, buf, size);
```

The escalation ladder collapses because both surviving rungs now sit on the same
rung of the axis that mattered.

Keeping two tiers would mean every author remembering **which list a call is on**
— exactly the per-case knowledge the blueprint philosophy exists to eliminate —
in exchange for a boundary that any code can step over by writing `sys_full`.

### Where the two real safety properties go instead

The whitelist was carrying two things worth keeping. Neither belongs in the
syscall primitive.

**1. Restricting which syscalls a binary may make → `--seccomp`.**

`FORMAL_DRAFT` 14.5.3 already provides `--seccomp`, embedding a seccomp-bpf
sandbox with a syscall allowlist into the binary. That is **kernel-enforced at
runtime** and cannot be bypassed by choosing a different spelling in source.

A compile-time whitelist with an escape hatch beside it is strictly weaker: it
constrains only the author who chooses to stay inside it. Seccomp constrains the
process. For Nikola's mini-VM isolation this is the mechanism that matters
anyway.

**2. Argument-level filtering → the typed API above it.**

`sys_safe`'s genuinely valuable behaviour was not the syscall list but the
per-argument constraint — `SYS_IOCTL` admitted only for three specific request
codes, everything else rejected with `-EINVAL`.

That belongs in the typed layer, and **`libn` already has it**:
`io_set_cloexec`, `io_set_nonblocking`, `io_set_append`, `io_fcntl_setown`, and
the rest express the permitted operations as named functions. A caller reaching
those cannot pass an arbitrary `ioctl` request code because the API has no slot
for one. Under D-044 the flag types make this stronger still.

So the constraint is better expressed as **"there is no function that does the
wrong thing"** than as **"the primitive rejects it at runtime"**.

### Consequences

- `BUILTIN_REFERENCE.md` §3 — one entry, `sys(CONST, ..*int64[]) → Result<int64>`.
  The unspecified "curated whitelist" question dissolves rather than needing an
  answer.
- **D-047's extraction task changes shape**: `sys_safe`'s syscall list is no longer
  needed, but its **`ioctl` argument constraints must be checked against `libn`'s
  typed `io_*` wrappers** to confirm nothing is lost. That check still happens.
- `--extra-picky` gains **`no-sys`**, banning direct syscalls in high-level
  application code the way `no-wild` bans manual memory.
- `FORMAL_DRAFT` 10.2's three-tier section is struck entirely.
- `sys_full` never enters the language, so D-046's `sys!!` → `sys_full` rename
  becomes moot — there is only `sys`.

### Note on `!` and `*`

Accepted as-is. `!` reading as both negation and emphasis matches ordinary usage —
it marks *not* and it marks *attention* or *danger* in plain writing too. `*`
carries multiplication and C pointer syntax, but the second is **confined to
`extern` blocks**, which contains it in the place where C conventions are
expected anyway. Neither is a deal-breaker, and both are documented rather than
implicit.

---

## D-049 — `cstring`: a NUL-terminated string type, not a convention — **SETTLED**

`as_cstring(string) → char8[]` is replaced by **`to_cstring(string) → Result<cstring>`**,
producing a distinct type.

### Why the `char8[]` form cannot stay

`TYPE_REFERENCE.md` §3.1: a Nitpick `string` is `{ptr, len, cap}` — length-carrying
and **not NUL-terminated**. Every kernel-bound string therefore needs a
conversion, and today's answer produces a `char8[]`.

A `char8[]` does not carry the guarantee. It is an ordinary char array; one built
by hand need not end in `0u8`. The termination lives in `as_cstring`'s
*behaviour*, not in the type, so nothing stops a caller passing an unterminated
array to `execve` and nothing in the type system flags it. This is the D-042
argument exactly: an `fd` is always valid because `-1` is not representable, and
a `cstring` should always be terminated because there is no way to build an
unterminated one.

### The property that makes this a safety decision, not ergonomics

A `string` may contain `0u8` **anywhere in its interior** — it is length-carrying,
so an embedded NUL is just a byte. Converting such a string to a NUL-terminated
form silently truncates at the first one, and the caller has no indication.

That is the **poison-NUL** class of vulnerability, and it is a disagreement
between two layers about where a string ends:

```
attacker supplies:   "avatar.png\0.sh"
validator sees:      len 14, suffix ".sh" rejected — or suffix ".png" accepted,
                     depending on which end it checks
kernel sees:         "avatar.png"        — stops at the NUL
```

Any check performed on the length-carrying `string` and any use made of the
NUL-terminated bytes are examining **different strings**. Historically this has
produced authentication bypasses, extension-check bypasses, and path-validation
bypasses across essentially every language that talks to a C API.

**`to_cstring` rejects an interior NUL.** It is the whole reason the conversion
is fallible, and it is enforced once at the boundary rather than at every
validator.

### Representation

```
cstring   { ptr: wild char8->, len: int64 }
```

The buffer is `len + 1` bytes with `buf[len] == 0u8`. **The length is retained**,
which is strictly better than C and matters for verification:

- `nlibc` never calls `strlen`. The **unbounded "scan forward until NUL" read
  disappears** from every path, name, and argument in the library — the pattern
  static analysers cannot discharge, and the one Astrée would otherwise stall on.
- Bounds are known before the call, so every length precondition is checkable.
- `ptr` is still exactly what the kernel wants, so there is no cost at the syscall
  boundary.

`cstring` is immutable. Mutation could break the terminator invariant, and no
caller needs it — building strings is `string`'s job, and conversion happens once
at the edge.

### Literals: checked at compile time, exactly like `fmt`

Requiring `to_cstring("/bin/ls")?` at every call site would add a runtime check
and a `Result` for something the compiler already knows. So `cstring` has two
inhabitants:

| Source | When checked | Cost |
|---|---|---|
| **string literal** in `cstring` position | compile time — interior NUL is a compile error, terminator emitted into the constant | zero |
| **`to_cstring(s)`** for a runtime `string` | runtime — interior NUL is `Result.err` | one scan |

This deliberately mirrors **D-045's `fmt`**: a type inhabited by literals whose
constraint the compiler discharges. Two types, one mechanism — nothing new for a
reader to learn.

Implicit conversion in the other direction does not exist. `cstring` → `string`
is an explicit `to_string`, because it copies.

### Naming

`to_`, not `as_`. The conversion allocates and can fail; `as_` reads as a free
reinterpretation of existing bytes, which this is not. This also matches the
`to_cstring` spelling intended for the string API.

### Consequences

- `TYPE_REFERENCE.md` §3.2 — the `as_cstring` row is replaced; §3.1 gains
  `cstring`'s layout.
- Every path, name, and kernel-bound string parameter in `nlibc` becomes
  `cstring`: `open`, `stat`, `execve`, `getenv`, `chdir`, `mkdir`, `readlink`,
  `unlink`, and the rest.
- `EXEC_FAMILY.md` §3's open question is answered; those signatures take
  `cstring`.
- `char8[]` remains an ordinary array type. It simply stops being the thing
  handed to the kernel.

---

## D-050 — Line endings are a property of a stream, never of a string — **SETTLED**

A `string` holds the bytes it holds. Whether those bytes contain `\n` or `\r\n`
is not something its type can or should express.

Line endings matter at exactly two moments: **reading** text and **writing** it.
Both are stream operations, so that is where the policy lives.

- **Reading text normalizes.** A text reader accepts `\r\n`, `\n`, and a lone
  `\r`, and yields `\n`. Code downstream of a read never branches on platform.
- **Writing emits `\n`** unless the writer was explicitly opened requesting
  platform-native or `\r\n` endings. The opt-in is greppable, in one place, and
  named at the point the stream is created.

Binary streams do no translation at all, ever. There is no "text mode by
accident."

### Why not a string type

A string type that carried a line-ending discipline would mean different things
depending on which OS produced it — a construct whose meaning varies by context,
which the blueprint philosophy rejects outright. It would also be unenforceable:
concatenating a `\n` string and a `\r\n` string yields something that is neither,
with no place to report the error.

Normalizing at the boundary means there is exactly one representation in memory
and one rule to remember.

---

## D-051 — No `ostring`; portability lives in a `Path` type above `nlibc` — **SETTLED**

The proposal was an `ostring` type handling OS-specific string concerns — line
endings, path syntax, and a future Windows build. Those are **three separate
concerns**, and bundling them into one type would produce exactly the
context-dependent construct the blueprint philosophy exists to prevent: what
`ostring` *meant* would depend on which of the three you were using it for.

Taken separately:

| Concern | Where it belongs |
|---|---|
| Line endings | the stream (D-050) |
| Path syntax — separators, roots, traversal | a **`Path`** type |
| Kernel string encoding | `cstring` (D-049) on Unix; a Windows backend's problem |

### Why `nlibc` says `cstring` and never needs to change

`nlibc` **is** the POSIX syscall surface. Linux syscalls take NUL-terminated byte
strings with no encoding guarantee, so `cstring` is not an approximation there —
it is precisely and permanently the right type.

A Windows build of Nikola would not route through `nlibc`'s syscall layer at all;
it would need a Win32 backend, where the OS string is UTF-16LE and essentially
every call differs. Putting an `ostring` in `nlibc` would be future-proofing the
wrong layer — adding a representation that the verified Linux target never uses,
widening the state space Astrée has to consider, for a platform that would
replace the module wholesale anyway.

### `Path` earns its place on Linux today

This is not a Windows-only concern deferred until Windows exists. Building paths
by string concatenation is a live defect source right now:

```nitpick
string:p = dir + "/" + name;      // "/etc/" + "/passwd" → "/etc//passwd"
                                  // name = "../../etc/shadow" → traversal
```

A `Path` type that cannot be built by naive concatenation prevents both. It
carries:

- **join** that cannot produce doubled or missing separators
- **explicit** absolute-vs-relative status
- **normalization** with `..` resolution, so a traversal check inspects the same
  path the kernel will open — the D-049 poison-NUL lesson applied to `..`
- `parent`, `basename`, `extension` as operations rather than string surgery
- **`to_cstring`** as the single, explicit way it reaches a syscall

`Path` lives in the **stdlib, above `nlibc`**. That placement is what makes a
Windows port a matter of adding a backend rather than re-signing every function:
`nlibc` keeps saying `cstring` because it is the Unix layer, `Path` is what
portable code holds, and the conversion between them is the seam.

**Decided now, built later.** The interface above is fixed, so `nlibc`'s
signatures are final and no downstream churn is pending — `Path` can be
implemented on the frontend's schedule without any signature being revisited.

---

## D-052 — Format strings are lowered at compile time, not parsed at runtime; `%n` never exists — **NARROWED by D-053**

> **Still correct, with no format strings left to apply it to.** D-053 removed
> the format language, so the lowering mechanism below now applies to **`&{ }`
> interpolation**, which lowers to the same straight-line typed emitters. The
> `%n` prohibition becomes moot rather than wrong. The evidence gathered here —
> the erased `%s` read, the discarded length modifiers, the off-by-one width — is
> what led to D-053.


D-045 established that `fmt` is inhabited only by string literals and that the
compiler checks each specifier against its argument's type. This settles what the
compiler does with that knowledge: **it lowers the format string to a
straight-line sequence of typed emitters.** No runtime format parser exists.

```nitpick
str_snprintf(buf, size, "x=%d y=%s\n", x, name)
```

becomes

```
fmt_lit(st, "x=", 2)
fmt_i64(st, x, <flags, width, precision as constants>)
fmt_lit(st, " y=", 3)
fmt_str(st, name)
fmt_lit(st, "\n", 1)
```

Each emitter is monomorphic; its flags, width, and precision are compile-time
constants. `%*` and `%.*` remain expressible — the width becomes a runtime
integer parameter, type-checked like any other argument.

### Why checking is not sufficient on its own

`libn`'s formatter demonstrates the limit. `str_format_args` bounds-checks its
argument index correctly at all three read sites, so too few arguments degrades
to zeros rather than reading out of bounds — the failure mode most hand-written
formatters get wrong, handled properly.

It still contains an arbitrary-read primitive, because every argument arrives
erased to `int64`:

```nitpick
str_snprintf1(buf, size, "%s", 42i64)   // passes every bounds check
```

The engine then dereferences `42` as a pointer and scans for a zero byte. The
information needed to reject that was destroyed at the call site; no check inside
the engine can recover it.

Checking at compile time fixes the call. **Lowering removes the engine**, and
with it:

- the ~300-line format state machine, which otherwise has to be proven correct
  and proven unreachable-into-type-confusion for every call site;
- the erased argument vector, so arity mismatch is unrepresentable rather than
  merely handled;
- the `strlen` scan behind `%s`, since the emitter uses the length carried by
  `string`/`cstring` (D-049). No format operation performs an unbounded read.

The trade is a shared runtime parser for per-call-site code — five calls instead
of one for a four-specifier format. That is the right side of the trade here:
format strings are short, the emitters themselves are shared, and the alternative
is carrying an interpreter through verification. The engine is code that must be
proven correct; the lowering is code that cannot be wrong in that way.

### `%n` is permanently prohibited

`%n` writes the running output count through a pointer argument. It is the reason
format-string bugs escalate from information disclosure to arbitrary write and
then to code execution.

`libn` does not implement it. That becomes a **decision rather than an
accident**: `%n` is never added to Nitpick's format language, for any caller, at
any privilege level, including behind `raw` or `wild`. There is no legitimate use
that a return value does not serve better — every emitter already returns what it
wrote.

### Consequences

- The `fmt` machinery in the frontend gains a lowering pass, not just a checker.
- `nlibc` deletes `str_format_args` and every arity variant; no erased entry
  point survives, since one beside the checked wrappers would repeat the
  `sys_safe` mistake of a typed API with an untyped bypass.
- **`scanf` is where this pays most.** A mismatched `printf` specifier reads
  through a bad pointer; a mismatched `scanf` specifier *writes* through one.
  Lowering emits a typed reader per specifier, so `%d` paired with a `string` is
  a compile error rather than a four-byte write into a string header.

### A guarantee that follows: formatted output never allocates

Because every emitter's output length is either a compile-time constant (literal
text; the digit bound of a numeric specifier) or a value already in hand
(`string` and `cstring` carry `len`, D-049), the **total output size is a sum
computed before emitting** — not a measuring render.

`libn` establishes what the alternative costs. Its `io_fprintf` formats twice —
once into a null buffer to obtain the length, once for real — then writes, and
past 4096 bytes of output allocates a heap buffer, renders into it, writes, and
frees. So `printf` currently has an **`ENOMEM` failure path**.

Under lowering, output accumulates into one stack buffer sized from the computed
bound, flushing in chunks if it exceeds it. **Formatted output performs no
allocation and cannot fail for want of memory.** Removing a failure mode from the
call used *to report failures* matters more than the cycles saved.

One constraint on the implementation: emitters must accumulate and flush once,
never issue a write per emitter. Per-emitter writes would turn
`printf("x=%d\n", n)` into three syscalls and destroy output atomicity between
concurrent writers. Buffered streams accumulate into the `FILE` buffer instead —
same shape, different destination.

### The `scanf` side: the store width comes from the destination's type

`libn`'s scanner shows why checking the specifier is not enough on the input
side either. It parses length modifiers and **discards them** — its own comment
reads *"Skip length modifiers (l, h, hh, ll, z, j, t) — all use int64 anyway"* —
and every integer conversion ends at the same store, writing **8 bytes**:

```nitpick
int32:n;
sscanf1(str, "%d", cast_unchecked<int64>(@n));   // 8 bytes into a 4-byte slot
```

`%d` means 32 bits in C, so that call is the conventionally *correct* spelling
and it overflows by four bytes. `%hhd` into an `int8` overflows by seven.

Under lowering the modifier is not needed and not consulted: `%d` against an
`int32->` emits `scan_i32`, which stores four bytes because that is what its
parameter type is. A specifier that disagrees with its destination is a compile
error rather than a silently wider write.

The same applies to string input. C's `%Ns` reads N characters plus a NUL, so the
number in the format string is the buffer size *minus one* and `char8[32]` paired
with `%32s` overflows by one. Under lowering the bound comes from **the
destination's own length**, so there is no number to transcribe and nothing to
get off by one. A width specifier on `%s` becomes redundant and, if written, must
agree.

`%n` is prohibited on the input side for the same reason as the output side, and
with the same replacement: the consumed count is a return value or a typed
accessor, never a pseudo-conversion that writes through a caller pointer. It
exists in C only because there is no other way to retrieve it.

### The general statement

**C's format string carries a description of the arguments, and the arguments
carry no description of themselves.** Every defect above — the erased `%s` read,
the discarded length modifier, the off-by-one width — is that description being
wrong, absent, or thrown away.

Lowering deletes the description and reads the arguments directly. On the output
side that converts an arbitrary read into a compile error; on the input side it
converts three classes of arbitrary write into compile errors.

---

## D-053 — Formatting is functions, not a format language — **SETTLED**

**Supersedes D-045** (the `fmt` type) and **narrows D-052** (format-string
lowering). `printf`, `scanf`, and every relative are removed from `nlibc`.

Output is `print` / `println` / `fprint` / `eprint`, each taking **one `string`**.
Formatting happens before the call, as ordinary functions returning `string`,
spliced by `&{ }` interpolation:

```nitpick
println(`Total: &{x.prec(2).pad_left(8)}  Name: &{name.pad_right(20)}`);
```

### This restores the original design

The prototype's print took only a string, and formatting was handled separately.
`nitpick/stdlib/fmt.npk` already implements it — `fmt_int`, `fmt_bool`,
`fmt_hex`, `fmt_float(x, decimals)`, `fmt_pad_left`, `fmt_pad_right`,
`fmt_repeat`, each taking a typed value and returning a `string` — and its header
records that it uses **no extern calls, only compiler builtins**. The `printf`
family in `libn` was added by the implementation, not designed in.

`fmt.npk`'s own `fmt` / `fmt2` / `fmt3` / `fmt4` placeholder-substitution helpers
are **not** carried over: they are a template mini-language, arity-expanded,
which interpolation replaces exactly.

### The reasoning: what to print, separate from how to print it

A format string fuses three things into one literal. `"%-8.2f"` is at once the
output text, the layout instruction, and a type assertion about an argument
sitting elsewhere in the call. Every format defect found while porting `libn` is
that fusion coming apart:

| Defect | The fused thing that disagreed |
|---|---|
| `%s` against an integer → arbitrary read | type assertion vs. actual type |
| length modifiers discarded → 8-byte store into 4 bytes | width assertion vs. destination |
| `%32s` into a `char8[32]` → one-byte overflow | bound assertion vs. buffer |
| more specifiers than arguments | count assertion vs. arity |

Separating them means there is nothing left to disagree. `x.prec(2)` returns a
`string`; `pad_left(8)` pads a `string`; interpolation splices a `string`. Each
step is checked by the ordinary type checker, and the value's type is consulted
directly rather than described in a second language.

### What this removes from the compiler

D-052 established that a format string should be lowered at compile time rather
than parsed at runtime. D-053 goes one step further: **there is no format
language to lower.**

- No runtime format parser — already true under D-052.
- **No compile-time specifier grammar either**, and no specifier/argument
  checker. Option "add format specs to interpolation" (`&{x:>8.2}`) was rejected
  for precisely this reason: it would reintroduce a second mini-language, with
  its own grammar, its own diagnostics, and its own conformance surface for
  Astrée, purely to save characters.
- **`fmt` (D-045) has no remaining consumer** and is removed from the language.
  Its only users were the format-directed variadics. A literal-only parameter
  type may be worth having some day; it needs its own justification then, not a
  vestige now.
- **Format-directed `..*` is removed.** Homogeneous `..*` survives — the `sys`
  builtin, `sys(CONST, ..*int64[])`, is its consumer.

D-052's lowering mechanism itself survives and is still correct; it now applies to
**interpolation**, which lowers to the same straight-line typed emitters. Its
`%n` prohibition becomes moot rather than wrong — there is no `%` language for
`%n` to be part of.

### Complex composition uses a string builder

For output too complex to read well inline, the answer is a builder, not a
richer format string. `libn`'s `StrBuf` (27 public functions, C-free) is that
tool and stays — minus `strbuf_appendf`, which is format-directed and goes with
the rest.

The prototype's `stdlib/string_builder.npk` is **superseded**: it depends on an
`extern "nitpick_libc_string"` block of three C functions, where `StrBuf` does
not.

### Cost, stated plainly

`` `&{x.prec(2).pad_left(8)}` `` is longer than `"%8.2f"`. That is the trade, and
it is accepted: the shorter form is shorter because it omits the information the
type system needs, and every defect in the table above is the consequence of
that omission.

---

## D-054 — The `Path` type — **SETTLED**

D-051 fixed `Path`'s place in the layering — stdlib, above `nlibc`, reaching a
syscall by `to_cstring` — and deferred nothing but its shape. This is the shape.

```
Path   { bytes: char8[] }      // absolute, lexically normalized, no interior NUL
```

### Four invariants, each removing a defect class

**1. A `Path` is always absolute.**

Constructing one from a relative string requires an explicit base:
`Path.from(base, relative)`. There is no way to obtain a relative `Path`.

This makes CWD-relative opens **unrepresentable**. The working directory is
process-global mutable state, so a relative path resolved at open time is a race
against any other thread calling `chdir` — and in a system with Nikola's
concurrency that is not hypothetical. Relative fragments stay ordinary `string`s
until they are joined onto a base, at which point they become absolute.

**2. `join` takes a component, not a fragment.**

```nitpick
Result<Path>:p = base.join(name);    // name is ONE component
```

A component containing `/` or `0u8` is an **error**, not a path. That single rule
is the traversal defense: `join("../../etc/shadow")` cannot construct a path
outside `base`, because it cannot construct a path at all. Contrast the string
form, where `dir + "/" + name` silently accepts whatever `name` holds.

**3. Bytes, not `string`.**

Linux paths are arbitrary byte sequences and are not required to be valid UTF-8.
A `readdir` that cannot represent what the kernel returned is a library that
cannot report what is on disk. So `Path` holds `char8[]`, and `to_string()` is
**fallible** — it fails on invalid UTF-8 rather than lying.

This is also the second half of D-051's answer on `ostring`. Non-UTF-8 byte
strings realistically appear in exactly one place — filesystem paths — and `Path`
is that place. A general OS-string type is not needed to cover it.

**4. No interior NUL**, checked at construction, so `to_cstring` at the syscall
boundary cannot fail for that reason and the D-049 poison-NUL split cannot
reopen between a path check and a path use.

### Lexical normalization is not kernel resolution — and this must be loud

`Path` normalizes `..` **lexically** at construction, so `/a/b/../c` becomes
`/a/c`.

**That is not what the kernel does when a symlink is involved.** If `/a/b` is a
symlink to `/x/y`, the kernel resolves `/a/b/../c` to `/x/c`, not `/a/c`. A
security check performed on the lexically normalized path therefore inspects a
**different path than the one that gets opened** — structurally the same failure
as the poison-NUL split in D-049, with `..` in place of `\0`.

So lexical normalization is for **hygiene**, never for authorization:

- `resolve()` performs kernel-level resolution and returns the real path.
- **A containment check must not use lexical normalization.** The correct
  mechanism is `openat` traversal: open the base directory, then open each
  component with `O_NOFOLLOW` relative to the previous fd. That is TOCTOU-safe,
  where "normalize, compare prefix, then open" is not — the filesystem can change
  between the compare and the open.

`Path` exposes that traversal as the supported way to open something under a
base. The lexical form exists so paths print and compare sensibly, and the
documentation must say plainly that it proves nothing about containment.

### Why this is worth building before it is needed

Not Windows preparation. Every defect above is reachable on Linux today, and
three of them — CWD races, `..` traversal, and the symlink split — are the
classic filesystem vulnerability set. Building the type after `nlibc`'s callers
exist would mean revisiting them; building it now costs nothing downstream,
because D-051 already fixed the boundary at `to_cstring` and `nlibc`'s signatures
do not change.

---

## D-055 — GPU and GUI run out of process; `#[gpu_kernel]` is a codegen target, not a call — **SETTLED — and generalized by D-149**: the out-of-process rule now covers ALL foreign code, not only GPU and GUI; this section's architecture is the template the general rule reuses

`MACRO_AUTHORING_GUIDE.md` documents `#[gpu_kernel]` and `#[gpu_device]`, which
`PROTOTYPE_DELTA.md` §5 flagged as conflicting with the zero-dependency rule.
They do not, given the architecture — but their **meaning has to be narrowed** so
they cannot reintroduce the conflict.

### The architecture

Anything requiring GPU access — CUDA included, which Nikola requires — and any
complex GUI runs as a **separate server process**, not linked into the Nitpick
program.

This is not a workaround for the dependency rule. It satisfies the rule's actual
purpose. The prohibition exists because **past the FFI barrier the runtime cannot
intercept a fault and route it through `failsafe`**. Process isolation restores
precisely that: the vendor runtime lives elsewhere, so its failure arrives as a
closed socket or a reaped child — **a value**, handled through `Result<T>` like
any other error. A segfault inside libcuda becomes an error return rather than a
dead process with actuators live.

The architecture therefore draws a **TCB boundary**. The Nitpick process is
verified; the server is explicitly outside the trusted computing base and does
not need verification, because its failure is contained and observable. That is a
stronger position than in-process CUDA could ever reach, since no amount of
verification makes a linked vendor blob analysable.

The rejected alternative — writing our own NVIDIA drivers and CUDA libraries —
would move an undocumented, enormous surface *into* the TCB. Worse on every axis
that matters.

### What the attributes mean now

`#[gpu_kernel]` marks a function as a **codegen target**, not a callable:

- The body compiles to GPU ISA through LLVM's NVPTX or AMDGPU backend. **No
  vendor toolchain is required for this** — LLVM 20 emits PTX directly, so
  kernels stay written in Nitpick, stay type-checked, and stay analysable.
- **No host-callable symbol is emitted.** This is the load-bearing part: if
  `#[gpu_kernel] func:foo` could be called as `foo(x)`, a process boundary and a
  vendor runtime would sit behind something that looks like an ordinary call —
  exactly the hidden FFI crossing that breaks failsafe reasoning. A kernel is
  **dispatched**, never called.
- Kernel bodies are more constrained than ordinary functions, not less: no
  allocation, no syscalls, no unbounded recursion.

`#[gpu_device]` marks a device-side helper — same restrictions, callable only
from kernel or device code.

Launching is an explicit dispatch to the server, returning `Result<T>` that
reflects the round trip.

### Four requirements on the boundary

**1. Every dispatch carries a deadline.** A hung kernel is worse than a crashed
one — a crash is observable, a hang is not, and Nikola may have actuators live.
Timeout is an ordinary error, not a special case.

**2. No partial results.** A failed or retried dispatch must never yield output
that looks complete. Nikola's entire safety rationale is that small numeric drift
produces behaviour resembling PTSD or schizophrenia; a half-finished GPU
computation is exactly that drift, arriving through the door the safety
architecture was built to close. Dispatch is all-or-nothing, and a retry is only
permitted where the operation is idempotent.

**3. The server is a supervised child**, spawned and monitored by the Nitpick
runtime, so its lifecycle is a `Result`-typed concern rather than an external
dependency that may or may not be running.

**4. GUI failure must never stop control logic.** For a companion robot the
display crashing cannot take the robot with it — an independent argument for the
same split, and a reason the GUI server is separate from the GPU compute server
rather than sharing one process.

### Portability note

Nothing here depends on CUDA specifically. Vulkan compute with SPIR-V would fit
the same boundary unchanged, since the isolation is what provides the safety
property, not the API behind it. The choice affects portability and vendor
lock-in, not the safety architecture — so it can be revisited without disturbing
anything decided here.

---

## D-056 — Deadlock: lock levels prove the common case, deadlines contain the rest — **SETTLED**

> **`Mutex<T, LEVEL>` and `Guard<T>` LANDED at 1.1.11** — builtin type kinds
> like `Channel`, one managed cell per mutex ([lock | listlock | waiters | T]),
> the value one pointer. `acquire` is the sole operation: `async`,
> deadline-mandatory, returning `Result<Guard<T>>`; there is no `release`
> method and no `try_acquire` — the guard's scope-exit DROP is the release
> (the first drop in the language that frees no memory, and the thing cycle
> 1.2 existed to make possible), and a zero deadline is "do not wait".
> `guard.value` reads and writes the element through the cell for exactly the
> guard's lifetime. The waiter protocol reuses the channel's (frame links,
> owner rouse); a woken waiter re-runs the acquire and may lose to a barger,
> which the deadline bounds. Three sharpenings the build forced:
> **the acquisition is TYPED, not named** — the 0.5.6 doctrine's "contract,
> not a name" was written against an `int64` handle, and the receiver's type
> now carries the level to the ordering analysis directly, no clause needed
> at the site; **a borrow of a mutex crosses a spawn** — D-180's hazard
> (mutation the holder cannot see, at a suspension it did not choose) is
> answered by the lock itself, so `Mutex<T, LEVEL>->` is the sanctioned
> crossing while `Guard<T>->` stays refused; and **`acquire` the reserved
> word interns its own spelling in name position** — it was reserved FOR this
> operation, and a keyword token carries no intern, so `.acquire(` is its one
> legal home.
>
> **The rest of the table landed at 1.1.11b.** `RwLock<T, LEVEL>`: `read`
> returns `RGuard<T>` — a shared hold whose `.value` is READ-ONLY (a write
> through it is refused as the unsynchronized mutation the write lock exists
> to serialize) and whose drop releases a reader; `write` returns the same
> `Guard<T>` a mutex does, because an exclusive hold is ONE meaning
> everywhere — which is why the cell carries a KIND and one
> `npk_guard_release` chooses the wake policy (a mutex wakes one, a writer
> release wakes the crowd). `CondVar<LEVEL>`: `timedwait(guard, deadline)`
> LENDS the guard, releases its mutex link-first (no lost signal), and
> reacquires under the same absolute deadline; on DeadlineExceeded the guard
> is SPENT — holding a lock past an expired deadline is the unbounded
> acquire this decision forbids — so the lent guard's cell is nulled, its
> drop releases nothing, and the caller re-acquires explicitly. POSIX
> reacquires unboundedly here; Nitpick deliberately does not.
> `signal`/`broadcast` never suspend and never fail. `Barrier<N, LEVEL>` —
> the spec table wrote `Barrier<N>`, and this decision's own sentence
> ("every blocking primitive is levelled") corrects it — has `arrive` alone:
> the N-th arrival moves the GENERATION and wakes everyone; a timed-out or
> wound-up party has NOT arrived and hands its slot back under the cell's
> lock, so the next round completes instead of wedging one short. The
> ordering analysis was refined in kind: `acquire`/`read`/`write` yield
> HOLDS (binding one raises the held level for the rest of the block);
> `timedwait`/`arrive` are ordered against what the caller holds but yield
> none — binding their `Result<NIL>` raises nothing.

`--verify-concurrency` is documented as verifying "data race **and deadlock**
freedom". Data-race freedom is accounted for (D-004, D-017, D-032). **Deadlock
freedom had no mechanism anywhere** — the flag promised a safety property nothing
delivered, which is worse than promising nothing, because it invites reliance.

### Root cause: the lock API erases which lock is which

```nitpick
func:create = int64();              // stdlib/mutex.npk
func:lock   = int32(int64:handle);
```

A lock is an `int64` handle. At a call to `lock(h)` **nothing in the program text
says which lock is being acquired**, so no static analysis can order acquisitions,
and no reviewer can either.

This is the same erasure found in `printf` (every argument an `int64`, so `%s`
against an integer becomes an arbitrary read) and in `libn_ioctl` (an arbitrary
request code with the constraint checked three layers down). The fix is the same
one taken there: **put the information in the type.**

### `Mutex<T, LEVEL>`

```nitpick
Mutex<Config, 2>:cfg_lock;

{
    Guard<Config>:guard = relay await cfg_lock.acquire(deadline);
    guard.value.retries = 3i32;
}   // guard drops here; the lock is released
```

> **Amended by D-082.** This example originally used a `with (…) : guard { … }`
> construct that had no AST node and collided with `with errno`; and it predates
> D-071, so the acquisition was not awaited. A bare block already provides the
> scoping, and every blocking acquisition suspends the task.

Three properties, each removing a class:

**1. The mutex owns its data.** `T` is reachable only through a guard, so
"forgot to take the lock" is unrepresentable rather than a review item. The
guard's lifetime is the critical section and release is automatic under D-003 —
no unlock to forget, no early-return path that leaks the lock.

**2. The level is part of the type.** `LEVEL` is a compile-time constant, so at
every acquisition the compiler knows exactly which rank is being taken.

**3. Acquisition must strictly increase.** A thread holding level N may acquire
only levels > N. Circular wait — the Coffman condition every deadlock needs — is
then impossible by construction, because a cycle requires some thread to acquire
downward.

### What the analysis actually does

Whole-program, which is available since verification is whole-program anyway:

1. Every `Mutex<T, L>` has a statically known `L`.
2. For each function, compute the set of levels it may acquire, transitively
   through its call graph.
3. At each acquisition site the set of possibly-held levels follows from the call
   graph.
4. Reject any path that can acquire `L' ≤ L` while holding `L`.

**Dynamic dispatch is the hole.** A call through a trait object can reach
anything, so its acquisition set is unbounded. Rather than give up or silently
under-approximate, a dynamically dispatched call **declares its maximum
acquisition level** as part of the trait method's contract, and implementations
are checked against it. An undeclared method may not acquire at all.

### Every blocking primitive is leveled, not just `mutex`

Deadlock does not require a mutex. Two tasks blocked on each other's bounded
channel are deadlocked just as thoroughly. So `rwlock`, `condvar`, `channel`,
`barrier`, and any future blocking primitive carry a level and participate in the
same ordering. A rule that covered only mutexes would be a rule that looks like a
proof and is not one.

### Deadlines are mandatory — the same rule as D-055

Static ordering proves the common case. It cannot cover everything: dynamic
dispatch under a declared bound, priority inversion, a peer process that stops
responding, or a lock held across a boundary the analysis cannot see.

So **every blocking operation takes a deadline and returns `Result`**:

```nitpick
Result<Guard<Config>>:g = cfg_lock.acquire(deadline);
```

There is no infinitely blocking acquire. A residual deadlock becomes a **timeout
error at a known point** — a value, propagated or escalated to `failsafe` —
rather than a process wedged with actuators live.

This is deliberately the identical rule D-055 imposed on GPU dispatch, and for
the identical reason stated there: **a hang is worse than a crash, because a
crash is observable and a hang is not.** One rule covering every blocking
operation in the language, with no per-primitive exceptions to remember.

### `create_recursive` is removed

Recursive mutexes exist to paper over unclear ownership — code that cannot tell
whether it already holds a lock. With the mutex owning its data and guards
scoped, that uncertainty does not arise, and a recursive acquire would defeat the
level discipline's central invariant. If a genuine need appears it requires its
own justification, not a default.

`trylock` stays: a non-blocking attempt is a legitimate deadlock-avoidance tool
and cannot itself block.

### The flag says what it proves

`--verify-concurrency` verifies **data-race freedom and lock-order freedom**.
It does **not** claim deadlock freedom outright, because the deadline backstop is
containment, not proof.

That distinction is the point of the whole decision. An honest narrow guarantee
plus a stated containment mechanism is worth more than a broad claim nothing
backs — particularly here, where the person relying on it is deciding whether a
robot near a child can wedge.

---

## D-057 — Macro hygiene and expansion order — **SETTLED**

The macro system is far larger than either document describes, and **its
specification exists only in regression tests** — 49 `.npk` files under
`nitpick/tests/`, carrying the semantics in comments keyed to decision codes
(`MACRO2-DEC-001…007`, `COMPTIME-006/007`). No user code in `stdlib`, `ARCHIVE`,
or the examples declares a macro; the entire corpus is tests.

That is why it looked unspecified. It is not unspecified — it is **specified in
the wrong artifact**, one that ships with no prose and cannot be read as a whole.

### What actually exists, none of it documented

| Capability | Evidence |
|---|---|
| Hygiene with defining-scope capture | `MACRO2-DEC-004/005` |
| `NITPICK-061 MACRO_HYGIENE_VIOLATION` diagnostic | `MACRO2-DEC-007` |
| `#caller(NAME)` explicit opt-out | `MACRO2-DEC-006` |
| Emitting **multiple declarations** | `bug591`, `bug592` (with cross-references between them) |
| **Struct field splicing** | `bug595`, `bug597`, `bug598` |
| **Impl method splicing** | `bug596`, `bug599` |
| Module-level invocation and nesting | `MACRO2-DEC-001` |
| Macro ↔ `comptime` interaction, both directions | `COMPTIME-006/007` |

The K semantics model none of it — `nitpick.k` handles zero- and one-argument
substitution only, and `SEMANTIC_GAPS.md` rates macro expansion "Low". The 40-line
spec and the authoring guide describe a fraction.

**`macro:` stays.** Splicing fields into a struct and methods into an `impl`, and
emitting mutually-referencing declarations, are things generics, `comptime`, and
`#[derive]` cannot do. Months of regression tests exist because the feature is
load-bearing, not vestigial.

### Hygiene: the default is backwards, and flips

Today, when an identifier in a macro body resolves to a **different symbol** in
the defining scope than at the call site, the compiler emits `NITPICK-061` and
**keeps the caller's binding**. `bug603` states the intent plainly: *"the macro
doesn't use `#caller()`, so the back-compat path keeps the caller-scope binding
while NITPICK-061 surfaces the hazard."*

A back-compat path, not a design. And it is precisely the failure the blueprint
philosophy exists to prevent: **the macro means something different depending on
where it is invoked**, with a warning as the only guard. A warning is not a
mechanism; it is a request that someone be paying attention.

**Flipped:**

1. An identifier in a macro body resolves in the **defining scope**. Always.
2. If it does not resolve there, that is a **compile error** — never a silent
   fallback to the caller's scope.
3. **`#caller(NAME)`** is the sole way to reach the call site: explicit,
   greppable, and already spelled with `#`, so D-046 needs no adjustment here.
4. `NITPICK-061` disappears as a warning, because the hazard it reported is
   structurally absent rather than detected.

This is the standing pattern: explicit opt-outs (`raw`, `wild`, `=>!`,
`#caller`) are consistent with the philosophy; silent contextual variation is
not.

### Expansion order, recovered from the tests

1. **Module-level expansion iterates to a fixed point.** `bug594`: `outer!()`
   expands to `{ inner!(); f3 }`, and `inner!()` expands to `{ f1; f2 }` on the
   next iteration. The flatten loop repeats until no invocation remains.
2. **Expansion precedes `comptime` folding.** `COMPTIME-006`: a macro body
   containing `comptime(expr)` is folded *after* expansion.
3. **`comptime` delegates to the expanded AST.** `COMPTIME-007`:
   `comptime(double_it!(3))` expands first, then evaluates — and nests, as
   `comptime(add_one!(double_it!(10)))`.

One rule covers all three: **expansion always precedes evaluation, and runs to a
fixed point first.**

### Expansion is bounded, and this is new

A fixed-point loop must terminate, and nothing in the tests bounds it.
`macro:m = () { m!(); };` iterates forever, so **the compiler fails to
terminate** — unacceptable in a compiler under formal verification, where
termination is itself a property to be established.

Expansion therefore carries a **depth and iteration limit**, exceeding it being
an ordinary compile error naming the macro and the chain that reached the bound.
`--comptime-budget <N>` is the existing precedent for exactly this shape, so the
mechanism is already familiar rather than novel.

### `MacroPattern` in `pick` is removed

`FORMAL_DRAFT` 05 §5.6.2 allows `pick` arms to match macro invocations —
`MyMacro!(a, b) where (a > b)` — which `AST_REFERENCE` §8 carried as an open item
pending D-046's respelling.

It should not be respelled; it should go. **Macros expand to a fixed point before
semantic analysis, so by the time a `pick` executes no macro invocation exists to
match.** The pattern can never fire. No test exercises it, and nothing in the
corpus uses it.

If matching over AST fragments is genuinely wanted — for a self-hosted compiler
matching its own nodes — that is a different feature with different requirements,
and it needs its own design rather than a pattern form that is inconsistent with
the expansion order.

---

## D-058 — `Future<T>` is an internal lowering artifact, not surface syntax — **SETTLED**

`TYPE_REFERENCE.md` §17 defines `Future<T>` as a user-facing type. No chapter
uses it, and `CONCURRENCY_REFERENCE.md` §2.4 recorded its visibility as open.

**It is not user-visible.** The evidence is that nothing in the language produces
one:

| Construct | Yields |
|---|---|
| `await f()` | **`T`** — the inner type directly |
| `drop work()` (spawn) | nothing; the result is discarded |

There is no third form. A type no construct can produce and no signature can name
is not part of the surface language.

The prototype already behaves this way and says so. `type_checker.cpp`'s `AWAIT`
case returns the operand's type, with the unwrap sitting in a comment:

```cpp
// Async functions return i8* at the IR level but their semantic return
// type is already the inner T (set by checkFuncDecl). When Future<T> is
// added to the trait system, unwrap it here:
//   if (operandType->getKind() == TypeKind::FUTURE)
//       return static_cast<FutureType*>(operandType)->getInnerType();
```

A `FutureType` class exists; nothing routes through it. `Future<T>` was
aspirational, and the aspiration never landed.

### Why it should stay internal

Making it surface syntax means committing to everything a first-class future
implies — composition (`join`, `select`), cancellation, manual polling, and a
lifetime story for a value holding a suspended coroutine frame. Each is a
verification surface, and none has a caller asking for it.

It also **dodges a genuinely nasty interaction**. Every function returns
`Result<T>` (except `main` and `failsafe`), so a visible future forces an answer
to `Future<Result<T>>` versus `Result<Future<T>>` — where the first is "the task
may fail" and the second is "spawning may fail," and both are true. Keeping the
future internal means `await` yields `Result<T>` like every other call and the
question never arises.

### The expressiveness limit, stated

`drop work()` discards the result, so there is **no spawn-now-await-later**. Fan
out and collect is done through the existing `channel`: spawn tasks that send,
then receive N results. That is expressible today, is already specified, and
keeps the coroutine frame's lifetime inside the executor's arena (D-034) rather
than in a user-held value.

Recorded so the limit is a known consequence rather than a later surprise.

### Consequences

- `TYPE_REFERENCE.md` §17 stops presenting `Future<T>` as a user type. The IR
  shape `%Future = { ptr, ptr }` moves to `CONCURRENCY_REFERENCE.md` §2.4, where
  the coroutine lowering is described.
- `AST_REFERENCE.md` needs no `Future` type node.
- If a first-class future is ever wanted, it is a new feature with its own
  decision — not the completion of a half-built one.

---

## D-059 — `give` and expression-`pick` are kept — **SETTLED**

`give` yields a value from an expression-`pick` arm. Its provenance was doubted;
it is real, and it is in the prototype:

- `token.h`: `TOKEN_KW_GIVE, // give - yield a value from an expression-pick arm`
- the lexer keyword table and the parser
- a dedicated audit suite, `nitpick/TMP/audit037/`, with error cases
- `k-semantics/tests/core/215_pick_expr_wildcard_pass.npk`

It is **not** part of the original `pick` design. `pick` began as a pure switch
replacement — no implicit fallthrough, explicit fallthrough via labelled
conditions and `fall(label)` — and was deliberately **renamed from `switch` so
the C instinct would not carry over**. Expression-`pick` came later.

It also has **zero uses** in `stdlib`, `ARCHIVE`, or the examples. On that
evidence alone the case for removing it looked strong.

### Why it is kept anyway

**Uninitialized variables are a compile error.**
`UNDEFINED_STATE_PREVENTION.md`: *"Uninitialized variables → Compiler enforces
initialization."*

So the obvious alternative does not exist:

```nitpick
int32:r;                                  // COMPILE ERROR — no initializer
pick(x) { (1i32) { r = 10i32; } ... };
```

Without expression-`pick` there is **no way to initialize a variable by matching
on something.** The value would have to be computed by a helper function whose
body is a statement `pick` writing to a local it then returns — replacing a
checked construct with a hand-written one, in a language whose entire posture is
the opposite.

Expression-`pick` also makes exhaustiveness a **type-checked property**: the
construct must produce a value, so every path must give one. The statement form
plus definite-assignment analysis would have to prove the same thing indirectly.

### Rules, recovered from `audit037`

| Rule | Test |
|---|---|
| The pick must be **exhaustive** | `e_nonexh` — missing arms is an error |
| **All arms give the same type** | `e_mismatch` — `give 10i32` beside `give 99i64` is an error |
| `give` is legal **only inside an expression-pick arm** | `e_outside` — `give` at function level is an error |
| An expression-pick is usable as a **function argument** | `e_arg` |
| Expression-picks **nest** | `e_nested` |
| Tagged-union selectors work | `e_tagged` |

### The statement/expression boundary, resolved

`AST_REFERENCE` §8 recorded that `FORMAL_DRAFT` 04 §4.1 and 05 §5.1 state the
boundary inconsistently. Settled: **`pick` is both, and the arms decide.** Arms
containing `give` form an expression-pick — exhaustive, single-typed, usable
anywhere an expression is. Arms without form a statement-pick.

This is not context-dependent meaning: `pick` means "select one arm by matching
the selector" in both cases, and `give` is an explicit marker inside the
construct rather than a property of its surroundings. A construct that declares
its own nature is exactly the shape the blueprint philosophy asks for.

**Zero usage is not evidence of uselessness here** — it reflects that `libn` and
the stdlib were written before the feature existed, in a style that predates it.

---

## D-060 — Nitpick is statement-oriented; the expression forms are a closed list — **SETTLED; D-163 adds the STATEMENT-side closed list (checked since 1.1.0)**: a value-less statement is one of `drop f();` / `relay f();` / `f() ?! c;` / `f() ?| NIL;`, and a bare `f();` on a `Result` is refused — the statement-side counterpart of this decision**

Resolves conflicts **24** and **18**, which are the same question.

`FORMAL_DRAFT` 04 §4.1 says "almost every construct is an expression". 05 §5.1
says the opposite, and says it precisely:

> Statements in Nitpick represent actions that do not yield values (unlike
> expressions). The compiler strictly delineates between statement-level
> constructs and expression-level constructs. Attempting to use a statement (such
> as an `if` block) in an expression context is a compile-time error.

**05 is authoritative; 04 §4.1 is struck.** It reads as Rust-influenced drafting,
and nothing else in the corpus supports it — `AST_REFERENCE` already separates
statements from expressions, and the prototype rejects `if` in expression
position.

### The expression forms, enumerated

Literals, identifiers, unary/binary/comparison/logical operators, calls, member
access, indexing, casts (`=>`, `=>!`), address-of and dereference, the ternary
`is (cond) : a : b`, range and spread, the `$` iteration variable, string
interpolation, `await`, **`raw`**, **`drop`**, **`relay`** *(D-080)*, macro
invocation, `comptime(expr)`, and **`pick` whose arms `give`** (D-059).

> `raw` and `drop` were **missing from this enumeration** and should always have
> been in it — `int32:y = raw f();` is plainly an expression. Corrected alongside
> the addition of `relay`.

Everything else is a statement and yields nothing: `if`, `while`, `for`, `loop`,
`till`, `when`, `pick` without `give`, assignment, `defer`, `pass` / `fail` /
`return`, `discard`, `prove`, `assert_static`.

The list is **closed**. "Almost every construct is an expression" requires a
reader to know which constructs are the exceptions; an enumeration means the
question is answered by looking, which is the whole point of the blueprint
philosophy.

### Assignment is a statement (conflict 18)

`a = b = 5` does not parse. Assignment produces no value.

This matters more than chained assignment is worth. `FORMAL_DRAFT` 05 §5.3.1
currently carries a special rule *rejecting `=` in conditions* — the guard
against `if (a = b)`. **That rule becomes unnecessary**: if assignment is not an
expression, `if (a = b)` is not expressible, and there is nothing to reject.

A grammar that cannot express the bug is strictly better than a rule that
forbids it, because the rule is a thing to remember and to implement correctly in
one more place. It also removes a context-dependent case — assignment being an
expression *except* in conditions — which is exactly the shape the blueprint
philosophy rejects.

`AST_REFERENCE` already carries `AssignStmt` rather than an assignment
expression, so the AST needs no change; this makes the grammar agree with it.

---

## D-061 — The `(!)` unreachable pattern marker is removed — **SETTLED**

Resolves conflict **22**. `FORMAL_DRAFT` 05 §5.6.3 introduces `(!)` as a `pick`
arm pattern marking a case unreachable.

**Removed.** An unreachable claim is an assumption the compiler cannot verify in
general, and there are only two honest treatments:

- **Prove it unreachable** — in which case no marker is needed, because the
  exhaustiveness checker already knows.
- **Trap if reached** — which `#unreachable()` already does, lowering to
  `AssertStaticStmt(false)`.

So an arm the author believes cannot occur is written as an ordinary arm whose
body is `#unreachable()`. Explicit, greppable, and it traps rather than
silently proceeding.

### The reason this is not merely tidying

`(!)` lets an arm be **elided from exhaustiveness**. D-008 requires an `ERR:` arm
for every `tbb` selector, because `tbb`'s ERR sentinel is sticky and absorbing —
once a value is ERR, every operation on it yields ERR, so ERR is exactly the
state that propagates furthest from where it originated.

`(!)` on that arm is the author asserting "this value cannot be ERR." That is the
single least safe assumption available in the type, and the marker exists to let
it be written without a runtime consequence. Removing `(!)` means the required
ERR arm **cannot be skipped** — it is written, and if the author truly believes
it unreachable, `#unreachable()` traps and routes through `failsafe` rather than
falling through.

It also retires a second spelling of one idea, `(!)` and `#unreachable()`, in
favour of the one that already exists and already has defined semantics.

---

## D-062 — Task lifetime is lexical; preemptive cancellation is removed — **SETTLED**

Settles the **task cancellation** open item in `CONCURRENCY_REFERENCE.md` §6.

### The prototype implements cancellation twice, incompatibly

| | Mechanism | Runs `defer`? | Frame |
|---|---|---|---|
| **Preemptive** | `Executor::cancel(id)` → `coro.destroy()` (`src/runtime/async/executor.cpp:150`) | **no** | destroyed immediately |
| **Cooperative** | `CancellationToken`, polled by compiler-inserted code at every `await` (`include/runtime/async/cancellation.h`) | **yes** | unwinds normally |

`cancellation.h`'s own header states the split as deliberate: *"Separate from
preemptive cancellation (`Executor::cancel`) which destroys the frame
immediately. Cooperative cancellation allows the task to see the request and
execute defers/drops normally."*

Two disciplines for one job means remembering which one applies where — the exact
cost the blueprint philosophy exists to avoid. One of them has to go, and which
one is not a close call.

### Preemptive cancellation is removed

`Executor::cancel` destroys a live coroutine frame at a point the task did not
choose. Three separate failures:

1. **It runs no cleanup.** D-014 is precise about when `defer` does and does not
   run: it runs on *every normal exit path* and never on a trap. Preemptive
   cancel is neither — it is a third path that skips cleanup without being a
   fault. A task holding a `wild` allocation leaks it; a task holding a device
   handle leaves the device mid-operation. For a system with actuators live that
   is a physical safety event with no fault to explain it.
2. **It leaves an admitted dangling handle.** The implementation comment says so
   directly — *"After `destroy()`, the handle is dangling — the Task destructor
   must NOT call `coro_destroy` again, so we null the handle via a state check."*
   The invariant is maintained by a runtime state convention rather than
   structurally, which is precisely the shape D-056 rejected in the old `mutex`
   API.
3. **Nothing can call it.** D-058 makes `Future<T>` an internal lowering artifact:
   `await f()` yields `T` and `drop work()` discards, so **no surface construct
   produces a task handle**. There is nothing to name in a `cancel(x)` call. The
   operation is unreachable from the language it supposedly serves.

### Task lifetime is lexical

**A spawned task cannot outlive the scope that spawned it.** `drop work()` starts
the task concurrently, as before, but the enclosing `async` function does not
return until that task has finished.

This is a real change to `drop work()`. It previously read as fire-and-forget with
no stated lifetime at all; it now reads as *run concurrently within this scope*. A
task that must live for the whole program is spawned in `main`'s scope, which is
exactly as long-lived as "detached" ever meant in practice.

**This is what makes D-034 correct rather than merely stated.** D-034 allocates
frames from the executor's `arena<T>`, released on task completion. An arena is a
batch-lifetime allocator: it is the right structure precisely when lifetimes
nest. If a task may outlive an arbitrary scope, frame lifetimes are arbitrary,
and the executor's arena needs a free-list, reuse, and a generation counter —
rebuilding a general-purpose allocator inside the thing D-003 removed. Lexical
task lifetime makes frame lifetimes nest, and the arena is then doing the job
arenas are for.

It also matches D-004 exactly. Borrows are second-class: they pass down the call
stack and never up. Tasks now obey the same rule — they nest inward and never
escape outward. One rule covering both, rather than two.

### Scope exit joins, with a deadline

The joining scope asks each unfinished task to wind up, and the **cooperative
token is the mechanism** — the task observes the request at its next `await` and
takes a normal error exit, so `defer` runs, per D-014.

A task that never reaches another `await` never observes the request, so the join
carries a **mandatory deadline**, exactly as every blocking operation does under
D-056. There is no unbounded join. On expiry the scope exit **traps to
`failsafe`**; it does not detach the task and it does not continue silently.

Hanging is not the safe failure here. For a robot, a shutdown that never
completes is as dangerous as one that never started — which is why the deadline
is mandatory rather than optional, and why expiry is a fault rather than a
warning.

### The K-semantics `exit` rule already sets the precedent

`nitpick.k` does not let `exit` succeed with unmanaged resources outstanding.
`exit V;` completes only when `<wild-live>`, `<wildx-states>`, and
`<defer-stack>` are all empty; the very next rule routes an `exit` with a
**non-empty `<wild-live>` to `failsafe`** instead (`nitpick.k:4215-4237`). A
leaked `wild` allocation at exit is already a fault, not a silent success.

**A live task frame at shutdown is the same shape as a leaked `wild` allocation
at exit**, so it gets the same answer: the executor's task set joins the
emptiness precondition on `exit`, and a non-empty one traps.

### Two layers, the same shape as D-056

| Layer | Mechanism | Covers |
|---|---|---|
| **Structural** | lexical task lifetime — a task cannot outlive its spawning scope | makes "executor shuts down with live frames" unreachable in a well-formed program |
| **Containment** | deadline on join; non-empty task set at `exit` traps | the residue — a task that stops awaiting, or a frame live at exit anyway |

D-056 proves the common case with lock levels and contains the rest with
deadlines. D-062 proves the common case with lexical lifetime and contains the
rest with deadlines. Same problem shape, same answer shape, one thing to
remember instead of two.

### Consequences

- `Executor::cancel`, `TaskState::CANCELLED`, and the `tasksCancelled` counter
  are **not ported**.
- `CancellationToken` **is** ported, as the join mechanism rather than as a
  user-facing feature. It is not nameable from source.
- `work_stealing.cpp` (381 lines) was already dead under D-032; lexical lifetime
  gives a second independent reason, since a stolen task's scope lives on another
  thread.
- `gc_integration.cpp` (269 lines) and
  `tests/runtime/test_async_gc_suspended_frames_v03106.cpp` are dead under D-003
  and are not ported.
- No surface syntax is added. Lexical lifetime is a property of `drop work()`,
  not a new construct.

---

## D-063 — A trap is a whole-program event; no task is ever resumed after one — **SETTLED**

Settles the **`async` + `failsafe`** open item in `CONCURRENCY_REFERENCE.md` §6.

D-014 says a trap transfers control directly to `failsafe` without unwinding, and
that `defer` does not run. It was written for synchronous code and says nothing
about the tasks suspended alongside the one that trapped.

### No task is resumed — the trapping one or any other

On a trap, **no coroutine is resumed on any thread**, no `defer` runs anywhere,
and no frame is destroyed. Frames freeze exactly as they are.

The rationale is D-014's own, applied one step further. `defer` does not run in
the *trapping* task because at trap time the state of the system is unknown,
including how degraded it is, and running arbitrary cleanup against possibly
corrupt state in an order nobody chose is worse than not running it. Resuming a
**sibling** task is strictly worse again: it is not cleanup but arbitrary
application code — allocating, taking locks, driving hardware — run against the
same unknown state, and chosen by the scheduler rather than by anyone.

Freezing rather than destroying follows for the same reason. `coro.destroy()`
executes the frame's cleanup block, which is exactly the code D-014 forbids. The
frames are not freed either: the process is ending, the executor's arena is
preallocated, and freeing buys nothing that reaching `failsafe` sooner does not
buy more.

### `failsafe` never runs on an executor

`failsafe` runs on the trapping thread, as a plain call. It is **not a task, is
never scheduled, and is not `async`.**

A scheduler on a broken program is not trustworthy, and the handler that exists
to contain the fault must not be preemptable by the tasks it is containing. This
also keeps entry to `failsafe` deterministic, which D-014 lists as a primary
benefit.

### Other threads stop before `failsafe` runs

A trap on one thread stops every other thread's executor **before** `failsafe`
gets control. Threads park where they are without running further user code.

`libn`'s syscall layer already wraps what this needs — `tkill`, `futex`, and
`set_robust_list`. Two checkable constraints follow:

1. **A Nitpick thread may not block signals indefinitely.** A thread that could
   refuse the stop would leave application code running concurrently with
   `failsafe`, which defeats the purpose. Bounded blocking regions are fine;
   indefinite masking is a compile-time rejection.
2. **The stop must itself be bounded.** A thread that has not parked within the
   deadline is reported to `failsafe` as still-running rather than waited on
   forever — the D-056 discipline again.

Without this, `failsafe` safes an actuator while a sibling thread's task drives
it back the other way. That is not a hypothetical for a system with robotics.

### Async adds no new safing requirement — it makes the existing one visible

The tempting conclusion is that suspended tasks need some new cleanup path so
their resources get released. They do not, and adding one would be a mistake.

D-014 already established that **`defer` does not run on a trap in synchronous
code.** A synchronous function holding an actuator open therefore already cannot
rely on `defer` to safe it. The rule has always been that safing belongs to
`failsafe`, reached through **state preallocated before the fault** — the global
allocation registry `failsafe` receives intact, per D-014.

Async changes nothing about this. A suspended task is in exactly the position a
mid-call synchronous function is in, and gets exactly the same treatment. Stating
it once, uniformly, is worth more than a task-local cleanup path that would be a
second discipline for the same job and would run user code after a trap.

**The standing guidance sharpens accordingly:** anything whose abandonment is
physically unsafe must be reachable from `failsafe` without traversing a task
frame, because task frames are frozen and unreadable as live objects at that
point.

### What this rules out

- **No task-local trap handling.** A trap inside a task is not catchable by that
  task, and `async` introduces no per-task recovery. There is one handler.
- **No partial shutdown.** The executor is not drained, quiesced, or given a
  chance to finish in-flight work. D-062's join has a deadline and runs on the
  *normal* exit path; a trap is not a normal exit path and does not join.
- **No `async failsafe`.** `failsafe` may not be declared `async`; a handler that
  could suspend could be starved by the executor it is shutting down. Compile
  error, alongside D-014's existing three checkable requirements.

### Consequences

- `failsafe` gains a fourth checkable requirement: **it may not be `async`.**
- The trap path gains a stop-the-world step ahead of the handler call, ordered
  before any `failsafe` code runs.
- `--extra-picky`'s no-allocation-in-`failsafe` rule (D-014) extends naturally to
  **no `await` in `failsafe`**, which the `async` prohibition already implies.

---

## D-064 — Generics: definition-time checking, turbofish-only in expression position, bounded monomorphization — **SETTLED**

Settles the **generics** gap in `PRE_PLANNING_REVIEW.md` §4 — the last
frontend-blocking item. D-030 settled *declaration* syntax; everything below it
was open.

### What was already decided, and is unchanged

`struct:Container<T>`, `func:extract_value<T>`, bounds as `<T: Renderable &
Serializable>` with `&` combining (D-029, D-030). Parameters follow the name.
Monomorphization is the implementation strategy. Object safety and coherence are
`TRAITS_REFERENCE.md` §4.

### 1. Generic bodies are checked at their definition, not at each instantiation

**A generic body is type-checked once, treating each type parameter as an opaque
type that satisfies exactly its declared bounds and nothing else.** Instantiation
then checks only that the concrete arguments satisfy those bounds; it does not
re-check the body.

The prototype does the opposite, and the prototype's own bug log is the argument.
`tests/bugs/bug403_fixed_generic_primitive_reassign_fail.npk` records it in
detail:

> *"the binding-property `isFixed` flag was dropped in `cloneAST` when the
> monomorphiser cloned the generic body, and the specialised body was never
> type-checked, so this reassignment slipped through silently."*

The fix added `checkFixedReassignInSpecialized` — a re-check of the *clone*. That
treats the symptom. `fixed` is a binding property of the generic body and is
checkable without knowing `T` at all; under definition-time checking the error is
caught once, at the definition, and no property of the source can be lost in a
clone because no check depends on the clone.

Three reasons this is the right side of the tradeoff here specifically:

- **The single verification run.** Astrée analyses monomorphized output. Under
  instantiation-time checking a generic body with *N* instantiations is *N*
  separate things to establish, and a body can be correct for every instantiation
  that happens to exist while being wrong in general. Checked once against its
  bounds, it is established once.
- **Errors land on the code that is wrong.** Instantiation-time checking reports
  a fault in a generic body at the *caller*, who supplied a perfectly reasonable
  type. That is the C++ template diagnostic experience, and it is a poor fit for a
  language whose entire posture is that mistakes must be easy to see.
- **An unbound body is an unstated contract.** If a body may do anything the
  concrete type happens to support, the bound list stops describing the
  requirement, and a later instantiation with a different type fails at a distance
  from any change. Bounds become documentation rather than a checked interface.

The cost is real and worth stating plainly: **a generic body may not use any
capability its bounds do not declare.** Duck-typed templates are gone. That is
the constraint that makes the body checkable, and the bound syntax to express it
already exists.

### 2. Value parameters are declared `comptime <type>:<name>`

D-056 requires `Mutex<T, LEVEL>` with `LEVEL` a compile-time constant, and
`TRAITS_REFERENCE.md` §3 does not mention value parameters at all. They exist:

```nitpick
struct:Mutex<T, comptime int32:LEVEL> = { … };

Mutex<Config, 2>:cfg_lock;          // use site supplies the value
```

The declaration states the kind; the use site supplies type and value
positionally.

**Why `comptime` is required rather than bare `int32:LEVEL`.** Without a marker,
`<T: Renderable>` and `<int32:LEVEL>` put the newly introduced name on *opposite
sides of the same colon* — in the first the new name is on the left and the right
is a bound, in the second the new name is on the right and the left is a type. A
reader could not tell which identifier is being introduced without already
knowing whether `Renderable` is a trait or a type. That is exactly the kind of
context-dependent reading the blueprint philosophy exists to prevent.

`comptime` removes it, uses an existing keyword with its existing meaning, leaves
D-030's bound syntax untouched, and makes value parameters visibly heavier than
type parameters — which matches how much rarer they are.

### 3. In expression position, explicit type arguments are always `::<T>`

| Position | Form | Example |
|---|---|---|
| **Type** | bare brackets | `Handle<Node<int64>>:h;` · `struct:Container<T>` |
| **Expression** | turbofish, always | `extract_value::<int32>(c)` |
| **`#`-builtin** | bare brackets | `#size_of<int32>()` · `#wild_ptr<T>(addr)` (D-020) |

`TRAITS_REFERENCE.md` §3.2 previously made implicit `f<int32>(x)` the normal form
with the turbofish as *"the fallback where that is ambiguous"*, resolved by
unspecified *"lookahead"*. Both halves are struck.

- **"Fallback where ambiguous" is a context-dependent spelling.** The author has
  to know whether *this* call site happens to be ambiguous in order to know which
  of two forms to write. One spelling that always works is strictly better than
  two that divide a case space.
- **"Lookahead" is not a specification.** It names a technique, not a rule, and
  leaves the parser author to invent the boundary — precisely what
  `AST_REFERENCE.md` §8 exists to prevent.

**This costs almost nothing**, because type arguments are inferred at the
overwhelming majority of call sites and neither form is written: `extract_value(c)`
is the normal case. The turbofish is for the residue where inference genuinely
cannot decide.

**Why `#`-builtins keep bare brackets.** The `#` sigil is itself the
disambiguator and it is visible in the token — `#size_of` cannot be a variable, so
`<` after it is unambiguously a type-argument list. This is the same principle
D-046 settled for `!`: a reader can tell which meaning applies from the token
alone, without knowing what surrounds it. It is not an exception to the rule so
much as the rule already being satisfied by the sigil.

### 4. `>>` splitting is now confined to type-argument context

`LEXICAL_REFERENCE.md` §5.2 requires the lexer to split `>>` into two `>` tokens
"when the parser is in a type-argument context." §3 above makes that context
precisely delimited: a type-argument list is opened by a type position or by
`::<`, and nowhere else.

Consequently `>>` is a right-shift **everywhere outside** a type-argument list,
with no lookahead and no speculative parse. `Handle<Node<int64>>` splits;
`x >> 2i32` does not; and there is no third case.

### 5. `<T>` and `type:T` are different mechanisms and do not overlap

The prototype has both, and `tests/bugs/bug166_comptime_generic_noncomptime_fail.npk`
enforces the boundary — a `type:T` parameter in a non-`comptime` function is a
compile error (COMPTIME-010).

| | `<T>` | `type:T` |
|---|---|---|
| Kind | runtime generic | parameter to a `comptime` function |
| Emits | one specialization per instantiation | **no code at all** |
| Legal in | any function | `comptime` functions only |
| Used for | generic data structures and algorithms | introspection — `#type_name(T)`, `#has_field(T, "f")` |

These are not two ways to do one job. A `comptime func` is *evaluated* during
compilation and produces a value; it never produces a specialized runtime
function, so neither form can be substituted for the other. Both are kept, and
the COMPTIME-010 restriction is kept with them. `TRAITS_REFERENCE.md` §4.2's
third object-safety rule — no method with comptime type parameters — refers to
this form.

### 6. Monomorphization is bounded, deduplicated, and reversibly mangled

**Depth.** Instantiation depth is capped at **64**, as the prototype already does
(`generic_resolver.cpp:399`, `MAX_INSTANTIATION_DEPTH`). Exceeding it is a
compile **error** with the instantiation stack printed, never a silent
truncation. Recursive generic instantiation is otherwise non-terminating, and an
unbounded expansion in the frontend is the same hazard D-057 bounded for macros —
same problem, same answer.

**Deduplication.** Instantiations are keyed by mangled name; a repeat request
returns the existing specialization. The prototype already does this for structs
via a `getStructType(mangledName)` lookup.

**Cross-module.** A generic's body is part of what its module exports, since a
using module must be able to instantiate it. Instantiation happens in the using
module and identical specializations are folded at link time.

**Mangling is readable and reversible.** The prototype emits
`_Nitpick_M_<name>_<hash>_<types>` — both a hash and the canonicalized type
names. **The hash is dropped.** A mangled symbol must map back to its source
declaration by inspection, because an auditor reading a verification report or a
disassembly has to do exactly that, and a hash makes it a lookup against a table
that may not have survived. Symbol length is not a safety property; reversibility
is.

### 7. What does not exist

- **No specialization.** There is no way to give one instantiation a different
  body from another. Specialization makes the code that runs depend on the type
  in a way the definition does not show, which is the same objection that removed
  `(!)` in D-061 — the reader cannot see from the generic what a given
  instantiation will do.
- **No variance and no subtyping.** Nitpick has no inheritance, so
  `Container<Derived>` and `Container<Base>` never arise as a question.
  `dyn Trait` is the only type-erasing construct and is governed by object safety.
- **No implicit conversion through a type parameter.**
  `tests/adversnitpickl/type_system/generic_type_mismatch.npk` already pins this:
  passing a `tbb8` where `identity<int8>` expects an `int8` is an error. A type
  parameter binds to exactly one type.
- **No duck-typed bodies.** Per §1 — a capability not in the bounds is not
  available in the body.

### Consequences

- `TRAITS_REFERENCE.md` §3.2 is rewritten; the implicit-call-with-lookahead form
  is struck.
- `LEXICAL_REFERENCE.md` §5.2 gains the precise delimitation of type-argument
  context.
- `generic_resolver.cpp`'s `checkConstraints` / `validateConstraints` port across
  as the *instantiation-side* check. The definition-side check is new work with
  no prototype counterpart.
- `checkFixedReassignInSpecialized` is **not** ported — the bug it patches cannot
  occur once bodies are checked at their definition.
- `comptime` is added to the generic-parameter grammar.

---

## D-065 — `move` is an operator, not a memory qualifier — **SETTLED; the qualifier half is narrowed by D-183** (`move T:p` is a CONSUMING PARAMETER, and the operator form below is untouched)

Settles the `LEXICAL_REFERENCE.md` open item. The production

```ebnf
MemoryQualifier ::= "wild" | "wildx" | "stack" | "defer" | "move"
```

is **wrong about `move`**, which is not a qualifier and never was. The prototype
spells it as a parenthesized operator on a place:

```nitpick
wild int8->:buffer = malloc(100i64);
wild int8->:moved  = move(buffer);

free(buffer);   // NITPICK-019 — use after move
```

`move` is removed from `MemoryQualifier`, leaving four qualifiers — `wild`,
`wildx`, `stack`, `defer` — which is what the memory model actually has.

### Spelling: `move(place)`, unchanged

`move` does not return `Result<T>` and its operand is a **place**, not a value, so
it is not a function and could not be one. The obvious inference is that it should
therefore take the `#` sigil under D-020, as `#size_of<T>()` and `#wild_ptr<T>()`
do.

**It should not**, because Nitpick already has this exact shape: `comptime(expr)`
is a keyword operator with a parenthesized operand that returns no `Result` and is
not a call. `move(place)` is the same construct with a different keyword, and both
are keywords rather than identifiers, so no parse ambiguity exists. Introducing a
sigil here would add a second spelling convention for a form the language already
has one for.

### Ownership transfer is explicit only

**Ownership moves only where `move` is written.** Passing an owning value to a
function borrows it, per D-004 — borrows pass down the call stack and never up —
and returns it on return.

An implicit move would change ownership with nothing visible at the point it
happens, which is unreadable at review time and ungreppable at audit time. The
same reasoning already governs `raw`, `wild`, and `=>!`: the bypass is explicit or
it does not exist.

### The moved-from binding

- It is **invalid**, not "valid but unspecified". Any read is `NITPICK-019`. This
  is the `UNDEFINED_STATE_PREVENTION` discipline — there is no such thing as a
  value you may hold but not inspect.
- Freeing it is separately rejected — the prototype's negative test requires both
  *"Use after move"* and *"Cannot free moved variable"*, which is what makes
  `move` the mechanism that prevents double-free on `wild` allocations.
- **It may be reinitialized by assignment**, after which it is live again. This is
  ordinary definite-assignment analysis, which the frontend already needs and the
  prototype already has. A `fixed` binding cannot be, since it cannot be assigned
  at all.

### One meaning for every type

`move` means the same thing on every type: the source binding is invalidated and
ownership transfers. Applying it to a primitive is legal and merely pointless.

The tempting alternative is to reject `move` on trivially-copyable types, since
there it can only destroy information. **Rejected** — it would make the operator's
legality vary by the type it is applied to, and under D-064 a generic body is
checked against its bounds alone, so `move(x)` inside `func:consume<T>` would be
un-checkable without inventing an ownership bound to carry the distinction. A
uniform operator costs one avoidable footgun, which use-after-move catches
immediately; a type-varying one costs a new trait and a context-dependent reading.

---

## D-066 — `opaque struct:Name;` is the one form, and is `extern`-only — **SETTLED; the C-pointer half is superseded by D-149** (there is no in-process C to point at — an `extern` block is a driver interface, and an opaque struct declared in one is a TYPED WIRE HANDLE the Bridge round-trips by value, never an address)

Resolves conflict **49**, which recorded two spellings for one concept:
`FORMAL_DRAFT` 02 §2.7.1's `StructType ::= "struct" | "opaque" "struct"` against
`TRAITS_REFERENCE.md` §2.7's standalone `opaque:DatabaseHandle;`.

**`opaque struct:Name;` wins.** The standalone form has **zero** occurrences in
the prototype; the modifier form has real usage, its own test directory, a
negative test, and a K-semantics `loadStructs` rule.

```nitpick
extern "libc" {
    opaque struct:OpHandle;
    func:opaque_make = OpHandle();
    func:opaque_use  = int32(OpHandle:h);
}
```

It also reads correctly: `opaque` modifies `struct`, saying *what the thing is* and
*what you do not know about it*, where the standalone form says only the latter.

### Legal only inside an `extern` block

An opaque type exists because a foreign library owns a layout Nitpick cannot see.
That is its entire purpose, and outside `extern` there is nothing it does that
module visibility does not already do — so permitting it there would create a
second encapsulation mechanism competing with `pub`.

### No value semantics

Opaque values **cannot be copied**. The prototype enforces this as
`OPAQUE-COPY-001`:

```nitpick
Handle:h  = handle_create();
Handle:h2 = h;              // rejected — OPAQUE-COPY-001
```

The reason is that a copy would have to know the size, which is precisely what an
opaque type withholds. Initialization from a call is fine, passing borrows per
D-004, and transferring ownership is `move(h)` per D-065 — for which an opaque
handle is the canonical case.

### Consequence

`AST_REFERENCE.md`'s `OpaqueDecl` is corrected: the form is `opaque struct:Name;`,
it carries no fields, and it is valid only as an item inside `ExternBlock`.

---

## D-067 — LLVM and Z3 are invoked, never linked — **SETTLED**

Resolves Part X. `FORMAL_DRAFT` 00b §0 describes the native compiler as enforcing
zero C/C++ dependencies *"with the only explicit and isolated exceptions being the
LLVM IR generator and the Z3 SMT Subsystem."*

**There is no exception, because neither is a dependency in the sense the rule
means.**

| | Linked (`libLLVM`, `libz3`) | **Invoked (adopted)** |
|---|---|---|
| C++ in the compiler binary | yes | **no** |
| Interface | C++ API | **text** — LLVM IR, SMT-LIB2 |
| A crash in the tool | takes the compiler with it | **a failed subprocess — a value** |
| In a compiled Nitpick program | never | never |

The compiler emits **text**: hand-written LLVM IR (D-011, D-015) and SMT-LIB2 for
the solver. `llc`, `opt`, and `z3` are subprocesses. `--debug-z3` already dumps
SMT-LIB2 in the prototype, so the text interface is not hypothetical.

### This is D-055's argument applied to the toolchain

D-055 put GPU and GUI work behind a process boundary so a vendor runtime's failure
reaches the Nitpick program as a closed socket or a dead child — an ordinary
`Result` error rather than an uninterceptable fault. The same reasoning applies
here: a segfault inside `libLLVM` linked into the compiler is a compiler crash
with no diagnostic, while a segfault in an `llc` subprocess is a nonzero exit
status the driver reports and recovers from.

The zero-dependency rule exists because **past the FFI barrier a fault cannot be
intercepted**. A subprocess boundary is not an FFI barrier. Nothing crosses it but
bytes.

### What this does not claim

Stated plainly, because an auditor will ask and a hedge would be worse than the
admission:

**LLVM's translation from IR to machine code is outside the verified boundary.**
Verification establishes properties of the Nitpick source and of the IR the
compiler emits. It does not establish that `llc` translated that IR faithfully.

This is inherent rather than a Nitpick shortcoming — every verified toolchain has
a trusted backend unless it is a CompCert-style verified compiler, and writing our
own code generator for every target is not a serious proposition against the
alternative of an enormously exercised one. The residual is bounded by keeping the
IR the artifact of record, by differential testing against the prototype as a
behavioral oracle, and by translation validation should it later be wanted. It is
recorded here rather than left for someone to discover in the middle of the
Astrée trial.

### Consequence

`nitpick-native` links **nothing**. The IR emitter and the SMT-LIB2 emitter are
Nitpick source in this repository, and the toolchain requirement is the presence
of `llc`, `opt`, and `z3` on `PATH` — the same status `ld.lld` already has.

---

## D-068 — `limit<Rules>` is always enforced; `--verify` decides only whether the check survives — **SETTLED**

Resolves N5, which recorded that `FORMAL_DRAFT` 12.6.1 says constraints are
*"enforced dynamically at runtime"* without saying what a violation does.

**A runtime constraint violation traps to `failsafe`**, like every other
unrecoverable condition. `VERIFICATION_REFERENCE.md` §2 already says so; the
`FORMAL_DRAFT` sentence is the incomplete one.

### The larger question N5 exposed

`VERIFICATION_REFERENCE.md` §2 reads *"When you compile with `--verify`, the
compiler's integrated Z3 solver will prove … If it cannot prove it statically … it
will enforce the check at runtime."* Read literally, a build **without**
`--verify` has neither the proof nor the check, and `limit<Rules>` constrains
nothing at all.

**That reading is rejected.** `limit<Rules>` is enforced in every build.
`--verify` decides only whether a given check is **discharged statically and
therefore elided**, never whether it exists.

Three reasons, in order of weight:

1. **A safety property must not depend on a compiler flag.** A constraint that
   silently evaporates in a build someone forgot to pass a flag to is a default
   that varies by circumstance, which the blueprint philosophy treats as a defect
   in the design rather than a configuration choice.
2. **It inverts the risk.** Verification is most likely to be run in development
   and least likely in a hurried rebuild. The literal reading makes the shipped
   binary the *weakest* one — checks present exactly where they are least needed
   and absent where they are most.
3. **It makes `limit` unreadable at the point of use.** A reader of
   `limit<r_positive> int32:x` could not tell whether anything is enforced without
   knowing how the build was invoked.

### Verification therefore pays for itself

The useful consequence: **proving a constraint removes its runtime check.**
`--verify` is not only a safety flag but the mechanism by which constrained code
reaches the speed of unconstrained code, and an unproven constraint has a visible,
attributable cost rather than a hidden one.

That is the correct ordering of the project's priorities — performance is a
first-class requirement, obtained here by proving more rather than by checking
less.

> **[1.5.2, 2026-09-04; D-251, D-252]** As built: the check runs in every
> build after every write; the verified build elides a discharged write
> point into one `llvm.assume` over the rule's range clauses (the channel
> 1.6's analyzers read), and a discharged `limit-subsume` row lets a direct
> call skip the callee's entry check by naming its body — so "constrained
> code reaches the speed of unconstrained code" holds for a limited
> PARAMETER too, the common placement.

---

## D-069 — `Result` stores the error once; `is_error` becomes derived — **SETTLED**

Settles the D-005 follow-up: *"how `tbb32`'s sticky ERR state is encoded within
the 4-byte error field. A bare `i32` either loses the ERR encoding or implies an
undocumented reserved bit pattern."*

### There is no undocumented bit pattern

The premise was wrong in a useful way. `tbb32`'s ERR sentinel **is** a documented
bit pattern — `INT32_MIN`, per `TYPE_REFERENCE.md` §6 — so storing the field as
`i32` loses nothing. The real question is what that value *means* when it appears
in `Result.err`, which nothing answered.

It means **an error whose identity was lost**: the code itself went ERR, most
often because it was computed from `tbb` arithmetic that saturated.

### An error whose code is ERR is unconstructible

`fail(e)` where `e` is ERR **traps to `failsafe` at the point of construction.**

The `Result` discipline exists so that a caller is *forced to handle* the error.
A caller cannot handle an error it cannot identify, so such a value defeats the
mechanism it is travelling through — and it would do so silently, satisfying every
`is_error` check while carrying no information.

Trapping is also where the existing rules already lead. Branching or comparing on
a `tbb` ERR value traps (`SAFETY_ARCHITECTURE.md`), so any attempt to *handle* an
ERR-coded error traps anyway. Constructing-time trapping simply moves it to the
point where the context that produced it still exists — the same argument D-014
makes for entering `failsafe` without unwinding first.

The error field's value space is therefore exact and total:

| Value | Meaning |
|---|---|
| `0` | no error — this is what `NIL` denotes in this field |
| `1 … 2^31-1` | user error codes |
| `-(2^31-1) … -1` | system error codes |
| `INT32_MIN` | ERR — **unconstructible**; traps where it would be built |

### The larger defect: the error state is stored twice

Examining the layout to answer the above surfaced a worse problem.

```nitpick
struct<T>:Result = {
    T:value;
    tbb32:error;     // NIL if no error
    bool:is_error;   // false = success, true = error
};
```

`is_error` and `error != NIL` encode **the same fact in two places, with no stated
invariant relating them.** Nothing says what `{err: 0, is_error: true}` or
`{err: 5, is_error: false}` mean, nothing rejects them, and both are
constructible today through the explicit-literal form the spec documents:
`return Result{err: errCode, value: retVal, is_error: true};`.

Two representations of one fact that can disagree is precisely the latent
inconsistency this project cannot carry. It is also the shape D-056 rejected in
the old `mutex` API and D-062 rejected in preemptive cancellation — an invariant
maintained by convention rather than by construction.

**The stored `is_error` field is removed.**

```nitpick
struct<T>:Result = {
    T:value;
    tbb32:error;     // 0 = success; sign selects user/system; ERR unconstructible
};
```

`r.is_error` **remains valid source** as a derived accessor for `r.err != 0i32`,
so every existing `pick(r.is_error)` and every library using it continues to read
and behave identically. What changes is that the fact is computed rather than
stored, and so cannot contradict the field it summarises.

### Consequences

- **Layout.** `Result<int32>` goes from `{i32, i32, i8}` + 3 bytes padding = **12
  bytes** to `{i32, i32}` = **8 bytes**, align 4. Wider payloads mostly break even
  since the removed byte lived in padding. The saving is real but incidental — the
  reason is the invariant, not the size.
- **No performance cost.** Testing `i32 != 0` and testing an `i8` are the same
  single instruction.
- **`pass` / `fail` desugaring** loses its third initialiser:
  `pass(v)` → `Result{value: v, err: 0i32}`; `fail(e)` → `Result{value: zero,
  err: e}` with `e` checked non-zero and non-ERR.
- **`fail(0i32)` is rejected** at compile time where the code is a literal, and
  traps where it is computed. A failure with no code is the same unidentifiable
  error as an ERR code, arrived at from the other direction.
- **Returning a value *and* an error remains expressible** —
  `Result{value: retVal, err: errCode}` — since that never depended on the
  separate flag.
- **ABI.** This changes the `Result` layout, which is the most pervasive type in
  the language. It is free to change now and expensive later, and nothing external
  links against it (D-067).

### Correction to `TYPE_REFERENCE.md` §27

The `Result<NIL>` IR is given as `{ ptr undef, ptr null, i8 0 }`, which types the
error field as a **pointer**. The canonical layout four sections earlier gives
`{ T value, i32 error, i8 is_error }`. The `ptr null` form is wrong; under this
decision `Result<NIL>` is `{ i8 undef, i32 0 }` — a `NIL` payload and a zero error.

---

## D-070 — `T[]` is a slice: bounds live in the array type, not the pointer type — **SETTLED**

Closing Part W required checking what carries bounds if pointers do not. Part W
itself was already answered — **D-038 settled that pointers are thin**, struck
`FORMAL_DRAFT` 15 §15.1.3 by name, and gave the reasoning. Its heading still read
"Open question", which is how it reached the queue; conflict 52's note that this
is *"a real open question, not stale text"* is likewise out of date.

**But the fat/thin question was asked about the wrong type**, and the type it
should have been asked about is unspecified.

### The gap

`TYPE_REFERENCE.md` §9.2 specifies **fixed** arrays only — `T[N]`, a value type,
copied on pass, bounds known statically and checked at every index with a branch
to `failsafe_oob`. That is complete and correct.

`T[]` — the **unsized** form — has no layout, no bounds story, and no statement of
whether it owns or views. It is used throughout the language and the libraries:

| Use | Where |
|---|---|
| `char8[]` character arrays | `TYPE_REFERENCE` §2.3 |
| `..*T[]` variadic parameter | `AST_REFERENCE` `VariadicSpec` (D-047) |
| `cstring[]:args` | every `exec` signature in `nlibc` (D-048) |
| `toCharArray` / `fromCharArray` | `TYPE_REFERENCE` §3 string table |

D-038's argument depends on this type. It says the hazards fat pointers address
are closed statically and at zero cost — which is true, but only because an array
knows its own length. With `T[]` unspecified, nothing said that it did.

### The decision

**`T[]` is a slice — `{ptr, i64 len}`, 16 bytes, align 8 — and it is a
non-owning view.**

```llvm
%slice = type { ptr, i64 }    ; data, element count
```

- **Indexing is bounds-checked against the runtime `len`**, trapping to
  `failsafe` exactly as a fixed array's static check does. This is where
  out-of-bounds detection actually comes from, and it is why pointers do not need
  to carry it.
- **`.len` is available** on every slice.
- **A slice is a second-class borrow (D-004).** It passes down the call stack and
  never up, cannot outlive the storage it views, and cannot cross a thread spawn
  or an `await`. That is what makes a non-owning view safe without a generation
  counter, and it is the same rule every other borrow follows.

### Why the bounds belong here rather than on the pointer

This is the distinction Part W missed. A pointer is *one address*; there is no
correct length to attach to it, which is why fat pointers have to invent one and
carry it everywhere. An **array** is an address *and* an extent by definition —
the length is not metadata bolted onto it, it is half of what the type means.

So bounds are carried exactly once, by the type that actually has them, and every
pointer stays one word. `string` (`{ptr, len, cap}`) and `cstring` (`{ptr, len}`)
already work this way; slices make the rule general rather than special-cased per
type.

### Constructing a slice

| From | Form |
|---|---|
| fixed array or another slice | ranging — `arr[0...n]`, with `..` and `...` keeping their existing inclusive/exclusive meanings |
| a raw pointer and a length | **`#wild_slice<T>(ptr, len)`, `wild` context only** |

The second is deliberately parallel to D-019's `#wild_ptr<T>(addr)`: constructing
an extent the compiler cannot verify is exactly as privileged as constructing an
address it cannot verify, and it should be as greppable. It is how a slice is
obtained across the FFI boundary.

### Owning sequences are a library concern

`T[]` never owns. A growable, owning sequence is a library type, which is what
D-041 already decided when it returned the 35 `a*` collection keywords to
userland — collections belong in a library.

### Consequence: an API documented as returning a copy cannot return `T[]`

`TYPE_REFERENCE.md` §3 lists `toCharArray : (string) → char8[]` as *"Convert to
character array (copy)"*. **That is now ill-formed**: the copy needs an owner, and
a `T[]` view cannot be one, so the returned slice would view a buffer nobody owns.

It should take a destination instead —
`toCharArray(string, char8[]:dest) → Result<int64>`, returning elements written —
which is the same shape the `nlibc` collapse settled on for every other
buffer-filling call, and which lets the caller own the storage. `fromCharArray`
is unaffected: it consumes a view and produces an owning `string`.

### What does not change

- Pointers remain thin (D-038). Nothing here reopens it.
- Fixed arrays remain value types with static bounds (§9.2).
- `--verify-memory` keeps proving bounds **statically**; the runtime check is what
  remains where a proof is not available, which is the same two-layer arrangement
  as D-068's `limit<Rules>`.
- The C ABI is unaffected for pointers. A slice is not C-compatible and does not
  cross an `extern` boundary — `extern` signatures take a pointer and a length as
  separate parameters, as C does.

---

## D-071 — Every thread runs an executor; blocking is always task suspension — **SETTLED**

Specifying channels surfaced an interaction between D-032, D-034, and D-058 that
nothing had stated, and that has a safety consequence.

### The hazard

- **Tasks are pinned to threads** and never migrate (D-032).
- **Each thread's executor owns the arena its task frames come from** (D-034).
- **Fan-out and collect goes through `channel`** — D-058 says so explicitly, since
  removing user-visible `Future<T>` removed spawn-now-await-later.

So tasks use channels. But the prototype's channel blocks the *operating system
thread*, on a mutex and a condition variable. A task that receives from an empty
channel therefore parks the thread it is pinned to — **and with it every sibling
task on that executor**, none of which had anything to do with the channel.

On a system with actuators live, one task waiting on a sensor queue silently
stalling the control task that shares its executor is exactly the class of event
the safety architecture exists to prevent. Nothing in the current specs forbids
it, and nothing would have caught it.

### Two bad ways out, and the one taken

| | Approach | Why not |
|---|---|---|
| A | Channels are thread-only; tasks may not use them | contradicts D-058, and leaves tasks with no way to communicate at all |
| B | Two APIs — a blocking one and an `async` one | two mechanisms for one job, and the choice varies by caller context, which is the blueprint violation D-064 §3 rejected for the turbofish |
| **C** | **Every thread runs an executor. All blocking is suspension.** | adopted |

**Every thread has an executor** — D-034 already gives it one — **and every
blocking operation suspends the calling task rather than parking the thread.**
When the executor has another runnable task it runs it; when it has none it parks
the thread on a futex, which is exactly what the blocking implementation would
have done anyway.

### What this buys

- **One channel API.** `await ch.recv(deadline)` is written the same way
  everywhere, because there is no context in which it means something different.
- **The stall is structurally impossible.** A waiting task cannot prevent a
  sibling from running, because waiting is a suspension point rather than a
  syscall.
- **No cost when there is nothing else to run.** A thread doing bulk numeric work
  that never spawns a task and never awaits pays for an arena it does not
  allocate from. The executor is a few words of thread-local state.
- **D-034 becomes universal** rather than a rule that applies only to threads that
  happen to be running `async` code.

### Consequences

- **A thread's entry point is an `async func`.** `await` is legal in it, which
  `CONCURRENCY_REFERENCE.md` §2.1's rule already implies — the restriction is that
  `await` requires an `async func`, and a thread body now is one.
- **`main` is `async`** in a program that uses concurrency, which the existing
  examples already show (`async func:main`).
- **There is no `block_on`.** It would be an adapter between two disciplines, and
  after this decision there is only one.
- **`failsafe` remains non-`async`** (D-063). It runs on the trapping thread as a
  plain call, with every executor already stopped, so there is nothing for it to
  suspend into.
- The §1 table's split stands, but gains a connecting rule: **threads supply
  parallelism, tasks supply concurrency, and waiting is always a task-level
  event.**

---

## D-072 — `Channel<T, LEVEL, CAP>`: typed, capacity in the type, deadline required, no `select` — **SETTLED**

Specifies the channel surface `CONCURRENCY_REFERENCE.md` §6 recorded as missing.
Evidence for each rule is in `meta/CONCURRENCY_STDLIB_AUDIT.md`.

### Shape

```nitpick
Channel<T, LEVEL, CAP>          // element type, lock level, capacity
```

- **`T` is the element type.** The prototype's `send(int64, int64)` /
  `recv → int64` erases it, which is the defect pattern this project has now found
  five times (audit §3).
- **`LEVEL` is the D-056 lock level.** A channel blocks, so it is a blocking
  primitive and carries a level like every other one.
- **`CAP` is a `comptime int64`.** Capacity belongs in the type, not in a runtime
  `mode` field dispatched by `if` chains at every operation. `CAP == 0` is a
  rendezvous channel and `CAP > 0` is buffered; each instantiation monomorphizes
  to one behaviour with no branch (D-064).

**The `oneshot` mode is not carried across.** It is a capacity-1 channel that the
sender closes, and a mode flag that selects a behaviour expressible in the two
that already exist is a third mechanism for no gain.

### Operations

```nitpick
await ch.send(move(v), deadline)   -> Result<NIL>
await ch.recv(deadline)            -> Result<T>
ch.close()                         -> Result<NIL>
```

- **`recv` returns `Result<T>`.** The prototype returns `0i64` for a bad handle,
  for a closed channel, and for a received value of zero — three meanings on one
  encoding (audit §3). A closed channel is an **error code**, never a value.
- **Deadlines are mandatory** (D-056). There is no unbounded `recv`.
- **`send` takes ownership**, so the value is written `move(v)` (D-065). Ownership
  transfer across a channel must be visible at the call site.
- **`T` may not contain a borrow.** Borrows are second-class and may not cross a
  thread spawn or an `await` (D-004), which makes `T[]` — a slice, and therefore a
  borrow (D-070) — unsendable. Send an owning type instead.
- **`try_send` / `try_recv` are removed.** They existed to avoid blocking, which
  a deadline already does, and the unbuffered `try_send` shipped a defect where
  the value was delivered *and* failure was reported (audit §1.3). A zero deadline
  expresses "do not wait" exactly, in the same call, with the same result type.

### There is no `select`

`select2` busy-waits forever on any non-zero timeout, reports a closed channel as
ready, and races its own follow-up `recv`; `select3` and `select4` are stubs that
always return "nothing ready" (audit §2, §4). None of it is salvageable, and it
should not be replaced.

**A correct `select` is incompatible with D-056.** Waiting on N channels means
holding, or at least acquiring, N channel locks — and D-056 requires that lock
acquisition strictly *increase* in level, which two channels at the same level
cannot do. Making a general `select` sound would mean either giving every channel
a distinct level, which does not compose, or exempting `select` from the level
discipline, which is the one mechanism that makes lock-order freedom provable.

**The common use of `select` is already covered.** Its usual job is "receive work,
but also notice shutdown", and a mandatory deadline provides that directly: a
`recv` that returns `DEADLINE_EXCEEDED` is the loop's opportunity to check for
shutdown. Genuine fan-in is done the way it is done in practice anyway — several
producers sending to **one** channel — which needs one lock, not N.

### Endpoints and lifetime

A channel's storage belongs to the scope that created it, and endpoints are
~~second-class borrows of it~~ **— amended by D-182: an endpoint is an opaque
generation-checked HANDLE, not a borrow. As borrows they could not cross a
spawn (D-004/D-180), which made channels unusable for the thing channels are
for; as handles they may travel freely while the scope still owns the
channel's life.** They cannot outlive that scope, which is the same rule
tasks (D-062) and borrows (D-004) already follow.

This is what closes the teardown race in audit §5, where `destroy` freed the mutex
and condition variables out from under parked waiters. With lexical ownership,
scope exit cannot run while a task holding an endpoint is still live, because
D-062 already joins those tasks first.

It also means **no reference counting on endpoints** and no `destroy` in the
surface at all.

---

## D-073 — Actors and thread pools are built from channels; `lockfree` is not ported — **SETTLED**

### One queue primitive

`thread_pool` hand-rolls a second queue — a raw mmap'd ring with independent
`pending`, `head`, and `tail` fields — and that is where its worst defect lives:
`head` advances at dequeue while `pending` is decremented only after the task
runs, so a second worker reads a still-positive count, takes an unwritten slot,
and calls whatever it contains as a function (audit §1.1).

**A thread pool is N worker tasks receiving from one `Channel<Job, LEVEL, CAP>`.**
One count, one lock, one implementation to verify. The race is not fixed; it
becomes unexpressible, because there is no second counter to disagree with the
first.

The same applies to the actor mailbox, which the prototype already built on a
channel and should continue to.

### Thread pools

```nitpick
ThreadPool<LEVEL, CAP>:pool = ThreadPool.create(worker_count)?;
await pool.submit(move(job), deadline)?;
```

- **Submitted work is lexically scoped** (D-062): the pool's owning scope does not
  exit until every submitted job has finished, under a deadline, and expiry traps.
  This replaces a `shutdown` that wrote its stop flag to the wrong field, never
  joined because the thread ids were discarded at spawn, and returned success
  regardless (audit §1.2).
- **The job type is checked.** `submit(int64:pool, ?->:func, int64:arg)` erased it
  and silently zeroed every closure's captured environment (audit §6).
- **`wait_idle` is removed** — it busy-spun with no deadline, and lexical scoping
  makes "wait for the work to finish" the behaviour of scope exit rather than a
  call.

### Actors

```nitpick
Actor<M, R, LEVEL>              // message type, reply type, lock level
await actor.tell(move(m), deadline)   -> Result<NIL>
await actor.ask(move(m), deadline)    -> Result<R>
```

An actor is a **task** with a mailbox, not a thread with a mailbox. The prototype
spawns an OS thread per actor and inherits every defect in `thread.npk`; under
D-071 a task suspends rather than parking a thread, so the thread was never
buying anything.

**`ask` replaces the reply-channel machinery**, all three functions of which are
stubs returning failure or zero (audit §2). The obvious alternative — put a reply
channel *in* the message — cannot work here: an endpoint is a second-class borrow
(D-072) and borrows may not cross a thread spawn (D-004). `ask` keeps the reply
channel in the caller's scope, where it is legal, and the runtime routes the reply
to it.

`R = NIL` for an actor that does not reply. `ask` remains useful there as an
acknowledgement, which is how backpressure is expressed.

`alive` becomes `atomic<bool>` — the prototype writes a plain `int32` from the
stopping thread and reads it in the actor loop with no synchronization at all.

### What is not ported

| Module | Disposition |
|---|---|
| `lockfree.npk` | **not ported.** A lock-free MPMC queue under SeqCst-only atomics (D-016) is both hard to get right and expensive to verify, and a channel already provides the operation. Adding a second queue with a harder proof obligation, for a case nothing has been shown to need, spends verification budget against the one Astrée run. |
| `barrier.npk` | reimplemented natively — 34 lines wrapping three C shims, and the operation is a count, a mutex, and a condition variable |
| `atomic.npk` | not ported (already settled) — superseded by the language-level `atomic<T>` |
| `Thread.detach` | **removed.** It returns success and leaks a 2 MiB stack and a context page, and lexical lifetime means nothing is detached. |
| `Thread.sleep_ns/ms` | ~~reimplemented~~ — **STRUCK by D-181**: D-071 postdates this row, and under "all blocking is task suspension" a thread-blocking sleep is a second waiting mechanism that stalls every sibling task on its executor. `await sleep(within)` is the one way to wait. |
| `Thread.hardware_concurrency` | reimplemented — hardcoded to `4` |
| `CondVar.wait` | **removed**; `timedwait` is the only form, per D-056 |

---

## D-074 — Five reserved words return to userland; `binary` folds into `uint8[]` — **SETTLED**

Specifying I/O began by looking for what `stream` means. It means nothing:
`stream` is in `LEXICAL_REFERENCE.md`'s `BuiltinType` production and **has no
definition anywhere in the spec set.** Neither do four of its neighbours.

| Reserved word | Definition |
|---|---|
| `stream` | none — `TYPE_REFERENCE.md` skips from §23 to §25, which is where it presumably went |
| `process` | none |
| `pipe` | none — the only `pipe` in `TYPE_REFERENCE` is the `\|>` operator |
| `debug` | none |
| `log` | none |

**All five are removed from `BuiltinType`.** A reserved word that names nothing
costs userland an identifier and gives a reader a keyword they cannot look up.
D-041 set the precedent exactly: it removed 35 `a*` collection keywords on the
grounds that collections belong in a library, and returned them to userland.

Streams, processes, pipes, and logging belong in a library for the same reason.
The I/O model is `IO_REFERENCE.md`; none of it needs language syntax.

### `binary` is `uint8[]`

`TYPE_REFERENCE.md` §22 defines `binary` as `{ ptr, i64 length }` — *"immutable
blob … like `string` but without encoding semantics"*, with `binary_slice` a
zero-copy sub-range.

**D-070 defines a slice as `{ ptr, i64 len }`.** Identical layout, identical
non-owning behaviour, identical sub-ranging.

The remaining difference is immutability, and that is not a difference either:
immutability in Nitpick is **a binding property, not a type property** — the
prototype's own `bug403` comment states the rule — so an immutable byte view is
`fixed uint8[]`, which the language already provides.

`binary` is therefore redundant twice over and is **removed**, along with its
seven `binary_*` operations, which are slice operations under other names.
`buffer` (`{ptr, len, cap}`) is retained: a slice cannot own, and the owning byte
container is what a read fills and a write drains.

### Consequence

`BuiltinType` loses six entries. `TYPE_REFERENCE.md` §22 is deleted and §24's
absence is explained rather than filled.

---

## D-075 — I/O is a `Stream` trait, asynchronous, with end-of-input as an error code — **SETTLED**

The I/O model, specified in full in `IO_REFERENCE.md`. This records the decisions
behind it.

### `Stream` is a trait, not a type

Concrete streams — files, pipes, sockets, memory — are stdlib types implementing
it. The decisive reason is specific to this project: **the compiler's diagnostics
must be capturable.** Writing them through `dyn Stream` means production writes to
stderr and the test harness writes to a memory buffer, with one code path. A
concrete stream type would need a second mechanism for that, or an internal tag
selecting behaviour at runtime — which is the mode-field pattern D-072 rejected
for channels.

Object safety holds: every method takes `self`, none returns `Self`, none has
comptime type parameters (`TRAITS_REFERENCE.md` §4.2).

### Every operation is `async`

Required by D-071. A read that parks the OS thread stalls every sibling task on
that executor, and a compiler driver reading source files while diagnostics stream
out is exactly that shape.

The executor's readiness mechanism is `io_uring` or `epoll` **through raw
syscalls**, which is not a dependency question — `nlibc` is the syscall surface
and neither is a library.

### End-of-input is an error code, never a value

`libn`'s buffered layer returns `FILE_EOF = -1` for **both** end-of-file and
error, requiring `feof`/`ferror` to disambiguate — the C design, inherited
wholesale.

That is the same defect this project has now removed three times: `Result` encoded
its error state twice (D-069), a channel `recv` returned `0` for a closed channel
and for a received zero alike (D-072), and now one sentinel means EOF, error, and
in `fgetc`'s case a legitimate byte value would if the type were narrower.

**One rule, stated once:** a stream operation returns `Result<T>`; end-of-input is
an error code; no operation returns a sentinel. It is the same rule as "a closed
channel is an error code, never a value", and it exists so that a caller is forced
to handle a condition it cannot otherwise distinguish.

### `fd` is an `fd`

`FILE.fd` is an `int64` with `-1` meaning not-open. D-042 already settled this:
kernel identifiers are distinct types, and **POSIX's `-1` goes to `Result.err`
and is not representable** in an `fd`. The unopened state is not a value of the
field; it is the absence of the stream.

### Text and byte streams are different types

D-050 puts line-ending policy on the stream. Two types rather than a mode flag:
a text stream normalizes `\r\n`, `\n`, and lone `\r` to `\n` on read and emits
`\n` on write unless opened otherwise; a byte stream never translates anything.

The line-ending choice is a **creation parameter held in the writer**, not a
comptime parameter. A comptime parameter would put the policy in the type, which
sounds better until every function that accepts a writer has to be generic over it
for no benefit.

---

## D-076 — Buffering is fixed; it is never inferred from whether the output is a terminal — **SETTLED**

C decides `stdout`'s buffering by calling `isatty`: line-buffered to a terminal,
fully buffered to a pipe. **Nitpick does not.**

| Stream | Buffering |
|---|---|
| `stdin` | fully buffered |
| `stdout` | **line buffered, always** |
| `stderr` | **unbuffered, always** |

The C behaviour is a default that varies by circumstance, which the blueprint
philosophy treats as a defect in the design rather than a convenience. Its
practical cost is well known: a program's output interleaves differently, or
vanishes on a crash, depending on whether it was run in a terminal or through a
pipe — so the configuration that gets debugged is not the configuration that
ships.

`io_isatty` remains available (it is the one `ioctl` the D-049 syscall audit
retained). It answers a question; it does not silently change behaviour.

A program wanting throughput on `stdout` wraps it in its own buffered writer,
explicitly.

### Buffered data is not flushed on a trap

`defer` does not run on a trap (D-014), and D-063 makes a trap a whole-program
event in which no task resumes and no cleanup executes. **Pending buffered output
is therefore lost**, and no flush is attempted, because flushing means running
code against state the fault may have corrupted.

This is stated rather than mitigated, because the mitigation belongs to the
program:

- `stderr` is unbuffered, so **diagnostics written to it survive a trap** — which
  is the reason it is unbuffered rather than a performance judgement.
- The registry of open streams is reachable from `failsafe`, alongside the
  allocation registry D-014 already hands it. `failsafe` may choose to flush; the
  runtime may not choose for it.
- Anything whose loss is unacceptable goes to `stderr` or an unbuffered stream, on
  the same reasoning that puts actuator safing in `failsafe` rather than in
  `defer`.

---

## D-077 — One manifest schema; no editions — **SETTLED**

`nitpick.toml` currently has **two incompatible schemas in the wild**, one
filename:

| | `nitpick`, `nlibc`, `npkc-native` | `npkg` |
|---|---|---|
| Identity table | `[project]` | `[package]` |
| Build settings | `entry` inside `[project]` | a separate `[build]` table |
| Dependencies | `nfs = { path = "../nfs" }` | `ntoml = "1.0.0"` |
| Extra keys | `target`, `[nikos]` | `edition`, `license` |

A tool reading one cannot read the other, and nothing says which is correct.
This is the same defect shape as the two spellings of `opaque` (D-066) and the
two encodings of the error state in `Result` (D-069).

**The canonical schema is `[project]` for identity, `[build]` for build settings,
`[dependencies]` for dependencies, and `[verify]` for verification
configuration** — the three-repo form, with `npkg`'s genuinely useful `[build]`
separation adopted rather than folding `entry` into `[project]`.

`[nikos]` in `npkc-native`'s manifest is retained as a subtable of `[verify]`:
verification configuration in the manifest is established practice here and is
correct, since the flags a project must be verified under are a property of the
project, not of whoever typed the command.

### There are no editions

`npkg`'s manifest carries `edition = "2027"`. **Rejected.**

An edition is a mechanism for keeping incompatible language versions alive in one
compiler. For Nitpick that means the frontend accepts more than one language, the
type checker branches on which, and **every verification obligation has to be
discharged for each edition**. That is a direct multiplier on the one thing this
project cannot afford more of.

There is one language. When it changes incompatibly before 1.0, the source
changes with it; after 1.0, it does not change incompatibly.

---

## D-078 — A build never touches the network; dependencies are locked and vendored; output is reproducible — **SETTLED**

### No resolution at build time

`npkg` today resolves version strings (`ntoml = "1.0.0"`) against a registry, with
`registry.json`, `downloaded_registry.json`, and a PubGrub solver
(`npkg/src/pubgrub.npk`) to do it.

**A build must not do that.** Three reasons, in order of weight:

1. **The artifact verified must be the artifact shipped.** If dependency
   resolution can produce a different graph tomorrow, verification establishes a
   property of something that no longer exists. Against a single non-renewable
   30-day Astrée run, that is disqualifying.
2. **It is a supply-chain surface.** The zero-dependency rule exists because code
   outside the trusted computing base cannot be intercepted or reasoned about;
   fetching source at build time reintroduces exactly that, over a channel nobody
   is watching.
3. **A build that needs a network is not a build.** It fails in a locked-down
   environment, which is where safety-critical software is built.

**Dependencies are pinned in a lock file by exact version and content hash, and
their source is vendored into the repository.** Resolution — including any
version-range solving — happens only during an explicit, human-invoked
`npkg update`, whose output is the lock file and the vendored tree. The build
itself reads the lock and the filesystem, and nothing else.

### Builds are reproducible

**The same inputs must produce a byte-identical output.** No timestamps, no build
paths, no hostnames, no environment leakage into the artifact, and deterministic
ordering everywhere it could vary — module compilation order, and the order
monomorphized instantiations are emitted and deduplicated (D-064).

This is not tidiness. It is what lets anyone confirm that the binary they are
running is the binary that was verified, and D-079's fixpoint check is impossible
without it.

D-064 already helps: mangled names are readable and reversible with no hash, so
nothing in a symbol name depends on how the compiler happened to be invoked.

---

## D-079 — Three-stage bootstrap with a fixpoint check — **SUPERSEDED by D-085**

> ⛔ **The fixpoint check and its honest limits survive; the choice of stage 0 does
> not.** This decision made the prototype `npkc` the bootstrap compiler. D-085
> replaces it with a purpose-built seed, because the prototype implements the
> language Nitpick *used to be* — so seeding from it forces our own sources into a
> foreign dialect and creates a migration debt to undo later. Read D-085 for the
> ladder; the reproducibility requirement and the Thompson caveat below are
> unchanged and carry forward. The "stage 1 and stage 2 must be byte-identical"
> sentence below was restated by **D-202** (successive *emissions* of the
> compiler, not binaries from two emitters).

The prototype builds with **CMake**, which is barred here, and nitpick-native
cannot build itself before it can compile anything. The ladder:

| Stage | Built by | Purpose |
|---|---|---|
| **0** | the prototype `npkc` (`../nitpick/build/npkc`) | scaffolding — compiles nitpick-native's sources once. **Not** the verified artifact. |
| **1** | stage 0's output, compiling nitpick-native's own sources | the first self-hosted compiler |
| **2** | stage 1, compiling the same sources again | the artifact of record |

**Stage 1 and stage 2 must be byte-identical.** That is the fixpoint check, and
it is the whole point of the arrangement: if they differ, something from stage 0
is still influencing the output, and the result is not self-hosted. D-078's
reproducibility requirement is what makes the comparison meaningful.

**The verified artifact is stage 2**, and stage 0's C++ lineage does not appear in
it.

### Using the prototype as stage 0 is consistent with the dependency rule

The rule concerns what is in the shipped artifact's trusted computing base, and
after the fixpoint nothing of stage 0 is. This is the same distinction D-067 drew
for LLVM: a tool that is *invoked* and produces text is not a dependency; a
library that is *linked* is.

### What the fixpoint does not prove

Stated because an auditor will ask, and because a hedge would be worse than the
admission.

**A self-reproducing compiler backdoor introduced at stage 0 would survive the
fixpoint check.** This is Thompson's *Reflections on Trusting Trust*, and it is a
property of bootstrapping in general rather than of this arrangement. The check
proves self-consistency, not the absence of an adversarial stage 0.

The available mitigation is **diverse double-compilation**: build stage 1 from a
second, independently obtained stage 0 and confirm the stage 2 outputs match. It
is not required today — the prototype is our own code on our own machine — and it
is recorded here so that the option is understood before it is needed rather than
discovered during a trial.

---

## D-080 — `relay` propagates an error to the caller — **SETTLED**

> **Amended by D-179 (1.1.6):** relay propagates the error's IDENTITY
> verbatim; the ORIGIN CHAIN grows one site per hop. What the caller
> receives is the same `Error`; what `failsafe` can additionally read is
> where it has been.

Nitpick had **no way to propagate an error.** The complete `Result` surface was
`?` (unwrap with a *mandatory* default), `??`, `?!` (trap to `failsafe`), `?.`,
`?|`, `_?` / `drop`, `_!` / `raw`, and `!!!`; the return forms were `pass`,
`fail`, and the literal `return Result{…}`. Nothing meant *"if this is an error,
return that error to my caller."*

Verified against the prototype before being called a gap: `token.h` has
`TOKEN_QUESTION`, `TOKEN_QUESTION_BANG`, `TOKEN_QUESTION_PIPE`, and
`TOKEN_UNDERSCORE_QUESTION`, and nothing else in the family.

### Why the absence is a safety problem, not an ergonomic one

In a language where **every function returns `Result<T>`**, propagation is the
single most common operation, and hand-writing it costs three or four lines at
every call site. The three convenient alternatives are all worse:

| Reached for instead | What it actually does |
|---|---|
| `raw` / `_!` | *(as this was written)* bypassed the `Result` discipline entirely; **D-163 re-grounds it** — a checked, zero-cost unwrap licensed only by a `never fails` callee (the refusal flips at 1.1.2) |
| `?!` | escalates a **recoverable** error into whole-program shutdown |
| `?|` with a default | substitutes a value for an error — the D-002 failure mode, silent success |

So the missing operator applies steady pressure toward exactly the constructs the
safety case exists to make rare.

**The evidence that hand-writing does not hold up**, measured across the
ecosystem: `libn` forwards the original error code in **92 of 126** sites — mostly
right. The prototype's own standard library forwards it in **0 of 19**; every one
is `fail 1;`, a fabricated code that discards what actually went wrong.

That second number is the finding. A mechanism that must be written by hand at
every call site gets written wrong some of the time, and when it is wrong the code
reaching `failsafe` did not come from the fault. D-014 rests on the opposite —
*"the person who knows the real conditions is the application author"* — and a
handler cannot distinguish an out-of-memory from a device fault if every layer
rewrote the code to `1`.

### The form

```nitpick
int32:v = relay parse(s);        // keyword
int32:v = _^ parse(s);           // shorthand
```

- **If the operand is an error**, the enclosing function returns immediately with
  `Result{ value: zero, err: <the same code, verbatim> }`.
- **Otherwise** the expression evaluates to `.value`.

`relay` joins `raw` / `_!`, `drop` / `_?`, and `discard` / `_~` as a keyword with
an underscore-family shorthand — the family of unary operations on a `Result`.

> **On the choice of `^`.** `..^` already uses the character, for spread. The
> objection is worth stating rather than hiding: the family prefix — `..` or `_` —
> is what names the family, and the trailing character discriminates *within* it,
> so `^` is not carrying a global meaning that two uses would contradict. Within
> the `_` family it reads as upward, to the caller, which is what the operation
> does.

**`relay` rather than `try`.** `try` belongs to languages with exceptions, and
Nitpick has none; the word would import an expectation of catching that nothing
here provides. `relay` states the actual behaviour — forward it onward,
unaltered — and sits naturally beside `pass` and `fail` as the third thing that
can go back to a caller.

### It is a normal exit path

**`defer` runs.** `relay` is an ordinary return, not a trap, so it belongs in
D-014's list of paths that execute `defer` — *"scope exit, `return`, `pass`,
`fail`, and `exit`"* — which is **amended to include `relay`**.

This distinguishes it sharply from `?!`, which traps and therefore runs nothing.

It also leaves **no `unknown` taint**: on the success path the value is a real
value that was checked, unlike `raw`, which produces a value that was not.

### Where it is illegal

**In `main` and `failsafe`.** They return a bare `int32` and leave through
`exit`, so there is no `Result` for an error to be relayed into. A compile error,
and a precise one — those are exactly the two functions D-013 exempts from the
universal `Result` rule, so the restriction needs no new concept.

### Amendments this forces

1. **D-014's `defer` list** gains `relay`.
2. **D-060's closed expression list** gains `relay` — and gains **`raw`, which was
   already missing from it.** `int32:y = raw f();` is plainly an expression and
   the enumeration did not include it.
3. **The precedence table** gains a level. `raw`, `drop`, `await`, and now `relay`
   were **absent from all eighteen levels**, so nothing said whether
   `raw a.eq(b)` binds to `a` or to the call, or whether `raw f() => int32` casts
   the `Result` or the value. See D-081.

---

## D-081 — The unary `Result` operators get a precedence level — **SETTLED**

`raw` / `_!`, `drop` / `_?`, `await`, and `relay` / `_^` appear nowhere in
`OP_REFERENCE.md`'s eighteen-level precedence table, so their binding was
undefined. They are the most frequently written operators in the language.

**They occupy one new level, immediately below Postfix and above Pipeline**, and
are right-associative, as prefix unary operators must be.

The level is fixed by three requirements, each forced rather than chosen:

| Must parse as | Therefore |
|---|---|
| `raw a.eq(b)` → `raw (a.eq(b))` | **looser** than Postfix — the operand is the whole call, not the receiver |
| `raw f() => int32` → `(raw f()) => int32` | **tighter** than Cast — casting a `Result` is meaningless; the value is what is cast |
| `raw f() \|> g()` → `g(raw f())` | **tighter** than Pipeline — the value is piped, not the `Result` |
| `raw f() + 1i32` → `(raw f()) + 1i32` | tighter than Additive, which the above already implies |

Levels 2 through 18 shift down by one. Nothing implements the table yet, so
correcting it now costs a renumbering and no rework.

`discard` / `_~` is **not** included: D-060 makes it a statement, not an
expression, so it has no place in an expression precedence table.

---

## D-082 — Lock acquisition is a plain block and is awaited; the `with` construct is removed — **SETTLED**

Two defects in D-056's surface syntax, both introduced by later decisions or by
never having reached the grammar.

### `with` had two unrelated meanings, and one of them did not exist

| Use | Source |
|---|---|
| `func:open = int32(…) fails on result < 0i32 **with errno**;` | D-002 — binds the FFI error source |
| `**with** (cfg_lock.acquire(deadline)?) : guard { … }` | D-056 — scoped acquisition |

Unrelated jobs, one keyword — the exact defect **D-028** fixed for `Type`, which
*"had two unrelated meanings distinguished only by position — a direct blueprint
violation, and genuinely ambiguous to parse."*

The scoped form was also **not in the grammar at all**: `with` appears only in
`VerificationKeyword`, never in `ControlFlow`, and there is **no `WithStmt` in
`AST_REFERENCE.md`** — checked against every other surface construct (`defaults`,
`fall`, `give`, `where`, the ternary, macros, `comptime`, `await`), and it was the
only one with no node. The syntax for *acquiring a mutex at all* was missing from
the grammar the parser is being built from.

### It is removed, not renamed

The obvious repair is a new keyword. **The better one is no keyword**, because a
bare block already does the job:

```nitpick
Mutex<Config, 2>:cfg_lock;

{
    Guard<Config>:guard = relay await cfg_lock.acquire(deadline);
    guard.value.retries = 3i32;
}   // guard drops here; the lock is released
```

`CONTROL_REFERENCE.md` §4.1 already specifies bare blocks — *"blocks introduce a
lexical scope … scope-managed bindings are destroyed at the closing brace"* — and
that is precisely and entirely what `with` was providing. The guard's lifetime is
the block, release is RAII, and D-080's `relay` propagates a failed acquisition
without ceremony.

So the fix costs **one fewer keyword meaning, one fewer AST node, and no new
grammar**, and `with` goes back to having exactly one meaning: the FFI error
source binder.

### Acquisition is awaited

D-056 predates **D-071**, which requires that every blocking operation suspend the
calling task rather than park the thread. A lock acquisition blocks.

`await cfg_lock.acquire(deadline)` — an acquisition that parked the OS thread
would stall every sibling task pinned to that executor, which is the precise
hazard D-071 exists to remove, and a mutex is the most likely place to hit it.
`CONCURRENCY_REFERENCE.md` §9 said acquisitions are deadline-bounded and return
`Result`, and never said they suspend.

This applies to the whole of §9: `Mutex`, `RwLock`, `CondVar.timedwait`, and
`Barrier.wait` are all `async`.

---

## D-083 — Thread lifetime is lexical, like everything else; and where the join deadline comes from — **SETTLED**

D-073 removed `Thread.detach` — it returned success while leaking a 2 MiB stack
and a context page — but **nothing then said how a thread's lifetime is bounded or
who joins it.** `Thread.spawn` appears nowhere in the spec set at all.

### Threads nest, like everything else

**A spawned thread cannot outlive the scope that spawned it.** Scope exit joins
it, under a deadline, and expiry traps to `failsafe`.

This is not a new rule; it is the fifth application of one the language already
makes everywhere:

| Thing | Cannot escape its scope | Decision |
|---|---|---|
| borrows | pass down, never up | D-004 |
| task frames | joined at scope exit | D-062 |
| channel endpoints | borrows of scope-owned storage | D-072 |
| slices | non-owning second-class views | D-070 |
| streams | closed at scope exit | `IO_REFERENCE.md` §6 |
| **threads** | **joined at scope exit** | **this** |

A thread that must live for the whole program is spawned in `main`'s scope —
exactly as long-lived as "detached" ever meant in practice, which is the same
answer D-062 gave for tasks.

**There is no thread handle**, for the same reason D-058 leaves no way to name a
task: with the lifetime structural, there is nothing a handle would be used for,
and a handle is what makes leaking and double-joining expressible.

### D-004's borrow ban stays, and for a different reason than lifetime

It is worth being precise, because lexical thread lifetime *would* make a borrow
crossing a spawn lifetime-safe — the spawning scope now strictly outlives the
thread.

**The ban remains**, because it is not a lifetime rule. Two threads holding
borrows of the same storage is a **data race**, and race freedom is what
`CONCURRENCY_REFERENCE.md` §5.3 lists it for. Lexical lifetime closes the
dangling half; the ban closes the aliasing half. Both are needed and neither
replaces the other.

### Where the join deadline comes from

D-062 requires that scope exit join unfinished tasks *"under a mandatory
deadline"* and **never said where that deadline comes from.** `drop work()` has
nowhere to put one.

**The join deadline is a property of the executor, fixed where the executor is
created**, and it applies to every task on it. A thread's is fixed at spawn, since
spawning a thread creates its executor (D-071).

That places it in exactly one greppable location per thread, keeps `drop work()`
free of ceremony, and makes the value reviewable — which a per-call-site deadline
scattered across a codebase would not be. A program-level default applies where
nothing is stated, and the default is a stated constant rather than "whatever the
runtime felt like", so it can be audited and overridden.

---

## D-084 — `NIL` is zero-sized; and the ABI cost of universal `Result<T>` is recorded — **SETTLED**

Two performance findings from the final spec sweep. Neither weakens a safety
rule; the first is free, the second is an obligation rather than a change.

### `NIL` is a zero-sized type

**`NIL`'s size was never specified.** `TYPE_REFERENCE.md` §27 describes it as
*"nothing at the type level"* and then gives `Result<NIL>` as `{ i8 undef, i32 }`
— which, with alignment, is **8 bytes to carry no information**: one byte, three
of padding, four of error.

Since "void functions do not exist" and every value-less function returns
`Result<NIL>`, that is the most common return type in the language.

**`NIL` is zero-sized.** `Result<NIL>` is therefore `{ i32 }` — **4 bytes, align 4,
returned in a single register.** A zero-sized type is also the honest
representation of a type whose only value carries no information, and LLVM
represents it directly.

Consequences are uniformly benign: a `NIL` struct field occupies nothing,
`pass(NIL)` moves nothing, and `Optional<NIL>` degenerates to its tag.

### The `Result<T>` ABI cost, and the obligation it creates

**Nothing in the spec set analysed what universal `Result<T>` costs** — a search
for any discussion of the return-ABI cost returns nothing. For a language where
*every* function returns `{T, i32}`, and whose flagship consumer is
computationally enormous, that gap should not survive into planning.

This is **not** an argument for weakening D-013. It is the missing analysis.

**Expected codegen**, x86-64 SysV:

| Success type `T` | `Result<T>` | Returned |
|---|---|---|
| ≤ 12 bytes | ≤ 16 bytes | **in registers** — no worse than a two-word return |
| **13–16 bytes** | 20–24 bytes | **memory (`sret`)** — where bare `T` would have been in registers |
| > 16 bytes | > 16 bytes | memory either way — **no marginal cost** |

So the cost is not uniform: it is nil for large types, small for small ones, and
concentrated in a **cliff at 13–16 bytes**, where wrapping pushes a
register-returned value into memory. `int128` and `complex<flt64>` are exactly
there.

**The branch is the other half**, and it is recoverable. After inlining, LLVM's
SROA folds the `Result` apart and the check disappears wherever the error path is
provably not taken; `raw` elides the check but not the struct. The cost therefore
concentrates at **non-inlined call boundaries**, which is where to look.

**The obligation:** this must be **measured before the performance case is made**,
not assumed in either direction. Specifically —

1. benchmark the 13–16 byte cliff against bare returns, since that is the only
   place the ABI genuinely regresses;
2. confirm SROA folds the `Result` at `-O2` across the inlining boundary in
   practice, rather than trusting that it should;
3. if the cliff proves to matter on a hot path, the answer is **layout** — the
   error field's position and the struct's alignment are ours to choose — and
   never an exemption from returning `Result`.

Recording this now means the performance argument rests on numbers when it is
made, which is the standing requirement that performance is first-class but
strictly subordinate to safety.

---

## D-085 — The bootstrap seed is purpose-built, not the prototype — **SETTLED**

> **[1.4.0 note]** The "byte-identical" fixpoint wording this record
> carries from D-079 (here, the ladder table, and "What is unchanged")
> was restated by **D-202**: stage 1 and stage 2 are two independent
> emitters and can never be byte-identical as binaries — the criterion is
> that successive **emissions of the compiler** by current-source stages
> are byte-identical, which is what the harness has measured since 0.8.1.
> Read those sentences through D-202.

Supersedes **D-079**'s choice of stage 0. The three-stage structure, the
byte-identical fixpoint check, and the Thompson caveat all carry forward
unchanged; what changes is what sits at the bottom.

### Why the prototype cannot be stage 0

D-079 made `../nitpick/build/npkc` the bootstrap compiler on the grounds that a
tool which is *invoked* is not a dependency, and that the fixpoint washes out its
lineage. Both remain true, and both miss the decisive objection.

1. **It implements a different language.** Checked against
   `src/frontend/lexer/lexer.cpp`: no `relay` (D-080), no `cstring` (D-049). The
   prototype is the language Nitpick *used to be*.
2. **So it forces our own sources into a foreign dialect**, and creates a debt to
   undo later. The plan built on D-079 scheduled an entire cycle to migrate off
   that dialect — **a bootstrap that needs a migration phase to escape its seed is
   telling you the seed is wrong.**
3. **It is unaudited and unverified**, which is the opposite of the property this
   project exists to establish, and `nitpick-repo-lineage` already frames the
   prototype as a **behavioural oracle, not a structural model**. Stage 0 is a
   structural role.

### The ladder

| | What it is | Written in | Fate |
|---|---|---|---|
| **Seed** | a **subset-1** → LLVM IR compiler | a throwaway generator script | discarded once stage 1 exists; its emitted IR is committed as the reproducible seed |
| **Stage 1** | the real compiler — **full frontend**, rung-1 backend | **Nitpick, subset 1** | permanent |
| **Stage 2** | same source, compiled by stage 1 | Nitpick | **must be byte-identical to a stage-1 rebuild** |

We therefore **write in our own language from day one**. "Subset 1" is an honest
statement about what our own backend can lower yet — the capability ladder working
as designed — rather than a workaround for someone else's compiler.

### The rule that keeps the frontend intact

> **The parser never restricts. The backend does.**

The real frontend accepts the **whole grammar from day one**, per `CLAUDE.md`'s
build-the-frontend-once strategy. A construct the current rung cannot lower
produces a **backend** diagnostic — *not supported at this rung* — never a parse
error.

This is the decision's most important consequence, and it is aimed at a specific
past failure: `nitpick-bootstrap` was abandoned because **the parser was rewritten
at every stage.** That happens when the grammar is partial and gets re-widened
rung by rung. Here it cannot: the grammar is complete before the backend exists,
and capability restriction lives entirely in lowering.

It also bounds the seed. **The seed only needs to compile the subset the real
compiler's source is written in**, not the full language — so a seed implementing
the full parser would mean writing the full parser twice, in two languages, and
letting them drift.

### Subset 1

Constrained from both sides: expressive enough to write a complete compiler
frontend in, small enough that a throwaway seed can lower it. Roughly — integer
and char types, `bool`, pointers, slices, structs, tagged enums, arrays,
functions returning `Result<T>`, `if` / `while` / `pick`, `pass` / `fail` / `raw`,
and allocation.

Explicitly **not** in subset 1: generics, traits and `dyn`, `async`, macros,
`comptime`, contracts, verification constructs. The AST is expressible without
generics because it is **tagged enums over composable structs** — which
`CLAUDE.md` already records as the transferable frontend technique, and which is
what makes this subset viable.

Defining subset 1 precisely is the first real work of cycle 0.0.

### The seed

A small throwaway generator emitting `.ll` text. It is **invoked once, ever** —
weaker than D-067's "invoked, never linked", since `llc` runs at every build and
the seed runs at the beginning of history.

**Its emitted IR is committed**, so rebuilding from source needs only the LLVM
toolchain; the generator is needed to *regenerate* the seed, never to build. That
is what keeps the standing target true: **a machine with only the LLVM toolchain
should be able to build the compiler**, plus Z3 and NIKOS for verification.

Nothing from the generator reaches any artifact, and D-079's fixpoint check —
carried forward — is what demonstrates it.

### What is unchanged from D-079

The stage-1/stage-2 byte-identical comparison, its dependence on D-078's
reproducible builds, the fact that **stage 2 is the artifact of record**, and the
honest limit: a self-reproducing backdoor introduced at the seed would survive the
fixpoint, with diverse double-compilation as the mitigation if ever required.

That limit is **smaller now than under D-079**, and worth noting as a second
benefit: a purpose-built seed of a few thousand readable lines can actually be
audited, where a 26,000-file C++ prototype cannot.

---

## D-086 — A cycle among modules is legal — **SETTLED**

`MODULE_REFERENCE.md` says nothing about import cycles. The question arrived from
the parser rather than from the module system: cycle 0.2.6 needed it, and 0.2.8
had it scheduled behind work that could not start without the answer.

**A `use` cycle among modules is legal. The module loader resolves it.**

### What forced it

Three constructs in the frontend want to import each other, and none of the three
is a mistake in decomposition:

| Construct | Needs | Which needs |
|---|---|---|
| `pick` as an expression (D-059) | the expression parser | the arm parser |
| `pick` as a statement | the statement parser | the arm parser |
| arm bodies | the statement parser | blocks |
| `ArrayType.size` (0.2.8) | the type parser | the expression parser |
| casts | the expression parser | the type parser |

`pick` is the clearest case. D-059 settled that it is **both** a statement and an
expression and that its arms decide which. Both parsers must therefore reach the
same arm-parsing code, and the arms contain blocks, which belong to the statement
parser. There is no ordering of these files that removes the cycle, and no third
file to factor out that is not simply the union of the two.

The type/expression pair is the same shape: a cast holds a type, an array size
holds an expression. That is a property of the *language*, not of where the code
was put.

### Why legal, rather than forbidden

**`use` names a namespace. It imports no initialisation order.** That is the
whole argument. A cyclic import is a hazard in languages where importing a module
*runs* it, because then a cycle has to pick a first module and some names are
observably unbound while it runs. Nitpick has no module-level execution: a module
is a set of declarations, globals are compile-time-initialised, and there is
nothing to sequence. The cycle is a fact about the *name graph*, which is
resolved by collecting declarations before resolving bodies — a two-phase load
the compiler needs anyway for forward references within a single file.

Forbidding cycles would mean splitting files whose only reason to exist is to
satisfy the rule. Those files are worse than the cycle: they carry no idea, and a
reader must reconstruct why the split happened before they can tell whether it
still should.

### What the loader must do

1. **Collect every declaration in every module in the graph** before resolving
   any body. This is the phase that already exists for within-file forward
   references (`ast_scratch_commit` calls `ast_ids_push`, defined below it), now
   applied across the graph.
2. **Report a cycle only when it is genuinely unresolvable** — a struct whose
   size depends on itself other than through a pointer, or a `const` whose
   initialiser depends on itself. Those are errors about the *declarations*, and
   the diagnostic must name the cycle's members in the order they refer to each
   other. "Circular import" is not a diagnostic anyone can act on.
3. **Never make resolution order-dependent.** The same module graph must produce
   the same program regardless of which member the loader entered first. This is
   a reproducibility requirement (D-078) before it is a convenience: a build that
   depends on entry order is a build that differs between the stage-1 and stage-2
   compilers, and the fixpoint check would fail for a reason unrelated to
   correctness.

### What it does not license

**Cycles are legal, not encouraged.** A cycle that exists because two modules
each grew a function that belonged in the other is still a decomposition mistake;
this decision says only that the *language* does not forbid it, so the fix is
moving the function rather than inventing a file. `parse_decorate.npk` — created
in 0.2.5 to hold the attributes, `limit`, contracts and invariants that decorate
both declarations and statements — is the good case: it exists because those four
are genuinely a shared layer, not because a rule demanded a file.

### Follow-up

- `MODULE_REFERENCE.md` — a section stating cycles are legal and what the loader
  guarantees.
- Cycle 0.3, which builds the module loader, implements the two-phase collect and
  the order-independence property. The unresolvable-cycle diagnostics belong
  there too.
- 0.2.8 loses its blocking question; `ArrayType.size` can call the expression
  parser directly.

---

## D-087 — A function type is spelled `func RetType(ParamTypes)` — **SETTLED**

`AST_REFERENCE.md` §4 has declared a `FuncType { params, return_type }` all
along. **No document in the set gives its syntax.** The whole of the
specification is one line of `FULL_specs.txt`:

```ebnf
FuncType   ::= "func"
```

followed by a note deferring the details to "Chapter 02: Declarations and Scope",
which does not cover them. So the node existed, the parser had nothing to build
it from, and a function-pointer-typed variable could not be declared — while §3.6
states that "function pointers are ordinary values referenced by
`IdentifierExpr`". Values with no type.

### The spelling

```nitpick
func int32(int32, int32):op = @add;        // a variable holding a function
func:apply = int32(func int32(int32) never fails:f, int32:x) never fails { pass raw f(x); };
func NIL():callback;                        // no parameters, no return value
```

**`func` then the return type then the parameter types.** Parameters are types
only — a function *type* names no bindings, because there is nothing for a name
to bind to.

### Why not the prototype's form

`../nitpick`'s parser accepts `(int32)(int32, int32)` — the return type
parenthesised, then the parameters parenthesised. It is a real form that really
works there, and it is being replaced rather than carried forward.

- **It announces nothing.** Parentheses already group expressions, hold call
  arguments, and wrap `move(…)` and `comptime(…)`. Giving them a fourth job means
  a reader works out which one from what follows rather than from what they are
  looking at. `func` says what it is in the first token, which is facet 2 of the
  blueprint philosophy: maximum meaning in minimum space, and a symbol that could
  mean four things carries less than a word that means one.
- **It does not match the declaration.** `func:add = int32(int32:a, int32:b)`
  declares a function: keyword, name, return type, parameters. The type of that
  function should read the same way minus the name, and `func int32(int32, int32)`
  does. `(int32)(int32, int32)` reorders nothing but re-spells everything, so a
  reader has two shapes to hold for one idea — the exact cost facet 1 exists to
  avoid.
- **It keeps the one line of specification that exists true.** `FuncType ::=
  "func"` becomes the head of the production rather than a fragment nobody
  completed.

### Where it may appear

Anywhere a type may: a variable, a parameter, a struct field, a return type, a
generic argument. It composes with the type suffixes like anything else —
`func int32(int32)->` is a pointer to one, `func int32(int32)?` an optional one,
`func int32(int32)[]` a slice of them.

**There is no capture and no closure** (D-018), so a value of function type is a
plain code address and the type carries no environment. That is what makes it one
machine word and what makes `->` on it mean the ordinary thing.

### Follow-up

- `LEXICAL_REFERENCE.md` — the production, replacing the one-line fragment.
- `AST_REFERENCE.md` §4 — the note column, done.
- The parser reads it in 0.2.8. Lowering waits for a rung that needs indirect
  calls; the parser never restricts (D-085).

---

## D-088 — `mod` and `extern` take the declaration shape; the `Type` namespace is removed — **SETTLED**

Supersedes the namespace half of D-028.

### 1. Every declaration has one shape

`keyword : name = value ;` — and until now two declarations did not:

```nitpick
mod network  {  … }          // no colon, no `=`, no terminator
extern "libc" { … }          // the same
```

against `func:`, `struct:`, `enum:`, `trait:`, `impl:`, `Rules<T>:`, `macro:`
and `assoc:`, all of which do. The forms become:

```nitpick
mod:network = { … };         // inline
mod:network;                 // the body is in network.npk
extern:"libc" = { … };
```

`mod:network;` with no `=` matches `assoc:Item;` exactly: a declaration with no
bound value is a declaration that something exists elsewhere.

**Why the string stays a string in `extern:"libc"`.** A library name is not an
identifier — `libc++`, a bare `m`, a path — so the name position holds a string
literal. That is a *value* in the name slot, not a second shape.

**An earlier attempt at a rule failed, and the failure is the argument.** During
0.2.5 this was written up as "`mod` and `extern` GROUP rather than bind, so they
take no `=`". It does not survive contact with `trait`, `impl` and `Type`, which
group *and* bind. There was no rule — only two constructs that happened to be
spelled the way another language spells them.

The cost of a special case is not the character count. It is that **every
exception is one more thing a reader has to remember**, and nothing was bought
here in exchange.

### 2. `Type:Name = { … }` is removed

D-028 gave `Type` two jobs — namespace and associated type — and moved the
associated type to `assoc`, keeping `Type` as the namespace on the grounds that
it was "the older and more visible construct".

**That left two constructs for one job.** `AST_REFERENCE.md` §1 called `TypeDecl`
"the namespace construct"; `MODULE_REFERENCE.md` §1 opens "Modules allow the
organization of code into hierarchical namespaces". D-028's own table names the
`Type` row "namespace / **module** grouping". An inline `mod` holds functions and
structs and nothing else, which is exactly what `GRAMMAR_ADOPTION_CONFLICTS.md`
records `Type:Counter` as holding.

So `Type` goes and `mod` stays. `mod` is strictly the more capable of the two: it
can name a **file**, it participates in the module graph, and `use` imports from
it. `Type` could do none of that.

**Its origin supports removal.** The construct appears to be a survivor of an
earlier design in which OOP was emulated through a text preprocessor and macros;
it was carried forward into the current macro system without anyone re-deciding
that it should be. No document motivates it, and nothing in the ecosystem uses
it. A construct nobody chose is exactly the kind that accumulates.

### 3. `Type` stops being a reserved word

With no construct left to name, `Type` returns to userland — the same reasoning
D-041 applied to 35 collection keywords and D-074 to `stream`, `process`, `pipe`,
`debug` and `log`. **A reserved word that names nothing costs a user an
identifier and gives a reader a keyword they cannot look up.**

`assoc` is unaffected and keeps the job D-028 gave it.

### Follow-up

- `MODULE_REFERENCE.md` §1.1 — the two `mod` forms.
- `LEXICAL_REFERENCE.md` — `Type` leaves `TypeKeyword`.
- `AST_REFERENCE.md` §1 — `TypeDecl` removed; `ModuleDecl` and `ExternBlock`
  keep their nodes and change only their spelling.
- `TRAITS_REFERENCE.md` and `GRAMMAR_ADOPTION_CONFLICTS.md` — the notes
  contrasting `assoc` with `Type` now record that `Type` is gone entirely, which
  is a stronger form of the same point.

---

## D-089 — `main` takes `cstring[]:argv` and nothing else; the declaration-site `_~` is restored — **SETTLED; D-163 sharpens `discard` (checked since 1.1.0)**: it takes a VALUE, never a `Result` — a never-failing value ignored is `discard(raw f())`, and a `Result` in statement position needs a keyword (`drop`/`relay`/`?!`/`?| NIL`)**

Two features the prototype implements and the carried-over spec set lost. Both
are recovered here, and one of them is corrected on the way in.

### 1. `main` has parameters, and the specs dropped them

Every document in `meta/specs/` writes `func:main = int32()`. The prototype
writes:

```nitpick
pub func:main = int32(int32:argc, wild int8->:argv) {     // nitpick/tests/regression/
pub func:main = int32(int64:argc, int8*:argv) {           // nitpick-bootstrap/
```

`argc` was an integer whose width moved between revisions; `argv` changed
repeatedly — `wild int8->`, `wildx int8->`, `int8*` — none of which is really
`char**`, which is a fair sign the type was never settled.

### 2. `argc` goes

**A slice carries its length.** `T[]` is `{ptr, i64 len}` (D-070), so a separate
count is a second copy of a fact the value already holds — and two things
carrying one fact can disagree. That disagreement is the specific C bug where a
loop trusts `argc` past the end of `argv`, and here the type system removes it
rather than discouraging it.

C needs `argc` because `char**` carries no length. Nitpick does not.

### 3. The type is `cstring[]`

```nitpick
func:main = int32(cstring[]:argv) { … };
```

`argv` is kernel-supplied, NUL-terminated, and not ours. `cstring` is exactly
that type — NUL-terminated, `{ptr, i64}`, and impossible to construct
unterminated (D-049). **`nlibc` already spells this same data `cstring[]:args`
in every `exec` signature** (D-048), so the language and the library agree
without anyone arranging it.

The alternative — converting to owned `string[]` at entry — allocates and copies
before `main` runs, for data most programs read once or never.

### 4. The signature is FIXED, not optional

`main` always takes exactly this one parameter. A program that ignores the
command line writes:

```nitpick
func:main = int32(cstring[]:_~argv) { exit 0i32; };
```

The prototype accepted both arities. That is one more thing to remember for
nothing, and **`failsafe` already sets the precedent in the other direction**:
`func:failsafe = int32(tbb32:err)` takes its parameter always, and most handlers
never read it. Entry-point signatures are fixed. One rule, both of them.

### 5. `Type:_~name` — the declaration-site discard

This is what `_~` was invented for, and the spec set kept only the other half.

| Form | Position | Meaning |
|---|---|---|
| `Type:_~name` | declaration | this parameter is **deliberately** unused |
| `discard(e)` / `_~ e` | statement | suppress the unused warning for a variable |

`AST_REFERENCE.md` §2 had only the second; `OP_REFERENCE.md` described `_~` as
"suppresses unused variable warnings" and nothing more. The prototype's parser
carries the note that names the case exactly:

> `_~` declaration-site discard annotation: `Type:_~paramName`. Marks this
> parameter as intentionally unused without requiring a body-level `discard()`
> call. Parser strips the `_~` prefix so the stored paramName is the plain
> identifier (e.g. `"argc"`, not `"_~argc"`).

**The parameter keeps its name.** That is the point, and it is why this is better
than the `_` placeholder other languages use: `cstring[]:_~argv` still documents
what the slot *is*, so a reader learns the signature from the signature. A
parameter named `_` teaches nothing and a second one is a name collision.

The origin is worth recording because it explains the whole `_` family. The
compiler warned about `main`'s unused `argc` and `argv`; the only escape was a
compiler flag, and there was no placeholder to assign them to. So `discard` was
added beside the operator that already existed for unwanted *return* values:

| Operator | Keyword | Discards |
|---|---|---|
| `_!` | `raw` | the `Result` wrapper, unchecked |
| `_?` | `drop` | a return value |
| `_~` | `discard` | a binding nobody reads |

### 6. Using a discarded parameter is an error

**Not a warning, and not merely unenforced.** `Type:_~name` is a claim about the
function, and a claim the compiler does not check is decoration. If the body
reads `argv` after the signature said it would not, one of the two is wrong and
the compiler knows which line to point at.

This is D-002's reasoning applied to a smaller thing: `never fails` is required
rather than implied so that infallibility is an auditable claim. `_~` is the same
shape — the author asserting something a reviewer can rely on.

### Follow-up

- `AST_REFERENCE.md` — `ParamDecl` gains `discarded: bool`; §2's `DiscardStmt`
  note gains the declaration-site form.
- `OP_REFERENCE.md` — the `_~` row covers both positions.
- `LEXICAL_REFERENCE.md` — the annotation in the parameter production.
- Every `func:main = int32()` in the specs, the tests and the compiler's own
  sources becomes `int32(cstring[]:_~argv)`, and `failsafe`'s unused `err`
  becomes `tbb32:_~err`.
- The runtime floor gains `argv` capture in `_start`; cycle 0.3.0 does it.

---

## D-090 — Nitpick is nominally typed, and types are interned — **SETTLED**

`TYPE_REFERENCE.md` gives every type a layout and **no identity rule**. That is
not a small omission: every comparison the type checker makes depends on the
answer, and the question has a language-visible half and an implementation half.

### The language half: two types with the same shape are different types

```nitpick
struct:Meters = { flt64:v; };
struct:Seconds = { flt64:v; };
```

**These are different types, and one may not be passed where the other is
expected.** Nitpick is **nominally** typed: a type's identity is its declaration,
not its shape.

This is the same decision the language already makes everywhere else, and
recording it here only makes it explicit:

- `bool`, `char8` and `uint8` are all one byte and are three types (semantic types
  over representation).
- `fd`, `pid`, `tid`, `uid` and `gid` are all integers and are five types, which
  is the whole of D-042.
- `cstring` and a slice of bytes have the same layout and are different types,
  which is the whole of D-049.

Structural typing would undo every one of those. `Meters` and `Seconds` are the
general case of the same argument: the reason to declare two structs rather than
one is that they mean different things, and a type system that ignores the
declaration ignores the only place that meaning was written down.

**A named alias is therefore a distinct type, not a synonym.** There is no
`typedef` in the C sense, and nothing that produces two names for one type.

### The implementation half: canonical interned types, compared by index

One `int32` for the whole program. Constructing `int32->` twice yields the same
index, and **type equality is an integer compare**.

- There is no structural walk, so there is nothing structural to get subtly
  wrong — which matters, because a comparison that is *almost* right produces a
  program that compiles and misbehaves.
- A type becomes usable as a **key**, which D-064's monomorphization needs in
  order to deduplicate instantiations at all.
- It matches the AST's own arrangement — index-based, never pointer-based — for
  the same reasons: an index survives the table growing, and an index graph is
  easier to verify than a pointer graph.

The cost is a lookup when a type is constructed. In a compiler whose most
repeated question is "are these the same type", that is the right side of the
trade.

### What this does not decide

Whether a *generic instantiation* is the same type as another with equal
arguments — `Container<int32>` twice — is the same question and gets the same
answer by construction, because interning makes it one index. Recorded here so
that D-064's deduplication is not mistaken for a separate mechanism.

---

## D-091 — `Result<T>` may not be written as a return type — **SETTLED**

Every function returns `Result<T>` (D-013), and the declared type is the
**success** type: `func:f = int32()` returns `Result<int32>`. So what does

```nitpick
func:f = Result<int32>() { … };
```

mean — `Result<int32>`, or `Result<Result<int32>>`?

**Neither. Writing it is a compile error that says so.**

Both readings are defensible and that is precisely the problem. If it means the
first, the spelling is redundant and two ways to write one signature exist. If it
means the second, the language has grown a nesting nobody asked for and every
caller has to unwrap twice. A reader would have to know which, and the answer
would be invisible in the signature they are reading.

Refusing the spelling keeps **one way to write a return type**, which is the
blueprint philosophy's first facet: a construct does not change meaning by
context, and here it does not acquire a second spelling either.

The diagnostic says the rule rather than just refusing:

> every function returns `Result<T>` already, so a return type is written as the
> success type — `func:f = int32()` returns `Result<int32>`

### `Result<T>` stays writable everywhere else

In a **variable** declaration it is the ordinary way to hold a call's outcome
before unwrapping, and the compiler's own sources are full of it:

```nitpick
Result<string>:r = read_file(path);
if (r.is_error) { … }
```

It is also writable as a parameter type, a field type and a generic argument. The
restriction is exactly one position — the return type — because that is the only
position where the wrapper is already implied.

`main` and `failsafe` return a bare `int32` and are unaffected: they are outside
the `Result` discipline entirely (D-013), which is why `relay` is illegal in them.

---

## D-092 — When the compiler chooses a numeric type for you — **SETTLED**

Two questions cycle 0.4.2 could not start without, neither stated outright, both
implied by `SAFETY_ARCHITECTURE.md`'s `--extra-picky` table.

### 1. An unsuffixed integer literal takes its type from CONTEXT

`literal-suffixes` is an **optional** `--extra-picky` rule — "every integer
literal must carry an explicit bit-size suffix" — so a bare `42` is legal in the
language by default. `SUBSET_1.md` §1.4 applies the rule unconditionally to the
compiler's **own** sources, which is why every literal we write is `42i32`; that
is our discipline, not the language's.

So an unsuffixed literal is typed by the position it appears in: the declared type
of what it initialises, the parameter it is passed to, the other operand it is
combined with.

**With no context at all, it is an error rather than a silent `int32`.** A default
would be a type nobody wrote, chosen by a rule nobody read, and every subsequent
diagnostic about that expression would name it — which is exactly the shape of a
bug that takes an afternoon.

### 2. There is no implicit widening

> ⚠️ **Corrected.** This section previously read *"Implicit widening is permitted,
> and narrowly"* and allowed an integer operand to widen to the wider operand's
> type within one signedness. That was wrong, and the reasoning that produced it
> is recorded below because the same mistake is easy to make again.

**Two typed operands share a type or the programmer writes the cast.** Widening
is never implicit, at any pair of widths, in either signedness, for any operator.

```nitpick
int32:a; int64:b;      a + b                 // REFUSED
int32:a; int64:b;      (a => int64) + b      // int64
uint32:c; uint64:d;    c + d                 // REFUSED
int32:e; uint32:f;     e + f                 // REFUSED
int64:g;               g + 1                 // int64 — see below
```

### Why the earlier reading was wrong

`SAFETY_ARCHITECTURE.md` listed `explicit-widening` among the `--extra-picky`
rules, described as "bans implicit widening; all widenings use an explicit cast".
The inference drawn was that a rule can only be *optional* if the default permits
what it bans, so implicit widening must be the default.

**That inverts the document.** Its own summary, three lines below the table,
reads: *"Every escape hatch is explicit, named, and greppable. That is the
standing shape of a Nitpick guarantee: absolute by default, suspended only
through a construct an auditor can search for."* Every rule in that table adds
**pedantry beyond what safety requires** — `shadow` bans shadowing, which is
confusing rather than unsafe; `wild` and `no-wildx` ban constructs that are
already explicit; `literal-suffixes` demands a suffix where the width could be
inferred safely. **None of them gates a safety property**, so none of them
implies the default is unsafe. The row has been removed and the reasoning
recorded there.

The prototype's own name for the policy is **"Zero Implicit Conversion"**, and
its newer, self-hosting-oriented checker (`src/runtime/sema/sema_helpers.cpp`)
requires binary operands to be *identical*: `"Arithmetic operands must have
matching types"`, and the same for comparison and bitwise. Its older C++ path
does widen via `findCommonType`, so the prototype contains both — with the
direction of travel toward the strict rule.

### The safety argument, which is not the obvious one

A same-signedness widening **cannot lose a value**, and that is the reason the
permissive reading looked defensible. It is also beside the point.

**The widening decides which width the operation happens in.** `a + b` computing
in 64 bits because `b` is an `int64` today computes in 32 bits the day someone
narrows `b`, and overflows where it used to fit. The expression is unchanged and
its meaning is not. That is a small drift in numbers arriving through a
conversion nobody wrote — the exact failure mode the language exists to prevent —
and it is invisible at the call site in both states.

It also interacts with D-008: `tbb` arithmetic saturates to ERR at the type's
width, so a rule that silently changes the width changes where saturation fires.

### Literals are not an exception to this

`g + 1` where `g` is `int64` is `int64`, and nothing has been widened: the
literal never had a width of its own. Section 1 above governs it, and the default
is not "guess a width" but "take the width from a context that states it, and
refuse when no context does". `--extra-picky=literal-suffixes` then requires the
suffix written even where the context is unambiguous.

This is the boundary between the two halves of this decision, and it is a real
one: an unsuffixed literal has no type yet, while an `int32` variable already has
one and converting it is a decision somebody must make in the source.

---

## D-093 — A range is a typed value, and ordering is narrower than equality

**Settled in cycle 0.4.2.** Two questions the specs left implicit, decided
together because they are the same question asked twice: *which types can be
put in order?*

### 1. `lo..hi` has a type of its own

`TYPE_REFERENCE` §"Range" lists `a..b` and `a...b` and says only "used in `for`,
`pick` patterns". Nothing says what a range *is*, and the two obvious readings
disagree about a real program.

**A range is a value of type `range<T>`,** where `T` is the common type of its
endpoints. It is interned like every other type (D-090), and `..` and `...`
produce the *same* type — the bound is a property of the value, not of the type,
so a function taking a range accepts both spellings.

> **The spelling landed at 1.4.8 (S-8, user-ratified 2026-09-02).** For four
> cycles this type could only be INFERRED: the resolver had no type named
> `range`, so `range<int32>:r = …` refused with TYPE-001 and nothing in the
> tree ever wrote one — the dormant-rule pattern, found by a D-235 probe.
> `range` is now a builtin generic type keyword beside `Result`, one
> argument, the element held to the range expression's own rule (ordered,
> not a float — D-145's discrete successor). A binding of it iterates and
> passes exactly as the literal does, because the VALUE was always
> canonical half-open. Measured before adding the keyword: no `range`
> identifier anywhere in the tree.

The alternative was to give the expression its element type and let `for` and
`pick` — the only two consumers — check the node kind. That is simpler and
wrong: it makes

```nitpick
int32:x = 1i32..5i32;      // would typecheck, silently, as int32
```

legal. A construct that means one thing in a `for` header and another in an
assignment is exactly the context-dependence the blueprint philosophy exists to
forbid. Giving the range a type means the mismatch is reported where it is
written and names what was found.

### 2. `tbb` and the kernel identifiers compare, and do not sort

D-042 says the five kernel identifiers "compare and do not add", and D-008 says
the same of `tbb`. Neither says whether *comparison* includes `<`. It does not.

**Ordering (`< <= > >= <=>`) requires an ordered type: integers, floats, and
characters. Equality (`== !=`) is available to every type that has one.**

- An **`fd` is an opaque handle.** Its number is an artifact of the order the
  kernel happened to hand it out, so `fd1 < fd2` invites code that reads meaning
  into that order — the same mistake as `fd + 1`, one operator over.
- An **error code is compared against a named constant**, not sorted. `err < ERR`
  is not a question anyone means to ask.
- A **`bool` is two values.** `false < true` is a fact about the representation,
  not about booleans.

**`char8` does order.** Code-point order is a real ordering and `'a' <= c` asks a
question about letters rather than about numbers — which is why `'a'..'z'` is a
legitimate range while `true..false` is not. **Ordering is not arithmetic**, and
this is the decision that keeps those two separate: D-005 bars `'a' * 2` without
barring `'a' < 'b'`.

Where the number really is what is meant, `=>` says so.

---

## D-094 — Slots `a`, `b`, `c` of a node hold node references, never scalars

**Settled in cycle 0.4.2, by a bug that was never an error at any point.**

The AST is six arrays of `{kind, span, a, b, c, payload}`, and the kind decides
what the slots mean. 0.4.2 needed a literal's width suffix — `42i32` and `42` are
different types and the parser had been discarding the difference — and slot `a`
was unused on a literal, so the width went there.

`resolve_expr` walked slot `a` as a child expression on every kind it did not
name, on the strength of a comment reading *"slot `a` is an operand on every node
that has one"*. Nothing checked that. So `42i32` began resolving whatever node
sat at index `WI32`, and the only symptom was a name in an unrelated file
failing to bind. No crash, no diagnostic, no bad output — one test, two cycles
away, exiting non-zero.

The same assumption had been copied into `init_reaches`, the global-initialiser
cycle check, where it also carried its own partial list of window kinds: three
named, four missed, so a self-reference reached through a template literal or a
method call's arguments went unseen.

### The rule

**Slots `a`, `b` and `c` hold node references or window bounds. A scalar goes in
`payload`, or — when the payload is already spoken for, as on a literal whose
value lives there — in a field of its own.** `Token` had already made exactly
this choice, keeping `width` beside `payload` rather than packed into it.

### The check

The rule alone would have prevented this instance. It would not have prevented
the next one, because the walkers were each deciding a node's shape privately and
neither could be shown to be right. So:

- **`expr_shape(kind)`** in `ast.npk` states, once, which slots of each kind hold
  children. Both walkers are driven by it.
- **`EXPR_KIND_MAX`** is generated from `AST_REFERENCE.md` alongside the kind
  enum, and `tests/frontend/ast_storage.npk` walks `1..EXPR_KIND_MAX` asserting
  every kind has an entry. A node kind added to the specification and left out of
  the table fails there, naming the kind, rather than being walked as whatever
  shape the last author assumed.
- **`NITPICK-RESOLVE-009`** is what a walker emits when it meets an unclassified
  kind — a diagnostic about the *compiler*, not the program, because the only
  honest thing to do with a node whose layout nobody has described is refuse to
  read it.

Verified by making it fail: removing `ExprPipeExpr` from the table exits `116`,
which is `100 + 16`, and 16 is `ExprPipeExpr`.

This is the third mechanical completeness check in the compiler, after
`check_kinds_reachable` (0.2) and `resolve_audit` (0.3.5), and it was found the
same way all three were: **two lists that had to agree, with nothing making them.**

---

## D-095 — What `=>` accepts: range containment, not type families

**Settled in cycle 0.4.3.** D-021 fixed the spelling and `TYPE_REFERENCE` §5 fixed
the discipline — *"`=>` is a compile-time error wherever data loss is possible —
not a runtime trap and not a warning"*. Neither says which conversions those are.

**A conversion is lossless when every value of the source type is representable
in the target type, computed from the actual ranges.** Not "same signedness and
wider", which is the rule the prototype uses and the rule this compiler first
had.

The difference is not academic. `uint8 => int16` is lossless — 0..255 sits
inside −32768..32767 — and a family rule refuses it and forces `=>!`. That
matters because **`=>!` is an audit tool**: it exists so that a search for one
token finds every place a conversion was allowed to lose information. Every
provably-safe conversion pushed into it is a false positive in that search, and a
diluted signal is a lost one.

Ranges are held as *(can it be negative, how many magnitude bits)* rather than as
numbers, because the numbers do not fit: `int4096` is a real type.

### Integer to float goes by significand, not by byte width

`int32 => flt64` is accepted — a double represents every `int32` exactly, 53
bits being more than 31. `int64 => flt64` is **refused**, because 53 is fewer
than 63, and this is the conversion every mainstream language performs silently.
`int32 => flt32` is refused too: 24 is fewer than 31.

Float to integer is always lossy at every width, including `flt64 => int128`
which has room for every magnitude and still drops the fractional part. Range
containment cannot see that, so it is stated rather than computed.

### Float significands are IEEE 754-2008 §3.6

The spec names `flt32`, `flt64` and `flt128` against C's `float`, `double` and
`fp128`, and names `flt256` and `flt512` without saying what they are. They
follow the binary interchange format, defined for every width that is a multiple
of 32 at or above 128: exponent = `round(4 * log2(k)) - 13`, significand = the
rest plus the implicit bit.

| | 32 | 64 | 128 | 256 | 512 |
|---|---|---|---|---|---|
| significand | 24 | 53 | 113 | 237 | 489 |

A guess here would not have been visible as a guess — it would have quietly
accepted or refused conversions at the two widest types with nothing to
contradict it.

### Three answers, not two

A conversion is checked-legal, unchecked-only, or **not a conversion at all**,
and `=>!` does not rescue the third.

- **`intN => bool` is refused outright.** This is not a conversion that loses
  information; it is one with no definition. Which integers are true? C answers
  "everything but zero", and that answer is the truthiness this language does not
  have. **`=>!` opts out of a check, not of a meaning.**
- **`uint32 => char32` needs `=>!`** — not every 32-bit number is a Unicode
  scalar value. The surrogates are excluded and the maximum is U+10FFFF, so this
  is a validity question rather than a width one.
- **`fd => int32` is accepted and `int32 => fd` needs `=>!`.** Reading a handle's
  number loses nothing, and D-093 tells the programmer to cast when the number is
  what they mean. Manufacturing one asserts what the compiler cannot check — that
  this number is a live descriptor. `nlibc` needs it, a syscall returning an
  integer that has to become an `fd`, and it should be the only place that does.

### `any-> => T->` requires `=>!`, against `TYPE_REFERENCE` §27

The spec spells it `p => T`. **This decision changes it to `=>!`**, and the
reason is what the two spellings mean: `=>` says *nothing can be lost and the
compiler proved it*. Nothing is proven here — an `any->` is type-erased, so
giving it a type is an assertion about memory the checker cannot see. Calling
that a checked cast misreports which of the two spellings is doing the work, and
it hides the single most consequential unchecked operation in the language from
the audit that exists to find it.

> **Reversible in one place** if this is judged wrong: `cast_class`'s pointer
> arm. Flagged rather than buried, because it contradicts a spec section.

### `tbb` casts are conversions, never bit operations (D-008 §6)

ERR is a **different bit pattern at every width**, so sign-extension and
truncation are both wrong: `tbb8`'s ERR is −128, which sign-extended into a
`tbb32` is an ordinary valid number. Every cast touching a `tbb` carries a
sentinel test the backend must emit.

**Narrowing a `tbb` is allowed, and that is not an exception to the narrowing
rule.** Out of range becomes ERR, exactly as the type's arithmetic saturates
(D-008 §3). The loss is not silent — it is a sticky taint the program is forced
to handle, which is the entire purpose of the type. Refusing it would force the
programmer to hand-write the range check that produces the same ERR.

`tbb => intN` and `tbb => flt` trap to `failsafe` on ERR — the controlled
shutdown path, never a silent reinterpretation. `intN => tbb` traps on the value
equal to the target's sentinel, which would otherwise **forge a taint**: a number
arriving from outside claiming to be an error state.

### Against the prototype

The prototype's checked cast emits a **warning** and adds a runtime check for
narrowing. `TYPE_REFERENCE` §5 supersedes that explicitly — *"not a runtime trap
and not a warning"* — so the spec is followed and the prototype is the lagging
artifact here. Recorded in `PROTOTYPE_DELTA.md`.

> **Amended at 1.0.9 — retyping a pointer is `=>!` in BOTH directions.** This
> decision settled `any-> => T->` as `=>!`-only and said nothing about
> erasure, `T-> => any->`; the implementation made every pointer-to-pointer
> cast `=>!`-only and the 1.0.8 audit asked whether erasure should be the
> checked form, since it asserts nothing. It stays `=>!`: an erased pointer's
> only use is to be retyped back, so the pair of casts is one assertion split
> across two sites, and keeping both under the bang keeps "where does this
> program retype a pointer" one grep — the property D-019 built `#wild_ptr`
> for. A conversion that loses nothing but still needs the bang is the class
> `cast_class` already has (a handle manufactured, a tag read), and the
> refusal's lead-in says so since 1.0.8.

---

## D-096 — `is_err` and `Result{…}` are keyword forms with nodes of their own

> ⚠️ **Corrected by D-097.** This decision originally covered `ok` as well and
> gave it a meaning it never had — a `Result` **constructor**. `ok` tested the
> user-writable `unknown` that D-007 removed, and it has now left the language.
> `is_err`'s operand was also recorded wrongly as a `Result`; it is a `tbb`.
> What survives here is the shape argument, which was right.

**Settled in cycle 0.4.4, by finding that neither could be written.**

Three documents require them:

- `TYPE_REFERENCE.md` §27 — an `unknown` taint "must be cleared via `ok(val)`".
- `OP_REFERENCE.md` §5 — "use `is_err(x)` to test without trapping".
- `LEXICAL_REFERENCE.md` — `ok` is a `ControlFlow` keyword, `is_err` a
  `BuiltinHelper`.

**Neither had an AST node, and being keywords they never lexed as identifiers, so
`ok(1i32)` was "expected an expression".** The language's only non-trapping
failure test could not be written down.

### They are `OkExpr` and `IsErrExpr`, not calls

Same shape as `MoveExpr` (D-065): a keyword operator with a parenthesised
operand. Three reasons they are not calls:

1. **They are keywords.** A call's callee is an identifier, and these never lex
   as one. Making them identifiers would have meant removing two keywords from a
   grammar that lists them deliberately.
2. **`is_err` carries a guarantee.** It is the one test in the language
   guaranteed not to trap — branching on a `tbb` ERR value otherwise does
   (`SAFETY_ARCHITECTURE.md`), so a program needs one way to ask "did this fail"
   that is safe on every value. 0.5's flow analysis and 0.7's lowering both have
   to *see* which construct that is, and recovering it from a callee's name is a
   string comparison standing in for a language rule.
3. **`ok` constructs.** Nothing else in the language builds a `Result` value, so
   it is closer to a literal than to a call.

### The comment that described an impossible path

`resolve.npk` carried: *"A bare-name builtin is declared in no module: `alloc`,
`string_concat`, `sys`, `ok`, `is_err`. They are ordinary calls the compiler
happens to provide, so they resolve to nothing and that is correct."*

Both halves were false for those two names — they are keywords, so they cannot
reach name resolution at all, and they are not in `builtins.npk` either. A
mechanical check settled the question in one pass: **cross the 36 bare-name
builtins against the keyword list.** The overlap is zero, so `ok` and `is_err`
were never in the set the comment placed them in.

That check is the same technique that found the seed-keyword drift in 0.2 and the
shape-table gap in 0.4.2: **two lists that had to agree, with nothing making
them.** The comment was the only thing asserting the relationship, and comments
do not fail.

### `Result<T>` did not resolve either

Found in the same subcycle and worth recording together. `Result` is a builtin
type name that takes generic arguments, so it matched nothing in the scalar
builtin table and fell through to the user-name lookup — where a **token kind was
reinterpreted as an intern index** and the diagnostic read "there is no type
named" against whatever string happened to sit at that index.

D-091 requires `Result<T>` to stay writable outside a return position, and
`Result<string>:r = read_file(p);` is how this compiler's own sources hold an
outcome before unwrapping. `builtin_generic_kind` and `builtin_is_generic` are
now generated beside the scalar table, and a constructor whose kind arrives at a
later rung — `Handle`, `arena`, `simd` — **names the rung** instead of claiming
the type does not exist (D-085).

---

## D-097 — `ok` leaves the language; `Result{…}` is the constructor

**Settled in cycle 0.4.4, correcting D-096.** Three findings, one root cause.

### 1. `ok` tested a feature that no longer exists

The prototype settles what `ok` was, in `tests/special/test_unknown_ok.npk`:

```nitpick
int32:x = unknown;
if (ok(x) != 0i32) { exit 2; }    // ok() is false for unknown
int32:y = 42i32;
if (ok(y) == 0i32) { exit 3; }    // true for known values
```

**`ok(x)` asked whether an ordinary value was `unknown`.** It has nothing to do
with `Result`. And **D-007 already removed user-writable `unknown`** — "it should
stay removed… `tbb` sticky ERR covers degraded computation better… keeping both
would leave two mechanisms for one job". What remained was a compiler-assigned
taint on `Result.value`, which `TYPE_REFERENCE.md` §27 says is cleared "via
`ok(val)` **or by checking `Result.is_error` first**".

So `ok` was an operator whose subject had been removed, offering a second route
to something `.is_error` already does. **It is removed from the language** — from
the `ControlFlow` keyword list, the AST, and the parser. `is_err` remains the
non-trapping test for `tbb`; `.is_error`, `?` and a `pick` with an `ERR:` arm
cover `Result`.

D-096 had instead invented `ok(x) → Result<T>`, a constructor, from that one
§27 sentence. Nothing in the language or the prototype ever worked that way.

### 2. `is_err` takes a `tbb`, not a `Result`

`OP_REFERENCE.md` §5 states the purpose plainly: *"Because `bool` has exactly two
values and cannot represent ERR, **comparing or branching on an ERR value traps to
`failsafe`**… Use `is_err(x)` to test without trapping."*

The operand is a `tbb`. Asking whether a `Result` failed is a different question
and `.is_error` answers it. D-096 recorded the operand as a `Result`; corrected.

### 3. `Result{…}` is the constructor, and it did not parse

`TYPE_REFERENCE.md` §11 has always carried the full table:

| Syntax | Desugars to |
|---|---|
| `pass(retVal);` | `return Result{err: 0tbb32, value: retVal};` |
| `fail(errCode);` | `return Result{err: errCode, value: zero};` |
| `return Result{err: e, value: v};` | (literal, no desugar) |

and `AST_REFERENCE.md` §2 restricts `ReturnStmt` to *"the literal `Result{…}`
form only"*. `<T>` is never written — it comes from the enclosing function's
declared success type, which is exactly what lets `pass` and `fail` be sugar
rather than primitives.

**It could not be written.** The struct-literal path is gated on
`TokenKind.Ident` and `Result` is `KwResult`, so the real parser answered
`NITPICK-PARSE-002` for the form every function in the language ultimately
returns.

`ResultLiteralExpr` has a **fixed shape** — exactly `value` and `error`, in
either written order, either one omissible — rather than reusing
`StructLiteralExpr`, whose window alternates *interned names* with values. A
keyword has no intern index, and putting a token kind where a name index belongs
is the same confusion that made `Result<int32>` report "there is no type named"
(D-096).

### The root cause, and the countermeasure

All three are one mistake: **giving semantics to a construct on the strength of a
passing mention, without establishing its shape.** `ok` came from half a sentence
in §27. `is_err` came from half a sentence in §5, read without the sentence
before it. `Result{…}` was never looked for at all — and the 0.4.4 notes then
asserted that nothing in the language constructs a `Result`, which the same
document had a table for.

**A passing mention is not a specification.** Before implementing a named
construct: find the grammar production, the defining table, or the prototype test
that fixes its shape — and if none exists, say so rather than filling the gap.
The invented version typechecks, passes its own tests, and is wrong.

The corollary that would have caught it in one step: **before claiming something
is the only way to do X, search for the other ways.**

### `is_error` stays derived

Confirmed rather than changed. The prototype stores it as an `i8` third field —
`{T value, void* error, i8 is_error}` — and D-069 deliberately departed: the
error becomes a `tbb32` and `is_error` is derived as `error != 0`. A stored flag
is a second source of truth about one fact, and two sources of truth eventually
disagree. The literal therefore has **two fields and no `is_error` to write**.

---

## D-098 — Auto-dereference is one level, and only one

**Settled in cycle 0.4.5.** D-006 made `.` the only member operator and gave it
automatic dereference, which left one question the plan flagged and nobody had
answered: what does `pp.x` mean when `pp` is `Point->->`?

**One level. `p.x` on a `Point->` works; `pp.x` on a `Point->->` is an error that
says to write `<-`.**

The alternative — peel until a struct appears — is what most languages with
auto-dereference do, and it is wrong here for the reason the notation exists.
Nitpick spells direction deliberately: `->` points *to* a target, `<-` brings a
value *back*, `@` takes an address. A rule that peeled an arbitrary number of
levels would make **the number of indirections invisible at the use site**, so
`pp.x` and `p.x` would read identically while doing different amounts of work,
and changing a declaration from `Point->` to `Point->->` would silently keep
every use compiling.

One level is also the level that is *not* a choice: a member access on a pointer
has exactly one sensible reading, and every level past the first is a decision
the writer should be making.

The rule applies identically to **UFCS**. `q.magnitude()` where `q` is a `Point->`
peels one level, because `q.x` does — a method call that behaved differently
would make `.` mean two things one character apart, which is the
context-dependence the blueprint philosophy exists to forbid.

`any->` has **no members at any level**: it is type-erased, so there is nothing
to reach into and `p =>! T->` comes first (D-095).

---


## D-099 — `NIL` is the empty `Optional`; `Some` and `Optional{…}` are both struck

**Settled in cycle 0.4.5.** `Result` gained its constructor in D-097 and the same
question was asked of `Optional`: **what builds one?** The first answer was wrong
in the specific way D-097 was written to prevent, and the record of the wrong turn
is kept here because the correction is the useful part.

### `Some` was never designed

The only trace of it in the entire specification set is one line of an IR comment
in `TYPE_REFERENCE.md` §11.1:

```llvm
; Some(42) = { i8 1, i32 42 }
```

No grammar production, no keyword, no AST node, and **no occurrence anywhere in
the prototype**. The user's response on being shown it: *"I do not know where
`Some()` even came from. I think that is the first time I have heard about it."*
It is struck.

### `Optional{…}` was the wrong replacement, and shipped nothing

Reasoning from symmetry with `Result{…}`, a literal form was drafted — a node
kind, a shared parser, a typing rule, and tests that passed. **It was an invention
too**, arrived at the same way `Some` was: by filling a gap rather than looking
for what already filled it.

**Nothing in the language was missing.** The prototype has the answer in two test
files whose names say what they are for. `tests/special/test_nil_vs_null.npk`,
header comment verbatim: *"NIL = optional-none (value type), NULL = null pointer
(reference type)."* And `tests/special/test_nil_optional.npk` uses it in every
position an expression can occupy:

```nitpick
func:get_none = int32?() { pass NIL; }        // returned
int32?:a = NIL;                                // empty
int32?:b = 42i32;                              // holding a value — no constructor
if (a != NIL) { exit 1; }                      // tested
int32:result = raw check_nil(NIL);             // passed as an argument
```

The countermeasure D-097 states — *find the grammar production, the defining
table, or the prototype test that fixes its shape* — was applied to `Some` and
not to the replacement. **Striking one invention is not the same as checking
whether the thing it replaced needed inventing.**

### `NULL` is no pointer; `NIL` is no value

This is the user's original design, stated directly, and it is a **pair**:

> *"My original design for NIL was as a complement to NULL where null means no
> pointer and nil means no value. Also, since all functions return `Result<T>` it
> serves as the return value for 'void' functions which don't return anything
> useful."*

Those two uses are **one meaning in two positions**, not two meanings sharing a
spelling — "no value" is what it says in both — which is what lets it survive the
blueprint rule that a construct means the same thing everywhere. Anything `NIL`
was made to do beyond them was added by an implementing agent and is not carried.

So an `Optional<T>` is:

| Written | Is |
|---|---|
| `int32?:a = NIL;` | empty |
| `int32?:b = 42i32;` | holding `42i32` |
| `a == NIL`, `a != NIL` | the test |
| `a ?? d` | the value, or `d` |
| `a?.f` | the field, still wrapped |

### The wrap is implicit, and that is the one exception in `fits`

`int32?:b = 42i32;` puts a bare `int32` where an `Optional<int32>` is expected.
Every "does this value fit that slot" question in the checker now goes through one
predicate, `fits`, whose rule is **equality plus that single wrap** — no implicit
widening (D-092), no numeric conversion, no pointer decay. One answer for a
declaration's initialiser, a call argument, `pass`, a `Result{…}` field, and an
unwrap's default, because a call that accepted what an initialiser refused would
be one more thing to remember.

`NIL` itself is not handled by `fits`. It reads its context directly, exactly as
`NULL` does, and a `NIL`-**typed value** — what `drop f()` yields — is deliberately
*not* accepted into an `Optional` slot: `drop` produces `NIL` precisely so that
using a discarded outcome as a value is an error.

### Only `NIL` was not reading its context

`NULL` has been contextual since 0.4.2. `NIL` typed as the unit value everywhere,
so `int32?:a = NIL;` — the spelling the prototype's own test file is built out of
— reported a mismatch against the very type it constructs. Making it contextual
surfaced a second defect of the same shape: the list of nodes that take their type
from where they sit held **only unsuffixed integers**, so `NIL == opt` and
`NULL == p` typed their left operand with no context and reported the sentinel as
homeless, while `opt == NIL` and `p == NULL` — the same question the other way
round — worked. The list is now named for what it answers.

The two sentinels still differ in one way, and it is forced: **`NULL` with no
context is an error and `NIL` is not.** `NULL` has no type of its own to fall back
to; `NIL` is the zero-sized unit value (D-084), and `pass NIL;` is the most common
statement in the language.

### An `Optional` has no readable members

The draft gave it `.value` and `.has_value`, mirroring the prototype's **IR field
names** — neither is a source-level member there. Both are struck.

`.has_value` duplicates `== NIL`, and two spellings for one question is how one of
them ends up wrong. `.value` is worse than redundant: reading it on an empty
`Optional` is the unchecked access the wrapper exists to prevent, offered as a
one-word shortcut past `??` and `?.`, which are the accessors that cannot be
wrong.

### `??` unwraps an `Optional` and nothing else

The prototype's checker also accepted a **pointer**, unwrapping it to its pointee
— `ptr ?? 99i32` meaning "the pointed-to value, or the default if null". Its own
test for that (`tests/null_coalesce_test.npk`) is a stub whose body is a comment
waiting on `<-`, so the behaviour was specified and **never once ran**.

It is not carried across, for two reasons. A `??` that dereferences hides the read
behind an operator whose job everywhere else is to unwrap a wrapper, and `<-`
exists precisely so that bringing a value back is visible in the source. And it
would give one operator two meanings chosen by operand type, which is the first
thing the blueprint philosophy forbids. Both halves remain writable: `p == NULL`
asks whether a pointer points anywhere, and `<-p` brings the value back. The
diagnostic names them, because somebody may arrive expecting the prototype's
behaviour.

### Two inner types are refused

Both name a type with a state nobody can write down, and the check lives in one
place because `T?` and `Optional<T>` are one type — a rule written at only one
spelling could be stepped around by using the other.

- **`NIL?`** — `NIL` is both the unit value and the empty `Optional`, so
  `NIL?:x = NIL;` is genuinely ambiguous: empty, or holding the one value `NIL`
  has? Both readings are defensible, which is what makes the spelling wrong rather
  than merely unusual.
- **`Optional<Optional<T>>`** — the inner absence is unreachable, since `x = NIL`
  sets the outer tag and nothing sets the inner one. The postfix form cannot even
  be lexed (`??` is one token, so `int32??` reads as `int32` followed by the
  null-coalesce operator), which leaves the generic spelling as the only way in.

**`?.` flattens for the same reason.** `op?.f` where `f` is itself an `Optional`
stays one deep — the two absences are one absence, and both mean "there is no
value here". Without that, safe navigation would manufacture behind the rule's
back the exact type the resolver refuses. The prototype hit the other side of
this and left a note about it: an `{ i1 true, {i1, T} }` whose outer tag says
present while the inner says absent, so a test reads the wrong flag and passes.

### What survives from `Result{…}` — the field rules

The shared-parser draft is withdrawn, but one thing it found is independently
right and is kept. `p_result_literal` matched the two names it knew and let
anything else **fall through untouched**, so `Result{value: v, is_error: true}`
parsed clean and silently dropped a field. A discarded field is a value the writer
believed they had set — the failure the fixed shape exists to prevent — and
`is_error` in particular is a spelling somebody arrives with, because the
prototype stored it as a third field before D-069 made it derived.

Three rules now, under `NITPICK-PARSE-008`, which earns its own code for the same
reason `NITPICK-PARSE-007` does: the diagnostic has to name what replaced
`is_error`, and a generic "unexpected token" cannot.

- An unrecognised name is refused, and `is_error` is told it is derived.
- A **repeated** field is refused rather than overwritten. Keeping the last one
  discards the first — the same lost value, differently spelled.
- The field name must be an **identifier before its payload is read as one**.
  `field.payload` is an intern index only on an `Ident`; on anything else it is
  that token's own payload, so `Result{5: v}` either named whatever string sat at
  index five or ran `intern_get` off the end of the table, which fails the parse
  through `relay` and produces no diagnostic at all.

The test asserting the old behaviour asserted **zero errors** for the dropped
field, recording the defect as correct. That is the second time in this cycle a
test has certified a silent discard, and both times the test was written in the
same commit as the code it was checking.

---

## D-100 — A variadic function is a different type kind, not a flag

**Settled in cycle 0.4.5.** `..*T[]` parsed, resolved and bound its name from
cycle 0.2 onward, and **nothing checked a call against it.** `fn_type` built a
plain `TY_FUNC` from the fixed parameters and dropped the variadic spec on the
floor, so `sys(1i64, 2i64, 3i64)` was checked against a one-parameter function
and reported as passing too many arguments. Every variadic call in the language
was either refused for the wrong reason or accepted by accident.

### The representation, and why it is not a boolean

**`TY_FUNC_VARIADIC` is a distinct kind.** The parameter window is the same shape
and its **last entry is the variadic parameter, typed as the slice `T[]` it is
actually bound to** inside the body (D-070). The element every trailing argument
is checked against is `type_elem` of that, so nothing extra is stored and the
fixed count is simply one less than the window length.

The reason for a kind rather than a flag beside `TY_FUNC` is the one
`AST_REFERENCE.md` already gives for `FailsOn` and `NeverFails` being separate
node kinds:

> a shared kind plus a boolean makes an unwritten contract and a `never fails`
> contract the same node until someone reads the right slot … an encoding that
> can be misread by ignoring a field undoes it.

That argument is not hypothetical here — **it is a description of the defect being
fixed.** A checker that forgets to read a variadic flag treats a variadic function
as fixed-arity, which is exactly what happened. With two kinds, every site that
switches on the kind has to account for it, and `type_is_func` exists so that no
site accidentally answers "that is not a function" about a perfectly good variadic
one.

The caveat that governs the opposite choice — *"two node kinds for one syntactic
form, discriminated by a fact the parser does not have, is a kind that can never
be built"* — does not apply. The resolver knows whether the declaration carried a
`..*` spec; it is written right there in the signature.

### Two arity rules, chosen by kind

| Callee | Arity | Trailing arguments |
|---|---|---|
| `TY_FUNC` | exactly the parameter count | — |
| `TY_FUNC_VARIADIC` | **at least** the fixed count | each checked against the collected **element** |

The diagnostic names both counts, and says "at least" when that is the rule — a
reader told "takes 2" about a function that accepts any number above one would go
and correct the wrong thing.

### `..*T[]` and not `..*T`

The brackets are the type. Written without them there is no slice to bind the
parameter to and no element to recover, so the call site and the body would
disagree about what `rest` is. It is refused where the signature is resolved, and
the invalid type is handed back rather than a guess, so the call checker declines
to check arguments against something that was never a slice.

### `..^` expands into the tail, and nowhere else

0.4.2 typed a spread as its operand's type and left the position rule to this
subcycle, *"because that is where an argument list exists to check it against"*.
Three rules, all of them about position rather than type:

- **The callee must collect something.** `..^` expanding into a function with no
  `..*T[]` parameter has nowhere to go.
- **It must be in the tail.** An expansion filling a fixed parameter is refused
  where it sits.
- **Nothing may follow it.** `..^` expands to however many elements the collection
  holds, so an argument written after one has no position anyone can name.

Its type is the **collector, not the element** — `..^coll` supplies the whole
slice, which is the one place a `T[]` rather than a `T` belongs in a variadic
argument list. That makes the pair exact: `..*` collects `T`s into a `T[]`, `..^`
expands a `T[]` back into `T`s, and D-026's claim that they are inverses is now
enforced rather than merely asserted.

### One accessor was named for the wrong thing

`variadic_elem` returned the **slice**, not the element — `..*T[]:rest` parses
`T[]` with the ordinary type parser, so the slot always held the whole thing. A
caller trusting the name would have checked every trailing argument against
`int64[]` instead of `int64`, accepting slices and refusing values, which is the
defect inverted. Renamed `variadic_type`, and the same correction is made in
`AST_REFERENCE.md`.

---

## D-101 — The statement walk is total, and assignment is checked

**Settled in cycle 0.4.5.** `check_stmt` answered **nine of the twenty-five**
statement kinds and returned quietly for the other sixteen — including **every
kind that contains statements**. An `if` had its condition checked and its body
ignored. Nothing inside any block, loop, `pick` arm or `defer` was type-checked at
all.

### The test suite reported the hole as a convention

`tests/frontend/type_stmt.npk` unrolled a function body's top level by hand,
under a comment reading *"Nested blocks are not descended into here — every case
below puts its statement at the top level of the body."* That was true of the
**checker**, not of the helper, and writing it as a property of the harness is how
sixteen unvisited kinds stayed invisible through two subcycles.

**A test that works around a hole in the thing it tests turns a defect into a
house style.** The helper is now one call to `check_stmt` on the body, which is a
block, and every case that nests is checked.

### Total, in the way this project has answered twice already

The same failure as `expr_shape` walking slot `a` on the strength of a comment,
and as the resolve walk that `resolve_audit` was built to police. Third pass,
third instance, same answer:

- Every kind is reached **explicitly**.
- The kinds that carry nothing — `break`, `continue`, `fall`, which hold a label —
  say so **out loud** rather than being reached by falling off the end. *"Nothing
  to check"* and *"nobody has looked"* are the two answers the function exists to
  keep apart.
- Where the walk used to return quietly there is now `NITPICK-TYPE-011`, an
  **internal** diagnostic: a reader who sees it has found a hole in the compiler
  and there is nothing in their source to change.
- `stmt_is_classified` is a total table, and the suite walks `1..STMT_KIND_MAX`
  against it.

The audit and the block walk were each **verified by making them fail**.
Un-classifying one kind trips the audit; not walking a block breaks the *first*
declaration test, because a function body is itself a block — with blocks
unwalked, nothing was checked at all.

### `resolve_stmt`'s catch-all is the same shape of risk

It ends with an unguarded `resolve_expr(stmt_expr(s))` for "everything left",
which **silently absorbs any kind added later** and reads slot `a` as an
expression whether or not it holds one. The checker's version is a named list,
`stmt_carries_expr`, so a new kind falls into the internal diagnostic rather than
into a wrong read.

### `till` has no condition, and slot `a` is its body

`check_stmt` called `check_cond` on a `StmtTillStmt`. On a counted statement slot
`a` is the **body**, so this handed a `StmtId` to a function expecting an `ExprId`
and typed whatever expression node happened to sit at that index. `stmt_cond` and
`stmt_counted_body` both read slot `a` and mean different things by it — **the
third slot-means-two-things defect in this compiler.** `loop` and `till` bound
themselves with head arguments; there is no condition in either form.

### Assignment

Parsed since cycle 0.2, resolved since 0.3, never checked. Three questions, each
unasked, so `int32:x = 1i32; x = true;` passed clean:

1. **Is the target somewhere to store a value?** A name, a field, an element, or a
   `<-` dereference. `5i32 = x;` parses. The diagnostic names `==`, because the
   usual cause is a comparison written with one `=` and the reader needs pointing
   at that rather than at a definition.
2. **Does the value fit?** The same `fits` every other slot uses, so an `int32?`
   target takes a bare `int32` here exactly as a declaration does (D-099). A rule
   that differed between an assignment and an initialiser would be one more thing
   to remember.
3. **For `op=`, do the operands support the operation?** `x op= v` is `x = x op v`,
   so it is held to the operator's own rule by **calling that rule** —
   `type_arithmetic` and `type_bitwise`, which now take a `Span` rather than an
   `ExprNode` since the node was only ever read for its span. Restating the rules
   would let `+=` refuse what `+` accepts, a difference nobody would predict.

---

## D-102 — An `impl` method is found through the type, not through the scope

**Settled in cycle 0.4.6.** Cycle 0.3.1 left a comment on `collect_decl`:

> An `impl` block declares nothing at module scope. Its methods are reached
> through the type, which needs the type to exist first — so they are collected in
> cycle 0.4 where coherence is decided, not here.

Nothing was built to reach them, and `type_method_call` resolves a method by
**ordinary scope lookup**. So UFCS found module-level free functions and nothing
else: **a method written where the specification puts it — inside `impl:Point` —
could not be called at all.**

### The 0.4.5 tests passed, and meant less than they looked like they meant

The UFCS fixture declared `func:magnitude = int32(Point:self)` at module scope,
which is a perfectly good UFCS case and is what D-006 is about. It is also the one
form `TRAITS_REFERENCE.md` §2.4 does *not* use — that section's example is
`impl:Point = { func:magnitude = … };`, and the very next sentence calls inherent
dispatch "an independent confirmation that UFCS is part of the language".

The lesson is not that the tests were wrong. It is that **a feature can be tested
through the one spelling that avoids the gap**, and the way to catch that is to
write the test from the specification's example rather than from a convenient one.

### The impl table

Built once per module, holding `(target, trait, decl)` per `impl` block. The
target and trait are **resolved at collection**, so two impls on the same type
compare as one integer and coherence does not depend on how the annotation was
spelled (D-090).

It is a **parameter to `etyper_init`**, not a field set afterwards. A caller that
forgets it would silently get the old scope-only behaviour back, which is the
failure this decision exists to end; a context with no impls passes an empty table
and says so in a comment.

### Lookup order: inherent, then traits, and ambiguity is an error

**Inherent wins**, and that is not a tie-break invented here — §2.6 settles the
same question for blanket impls, *"concrete impls take priority"*, and an inherent
method is the most concrete thing there is.

**Two traits supplying one name is an error naming both.** The specification gives
no rule to choose by, and any rule invented here would be a silent choice between
two things somebody wrote deliberately. The caller disambiguates by writing the
free-function form, which UFCS guarantees is available.

**A free function is still a method.** The impl table is the first place to look,
not the only one; D-006 makes `p.magnitude()` and `magnitude(p)` the same call.

### Coherence (§4.1), and what is not subject to it

At most one implementation of a trait for a type. Both impls are named and the
diagnostic carries the second one's span with the first one's line — "conflicting
implementations" with one location makes the reader search for the other, which is
the one thing the compiler already knows.

**Two inherent impls are not a coherence error.** Splitting a type's methods
across two blocks is a formatting choice. What would be wrong is the same *method*
twice, which is a duplicate-name question about a declaration rather than a
coherence question about a trait.

### The second segment is a trait and nothing else

`impl:Point:Wrap` resolves perfectly well — `Wrap` is a type — and means nothing.
The diagnostic says **which segment** is wrong and restates D-031's ordering,
because the two segments are one character apart and a reader who got the order
backwards needs to be told the order, not that a name was unusable.

---

## D-103 — What a trait requires of an impl, and what `dyn` requires of a trait

**Settled in cycle 0.4.6.** Three checks that all need the same thing — a trait's
declared surface compared against an impl's — plus the object-safety rules that
decide whether a trait can be a `dyn` at all.

### `Self` needed a type before any of this could be checked

`TypeResolver.self_type` existed from 0.4.1 and **was never set**, so `Self`
resolved to an error everywhere it appeared. That was invisible while nothing
compared signatures, because a trait method's body is not checked and its
signature was not resolved.

Inside a **trait body** `Self` is `TY_SELF`, a placeholder: the implementing type
does not exist until an impl names one, and a trait's declared signature still has
to *be* something to compare against. Inside an **impl body** it is the target
type. The two are told apart by what the resolver was given, and `self_type` is
saved and restored around each resolution — leaking it would make the next impl's
`Self` mean this impl's target.

### An impl is checked three ways, and each is a different mistake

| Wrong | Fix |
|---|---|
| a **required** member is missing | supply it |
| a member the trait **never declared** | move it to an inherent `impl:Type` |
| a member whose **signature disagrees** | match the declaration |

A trait member is required or defaulted, and the AST already says which: a method
with **no body**, an `assoc` with **no bound type**. Both absences are real rather
than a flag, which is what lets them be told apart without one.

**The third check is the one that would otherwise pass.** An impl supplying
`func:area = int32(Point:self)` where the trait declares `flt64(Self:self)` has
every method the trait asked for and satisfies nothing — a caller holding a `dyn`
would dispatch through a vtable slot typed one way into a body typed another.
Checking names without checking shapes is checking the label.

Comparison substitutes `Self` for the target, **one level**: `Self` is a whole
type where it appears, and `Self->` or `Self[]` would need the substitution to
rebuild the wrapper. Those are refused rather than silently mismatched, since a
trait method taking `Self->` is a shape somebody meant.

### Object safety names which of the three rules broke

1. **Every method takes `self`** — a vtable dispatches on a receiver.
2. **No method returns `Self`** — its size is unknown once the type is erased.
3. **No method has comptime parameters** — those monomorphize, and a vtable slot
   is one address.

Verified against the prototype (`type_checker_stmts.cpp`, TRAIT-022), which
implements the same three. **Two departures, both stricter:**

- It accepts a parameter named `self` **anywhere** in the list. Here it must be
  **first**, because UFCS binds the receiver to parameter zero — a `self` in
  position two would be dispatched with the receiver in position zero.
- It refuses comptime **type** parameters and says nothing about value ones. Both
  are refused here: `<comptime int32:LEVEL>` monomorphizes exactly as a type
  parameter does, and the message names which kind it found.

A non-object-safe trait is still a perfectly good trait — **only `dyn` is
refused**, because static dispatch has the concrete type and needs no vtable.

### `dyn` assignment, which §5.2's own example did not typecheck

`dyn Serializable:obj = msg;` is the specification's example and there was no rule
admitting it. A `dyn` slot now accepts a concrete type that implements **every**
trait it names, and another `dyn` that **carries at least** them — §5.3's widening
by dropping bounds. The reverse would invent a vtable that was never built.

This is not a coercion in the sense D-099's `fits` otherwise rules out. Nothing
about the value changes; it is a fat pointer either way. It is a question about
what was implemented, and the impl table is the only thing that can answer it.

---

## D-104 — A type name is not a token kind, and the node says which

**Settled in cycle 0.4.6.** `resolve_named` asked `builtin_type_spec` about
**every** payload, on the strength of a comment reading *"the two are told apart
by which range the payload is in."*

**Nothing separated the ranges.**

- A **builtin type keyword** carries no intern index, so the parser stored the
  `TokenKind` as the name. Those are **80–175**.
- An **identifier** carries its intern index. Those start at **1** and count up.

So a program's **88th distinct identifier, used as a type name, resolved to
`int32`** — silently, with no diagnostic, producing a different type from the one
written. The 129th became `bool`, the 133rd `string`.

### How it was found, and why it took this long

A two-trait `dyn` in a long test file reported **both** of its perfectly good
traits as "not a trait". The traits were real, declared two lines above. They had
simply been interned late enough to land in the keyword range, so they resolved to
builtin scalars.

It survived because nothing had resolved a user type name from a large intern
table before. Each test builds its own AST but shares one `InternTable` across
cases, and the cases that used user types ran early, while the ones that ran late
used builtins. **`tests/grammar/` never resolves anything** — it is parse-only by
design — so the corpus that exercises the whole language could not have caught it.

### This is D-096's defect at its root

D-096 fixed `Result<int32>`, where the same reinterpretation ran the other way: a
keyword read as an intern index, reporting "there is no type named" against
whatever string sat there. That decision fixed the symptom for one construct and
**left the representation that allowed it**. The same confusion was still present
in two places, reaching opposite wrong answers.

**One bit ends it.** `TypeNamedType` records which kind of name it holds, and both
readers consult the node rather than guessing from the payload's value. The
second reader was `check_not_result_return`, which compared an intern index
against `KwResult` and would report a user type named at that index as a `Result`
return.

### The regression test forces the collision rather than hoping for it

A fresh intern table is padded so the next new identifier lands on **exactly 87**,
`KwInt32`. A struct declared there is then assigned to an `int32` local: before
the fix that typechecked, which is the whole shape of the bug — not a diagnostic,
an agreement. Verified by reverting the fix, with the earlier dyn cases neutered
so execution reaches it.

---

## D-105 — Supertraits are enforced, and a blanket impl parses

**Settled in cycle 0.4.6.** Two features that had been written down, stored in the
AST, and never read.

### Supertraits parsed since 0.2 and nothing consumed them

`trait:Ordered = Equatable & { … };` recorded its supertraits in the item node's
extras window from cycle 0.2, and **no pass ever looked at it.** So a type could
implement a trait while satisfying none of what that trait was declared to build
on — which is the entire content of §2.2 — and a trait could name a **struct** as
a supertrait with nothing objecting.

Both are checked now, and **transitively**: `C` requiring `B` requiring `A` needs
all three. The requirement is on the **implementing type**, not on the
intermediate trait, so the walk carries the impl down rather than restarting.

**The walk is fuel-bounded at 64.** Supertraits are a graph a *program* writes, so
`trait:A = B & {…}; trait:B = A & {…};` is a cycle somebody can type and a
recursive walk over it does not return. The cycle is a real error belonging to
trait-declaration checking; what matters here is that the checker cannot hang
before reporting anything, and the cap is stated rather than assumed unreachable.

### A blanket impl did not parse, which is a D-085 violation

`impl:<T: Printable>:Loggable = { … };` is §2.6's form and answered **"expected a
type"** — the target position called `p_parse_type`, and `<` is not a type.

That is not a missing feature, it is the rule the bootstrap strategy rests on
being broken: **the frontend accepts the whole grammar from day one**, and a
construct no rung can lower yet produces a *backend* diagnostic, never a parse
error. The rule exists so the grammar is never partial and never has to be
re-widened — which is the failure that killed `nitpick-bootstrap`.

**The generic list stands in the target position.** `impl:<T: Printable>` writes
no type there at all; the parameter it declares *is* what is being implemented on.
So slot 0 is empty for a blanket impl and the generics window says which type it
means, which makes `impl_generic_count > 0` the test for "is this blanket" with no
separate flag needed. `resolve.npk` opens a scope for the block when it declares
one, and only then — a concrete `impl:Point` opens nothing, since a block that
opened a scope either way would put its methods where a later lookup does not
reach.

**Recorded, and not checked, at this rung.** Every question about a blanket impl —
which types it applies to, whether a concrete impl outranks it (§2.6), what `Self`
substitutes to — is bound satisfaction or monomorphization, and both are 0.4.7's.
Recording them now is what lets 0.4.7 find them. It is also why a blanket impl is
**not** a coherence conflict with a concrete one: overlapping is the point, and
§2.6 already says which wins.

### The corpus that should have caught it did not contain it

`tests/grammar/whole_grammar.npk` exists so every construct in the language is fed
to the real parser on every harness run. **The blanket impl form was absent**, so
nothing checked the parser against it. It is there now.

That is the second time in this subcycle that a gap survived because the thing
meant to catch it did not cover the case — the other being the 0.4.5 UFCS tests
declaring their method at module scope (D-102). Both point the same way: **a
corpus is only as good as its worst-covered construct**, and the way to keep it
honest is to add each form from the specification's own example rather than from
whatever was convenient to write.

---

## D-106 — A type's inherent methods are one namespace

**Settled in cycle 0.4.6, closing it.** D-102 made two inherent `impl` blocks on
one type legal — splitting a type's methods is a formatting choice, and coherence
(§4.1) governs *traits*. That left the duplicate-**name** question with nothing
answering it.

`find_method` returned the first match, so the second `scaled` was **unreachable**:
a method somebody wrote, that nothing could ever call, with no diagnostic saying
so. Dead code the compiler knew about and did not mention.

### Two namespaces, and the difference is not cosmetic

- **A type's inherent methods are one namespace across every inherent block.**
  `scaled` in two of them is one name declared twice, and is refused.
- **Each trait impl is its own.** `Point` implementing both `Ta::tag` and
  `Tb::tag` is legal — the trait qualifies the name, and refusing the
  *declarations* would refuse two traits that never meet. D-102 already reports
  the ambiguity **at the call**, which is where a reader can act on it.

An inherent method alongside a trait one is likewise fine: inherent wins at the
call (D-102) rather than colliding at the declaration.

Inside **one** block a repeat is always wrong, whichever kind of block it is.

The diagnostic carries the second declaration's span and names the first one's
line — the same shape the coherence message uses, and for the same reason: a
reader told only that something is duplicated has to go and find the other one.

---

# Cycle 0.4.6 closed

Traits and impls, in seven decisions' worth of work across four commits. **Four of
the seven plan items turned out to be repairs rather than construction**, which is
the part worth carrying forward:

| Found | Had been true since |
|---|---|
| a method in an `impl` could not be called at all (D-102) | cycle 0.3 |
| supertraits parsed and nothing read them (D-105) | cycle 0.2 |
| a blanket impl did not parse (D-105) | cycle 0.2 |
| a type name could resolve to `int32` (D-104) | cycle 0.4.1 |

The last of those is the one that mattered most and had nothing to do with traits.
It was found because a two-trait `dyn` reported both of its perfectly good traits
as "not a trait", and it would have silently mistyped any program with more than
about ninety identifiers.

**Three of the four survived because the thing meant to catch them did not cover
the case** — the 0.4.5 UFCS tests declaring their method at module scope, the
grammar corpus lacking the blanket impl form, and `tests/grammar/` never resolving
anything at all. The standing lesson for 0.4.7 and 0.4.8: **write each test from
the specification's own example**, not from whatever is convenient, and treat a
construct missing from `whole_grammar.npk` as a construct nothing checks.

---

## D-107 — A generic parameter's capabilities are its bounds', and UFCS does not reach past them

**Settled in cycle 0.4.7.** D-064 §1 says a generic body may use exactly what its
bounds declare and nothing else. Three questions that rule leaves open all have to
be answered before "no duck typing" is a property rather than a slogan.

### 1. A bound set is transitively closed

`T: Ordered` where `trait:Ordered = Equatable & { … }` guarantees `Equatable` too:
§2.2 makes a supertrait a requirement on the *implementing type*, so any type
that satisfies the bound satisfies the supertrait as well. A body may therefore
use `Equatable`'s methods, and the closure is computed **once**, when bounds are
collected, rather than by a graph walk at every lookup.

The closure **must not repeat a trait**. `T: Equatable & Ordered` reaches
`Equatable` by two routes, and two copies of one bound make every one of its
methods report as "supplied by both `Equatable` and `Equatable`" — a correct
program refused, with a message naming one trait twice. The walk is fuel-bounded
at 64 for the reason `SUPER_FUEL` already is: a supertrait cycle is something a
program can express.

### 2. UFCS does not reach a free function through a parameter

D-006 makes `p.magnitude()` and `magnitude(p)` the same call, so a method lookup
that misses the type falls back to module scope. **That fallback does not apply
to a generic parameter.**

A free function taking a `T` is not a capability `T` declares — it is one the
surrounding module happens to contain. Reaching it would be duck typing arrived at
by omission, and it would fail in the worst possible way: the body would compile,
and break at an instantiation with a type that has no such method, reported
against a caller who supplied a perfectly reasonable type. That is precisely the
C++ template diagnostic experience D-064 rejected.

The diagnostic therefore **names the bounds**. "`T` has no method `render`" leaves
two candidate fixes — change the call, or widen the bound — and the compiler
already knows which are possible, so it says which traits `T` does declare.

### 3. A `comptime` value parameter is not a type

`<comptime int32:LEVEL>` introduces a compile-time **constant**, and `LEVEL:x` is
as wrong as `MAX:x` would be. It is refused by name where a type belongs rather
than becoming an opaque type that fails further along with a worse message. The
`comptime` marker is exactly what makes the two kinds of parameter readable apart
(D-064 §2), so a checker that ignored it would give the marker nothing to do.

### 4. A parameter shadows a module-level type of the same name

It is the inner binding, so `struct:T` at module scope and `<T>` on a function are
two different types and the body's `T` is the parameter. Consequently the
parameter is looked up **first**, before the scope.

### Where a parameter is resolved from, and why it is not a scope

Every other name in the language resolves through the symbol table. A generic
parameter resolves from the **declaration that introduced it**, carried as a
window of parameter declarations plus an optional window of arguments.

The reason is that a parameter is not reliably in a scope. `resolve.npk` declares
a *function's* parameters into the scope it opens, but the type checker walks a
body holding the **module** scope; a *struct's* are declared into no scope at all,
and a struct's field types are resolved on demand from wherever the access
happened to be written. Carrying the window means the answer travels with the
declaration instead of depending on which scope a caller passed.

The same structure carries both halves of D-064, which is why it is one mechanism
and not two: **unbound** is the definition, where `T` is opaque and the body is
checked once; **bound** is an instantiation, where `T` is the argument and the
body is not re-examined. `T` means the same thing in both, and only the binding
differs.

---

## D-108 — An instantiation's identity is its mangled name, and its bounds are judged in a pass

**Settled in cycle 0.4.7.** D-064 §6 requires instantiations to be deduplicated
and reversibly mangled with no hash. Making that work against an interned,
nominally-typed table (D-090) settles three things.

### 1. The name is the identity; the argument window is a derivation of it

`tt_intern` compares every slot, and an argument **window** cannot be compared
that way — two spellings of `Container<int32>` commit two windows at two different
starts, so field equality would make one instantiation two types. `type_eq` is an
integer compare everywhere else in the compiler, so the two would then silently
differ in every comparison downstream.

The **mangled name** identifies them instead. It is sound to key on it because the
name is a *total function* of the declaration and the arguments: two entries
carrying one name necessarily hold equal arguments, so the window can be read back
for substitution without ever being compared.

The mangling is the **source spelling** — `Container<int32>`, `Handle<Container<int32>>`
— which is the most reversible form there is and doubles as the display name, so
every diagnostic about an instantiation says which one without anything extra.
A mangled name contains `<`, which no identifier does, so it cannot collide with a
plain named type.

> **Amended at 1.0.7: the identity is the declaration and the argument
> CONTENTS; the name is a derivation.** The "total function" claim fails
> exactly when an argument is a generic PARAMETER: a parameter renders by its
> name, and two declarations may each call theirs `T`. `func:first<T> =
> T(List<T>:l)` and `func:second<T> = T(List<T>:l)` both spelled `List<T>`,
> so the second's was the first's instance — its argument window held the
> first function's parameter — and `l.items[0]` in the second reported
> "expected `T`, found `T`". Found writing the first generic collection
> (1.0.7); every earlier program had one generic function per generic struct,
> or different parameter names. `tt_instance` now compares the window's
> contents element-wise (each argument is itself an interned id, so that is
> canonical), which is what the paragraph above said could not be compared —
> the START cannot, the CONTENTS can. Nothing else in this section changes: a
> concrete instantiation's name is still unique, still the source spelling,
> still hash-free, and still what D-156's symbols are built from. Only a
> template-shaped instance can share its name with another, and those are
> never emitted (1.0.7 skips them at the instance loop and substitutes them
> transitively — `pair_of<T>` calling `list_make::<T>` records `list_make<int64>`
> while `pair_of<int64>` is emitted).
>
> **The same defect one table away, fixed the same day:** a `dyn`'s identity
> was its bound WINDOW's start, so two spellings of `dyn Speaks` were two
> types with two layout entries — masked while `Pair<dyn Speaks>` deduped by
> name, exposed the moment it deduped by contents (`llc`: "redefinition of
> type"). `tt_dyn` compares contents too, and interns the spelling
> (`dyn Speaks & Walks`, canonical order) as the type's name, so
> `type_display` renders it and `Pair<dyn Speaks>` mangles reversibly — it
> was `Pair<<type>>`, one symbol for every `Pair<dyn …>` a program could
> write.

### 2. Bound satisfaction is a pass; arity is decided at the annotation

`resolve_type` runs during **collection** — `collect_impls` resolves an impl's
target, `collect_bounds` resolves a bound — and the impl table is still being
filled while it does. "Does `Point` implement `Printable`" would answer `false` for
an impl recorded on the very next line, so a check written at the annotation would
report correct programs as wrong.

So the resolver **records** every instantiation, with the span it was first written
at, and a later pass **decides**. Neither half has a mode: recording always
happens, and the check always runs after collection.

**Arity stays at the annotation**, because counting needs nothing but what was
written. One record per distinct instantiation, so a bound violation is one mistake
reported once, at a real site rather than at every site.

A parameter satisfies a bound by **declaring** it, not by having an impl —
`Container<T>` inside `func:f<T: Printable>` forwards a type that does not exist
yet, and everything known about it is its bound list. A `dyn` is deliberately not a
third case: `dyn Printable` is a type-erased value, not a type implementing
`Printable`, and admitting it would put a fat pointer where a monomorphized body
expects a value.

### 3. Two limits, one of them binding

`p_looks_like_decl` scanned at most **64 tokens** to tell `Container<int32>:c;`
from a chain of comparisons, while D-064 caps instantiation at **64 levels** — and
one level costs three tokens. A declaration could therefore sit well inside the
type system's limit, outrun the scan, and be silently reparsed as an expression.
That is D-104's failure one layer up: not a rejection, an agreement about
something else.

The scan budget is now **512**, above the deepest type the checker will accept, so
the depth cap is the binding limit and reports when it binds. Exhausting even 512
is a diagnostic naming the reason rather than a `false` that reads as an answer.
Two limits where only one is reachable is one limit; two where both are is a
program whose refusal depends on which fires first.

---

## D-109 — A `comptime` value argument: where it ends, and what it is

**Settled in cycle 0.4.7.** D-064 §2 introduced value parameters and gave
`Mutex<Config, 2>` as the example. **That example did not parse**: a type-argument
list took a type per entry and `2` is not one, so the form D-056's lock levels are
built on answered "expected a type". It was also absent from `whole_grammar.npk`,
which is why nothing caught it — the same gap that hid the blanket impl (D-105).

### Where a value argument ends

**It stops below the binary operators.** Inside `<…>` a `>` *closes the list*, so
an expression parser that could take `>` as a comparison would read
`Mutex<Config, 2>` as `Mutex < Config, (2 > …)` and consume the closing bracket as
an operator. No lookahead fixes that; only a rule about where the value ends does.

So a value argument is a literal, a name, a unary expression, or a **parenthesized**
one — `Mutex<Config, (A > B)>` if a comparison is genuinely meant. Parentheses are
how the language already resolves this everywhere else, and the alternative is a
`>` whose meaning depends on what precedes it.

**One function parses all three argument lists** — a named type's, the turbofish,
and a `#`-builtin's. A value accepted in one and refused in the others would make
the same list mean different things depending on what opened it. A builtin that
takes no values says so in the checker, where the message can name the builtin.

### Which kind an argument is, is decided by the parameter

`Mutex<Config, LEVEL>` cannot be told apart at the parser, so **an identifier is
always parsed as a type** and the declaration decides. The parser records what was
*written*; the same division `T[]` versus `T[N]` already follows.

Supplying the wrong kind **says which kind was wanted**. "There is no type named
`2`" would be true and useless — the mistake is about the position.

### What a value argument is, in the type table

A **`TY_COMPTIME` entry**: slot `a` is the value's type, and the name is its
rendering. It is not a type and nothing can declare one; it exists because a
generic argument list is positional and holds both kinds, so every entry has to be
one index. A mixture only the parameter list could tell apart is an encoding that
can be misread by ignoring a field — the mistake `TY_FUNC_VARIADIC` (D-100) exists
to avoid.

**Identity is the rendering**, so `2` and `2` are one argument and `2` and `3` are
two — and therefore `Mutex<T, 2>` and `Mutex<T, 3>` are two types, which is what
D-056 exists to keep apart. It also renders inside the instantiation's mangled
name (D-108), so `Mutex<Config, 2>` is what diagnostics print.

### What the value may be

**Only an integer literal**, which is the same rule and the same sentence
`resolve_fixed_array` uses for `int32[N]`. General constant folding is `comptime`'s
job in cycle 0.6, and until then a non-literal is refused **by name** rather than
silently taking a wrong value — 0.2.8 found this exact path taking an intern-table
slot as an array length.

**An unsuffixed literal takes the parameter's declared type** (D-092's context rule
applied to the one context a generic argument has); **a suffixed one must already
be that type.** `Mutex<Config, 2i64>` against `comptime int32:LEVEL` is a mismatch
somebody wrote, not a width to be adjusted.

In a **body**, a value parameter reads as a value of its declared type — that is
the entire content of `comptime int32:LEVEL`, and the reason a value parameter
carries a type where a type parameter carries bounds. In a **type position** it is
refused by name (D-107 §3).

---

## D-110 — Inference at a call, and what an unsolved parameter does

**Settled in cycle 0.4.7.** `TRAITS_REFERENCE.md` §3.3 says type arguments are
inferred at the overwhelming majority of call sites and nothing is written. The
generic arguments were **parsed, stored, and skipped**: `type_call` stepped over
them, so `extract::<int32>(c)` checked as though the turbofish were not there and
`extract(c)` returned `T` rather than the type it was called with.

### Only an identifier callee can be generic

A function **value** cannot be. Monomorphization produces one body per
instantiation and a code address is one body (D-018, D-087), so a callee that is
any other expression has no type parameters by construction. Nothing is inferred
there because there is nothing to infer.

### Explicit first, then inference, then substitution

1. **The turbofish is not inferred.** It is the only expression-position form
   (D-064 §3), and what it supplies decides even where inference could have.
2. **Inference is structural unification** of each declared parameter type against
   the actual argument type, reaching through the shapes a parameter can appear
   in — `T`, `T->`, `T[]`, `T[N]`, `T?`, `Result<T>`, and `Container<T>` against
   `Container<int32>`. A rule that matched only bare parameters would make the
   *shape* of a signature decide whether inference works.
3. **The signature is substituted and then checked like any other.** The body is
   never re-checked; it was checked once at its definition (D-064 §1).

**Unification learns nothing where the shapes disagree, and reports nothing.** A
genuine mismatch belongs to the argument check, against the *substituted*
signature, where the message can name both types. Complaining during inference
would name `T` and be useless.

**First writing wins.** `f(a, b)` where both parameters are `T` and the arguments
differ leaves `T` as the first, and the second argument is then reported as not
fitting — the mismatch, at the argument that caused it, rather than an inference
failure the reader cannot locate.

### An unsuffixed literal cannot solve a parameter

It takes its type **from** the parameter it fills (D-092), so it cannot also
supply it. Inference therefore skips every argument that takes its type from
context, and the honest message when nothing else solves `T` is that `T` could not
be inferred. The consequence is visible to programmers and is stated rather than
hidden: `identity(5)` needs `identity::<int32>(5)`; `identity(x)` does not.

### An unsolved parameter ends the check

Carrying on would check the call against a signature still mentioning `T` — a
different signature from the one being called — so every argument and the return
would be reported as not fitting a type the programmer never wrote. One cause,
three diagnostics, two of them misleading. The call yields the invalid type
instead, which is the discipline `resolve_type` already follows.

**Nothing is left untyped by stopping.** Inference has already typed every
argument that has a type of its own, and the ones it skipped are precisely the
ones with nothing to be checked against.

### A non-generic function given type arguments is refused

They were silently ignored, so `plain::<int32>(n)` checked as `plain(n)` and the
turbofish meant nothing at all.

### Bounds are checked at a call by the same function as at an annotation

A call and a type annotation refusing the same thing for different reasons would
be two rules. `Container<Point>` written down and `render(p)` inferring `T = Point`
reach the same check and the same sentence.

### A generic trait is instantiated like a generic struct

`p_parse_trait` calls `p_parse_generics`, so `trait:Into<T>` parses — and its
arguments were dropped, making `Into<int32>` and `Into<string>` one trait. The same
hole as the struct one (D-108), one declaration kind over, and it closes the same
way.

### Identical diagnostics are deduplicated

Same code, same span, same message is the same finding by construction: two
different findings never agree on all three, because the code says which rule and
the span says where.

This is not tidying, and it is what makes the above affordable. The checker
legitimately resolves one annotation more than once — a struct's field types are
resolved at every access, and inference must type an argument to solve `T` before
it can supply that argument's expected type — and each of those paths is *correct*
to report what it finds. Without deduplication, being thorough would mean being
noisy, and both alternatives are worse: a resolver that reports on some paths and
not others is a rule that depends on which path ran, and a checker that visits less
to stay quiet is checking less.

**A diagnostic must therefore carry the span of the thing that is wrong.** An
argument's mismatch belongs at the *argument*, not at the call. Reported at the
call, `vsum(1i32, 2i32, 3i32)` produced the same code, span and message twice, and
deduplication folded one away — so a reader was told about one of two mistakes.
Two findings that are only identical because the span is too coarse are a defect
in the span, not an argument against deduplication.

---

## D-111 — A blanket impl's target is its parameter, and two of them for one trait conflict

**Settled in cycle 0.4.7, closing it.** D-105 made `impl:<T: Printable>:Loggable`
parse and recorded it with a **target of zero**, deferring every question to this
subcycle. A zero is not a type, so every check that reads a target skipped it: its
methods were unreachable, it satisfied no bound, and it collided with nothing.

### The target is the parameter type

That single change pays the whole schedule off. `Self` inside the block then
resolves to the parameter through the substitution D-103 already built, and
completeness, supertraits and duplicate-name checking all run on a blanket impl
**unchanged** rather than being skipped. Nothing else had to learn what a blanket
impl is.

### Two blanket impls of one trait conflict, full stop

The overlap question looks as though it needs bound satisfaction and does not.
The language has **no negative bounds**, so nothing stops a type implementing both
`A` and `B` — `impl:<T: A>:Loggable` and `impl:<T: B>:Loggable` genuinely overlap
wherever a type satisfies both. With no specialization (D-064 §7) there is no rule
to choose by, so choosing either would be a silent choice between two things a
programmer wrote on purpose. Both are named, the shape the concrete conflict
already uses.

A blanket impl beside a **concrete** one is *not* a conflict (D-105): overlapping
is the point, and §2.6 says the concrete one wins.

### Concrete priority is the order the question is asked

`find_method` asks the impl table first and the blanket impls only if nothing
concrete answered. Expressing §2.6 as an *order* rather than a tie-break means
there is never a moment at which two candidates exist and something has to choose.
Among themselves, blanket impls are ordinary trait candidates: two supplying one
name is the ambiguity two concrete trait impls would be.

### `type_implements` is one question with three sources

It was two functions with the third missing. A type implements a trait by having a
**concrete impl**, by being a **parameter that declares it** (D-107), or by
satisfying the bound of a **blanket impl**. Fuel-bounded, because a blanket impl's
bound may itself be satisfied by another blanket impl and a program can write that
chain. **A blanket impl does not apply to itself** — letting it satisfy the bound
that admits it would make `impl:<T: Loggable>:Loggable` true of everything by
circular reasoning.

### An inherent blanket impl is refused

`impl:<T: Bound> = { … };` parses, because the grammar is never partial (D-085),
and would add methods to every type satisfying a bound — which is adding methods to
types the writer does not own. §2.6 gives the form with a trait and only with one.

### Two bindings a blanket impl needs

- **A generic trait's parameters are bound by the impl.** `impl:Point:Into<int32>`
  means `T = int32` in everything `Into` declares, so the trait's
  `func:into = T(Self:self)` is `int32(Point:self)` here. Without it the
  comparison is `Result<T>` against `Result<int32>` and *every* impl of a generic
  trait is reported as having the wrong signature.
- **A blanket method's parameter is bound to the receiver at the call.**
  `func:l = int32(T:self)` called on a `Point` has `T = Point`; without saying so,
  `T` resolves to "there is no type named `T`" at a call site nowhere near the
  declaration. Only the first parameter is bound, because only the first is the
  target.

---

## D-112 — A layout is written in place, and the table is completed for the whole program

**Settled in cycle 0.4.8.** Every struct in every program was **size zero**, and
had been since types were interned.

### Why it could not have worked

`tt_intern` keys on kind, the three operand slots and the name, and **does not
compare size** — deliberately, because a type's identity is what it *is* and not
how wide it happens to be. So `struct_layout` computed a size, called `tt_struct`
with it, and got back the original zero-sized entry. It could not work, which is
why nothing called it: the function existed, was correct on its own terms, and had
no effect.

The consequence was invisible by construction. `Result<Point>` was four bytes
where it should be sixteen; `Point[4]` was nothing at all. **A wrong size does not
fail** — it corrupts memory later, which is the failure `TYPE_REFERENCE.md` and
`types.npk` both open by warning about.

### The layout is written in place

`tt_set_layout` is the **one mutation of an interned type**, and it is a
*completion* rather than a change: the entry goes from absent to known, once, and
every id already handed out keeps meaning the same type.

**Size zero means "not yet computed"**, and that reading is safe rather than
convenient — a laid-out struct is at least one byte, because two distinct empty
structs must have distinct addresses. `NIL` is genuinely zero-sized (D-084) and is
not a struct.

### Demand-driven, recursive, and no ordering pass

A struct containing a struct needs the inner one's real size. That wants either an
ordering pass or a recursive walk, and the walk is correct **because the loader
already refused the cycles**: a by-value containment cycle is a D-086 error, so the
containment graph is acyclic before layout ever runs. Fuel bounds it anyway —
"acyclic because something else checked" is exactly the assumption worth bounding,
since being wrong costs a compiler that does not terminate.

**A pointer and a slice are not followed**, and that is why the walk terminates at
all: a pointer is one word whatever it points at (D-038) and a slice is
`{ptr, len}` whatever it holds (D-070). It is the same fact that makes `Node->`
break a containment cycle where `Node` does not.

### The table is completed, not left to what was asked

The checker asks for very few sizes — it compares types and almost never needs a
width — so demand-driven layout alone leaves most of the table at zero. That is
fine for checking and not fine for what follows, so `finish_layouts` sweeps every
entry once the program is known. It is order-independent by construction, since
`ensure_layout` recurses into what a type is built from before computing the type
itself, and it re-reads the table's count each iteration because laying a struct
out resolves field types and resolving a type can intern new ones.

### Sizes are asserted against the machine, never against a number

`tests/frontend/type_layout.npk` compares every size to `#size_of` of the same
shape. A hand-written size that disagrees with what LLVM lays out survives testing
and corrupts memory later, and the seed's layouts and this table's have to agree
until the bootstrap fixpoint closes because their output interoperates.

---

# Cycle 0.4.7 closed

Generics, in five decisions (D-107 … D-111). **Five of the eight plan items turned
out to be repairs rather than construction**, and every one of them dates to the
cycle that *parsed* the construct:

| Found | Had been true since |
|---|---|
| `Container<int32>` and `Container<string>` were one type | cycle 0.2 |
| a generic struct's field types could not resolve at all | cycle 0.2 |
| `Mutex<Config, 2>` did not parse | cycle 0.2 |
| a generic call ignored its type arguments entirely | cycle 0.2 |
| a generic trait dropped its arguments | cycle 0.2 |

The pattern is exact and worth naming, because it will recur: **cycle 0.2 recorded
the whole grammar faithfully, and wherever nothing downstream read what it
recorded, the construct silently meant something else.** D-085's rule — the
frontend accepts the whole grammar from day one — is what makes the parser
trustworthy, and by itself it makes the *checker's* silence invisible. A construct
that parses is not a construct that works, and the only thing that tells them apart
is a test written from the specification's own example.

Two further repairs came from outside the plan. `p_looks_like_decl` scanned at most
64 tokens to tell a declaration from an expression while D-064 caps instantiation
at 64 *levels*, so a declaration inside the type system's limit could silently
reparse as a chain of comparisons — D-104's failure one layer up. And an argument
mismatch reported at the *call's* span, which identical-diagnostic deduplication
then correctly folded away, hiding the second of two bad arguments.

**Three findings had no code behind them** — "no such method", "ambiguous", "wrong
kind of generic argument" — so those distinctions lived only in the wording and
nothing could assert on them. `BUILD_REFERENCE.md` §7.1's convention is what
surfaced that: tests assert on codes and spans, never on message text, and a
distinction with no code is a distinction nothing can check.

## D-113 — A dynamically dispatched method declares its acquisition level as a contract clause

**Settled in cycle 0.5.** D-056 left a spelling unwritten, and the frontend is
built once, so it could not be left to whoever implemented the analysis.

### The gap

Lock-level analysis is whole-program: the acquisition set of a function follows
transitively from the call graph. **Dynamic dispatch is the hole** — a call through
a trait object can reach anything, so its acquisition set is unbounded. D-056's
answer:

> a dynamically dispatched call **declares its maximum acquisition level** as part
> of the trait method's contract, and implementations are checked against it. An
> undeclared method may not acquire at all.

`VERIFICATION_REFERENCE.md` §248 restates the requirement. **Neither gives the
syntax**, and an analysis cannot consume a declaration nobody can write.

### The spelling: a third contract kind

```nitpick
trait:Storage = {
    func:commit = NIL(Self:self) acquires <= 3i32;
};
```

`acquires <= N` sits beside `requires` and `ensures` in the contract window a
function already carries, adding a `VerifyKind` and **no new syntax shape**.

Four reasons it belongs there rather than anywhere else:

- **It is an obligation on the signature**, which is exactly what the contract
  window holds. A maximum acquisition level constrains what a caller may already
  be holding, in the same sense a `requires` clause does.
- **An absent clause reads correctly.** "An undeclared method may not acquire at
  all" is the natural meaning of no clause, rather than a special case someone has
  to remember — the same shape that makes a trait method with no body a real
  absence rather than a flag (D-103).
- **It composes with verification already.** `requires` and `ensures` are
  discharged by Z3 through the Design-by-Contract machinery, and the positive-return
  rule on `failsafe` is already implemented as a compiler-injected `ensures`. A
  third clause needs no new pipeline.
- **A `#` builtin or an attribute would both be new shapes** for a thing the
  language already has a place for, and D-020's sigil rule exists to mark
  *compiler intrinsics*, which this is not.

### What it is checked against

An implementation of the method may acquire no level above `N`, transitively. A
call through `dyn Storage` therefore contributes exactly `N` to the caller's
possibly-held set, which is what makes the whole-program analysis terminate at a
trait object instead of giving up or silently under-approximating.

**`N` is a compile-time constant**, the same kind `Mutex<T, LEVEL>` carries, so the
comparison is available wherever the call is (D-064 §2, D-109).

---

# Cycle 0.4 closed — the type system

Nine subcycles, and the largest cycle in Phase A. `TYPE_REFERENCE.md` and
`TRAITS_REFERENCE.md` are implemented in full, and `tools/check.npk` validates a
whole program and emits nothing — which is what "at the end of Phase A the
artifact is a checker" means in practice.

## What the cycle actually spent its time on

**Most of the work was repair.** Four of 0.4.6's seven plan items and five of
0.4.7's eight turned out to be fixes rather than construction, and 0.4.8 closed
three more gaps that predated it. That is not a comment on the earlier cycles'
quality; it is a structural consequence of D-085 and it is worth naming so the
later cycles expect it:

> The frontend accepts the whole grammar from day one. That is what makes the
> parser trustworthy, and **by itself it makes the checker's silence invisible.**

Every one of 0.4.7's five repairs dated to cycle **0.2** — the cycle that *parsed*
the construct. 0.2 recorded the source faithfully; nothing downstream read what it
recorded, so the construct silently meant something else. A construct that parses
is not a construct that works, and the only thing that tells them apart is a test
written from the specification's own example.

## The three failure shapes this cycle kept finding

- **A slot that means two things.** `expr_shape`, `stmt_cond`, the type name that
  could be a token kind (D-104), the argument window that holds two kinds of
  index. Each cost a defect, and each was fixed by naming the reading rather than
  commenting it.
- **A check that could not fire.** `struct_layout` computed a size and re-interned
  it, which returns the existing entry (D-112). A blanket impl was recorded with a
  target of zero, so every check that reads a target skipped it (D-111). Both
  existed, were correct on their own terms, and had no effect.
- **A distinction that lived only in the wording.** Three findings shared a code
  with something else, so no test could tell them apart — which is what
  `BUILD_REFERENCE.md` §7.1's codes-and-spans convention surfaced.

## What was verified by making it fail

Fourteen halves across 0.4.6–0.4.8, each broken deliberately and the exit code
recorded in the commit that fixed it. That habit is the cycle's most transferable
practice: a test that has never failed is a test whose failure mode is unknown.

## Carried into 0.5

Nothing. The three driver-completeness gaps 0.4.7 recorded — a declaration nothing
reaches, an `impl` method body, a struct's layout — were all closed in 0.4.8.

---

## D-114 — An `extern` function has a type, and it is a `Result<T>` like every other — **SETTLED (its open sub-item is queued)**

**Settled in cycle 0.5.1**, after every call to an `extern` function turned out to
be a type error.

### The gap

`extern:"libc" = { … };` parsed from cycle 0.2 and bound its names from cycle 0.3.
**Nothing in cycle 0.4 gave those names a type.** `type_ident` had a case for
`DeclFunctionDecl`, for a parameter, for a global and for a generic parameter, and
none for `DeclExternFn` — so the name fell through to the ending that reports "`X`
is a type, not a value", and every foreign call was refused with a sentence about
the wrong thing.

This is the same shape cycle 0.4 kept finding and the roadmap predicted would
recur: a construct that parses is not a construct that works, and D-085 makes the
checker's silence invisible because there is nothing to see.

### The decision

An `extern` function's type is `func Result<T>(P…)` — **the declared return type
wrapped exactly as a Nitpick function's is** (D-013).

D-002 already settles the semantics: `extern` declarations "auto-wrap into the
correct `Result<T>` and populate the error field when the call fails". What was
missing was only the construction. Three consequences follow and all three are
wanted:

- **A caller writes `relay` or `?|` on a foreign call, exactly as on
  a domestic one.** The blueprint philosophy's first facet in its purest form: a
  caller never has to ask whether *this particular* function needs error handling,
  and "is it foreign?" would be precisely such a question.
- **`never fails` does not change the shape of the type.** It is an audited claim
  about the contract, not a second calling convention. A `never fails` function
  still hands back a `Result` that is always `Ok`, because the alternative is two
  shapes of foreign call to remember.
- **The failure contract is not consumed by the checker.** `fails on … with errno`
  is a declaration the *emitter* reads to synthesise the wrap at the call boundary
  (cycle 0.7). What the frontend owes is the type a caller sees.

### An `extern` function's node is not a function's node

`DeclExternFn` has a **three-slot header** — return type, failure contract,
parameter count — where `DeclFunctionDecl` has seven, because an extern
declaration has no body, no generics, no contracts and no attributes.

The two layouts are not interchangeable and the accessors are therefore separate:
`fn_param_count` reads slot 4, which on an extern function is the *second
parameter*. Pointing the `fn_*` accessors at an extern node would be the same
defect that typed a `till` body as an expression and resolved a numeric width
suffix as a name — one slot read as two different things.

### What is deliberately refused: the C variadic tail

`func:printf = int32(int8->:fmt, ..*) fails on result < 0i32;` is **refused**, with
its own code (`NITPICK-TYPE-023`), because the language has not settled what a C
vararg's type is.

A Nitpick variadic writes `..*T[]:rest`, and `check_args` checks every trailing
argument against `T`. A C `..*` names nothing. The obvious filler is `any[]`, and
the same type system refuses a bare `any` on purpose (`NITPICK-TYPE-003`), so
smuggling one in through a variadic collector would be the type system
contradicting itself.

**Refusing loudly is not the same as leaving it alone.** These calls were already
refused before this decision — as "`printf` is a type, not a value" — so this
replaces a misleading refusal with an accurate one and makes the open question
visible. It is recorded as open rather than parked: the tail's element type is a
language-surface decision. (This paragraph once named a pre-Phase-B deadline;
Phase B is well underway with `extern` refusing at the 1.1 rung, so the true
deadline is the FFI work there — the sub-item rides OPEN_DECISIONS' 1.1 group,
annotated by the 0.9.8 doc-sync.)

---

## D-115 — Every walk over a module's members descends into a nested `mod`

**Settled in cycle 0.5.1**, after a nested module turned out to be invisible to
three separate passes.

### What was found

`mod:name = { … };` nests. The collector opens a scope for it and `symtab_set_inner`
records which scope the name opened. From there, three walks disagreed with the
collector about whether the module existed at all:

| Walk | Behaviour | Consequence |
|---|---|---|
| `check_decl` (type checker) | no case for the kind | a function one level down was **never type-checked** |
| `collect_impls` | no case for the kind | an `impl` one level down was **absent from the impl table** |
| `collect_bounds` | no case for the kind | a bound one level down was **never collected** |

The type-checker hole was demonstrated directly: `pass "not an int32";` from an
`int32` function is reported at the top level and **accepted, silently, inside a
`mod` block**. The impl hole is worse than a missing diagnostic — an absent entry
is coherence never seeing a second implementation, completeness never checking the
methods, and a method call finding nothing to dispatch to.

None of the three announced itself, because **a walk with no entry for a kind
reports nothing by construction**.

### The second defect, in the resolver

The resolver *did* descend, using a helper that **searched the scope table for the
first module scope whose parent matched**. With one nested module that is right by
luck. With two it is the same answer twice: the second module's members resolved
in the **first one's scope**, so a call between siblings in the second module
failed as an unknown name and *swapping the two declarations fixed it*.

Its own comment said "recorded rather than recomputed would be better" — and it
already **was** recorded, on the symbol, by `symtab_set_inner`. The search was a
second computation of a fact the table already held, and it disagreed with the
first as soon as a file had two nested modules.

### The decision

**One function answers where a nested module's members live**
(`nested_module_scope`), reading the symbol, and every walk over a module's
members calls it.

Not three copies of six lines: the helper it replaced is the argument against
that. Three copies of a lookup are three chances for one of them to answer
differently, and an analysis whose result depends on declaration order is exactly
what the recorded scope was added to prevent.


---

## D-116 — A binding-state analysis marks to a fixpoint before it reports

**Settled in cycle 0.5.1**, after the escape analysis was written with this bug
and the cycle's own plan document described it in advance.

### The rule

An analysis that carries **per-binding state** runs in two phases:

1. **Marking**, repeated until nothing changes. Nothing is reported.
2. **Reporting**, once.

### Why a single pass in source order is wrong

```nitpick
while (i < n) {
    if (i > 0i32) { pass p; }   // checked here, on a binding not yet marked
    p = @x;                     // marked here
}
```

`pass p` is visited before `p = @x`, so the binding is unmarked when the escape is
checked and the escape is missed. On the second iteration `p` holds a pointer into
the frame and it is returned. **The failure is silent, and silence is what makes
it dangerous** — a rule that under-reports looks exactly like a program with no
violations.

Verified by reverting to one pass: nine findings where there are ten.

This is the shape `meta/roadmap/0.5/README.md` names as the cycle's third failure
mode — "an analysis that is right on straight-line code and wrong after a merge" —
and it arrived first in the analysis itself rather than in a program it checks.

### What makes the fixpoint terminate

**A mark is never cleared.** Every round either sets a mark that was unset or
changes nothing, and there are finitely many statements, so the sequence is
monotone and bounded.

That is also a deliberate loss of precision: `p = @x; p = NULL;` leaves `p`
marked, and a program that is in fact fine is refused. **Conservative is the
direction a safety analysis is allowed to be wrong in**, and the alternative — a
lattice with joins at every merge point — is the complexity D-004 chose
second-class borrows to avoid in the first place.

### The round bound, and what exceeding it means

The loop is bounded, because **an unbounded loop in a checker is not a checker**.
Exceeding the bound **refuses the program** with an explicit "the analysis did not
settle" diagnostic — it does not fall through to reporting whatever it had. An
analysis that quietly reports partial results is the under-reporting failure
above, reached by a different road.

That diagnostic gets **its own code**, not the one for a compiler defect, even
though a defect is the likeliest cause when it fires. The two are bounded by
different things — a walk with no entry for a node kind, versus a chain of
assignments longer than the rounds allow — and a shared code makes them one
finding to anything reading a build log.

### Generalisation

This applies to every later analysis in cycle 0.5 that carries binding state —
definite assignment (0.5.2), moved-from bindings (0.5.3), `unknown` taint (0.5.5).
Each has the same shape and each will have the same bug if written as one pass.


---

## D-117 — A borrow does not need a second-class parameter; the caller tracks what comes back — **rule B's connection model amended by D-223 (derivation-aware `can_connect`; the one-borrow exemption keys on self-connectability)**

**Settled in cycle 0.5.1**, and it settles it the opposite way to the
recommendation that was made first. What decided it was checking the
recommendation against this compiler's own source.

### The hole that had to be closed

D-004's five rules are all about the frame a borrow was taken in, and this program
walks straight through them:

```nitpick
func:launder = int32->(int32->:q) { pass q; };
func:caller  = int32->() { int32:x = 5i32; pass (raw launder(@x)); };
```

`caller` returns a pointer into its own dead frame. Rule 2 never sees a borrow,
because a call's result was not one. **The escape rules were defeated by one
function call.**

### The rejected fix: make a parameter second-class

`AST_REFERENCE.md` names `borrow_imm` / `borrow_mut` and D-004 cites them as
existing machinery, so the obvious move is to put them on parameters and let rule
2 refuse `pass q;` inside `launder`.

**It does not survive contact with this compiler.** Every context struct is built
from pointer parameters and handed back:

```nitpick
func:ctx_init = Ctx(int32->:a, int32->:b) { pass Ctx{ a: a, b: b }; };
```

`resolver_init`, `etyper_init`, `tyres_init`, `graph_init` and `escape_init` are
all exactly this. Once a parameter is a borrow binding, that `pass` is a rule 2
violation and **the compiler stops compiling itself** — while being perfectly
safe, because those pointers are the *caller's* and the struct goes straight back
to that caller.

### Why D-004 was right and the diagnosis was wrong

The reason "passing down needs no annotation" holds is sharper than it first
looks:

> **A pointer parameter never points into the callee's own frame.**

It points into the caller's, or somewhere older. So a callee cannot create an
escape out of a pointer it received, and `pass q;` inside `launder` is returning
the caller's pointer to the caller — which is correct code. The escape is one
level up, and that is where the fix belongs.

### The decision: two caller-side rules, and no new syntax

**Rule A — a call's result is a borrow if an argument was one and the result type
can carry a pointer.** That closes `launder`. The type test is what keeps it
usable: `sum_through(@x, @y)` returns an `int32`, carries nothing, and stays
ordinary code.

**Rule B — two or more borrow arguments may have been connected to each other.**

```nitpick
func:store_through = NIL(Cell->:h, int32->:q) { h.slot = q; pass NIL; };
drop store_through(@c, @x);       // `c` now points at `x`, and nothing was returned
```

Rule A cannot see this: the result is `NIL`. Neither can the callee's own rules,
correctly — both arguments are the caller's, and writing one into the other is the
caller's business. So the caller assumes the connection was made and marks each
borrow argument whose *pointee* can hold a pointer.

**One borrow argument is exempt, and that exemption is load-bearing.** With
nothing else passed in there is no second borrow to store, and `tt_intern(@t, …)`
— take the address of a local, call a helper, keep the local — is this compiler's
other universal idiom. Removing the exemption refuses `typetable_init`.

### What decides "can carry a pointer"

Pointers, slices, strings, `cstring`, `dyn` and `any` carry one outright. The
wrappers carry what they wrap, which is why an `int32[4]` carries nothing and an
`int32[]` does. The scalars carry nothing. A **function value carries nothing** —
it is a code address, and with closures removed (D-018) there is no environment
beside it.

A **struct and an enum answer `true` without being looked into.** Their field
types live in the AST as unresolved `TypeId`s, so answering properly needs a
resolver and the declaring scope, neither of which an analysis pass carries. The
cost is a false refusal on a struct of scalars; the alternative is threading a
resolver through every analysis in this cycle to answer a question whose wrong
answer is safe.

### What `borrow_imm` / `borrow_mut` are still for

**Not escape.** They may still be wanted for the other half of D-004 — the
aliasing rules that make `$$i` many-readers and `$$m` exclusive, and that Z3
discharges for disjointness. That is a different question with a different
answer, and it is not settled here.


---

## D-118 — Definite assignment carries two sets, and `$$m` is what fills a binding

**Settled in cycle 0.5.2**, implementing the analysis D-010 rests on.

### Why this analysis is load-bearing rather than a nicety

D-010 chose **no implicit default value** over poisoning every scalar with `ERR`,
and the argument was that a default makes an uninitialised read *defined* whereas
definite assignment makes it *impossible*. The second half of that sentence is a
promise about this analysis. If it is wrong, `int32:x;` followed by a read of `x`
compiles into a load of whatever the stack happened to hold — which is the class of
bug the language exists to make unrepresentable.

### Two sets, merging in opposite directions

| Set | Meaning | Merge | Answers |
|---|---|---|---|
| `must` | assigned on **every** path reaching here | intersection | may this be read? |
| `may` | assigned on **at least one** path | union | is this the second write to a `fixed`? |

**One set cannot answer both.** `if (c) { x = 1i32; }` leaves `x`
readable-nowhere *and* assignable-never-again, and a single set has to pick which
of the two questions it gets right.

### An arm that exits contributes nothing to the merge

`if (c) { pass 0i32; } else { x = 2i32; }` reaches the merge only through the
second arm, so intersecting with the first would forget that `x` was assigned.
That is not a refinement — it is the difference between accepting correct code and
refusing it, and removing the exemption refuses two cases in the acceptance suite.

The predicate is therefore **wrong towards `false` by design**: a statement wrongly
called an exit drops its arm from the merge and lets the other arm's assignments
stand alone, which is unsound. Everything not explicitly listed falls through.

### `$$m` assigns; `$$i` and `@` read

D-004 lists **"pass `$$m` of the destination downward"** as one of the four
mechanisms that replaced `gc` for returning references. So an unassigned binding
handed to a callee as `$$m` is *being filled*, and refusing it would refuse the
pattern the decision endorses.

`$$i` is a **shared** borrow, and shared means readable — the callee may read what
nothing wrote. `@` is plain address-of, with nothing in it that says which way the
data will travel, so it reads too.

That leaves `$$m` as the explicit, greppable way to say "the callee will write
this", which is the shape every other opt-out in the language has.

### A `defer` is checked where it was registered

Not where it runs. A deferred body naming a binding assigned later in the block
would, on any path that leaves early, read something nothing wrote. Its own
assignments do not survive it, because the code after a `defer` runs *before* the
deferred body does.

### `fixed` and `const` are one rule with two spellings

**Assigned once, where it is declared.** `fixed` is the body-scoped spelling and
`const` the module-scoped one, and they share a diagnostic code because a reader
filtering a log is asking the same question either way.

A global has no enclosing body, so its declaration is the only place an assignment
could appear — which makes the `const` rule two halves: the initialiser has to be
there, and it has to be **constant**. That second half reuses `NITPICK-TYPE-004`,
the code an array size and a `comptime` argument already use, because it is the
same question at the same rung with the same answer: folding a general constant
expression is cycle 0.6's job, so a literal is what "constant" means today.

### Where it is deliberately imprecise

- **A loop body may run zero times**, so nothing it assigns survives it. That is
  the correct answer, not an approximation.
- **A `pick` is exhaustive only if it has an unguarded wildcard arm.** A `pick`
  over every variant of an enum is exhaustive too, and proving that is 0.5.4's
  subject; until then this under-approximates, which costs a refusal on correct
  code and never the other way round.
- **A field assignment does not initialise its aggregate.** `Holder:h;` followed by
  `h.inner = NULL;` is refused, because with no implicit default the other fields
  are still unestablished, and treating field-by-field filling as initialisation is
  how a partially-initialised aggregate gets read as a whole one.
- **`when`'s `then` and `end` contribute nothing to `must`.** Which one runs is not
  decided here, and `when` is rare enough that buying the precision is paying for
  something nobody has asked for.


---

## D-119 — A free is a move, and an operand that needs an address is one question

**Settled in cycle 0.5.3**, implementing D-065 and closing a gap the seed had been
covering for.

### A deallocator takes ownership, which is what `move` does

D-065 says `move` is "the mechanism that prevents double-free on `wild`
allocations", and the implementation follows that literally: **`dalloc(p)`
invalidates `p` exactly as `move(p)` does.** Ownership of the allocation has left
the binding either way, and what remains is a name with nothing valid behind it.

Consequences, none of which needed new machinery:

- **Use-after-free is use-after-move**, reached through a different operator.
- **Double-free is use-after-move**, where the second use happens to be another
  free.
- **Reinitialisation revives both.** D-065 says a moved-from binding "may be
  reinitialized by assignment, after which it is live again"; a freed one is the
  same, because the same assignment establishes the same thing.
- **`ralloc` frees too.** A reallocation invalidates the pointer it was given and
  hands back a different one, so holding the old one is the same dangling pointer.

Two codes rather than one, because D-065 requires both sentences — the prototype's
negative test asks for *"use after move"* **and** *"cannot free moved variable"* —
and a reader who sees the second has a dangling pointer rather than a transferred
value.

### The four sets merge in three directions

`must` intersects; `may`, `moved` and `freed` union. Written side by side in one
function, because the asymmetry is the thing to get wrong:

**`must` is the odd one out, and it is the only one where the conservative answer
is the smaller set.** For the other three, being wrong towards `false` lets a
use-after-move through; for `must` it refuses correct code. A shared merge helper
taking one direction would be right about one of the four and silently wrong about
three.

The same split governs what survives a loop. **What a body assigns does not survive
it — what a body moves does**, because a body that moves something moves it again
on the second iteration.

### One analysis, not two

Assignment, immutability, moves and frees are all per-binding state threaded
through the same statements and merged at the same branches. They live in one walk
(`analysis/bindings.npk`) because two walks would be two sets of merge points that
have to agree about what an `if` does, and this compiler has paid for that shape
often enough to stop choosing it.

### An operand that needs an address

`@`, `$$i`, `$$m`, `move(place)` and an assignment target all require a **place** —
a name, a field or element of one, or what a pointer points at. There is no way to
spell the address of a temporary.

**Four of the five were unchecked entirely, and the fifth had a hole.** The seed
refuses these out of necessity — it has to emit an address and there is none — so
the two compilers disagreed about what a valid program is, exactly as they did over
`fixed` before 0.5.2. The assignment target's check looked at the **outermost
operator only**, so `f().x = 5i32;` passed as "a member access" — a store into a
field of a value with no home.

**One predicate answers it for all five**, and it walks to the **root** of the place
rather than testing the top node. `<-p` is a place, which is the one people get
backwards: dereferencing yields what the pointer points at, which is somewhere real.

Its own diagnostic code, not `TYPE_MISMATCH`: nothing is wrong with the operand's
*type*, and telling somebody their `int32` is not an `int32` sends them looking in
the wrong direction.

### `nodrop` requires `wild` or `wildx`

The qualifier suppresses the drop of a manual allocation, which is the one case
where *not* dropping is correct rather than a leak. On a managed binding there is
nothing to suppress, and the qualifier then says something untrue about the binding
it is written on.

This is the last of the four qualifiers cycle 0.5 inherited as "parses and nothing
reads it": `stack` and `wild` got their meaning from the escape rules in 0.5.1,
`fixed` from assignment in 0.5.2, and `nodrop` had nowhere to attach until there
was an ownership analysis to attach it to.

### `malloc` and `free` are not builtins, and ownership is not decided by spelling

**They are C functions**, reachable only by declaring them in an `extern` block
like any other C function. Verified against the prototype rather than inferred:
`type_checker_call.cpp` handles exactly `alloc`, `calloc`, `ralloc` and `dalloc`,
`dalloc`'s own comment calls it "preferred alias for `free()` per the NitpickAlloc
API", and every `malloc` in `FULL_specs.txt` and in the prototype's parser tests
appears inside `extern "libc" { … }`.

`BUILTIN_REFERENCE.md` claimed `free` and `realloc` were "preserved as legacy
aliases". **That line was wrong** — carried over from the era when the prototype
was C/C++-backed and `malloc`/`free` were what there was. The native allocator that
replaced them uses the four names above, and the aliases were never part of it. The
line is removed.

**An `extern` deallocator is opaque, and that is not a gap.** Past the FFI barrier
the runtime cannot intercept a fault, and the compiler cannot know what a callee did
with a pointer either. A program that frees through `extern` has suspended this
guarantee explicitly, which is how every other Nitpick guarantee is suspended.

**The prototype decided this by matching the spelling, and that part is not carried
over.** Its `KNOWN_DEALLOCATORS` set held `close`, `release`, `destroy`, `cleanup`,
`dispose` and `drop`, and it additionally matched any function name *ending* in
`_free`, `_close`, `_destroy` or `_release`. So `window_close` acquired ownership
semantics it never declared and `deallocate_thing` did not. That is behaviour that
varies by what something happens to be called — the context-dependence the blueprint
philosophy exists to refuse, and a list a reader would have to memorise. **This
analysis knows two builtin names and nothing else about names.**

### Also found and deliberately not fixed

**D-065's own example does not compile.** It is written with a variable named
`buffer`, and `buffer` is a keyword. The test uses a valid spelling and says why.


---

## D-120 — Coverage is one fact, answered once, and a wildcard never absorbs ERR

**Settled in cycle 0.5.4**, implementing D-008 §5.1, D-059 and D-061.

### The exception is the rule's reason for existing

`pick` must be exhaustive — ordinary enough. **A `tbb` selector additionally
requires an explicit `ERR:` arm, and `(*)` may not stand in for it**, and that is
the part carrying the safety property.

ERR is precisely the case a wildcard swallows. A `pick` that handles "anything
else" has, by writing four characters, decided that a value the type says may be
meaningless is fine to act on — a tainted value steering a branch, which is what
`tbb`'s sticky ERR exists to prevent.

So the two are **separate diagnostic codes**, not one. A `pick` may have `(*)` and
still be missing its `ERR:` arm; reporting that as "not exhaustive" would send the
author to add the wildcard they already wrote. And a `tbb` `pick` with only `ERR:`
gets both findings, because neither substitutes for the other.

### A guarded arm covers nothing

`where (…)` makes an arm conditional, so it runs only sometimes and proves nothing
about the case it names. Without this, **the guard becomes a second way to elide an
arm** — the door D-061 closed by removing `(!)`, reopened with different syntax.

The converse matters too and is tested: a guarded arm sitting beside an unguarded
one for the same variant is fine. **The rule is that a guard proves nothing, not
that it poisons the case.**

A guarded `ERR:` arm **cannot currently be written** — `p_parse_arm` gives `ERR:`
no guard slot. The check exists anyway, for the reason D-061 exists: the arm most
likely to be elided is that one, and a rule that depends on the grammar continuing
to forbid something stops holding when the grammar changes.

### Three codes where one would do, and why

| Situation | Fix the reader needs |
|---|---|
| an enum variant or a `bool` value uncovered | **add an arm** — and the message names which |
| a selector whose values cannot be listed | **add `(*)`** |
| a `tbb` with no `ERR:` arm | **add `ERR:`**, and `(*)` will not do |

A reader told "not exhaustive" about an `int32` goes looking for the variant they
forgot. The fixes differ, so the codes differ.

**The message names the missing cases.** "This `pick` is not exhaustive" makes the
reader enumerate the type themselves, which is the one thing the compiler already
did. Past a handful of names the list stops being readable, so it names six and
then counts the rest.

### Coverage is answered once and consumed twice

0.5.2's definite assignment needs to know whether some arm always runs — "a binding
assigned on every arm of an exhaustive `pick` is assigned after it". Until this
subcycle it could only recognise a wildcard, so a `pick` covering every variant of
an enum **under-approximated and refused correct code**. That was recorded as a
debt owed to 0.5.4, and it is paid by asking rather than re-deriving.

**The predicate takes no `DiagList` and therefore cannot report.** That is the
point of its parameter list rather than an accident of it: two analyses looking at
one `pick` must not both be able to speak about it, or one construct produces two
findings a reader cannot tell apart. The arm classifiers take an `Ast` and nothing
else for the same reason — the reporting-free path is reporting-free **by
construction**.

Where both *should* speak, they do. A partial `pick` draws `ASSIGN-001` at the read
and `PICK-001` at the `pick`, each naming its own objection.

**A wildcard satisfies the assignment question even for a `tbb`**, and that is not
a contradiction with D-008. The question there is whether an arm runs, and the
wildcard does run; it is refused for *acting on ERR*, which is a different
objection with its own code. Conflating them would make a missing `ERR:` arm look
like a definite-assignment problem.


---

## D-121 — The taint is one thing, cleared by one question, and the branch decides

**Settled in cycle 0.5.5**, implementing what survives of `unknown` after D-007 and
D-097.

### What is left of `unknown`, and why so little

`unknown` is **compiler-assigned and not user-writable** (`TYPE_REFERENCE` §27).
The prototype allowed `int32:val = unknown;` and used it as a general degradation
mechanism; **D-007 struck that**, on the grounds that `tbb`'s sticky ERR already
covers degraded computation and two mechanisms for one job is one too many.

What survives is a single thing: **the `value` field of a `Result` carrying an
error.** So this analysis has exactly one place to look, and looking anywhere else
would be inventing a fourth Layer 2 mechanism — which D-007 is explicit that
nothing may do, because which mechanism applies is decided by **type, never by
context**.

### Why it earns a rule rather than a convention

D-007's argument for trapping rather than tainting, in its own words: *"a tainted
value that reaches an actuator command produces a wrong action, which is worse than
no action. `ok()` is supposed to prevent this, but that is a discipline, and
disciplines fail under pressure."*

The taint exists so that reaching an actuator with one is a **compile error**
rather than a discipline.

### `ok()` does not clear it, because `ok` does not exist

0.5.5's plan says the taint is "cleared by `ok()` or by checking `is_error`". That
was true when it was written and stopped being true at **D-097**: `ok(x)` asked
whether an ordinary value was `unknown`, D-007 had already removed user-writable
`unknown`, and an operator whose subject no longer exists went with it.

**Checking `is_error` is the only route**, and it was always the one that composed
with the `Result` discipline.

### The branch decides, not the mention

A syntactic "was `is_error` tested" check would accept this:

```nitpick
if (r.is_error) { pass (r.value); }      // the test is right there
```

`r.is_error` being **true** is the tainted branch. So the condition **refines the
two arms**: whichever arm the test proves safe is the one where the read is
allowed, and the negated form swaps them. The early-exit shape — `if (r.is_error)
{ pass 0i32; }` and then read below — needs nothing of its own: an arm that leaves
contributes nothing to the merge, so what survives is the other arm's state.

**A `pick` on `r.is_error` gets the same refinement.** `.is_error` is a `bool`, so
that is a two-armed `pick` that 0.5.4 made exhaustive-checkable, and recognising
only the `if` would push people toward the less readable form — the opposite of
what an analysis should do.

### `checked` intersects, which makes two of five and three of five

Adding it to the binding state settles the shape of that table:

| Set | Merge | What it does |
|---|---|---|
| `must` | intersect | **permits** a read |
| `checked` | intersect | **permits** a read of a tainted value |
| `may`, `moved`, `freed` | union | **forbid** something |

**A permission has to hold on every path; a prohibition has to fire on any of
them.** That is why two intersect and three union, and it is a better rule to carry
forward than five directions memorised individually.

**An assignment clears it**, because the binding then holds a different `Result`
that nothing has checked. Without that, one check at the top of a function would
license every read below it.

### Only a binding-rooted read is checked

`f().value` reads the field of a temporary, and there is no way to have checked
*that* temporary — the check would call `f` again and get a different `Result`. So
the idiom is to bind it, and the binding is what this tracks. That is the shape of
the language rather than a hole in the analysis.

`raw` and `?!` need nothing here: they do not read `.value`, they unwrap. Explicit
and greppable is the only property this language asks of an escape hatch, and both
already are.


---

## D-122 — What marks an acquisition is a contract, not a name

**Settled in cycle 0.5.6**, completing D-056 and extending D-113 by one form.

### The gap D-113 left

D-113 settled `acquires <= N` for a trait method — the **bound** that makes a call
through a trait object have a finite acquisition set. It says implementations "may
acquire no level above `N`, transitively", and that the whole-program analysis
computes acquisition sets.

**Nothing said how the analysis knows an acquisition happened.** D-056 gives the
shape — `cfg_lock.acquire(deadline)` on a `Mutex<T, LEVEL>` — and the concurrency
stdlib does not exist, so there was nothing to recognise even in principle.

### The decision: a second form of the same clause

```nitpick
func:acquire = Guard<T>(Self:self, Deadline:d) acquires LEVEL;   // a FACT
func:commit  = NIL(Self:self)                  acquires <= 3i32; // a BOUND
```

**One keyword, two forms, and the `<=` is where a reader sees the difference.**
`acquires N` states that this function takes lock level N; `acquires <= N` states
that whatever this method reaches takes nothing above N. A declaration and an
obligation on the same axis, which is why they share a keyword rather than getting
two.

The analysis therefore **knows nothing about what a `Mutex` is**. It needs no
stdlib to exist, and `rwlock`, `condvar`, `channel`, `barrier` and anything later
participate by writing the clause — which is what D-056's "every blocking
primitive is levelled, not just mutex" requires, rather than a list of blessed
types the compiler carries.

### What was rejected: recognising the acquisition by its spelling

A method named `acquire`, or a type named `Mutex`, or a return type named `Guard`.
**That is what the prototype's borrow checker did for deallocators** — a set
containing `close`, `release`, `destroy`, plus any name ending in `_free` — and
what D-119 refused three subcycles ago. A rule keyed on what something happens to
be called is a rule you have to memorise a list for, and it gives ownership
semantics to code that never asked for them.

A marker trait was the other candidate. It leaves *which* comptime argument is the
level unanswered — `Channel<T, CAP, LEVEL>` is ambiguous — and still needs
something to say which methods acquire.

### The keyword went in through the spec

`acquires` was added to `LEXICAL_REFERENCE.md`'s `VerificationKeyword` production,
and `gen_tables.py` regenerated `token_kind.npk`, `keywords.npk`, `token_name.npk`
and the seed's keyword table from it. **Nothing was hand-edited**, including the
renumbering a new keyword forces on every token after it — which is the whole
reason that generator exists.

### When a lock is released, with no release to look at

D-056's model is that **the guard's lifetime is the critical section**: release is
automatic at scope exit, so there is no unlock to forget and no early-return path
that leaks the lock. What that means for the analysis:

- **A level is held for the rest of the block that BOUND it.** `Guard<T>:g = …
  acquire(…)` holds it; the block ending releases it.
- **A bare call leaves nothing held.** A helper that acquires and releases
  internally had its guard scoped inside itself, and the caller holds nothing
  afterwards. Treating a call and a binding alike refuses every second call to an
  ordinary helper — verified by breaking it.
- **A function's own declared level is held throughout its body.** `acquires 2i32`
  means everything below happens with 2 held, which is what makes a helper reaching
  1 a downward acquisition.

### An implementation inherits its trait's bound

Whether or not it repeats the clause. Requiring the repetition would make the
promise **opt-in at the one place it has to be kept**, and "an undeclared method
may not acquire at all" would quietly become "unless the implementation forgot to
say so".

An absent clause is a bound of nothing — not a special case to remember, but the
same shape that makes a trait method with no body a real absence rather than a flag
(D-103).

### Two directions, and they are not the same question

- **The ordering rule reads `acq_min`** — the lowest level reachable, because that
  is what could be ≤ what is held.
- **The bound check reads `acq_max`** — the highest, because a ceiling is about the
  worst case.

Confusing them accepts an implementation that acquires 9 because it also acquires
1.

### What this does not claim

Static ordering proves the common case. It cannot cover priority inversion, a peer
process that stops responding, or a lock held across a boundary the analysis cannot
see. That is why every blocking operation takes a deadline and returns `Result`,
and why the flag says **lock-order freedom** rather than deadlock freedom.

An honest narrow guarantee plus a stated containment mechanism is worth more than a
broad claim nothing backs — which is the point of D-056 and the reason this
analysis exists at all.


---

## D-123 — What `#[derive]` may derive, and what each one generates

**Settled in cycle 0.6.0**, because derive emits declarations and the frontend
accepts declarations exactly once.

`TRAITS_REFERENCE.md` §2.5 listed nine — `Default`, `PartialOrd`, `ToString`,
`Eq`, `Hash`, `Clone`, `Debug`, `Ord`, `Display` — and **what any of them
generates was written nowhere.** Two leave the list.

### `Default` is not derivable

**Deriving it means the compiler choosing values that carry meaning**, and in this
language several of the obvious choices are false:

- **`fd` zero is stdin**, not "no file descriptor". That is D-042's entire
  argument: an `fd`'s number is an allocation artifact and reading meaning into it
  is the mistake the type exists to prevent.
- **`tbb32` zero is "no error"** — a claim rather than an absence, and the one
  claim a `tbb` is least entitled to make by default.
- **A pointer zero is `NULL`**, which the pointer rules constrain rather than treat
  as a neutral starting value.

D-010 removed implicit defaults on the argument that **"a default makes an
uninitialized read *defined*, whereas the analysis makes it *impossible*."** A
derived `Default` reintroduces exactly that through an explicit door: the value
still appears without anybody choosing it, and the fact that `derive` was written
does not make the *contents* chosen.

**If a type wants a default, someone writes one.** Then it is a value somebody
picked, and the reason it is that value is somewhere a reviewer can read.

### `Display` is not derivable, because `ToString` already is

Two names for one job. `Display` and `ToString` would both generate a function
returning `string` — D-053 moved formatting to `&{ }` interpolation, so there is no
second mechanism for one of them to use.

"Fewer parallel mechanisms is better than more" is the blueprint philosophy applied
directly: two spellings mean remembering which applies where, which is the cost the
philosophy exists to avoid. **`ToString` stays; `Display` is removed from the
list.**

### The seven, and what each generates

| Derive | Generates | Refused when |
|---|---|---|
| `Eq` | field-wise equality | a field's type is not `Eq` |
| `Ord`, `PartialOrd` | lexicographic comparison in **declaration order** | a field's type is not ordered |
| `Clone` | field-wise clone | a field's type is not `Clone` |
| `Hash` | field-wise combination, **FNV-1a** | a field's type is not `Hash` |
| `ToString` | a function returning `string`, built with `&{ }` | a field's type is not `ToString` |
| `Debug` | the same, in a diagnostic form naming the type and its fields | as above |

**A refusal names the field that blocks it, not the type.** "`Point` is not
`Hash`" makes the reader check every field themselves; the compiler already knows
which one.

### Two choices inside those, and why

**`Ord` compares in declaration order.** It is the standard answer and it has a
real cost worth stating: **reordering a struct's fields becomes a semantic
change.** The alternative — refusing to derive `Ord` and requiring it by hand — is
worse for a comparison that is mechanical in the overwhelming majority of cases,
and the cost is visible at the declaration rather than hidden.

**`Hash` combines with FNV-1a.** The combiner has to be *specified* or the derived
implementation is not deterministic across builds, which a verified compiler cannot
have. FNV-1a is simple enough to verify by inspection and is non-cryptographic by
intent — **the real crypto is `ncrypto`, a separately audited artifact**, and a
hash for a map is not that job.

### Derive is a macro that does not look like one

It emits declarations, so it runs in the expansion phase and inherits its
constraints — output complete before resolution begins, and its diagnostics point
at generated code.

A derived `impl` beside a hand-written one for the same trait is **two impls of one
trait**, which `check_coherence` already refuses (0.4.6). It should keep refusing,
and the message should say which one was derived — otherwise the reader is looking
for a second `impl` they never wrote.


---

> **[D-258, 2026-09-05]** A derived `Clone` is member-wise (a struct
> literal of each field's clone; an enum's `pick` per variant), never `pass
> self` over a subject that may own — the generic form aliased an owner
> (OPEN_DECISIONS DEF-18) — and a derived `Debug` reaches a named or
> parameter member through `debug`. The prelude implements the seven for
> every scalar it can name (D-257). `Hash`'s tag-only enum rule is unchanged.

## D-124 — A macro's reach is exactly its module, and hygiene is what decides it — **SETTLED**

D-057 flipped hygiene: an identifier in a macro body resolves in the scope the
macro was **written** in, always, and `#caller(NAME)` is the sole way to reach the
invocation site. It did not say **how far an invocation may be from that scope**,
and the answer turns out to decide how much of the compiler hygiene touches.

**A macro is invocable only in the module that declares it.** It is not exported,
`use` does not bind it, `pub` on it changes nothing, and a module nested inside the
declaring one cannot reach it either.

### Why this is the rule and not a restriction bolted on

The defining scope and the landing scope become **the same scope**, and that
collapses hygiene from a mechanism into a consequence:

| Position | What hygiene needs | With the rule |
|---|---|---|
| declaration | the emitted body resolves in the defining module | it landed there — nothing to do |
| statement | free names skip the caller's locals | the expansion is a block whose parent is the module scope |
| expression | free names skip the caller's locals | one flag on the substituted node, one switch in `resolve_expr` |
| type names | resolve in the defining module | no type is declared inside a function, so every type name already resolves at module scope or above |

Without it, every one of those needs a **per-expansion scope carried alongside the
one being walked** — through `resolve.npk`, `resolve_type.npk`, and the four
`type_*.npk` walks that thread a scope down into declarations. Six walks, each
with a call site that can be forgotten, and forgetting is silent: the name binds
to the wrong thing and nothing reports it.

### The evidence that nothing is lost

**Not one of the prototype's twenty-five macro declarations is `pub`.** Every
macro test in the corpus is a single file. Cross-module macro invocation is
exercised nowhere and specified nowhere — it is not a feature being removed, it is
a feature that was never written.

And there is a positive argument, not only an absence. A macro that crossed a
module boundary would mean **its body reads names the invoking module cannot see**
— a private helper of the defining module, reached through a name the caller has
no way to inspect. Its expansion would depend on text in a file the reader is not
reading. That is the blueprint philosophy's failure mode stated exactly: the
construct means something different depending on where it is invoked.

### What plays the exportable-code-generation role instead

`#[derive]` (D-123). That is the mechanism designed to be shipped by one module
and applied in another, and it carries no free names at all — a derived impl is
generated from the type it is applied to, so there is no defining scope to escape.
The two are not competing: `macro:` is a local shorthand, `#[derive]` is the
distributable one.

### The refusal is its own code

`NITPICK-MACRO-007`, not `MACRO-001`. "Names no macro" and "names a macro you
cannot reach from here" are unrelated mistakes and only the second has an obvious
fix, which is the same argument that gave the exhaustiveness rules three codes
rather than one (D-120).

---

## D-125 — What `#name(...)` means is decided by the body, and a body decides once — **SETTLED**

`#name(...)` is spelled the same in all four positions (D-046), which leaves the
parser with one item form that could be three different nodes:

```nitpick
macro:outer = () { #inner(); func:f3 = …; };   // a declaration-position SPLICE
macro:opt   = () { #caller(x) + 1i32; };       // an EXPRESSION that begins with one
macro:twice = () { #setup(); #setup(); };      // two STATEMENTS
```

A splice must be a `DeclId` and a statement must be a `StmtId`, the body window is
read as one or the other, and **the sigil does not say which**.

**Reading the sigil alone was the first answer and it was wrong.** It made every
body that began with `#` a declaration body, so `macro:opt` above was refused at
its invocation as "not a single expression" — a true sentence about a body nobody
wrote — and a statement macro could not invoke another statement macro at all.

**The rule: a macro body is a declaration body if it contains a declaration**,
which is what `MACRO_REFERENCE.md` §1 already said. It is decided by scanning
tokens from the body's `{` to its match, at brace depth 1, **before the first item
is parsed** — so every item is then read the one way the body has settled on.

### The one shape this cannot classify, and what happens to it

A body that is **nothing but a single invocation** — `macro:alias = () { #b(); };`
— is whatever `b` is, and the parse cannot know. It is classified as an
**expression body**, which is right when `b` is expression-bodied and is otherwise
resolved at expansion: instantiated at statement position it becomes a block
containing `#b();`, and the next round expands that in place. Both work.

`comptime` folding (0.6.4) inherits the same question for `comptime(#m())` and the
same answer: expansion precedes evaluation and runs to a fixed point first (D-057).

---

## D-126 — Every `#name(...)` still standing at the end of expansion is refused — **SETTLED**

`#` is the compiler-directive sigil for **both** kinds of thing — the three
builtins (`#size_of`, `#wild_ptr`, `#wild_slice`) and every macro invocation
(D-046). Nothing checked which one a given `#name` was.

The consequence was not a bad diagnostic; it was **no diagnostic**:

```nitpick
int32:b = #totally_not_a_macro_or_builtin(3i32);   // compiled clean
```

`type_of_expr_inner` has no case for `ExprBuiltinExpr`, so it fell through to type
`0` — the **invalid** type, which exists precisely so that one bad annotation
produces one diagnostic instead of a cascade, and which is therefore silent. A
mistyped macro name expanded to nothing, typed as nothing, and said nothing.

**After expansion reaches its fixed point, the whole expression array is scanned,
and a surviving `#name(...)` is refused** — as `MACRO-001` if the name is unknown,
`MACRO-007` if the macro exists elsewhere, `MACRO-008` if it is `#caller`.

### It is also the proof that the expansion walk has no hole

Expansion finds invocations by **walking** each module's declarations, because a
whole-array scan cannot say which module a node is in and D-124 makes that the
deciding question. A walk can miss a statement kind — the failure this compiler
has paid for repeatedly — and a missed body would silently never expand.

The scan is the counterpart: an invocation the walk never reached is still in the
array, so a hole arrives as **a refusal naming the invocation** rather than as a
program that quietly did nothing. The walk is the mechanism and the scan is the
proof, and neither is trusted alone.

**Two exemptions, both principled.** A macro body is a **template**, so the
invocations written in one are consumed when the macro is cloned and are marked at
parse time by id range rather than by a second walk. And the scan does not run when
expansion has already reported — an invocation refused for wrong shape is still
standing, and reporting it again as "names no macro" is a sentence about the
wreckage of the first.

### What this leaves open, named rather than parked

The three `#` builtins are **recognised but still not typed**: `#size_of<T>()`
returns the invalid type, so `alloc(n * #size_of<Symbol>())` — which this compiler
writes everywhere — is not type-checked, and `#wild_ptr`'s "legal only in `wild`
context" rule (D-019) is enforced nowhere. That is 0.4-era work the type checker
missed, it is unblocked by nothing, and **0.6.4 is where it lands**: folding
`#size_of<T>` is exactly what a `comptime` evaluator has to do, and it cannot fold
what it cannot type.

---

## D-127 — The seed did not check call arity, and two days went to the wrong theory — **SETTLED**

A call passing fewer arguments than the function declares compiled **silently**, and
emitted a call with fewer operands than the callee reads. The callee then read
whatever was in that register or stack slot.

```nitpick
pub func:etyper_init = ExprTyper(Ast->, TypeTable->, SymbolTable->, InternTable->,
                                 DiagList->, int32, ImplTable->, BoundTable->,
                                 InstanceTable->, ExprTypes->)      // ten

raw etyper_init(@ast, @types, @tab, it, @diags, 0i32, @im, @bd, @ets)   // nine
```

`@ets` landed in the `instances` slot; `expr_types` read a register, which came back
as `320` — a plausible *count*, which is why it never looked like corruption. Three
call sites, all in `tests/frontend/`, all three of them the tests that had been
segfaulting.

**The fix is the check, not the call sites.** The seed refuses a call whose argument
count disagrees with the declaration now. It is throwaway (D-085) and that does not
excuse it: **a tool that silently miscompiles the compiler is worse than no tool.**
Its first run found the three sites and nothing else, so the compiler's own source
was clean.

### Why it read as a memory bug for two days, and what that cost

The symptom moved whenever anything unrelated changed size — because *what junk sits
in an unwritten register slot* depends on the binary's layout. Every experiment
confirmed the wrong theory:

| experiment | result | what it "showed" |
|---|---|---|
| add a size header to each allocation | three tests segfault | the allocator |
| pad allocations without moving the pointer | passes | it is the *write*, not the shift |
| write the header only on the `grow` path | passes | narrower still |
| add 1.5 MB of `.bss`, poisoning **disabled** | segfaults | not the allocator at all |

That last row is the one that should have ended it: a semantic no-op cannot cause a
crash, so the thing being measured was never the allocator. It was read as "the
instrumentation perturbs the bug" and the hunt continued.

**And the evidence was there from the first hour.** `t == 320` is a *plausible
value*. Corruption produces addresses, poison patterns, or zeros; a small round
number where a pointer belongs is a value that was never written. It was noted, and
the allocator theory was not dropped.

**Two of three instrumented runtimes contained bugs of their own** — a string
replacement that shrank an unrelated 64 KB buffer to 1 KB, and a return-type
mismatch across three call sites. Hand-editing LLVM IR by substitution to chase a
layout-sensitive bug adds perturbations that look exactly like the thing being
hunted.

### What actually found it

**`valgrind`, in one run, on the binary that was passing.** "Use of uninitialised
value of size 8" in `exprtypes_set`, `--track-origins=yes` naming the stack
allocation it came from.

The tool was available the whole time. It was not reached for because the
zero-dependency rule felt like it applied — **it does not: it governs the artifact,
not the workbench**, and the seed is throwaway besides. Reach for the debugger before
building one.

Worth knowing which tools work on a freestanding binary, since it is not obvious:
`valgrind`, `gdb` and `objdump` operate on machine code and do not care that there is
no libc. **ASan and UBSan do not** — they link a runtime that needs one.

### The rule this leaves behind

**A symptom that moves when unrelated things change size is evidence of a value that
was never written, not only of memory that was overwritten.** Both produce
layout-dependent behaviour. The second is the more dramatic explanation and was the
wrong one.

---

## D-128 — A macro never renames what it emits, and a collision is an error — **SETTLED**

`MACRO_REFERENCE.md` §10 carried this as open: two invocations of a
declaration-emitting macro in one module emit the same names, and nothing said
whether that collides or is renamed.

**Nothing is renamed.** Hygiene (D-057) governs how a macro body *reads* names;
the names it *writes* are the names it wrote.

The argument is that renaming makes the feature useless in both directions:

| position | if the emitted name were renamed |
|---|---|
| module level | `#make_pair()` emits `greet1`, and `main` could not call it |
| struct body | `#make_xy_fields()` emits `x`, and `P{ x: 1i32 }` could not name it |
| impl body | the spliced method could not satisfy the trait it implements |

Every one of the corpus's splicing tests works by naming what was emitted. A macro
that emits inaccessible declarations is a macro that emits nothing.

**So a collision is an error, like any other name declared twice.** That is not a
rule about macros: `struct:P = { int32:x; int32:x; };` was accepted too, with the
second field unreachable — field lookup returns the first match while layout gives
both storage, so the struct was the size of two fields and only one could ever be
read. Splicing is what made it easy to hit; the check is on structs and enums,
where it belongs.

A rule that fired only on *spliced* names would be the construct-means-something-
different-by-context defect the blueprint philosophy exists to prevent.

### What this does not settle

Whether a macro should be *able* to control the names it emits — `macro:m = (N) {
func:N = …; };` — which is the other half of §10's entry and remains open.
Substitution reaches expressions, and a declaration's name is a payload, so today
the emitted function is literally called `N`. That is unimplemented rather than
refused, and it is a question about **parameters**, not about hygiene.

---

## D-129 — Eight expression kinds are never typed, and the checker says nothing — **SETTLED — landed 0.6.7** (this heading said OPEN long after the work shipped; annotated by the 0.9.8 doc-sync, which is what `check_decisions_current` exists to catch)

`type_of_expr_inner` reaches every expression kind by name and falls through to
type `0` — the **invalid** type. That encoding exists so one bad annotation
produces one diagnostic instead of a cascade (0.4.0), which means it is *silent by
design*. A kind with no case is therefore not a crash and not a diagnostic: it is a
construct the checker accepts **without looking at it at all**.

Eight of the forty-seven are in that state:

| kind | what is accepted unchecked |
|---|---|
| `ExprStructLiteralExpr` | a field the struct does not have, a value of the wrong type, a literal that omits fields entirely |
| `ExprArrayLiteralExpr` | element types, and the count against the declared length |
| `ExprVectorCtorExpr` | the same, for vectors |
| `ExprPipeExpr` | whether the left side is what the right side takes |
| `ExprBuiltinExpr` | `#size_of<T>()`, `#wild_ptr`, `#wild_slice` — D-126 |
| `ExprComptimeExpr` | the folded value's type |
| `ExprDynCastExpr` | whether the target trait is implemented |
| `ExprPickExpr` | the arms' common type |

### How they were found, which is the part that matters

Two by accident, both while doing something else. `#name(...)` in 0.6.2, when
`#totally_not_a_macro_or_builtin(3i32)` compiled clean. `Point{ x: 1i32 }` in
0.6.3, while checking that a *spliced* field participates in layout — the omission
was accepted, and so was the same omission on a hand-written struct.

**Neither announced itself, and reading the type checker alone never would.** The
kind list and the checker have to be diffed, which is exactly what
`check_kinds_reachable` has done for the parser since cycle 0.2 and what nothing
did for the checker.

`stmt_is_classified` exists for statements, added in 0.4 after a walk was found
with a hole in it, and its comment says the mechanical half — *whether each kind is
visited* — is the half a test can prove. **There was no expression equivalent.**

### The instrument, which ships now

`check_kinds_typed` in the harness diffs `ast_kind.npk` against every
`src/frontend/type_*.npk` and fails on any expression kind neither typed nor listed
in `UNTYPED_EXPR_KINDS`. The list shrinks and never grows: each entry records where
that kind is scheduled, and adding one is an admission rather than an oversight.

That turns eight holes found by stumbling into a maintained, enforced fact — and it
is what makes the schedule below checkable rather than a promise.

### Why this cannot wait for the emitter

Phase A's artifact is **a checker that validates completely and emits nothing**. A
checker that accepts `Point{ zzz: 1i32 }` does not validate completely, and a
struct literal that omits a field leaves it with no value — which is the undefined
state D-010's definite assignment refuses everywhere else. That is a safety
property, not an ergonomic one.

**0.6.7 — the expressions nothing typed**, before 0.6.6 closes Phase A. The two
`comptime`-shaped entries land in 0.6.4 instead, since folding a `#size_of<T>` is
something the evaluator has to do anyway and it cannot fold what it cannot type.

---

## D-130 — What folds, and the two bounds evaluation needs — **SETTLED**

D-057 settled that expansion is bounded and named `--comptime-budget <N>` as the
precedent. It did not say what a constant expression *is*, and four sites in this
compiler had been answering "an integer literal" and pointing at cycle 0.6.

### What folds

**A `const` global, and nothing else that is a name.** `const` is the marker that
says a binding has one value for the whole program (D-010), so it is the marker
that says a name may stand in a constant expression. Nothing is inferred:

| written | folds | because |
|---|---|---|
| `const int32:N = 4i32;` | ✓ | one value, for the whole program |
| `fixed int32:N = 4i32;` | ✗ | assigned once at RUN time |
| a local, a parameter | ✗ | the same |
| a `comptime func:` call | ✓ | the declaration says it may run at compile time |
| an ordinary `func:` call | ✗ | whether the compiler runs your code is not discovered by accident |

**And the evaluator is an interpreter, not a constant folder.**
`MACRO_REFERENCE.md` §8 recovered the list from `COMPTIME-001…013`: mutable locals,
assignment, `while`, calls that nest. Anything it can express is something the
compiler runs at build time.

### Where it lives, and why that is forced

**In `resolve_type.npk`.** Folding needs to resolve types — `#size_of<T>` is a
constant whose value is a type's size — and resolving types needs to fold —
`int32[N]` is a type whose shape is a constant. The two are mutually recursive, so
they are one module; split in two, each would have to `use` the other and the
frontend has no forward declaration to break that with.

The layering below it is one-way and stays that way: `type_layout.npk` uses this
module, so this module does not call `ensure_layout`. Resolving a type is what
computes its layout, so by the time there is a type id there is a size.

### Two bounds, and the second was found by segfault

The fuel bounds **total work**. It does not bound **recursion depth**, and those
fail differently — which is the same argument D-057 makes for expansion having two.

```nitpick
comptime func:forever = int32(int32:n) { pass (raw forever(n)); };
```

With only the fuel, this **segfaulted the checker**: each level spends about three
units of a 4096-unit budget and costs several native frames, so the compiler's own
stack went first. `FOLD_DEPTH` is 64, the same number `GENERIC_DEPTH` uses and for
the same reason — a nesting a person wrote on purpose is nowhere near it.

### The budget gets its own code

`NITPICK-TYPE-025`, not `TYPE_NOT_CONSTANT`. "This is not a constant expression"
and "it is, and it never ends" send the reader to opposite places. And the site
that asked for the fold does not add its own sentence on top: one failure, one
diagnostic, which is the rule the expansion audit already follows (D-126).

### `--comptime-budget <N>` is a driver feature and the driver does not exist

The bound is a constant today. The flag needs argument parsing, a manifest, and a
place to put a build option — `BUILD_REFERENCE.md` §7's `npkg`, which is cycle 0.8.
Nothing about the mechanism changes when it arrives: the number becomes
configurable, and the two bounds stay two.

---

## D-131 — Folding is not typing, and the 0.5.0 tripwire stands — **SETTLED**

`tests/frontend/expr_types.npk` asserts that **exactly one** expression in its
program is unrecorded by the checker: the `int32[4]` size. Its note said the count
would go to zero in cycle 0.6, "where `comptime` folds general constant expressions
and those will need types", and that the assertion failing would be the tripwire
working.

**Folding arrived and the count did not move.** That is the right answer, and the
tripwire is re-armed rather than retired.

`fold_const` computes a value **and the type it carries** — `Mutex<Config, 2i64>`
is checked against `comptime int32:LEVEL` precisely because the folded value knows
it is an `int64`. But it does not go through `type_of_expr`, and it does not record
anything in `ExprTypes`, because **nothing downstream asks**: `ExprTypes` is what
the checker gave each expression, and its readers are the cycle-0.5 analyses, which
walk bodies and never enter a type node.

So an expression inside a type is still not in the checker's record. The assertion
says something true; what changed is why, and the note now says which.

The general form is worth keeping: **a tripwire that does not fire is not
automatically wrong.** It fires when an assumption changes, and "the assumption
held for a different reason than expected" is an outcome that has to be written
down, or the next reader re-derives it.

---

## D-132 — The seven derivable traits are a prelude, in source — **SETTLED**

D-123 named the seven and said what each generates. **It did not say what any of
them IS.** `#[derive(Eq)]` generates `impl:Point:Eq`, which needs a trait `Eq` to
exist, and nothing declared one: not this compiler, not a library — `nlibc` has no
sources — and neither `TRAITS_REFERENCE.md` §2.5 nor D-123 gave a method name or a
signature. They were named everywhere and defined nowhere.

### The prototype answers it, and is followed where it speaks

`expandDeriveAttributes` in the prototype generates a trait impl per derive, and
the trait is compiler-known — nothing in its tests declares one:

| Derive | Method | Prototype signature | Here |
|---|---|---|---|
| `Eq` | `eq` | `bool(S:self, S:other)` | same |
| `Clone` | `clone` | `S(S:self)` | same |
| `Hash` | `hash` | `uint64(S:self)` | same |
| `ToString` | `to_string` | `string(S:self)` | same |
| `Debug` | `debug` | `string(S:self)` | same |
| `Ord` | `less_than` | `bool(S:self, S:other)` | **`cmp` → `Ordering`** |
| `PartialOrd` | `partial_cmp` | `int32(S:self, S:other)` | **`partial_cmp` → `Ordering?`** |

### Two corrections, and why they are corrections rather than preferences

**`Ord` returning `less_than` makes `Ord` weaker than `PartialOrd`.** A boolean
"is less" carries strictly less information than a three-way comparison, and `Ord`
is the *stronger* promise — a total order. It gets the three-way answer.

**An ordering is not an integer.** `partial_cmp` returning an `int32` meaning less,
equal or greater *by sign* is three meanings in one number, which is precisely the
shape D-036 rejects when it says `bool` and `char` are not integers. So there is an
`Ordering` enum — `Less`, `Equal`, `Greater` — and the partial form returns
`Ordering?`, `NIL` being the case that has no answer.

**`PartialOrd` survives where `Display` did not**, and the difference is not taste.
`Display` was a second name for `ToString`'s job. `PartialOrd` answers a different
question, and `OP_REFERENCE` §4 makes it a real one: floats are IEEE 754 with `nan`
and **no trap**, so two `flt64`s genuinely may not compare and a total `cmp` over
one would have to lie.

**The traits declare their methods.** The prototype registers them as *empty* trait
declarations and lets the derived impl supply whatever it likes. That is an
implementation shortcut, and copying it would undercut everything cycle 0.4.6 built:
`check_impls_complete` would have nothing to check, and a `<T: Hash>` bound would
promise no methods, so a generic body could not call `t.hash()`.

### Source, not synthesis, and generated rather than hand-escaped

The prelude is **`src/prelude/prelude.npk`**, ordinary Nitpick, and the generator
emits it into `prelude_source.npk` as a string the compiler carries. From the lexer
onward it goes through exactly what a program's own source does, so there is no
second way for a declaration to come into existence — and a person can read it.

**Embedded rather than loaded from a path**, because a file is a thing that can be
missing: a compiler whose prelude is found by path has a deployment story, and one
whose prelude is a constant does not.

**One line with `\n` escapes**, because the real lexer refuses a newline inside a
plain string literal while the seed's accepts one, and the block form `"""` is the
other way round — the real lexer has it and the seed does not. That is the one
spelling both accept, and getting it wrong would have compiled through the seed and
been rejected by the compiler the seed builds.

### It is an implicit import, not a scope above modules

The first attempt collected the prelude into the **root scope**, reasoning that
every module's scope has scope 0 as its parent. **It does not work: `scope_lookup`
stops at the first module scope.** A module is a closed namespace, reachable only
through `use`, and that is the whole of what a module means here — so there is
nothing above one to put a prelude in.

The prelude is bound the way `use "prelude.npk".*` would bind it, into every module
scope in the program including nested ones. That is the honest model rather than a
workaround: **the prelude is not magic, it is an import nobody has to write.**

**Declaring a prelude name is refused**, not allowed to shadow. A module's own scope
is searched first, so a program's `trait:Eq` would silently take over — and then
`#[derive(Eq)]` would generate an implementation of a trait the compiler knows
nothing about, against methods it guessed. One name, one meaning, which is the same
reason a variable cannot be called `if`.

---

## D-133 — Derive generates source, and refuses what the language cannot yet say — **SETTLED**

### Source, parsed — not AST built by hand

The prototype builds `DeclImplDecl` and `DeclFunctionDecl` nodes in C++. Doing that
here would be several hundred lines of slot-filling for seven derives, and **every
slot is a chance to write a member count into the field a trait id lives in** —
which is the defect this compiler has paid for seven times, most recently in 0.6.3
where an `impl` read as an "item" segfaulted files containing no macros at all.

Generated **text** goes through the same parser as everything else. The worst it can
do is fail to parse, loudly — and it did, once: `pick self { … }` written from
memory, where the real syntax is `pick (self) { (pattern) { … }, … }`. That surfaced
immediately instead of producing a tree nobody could have read.

### What each derived body uses

**Operators where the language has them.** `Eq` compares with `!=`, `Ord` with `<`
and `>`, `Clone` is `pass self`, `ToString` and `Debug` are `&{ }` interpolation
(D-053). Going through each field's own trait would require `int32` to implement
`Eq`, and then the prelude would need an impl per scalar type per trait.

**`ToString` on an enum goes through a `pick`, not `&{self}`** — interpolation is
what *calls* `to_string`, so a body built out of it would call itself. The `pick`
is exhaustive by construction and 0.5.4 checks that it is.

### Two are refused rather than guessed at

**`Ord` and `PartialOrd` on an enum.** Ordering a variant means comparing its tag,
and no operator yields one — `<` on an enum is refused by the type checker, which
was measured rather than assumed.

**`Hash`, on anything.** FNV-1a folds bytes. An integer, `bool` or `char` field can
supply its bits through `=>!`, and a nested type that derived `Hash` can supply its
own — but **nothing in `builtins.npk` exposes a `string`'s bytes to Nitpick source**,
and a derived hash that quietly skipped string fields would vary with data it
ignored. That is worse than not having one. One primitive unblocks it.

Both refuse at the `#[derive]` rather than generating code that fails to type-check,
because a diagnostic inside generated code points at a line nobody wrote.

### What is still owed, and where it lands

D-123 asks that a refusal **name the field that blocks it** — "`Point` is not
`Hash`" makes the reader check every field themselves. That cannot be answered where
derive runs: it generates in the **expansion** phase, before types exist, so "is
this field `Ord`?" has no answer yet; and letting the generated code fail to
type-check puts the diagnostic inside code the reader never wrote.

**It is the same mechanism 0.6.6 builds** — a diagnostic about generated code that
has to be told where the reader's code is — and it lands there. A derived impl is
marked `DECL_DERIVED` already, which is what lets the coherence message say one of
the two was generated rather than sending the reader to look for an `impl` they
never wrote.

---

## D-134 — `Self` in return position was unmatchable, and the reason is `Result` — **SETTLED**

Found by the prelude's `Clone`, which is the first trait method in this compiler's
history to return `Self`.

`check_impls_complete` compared a trait method's type against an impl's and
substituted `Self` **one level deep**, deliberately: `Self->` and `Self[]` are
refused rather than silently mismatched, because a trait method taking `Self->` is a
shape somebody meant.

But **every function returns `Result<T>`** — the one rule with no exceptions
(D-002). So `func:clone = Self(Self:self);` has the type `Result<Self>` and the
impl's has `Result<Point>`, and `Self` is a level down, where the one-level
substitution cannot see it.

The result: **every trait method returning `Self` was reported as not having the
signature its trait declares**, about two signatures that were identical.

The return type is now compared **through** its `Result` wrapper, because that
wrapper is the language's and not the author's. `Self->` and `Self[]` stay refused,
and that remains right — those wrappers are ones somebody wrote.

The general shape is worth keeping: **a rule about what the author wrote has to
account for what the language adds.** `Result` is invisible in the source and
present in every type, so any comparison of function types has to decide about it
explicitly rather than by not noticing.

---

## D-135 — `simd<T, N>` is the vector mechanism; the rest are library types — **SETTLED**

Nitpick had two overlapping ways to say "a small fixed-size bundle of numbers", in
adjacent sections of one document:

| | spelling | backing | tier |
|---|---|---|---|
| `TYPE_REFERENCE` §14 | `simd<flt64, 2>` | `<2 x double>` — **an LLVM vector** | 0 |
| `TYPE_REFERENCE` §15 | `vec2` | `{flt64, flt64}` — **a struct** | 1 |

### Why the keyword form is the slower one

The reason these were primitives was a performance hypothesis — that making the
types Nikola needs into primitives rather than library types "might squeeze out a
little extra performance". **For an LLVM target it is the wrong lever.**

What makes SIMD fast is the value landing in an **LLVM vector type, in a register**,
with arithmetic lowering to vector instructions. A `{flt64, flt64}` struct is passed
in memory or split into scalars, and recovers nothing unless SROA and the vectoriser
happen to fire. So §15's `vec2` — the *keyword* — is specified as the slow shape and
§14's `simd<flt64, 2>` — the *generic* — as the fast one.

**A generic the compiler knows how to lower is exactly as fast as a keyword**,
because the spelling is not what reaches the backend.

### And every primitive is trusted computing base

The stronger argument for this project. Astrée gets **one attempt**
(`astree-verification-one-shot-constraint`), and each primitive is more surface to
carry through it. Six vector/matrix/tensor primitives is six more things to verify
than one parameterised one.

It is also the blueprint philosophy applied directly: `vec2` and `simd<flt64, 2>`
are two spellings for one idea, which is the cost D-123 removed `Display` for.

### So

- **`simd<T, N>` is the mechanism**, and stays a keyword — it is the primitive.
- **`vec2`, `vec3`, `vec4`, `vec9`, `matrix<T>`, `tmatrix`, `tensor<T>`, `ttensor`
  become library types** built on it, and **stop being keywords**.

The keyword removal is **necessary rather than cosmetic**: a library cannot declare
a type whose name is a keyword, so leaving them reserved would make the library that
defines them unwritable.

It also resolves an inconsistency by deleting the question. **`vec4` was in
`TYPE_REFERENCE` §15 and absent from `LEXICAL_REFERENCE`'s keyword list**, so it was
a type the specs disagreed about and the lexer had never heard of.

### The balanced-ternary family stays primitive, and the argument does not transfer

`trit`, `tryte`, `nit` and `nyte` lower to `i8`/`i16` carrying base-3 and base-9
semantics. **No hardware implements balanced ternary**, so every operation on one is
emulation — and the compiler is the only place emulation can be done well. A library
version would be strictly worse, where a library `vec2` is strictly better.

That is why the two groups were decided separately rather than as one question about
"the exotic types".

### Provenance, because it decided who chose

The ternary family is the user's own design. The vector and SIMD types came from an
**earlier agent's recommendation**, made during a sweep through Nikola looking for
what the language would have to support — the user had "overlooked basic SIMD
operations and vectors entirely" and deferred to it.

Worth recording because it is the difference between a design to be asked about and
an analysis to be redone. A requirement found by sweeping Nikola is evidence about
what Nikola needs; it is not a decision about how to express it.

### What is NOT decided here

**Nothing is implemented.** `simd<flt64, 2>:s`, `vec2:v` and `trit:t` all report
"there is no type named …" — the keywords are lexed and nothing more. The type
system has no vector kind and no ternary kind. This decision says which shape the
work should take when it is done; it does not do it.

`simd`'s constructor spelling is also unsettled. The parser reads `simd(a, b, c)` as
an `ExprVectorCtorExpr`, which the type checker refuses as Tier 1 — correct under
any outcome, and the narrowest thing that keeps the node kind reachable.

## D-136 — `pass v` evaluates `v` before the `defer` stack runs — **SETTLED**

**Decision.** At every normal exit (`pass`, `fail`, `exit`), the exit's value
expression is evaluated **first**, at the exit statement, and the scope's `defer`
blocks run **after** it — so the value a function returns is the value that was
written at the `pass`, whatever the defers do afterwards. Defers run LIFO
(innermost frame first, latest registration first), which "pushes a block onto a
stack" (CONTROL_REFERENCE §4.5) already implies.

**The alternative was live, which is why this is a decision.** The seed evaluates
in the other order — defers first, value second — so

```nitpick
int32:x = 0i32;
defer { x = x + 9i32; }
pass x;
```

returns `9` under the seed and `0` under this decision. The spec was silent; the
divergence is unobservable in the bootstrap today (the compiler's own defers only
release memory), so the fixpoint is unaffected; and the artifact gets the
semantics that survive an audit: **the value you passed is the value returned.**
The other order makes every `pass` a hidden read-after-mutation whose meaning
depends on cleanup code written possibly pages earlier — a construct changing
meaning by context, which the blueprint philosophy forbids first.

**`fail` and `exit` follow the same rule** — the code is read at the statement,
then cleanup runs. One rule for all three exits, no per-exit special case.

**A trap still runs nothing** (D-014, unchanged): `?!` and `!!!` transfer to
`failsafe` without unwinding, value semantics moot.

## D-137 — A declaration's annotations resolve in its HOME scope — **SETTLED**

**Decision.** Every type annotation belonging to a declaration — a signature's
parameter and return types, a struct's field types, a variant's payload types —
resolves in the scope of the **module that declared it**, never in the scope of
whatever module happens to be asking. The symbol table records each declaration's
home as a side effect of collection (`symtab_home_scope`), and every walker that
resolves a foreign declaration's annotations consults it, with the asker's scope
only as the fallback for a declaration nothing collected.

**The defect this ends.** Until 0.8.1, `fn_signature`, `struct_field` and the
payload walkers resolved annotations in the CALLER's scope. `tyres_init(Ast->:…)`
typed fine from a caller importing `ast.npk` and reported "no type named `Ast`"
from one that did not — and under a name collision it would not have erred at
all: it would have silently bound the caller's same-named type. Every test passed
because every test's caller happened to import what the callee's types needed;
the first whole-graph self-check (`npkc src/main.npk`) surfaced fifteen instances
in one run. This is the blueprint rule — meaning does not change with context —
applied to name lookup.

## D-138 — The escape analysis: one narrowing, three relaxations — **SETTLED**

D-004's decision text pre-authorised relaxation: *"if real compiler code needs
borrow-into-inner-aggregate, relax it then, WITH EVIDENCE."* The evidence arrived
when the analyses first ran over the compiler itself (0.8.1). Four changes, each
independently sound:

1. **Type narrowing.** A value whose type cannot hold an address (integers,
   bools, chars, `tbb`, kernel ids, enums, aggregates of only these) never
   carries a borrow. The conservative walk was flagging returned `int32` fields
   as escaping borrows. The one door from pointer to integer is `=>!` through
   the wild family (D-019), which is the explicit surrender of tracking.
2. **Param-rooted borrows may travel up one frame.** A borrow rooted at a
   parameter points into the caller's frame or older, so returning it — or a
   value carrying only such borrows — cannot dangle this frame's return. The
   rule is compositional: if the caller returns it again, the caller's own
   return check proves the next hop. This is the constructor pattern
   (`tyres_init` storing its pointer parameters into the struct it returns).
3. **Param-rooted borrows do not taint bindings.** The same rule applied at the
   write instead of the read, for both declarations and assignments.
4. **Self-wiring.** A value built from borrows rooted at `X` may be stored into
   a field of `X` — the lifetimes are identical by construction. This is the
   pipeline's `f.g = graph_init($$m f.it, …)` shape.

Everything else still refuses: borrows of locals still cannot travel up, stores
rooted elsewhere still cannot travel in, and the analysis rejection suite holds
those lines.

## D-139 — `[]` is the zero array — **SETTLED**

An empty array literal, where a fixed-length array type is expected, is that
array **zeroed** — the same rule `Result{…}` has for omitted fields: absent means
zero. Without it there was no spelling at all for a zeroed `T[64]` field, since a
struct literal cannot omit fields. A **non-empty** literal must still match the
length exactly; partial initialisation stays unspellable, deliberately, because
"the first three, then zeros" is a meaning the reader must guess. Lowered as
`zeroinitializer`.

## D-140 — Enum tag casts, final form — **SETTLED**

`enum =>! intN` reads the tag (identity treated as quantity is an assertion, and
assertions cost the bang); `enum => intN` refuses. `intN =>! enum` **manufactures
a tag** and is permitted with the bang — the first draft refused it outright on
the exhaustiveness argument and was overruled within one subcycle by the
compiler's own source, which must round-trip stored tags (`payload =>!
TokenKind`, 88 sites). This is the same contract as `int32 =>! fd`: an assertion
the compiler cannot check, spelled per-site, greppable. A forged tag makes every
arm of a proven `pick` miss and the statement fall through — the programmer's
explicit lie, not the checker's gap. A payload-carrying enum still refuses the
manufacturing direction at the backend: a tag is not a whole `{tag, payload}`
value, and inventing the other half is not a cast.

## D-141 — The fd floor, the error-code space, and stdout/stderr's contract — **SETTLED**

Cycle 0.8.5, executing D-050/D-075/D-076 at the rung that exists. Four parts.

**The fd quartet.** The floor gains `open`, `close`, `read`, `write` — one
syscall each, faithfully (the floor is the syscall surface, D-051): a short
write is returned, not retried; `open` returns `Result<fd>` with the handle
typed per D-042. `write_all` — the retry loop over `write` — is also a floor
symbol, but only because stepping a pointer is not yet expressible in the
language; when `#ptr_add` lands (0.9) it graduates to `lib/nio.npk` and the
floor loses a symbol. `write_raw` (0.8.3's bare-return write) is gone: two
spellings for one job, and the unwrapped return was a pre-D-075 shape.

**The error-code space, concretely.** `Result.err` has said "< 0 system,
> 0 user" since D-005 laid out `Result`; the floor now honors it: the error slot carries the
kernel's return exactly as delivered (ENOENT is −2 — 0.8.3's floor negated
errno to positive, which this repairs). Floor-detected conditions reuse the
kernel's vocabulary negatively: interior NUL −22, slice out of range −34.
End-of-input is **`E_EOF` = −4096** — the first code past the kernel's error
space (errno stops at 4095), so it can never collide with an errno — and it is
an error code, never a zero in the value channel (D-075), at the floor exactly
as in the Stream trait above it. A zero-length read request returns ok(0):
E_EOF means the stream ended, never that the caller handed over nothing.

**stdout is the product; stderr is the report.** Every tool's diagnostics move
to stderr, through the unbuffered writer, for both of D-076's reasons: they
survive a trap, and they survive a redirect — `npkc bad.npk > out.ll` shows its
errors instead of burying them in the output file. Exit codes are unchanged;
the harness parses stderr.

**The text/byte split is real code now.** `lib/nio.npk` is D-050's normalize-
on-read and `\n`-on-write (CrLf a creation parameter), with D-076's fixed
buffering; `Sink` (diagnostics.npk) lost its fd arm — a write_raw-era mode tag,
the exact pattern D-072 rejected — and is now purely the capture buffer D-075
needs. The compiler's own report path runs through `nio`, so the library tier
is load-bearing from its first cycle.

## D-142 — Runtime traps: the code region below E_EOF, and the `npk_trap` route — **SETTLED**

Cycle 0.9.0, repairing LIVE-2 (and giving LIVE-1's refusals their floor). Two
parts.

**The route.** An emitted guard that fires calls the runtime's `@npk_trap(code)`,
which calls the program's own `failsafe` with the code and exits with its
return — the D-013 contract made mechanical: a runtime fault is a controlled
shutdown, never a hardware fault. `failsafe` is mandatory (D-013), so the
symbol always resolves; the runtime's undefined set is exactly `{main,
npk_failsafe}` and the harness holds it there. D-014 requires `failsafe` to
return positive; until 1.3 injects and verifies that `ensures`, the runtime
refuses to report success after a fault — a nonpositive return exits 70, the
floor's runtime-violation code. `npk_trap` is not a builtin: no program can
call it by name; only emitted guards reach it.

**The codes.** D-141 gave the error space its rule (negative system, positive
user, the region past MAX_ERRNO for conditions errno has no word for). Runtime
trap codes continue that region below `E_EOF`:

| Code | Name | Fires on |
|---|---|---|
| −4096 | `E_EOF` | end-of-input (D-141) |
| −4097 | `DIV_BY_ZERO` | integer `/` or `%` with a zero divisor (D-007) |
| −4098 | `INT_MIN_OVERFLOW` | `INT_MIN / -1` or `INT_MIN % -1` — no representable result, and D-008 already refused inventing one for a plain integer, so it traps rather than wraps |
| −4099 | `OUT_OF_BOUNDS` | a slice/array index past the end, or a range view that does not fit its source (D-070; registered 0.9.2) |
| −4100 | `TBB_ERR` | an ERR value at a bare comparison, or a checked cast out of tbb — the taint about to steer control flow or launder into a number (D-008 §5, D-144; registered 0.9.5) |
| −4101 | `BAD_STEP` | a counted loop's step was not positive at runtime where it could not be a literal (registered 0.9.9) |
| −4102 | `UNREACHABLE` | `#unreachable()` was reached — an arm a stricter analysis had excluded turned out to be reachable, made a controlled stop rather than undefined behaviour (D-061; registered 1.0.9c) |
| −4107 | `DEADLINE_EXCEEDED` | time ran out (D-176; registered 1.1.3): the catchable `Result` error every deadline API returns, and the trap a JOIN raises when a task outlives its mandatory deadline |
| −4108 | `ChannelClosed` | a `send` to a closed channel, or a `recv` from one that is closed AND drained (D-072/D-182; registered 1.1.10). A closed channel still delivers what it holds — a producer's last writes are not lost by its closing — so this is the END of a stream, and a `recv` loop reads it as termination rather than as a fault. |

The guards are emitted in EVERY build — D-068's rule, restated at the emission
site: a build without the verifier still emits the check, and 1.3's static
discharge may remove only what it proves. The signed-width MIN table fails
closed: a width it does not know is an internal `iv_broken`, so a new integer
width (0.9.3's `i128`) cannot silently ship an unguarded `INT_MIN/-1`.

> **[1.5.2, 2026-09-04; D-220, D-251]** −4111 `LimitViolated` — a
> `limit<Rules>` binding was written a value its rule refuses, at any of its
> write points (D-251). The verification family's three contract codes
> follow at −4112…−4114 (`RequiresViolated`, `EnsuresViolated`,
> `InvariantViolated`, reserved for 1.5.3). The table above is the region as
> D-142 registered it; the codes later cycles added (−4103…−4106, −4109,
> −4110) and this one live in the two registries every build reads: the
> prelude's `error:` block and `runtime/npkrt.ll`'s table.

## D-143 — The float family's final form — **SETTLED**

Cycle 0.9.4, resolving the audit's two open float decisions plus the shape of
`flt128`. Probes on the shipping toolchain decided everything measurable.

**`flt32`/`flt64` are the computational floats**, IEEE and TOTAL: arithmetic
through `frem` (lowered to the floor's hand-written `fmod`/`fmodf` — exact by
construction, Sterbenz-aligned subtraction, no rounding anywhere), `fcmp`
ordered comparisons with `!=` as `une` (NaN ≠ NaN is true, so `!=` remains the
negation of `==`), `fneg` negation, and the conversion set. **Float division
by zero does not trap** — ±inf/nan is defined behavior, the reason infinities
exist — and D-007's float row is corrected accordingly. The integer rows keep
their traps; the behavior is selected by type, written at the declaration,
exactly D-007's own non-context rule.

**`flt128` is a storage format**: it lowers (`fp128`) so values can be held,
passed, returned, and crossed over FFI — but it has no literals, no
arithmetic, and no comparison, because each is a soft-float libcall
(`__addtf3`, `__lttf2`, …) and a verification-grade binary128 soft-float
library has no consumer. This is a DECISION, not a deferral: soft-float
enters only if a consumer appears, as audited nlibc-tier work. The checker
enforces all three refusals (`NITPICK-TYPE-030`), so no fp128 instruction is
ever emitted.

**`flt256`/`flt512` do not exist**: LLVM has no fp256/fp512 type. The
keywords stay reserved (the `bt_spec(false)` shape `tfp256` already uses);
the resolver refuses them; the `f256`/`f512` suffixes are gone from the
lexer.

**A `flt32` literal carries at most 15 significant digits.** Its only
portable lowering rides a correctly-rounded double and truncates; decimal →
double → float equals direct rounding exactly when the decimal has ≤ 15
significant digits (53 ≥ 2·24+2, the double-rounding theorem), and beyond
that the value would be implementation-defined — which a literal must never
be. `flt64` literals are unbounded: LLVM's parser converts any decimal with
correct rounding directly.

## D-144 — `tbb` at run time: sticky, saturating, and the cast matrix — **SETTLED**

Cycle 0.9.5, making D-008 executable and closing the audit's third live rung
hole (the same-width int⇄tbb cast that could forge or launder taint through a
no-op fast path).

**Arithmetic is branch-free and total.** Any ERR operand yields ERR
(stickiness beats every identity: `ERR * 0` is ERR); overflow saturates to
ERR (add/sub by the sign trick, mul by `llvm.smul.with.overflow`, native at
every width per D-011); a result that lands ON the sentinel arithmetically IS
ERR by construction, no extra check. **Division and remainder by zero yield
ERR and continue** — the fail-operational row of D-007's table, the reason
the type exists — with the divisor replaced before the instruction so the
hardware can never fault. Lowered with selects, never branches: the cost and
shape are data-independent.

**ERR at a bare comparison traps** (`TBB_ERR`, −4100): a tainted value about
to steer control flow. `is_err(x)` is the look that does not trap; a `pick`
with an `ERR:` arm is the branch that handles it (its own compare is
deliberately unguarded — it IS the handler). Ordering on tbb was already a
compile error (D-093); D-008 §5's runtime-ordering text is annotated as
narrowed in the doc-sync.

**The cast matrix** *(leaving side amended at 1.3.2 — see the amendment
below)*. Per the settled `cast_from_tbb`/`cast_to_tbb` arms as landed at
0.9.5: every crossing is legal and carries its runtime semantics: `=>` traps
(−4100) where the value has no image — ERR leaving tbb, a
sentinel-or-out-of-range value entering — and `=>!` is the greppable
acceptance: entering saturates to ERR, leaving reads the raw carrier.
**tbb⇄tbb maps the sentinel across widths
in both spellings** — ERR is a state, not a number; widening must not
sign-extend it into a valid value and narrowing must not truncate it into
zero. float→tbb compares in double (one bound table serves both float
widths); NaN fails the ordered bounds and is out-of-range garbage like any
other.

### D-144 amendment — the leaving side (1.3.2 follow-up, user-settled)

D-195's amendment 3 gave `tfp` the rule that a cast OUT of the family traps
on ERR under BOTH spellings, and flagged the asymmetry with tbb's landed
`=>!`-reads-the-carrier for the user rather than changing 0.9.5 behavior
under a tfp decision. **The user settled it: tbb aligns — one rule, no
laundering path.** Leaving a twisted family:

- **The TAINT: ERR traps (−4100) under both spellings.** What `=>!`
  acknowledges is the VALUE's loss; ERR is not a value (D-008 §1), and no
  acknowledgment converts a taint. The struck carrier read crossed ERR as
  INT_MIN — recognizable, but unchecked, and one `is_err` short of honest.
- **The VALUE: classified by RANGE like every numeric pair**, which the
  0.9.5 arms never did — every int/float target was CAST_OK and the emitter
  resized the carrier raw, so `tbb64 => int8` truncated and `tbb32 =>
  uint32` sign-extended into huge positives, silently, under the CHECKED
  spelling. Now `tbb64 => int8`, `tbb32 => uint32` and `tbb64 => flt64`
  take the bang exactly as the plain matrix spells `int64 => int8`,
  `int32 => uint32` and `int64 => flt64`; fits (`tbb32 => int32`,
  `tbb32 => flt64`) stay `=>`. The prelude's four `tbbN:Hash` impls — the
  tree's only affected sites — now spell their guarded signed→unsigned
  crossing `=>!`, matching every neighboring impl.

The entering side and tbb⇄tbb are unchanged — entering's runtime
check-or-saturate IS the family's own discipline (the target has an
absorbing state; a plain target has none, which is why leaving is a
compile-time question). Proven by `tbb_cast_bang_trap` (the −4100 under the
bang), the `lossy_pair` rows in the frontend matrix test, and two
`NITPICK-TYPE-009` rejection rows.

**`ERR` takes the width of its slot** (`tbb64:e = ERR;` works; bare ERR with
no tbb context stays tbb32), from one shared sentinel table in the emitter —
no two sites can disagree about which bit pattern taint is.

**`?` on tbb stays refused**: OP_REFERENCE/D-008's promise of a tbb fallback
operator loses to D-099's one-wrapper rule — `?` takes a `Result` and nothing
else; the doc correction rides the 0.9.8 sync.

## D-145 — Ranges normalize half-open at construction; the ternary is lazy — **SETTLED**

Cycle 0.9.6, the two decisions the scalars-and-forms tail needed.

**Every range value is `{ T, T }`, half-open, from the moment it exists.** The
inclusive spelling `lo..hi` stores `hi+1` at construction; the exclusive
`lo...hi` stores `hi` (the spellings per LEXICAL §5.1 — `..` is the inclusive
one). No consumer ever asks which spelling built a range, which is the
blueprint rule applied to an object's lifetime: one representation, decided at
the boundary. Slicing normalizes identically — and until 0.9.6 it silently
treated BOTH spellings as exclusive, so `arr[0..2]` viewed two elements where
the language says three; the executed test now pins three. An inclusive range
ending at the carrier's maximum wraps under the +1 and the bounds guard traps
it — a loud outcome for a corner with no honest answer. **Floats may not form
ranges**: half-open needs a discrete successor, and a float has none.

**The ternary `is (cond) : a : b` evaluates ONLY the taken arm** — lowered
with branches through a slot, never `select`, because `select` evaluates both
arms and `is (b != 0) : a/b : 0` must not divide by the zero it just tested
for. The executed test's untaken arm divides by a runtime zero and does not
trap.

**Recorded with them, from the same subcycle's finds:** function types compare
STRUCTURALLY (`tt_func` interns the parameter window's start index — an
allocation artifact — so the annotation's spelling and the declaration's never
shared an id; nothing consumed function types until values of them existed);
the func type's return slot is the SUCCESS type, spelled exactly as a
declaration spells it, with every call — named or indirect — typing as
`Result<success>` by one wrap in one place.

## D-146 — The borrow discipline's four repairs — **SETTLED; F-1's matcher amended by D-223 (exact-type `dest_can_hold` becomes derivation-aware `can_connect`)**

Cycle 0.9.8, landing the audit's confirmed soundness holes (deep dive F-1,
F-2, F-4, F-6 — probes in `meta/roadmap/audit-0.8-close/probes/`, each now a
suite case).

**F-1 — rule B counts destinations, type-aware.** The stored value must be a
borrow, but the place it is stored through need only be a pointer whose
pointee can hold one. The repaired rule: a LOCAL-rooted borrow (a
frame-owned target — param-rooted borrows outlive the frame, D-138's
refinement) passed beside a non-borrow pointer destination **whose pointee
can transitively hold that borrow's own type** marks local destinations and
REFUSES parameter-rooted ones outright (`NITPICK-BORROW-002`) — the store
would outlive the frame and no mark could witness it. The type-awareness is
what keeps the compiler's own walker idiom (`f(@local_ctx, diags)`) ordinary
code: a DiagList has no slot that could ever hold an LlCtx. Struct fields
resolve through the checker's own resolver (Escape now carries it); an
`any->` slot matches every borrow; a value-held borrow (rule A's
conservative marking) has no frame pointee and does not trigger the
destination rule — rule A still owns its return-escape.

**F-2 — an expression-pick is as borrowy as its `give`s.** The arm walk
carries the verdict up instead of dropping it, so
`pass (pick … { give @x; })` is the dangling return it always was.

**F-4 — the taint tracks what it can see, and refuses what it cannot.** Only
a bare local Result is trackable, so only a bare local's `is_error` check
counts. A `.value` read through a member path or a parameter refuses
CONSERVATIVELY — bind the Result to a local and check that — matching the
untracked posture elsewhere. Before: a param-rooted read was invisible, and
checking `t.a` licensed reading `t.b.value`. The one documented exception
stands: a call-rooted temporary cannot be checked at all, and binding it is
the idiom.

**F-6 — exclusive pick arms are exclusive.** Every arm starts from the
pristine pre-pick state (as `if` always did); may/moved/freed gather in a
side accumulator and land once after the loop. Initialise-`fixed`-per-case —
the idiom `pick` exists for — is accepted again.

Ticketed onward with owners: F-3 (inner-block borrow deref'd after scope)
gates on lifetime intrinsics/stack coloring; F-5 (defer-named binding
invalidated after registration) lands with 0.10's real allocator, when
`dalloc` starts freeing.

## D-147 — The leading-digit rule — **SETTLED**

Cycle 0.9.9, at the user's request, after the fourth edit-build-fail cycle
caused by a variable name that was secretly a literal.

**Every numeric literal begins with a decimal digit `0`–`9`, and no
identifier ever does.** A value whose leading significant digit is a letter —
hex's `a`–`f`, ternary's `T`, nonary's `a`–`d` — takes a value-neutral
leading zero: `0FFhex`, `0Tt`, `0an`. The token class is decided by the
FIRST character alone.

Before this rule the split was decided from the RIGHT — scan the whole
alphanumeric run, strip the type suffix, strip the base suffix, test what
remains against the base's digits — and the letter-digit bases made whole
English words into numbers: `an`, `bn`, `cn`, `dn`, `ban`, `can`, `dan`
(nonary), `tt` (ternary), `chex` (hex). Each collision surfaced as a parse
failure lines away from the variable that caused it; the compiler's own
sources had already renamed every such variable once (0.7.3) and the traps
kept costing cycles anyway.

Two alternatives were weighed and refused:

- **A universal `value<T>` annotation** (`5<u8>`, `-45<tbb8>`) — proposed
  first, withdrawn when review found `<` ambiguous in expression position
  (`5<u8>` vs the comparison `5 < u8`), the same ambiguity that forced the
  turbofish for generic calls. It also left `b<n1>` still letter-leading at
  the lexer, and would have churned every literal in the tree.
- **Per-base prefixes** (`0x…`-style, considered once in `FORMAL_DRAFT` 02
  §2.4) — refused as a second spelling system beside the suffixes:
  meaning-by-context, the thing the blueprint philosophy exists to prevent.

The leading zero is not a prefix. It is a digit the grammar always allowed,
now required when the first significant digit is a letter — one rule,
already true of every other base, extended to cover the letter-digit ones.
The suffix machinery is untouched; `num_scan` gained a first-character
guard, and the seed's numeric path had gated on `isdigit()` from day one,
so seed and real lexer now agree by construction.

**The legacy C-style prefixes (`0x`, `0b`, `0o`, `0n`) are REMOVED by the
same decision**, not merely discouraged. They were retained "for C FFI
compatibility", but Nitpick never parses C headers, and two spellings for
one literal is the exact thing this decision exists to end. `0xFF` is now a
bad-digit error at the `x` (`NITPICK-LEX-003`) — a run that begins `0`–`9`
was meant as a number, so there is no identifier rescue. The only in-tree
uses were `derive.npk`'s FNV constants, migrated to `100000001B3hexu64` /
`0CBF29CE484222325hexu64`.

Consequences: `FFhex`, `an`, `ban`, `tt` are ordinary identifiers.
`tests/frontend/lexer_numeric.npk` locks both sides of the split, and
`tests/backend/programs/nonary_literals.npk` demonstrates the freed names —
`int32:dn = 0dni32;` is the decision in one line.

## D-148 — The value must fit: exact envelope, ranged literals — **SETTLED**

Cycle 0.9.9, found while probing D-147's migration of the FNV constants.
Two related holes, both the "a different number than the one written" class:

**Nothing checked a literal's value against its type, at any stage.**
`int8:a = 300i8;` survived the lexer, the checker, and every analysis, and
reached the backend, which emitted `i8 300` — malformed IR when llc catches
it, a silently wrapped constant when it does not. The same held for every
width, signedness, `tbb`, and `char`.

**The lexer's overflow guard was decimal-tuned and radix-blind.** It
compared the accumulator against `int64_max / 10` whatever the radix, so a
hex literal near 2⁶⁴ could slip under it and wrap: the FNV-1a offset basis
`0xcbf29ce484222325` compiled to −3750763034362895579 with no diagnostic —
`derive(Hash)` worked by accident. Meanwhile `uint64`'s actual maximum was
refused by the same guard. A coin decided which.

The repairs:

- **The scanner detects overflow exactly** (`numeric.npk`): non-balanced
  bases test `acc > (MAX − d) / radix` before each step — exact, since their
  digits and accumulator are non-negative. The balanced bases carry negative
  digits (|d| ≤ 4), so both ends are guarded with a four-count margin: the
  envelope's outermost four values per side are refused rather than risking
  a wrapped intermediate.
- **The literal envelope is signed 64-bit, and values outside it are
  CONSTRUCTED, not spelled.** This is a language rule, not an implementation
  apology: `uint64` values above 2⁶³−1 have no literal, the same as `int64`'s
  most negative value, the balanced envelope edge, and the wide integers
  (`int128`+) — one rule for every extreme. `derive.npk`'s basis is now
  `(0u64 - 3750763034362895579u64)`, exact by D-037's defined wrap, with the
  arithmetic shown at the definition. Revisiting full-range `uint64`
  literals would be a new decision; it is decided NO here because carrying
  bit-patterns in a value slot poisons every downstream consumer that
  compares payloads (const evaluation would need unsigned-aware folding),
  and a loud refusal with a one-subtraction spelling is strictly safer.
- **`lit_ranged` in the type checker** (`NITPICK-TYPE-031`): every integer,
  `tbb`, and `char` literal — suffixed or contextual (D-092) — is checked
  against its type's exact range at the literal's own span. `tbb` ranges are
  symmetric with ERR excluded (D-008 §1). `char32` checks Unicode scalar
  validity: at most U+10FFFF and never a surrogate. A balanced literal's
  negative value is refused into unsigned types.
- **A signed width's most negative value has no decimal spelling** (`-` is
  an operator, not part of the literal; magnitude 2^(N−1) exceeds the
  positive bound). It is spelled in a balanced base — `0b4bni8` is −128 —
  or constructed. Uniform with `int64`, which never had a decimal MIN.
- **`num_width_bits` gained its missing `char` rows.** Numeric char-suffixed
  literals had been typing as ZERO-BIT chars since the suffixes landed —
  latent until the range check made it loud, and the probable cause of the
  0.9.5 note where `4000000000000tbb64` "hit TYPE-007"; that literal accepts
  cleanly now.

`tests/types/rejection/literal_range.npk` locks seven refusals;
`tests/frontend/lexer_numeric.npk` locks the exact guard on the basis
constant that used to wrap.

## D-149 — The FFI barrier is the process boundary; `extern` declares a driver interface — **SETTLED; D-163 reuses the retired `never fails`**: a driver method may always fail, so `raw`/`drop` are never licensed on a driver call (falls out of D-163 rules 3–4), and when the Bridge lands (1.1) `VerifyNeverFails` becomes the one node for the one word**

Post-0.9, at the user's direction. Generalizes D-055; supersedes the contract
half of D-002; supersedes the C-pointer half of D-066.

**In-process FFI does not exist in Nitpick.** All foreign code — vendor
libraries, GUI toolkits, codecs, anything not compiled by `npkc` — runs in a
separate, supervised **driver process** behind the D-055 architecture
(`meta/roadmap/audit-0.8-close/driver_architecture_plan_v3.md`). The FFI
barrier and the process boundary are the same line.

### Why

1. **D-002 could only wrap the failures C admits to.** A `-1` return becomes
   an errored `Result`; a segfault, a scribbled heap, or a hang inside a
   linked blob cannot be wrapped, intercepted, or routed through `failsafe`
   — the exact hole the zero-dependency rule exists to close. Process
   isolation turns every one of those into A VALUE: a closed socket, a
   reaped child, an errored `Result<T>`.
2. **The verified boundary becomes the process image.** D-055 said it for
   GPU blobs: no amount of verification makes a linked vendor blob
   analysable. The sentence never mentioned GPUs. Under this decision the
   TCB that Astrée analyses IS the process; the driver is outside it,
   unverified because its failure is contained and observable.
3. **A present-but-discouraged unsafe path is the adversary's first link.**
   The threat model is an attacker chaining minor quirks; hassle-based
   deterrence also fails under ordinary deadline pressure. So the unsafe
   path is not made painful — it is made MECHANISM-LESS.

### The enforcement is structural, not hortatory

Nitpick binaries are statically linked with no libc and no dynamic loader:
there is no `dlopen`, so foreign code can only enter through the link line —
and the compiler owns the link line. `check_zero_dependency` (D-011) already
fails the compiler's own build BY NAME on any undefined symbol outside the
allowlist; `npkg` (1.2) applies the same closed world to every user program:
**only `npkc`-produced objects plus the audited runtime are linkable.** There
is no flag that relaxes this. The residue is named honestly: `sys` remains
(the kernel is mode-switched, not linked — it cannot load code into the
address space, and it is the floor the driver IPC itself stands on), and
`wildx` means a saboteur could hand-copy machine code into JIT pages — no
language stops assembly-level intent, and that is nobody's path of least
resistance.

### `extern` keeps its spelling and becomes the driver-interface declaration

The block form is unchanged and the word finally means what it says: outside
the process. The string names the driver; the functions are its methods.

```nitpick
extern:"cuda_driver" = {
    func:load_kernel = KernelHandle(int8[]:image);
    func:dispatch    = NIL(KernelHandle:k, int8[]:args);
};
```

**Stub generation is COMPILER lowering, not a macro library**: the backend
lowers `DeclExternBlock` to Bridge stubs — marshal into the sealed ring,
dispatch with the mandatory deadline, unmarshal or return the error — the
same way D-055 makes `#[gpu_kernel]` a codegen target. It needs the type
table (for the interface hash) and the executor (for the suspension), which
a macro cannot reach cleanly.

### The wire has a universal failure convention, so D-002's contracts die

`fails on <expr>` / `with errno` / `never fails` existed because C has no
universal failure convention and the mapping could not be inferred. The wire
HAS one: every dispatch returns status plus payload, and timeouts, driver
death, and protocol violations arrive as uniform negative codes in the D-141
error space (numbers assigned when the Bridge lands, 1.1). Every driver
method is `Result`-native by construction — nothing per-function to write,
nothing for a reviewer to audit. The contract grammar remains PARSED (the
frontend never restricts, D-085); the checker refuses a contract on an
`extern` declaration with this decision named, landing with the Bridge work.

### The wire vocabulary is closed

Fixed-width scalars, POD structs of them, and sized byte payloads — copied
out of shared memory BEFORE validation (v3's untrusted-input rule; the
shared region is an I/O device, not memory). Nothing address-shaped crosses
in either direction. Consequences:

- **`void` and `void*` are now valid NOWHERE** — the extern-only carve-out
  (TYPE_REFERENCE pointer rule; D-005's phrasing "not valid outside `extern`
  blocks") closes, and a whole class of unanalysable types leaves the
  language.
- **`opaque struct:Name;` in an `extern` block is a typed wire handle**: an
  opaque value with a generation counter, minted and honoured by the driver,
  type-safe on the Nitpick side, dead after a driver restart by
  construction. D-066's form survives; its C-pointer reading does not.
- **D-004 rule 4** (a borrow may not cross an `extern` call) stays, with a
  stronger reason: the other side is a different address space, and only
  copies cross.

### Validity is enforced at two layers

- **Wire-level, at connect**: magic, protocol version, and an **interface
  hash computed from the `extern` block's own signatures** — a driver built
  against a stale interface is refused at handshake, loudly, before any
  call; never a silent type confusion.
- **Nitpick-level**: the generated stub implements the `Driver` trait
  carrying D-055's four obligations — every dispatch has a deadline, no
  partial results, the driver is a supervised child, and the registry is
  `failsafe`-reachable so controlled shutdown tears drivers down in order.

### The honest cost

A dispatch is microseconds where a call is nanoseconds. The ring + bulk
region + batched dispatch design serves the workloads that matter (GPU and
GUI are batch-shaped); for chatty tiny-call workloads the answer is *write
it in Nitpick*. Safety outranks performance, and the pressure lands exactly
on the code that should be out of process or rewritten.

### Landing

The backend's `extern` rung message names this decision now. The Bridge,
the stub lowering, the C SDK header (the wire protocol and ring layout as a
C header plus a reference event loop — the CONTRACT is the protocol, not a
language binding, so later Rust/other SDKs are alternative implementations
with zero compiler work), and the wire-conformance suite are 1.1 work
(map row 1.1.8); the `npkg` closed-world link rule is a 1.3 obligation. The
C reference driver and conformance harness are buildable out-of-tree at any
time — the v3 POC (18/18 kernel checks) is their seed.

## D-150 — The heap: out-of-band slabs, canaries, and the request rules — **SETTLED**

Cycle 0.10.0. The bump-never-free floor is replaced by the real allocator in
`bootstrap/runtime/npkrt.ll` — hand-written IR under the D-015 TCB
discipline, exercised by the compiler's own execution on every build.

**Shape.** Fourteen size classes (16…2048 bytes payload) in 64 KiB,
64 KiB-aligned chunks — block→chunk is one AND — with per-chunk bitmaps, and
a direct-mmap large path above 2048. Per-class partial/full chunk lists give
O(1) transitions; a sorted chunk table and a sorted large table answer "is
this pointer mine" in O(log n) **before anything is dereferenced**.

**The two structural choices, made against the prototype's design:**

- **Out-of-band metadata.** The prototype's free-list-in-payload scheme
  (`slab_alloc.cpp`'s secret-XOR'd next pointers) was consulted and REJECTED:
  there, a use-after-free corrupts the allocator's own control data and
  detection is probabilistic. Here control state — bitmaps, tables, list
  links in chunk headers — is never inside user payload, so a UAF can touch
  only payload and canaries, and **double-free detection is deterministic**:
  the slot's free bit is already set. (Defense in depth — D-119 already
  refuses double-free of tracked bindings at compile time; this covers
  laundered aliases and fabricated pointers.)
- **Validate before dereference.** `dalloc`/`ralloc` prove membership via
  the tables before reading a header. A garbage pointer is
  `npk_trap(-4102)`, never a wild load: the allocator must not be the thing
  that segfaults.

**Canaries.** Every block: `[ size | magic ]` in 16 bytes (payloads stay
16-aligned; the size bounds `ralloc`'s copy — 0.7.3's discipline kept).
Magics are secret-keyed (getrandom at first allocation) and address-keyed,
one role constant each for live/freed/large/chunk/guard. In a slab the next
block's header is its neighbour's overrun canary and a 16-byte tail guard
closes the chunk, checked on every free in it; large blocks carry footer
guards; a freed slot keeps a FREED magic re-verified when the slot is handed
out again, and a chunk watermark traps frees of never-allocated slots. NOT a
CRC — the spec's "8-byte CRC32 header" was wrong three ways (total_audit
C-1) and both reference passages now describe this scheme.

**The request rules** (each a trap, never UB — codes in the D-141 runtime
region):

- `-4102` HEAP_INTEGRITY: double-free, null/misaligned/foreign pointer,
  corrupted header, torn guard, scribbled freed slot. `dalloc(NULL)` traps:
  `alloc` never returns null and Nitpick has no free(NULL) cleanup idiom.
- `-4103` HEAP_OOM: mmap failure. The trap path allocates nothing — the C-3
  obligation stated at the allocator boundary.
- `-4104` HEAP_BAD_REQUEST: negative size; `calloc` count×size overflow
  (the multiply is CHECKED — a wrap is an undersized allocation wearing a
  plausible size); `ralloc(p, 0)` (C's implementation-defined footgun,
  refused: freeing is spelled `dalloc`); zero or non-power-of-two alignment.
- `alloc(0)` is legal: a real, unique, freeable 16-byte block — trapping it
  would make every `alloc(count * elem)` with a legal zero count a landmine.

**`aalloc(size, align)` is the fifth native builtin**, for alignments above
the default 16 (`#[align(N)]` types will lower onto it): power-of-two only,
served by the large path with the alignment contract asserted at the
production site. D-149's "the four above are the WHOLE allocator API" line
becomes five.

**Single-threaded at this rung**, stated rather than implied: programs have
one thread until 1.1, and the heap's lock discipline lands with 1.1's
executor work. `memmove` in the runtime was also repaired in passing — it
forwarded to `memcpy`, which corrupts overlapping upward moves.

`tests/backend/programs/heap.npk` proves recycling, ralloc content/free,
calloc zeroing, the large in-place/move paths, and both `aalloc` paths;
`heap_double_free` / `heap_calloc_overflow` / `heap_ralloc_zero` lock the
three trap classes to their codes.

## D-151 — `<wild-live>` is the allocator's own tables; the exit rule is real — **SETTLED**

Cycle 0.10.1. The K-semantics exit rule (CONTROL_REFERENCE §4.6) stops being
a promise about code that does not exist.

**No second bookkeeping structure.** The live-set IS the 0.10.0 tables: a
wild allocation is counted by walking the chunk bitmaps (slots below each
watermark whose free bit is clear, wild-role headers only) and the large
table. Nothing can drift from the ground truth because there is only the
ground truth. Both views — `wild_live_count()` and the exit check — are
allocation-free over preallocated state, so they hold inside a degraded
`failsafe` (the C-3 discipline).

**The wild/internal role split is the spec's own scoping.** §4.6 names
"live `wild` or `wildx` memory" — the WILD regime. So the builtins
(`alloc`/`calloc`/`ralloc`/`aalloc`) stamp a wild-role magic and are
counted; runtime-internal storage (string bodies, argv, file buffers)
stamps an internal role and is NOT in the set — it is managed-regime
storage whose RAII arrives with the managed lowering, reclaimed wholesale
meanwhile by `wild_release_all()` or process death. `ralloc` preserves a
block's regime across a move. Without this split every string-touching
program would false-trap at exit for storage the program never manually
managed.

**"Successful" is load-bearing.** The check runs on `exit 0` only. A
failure exit keeps its code: hijacking an error report with a leak trap
would destroy the error being raised, and error paths carry no cleanup
obligation — the defer-does-not-run-on-trap reasoning (D-014) applied at
exit. A leaking `exit 0` routes `-4105` to `failsafe`, which may call
`wild_release_all()` (drops every chunk and large mapping, both regimes,
allocator stays usable) and exit positive.

**Two flags' worth of re-entry discipline, one flag.** `npk_trap` sets
in-failsafe before running the handler: `failsafe`'s own exit skips the
leak check (the check runs once, at the program's exit), and a trap raised
*inside* `failsafe` — the handler double-freeing, say — exits 70 directly
instead of recursing into the handler forever. The recursion guard predates
nothing: it was a real hole until this cycle.

**The compiler compiles under its own rule.** `src/main.npk` and the three
tools call `wild_release_all()` before their successful exits — the honest
*(1.5.1b step 5, 2026-09-04: THE STATEMENT AFTER `wild_release_all()` IS
`exit`, refused otherwise as TYPE-062 (S-27; ratified 2026-09-04 as D-255). Three unit tests released the
heap and then RETURNED from `main`; the day `List<T>` began to own, their
scope-exit drops ran over unmapped memory, the runtime refused the free, and
the refusal's own trap route died on the same released heap — an
uncontrolled stop, the class D-013 exists to prevent. Nothing can run after
the release but the exit itself; a measurement that must follow it goes into
`exit`'s operand, evaluated after the call. `release_then_exit.npk`.)*
run-once shutdown — and 69 executed tests gained the same line before
their success exits (the heap/leak demonstrations excluded: their explicit
frees ARE the assertion). The seed learned the two builtins
(`wild_live_count`, `wild_release_all`); both are ordinary runtime-table
entries in all three signature copies.

**One registry mechanism, three clients** (the audit's unification ask):
the sorted fixed-stride, mmap-grown, preallocated, `failsafe`-walkable
table is the mechanism; the allocation tables are its first client; the
stream registry (IO_REFERENCE §10) and D-149's driver registry are
specified as the next two, not re-inventions.

`leak.npk` (the set counts exactly; strings absent), `leak_trap.npk`
(-4105 fires), `leak_cleanup.npk` (failsafe releases and exits positive),
and `oom.npk` (-4103, allocation-free) lock the behavior.

## D-152 — `arena<T>` and `Handle<T>` lower; the operation set as built — **SETTLED**

Cycle 0.10.2. The D-003 mechanism is real: graph-shaped and cyclic data
lives in an arena behind generation-checked handles and is dropped
wholesale. Three design calls, each argued:

**Creation is `arena_make(cap)`, not `arena<T>.alloc(N)`.** The carried-over
spec used ONE NAME for two operations — `.alloc` on the type created the
arena, `.alloc` on the value allocated a slot — a direct blueprint
violation. `arena_make` is type-directed the way an unsuffixed literal is
(D-092): the `arena<T>` annotation names the element, and the compiler
passes the element's stride (size rounded to alignment, floored at 8 so a
free slot holds its freelist link) to a stride-erased runtime.

**`get` copies; `put` joins the set.** D-017 left `get`'s borrow status
open. Decided NO: a borrow-returning `get` would be a RETURNED borrow,
which D-004 refuses for every function in the language (`BORROW-001`) — an
arena exemption would be meaning-by-context. So `get(h)` returns
`Result<T>` by value and `put(h, v)` writes back, both failing a stale
handle with **error `-4106` in `Result.err`** — a condition the program
handles, never a trap. The set: `alloc() -> Handle<T>` (bare — its only
failure is an OOM trap), `get`, `put`, `free` (`Result`), `reset`,
`destroy` (bare `NIL`). Operations dispatch on the receiver's type before
any impl lookup — no user method can shadow the allocator's own verbs.

**Generations carry a parity discipline.** Live slots hold EVEN
generations, freed slots ODD; a handle only ever carries the even
generation `alloc` issued, so a stale or forged handle can never name a
freed slot — `at` demands exact equality and the freed slot is odd. Reuse
bumps odd back to even; a slot reaching 0xFFFFFFFE is RETIRED, never
reused: the counter cannot wrap. `reset` bumps every live slot odd in one
pass. Handles are INDICES (16 bytes, `{ i64, i32 }`), so slab relocation
on growth is invisible to them — and `haspt` stays false on `Handle<T>`
deliberately: crossing scopes and living in struct fields is the design.

**`destroy` consumes the arena**, enforced by the binding analysis exactly
as `dalloc` is — gated on the receiver TYPE being `arena<T>`, never on the
method's spelling (the prototype's name-matching `KNOWN_DEALLOCATORS` is
the counterexample). Use-after-destroy and double-destroy are compile
errors (`MOVE-002`). The slab and generation array are WILD-role heap
blocks: an un-destroyed arena is a countable leak the D-151 exit check
names — "drop the arena wholesale" is now the type's mechanical contract.

Element alignment is capped at 16 (the slab's own); `shared_arena<T>`
refuses naming 0.10.4 and `atomic<T>` naming 1.1 (closing total_audit
B-3's nameless refusals). Chained access through an arena embedded in a
struct works today as ordinary member-place addressing — no UFCS machinery
involved. `tests/backend/programs/arena.npk` runs the cyclic graph,
growth, staleness, reuse, reset, chaining, and the leak-checked wholesale
drop; `types/rejection/arena.npk` and `analysis/rejection/arena_destroy.npk`
lock the refusals.

## D-153 — The executor frame allocator, distinct and fixed — **SETTLED**

Cycle 0.10.3. The allocator D-034 actually needs, built where the heap
lives and consumed by 1.1 — SEPARATE from `arena<T>`, because the
concurrency audit's sharpest catch (total_audit B-1) is that the surface
arena cannot serve coroutine frames: fixed slots and generation-checked
indices on one side, per-function variably-sized blocks that
`@llvm.coro.begin` needs as raw pointers on the other.

**The fixed interface** — what C-7's coroutine lowering emits against:
`npk_frame_exec_new() -> ptr`, `npk_frame_alloc(fe, size, align) -> ptr`,
`npk_frame_free(fe, frame)`, `npk_frame_drain(fe)`,
`npk_frame_exec_destroy(fe)`. Runtime-internal only: no keyword, no
builtin, no surface type.

**The shape**: 64 KiB chunk list, single-threaded and ZERO-ATOMIC (tasks
are pinned, D-032 — pinning is exactly what buys the zero-cost path);
completed frames return to a free list bucketed by EXACT size — the
coroutine workload is one frame size per async function, recurring — with
LIFO reuse; frames larger than a chunk take dedicated heap blocks (flag
bit in the header); `drain` retires everything at once by resetting the
bump into the chunks the executor already owns. Frame headers carry size
plus a secret-keyed state magic: double-free and foreign pointers trap
`-4102` like every heap-integrity failure. The executor struct and chunks
are wild-role blocks — an un-destroyed executor is a leak D-151's exit
check names. Alignment caps at 16.

**Runtime families with no surface syntax get IR unit tests**: the harness
gained a `runtime` stage running hand-written `.ll` drivers under
`bootstrap/runtime/tests/` against the same npkrt.o everything links —
`frames.ll` proves exact-size LIFO reuse, out-of-order frees,
size-bucket separation, drain-into-owned-memory, the oversize path, chunk
growth under 200 frames, and destroy returning every byte (wild-live
count zero, and exit 0 through the D-151 check). This stage is how later
runtime-only families (0.10.4's shared_arena internals, 0.10.5's wildx)
get execution coverage before their surface arrives.

## D-154 — `shared_arena<T>`: alloc carries the value; reservation-based growth — **SETTLED**

Cycle 0.10.4, completing the arena family. D-017's smaller contract
(alloc/get/destroy — no free, no reset) is now mechanical, and the open
items it left are decided:

**`alloc(v)` carries the value.** With no `put` in the contract, the slot
must be written at allocation — and that is the concurrency design, not a
workaround: a slot is written once, before its handle can escape, and is
IMMUTABLE afterwards, so concurrent `get` is race-free with no per-slot
synchronization at all. The synchronizing edge is the handle transfer
itself (1.1's channels are SeqCst, D-016 — stated as the obligation it
is). The two arenas' `alloc` arities differ deliberately: two types, one
discipline each, is D-017's own frame.

**`get` copies** — the plan's "borrow yes, matching 0.10.2" inverted on
contact with 0.10.2's actual decision: D-152 decided NO because a
borrow-returning `get` is a returned borrow (`BORROW-001`), and that
argument is type-independent. Both arenas hand out values uniformly.

**Growth is reservation-then-publish, lock-free.** A grower reserves a
capacity range with one atomic `fetch_add` on `cap` — racing installers
receive DISJOINT ranges and cannot collide — builds the chunk against that
base, and publishes with a CAS push onto the chunk list. Chunks tile the
index space and NEVER move (D-017's decisive hazard, removed by
construction). Chunk sizes are geometric — each new chunk carries the
current capacity in slots, capped at 65536 — so a big arena is ~a couple
dozen chunks (the plan's fixed-vs-geometric question, decided geometric
capped). A bumped index in a reserved-but-unlinked range spins on the slot
walk, bounded by its installer's progress; 1.1's concurrency review owns
preemption liveness. All cross-thread state is SeqCst.

**Generation zero is shared_arena's constant, and arena<T> now starts at
2.** Nothing frees in a shared arena, so its handles carry generation zero
forever; `arena<T>`'s virgin slots are promoted to 2 at first issue
(found by the cross-arena test: virgin arena handles used to carry zero
too). The split makes a wandering single-threaded handle refusable as
stale (`-4106`) at a shared `get` — cross-arena confusion between two
arenas of the SAME kind remains a documented limitation, mitigated by
element types.

The surface value is one pointer (shareable by reference — the value IS
the reference); `destroy` consumes the binding (`MOVE-002`); an
un-destroyed shared arena is a wild-role leak (D-151). The plan's
"lowers atomic<int64> minimally" dissolved on inspection: the bump lives
INSIDE the runtime as atomic IR instructions, so no surface atomic type
was needed — `atomic<T>` remains wholly 1.1's (its refusal names that).
`shared.ll` proves the non-moving property the only way it can be proven —
a raw slot pointer taken before growth, dereferenced intact after 10000
allocations across ~10 chunk installs — plus the forged-index null and a
clean destroy; `shared_arena.npk` proves the surface, the geometric
growth, and the cross-arena staleness under the D-151 exit check.

## D-155 — The wildx W^X state machine is built — **SETTLED**

Cycle 0.10.5, closing D-035's three open deliverables and the cycle. The
guarantee is CONTAINMENT (the contents of generated code are unverifiable,
outside Z3 and K like the FFI barrier; the container is not), made structural
rather than hoped-for.

**The lifecycle reduces to state, mostly existing.** Only two transitions
needed new analysis; the rest fall out of machinery already built:

- **write-after-seal** (`NITPICK-WILDX-001`) and **execute-before-seal**
  (`NITPICK-WILDX-002`) are the two the seal state watches. Two columns in
  the binding-state machine, because the prohibitions want opposite merges: a
  page sealed on ANY path may not be written (`sealed`, unions — a
  prohibition), and a page may be executed only if sealed on EVERY path
  (`sealed_firm`, intersects — a permission). The branch that seals on one
  arm only is refused for BOTH, which is correct — an unknown seal state is
  safe for neither.
- **seal-after-free** — the exact use-after-free D-035 found in the
  prototype's `jit.npk` (free on write failure, then seal the freed page) —
  is `NITPICK-MOVE-002`, because seal reads a freed binding. **double-free**
  and **use-after-free** likewise fall out of the move machinery (a free is a
  move, D-065). **no-live-pages-at-exit** falls out of the `<wild-live>`
  registry (D-151): wildx pages are wild-role and counted, so a leaked page
  traps at exit.

**The runtime is real W^X.** `wildx_alloc` maps three pages (PROT_NONE guard
| RW code | PROT_NONE guard), the kernel placing them (ASLR); `wildx_seal`
`mprotect`s the middle RW→RX one-way — never W+X together; `wildx_call` is
the boundary, an indirect call into code the verifier does not model;
`wildx_free` unmaps. A secret-keyed header validates the pointer at seal and
free. `tests/backend/programs/wildx_jit.npk` writes `mov eax, 42; ret` into a
page, seals, calls, gets 42, and frees before exit — the whole lifecycle
executed.

**`--extra-picky=no-wildx`** is a build mode (`NITPICK-WILDX-003`), separate
from `no-wild` because manual memory and executable memory are different
risks. It scans the tree after a clean front half and refuses every `wildx`,
giving the high-assurance build that excludes runtime code generation
(D-035's certification note). The verification boundary and that note are
written into VERIFICATION_REFERENCE §6.4.

`tests/analysis/rejection/wildx.npk` locks the four refusals (the jit.npk bug
among them); `tests/accept/moves.npk` gains the correct lifecycle as the
positive case.

## D-156 — The symbol scheme: `npk.<module>.<spelling>`, quoted, hash-free — **SETTLED**

Cycle 1.0's opening decision (OPEN_DECISIONS C-1). D-064 §6 settled THAT
mangling is reversible and hash-free; D-108 settled the frontend identity
(the source spelling — `Container<int32>`). This settles the LINK level.

**The scheme.** Every emitted definition takes a quoted symbol built from
three parts joined by dots:

- functions: `@"npk.<module>.<function>"`; instantiations append the D-108
  spelling whole — `@"npk.geometry.extract<int32>"`.
- named aggregate types: `%"npk.<module>.<Type>"`;
  `%"npk.geometry.Container<int32>"`.
- vtables (D-158): `@"npk.<module>.vt.<Type>.<Trait>"`, in the impl's module.

> **Amendment, found at 1.0.5c's opening and owned by 1.0.5b: A METHOD'S SYMBOL
> MUST NAME ITS IMPL.** The list above says nothing about methods, so the
> implementation took the only reading available — a method's symbol is its
> module and its own name — and **the compiler has been emitting invalid LLVM
> for an ordinary program ever since.** Two impls of one trait on different
> types both render `@"npk.<module>.say"`, and `llc` refuses the module:
> `invalid redefinition of function`. A second shape is worse: two impls
> INHERITING one trait default collide the same way, and because 1.0.4 emits a
> default once per implementing impl from ONE declaration index, no map keyed by
> declaration can separate them.
>
> The qualifier is the pair D-158 already says a vtable is per — **(target,
> trait)**:
>
> - `@"npk.<module>.<Target>:<Trait>.<name>"` — a trait impl's method
> - `@"npk.<module>.<Target>.<name>"` — an inherent impl's method
> - `@"npk.<module>.<name>"` — a free function, unchanged
>
> Hash-free, path-free and source-derived are all preserved; those three are why
> the scheme exists. An inherent method and a trait method of one name on one
> type now differ, which they must — `find_method` lets both exist and refuses
> only the ambiguous CALL.
>
> **This decision also contradicts itself, and the contradiction is what let the
> gap through.** The paragraph on module names says a symbol collision "is caught
> at emit by the generalized collision check (below)" — and the paragraph below
> says that check was RETIRED at 1.0.1, leaving the case to surface "as an `ld`
> duplicate symbol". Nothing catches it. Worse, the case the decision worried
> about was two SAME-BASENAME MODULES, a narrow one; the collision that actually
> exists is between two impls in ONE module, which it never considered.
> `check_no_duplicate_symbols` is 1.0.5b's instrument and is what the forward
> reference should have pointed at all along.
>
> **Why no test caught it**: nothing that reaches the backend has ever
> implemented one trait twice. Three files do and all three are rejection tests
> that stop at the checker; a fourth is an acceptance test, which is checked and
> never emitted.

LLVM's quoted identifiers admit any byte, so the spelling embeds verbatim —
`<`, spaces, commas and all. Reversal is inspection: strip `npk.`, split at
the first dot, the rest is the source spelling. No hash (D-064 §6's
argument: an auditor reading a disassembly must map a symbol back to source
without a table that may not have survived), and no path (D-078: emitted
bytes must not vary with the build tree).

**Module names are NOT required program-unique** (refined during 1.0.0
implementation — the loader dedups by FILE, not name, and forbidding two
`util.npk` in different directories would be a real restriction on users).
The canonical name is the file BASENAME (= the `mod:` name, D-088;
path-free, D-078). What IS guaranteed program-unique is the SYMBOL: two
modules of the same basename each declaring a member of the same name would
render one symbol, and that narrow case is caught at emit by the
generalized collision check (below). Same-basename modules with disjoint
member names coexist freely — the common case, and the reason the compiler's
own two `prelude`-named modules build without complaint.

**The reserved namespaces do not collide by construction.** The runtime
floor's `@npk_<name>` symbols are single-segment with underscores; scheme
symbols always contain a dot. `main` keeps `@main` (the linker's entry);
`failsafe` keeps `@npk_failsafe` (the runtime's declared hook). Both are
the two functions outside the `Result` regime (D-013), and they are also
the two outside the naming scheme — one rule's boundary, stated twice.

**Comptime values** encode as their canonical display in the spelling
(`Buffer<1024>`), exactly as D-108 interns them — one spelling, both uses.

**Linkage.** Instantiated definitions emit `linkonce_odr`: identical
specializations fold when multi-object linking arrives (1.2's world);
today's whole-program emission already dedups via D-108, so the linkage is
declarative until then. Non-generic definitions keep today's linkage.

**The type-duplicate check was kept at 1.0.0 and RETIRED at 1.0.1.**
Removing it during 1.0.0 produced an emit-side type error on a two-module
program, read at the time as the interned type table merging two same-named
types. 1.0.1's investigation (a second session, verified here) found that
diagnosis WRONG: `tt_struct`/`tt_enum` intern on the DECLARATION (D-090, by
construction), and two `Local`s reach emission as two entries with two
bodies — the only collision was the pre-scheme spelling `%Local`, which this
decision's `%"npk.<module>.<Type>"` dissolves exactly as planned. The
confusion came from the FRONT HALF: the struct-literal typer chose the
expected type by BARE NAME, so a `b.Local{…}` literal in an `a.Local`
position was built as an `a.Local` (fixed as D-162). With that closed, two
same-named types in two modules are two declarations, two entries, two
symbols — and they link and run (`same_name_two_modules` emits
`%"npk.same_name_a.Local"` / `%"npk.same_name_b.Local"`, exit 13).
**Interning by qualified name is NOT the follow-up**: it would move identity
from the declaration to a string, weaker than D-090. The one narrow case
without a frontend check is a function-symbol collision between two
same-basename modules, which surfaces as an `ld` duplicate symbol.

**Proven at 1.0.0**: the compiler self-compiles with 1549 module-qualified
function definitions and the stage-1/stage-2 fixpoint holds byte-identical —
the deterministic, path-free property the 1.3 fixpoint requires. The value
delivered is reversible module-qualified names, generic-instantiation naming
(1.0.2), 1.3-separate-compilation readiness, and — once D-162 landed —
same-named-type coexistence.

**D-163 (1.1.0)**: where a FUNCTION TYPE appears as a generic argument, the
instantiation's mangled name is built from the type's display, which now
carries the `never fails` contract — so a family instantiated over a marked
function type and one over its unmarked twin get distinct symbols, matching
their distinct interned types. Unmarked function types display exactly as
before, so every pre-D-163 symbol is unchanged.

> **Annotation, 2026-09-01 (1.4.7, user-ratified): THE `<module>` HALF OF A
> METHOD SYMBOL IS THE IMPL'S.** The amendment above names the
> `<Target>:<Trait>` half and never says which module qualifies it. The
> vtable row already says "in the impl's module", and that is the rule for
> every method an impl provides — its own members and the trait defaults it
> inherits, sync bodies and coroutine frames alike — with every call site
> deriving the module from the impl that answers (`fb22bb6`:
> `ExprEmitter.cur_impl` on the definition side, `impl_decl_for` at the call
> sites). Until then the module was that of whichever DECLARATION's body was
> being emitted — an own member's for an own method, the TRAIT's for an
> inherited default, the trait's method for a call through a bound — which
> coincided with the impl's for every impl in the tree, because every impl sat
> beside its trait. The tree's first foreign-module impl (D-229 stage 2's
> `impl:Sink:Writer`) failed to assemble in one build. Every pre-existing
> program emits byte-identical IR under the rule; `impl_foreign.npk` pins it.
>
> **And a limit of the scheme, DECIDED rather than deferred (same date, same
> ratification):** the `<Target>` half is the type's bare display, so two
> same-named structs from two modules, both implemented for one trait in a
> THIRD module, render one symbol. Coherence cannot see it (two declarations,
> D-090) and nothing links it: `llc` refuses the redefinition and the
> harness's `check_symbols_unique` names it. The scheme stays hash-free and
> source-derived — the two properties it exists for — and a program that
> needs both impls writes each beside its own type, where the module halves
> differ.

## D-157 — Object safety: `Self` nowhere but the receiver — **SETTLED**

OPEN_DECISIONS C-2, a safety hole: `check_object_safe` inspected only the
RETURN node, so `func:eq = bool(Self:self, Self:other)` passed — and behind
a vtable the erased second argument is read at the wrong layout. The
extended rule:

> **`Self` may appear in exactly one place: as the receiver's type (the
> first parameter). Any other appearance — a later parameter, the return,
> or NESTED anywhere in a type tree (`Optional<Self>`, `Self[]`,
> `Handle<Self>`, `Self->`) — makes the trait not object-safe.**

The walk is over the full type tree of every parameter and the return,
with fuel, refusing on any `Self` found outside the receiver position. No
exceptions — not even `Handle<Self>`, whose SIZE survives erasure (16
bytes regardless of element): the uniform rule is one rule, and a
size-based carve-out would be the context-dependence the blueprint
refuses.

> **Implemented at 1.0.5.** `type_mentions_self` walks generic arguments,
> pointees, elements, function-type parameters and returns, and `dyn` operands.
> The receiver may *be* `Self` and may not *contain* one.
>
> **Rules 1 and 2 do not both fire for one mistake.** With no `self` at all
> there is no receiver for "nowhere but the receiver" to be measured against, so
> the parameter scan is skipped once rule 1 has reported — otherwise
> `int32(int32:k, Self:self)`, which is `self` in the wrong POSITION, drew a
> second diagnostic saying it "mentions `Self` in parameter `self`, and only the
> receiver may", of a parameter named `self`. The return is still scanned, since
> where `self` sits says nothing about what the method gives back, and the trait
> is refused by rule 1 regardless — only a line of noise is dropped, never a
> refusal.
>
> **Fuel exhaustion fails CLOSED**: the walk reports `found` as well as `exhausted`, so a tree it
> could not finish refuses the trait, and `TYPE_INTERNAL` is reported alongside
> because that is a compiler defect rather than a fault in the program — a
> safety check that answers "nothing found" for the one input it could not read
> is the failure mode this shape exists to prevent.

**Rule 3 is one statement** (the three contradictory phrasings the grammar
audit found, folded here): *no generic methods — type parameters and
comptime parameters alike — because a vtable slot holds one address and a
generic method is a family of them.* TRAITS_REFERENCE §4.2 is restated to
these three rules exactly.

## D-158 — `dyn` dispatch: declaration-indexed vtables, own methods only — **SETTLED; the TABLE gained a slot 0 at D-183's 1.2.4** (the concrete type's drop — methods sit at declaration index + 1)

OPEN_DECISIONS C-3, with the prototype as the behavioral oracle
(type_checker_call.cpp's dyn path, confirmed against traits_oop_specs).

- **Reachable methods**: those declared by the traits the `dyn` NAMES —
  own lists only, in bound order. **Supertrait methods are not reachable**
  through a `dyn` of the subtrait (the subtrait's vtable holds its own
  declaration list; no super-vtable was built). This is a decided NO, not
  a gap: code needing the supertrait's methods constructs `dyn Super` from
  the concrete value, where the vtable exists. The prototype behaves
  identically.
- **Ambiguity**: two named traits declaring one method name is a
  compile-time ambiguity error naming both traits (`TYPE_AMBIGUOUS_METHOD`
  — the same rule concrete method dispatch already has). No ordering
  tiebreak: an order-dependent resolution is meaning-by-position.
- **The vtable** is per (impl, trait): entries in TRAIT DECLARATION ORDER
  (slot = declaration index — stable, source-derived, hash-free like
  D-156). Entries point at **per-impl adapter thunks**: the caller always
  passes the data pointer plus the arguments; the thunk — compiled against
  the concrete type, which it knows — loads the by-value receiver and
  calls the real method. This is what lets impl methods keep their natural
  `Point:self` signatures while the erased caller passes one pointer.
- **Return typing**: the trait signature's return, `Result`-wrapped like
  every call (D-013). Borrow returns need no dyn-specific rule: BORROW-001
  refuses returned borrows language-wide, so the prototype's NITPICK-053
  case cannot arise here.

> **Dispatch implemented at 1.0.5; the vtable and its thunks at 1.0.5c.**
> `find_method` had no `TY_DYN` path at all, so every method call on a `dyn`
> fell through to the impl-table walk — which looks up by the RECEIVER's type
> and finds nothing, making the construct unusable end to end rather than merely
> unlowered. `find_in_dyn` searches the named traits' own member lists in bound
> order; the supertrait exclusion falls out of "own lists only" and is pinned by
> a test rather than assumed. The answer is the TRAIT's declaration, which is
> the D-163 hook.
>
> **The tables landed at 1.0.5c** and a `dyn` runs: one per (impl, trait),
> entries in trait declaration order, each an adapter thunk that loads the
> by-value receiver from the data pointer and calls the real method — which is
> what lets an impl keep its natural `Point:self` signature while the erased
> caller passes one `ptr`. The slot is the declaration index and the vtable word
> is the bound's canonical position, so both are compile-time constants and no
> runtime table is consulted.
>
> Two things the decision did not say, found by building it. **The table's NAME
> needs a pre-pass**: a construction site knows the concrete type and its traits
> and nothing about the impl, and this decision's own scheme names the table in
> the IMPL's module — so `build_vtmap` walks every impl before any body is
> emitted, the same answer `build_modmap` gives. And **a non-object-safe trait
> gets no table**, deliberately: its `dyn` was refused at every use (D-157) so
> none is ever loaded, while building one would mean thunking a method that
> returns `Self` and would turn a legal program into a rung.
>
> **A family impl has no single table** — its target is a template, so each
> instantiation would need its own — so a `dyn` over a trait implemented by a
> generic family is a named rung rather than a silent omission.

## D-159 — Multi-bound `dyn` ABI: data + one vtable word per trait, canonically ordered — **SETTLED**

OPEN_DECISIONS C-4, contradicted three ways in the tree. Settled as
type_trait.npk's N+1 words:

- `dyn A` = `{ ptr data, ptr vtable }` — 16 bytes, the spec's fat pointer.
- `dyn A & B & C` = `{ ptr data, ptr vtA, ptr vtB, ptr vtC }` — (N+1)×8.
  `types.npk`'s flat 16-byte interning was the bug; the size is computed
  from the bound count.
- **Bounds are CANONICALLY ORDERED at interning** (sorted by trait name),
  so `dyn A & B` and `dyn B & A` are one type with one layout — and every
  vtable word has a compile-time-known slot.
- **Widening (`dyn A & B → dyn A`) is a value rebuild**: copy the data
  word and the retained traits' vtable words into the smaller shape —
  positions statically known via the canonical order, no runtime tables,
  no prefix/subview scheme. O(retained words) at the widening site,
  nothing anywhere else.

> **Layout and ordering implemented at 1.0.5; widening at 1.0.5c.** `tt_dyn`
> computes `(N+1) x 8`, and `dyn_sort_bounds` canonicalises the window before
> interning, so `dyn A & B` and `dyn B & A` collapse to one type. This needed
> `name_lt`, the tree's first string ordering — named for its domain because it
> compares bytes as int8, which is lexicographic only while they are ASCII, as
> every identifier is.
>
> **Widening landed at 1.0.5c** and is the value rebuild this decision describes:
> the data word and the retained traits' words copied into the narrower shape,
> every position read off the canonical order at compile time. No runtime table,
> no prefix or subview scheme, and the cost paid once at the widening site.
>
> **The sort key's tie is UNREACHABLE — settled at 1.0.5b by measurement, not by
> plumbing.** The concern was that "sorted by trait name" leaves two same-named
> traits from different modules with equal keys, tie-broken by declaration index,
> which shifts under an unrelated edit. Before building the module-qualified key
> that would fix it, the premise was tested: **two same-named traits cannot both
> be named in one file at all.**
>
> - A glob import of both is `NITPICK-RESOLVE-008`, the loader's ambiguity — the
>   same rule `same_name_two_modules` relies on, and whose comment says so
>   ("two `Local`s in one scope would be the loader's ambiguity").
> - The qualified form (`dyn a.S & b.S`) does not parse: a dotted path is not a
>   type spelling. Measured — six `NITPICK-PARSE-001`.
>
> So no `dyn` can carry two traits whose names are equal, and the
> declaration-index fallback is a defensive tiebreak nothing can currently
> reach. It stays, because a total order should be total whether or not the tie
> arises.
>
> **This reopens if a MODULE path becomes a type spelling.** D-164 added a
> dotted suffix in type position at 1.0.6c and it does NOT reopen this, checked
> at 1.0.6d: that production projects an associated type from a TYPE, and a
> module is not a type — `dyn_a.S` reports "there is no type named `dyn_a`". The
> loader also still refuses two modules exporting one name, for non-glob imports
> as well as glob ones.
>
> So the dependency stands, narrowed to what actually triggers it: **whoever
> makes a MODULE nameable in type position must give this sort a
> module-qualified key in the same change**, or `dyn a.S & b.S` and
> `dyn b.S & a.S` become two types with two layouts again. Nothing tracks that
> today, which is why it is recorded on the decision rather than in a plan that
> would outlive its cycle.

## D-160 — Associated types: kind, in-trait resolution, impl binding, projection — **SETTLED**

OPEN_DECISIONS C-5. Descoping was refused: TRAITS_REFERENCE's own
`Iterator` is the spec's flagship trait, and the generic stdlib (1.0.7
onward) needs it. What lands at 1.0.6:

- **`TY_ASSOC`** — a type kind carrying (trait decl, name). Inside a trait
  body, a bare assoc name (`Item` in `func:next = Item(Self:self)`)
  resolves to it, exactly as `Self` resolves today.
- **Impl binding substitutes.** `assoc:Item = int32;` in an impl binds the
  projection for that impl; checking the impl's methods against the trait
  substitutes bindings the way `Self` substitutes already. Defaults
  (`assoc:Error = string;` in the trait) are inherited unless overridden
  (D-028's surviving half).
- **External projection `T.Item`** (in generic code, `T` a parameter
  bounded by the trait) rides the EXISTING dotted-path type grammar if the
  parser already admits it, else is spelled `Iterator.Item`-style through
  the trait — settled during 1.0.6 against the parser as it stands; either
  spelling resolves through the bound. No new token, no new node kind
  beyond the type kind.
- **Object safety**: a method whose signature mentions an assoc type makes
  the trait NOT object-safe (same erasure argument as `Self` — the
  caller cannot know the projection's layout behind a vtable). `dyn
  Iterator` is therefore refused; iterate through generics instead.

> **Implemented at 1.0.6 EXCEPT the projection, whose premise was false.**
> `TY_ASSOC`, in-trait resolution, impl binding with substitution, inherited
> defaults and the object-safety exclusion all landed, and the spec's flagship
> `Iterator` typechecks for the first time.
>
> **The external-projection clause does not survive contact.** It says `T.Item`
> rides the existing dotted-path type grammar "if the parser already admits it"
> — measured at 1.0.6, it does not (six `NITPICK-PARSE-001`) — and the fallback
> named in the same sentence, `Iterator.Item`, is dotted as well, so "no new
> token, no new node kind beyond the type kind" is unachievable for EITHER
> candidate. Neither authority has ever had a projection: `TRAITS_REFERENCE`
> §2.3 shows declaration and binding only, and the prototype has
> `AssociatedTypeDecl` and `AssociatedTypeBinding` and no projection node at all.
> So this clause was this decision's own addition rather than a restatement of
> anything, and it is reopened as **OPEN_DECISIONS C-20**.
>
> It is gated on **C-21**, a pre-existing defect found while testing the
> alternative: a generic trait named as a generic function's BOUND makes the
> trait's own parameter stop resolving. Until that works, "declare the trait
> generic instead of using an assoc" is not a route the projection can be
> weighed against.
>
> **One rule this decision did not state, added at 1.0.6**: an impl that omits a
> required binding is reported ONCE, by the completeness walk that names the
> associated type — the signature mismatch it also causes is suppressed, because
> it points at the method rather than at the omission.

## D-161 — Impls over generic families: the target between generics and trait — **SETTLED; superseded by D-171 (the segment/shape rule is retired — every impl names its target explicitly)**

OPEN_DECISIONS C-6 — and the audit's premise half-dissolved on probing:
**per-instance impls (`impl:Box<int32>:Sized2`) already parse, resolve, and
check end-to-end today.** Only the FAMILY form was inexpressible, and the
fix is one contained production change, not a grammar rework:

```ebnf
ImplDeclaration ::= "impl" ":" GenericList? ImplTail "=" "{" ImplBody "}" ";"
ImplTail        ::= Type (":" Type)?     ; target, optional trait
                  | ":"? Type            ; blanket: generics with no target
```

Concretely in `p_parse_impl`: after the generics window, parse a type; if a
second `:`-type follows, the first was the TARGET and the second the trait
(the family form, `impl:<T>:Box<T>:Sized2`); otherwise the first is the
trait and the target is empty (the blanket form, `impl:<T: P>:Loggable`,
unchanged). The inherent family (`impl:<T>:Box<T>`) falls out. The AST
carries both slots already (D-031's layout, untouched); no token changes;
the D-085 fear — parser rework cascading into the AST — does not arise,
which is why this is decided rather than raised.

> **Amendment SETTLED at 1.0.4b; FULLY IMPLEMENTED at 1.0.4c.** The
> disambiguation needed no new rule: **D-111 already requires a blanket impl to
> name a trait**, so after a generics window the SEGMENT COUNT decides — one
> segment is the blanket form's trait, two are a family impl's target and
> trait. None of the three heuristics floated below was needed. Family impls
> parse, resolve, check, dispatch, emit and run as of 1.0.4b.
>
> **A consequence found at 1.0.7, stated so it is not mistaken for a gap in
> the implementation:** the segment rule IS the "no inherent families"
> candidate listed below, adopted without saying so. `impl:<T>:List<T> = { … }`
> has one segment after the generics window and is read as a blanket impl of
> a trait named `List<T>` (refused: `NITPICK-TYPE-012`, not a trait). The
> original resolution's "the inherent family falls out" does not hold under
> the amendment. So a generic struct cannot carry inherent methods; they go
> through a trait — and a trait method cannot take `Self->`, so no method of a
> generic struct can mutate it. Raised as **T-7** at 1.0.9 with a
> recommendation (the blanket form takes an explicit target,
> `impl:<T: P>:T:Loggable`, and every impl reads by one rule); the first
> generic collection (1.0.7) spells its mutators as free generic functions
> over `List<T>->`, which is `TokenList`'s own shape.
>
> **Both clauses 1.0.4b left owed are met as of 1.0.4c**, and both landed on
> the better of the two branches this decision allowed:
>
> - **Overlap is refused at the impl.** `check_coherence` compares
>   DECLARATIONS, not interned targets — `Box<T>` and `Box<int32>` are
>   different types but one declaration, which is what makes the instance a
>   member of the family — and reports `NITPICK-TYPE-013` at the second impl
>   with both named, **with no call required**. Declaration order does not
>   change the refusal or the wording; two family impls of one trait
>   (`Box<T>` beside `Box<U>`, which intern differently) are the same overlap.
>   **Blanket-versus-concrete is untouched and stays accepted** — §2.6 permits
>   it because D-111 defines the lookup ORDER, and nothing defines one for
>   family versus instance; giving it one would be the specialization D-064 §7
>   rejected.
> - **`#[derive]` on a generic subject synthesizes the family form**, which is
>   this decision's preferred outcome rather than the interim refusal-by-name:
>   `#[derive(Eq)]` on `Box<T>` now writes `impl:<T>:Box<T>:Eq`. **No bound is
>   synthesized**, amending this decision's "the element bound mirrors the
>   derived trait" below: the generated body compares with the OPERATOR, never
>   by calling a trait method, so `T: Eq` would refuse `Box<int32>` for failing
>   to implement a trait its own `int32` field does not implement either.
>
> Closing it required an unrelated hole in 1.0.2b's generic lowering: body
> ANNOTATIONS were substituted, body EXPRESSIONS were not, so a derived
> `self.v != other.v` reached lowering carrying a bare `T`. `ll_type` now
> resolves a parameter to its argument while a specialization is emitted.
>
> The original amendment note follows, kept because it records why the first
> attempt failed.
>
> **Amendment owed, found at 1.0.4 (implementation).** The production as
> written above — "after the generics window, parse a type; if a second
> `:`-type follows, the first was the TARGET; otherwise the first is the trait"
> — **breaks the blanket form**. `impl:<T: Printable>:Loggable` has no type in
> the target position at all, so parsing one consumes `Loggable` as the target
> and the expected second `:` is not there; implemented literally it turned a
> working blanket impl into a parse error (caught by a probe, reverted
> immediately). The family and blanket forms are genuinely AMBIGUOUS at that
> position: `impl:<T>:X` could mean "family impl on `X`" or "blanket impl of
> trait `X`", and nothing local distinguishes them. The amendment must choose a
> rule and say why — candidates: decide by whether the type mentions a declared
> parameter (`Box<T>` does, `Loggable` does not — cheap, but makes the meaning
> depend on the type's SHAPE); require the trait segment for the family form
> (`impl:<T>:Box<T>:Trait` only, no inherent families); or give the blanket form
> a distinguishing spelling. **This is 1.0.4b's opening decision**, not
> something a lowering subcycle should settle. Per-instance impls
> (`impl:Box<int32>:Trait`) are unaffected and work today.

**Resolution**: the impl's generic parameters scope over its target and
trait exactly as a function's scope over its signature; an instantiation
`Box<int32>` matches the family impl by unification against the target
spelling, with the impl's bounds checked at the D-108 bound-judgment pass.
**Specificity**: a per-instance impl and a family impl for the same
(type, trait) are an OVERLAP and refused (TRAITS_REFERENCE §4.1's one-impl
rule, unchanged — no specialization, per D-064 §7).

**Derive on generics** synthesizes exactly the family form — `impl:<T>:Box<T>:Eq`.
This originally read `impl:<T: Eq>:Box<T>:Eq`, "the element bound mirrors the
derived trait"; **1.0.4c corrected that to no bound at all**, because the
generated body compares with the OPERATOR and never calls a trait method, so a
mirrored bound would refuse `Box<int32>` for failing to implement a trait its
own `int32` field does not implement either. The interim refusal-by-name this
paragraph mandated was never needed: synthesis was available at 1.0.4c, and a
probe confirmed the hand-written family impl type-checks before the generator
was changed to emit one.

**Default methods** (folded in from grammar #6): a trait method with a body
is inherited by every impl that omits it; `find_method` falls back to the
trait's default when the impl lacks the name — concrete receivers at
1.0.4, `dyn` via the vtable filling the slot with the default's thunk.

> **[D-258, 2026-09-05]** The no-bound story ends: a derived impl over a
> generic subject carries the derived trait as a bound on exactly the
> parameters its body reaches, and the body reaches a parameter through the
> trait's method — the operator form admitted programs the emitter could
> not lower (OPEN_DECISIONS DEF-16), and a family impl's bound is enforced
> where the impl is used (D-256).

## D-162 — A struct literal is typed by the declaration its name resolves to — **SETTLED**

Cycle 1.0.1, from a second session's investigation of 1.0.0's mis-diagnosed
finding (D-156's amended paragraph), verified against the code before
adopting.

`Point{ x: 1i32 }` carries a NAME and nothing else (D-129). Since 0.6.7 the
typer let the EXPECTED type win whenever its name matched the literal's — the
stated intent was "when it names the same DECLARATION", because a generic
literal (`Container<int32>{…}`) has no way to carry its arguments except
through the context asking for it. But the comparison was on the NAME
(`type_name(want) == name`), not the declaration.

**The rule.** A struct literal's type is the declaration its name resolves
to IN THE SCOPE WHERE THE LITERAL IS WRITTEN. The expected type contributes
only generic ARGUMENTS, and only when it is an instance of that same
declaration (compared by `type_decl_of`, which a generic instance carries in
slot `a`). An expected type naming a DIFFERENT declaration of the same name —
a same-named struct from another module — does not retype the literal; it is
built as what the author wrote, and the mismatch is refused where the value
is used (`NITPICK-TYPE-007`), with D-129's field checks having run against
the right declaration.

**Why a decision and not a bug fix.** The behaviour was language-visible: a
literal meant one type in one position and another type in another — the
blueprint rule's first facet (meaning must not change with context) broken
outright. And it SILENCED a safety check: the omitted-field refusal
(`NITPICK-TYPE-027`, D-010's definite assignment for aggregates) never ran
against the declaration the author wrote when two modules disagreed about a
type's shape. `same_name_two_modules` (npkc-only, since the throwaway seed keys structs by
name and cannot represent two same-named types) proves the positive: a_go
builds this module's one-field `Local`, not the other module's, and the
program runs. The rejection corollary — `b.Local` where `a.Local` is
expected refuses TYPE-007, a partial refuses TYPE-027 against the right
declaration — was verified against the real compiler; its two codes are
already covered by other suite cases.

**What it does not change.** `Container<int32>{…}` still takes its arguments
from context (by declaration now, not name); UFCS, member access, and
instantiation dedup (D-108) are untouched — they already compare
declarations or interned indices.

**Landed alongside** the retirement of `check_duplicate_type_names` (with the
literal hole closed, it had nothing to guard) and the repair of a
spanless-diagnostic bug: `emit_line` in `main.npk`/`tools/check.npk` relayed
`srcmgr_path` on a `file = -1` span and the failure was dropped, so the
retired guard had been exiting 1 while printing NOTHING. `emit_line` now
renders `<no span>` — a refusal a reader cannot act on is worse than no
check.

## D-163 — `raw` and `drop` are licensed by `never fails`; a `Result` is never discarded without a keyword; `never fails` is checked — **SETTLED and FULLY IMPLEMENTED (1.1.0–1.1.2): the licence refuses with `NITPICK-TYPE-042`; the `Result` discipline has no silent sink**

Raised by the user post-1.0.0, in two parts. *"The purpose of `raw` was to skip
checks for a function that cannot return an error"* — and the compiler has
never asked whether the function can. And: *"the whole purpose of `drop` was to
use with functions that do not return a useful value … just because a function
doesn't return a useful value doesn't mean an error didn't occur in flight."*

### The gap

Three ways to lose an error, none of them checked:

- **`raw e` / `_! e`** reads `.value` without a check. The checker verifies that
  `e` is a `Result<T>` (`type_unwrap`) and nothing else; the emitter is an
  unguarded `extractvalue …, 0` (`emit_raw`). The prototype did the same.
- **`drop e` / `_? e`** discards value and error together. Same check, and the
  emitter evaluates the operand and yields nothing.
- **A bare expression statement** — `f();` or `discard(f());` — is typed "for
  its own sake" (`check_stmt`) and its type is never examined, so a `Result`
  vanishes with no keyword at the site. `SAFETY_ARCHITECTURE.md`'s "the caller
  *must* explicitly handle the `Result<T>`" was never enforced.

The language already has the word for the property all three depend on.
`never fails` (D-002) is "an explicit, greppable assertion that the function
cannot fail … required rather than implied." It was attached to `extern`
functions only, and D-149 retired that use.

**Measured in this compiler** (heuristic; re-measured by the instrument):
5,885 `raw` sites in `src/`, `lib/`, `tools/`, **262–300 on a callee that
`fail`s** — `ast_id_at` ×92, `symtab_get` ×32, `graph_at` ×28, `tt_get` ×26,
each on an out-of-range index, each continuing on node 0. 2,005 `drop` sites,
**741 on a callee that can fail** — among them the driver's own stages:
`drop check_module(…)`, `drop resolve_module(…)`, `drop collect_impls(…)` in
`pipeline.npk`, so an internal defect inside the type checker is discarded and
the pipeline proceeds to the emitter as if the stage had passed. That is a
silent success in the compiler's own control flow — what D-002 and D-149
forbid a foreign call to produce. `tests/` carries 932 + 144 more, and 186 bare
statements.

Every other claim of this shape is checked. `_~` on a parameter is an error to
read (D-089). `never fails` on an extern had to be written, never implied. An
impl exceeding its trait method's `acquires` bound is refused (D-056). These
were the last claims the compiler took on faith, and the TOS framing was the
only thing behind them. A discipline is what fails under pressure; D-007 says
so of `ok()`, and 1,000 sites say so of this.

### The decision, in nine rules

**1. `never fails` is a contract on any function.** Ordinary functions, trait
method signatures, impl methods, `comptime` functions, builtins, and function
*types*. It sits in the contract position — after the parameter list, where
`requires` / `ensures` already go:

```nitpick
pub func:ast_kind_at = int32(Ast->:ast, int32:i) never fails { … };
pub func:p_error = NIL(Parser->:p, string:code, string:message) never fails { … };
trait:Shape = { func:area = flt64(Self->:self) never fails; };
func int32(int32) never fails:f = @square;       // a function TYPE carrying it
```

The keyword pair already lexes (D-002). The parser production exists
(`p_parse_failure`) and becomes reachable from `p_parse_contracts`, as a new
`VerifyKind` — `VerifyNeverFails` — in the contract window
`FunctionDecl.contracts` already has. No AST layout change; no lexer change.
`fails on` is **not** extended: a Nitpick function says how it fails with `fail`.

**2. `never fails` is checked.** In a function so declared the checker refuses,
naming the statement:

| refused | because |
|---|---|
| `fail c;` | returns an error |
| `relay e` / `_^ e` | returns the callee's error — refused even when the callee is itself `never fails`: the dead branch is refused rather than reasoned about, so the rule has no sub-case (write `raw`, licensed there) |
| `return Result{…};` | the literal can carry an error; `pass v;` is the success spelling and cannot |
| `requires` / `ensures` / `limit<Rules>` on the function or its parameters | a contract that can be violated at runtime is a failure channel. C-15/C-16 decide *which*; a function with a precondition can refuse its inputs under either answer and is not never-failing |
| the `async` modifier | the executor can fail a task independently of its body (deadline, D-056) |
| the clause on `main` / `failsafe` | they return no `Result`; the claim is vacuous |

> **[D-241, 2026-09-03] The contract row above is RETIRED.** It delegated the
> violation channel to C-15/C-16, and D-220/D-221 chose the trap route — the
> channel this same list permits two lines below (`?!`). `never fails` may
> carry `requires`, `ensures` and `limit<Rules>`; the row's other entries
> stand. The table is kept as written, D-085's pattern.

Permitted, because none of them *returns* an error: `?|` with a default, `?!`
(a trap is Layer 3, a different channel, and already greppable), `drop` and
`raw` of `never fails` callees, `is_err` branches, `pick` with an `ERR:` arm.

**The check is syntactic and modular** — a walk over statement kinds in one
body, the exact shape of the existing `main`/`failsafe` refusals (`fn_terminal`
in `check_fail`, `check_return`, the `relay` arm of `type_of_expr`): an
`fn_never_fails` flag on the typer, read in the same three places. No
interprocedural analysis, no fixpoint — every callee's contract is *declared*,
which is what makes the guarantee compose across function pointers, trait
objects, and separate compilation, and keeps the checker itself something
Astrée can read.

**3. `raw` is licensed only by `never fails`.** The operand of `raw` / `_!`
must be a **call** — direct, UFCS / method, trait-dispatched, `dyn`,
function-pointer, or builtin — whose resolved callee carries the contract.
Refused otherwise: a callee without it (the message names the callee, its
declaration line, and the honest spellings — declare it `never fails` if it
cannot fail; `?!` if this call must succeed; `relay` / `?|` / `is_err`
otherwise); a binding (`raw r` — the language already has the checked `.value`
read, D-007; a second spelling of it would be two mechanisms for one job); any
non-call expression.

**4. `drop` is licensed the same way, and is the `void` call.** The operand of
`drop` / `_?` must be a call whose callee is `never fails` **and whose success
type is `NIL`**. `drop f();` then means exactly what it was for: *call a
function that has nothing to return and cannot fail*. Refused otherwise:

- a callee that can fail → `relay f();`, `f() ?! c;`, or `f() ?| NIL;` (rule 6);
- a callee with a value → the value is not "dropped", it is *discarded*:
  `discard(raw f());` — two words, two claims, both checked (the call cannot
  fail; the value is deliberately unused), the D-089 spelling for a value
  nobody reads. (**Settled with the user.** Weighed: letting `drop` swallow a never-failing
  value too. Rejected — it puts "I don't need the value" and "this is a `void` call" on one
  word, and 90 sites in `src/` is the whole cost of keeping them apart.)
- a binding or non-call expression.

**Exception — the spawn form.** `drop work()` where `work` is `async` and not
awaited is the spawn construct (D-058; D-062 gives it lexical lifetime). An
`async` function can never be `never fails` (rule 2), so the licence does not
apply to it — **and its error is not discarded either**: the task's `Result`
error is delivered to the enclosing scope's D-062 join. **Settled with the
user, implemented by 1.1 (C-7 / C-9):** the join relays the **first child
error, verbatim (D-080), after every child has finished**, as the enclosing
`async` function's own error; a task that was wound up by the join's deadline
reports its wind-up code the same way. A spawned task's error is observable or
the program does not compile. That is
structured concurrency's error rule, and it is the natural completion of
D-062's lexical task lifetime. CONCURRENCY_REFERENCE §2.2's "result discarded"
becomes "value discarded; error joined".

**5. Trait conformance.** An impl of a trait method declared `never fails` must
itself be declared `never fails` — the direction of `NITPICK-LOCK-002`: the
implementation may not claim less than the trait promised. An impl may be
`never fails` where its trait is not; the guarantee is then visible on the
concrete receiver only. **A `dyn` call is licensed by the trait's declaration**,
never an impl's — the vtable hides which impl runs. `#[derive]`-generated impls
carry whatever the trait declares, and their bodies must satisfy rule 2.

**6. A `Result` is never discarded without a keyword, and the value-less
statement forms are a closed list.** An expression statement's expression must
have type `NIL`, and must be one of:

| statement | meaning |
|---|---|
| `drop f();` | the `void` call — `f` is `never fails` and returns `NIL` |
| `relay f();` | run `f`; its error, if any, returns to the caller |
| `f() ?! c;` | run `f`; its error, if any, traps |
| `f() ?| NIL;` | run `f`; its error, if any, is deliberately ignored — the one explicit swallow, written as what it is: a default of nothing. **Settled with the user** as the sanctioned spelling; an auditor greps `?| NIL` |

Refused in statement position: a bare call (`f();` — a `Result` with nothing
said about it, `TYPE_RESULT_DISCARDED`); any expression of a non-`NIL` type (a
value with nowhere to go — `discard(…)` if that is intended); `raw f();` (it
would be a second spelling of `drop f();`); and `discard(e)` where `e` is a
`Result` (`discard` takes a value, never an outcome — rule 4's spelling is
`discard(raw f())`). D-060's closed expression list gets its statement-side
counterpart, for the same reason: a reader answers "what can stand here" by
looking, not by remembering.

**7. A `defer` body decides nothing about the function's outcome.** `fail` and
`relay` are refused inside a `defer` body — D-014's model is cleanup on an exit
already decided, and the backend would re-run the defer frames from inside one.
A cleanup call that can fail is handled in the body with `?!` (a cleanup
failure is a `failsafe` event — "a failed close is reported, never swallowed"),
an `is_err` branch, or the explicit `?| NIL`. `drop` in a defer is licensed as
anywhere else.

**8. Function types.** `func T(P…) never fails` is a distinct interned type. A
`never fails` function is assignable to either spelling; a may-fail function
only to the unmarked one — the mark *adds* a guarantee, so it flows one way.
Identity includes it: `func_types_equal`, interning, and the D-156 mangled name
where a function type appears as a generic argument.

A **flag in the interned type's flags slot, not a kind** — stated against
`TY_FUNC_VARIADIC`'s reasoning, because the two differ in what ignoring the
field does. Ignoring the variadic bit accepted wrong arity. Ignoring this flag
treats a never-failing function as an ordinary one, which *loses a licence and
never grants one* — the safe direction. Its only readers are the licence and
the assignability check, and both read it on purpose.

**9. Builtins declare theirs too.** `BUILTIN_REFERENCE.md`'s table gains a
`fails` column; `gen_tables.py` emits it into `builtins.npk`; the licence reads
it. The prose already says `string_from_bytes` and `path_exists` never fail;
the implementing subcycle audits every bare-name builtin against its IR body,
not its prose — in particular whether `string_concat`, `int_to_string`,
`string_slice` have any failure besides OOM (which traps, D-150, and is
therefore not a `Result` error), and which "→ void" builtins (`dalloc`,
`wildx_free`, `wild_release_all`) are `never fails` because their failures
trap. `to_cstring` fails on an interior NUL; everything touching a descriptor
may fail.

### What `raw` and `drop` mean after this

Neither is an opt-out. `raw` is a checked, **zero-cost** unwrap: the compiler
proves the check redundant, and the zero-cost path is exactly the never-failing
set, which is where the performance argument for `raw` ever applied. `drop` is
the `void` call, and says so. The `Result` discipline then has **no silent
sink** — every way past a possible error is spelled and means one thing: `?| d`
(a visible default), `?! c` (trap), `relay` (propagate), `is_err` / `ERR:`
(handle). Nothing expressible is lost: the old `raw f()` on a may-fail `f` was
`f() ?| <zero of T>` with the default unwritten; the old `drop f()` was
`f() ?| NIL;` with the swallow unwritten. An unwritten default is precisely what
explicit-over-implicit forbids, and both spellings exist for anyone who meant
them.

The spellings stay. `raw`, `_!`, `drop`, `_?` mean what they always meant, in
every context; what changes is that the compiler now refuses the contexts where
the missing check was load-bearing. `SAFETY_ARCHITECTURE.md`'s "Escape Hatches"
list drops `raw`; the TOS system keeps `wild`, `wildx`, `#wild_ptr`, `=>!`, `?!`.

### D-149 is unaffected and needs no special case

A driver-interface (`extern`) method may always fail — timeout, driver death —
so D-149's refusal of the contract on an `extern` stands, and neither `raw` nor
`drop` is ever licensed on a driver call; that falls out of rules 3 and 4. When
the Bridge lands (1.1) and `DeclNeverFails` / `DeclFailsOn` (the extern failure
slot) are retired, `VerifyNeverFails` is the one node for the one word.

### Codes

Six, in the type stage, centralised in `type_codes.npk`:

| code | fires on |
|---|---|
| `TYPE_RAW_UNLICENSED` | `raw` / `drop` whose operand is not a call to a `never fails` callee (the message says which operator and what the callee lacks) |
| `TYPE_NEVER_FAILS_CAN_FAIL` | a `never fails` body containing `fail` / `relay` / `return`, a contract, `async`, or the clause on `main` / `failsafe` |
| `TYPE_NEVER_FAILS_IMPL` | an impl method weaker than its trait's declaration |
| `TYPE_NEVER_FAILS_ASSIGN` | a may-fail function where a `never fails` function type is expected |
| `TYPE_RESULT_DISCARDED` | a `Result` in statement position with no operator, `discard(Result)`, or a non-`NIL` expression statement |
| `TYPE_DEFER_EXIT` | `fail` or `relay` inside a `defer` body |

(`check_codes_tested` demands a case for each; `check_codes_centralised`
forbids the literals elsewhere.)

### Always on, not `--extra-picky`

`SAFETY_ARCHITECTURE.md`'s own standard: *"absolute by default, suspended only
through a construct an auditor can search for."* Every `--extra-picky` rule adds
pedantry beyond what safety requires and none gates a safety property; placing
this one there would make the default build the one that trusts the author.

### Migration (two sweep subcycles; numbers are the heuristic's)

- **~868 functions gain the clause** (728 under `raw` ∪ 144 under `drop`).
  Rule 2 checks every one: a clause on a function that contains `fail` is
  refused, so a wrong claim cannot land. Most of the parser family's may-fail
  status is transitive from a few accessors (`p_peek` relays `tokenlist_at`
  after bounds-checking it) — the sweep resolves those at the **root**, with an
  in-range accessor or one `?!` on the impossible branch, and 226 `drop p_*`
  sites become licensed without being touched.
- **~262 `raw` sites and ~741 `drop` sites are rewritten**, each a latent bug:
  `relay` where the enclosing function returns a `Result` and the failure
  should reach the caller (the driver's stage calls; an out-of-range index is an
  internal defect that should surface as a diagnostic, not as node 0); `?!`
  where the failure is an invariant violation with no caller able to act;
  `is_err` where it is meaningful. Never `?| NIL` / `?| <zero>` as a mechanical
  replacement — that is the old behaviour with the swallow written down, and
  the point of the sweep is to stop continuing past failures.
- **~90 `drop` sites** on never-failing value-returning callees become
  `discard(raw f())`; **188 bare statements** (2 in `src/`, 186 in `tests/`)
  gain their keyword.
- **~98 builtin sites** follow the column. `tests/`: ~3,500 `raw` + ~280 `drop`
  sites, ~1,070 on may-fail helpers (which collapse to a few dozen callees),
  plus the rejection cases for the six codes.
- **The seed** (`bootstrap/generator/`): `parse.py` accepts `never fails` in the
  contract position of a function, a trait method, and a function type, and
  does **not** set `has_contract` for it — `check.py` rung-refuses every
  contract as 1.3 work, and this one changes no lowering. `gen_tables.py` emits
  the builtin column.
- **Performance**: unchanged where `raw` / `drop` survive; one predictable
  compare-and-branch where `?!` or `relay` replaces them on a may-fail callee,
  which the ABI decision (D-084, "never an exemption from returning `Result`")
  already accepted — its obligation to measure SROA folding across the inlining
  boundary still stands.
- **The fixpoint**: the clause changes no emitted byte, so stage-1/stage-2
  equality is a property to assert. The rewrites change bytes only where a
  branch now exists.

### Rejected alternatives

- **Infer never-fails by whole-program analysis, no declarations.** Implicit: a
  `fail` added deep in a callee silently changes which distant sites are
  legal, with the error far from the cause. Non-modular: function pointers and
  `dyn` could never be licensed. A fixpoint to verify instead of a walk over
  statement kinds. Kept only as the throwaway *measurement*.
- **`drop` relays (or traps) the error instead of being licensed.** Hides an
  early return (or a trap) behind a word that says "drop" — two spellings of
  `relay`, one of them misleading. The bypass is written or it does not exist.
- **`--extra-picky` only.** Inverts the table.
- **Keep `raw` / `drop` unchecked and add checked siblings.** Two unwraps
  differing only in whether the compiler checks the claim — the two-mechanisms
  violation — and it leaves the unchecked one as the adversary's first link
  (D-149's reasoning for *mechanism-less*).
- **Remove `raw`; `?!` everywhere.** Loses the zero-cost path and plants ~5,500
  trap sites that can never fire — noise an auditor must discount.
- **Bare `T` returns for never-failing functions.** D-114 refused two calling
  conventions; D-013's universal `Result` is blueprint facet 1.

### Amendment status (recorded at adoption, D-163)

The amendments below are the doc-sync obligation this decision carries, and
**1.1.0 made the pass**: everything that LANDED there — the contract and its
body check (rules 1–2), trait conformance (5), the statement-side closed list
(6), the `defer` rule (7), function types (8), the builtin `fails` column
(9) — is now described as current state, and the amendment notes flipped from
"proposed" accordingly. What has NOT landed is rules 3–4's refusal itself:
`raw` and `drop` still work unlicensed until 1.1.2 flips
`TYPE_RAW_UNLICENSED` on, once the 1.1.1/1.1.2 sweeps drive
`check_raw_licensed`'s counts to zero — and the specs say so wherever they
describe the licence.

### Amendments this forces

- `TYPE_REFERENCE.md` — the `Result` operator table (`raw`, `drop` rows) and
  the `raw`/`drop`/`discard` note; the "D-002 era" note.
- `OP_REFERENCE.md` — the `_!` and `_?` rows.
- `SAFETY_ARCHITECTURE.md` — Layer 2's `raw` / `drop` bullets ("Forced
  Handling" becomes true); the Escape Hatches list.
- `AST_REFERENCE.md` — §1.1 `contracts`; `FuncType`; `ExprStmt` (NIL-typed,
  closed forms); the `FailureContract` note.
- `LEXICAL_REFERENCE.md` — the contract production.
- `TRAITS_REFERENCE.md` §2 — signatures must match *including the contract*.
- `CONCURRENCY_REFERENCE.md` §2.2 — "result discarded" → "value discarded,
  error joined"; D-058 / D-062 gain a note; OPEN_DECISIONS C-7 / C-9 gain the
  join's error requirement.
- `VERIFICATION_REFERENCE.md` §3.1 — the `raw divide(10i32, 2i32)` example
  becomes `?!`; "forcing the caller to unwrap … using `raw`, `drop`" rewritten.
- `BUILTIN_REFERENCE.md` — the `fails` column.
- `DECISIONS.md` — D-002 (status line), D-014 (defer: what a body may do),
  D-060 (the statement-side closed list), D-080's table rows for `raw` / `drop`,
  D-089 (`discard` takes a value, never a `Result`), D-114's bullets, D-149's
  "contracts die" paragraph, the function-type example
  `func:apply = int32(func int32(int32):f, int32:x) { pass raw f(x); };`
  (the parameter type needs `never fails`), D-156 (mangling includes the flag).
- `PROTOTYPE_DELTA.md`; `CLAUDE.md`'s "what it refuses" list; `SUBSET_1.md`
  where it describes `raw` / `drop` / statement calls.

## D-164 — Projecting an associated type: `T.Item`, a dotted type suffix — **SETTLED**

OPEN_DECISIONS C-20, raised at 1.0.6 when D-160's own clause did not survive
contact, and decidable only after C-21 closed at 1.0.6b.

**The problem.** D-160 said `T.Item` "rides the EXISTING dotted-path type
grammar **if the parser already admits it**". It does not — measured, six
`NITPICK-PARSE-001` — and the fallback named in the same sentence,
`Iterator.Item`, is dotted too, so its "no new token, no new node kind" is
unachievable for either candidate. Neither authority has ever had a projection:
`TRAITS_REFERENCE` §2.3 shows declaration and binding only, and the prototype
carries `AssociatedTypeDecl` and `AssociatedTypeBinding` and no projection node
at all.

**Why it cannot simply be dropped.** Every binding in this language spells its
type, so generic code over a trait with an assoc can call `next()` and then do
nothing with the result — there is no way to name what came back. An `assoc`
trait would be usable concretely and useless generically.

### Three candidates, and why this one

- **Descope: use a generic trait when generic code needs the element type.**
  This works today — 1.0.6b made `func:twice<P: Producer<int32>>` compile and
  run. **Rejected**, because it makes the choice between `assoc:Item` and a
  trait parameter depend on whether some DOWNSTREAM consumer wants generic
  code: the trait's author would have to change the declaration when somebody
  else writes a generic function over it. The distinction between the two is
  meant to be semantic — an assoc is determined BY the implementor, one per
  type; a parameter is chosen by the caller, many per type — and that is a
  property of the trait, not of who consumes it.

- **A bare `Item`, resolved through the enclosing function's bounds.** No
  grammar change at all: the same mechanism 1.0.6 built for in-trait resolution,
  extended to a function's bound list. Genuinely tempting, and **rejected for
  ambiguity**: with `<A: Iterator, B: Iterator>` a bare `Item` names neither,
  and resolving it by position or by declaration order would be
  meaning-by-context. Explicit beats implicit here as everywhere else.

- **`T.Item` — a dotted suffix in type position. ADOPTED.** `.` is already *the*
  member-access operator and an associated type is a member of its trait, so the
  notation agrees with the operation; `->` was removed from member access
  (blueprint facet 2) precisely for pointing the wrong way, and `.` points the
  right one here. It is explicit about which parameter it projects from, which
  is what the bare form cannot be.

**Reasoning disclosure**: the input-versus-output distinction above is general
language design rather than anything the specs or the prototype state. It is the
argument, not a citation.

### What it costs, stated rather than discovered

- **A new AST `TypeKind`** (base type + name), plus one branch in `p_suffixes`
  beside `->`, `?`, `[]`. Every walker over type nodes must handle it —
  `resolve_type`, `type_mentions_self`, `type_subst`, and the seed if `src/`
  ever uses one (C-13's rule). A missed walker is a silent wrong answer, which
  this codebase has been bitten by before (`ast.npk`'s note on a literal's width
  suffix landing in slot `a`).
- **A bound-ambiguity rule**: `T.Item` where two of `T`'s bounds declare `Item`
  is refused naming both, the same answer D-158 gives a method supplied by two
  bounds, and for the same reason — an ordering tiebreak would be
  meaning-by-position.
- **It was expected to REOPEN D-159's tie, and it does NOT — measured at
  1.0.6d.** The reasoning was that once `a.S` is a type spelling, two same-named
  traits become nameable in one file. It is not: this production projects from a
  TYPE, and a module is not a type, so `dyn_a.S` reports "there is no type named
  `dyn_a`". Both of D-159's barriers still stand — the loader refuses two modules
  exporting one name (`NITPICK-RESOLVE-008`, and not only for glob imports), and
  there is still no module-qualified type spelling. The obligation is not due,
  and D-159's amendment is unchanged: it reopens if a MODULE path in type
  position is added, which this is not.

  An earlier draft of this decision also claimed the production "gives qualified
  type paths, which are impossible today" as a side benefit. **That was wrong**
  and is corrected here: it gives projection from a type, and nothing else.

**It is not a violation of the capability ladder.** That rule is about not
REBUILDING the frontend at every rung — the previous attempt's parser supported
only some keywords per step — and not about freezing the grammar. Adding a
production because the language needs one is ordinary work; `CLAUDE.md` now says
so.

**Implemented at 1.0.6c (grammar and resolution) and 1.0.6d (normalisation).
Complete.**

> `T.Item` parses as a suffix beside `->`, `?` and `[]`, resolves through the
> parameter's bounds, refuses an unknown name BY NAME rather than falling through
> to the invalid type, and refuses two bounds declaring one assoc naming both. A
> generic function can declare and return the projected type.
>
> **Two things this decision did not anticipate**, both found by building it:
>
> - The call site must bind the TRAIT, not only its parameters. 1.0.6b bound a
>   generic trait's parameters there; an assoc in the same signature needs the
>   trait itself, since it is a member of it and in no scope. The same defect in
>   the same place, for the other half of what a trait signature can mention.
> - **`TY_ASSOC` must carry its BASE.** With only (trait, name), `T.Item` and
>   `U.Item` interned as ONE type, so a function generic over two iterators had
>   one element type and substitution had nothing to substitute. And the base is
>   whatever `Self` MEANS where it is resolved, not the literal `Self` type.
>
> **Normalisation landed at 1.0.6d, in `type_subst`.** It cannot live in
> `resolve_type` — `ImplTable` is declared in `type_trait.npk`, which imports it,
> so a resolver field is a real import cycle (tried and reverted). It cannot be a
> LATER PASS either, which is what 1.0.6d's plan recommended and its own caveat
> disproved: a generic call's result type is compared against the declared type
> in the same expression check, so a pass running afterwards arrives after the
> mismatch is reported. `type_generic.npk` already imports `type_trait.npk` with
> no cycle back, so substitution sees the impl table and normalises inside the
> walk it already does — one walk rather than two.
>
> **Three places needed it, and only the first was predicted**: substitution of a
> generic call's signature; the BACKEND's re-substitution of a specialization's
> signature (which first passed no table, on the wrong assumption that everything
> was normalised by then); and `ll_type`, because a generic BODY's expression
> types were recorded with the parameter opaque (D-107) — `it.next()` arrives
> carrying `T.Item` exactly as `self.v` arrived at 1.0.4c carrying `T`. It is
> normalised INSIDE `ll_type` rather than around it, since a projection can be
> nested and that is the walk which recurses.


## D-165 — A module-level binding is a compile-time constant, lowered to an LLVM global — **SETTLED**

1.0.9's T-1. The parser and checker accept a module-level binding (D-010's
definite-assignment rules apply, D-086 refuses an initialiser cycle), and the
backend refused every one at its declaration ("a module-level binding",
re-pointed from "0.8" to "1.0" at 0.9.7 as the next cycle rather than an
owner). `MODULE_REFERENCE.md` §1 already states the principle — "Nitpick has no
module-level execution: a module is a set of declarations, globals are
compile-time-initialised, and there is nothing to sequence" — and
`TYPE_REFERENCE.md`'s `fixed` section already gives the lowering
(`@name = global T val`). This decision makes both precise enough to build.

**The initialiser is a compile-time constant expression:** a literal of any
kind (a string literal included — diagnostic codes are module-level strings),
a sentinel (`NIL`, `NULL`, `ERR`), a struct or array literal whose every
member is one, a reference to another module-level binding (acyclic, which
D-086's check already enforces), or a `comptime(…)`. **A call is refused by
name** — a new code, `TYPE_GLOBAL_NOT_CONSTANT` — and so is any expression the
folder cannot fold. No module initialiser runs before `main`: an initialiser
that called a function would run in an order nobody wrote down, across
modules whose imports impose none (`use` imports no initialisation order,
MODULE_REFERENCE §1), which is a value appearing in an order nobody chose —
D-010's objection, one level up — and it is the one thing a verifier cannot
read off the declaration.

**Lowering.** `const T:name = v;` → `@"npk.<module>.name" = constant <T> <v>`;
`fixed` → `global` (and, since D-211, a plain binding does not exist to lower).
The symbol follows D-156's scheme. A
string constant is a constant `{ptr, len, cap}` whose `ptr` names a private
constant byte array, as a string literal in a function already does.

**Mutability and threads.** A plain `pub int32:counter = 0i32;` is a mutable
global today (the accept suite has one) and this decision does not change
that; what a thread may do with one is 1.1's concurrency decision set
(C-7…C-9), already scheduled, and it is noted here so it cannot be forgotten.

> **ANSWERED by D-211** (2026-08-29, landed 1.4.2b): a plain module-level
> binding now REFUSES (`NITPICK-TYPE-055`) — module state is `const` or
> `fixed`, and mutable process state lives in `main`'s scope. The paragraph
> above is why the question was left open and where to find it; the coverage
> audit's G-7 is what closed it. `const` and `fixed` module bindings, and the
> lowering described below, are unchanged.

**Considered and rejected:** a lazily-initialised global (a function call on
first read) — a hidden call on a read, the context-dependent behaviour the
blueprint rule forbids, and a data race under 1.1; and refusing all globals
— the compiler's own sources want constants, and the spec already promises
them.

## D-166 — What `for` iterates: a range, a slice, an array, or an `Iterator` — **SETTLED**

1.0.9's T-2. D-023 said "a range or collection" and the backend lowered ranges,
slices and arrays, refusing "a `for` over this iterable" for anything else;
TRAITS_REFERENCE §2 showed an `Iterator` trait whose `next` returns a bare
`Item`, with no way to say the iteration is over. The 1.0.8 audit also found
the checker never compares a `for` binding's type with the element type
(`for (int8:i in 300i64..302i64)` reached `llc`) and accepts a struct as an
iterable (refused as a rung, i.e. called valid).

**Settled:** `for (T:x in e)` iterates `e` when `e` is a range, a slice, an
array, or a value whose type implements the prelude trait `Iterator`:

```nitpick
trait:Iterator = {
    assoc:Item;
    func:next = Item?(Self->:self);
};
```

`NIL` from `next` ends the loop — `Item?` is the reason this was undecidable
before 1.0.7 lowered `Optional`. **The binding's type must equal the element
type** (`Item`, or the range/slice/array element): no wrap, no widening, the
same equality every other typed binding has. A `for` over anything else is
refused at the CHECKER by name (`TYPE_NOT_ITERABLE`), never at a rung.

**`Self->` as a receiver is admitted in a trait signature.** `next` must
advance its receiver, and a by-value `Self` receiver is a copy — so until now
no trait method could mutate what it was called on, and the checker's
one-level `Self` substitution refused `Self->` outright. The receiver position
is the one place a pointer to `Self` is a fact about HOW the value is reached
rather than a second mention of `Self` in a signature, so `check_signature`
substitutes through the receiver's pointer wrapper (`Self->` ↔ `Target->`);
every other position keeps D-157's refusal of `Self->`. Object safety treats a
`Self->` receiver as a receiver: behind a vtable the data word IS a pointer to
the value, which is what a by-pointer receiver takes. (An `Iterator` is not
object-safe regardless, since `next` mentions `Item` — D-160 — which is the
right answer: a `dyn Iterator` would need a type for `Item` that erasure has
thrown away.) A call `it.next()` on a VALUE passes `@it`, as `f(@it)` would
(D-006: the same call).

**Lowering.** A `for` over an `Iterator` is the loop `T?:n = next(@it);
if (n == NIL) break; T:x = n ?? <unreachable>; body` — with the unwrap emitted
as a plain extract, because the test just proved the tag. Ranges, slices and
arrays keep their counted lowering (0.9.7).

**Considered and rejected:** duck-typed iteration (anything with a `next`) —
D-107's "no duck typing" property; an `Iterator` over a by-value receiver
(returning a new state each step) — allocates or copies per element, and
makes the spec's example wrong in a second way.

## D-167 — `?|` / `defaults` is struck; `?` is the one spelling of a `Result` fallback — **SETTLED; its CHOICE OF SURVIVOR REVERSED by D-175**

1.0.9's T-3, decided by measuring the prototype, as D-097's countermeasure
asks. `OP_REFERENCE.md` §0 described `expr ?| fallback` as "a scoped fallback
for an entire expression chain"; the checker typed it as "both sides one
type" with nothing that said WHEN the fallback is taken; no rung lowered it.

**What the prototype actually had.** Its changelog (v0.4.3 onward) defines
one node, `DEFAULTS`, spelled two ways: `'expr ? fallback' / 'expr ?|
fallback' — scoped Result fallback`. Every test of it
(`tests/feature_validation/defaults_basic.npk`) applies `?|` to the `Result`
immediately to its left — `fail_always() ?| 99i32`, `succeed(fail_always() ?|
10i32)` — exactly as this compiler's `?` behaves since 0.9.7. "Entire
expression chain" was never more than precedence (level 18, below `??`), and
under D-013 and D-092 a `Result` cannot sit inside an arithmetic chain at all,
so there is no chain for a looser operator to scope over.

**So it is a second spelling of `?`**, differing only in precedence — the shape
D-021 struck for casts and D-123 for `Display`. It is struck: the parser still
reads `?|` and `defaults` (the grammar is never partial, D-085) and refuses
them by name, `PARSE_DEFAULTS_REMOVED`, with the answer (`expr ? d`, and
parentheses for any scope a reader wants). The `defaults` keyword stays
reserved, as `(!)` did under D-061, so old code gets the message and not a
stranger one.

## D-168 — `&{ x }` renders through `ToString`; a `string` is itself — **SETTLED; implemented 1.0.9d for the exact scalars, `char`/`flt` `ToString` scheduled post-1.0 (OPEN_DECISIONS §6b)**

1.0.9's T-4. D-053 moved all formatting to `&{ }` interpolation in backtick
templates, and nothing said how a value becomes text. **Settled:** inside a
template, a `string` interpolates as itself; any other type interpolates by
calling the prelude trait `ToString`'s `to_string` on it — statically, at the
value's concrete type — and a type with no `ToString` impl is refused at the
CHECKER by name (`TYPE_NOT_STRINGABLE`), never at a rung. The prelude supplies
`impl:<width>:ToString` for the builtin scalars — the integer widths, the
floats, `bool`, the characters, the `tbb` widths (ERR renders as `ERR`), the
kernel identifiers — over runtime conversions in the floor, so a program need
not write them. A template lowers to a `string_concat` chain over its parts.
**No format specifiers:** a width, a radix or a precision is a `to_string`
variant somebody writes and calls (`&{ pad(x, 8) }`); a mini-language inside
the braces is a second language with its own rules to remember.

`ToString` is the one trait (D-123 struck `Display` as its second name); a
derived `ToString` (D-123) renders a struct's fields with the same rule, and
`Debug` stays the separate question it is.

## D-169 — `==` on a non-scalar type is refused; equality is `Eq` — **SETTLED**

1.0.9's T-5. `type_comparison` unified its operands and checked only ordering,
so `==` on two structs typechecked and reached `llc` as `icmp` on an
aggregate; 1.0.7 made it a named refusal at the rung. **Settled:** `==` and
`!=` take two scalars — integers, floats (D-143), characters, `bool`, `tbb`,
kernel identifiers, pointers (address equality) — and an `Optional` against
`NIL` (D-099's test). On a struct, an array, a `Result`, two `Optional`s that
are neither `NIL`, a `string` or a `dyn` they are refused at the CHECKER
(`TYPE_NOT_COMPARABLE`, with the spelling that works). A type that wants
equality implements `Eq` and is compared with `a.eq(b)` — which
`#[derive(Eq)]` already writes fieldwise; `string` equality is `string_eq`,
as `src/` spells it everywhere.

**Why not a fieldwise `==`:** it would make `Eq` a second spelling of it (the
D-123 objection), a `dyn` comparing by pointer would be a third meaning under
one symbol, and a `string` comparing by pointer would be a silent wrong
answer — three context-dependent readings of `==`, the thing the blueprint
rule exists to prevent.

## D-170 — Parentheses group a type: `(dyn A & B)[2]` — **SETTLED**

1.0.9's T-6, found at 1.0.6e. The `dyn` production parses each operand with
the full type grammar and takes no suffix of its own, so `dyn Speaks[2]` can
only mean `dyn (Speaks[2])` — refused, an array is not a trait — and **no
spelling existed** for an array, a pointer or an `Optional` of a trait object;
the only reachable form was by substitution (`Pair<dyn Speaks>`'s `T[2]`).

**Settled:** one production, `Type ::= "(" Type ")" Suffix*` — parentheses
group a type exactly as they group an expression, and the suffixes apply to
the group: `(dyn A & B)[2]`, `(dyn Speaks)->`, `(dyn Speaks)?`. No new node
kind: the group IS its inner type. **Rejected:** "suffixes after a `dyn` set
apply to the whole set" — one fewer token, but a precedence rule a reader has
to know, and `dyn A & B[2]` genuinely reads two ways.

## D-171 — Every impl names its target: `impl:<params>:Target(:Trait)?` — **SETTLED; amends D-031 and D-161**

1.0.9's T-7, found at 1.0.7. D-031's rule is "slot 1 is always the type being
implemented on", and D-031's blanket form `impl:<T: Printable>:Loggable` let
the parameter WINDOW double as that slot. D-161's family form
`impl:<T>:Box<T>:Trait` then had a window that is NOT the slot, and the
segment-count rule that told the two apart (1.0.4b) made the inherent family
`impl:<T>:List<T> = { … }` unspellable — one segment after a window reads as
a blanket impl of a trait named `List<T>`. So a generic struct could carry no
methods of its own, and since a trait method could not take `Self->` (until
D-166), no method of a generic struct could mutate it at all.

**Settled:** the blanket form names its target. `impl:<T: Printable>:T:Loggable`
— "for every `T` that is `Printable`, implement `Loggable`" — and every impl
then reads by ONE rule:

| Form | Params | Slot 1 (the target) | Trait |
|---|---|---|---|
| `impl:Point` | — | `Point` | — |
| `impl:Message:Serializable` | — | `Message` | `Serializable` |
| `impl:<T: Printable>:T:Loggable` | `<T: Printable>` | `T` | `Loggable` |
| `impl:<T>:List<T>:Sized` | `<T>` | `List<T>` | `Sized` |
| `impl:<T>:List<T>` | `<T>` | `List<T>` | — |

The window declares parameters and nothing else; slot 1 is always the target,
D-031's own principle, now true of every row. It also closes a correctness
hole the old form had: with two parameters, `impl:<T: A, U: B>:Trait` never
said which was the target. A parameterised impl whose slot 1 is a bare
parameter is a blanket impl; whose slot 1 mentions its parameters inside an
instantiation is a family impl; no segment is counted and no shape is
inspected. **Cost:** one token on every blanket impl, and the existing blanket
impls in `tests/` and the specs are rewritten (none in `src/`, which the seed
builds).

## D-172 — A trait name is a namespace: `Trait.method(recv, …)` — **SETTLED; amends D-102**

1.0.9's T-8, found by the 1.0.8 audit. D-102 refused to choose between two
traits supplying one method name and told the reader to "call the one you
mean by its function form, which UFCS guarantees is available" — and no such
form exists: an impl's method declares nothing at module scope, and
`Ta.tag(p)` resolved `Ta` as a type in value position. The promise was never
implemented.

**Settled:** in expression position a trait name is a namespace whose members
are the trait's methods, exactly as an enum's are its variants and a module's
its declarations (`namespace_member`). `Ta.tag(p)` is `p.tag()` with the
trait said out loud: it resolves `tag` in `Ta` for `p`'s type (statically; a
generic receiver through its bound), passes `p` as the receiver with the same
fit every receiver gets, and is refused by name when `p`'s type does not
implement `Ta`. No new syntax — `Net.Disconnect(x)` already parses this way —
and it retires the backend's "a call through a member" refusal for this
shape. The ambiguity diagnostic names it. The same form disambiguates two
BOUNDS on a generic parameter, which the old promise could not reach.

## D-173 — Allocas are hoisted to the entry block — **SETTLED**

Cycle 1.0.9a, found when part one's added source made the seed-built `npkc`
segfault compiling `src/`. Recorded as a decision, not only a fix, because it
is a standing codegen invariant every later lowering must keep.

**The rule.** Every `alloca` a function emits is placed in that function's
ENTRY block, executed once per call, regardless of where in the body the local
is declared. A local declared inside a loop reuses one stack slot across
iterations rather than allocating a fresh one each time.

**Why.** An `alloca` in a loop body allocates stack on every iteration and
reclaims none until the function returns (LLVM `alloca` is frame-lifetime, not
scope-lifetime). A walk that scales with the program — `expand_audit` over
`src/`'s ~100k expressions — then overflows the stack, which is an uncontrolled
crash, the physical-safety event the language exists to prevent. Hoisting is
what every production compiler does and is the standard shape mem2reg expects.

**Why it is sound.** The entry block dominates every other block, so an alloca
placed there is valid at every use; the value name does not move, so nothing
that refers to the slot changes. Reusing one slot across loop iterations is
correct because each iteration writes the slot before reading it, and a local
whose address outlives its iteration is already refused by the escape analysis
(D-004) — so nothing the language admits depends on a fresh slot per iteration.

**Both codegens, by the same transform.** The seed (`emit.py`) inserts each
alloca line at a cursor just after `entry:`; the real backend (`ir_writer.npk`)
buffers a function's allocas and body in separate sinks and splices them
(allocas first) into the module stream at the function's close. Both move each
alloca line from its body position to the entry region in creation order, and
both produced byte-identical IR before, so both produce byte-identical IR
after — the stage-1/stage-2 fixpoint is the check. `irw_text` assembles the
in-progress view (out + allocas + body) so a read taken before a function
closes still sees the whole function.

**Standing obligation.** A new lowering that emits an `alloca` must route it
through `irw_alloca` (the real backend) / `self.alloca` (the seed), never write
the line inline. There is no instrument yet; a `check_allocas_hoisted` scanning
emitted IR for an `alloca` outside an entry block is the natural one and is
noted for when a lowering next adds an alloca.

---

## D-174 — `++` / `--` are struck; an increment is `x += 1`, not a value — **SETTLED**

Closing cycle 1.0, `++`/`--` (`ExprPostfixExpr`) was the last "1.0" backend rung
with no owner. The disposition table's instruction was "enumerate, then lower —
one shape remains; name it": 0.9.7 lowered the postfix *access* operators
(`()`, `[]`, `.`, `?.`); the one shape left was the C increment/decrement.

**What the prototype had.** `../nitpick` implements them C-style —
`codegen_expr_binary.cpp` returns `expr->isn ? oldVal : newVal`, so postfix
yields the OLD value and prefix the new, both mutating the operand.
`OP_REFERENCE.md` §1 carried the row (`i++` / `++i`, "Post/pre-increment"). But
this compiler only ever parsed the POSTFIX form — there is no prefix `++` node —
so "post/pre" was already half-fiction, and no rung lowered either.

**Why struck, not lowered.** Three independent reasons converge:

- **Redundant.** `x += 1` / `x -= 1` already exist and lower (`compound_op`,
  `ir_stmt`). As a *statement*, `x++` is exactly `x += 1` — a second spelling,
  the shape D-021 struck for casts, D-123 for `Display`, D-167 for `?|`.
- **Hidden mutation in expression position.** The only thing `++`/`--` add over
  `+= 1` is the *value* form (`y = x++` mutates `x` and yields its old value).
  A write hidden inside an expression is exactly what the blueprint philosophy
  ("explicit over implicit", one meaning everywhere, no silent side effects)
  exists to forbid, and it is pure developer comfort — subordinate to safety and
  correctness. It also opens the sequence-point class of bugs (`a[i++] = i++`)
  that a safety-critical language must not carry.
- **No owner cycle.** The roadmap runs 1.0 generics · 1.1 async · 1.2 the
  managed lowering · 1.3 self-hosting · 1.4 verification · 1.5 Astrée. None
  would ever lower an
  increment operator — evidence it was never a planned feature, only an
  unremoved prototype inheritance.

**The strike.** Mirroring D-167: the parser still reads `a++` and `a--` (the
grammar is never partial, D-085) and refuses them by name,
`PARSE_INCDEC_REMOVED` (`NITPICK-PARSE-010`), with the answer — `x += 1` /
`x -= 1`. The backend guard for `ExprPostfixExpr` becomes a defensive
`iv_broken`: a clean program never reaches it. `OP_REFERENCE.md` §1 loses the
two rows.

---

## D-175 — The `Result` fallback is `?|`, not a bare `?`; D-167 kept the wrong survivor — **SETTLED; amends D-167**

D-167 was right that `?` and `?|` were redundant — one operation, two spellings —
but it kept the *accidental* spelling and struck the *intended* one. The user's
design, stated directly: the fallback is `?|`, and a bare `?` was never meant to
exist. `?` "snuck in" and is confusing precisely because it mirrors Rust's `?`
(which propagates errors) while Nitpick's fallback yields a value.

**The principle it serves.** Anything that could be mistaken for a C/Rust
construct but behaves differently is deliberately renamed or respelled in this
language — `const` → `fixed`, the ternary `?:` → `is`, `switch` → `pick` — so a
reader's habit from another language cannot mislead them. The operators were
designed as consistent two-character families for the same reason: the `?`-family
is `?|` (a `Result` fallback), `??` (an `Optional`/`NIL` coalesce), `?!`
(unwrap-or-`failsafe`); the `_`-family is `_!` (`raw`), `_?` (`drop`), `_^`
(`relay`). A lone `?` breaks that family and re-introduces exactly the C/Rust
collision the family was built to avoid.

**No owner, tiny cost.** A bare `?` fallback appeared in zero source lines (the
one match was D-167's own diagnostic text) and about nine test lines — so this is
not a D-163-style sweep. The change:

- The parser makes `?|` the working fallback — building the `SafeUnwrap` node,
  the same one the seed already produces for `?|`, so the AST agrees. A bare `?`
  and the word `defaults` are refused by name, `PARSE_BARE_QUESTION_REMOVED`
  (`NITPICK-PARSE-011`), pointing at `?|`. The grammar is never partial (D-085):
  both are still read, then refused. `PARSE-009` (which removed `?|`) is retired.
- `OP_REFERENCE.md` restores `?|` as the fallback and marks a bare `?` struck.
- The ~nine test uses of `?` become `?|`; the parse/type fixtures now assert
  `?|` parses and a bare `?`/`defaults` are refused.

Discovered when the user, reviewing the 1.0 close, explained the design history
behind `?|` — the reason it was chosen over `?` in the first place.

---

## D-176 — `Duration`, the monotonic clock, and the deadline substrate — **SETTLED**

B-2, the hard blocker for cycle 1.1: every deadline API (D-056/62/71/83) uses
a duration type defined in no spec. Five parts, decided together because they
are one substrate.

**1. `Duration` is a prelude struct over `int64` nanoseconds.**

```nitpick
struct:Duration = { int64:ns; };
```

Not a primitive (it earns no special checking) and not a `timespec` pair —
the pair invites the tv_nsec-normalisation bug class C has carried for
decades, and a single `i64` of nanoseconds spans ±292 years, orders with one
compare, and fits one atomic word. Constructors are prelude functions —
`duration_ns`, `duration_ms`, `duration_secs` — all `never fails`; their
multiplications ride ordinary `int64` arithmetic, whose overflow TRAPS
(D-142), so a span past ±292 years is a controlled stop, not a wrap. There
is no `Duration` arithmetic surface until an API needs one: YAGNI, and every
helper added later must keep `never fails` provable.

**2. Deadline parameters are RELATIVE spans, and say so.** Every API sheet
wrote `deadline` but typed a span; a name that lies fails the blueprint rule.
The parameter is a `Duration` named **`within`** — `recv(within)`,
`acquire(within)` — and the executor converts to an ABSOLUTE monotonic
timepoint ONCE, at suspension entry, so re-arms and spurious wakes cannot
stretch the wait. No user-facing `Instant` type exists; the executor's
absolute timepoints are internal `int64` nanoseconds.

**3. The clock is `CLOCK_MONOTONIC`, through the floor.** One primitive:
`mono_now() → int64` — nanoseconds since an arbitrary epoch, `never fails`
(`clock_gettime(CLOCK_MONOTONIC, valid-ptr)` cannot fail on Linux; the floor
still guards the impossible branch with the D-061 trap, because "cannot
fail" is a claim, and this project checks claims). Wall clocks are excluded
from the deadline path entirely: NTP steps a wall clock, and a deadline that
moves with the wall is D-056's containment silently voided.

**4. Executor waits are absolute-monotonic.** The futex integration (1.1.5)
uses `FUTEX_WAIT_BITSET | FUTEX_CLOCK_MONOTONIC` with the absolute timepoint
from rule 2 — the kernel owns the arithmetic, and a wake-and-repark loop
cannot accumulate drift.

**5. `DEADLINE_EXCEEDED` is pinned at −4107** in the D-141/D-142 space. It
is BOTH channels' code, one value: as a `Result` error it is catchable — a
`recv(within)` that times out returns it, and "a `recv` that returns
`DEADLINE_EXCEEDED` is exactly how a worker notices shutdown"
(CONCURRENCY §6) — and as a trap code it is what a JOIN raises when a task
outlives its mandatory deadline (D-062/D-083), where expiry is a defect, not
an event. One number, so a log reader never learns two spellings for "time
ran out".

---

## D-177 — Coroutines are hand-lowered switched-resume state machines — **SETTLED**

C-7, the cycle's densest blocker. The prototype used `@llvm.coro`; 0.10.3's
outcome note assumed the same; this decision reverses that assumption, on
grounds that did not exist when the note was written.

### Why not `@llvm.coro`

1. **The emitted IR would stop being the program.** Coro intrinsics are a
   CONTRACT with LLVM's CoroSplit/CoroFrame passes: the artifact npkc emits
   is not what executes — the load-bearing transform (frame layout, spill
   selection, resume splitting) happens inside the C++ black box, after
   emission. Everything this project stakes on verifiability — Astrée reads
   the artifact; D-078's byte-determinism; the fixpoint — weakens to
   "modulo whatever CoroSplit did this version".
2. **The pipeline has no opt step.** BUILD_REFERENCE is `npkc | llc | ld.lld`;
   coro lowering REQUIRES opt passes before llc. Adding a mandatory pass
   pipeline adds toolchain surface exactly where the project has none.
3. **The burned-hand rule.** LLVM's optimiser has already removed a
   load-bearing guarantee once in the prototype's history. Handing it the
   SUSPENSION SEMANTICS of a safety-critical executor is that risk, enlarged.
4. **D-153 was built for the other answer.** The frame allocator's whole
   design — exact-size buckets because "the coroutine workload IS exact-size
   recurrence: one frame size per async function" — assumes the COMPILER
   knows each frame's size. Under `@llvm.coro` the size is LLVM's, surfaced
   through `@llvm.coro.size` at a stage our allocator calls cannot reach
   cleanly.

### The lowering

An `async func:f = T(args)` compiles to a **resume function** and a **frame**:

- **Frame layout** (compiler-owned, per async function, one exact size —
  D-153's bucket): a fixed header, then every local whose live range CROSSES
  a suspension point, laid out by the same layout machinery structs use.

  ```
  [ resume_fn: ptr | state: i32 | windup: i32 | result: Result<T>
  | join_head: ptr | sibling: ptr | awaitee: ptr | args..., crossing locals... ]
  ```

- **The resume function** `i8 npk.resume.f(ptr frame)`: a `switch` on
  `state` over the segments between suspension points. Locals that cross a
  suspension live in the frame; locals that do not stay ordinary allocas.
  Returns `0` = DONE (result slot written), `1` = SUSPENDED.
- **`await g(x)`** composes machines: allocate g's frame (D-153 `alloc`),
  store args, then drive — call g's resume; on DONE copy the result slot
  out and free the frame; on SUSPENDED record g as `awaitee`, save own
  state, return SUSPENDED (suspension propagates to the executor, never
  blocks a thread — D-071). **The `result_slot` is owned by the frame that
  computes it**; the awaiter copies out only at DONE, so no slot outlives
  its frame.
- **`drop work()` (the spawn, D-058)**: allocate the frame, store args, link
  it onto the enclosing async frame's `join_head` child list (the frame IS
  the task object; `sibling` chains the scope's spawns), and enqueue it —
  1.1.5's run queue; until then the join drives children directly.
- **The join (D-062/D-083/D-163 rule 4)**: at every exit of the enclosing
  async function — normal or error, after `defer`s — drive each child to
  completion under the executor's join deadline (`within`, D-176). All
  children finish → relay the FIRST child error, verbatim (D-080), as the
  function's own error. Deadline expires → set every unfinished child's
  `windup` word and grant the grace drive; a child still unfinished then is
  a defect: **trap −4107**.
- **The wind-up token** is the frame's `windup` word. Every resume segment
  that begins at a suspension point polls it first: set → the await
  completes with the wind-up error instead of its value, so the task unwinds
  through its own `defer`s (D-062's cooperative-only model; preemptive
  destruction stays removed).
- **`async main`** runs through an entry shim that allocates main's frame
  and loops its resume to completion — the degenerate executor, replaced by
  1.1.5's real one.

### The analysis this buys (C-8's hook)

Which locals cross a suspension is computed by the CHECKER (a
liveness-across-await walk beside the existing analyses) and recorded for
the backend — and it is exactly the surface C-8's borrow-across-await
narrowing reads: a borrow crossing a suspension is visible as a
frame-resident pointer, the thing the rule exists to refuse across spawns.

### The checker rules (settled with stage C, 1.1.4)

All `NITPICK-TYPE-043`, beside the two the decision already named (`await`
only inside `async`; the spawn form only inside `async`):

- **A bare call to an `async` function is refused.** The callee has no
  direct symbol — only a machine — and admitting the call would be an
  IMPLICIT await: a suspension nothing in the source says. `await f(…)` and
  `drop f(…)` are the two homes; there is no third.
- **`await` and the spawn form are refused inside `defer`.** A defer body
  is synchronous cleanup; the JOIN, not a defer, is where a scope waits on
  concurrent work. D-163 already bars `fail`/`relay` there — suspension
  joins that family.
- **`failsafe` cannot be `async`.** It is the controlled-stop path and must
  run without an executor, which may itself be the casualty. `async main`
  stands (the entry shim is its executor until 1.1.5).
- **A spawned callee declares `NIL`** (stage E). `drop` is the void call,
  sync or async — one meaning, no context. A spawned value would have
  nowhere to go (only errors reach the join), and the uniform `{ i32 }`
  result slot is what lets ONE join walk a heterogeneous child list through
  the generic frame header, dispatching through the resume slot. A child
  error reaching `async main`'s join has no `Result` to relay into: it
  routes through `failsafe`, the uncaught-error path (D-013). When the
  function's own exit already carries an error, its own error wins and the
  children's are discarded after their drives complete — first error
  standing, own before children's.

### What stays out

No user-visible `Future`/`Task` type (D-058 stands); no `coro.destroy`-style
preemption (D-062 stands); no thread migration (D-032 stands — a frame never
changes executors, which is what lets D-153 stay atomics-free). Mid-statement
suspension is out by D-178: an `await` is the first evaluation its statement
performs.

## D-178 — A statement suspends at most once, before anything else it computes — **SETTLED**

Ratified during 1.1.4: the resolution of the first-evaluation question stage
C surfaced. `x = v + relay await f(v);` evaluates `v` into an SSA value,
suspends, and re-enters mid-body on resume — a value from before the
suspension cannot dominate its use after it. Two candidate answers existed:
make the compiler spill in-flight temporaries into the frame (hand-built
liveness — the largest single analysis the backend would own, and exactly
the machinery D-177 declined to inherit from LLVM's CoroSplit), or make the
restriction a language rule. **The rule won**, with the spill explicitly
available as a LATER addition if deemed necessary — it cannot break a
conforming program.

### The rule

An `await` is the FIRST evaluation its statement performs, and a statement
contains at most one. Precisely:

- Everything the statement evaluates before the `await` must be a bare
  literal — literals lower inline and cost no temporary, so `1i32 +
  (await f() ?| 0i32)` stands while `v + (await f() ?| 0i32)` refuses.
- The `await`'s own arguments are exempt: they evaluate before the
  suspension and are CONSUMED into the callee's frame (`await f(v)` is the
  idiom, not a violation).
- An assignment whose value awaits takes a BARE NAME as its target — a
  field or element target computes an address before the suspension.
- The unwrap keywords sit outside and cost nothing: `relay await f(v)`,
  `await f(v) ?| d`, `await f(v) ?! c` are all first-evaluated.
- The workaround is always one line, and stage D made it free: bind first
  (`int32:t = relay await f(v); x = v + t;`) — a bound result that crosses
  a later suspension is frame-resident automatically.

Enforced by the CHECKER (`NITPICK-SUSPEND-002`, from the suspend walk,
which already sees every statement in evaluation order); the backend's
temp-count guard stays as the defensive backstop, the same belt-and-braces
every checker-guaranteed property gets. A suspension inside a `pick`
`where` guard remains a backend capability rung (the compare chain re-reads
the selector), not part of this rule.

### Why the rule and not the spill

Every suspension point is STATEMENT-VISIBLE: a reader auditing a coroutine
never has to reconstruct which half-evaluated expression is frozen across a
re-entry, because that state cannot exist — and neither does the verifier.
This is the blueprint philosophy applied to time: nothing suspends
mid-thought. (Cross-reference: D-177's lowering is what makes the rule
cheap; D-163's licence already keeps `raw` off `await`.)

## D-179 — Errors are a nominal type, not a number: `Error`, origin chains, and the exhaustive `failsafe` — **SETTLED**

Proposed by the user (initially as a note against their own design, then
through an external design review they brought in), analysed and shaped
in-session, ratified 1.1.4-close. The finding: using `tbb` for error handling
violates the blueprint philosophy twice — a balanced-ternary MATH type
carrying failure identity is meaning-by-context, and `tbb` already has a
second job (the D-008 ERR taint, a math sentinel). Errors become their own
type; the taint system is purified back to one meaning.

### The type

**`Error` is nominal and compiler-known.** Its machine representation is the
same 4-byte word the `Result` error slot holds today — and it is NOT a
number, by the same doctrine that makes `bool` and `char` non-integers
(semantic meaning outranks representation): no arithmetic, no ordering, no
casts in or out; equality and `pick` dispatch only; a value exists only by
naming a DECLARED error constant. `Result<T>`'s error half is `Error`;
`fail`, `?!` and `failsafe` take `Error`. `Result` stays ONE-generic —
`Result<T, E>` with per-function error types was considered and REJECTED: it
requires inferred union types through the call graph, varies the error
slot's size (breaking the uniform coroutine frame header, the join's
heterogeneous child walk, and `npk_failsafe`'s ABI), and reintroduces the
question "what error type does this return?" that uniformity exists to
kill. The reachable-error question is answered by ANALYSIS over declared
constants, not by the type system.

### Declarations and domains

A new declaration form, `error:Name;`, names one error constant. The SYSTEM
domain is exactly the constants the prelude/runtime declares (the D-142
table becomes prelude declarations with their fixed negative codes — the
runtime's hardcoded values are the stability constraint); user code cannot
declare into it, which turns the old "negative = system, positive = user"
convention into a compiler-enforced fact — the sign is now an encoding
detail, not a user-facing rule. User constants get compiler-assigned
positive codes DERIVED FROM THE DECLARATION — the FNV-1a of `module.Name`,
truncated positive; a collision refuses loudly at resolve. (Amended during
1.1.5-A from "module-graph order": the seed and stage 1 must number
identically for the fixpoint to hold, and a derived code needs no shared
walk order to agree on.) Codes are identity within one build, not an ABI
across programs.

### The origin chain

`fail E;` stamps the failure site and resets the chain; every `relay`
(`_^`) hop APPENDS its site; `?!` stamps before trapping. Sites are
compile-time-interned ids (module, function, line) in a table emitted with
the program; the chain is a FIXED ring (8 sites + a depth counter),
per-thread, allocator-free, written only on the failure path — the success
path costs exactly today's single zero store. `failsafe` reads the chain
through prelude accessors. **Chain fidelity is guaranteed within one
synchronous propagation**; a parked child's error carries its CODE in the
result slot and the join re-stamps the winning error at the join site —
chains are diagnostics, codes are semantics, and semantics are never
truncated. D-080 is amended one line: relay propagates the error's IDENTITY
verbatim; the chain grows.

### The exhaustive `failsafe`

There is one `failsafe` per program (D-013), so the reachability question
is one program-wide set: every error constant appearing at any `fail` or
`?!` site, PLUS the entire system family (heap integrity can fire anywhere,
so every failsafe must face it), PLUS everything a spawned child can fail
with. The set is CONSERVATIVE — no flow-sensitive narrowing; over-coverage
is safe and simple is verifiable. `failsafe` must contain a `pick` over its
`Error` parameter whose NAMED arms cover the computed set; **`(*)` is
permitted and counts for nothing** — the compile-time force (a new failure
mode anywhere breaks the build until every failsafe names it) and the
defensive runtime floor beneath the proof, both.

### Amendments from 1.1.7's build

**Error declarations are implicitly `pub`.** An error's identity crosses
every boundary a `Result` can, and `failsafe` must be able to NAME whatever
can reach it — a private identity that still arrives would be unhandleable
by construction. **Cross-module references use the qualified spelling**
(`ast.BoundsId`) — in `failsafe` arms and anywhere else — and the
reachability refusals print it, so the fix is always a paste. Two error
constants sharing a bare name across modules is NOT warned as a wildcard
clash (identity is module-qualified by construction; a module's own sites
bind locally), which keeps the per-module defect vocabulary — the pattern
the design encourages — quiet. And the conditional system detectors walk
USER modules only: a program reaches the prelude's guard-bearing internals
only through machinery its own text must contain, and those trip the
detectors where they appear.

### What stays out (decided, not deferred)

Error PAYLOADS (context data beyond identity + chain) — they are where
fixed-size dies; identity, domain and origin cover the stated need, and a
payload design can only ever arrive as its own decision with its own
layout. Generic `Result<T, E>` — rejected above. Narrowing of the reachable
set — the conservative set is the semantics. None of the three is blocked
by v1; each would be a new decision.

### Consequences

D-080 amended (identity-verbatim + chain-append); D-142's table becomes
prelude declarations (same values, `npk_failsafe(i32)` ABI unchanged — the
argument IS the Error word); D-176's −4107 becomes the `DeadlineExceeded`
constant in both its channels; D-008 untouched — ERR stays a math sentinel
only. Scheduled as 1.1.5–1.1.7, DELIBERATELY BEFORE the executor
(now 1.1.8): the executor mints deadline, windup and parking failures, and
built after this change it is built once. The migration flips every
`fail`/`?!` site in `src/` and `tests/` from numeric tbb literals to named
constants — a large mechanical sweep with proven tooling, and a readability
gain: the compiler's own trap conventions (`?! 9tbb32`, `?! 25tbb32`…)
finally get names.

## D-180 — The borrow-across-await rule narrows to borrow-across-SPAWN — **SETTLED; the sanctioned-crossing list grew a fifth member at 1.4.4 (user-ratified)**

C-8, settled at 1.1.8's close, on evidence that did not exist when the
question was raised: D-177's crossing-locals analysis is now built, and it
decides the thing the rule was guessing at.

> **AMENDED at 1.4.4 (2026-08-29), user-ratified: `shared_arena<T>->` joins
> `Mutex`, `RwLock`, `CondVar` and `Barrier` as a sanctioned spawn crossing.**
>
> The exemption below is keyed on the TYPE, and its test is that the hazard —
> "a mutation the holder cannot see, at a suspension point it did not choose"
> — cannot arise. A shared arena meets that test more plainly than the four
> locks do, because it has no mutation at all: D-154's contract is that a slot
> is written ONCE, at `alloc(v)`, before its handle can escape, and is
> immutable afterwards. There is no `put`, no `free`, no `reset` — the method
> set is alloc/get/destroy precisely so that reading one concurrently needs no
> synchronisation, and `alloc`'s own bookkeeping is the runtime's atomic bump.
>
> **Why the question only arose now.** Until D-207 gave `shared_arena<T>` a
> drop it owned nothing, so it crossed a spawn as a copyable pointer VALUE and
> needed no exemption. Owning made it move-only (TYPE-046 keys on
> `type_drops`), and a borrow became the only way to share one. That is sound
> because the same decision made a scope exit JOIN before it drops: the borrow
> dies after every task that could hold it, which is the guarantee the four
> locks already ride. Landing the drop without this would have left a type
> whose name and whose decision both say "shared" and which could not be
> shared with anything.
>
> `shared_arena_spawn.npk` is the case: two threads allocating concurrently
> through one borrowed arena, joined at the block's exit and released after,
> exiting ZERO so D-151's leak check is armed.

### What the blanket rule was protecting against

D-004 rule 4's third crossing refused a borrow held across an `await`,
reasoning that "the frame it borrows from does not survive the suspension".
Under the lowering that is true of ONE kind of storage: a local that stays an
ordinary `alloca` lives in the resume function's activation, which is
destroyed when the machine returns SUSPENDED. A pointer to it, held by an
awaited callee across the suspension, dangles on resume.

It is NOT true of the other kind. A local whose live range crosses a
suspension is FRAME-RESIDENT (D-177 stage D): its storage is in the
heap-allocated coroutine frame, which outlives every suspension by
construction, and so does a parameter, which the caller wrote into the frame
before the machine ever ran.

### Why the hazard cannot arise

**The suspend walk marks every address-taken local in an `async` function as
crossing** — "an address outlives its mention", written into the walk when it
was built, because the walk cannot follow where a pointer goes. So in an
`async` function, `@x` on a local of that function makes `x` frame-resident,
and the dangling case has no way to be spelled. The blanket rule was
therefore refusing programs whose hazard the compiler had already removed —
including the entire async I/O surface (`await write(fd, @buf, n)`), which is
what made C-8 urgent.

### The rule now

- **A borrow may be passed into and held by a directly-awaited callee.** Its
  referent is frame-resident by the guarantee above; the awaiter is suspended
  for the whole of the awaitee's life and its frame outlives it.
- **A borrow may NOT cross a spawn** — task or thread — which is
  `BORROW-004`, wired to the spawn form (`drop f(…)` on an `async` callee) by
  this decision. Lexical lifetime (D-062/D-083) closes the DANGLING half: the
  spawner joins the child at scope exit and outlives it. The ban stays for the
  ALIASING half, exactly as D-083 says: two tasks holding borrows of one
  storage is a race, and on one executor it is still a mutation the holder
  cannot see, at a suspension point it did not choose. Race freedom is what
  the rule is for, and lifetime does not buy it.
- **`BORROW-005` retires.** With the blanket rule gone its residue is empty:
  every borrow an `async` function can spell is either frame-resident (safe)
  or crosses a spawn (`BORROW-004`). The code stays declared with this as its
  stated reason rather than being deleted, so a future reader finds the
  reasoning instead of a gap in the numbering.

### What this does not change

`@` stays second-class (D-004 rules 1–3 untouched): a borrow still may not be
returned, stored in anything outliving the frame, or given to an `extern`.
The borrow-checker deep dive's obs. #1 closes with this.

## D-181 — Threads: the `thread` modifier, the per-thread executor, and the lexical join — **SETTLED**

C-9's thread half, ratified from `meta/roadmap/1.1/C9_THREAD_STUDY.md`. It
closes a gap D-083 itself opened: **`Thread.spawn` appears nowhere in the spec
set**, so the language had rules about a construct it could not spell.

### 1. A thread body is a function; the spawn form is the one that exists

```nitpick
thread async func:sensor_loop = NIL(fd:dev) joins SENSOR_JOIN { … };

async func:main = int32(cstring[]:_~argv) {
    drop sensor_loop(dev);      // starts the thread; scope exit joins it
};
```

**`thread` is a function modifier** beside `async`, and a thread starts
through the SAME spawn form a task uses — `drop f(args)`. No handle (D-083),
no `Thread.spawn(f, arg)` (the prototype's erased the job type and zeroed
captured environments — D-073 §thread-pools).

Reusing the spawn form is not economy, it is rule inheritance: D-179's
`NIL`-success requirement, D-177's join and wind-up, D-180's borrow ban (a
literal data race here rather than an interleaving one), and D-163's
licensing all apply unchanged. A second spawn spelling would restate every
one, and "which spelling does this rule apply to?" is the meaning-by-context
the blueprint philosophy exists to prevent.

**The modifier is on the DECLARATION, not the call site.** The cost is
stated plainly: reading `drop f(x)` does not tell you a thread was created.
It is paid because a thread body OWNS an executor and an arena (D-034) while
a task does not — under D-032's pinning they are different kinds of
function, not one function used two ways — because `rg 'thread async func:'`
answers "how many threads does this program have" exactly and completely,
and because `main` already works this way.

### 2. `joins <const Duration>` — where the join deadline is stated

D-083 requires the deadline fixed where the executor is created, and
*reviewable*. It rides the declaration in the contract position, beside
`never fails`, and takes a CONSTANT expression of type `Duration` — an inline
`Duration{ ns: … }` or a named `const`. The program-level default applies
where the clause is absent, and that default is itself a stated constant, not
"whatever the runtime felt like".

### 3. The executor becomes per-thread state

One `%npk.exec` struct — ready queue, sleepers, park word and request, join
deadline, origin-chain ring, frame arena (D-034) — reached through **one
`%fs`-relative word** installed by `CLONE_SETTLS`, honouring the ABI's
`%fs:0 = self` convention and using `%fs:8` for our pointer.

Chosen over LLVM `thread_local` globals deliberately: this program is static
and freestanding and owns its own `_start`, so the initial-exec model would
mean the runtime parsing `PT_TLS` and installing TLS blocks itself — a
strictly larger trusted computing base for the same result, and this project
has one Astrée run to spend.

**What does NOT move:**

| State | Scope | Why |
|---|---|---|
| ready queue, sleepers, park word | per-thread | sharing one would migrate tasks, which D-032 forbids outright |
| join deadline | per-thread | D-083: fixed where the executor is created |
| frame arena | per-thread | D-034, and it is what keeps the arena single-threaded |
| origin-chain ring | per-thread | two threads' in-flight errors interleaved into one history would make the diagnostic fiction |
| **the freeze flag (D-063)** | **GLOBAL** | "a trap is a whole-program event; no task is ever resumed after one" — a per-thread freeze would let siblings run on against unknown state, which is exactly what D-063 refuses |
| the heap | global | already thread-safe (0.10), and per-thread heaps would break `wild` ownership transfer |

### 4. Creation and the join, concretely

- **`clone(2)`**, not `pthread_create` — the zero-dependency rule, and the
  runtime already owns `_start`, guard-paged mmap and `exit`.
- **The stack** is one mmap with a `PROT_NONE` guard page below it — the
  three-region shape `wildx` already uses. Size is a stated constant (2 MiB).
- **`CHILD_CLEARTID` IS the join.** The kernel clears a word and futex-wakes
  it at thread exit; the joining scope waits on that word under the deadline.
  This is what makes D-083's "there is no thread handle" implementable rather
  than aspirational — nothing needs to be named to be joined.
- **`hardware_concurrency`** is `sched_getaffinity` (the prototype hardcoded
  `4` — D-073).

### 5. Failure

A thread's root task is a task: its error rides the frame, the join collects
it, and the first child error relays into the spawning scope verbatim
(D-080/D-177). A TRAP is D-063 unchanged — global freeze, `failsafe` on the
trapping thread as a plain call, `exit_group`.

### 6. Two amendments this forces

- **`Thread.sleep_ns/ms` is STRUCK, not reimplemented.** D-073's row predates
  D-071: under "all blocking is task suspension", `await sleep(within)` is
  the one way to wait, and a thread-blocking sleep would be a second
  mechanism, invisible at the call site, that stalls every sibling task on
  its executor — the precise hazard D-071 was written against.
- **`atomic<T>`'s minimal set is owed by the thread half**, not only by
  channels: the park word is written by one thread and read by another.
  Relaxed load/store plus one compare-exchange; the full permitted-`T`
  question stays with C-9's channel half.

## D-182 — Channels: endpoints are handles, `channel()` constructs, `Work` replaces the job, and `atomic<T>`'s set — **SETTLED**

C-9's channel half, ratified from `meta/roadmap/1.1/C9_CHANNEL_STUDY.md`, and
with it the last of cycle 1.1's decision load. D-072's surface —
`Channel<T, LEVEL, CAP>`, `send`/`recv`/`close`, mandatory deadlines, no
`select`, no `try_*` — is untouched. This settles what D-072 left as prose.

### 1. An endpoint is a HANDLE, not a borrow

**This corrects two sentences already in the spec set**, and the correction
is the decision's centre. D-072 §Endpoints says endpoints are "second-class
borrows"; `CONCURRENCY_REFERENCE.md` §7 says a reply endpoint cannot travel
in a message "because an endpoint is a second-class borrow and borrows may
not cross a thread spawn". Both are true together, and together they make
channels **unusable for what channels are**: `drop producer(ch)` is a spawn,
and D-180 kept the spawn ban on reasoning that stands — two tasks holding
borrows of one storage is a mutation the holder cannot see.

The rule was the error, not the ban. **An endpoint is an opaque, copyable
value — an index and a generation — naming a channel whose storage the
runtime owns**, exactly the shape `Handle<T>` (D-152) and the kernel
identifiers (D-042) already have.

| Property | Consequence |
|---|---|
| not an address | the aliasing hazard does not exist; an endpoint may cross a spawn, ride in a message, or be sent through another channel |
| generation-checked | a stale endpoint is `StaleHandle` (−4106) — a catchable error, never a dangling read |
| not reference-counted | D-072's rule survives intact; nothing is freed by an endpoint going out of scope |
| lexically bounded | the CREATING scope still owns the channel's life; scope exit closes and reclaims, and D-062 has already joined every task that could hold one |
| copyable | which is what makes D-072's own fan-in answer — several producers, one channel — expressible at all |

`CONCURRENCY_REFERENCE.md` §6.4 and §7 are amended: `ask` remains as
convenience, no longer as a workaround for a rule that no longer bites.

**What the generation guards, and what it does not** *(added 1.1.10-B, from a
bug)*. The generation moves when a channel's SLOT is reused — when its storage
is handed to a different channel — so an endpoint kept past that point is
caught instead of aimed at a stranger's buffer. **Closing is not that.** Close
is a state change on a live channel: the slot, the buffer and every value still
in it stay where the holder left them, and a receiver has to be able to drain
them. The first implementation bumped the generation on `close`, which made
every outstanding endpoint stale the instant a producer finished — a consumer
mid-drain was told its handle was dangling, `StaleHandle` standing in for
`ChannelClosed`, a use-after-free report for an orderly end of stream. Two
errors that must never be confused: one says the program has a lifetime bug,
the other says the stream ended normally.

The reclaiming half is **not built**, and knowing why matters more than the
gap. "Scope exit closes and reclaims" is the managed regime's RAII, and the
backend has no managed drop at all yet — a channel would be the only one in the
compiler, and standing up one type's lifetime discipline ahead of the general
mechanism is the two-parallel-mechanisms trap the blueprint philosophy names
outright. So until the managed lowering lands (**B-6** in
`meta/roadmap/OPEN_DECISIONS.md`, recommended as its own cycle before
self-hosting), a channel's slot is never reused, the generation never moves
after `open`, and `StaleHandle` is consequently **unreachable from source** —
it can fire only on a fabricated handle. The check is correct and it is the
right check; it is guarding against a reuse that nothing performs yet. This is
recorded rather than quietly tolerated, because a safety property that cannot
currently fire is exactly the kind of thing that reads as tested when it is
not.

**What may ride a channel, until then** *(added 1.1.10-B)*. The same gap has a
second face. §6.3 already bars a borrow from being sent; the sharper problem is
an element that OWNS heap storage. D-065 settled that nothing moves by being
passed — ownership transfers only where `move` is written — so `ch.send(s, dl)`
on a `string` copies a body pointer and the far side becomes a second owner of
it. D-072 writes `send(move(v), deadline)` in its own signature, but nothing
*requires* the `move`, and requiring it would mean nothing until a drop exists
for it to suppress. So this rung refuses an owning element by name: what
transfers is what transfers by copying its bytes — scalars, `Handle<T>`,
channel endpoints, pointer-free structs (layout's own `haspt` answer, the same
question the escape analysis asks of the same table) and arrays of those, which
covers `CONCURRENCY_REFERENCE`'s own `Sample` example. When the managed
lowering lands, this is one of the refusals it retires, and the `move`
requirement becomes enforceable at the same moment it becomes meaningful.

### 2. `channel()` constructs, typed from context

```nitpick
Channel<Sample, 3i32, 64i64>:ch = channel()?;
```

A bare-name builtin reading its type from the annotation, exactly as
`arena_make()` does (D-152). It takes NO arguments — capacity and level live
in the type, which is what D-072 put them there for — and returns a
`Result`, because creation allocates and allocation can fail.

Rejected: `Channel.create(…)` (no other builtin generic carries a static
method) and a bespoke declaration form (a binding shape for one type is
meaning-by-context).

### 3. A job is a `Work` value; pools are generic over it

```nitpick
trait:Work = { func:run = NIL(Self:self); };      // async, per §5
ThreadPool<J, LEVEL, CAP>                          // J: Work
```

Closures are gone (D-018), so a job cannot be a captured environment — and
D-073's complaint about the prototype's `submit(int64, ?->, int64)` was
exactly that it erased the job type and zeroed the environment. A generic
parameter keeps the type, the captured environment becomes explicit struct
fields, and monomorphization makes the frame size known, which is what lets
a worker `await job.run()` at all. A pool is then D-073's own sentence: N
worker tasks receiving from one channel — no new primitive, no new runtime.

### 4. `atomic<T>`

**Permitted `T`:** integer widths up to 64, `bool`, and pointer-shaped
values. **Not** floats (an atomic FP RMW is a CAS loop the author should
write visibly), **not** aggregates (lock-free structs are a different and
much larger promise), and **not** `tbb` — its ERR taint is a computation
discipline, and a read-modify-write over a sticky sentinel has no specified
meaning.

Operations take an explicit ordering from the keywords the lexer already
carries (`relaxed`/`acquire`/`release`/`acq_rel`/`seq_cst`), and every one
is `never fails` (D-163): an atomic operation on valid storage cannot fail,
and wrapping it in a `Result` would make an increment a decision point.

**Exactly six operations, all SEQUENTIALLY CONSISTENT.** The study proposed
an explicit ordering on every call and **was wrong**: D-016 and
`CONCURRENCY_REFERENCE.md` §4.3 already settled that every high-level
`atomic<T>` method is SeqCst, with the five ordering keywords reserved for
low-level intrinsics that do not exist. The reasoning is this project's own
and stands: a misused weak ordering does not fail loudly or reproducibly —
it produces intermittent corruption that surfaces on a different CPU, under
load, months later, having passed every test written for it. That is the
numerical-drift failure class the safety case cannot tolerate, and SeqCst is
also what keeps data-race freedom provable rather than combinatorial.
Corrected during 1.1.10-A, before any of it was built.

```nitpick
counter.load()                        -> T     never fails
counter.store(v)                      -> NIL   never fails
counter.swap(v)                       -> T     never fails (previous value)
counter.fetch_add(n) / fetch_sub(n)   -> T     never fails (integers)
counter.compare_exchange(exp, new)    -> T     never fails (previous value)

int32:seen = counter.compare_exchange(exp, new);
if (seen == exp) { … it swapped … }
```

**Every read-modify-write returns the PREVIOUS VALUE**, `compare_exchange`
included. The study proposed a `Cas<T> { swapped, observed }` struct against
the bare-bool alternative, and the bool's defect stands — it forces every CAS
loop to re-read the location, which is slower and a second opportunity to
observe a different value. But the previous value carries the same
information with NO new type (for a strong compare-exchange
`previous == expected` **is** success) and gives the family one shape rather
than two.

**All are `never fails`** (D-163): an atomic operation on valid storage
cannot fail, and wrapping it in a `Result` would make an increment a
decision point.

### 5. Async trait methods: generics yes, `dyn` no

- **Through a generic parameter — supported.** Monomorphization gives each
  instantiation a concrete callee, so the frame size is known at the call
  site and D-177's `await` lowering works unchanged.
- **Through `dyn` — REFUSED by name, with the reason stated.** `await
  w.write(…)` on a `dyn Writer` needs the callee's frame size exactly where
  `dyn` guarantees the callee is unknown. The honest implementations are an
  allocation per call or a size in the vtable and an allocation anyway —
  both on the path D-153 exists to keep predictable. **This is a stated
  capability gap, not a hidden one**, and it stays available later.

### 6. CondVar

`timedwait` (the only form — D-073 removed `wait`) **releases the lock,
suspends the TASK (D-071), and reacquires before returning**, with the
reacquisition under D-056's level discipline like any other. A CondVar
carries the level of the lock it is paired with and the pairing is fixed at
construction, so no call site can get "which lock does this go with" wrong.

### 7. Actors are a pattern, not a primitive

No new syntax. With §1's endpoints and §3's `Work`, an actor is a struct plus
an async loop receiving from its own mailbox, and `ask` is a helper over a
reply endpoint carried in the message. A third spawn-shaped construct beside
`drop f(x)` and `thread` is what the blueprint philosophy refuses.

## D-183 — The managed lowering: RAII at scope exit, and a type with a drop is move-only — **SETTLED** (cycle 1.2; study in `meta/roadmap/1.2/B6_MANAGED_LOWERING_STUDY.md`)

Closes **B-6**. The memory model's DEFAULT regime is "managed — static
ownership, RAII at scope exit" and the backend implements none of it: nothing
is dropped at a closing brace, so the regime a program gets unless it says
otherwise is leak-until-exit. D-151 records that as a knowingly accepted
interim — "managed-regime storage whose RAII arrives with the managed
lowering" — and 1.1.10 is where the interim ran out.

### Why it could not wait for self-hosting

A `Mutex<T, LEVEL>` hands out a guard, and `CONCURRENCY_REFERENCE` §9's own
example ends `}   // guard drops here; the lock is released`. **A guard is the
first type whose entire meaning is its scope.** Without a drop it never
releases and every `Mutex` deadlocks on its second acquisition; closures are
gone (D-018), so no scoped-callback form can stand in. Channels needed none of
this because an endpoint is a copyable handle. Phase C was renumbered to insert
cycle 1.2 — the map is in `ROADMAP.md`, and the 0.10 note there explains why
the same renumber was declined once before and why the answer differs here.

### A drop is a generated function per type, and most types have none

`@"npk.drop.<T>"`, emitted once beside the type's other machinery. **A type
with no drop generates no call at all** — the feature costs nothing for the
scalars that dominate every program, and if a scalar-only function's IR moves,
the design has slipped.

Scalars, `Handle<T>`, channel endpoints, pointers and slices drop nothing: an
index is not an owner, a pointer is not an owner (`wild` memory stays manual
via `defer`/`dalloc`, which is what makes the regime explicit), and a slice is
a borrow. `string` frees its body **when it owns it**: a literal's body is a module
constant, and handing that to `npk_dalloc` is a trap. The capacity field
settles it — it is read by nothing today (not the runtime, not the emitter, not
the seed; the only writes are the three `insertvalue`s that build a string), so
**`cap == 0` means the body is not owned**. Literals are emitted with `cap = 0`
— they are not growable in place, the same fact from the other side — and the
drop is `if cap != 0 { dalloc(ptr) }`. Asking the allocator "is this yours?"
per drop was rejected: D-150's metadata could answer it, but it turns something
the compiler knows statically into a runtime lookup on every drop of every
string forever. An array drops its elements; a struct drops
its fields in **reverse declaration order**; an enum drops the ACTIVE variant's
payload; `T?` and `Result<T>` drop the inner when there is one; `atomic<T>`
drops as `T`; arenas release their slabs.

**A `Channel` drop reclaims the slot and bumps the generation**, which closes
the hole 1.1.10 had to leave open: D-182 makes an endpoint generation-checked
so a stale one is `StaleHandle` rather than a dangling read, but nothing
reclaims a slot, so the generation never moves and `StaleHandle` is
**unreachable from source**. Reclamation is what makes the check real.

`dyn` drops through a **drop slot in the vtable** (D-158/D-159's shape, one
pointer). Note the contrast with D-182's async rule as implemented at
1.1.10-D5: an `async` method cannot go behind `dyn` because the caller needs
the frame SIZE before it calls, and erasure removes it. A drop needs no size
from the caller — one call, one pointer — so it works where the coroutine does
not.

### Order: `defer` first, then drops; a trap runs neither

Within a scope, drops run in reverse declaration order. **`defer` bodies run
BEFORE the drops of the same scope**: D-080 lists both on the same exits
without ordering them, and a `defer` body can name the scope's bindings, so
dropping first would hand it freed storage. The pair stays legible — `defer` is
what the author wrote, drops are what the regime owes, and the author's code
runs while its world is intact.

**A trap runs neither** (D-014, unchanged). Every path that LEAVES a scope runs
its drops — the closing brace, `break`, `continue`, `pass`, `fail`, `relay`,
`return`, `exit` — innermost first when several are unwound. **A suspension is
not a scope exit** (D-177): the frame lives on and its locals with it, which
falls out correctly only if drops are emitted at scope-exit edges rather than
function-return edges. On `exit`, **drops run and THEN D-151's leak check**;
backwards, every clean program starts trapping the day this lands.

### A type with a drop is MOVE-ONLY — the part that changes the language

D-065 settled that nothing moves by being passed: ownership transfers only
where `move` is written. That was consistent while nothing was dropped. The
moment drops exist it is a **double free** — `f(s)` copies a `string`'s body
pointer and both the caller's binding and the callee's parameter drop it.

So: **passing or assigning a value whose type has a drop, without `move`, is a
type error** naming the type and the reason, and the source is invalidated by
the machinery D-065 already has.

The alternative — deep-copying on assignment — was rejected as implicit expense
and implicit behaviour, which the blueprint philosophy refuses outright, and
because it makes the cost of a line depend on the type of a name declared
somewhere else. The chosen rule is explicit and greppable, and it turns
something that is currently a *lie* into a *fact*: D-072 writes
`send(move(v), deadline)` in its own signature, and 1.1.10-D found that nothing
required the `move`. Under this rule it is required — and 1.1.10-B's rung
refusing channels whose element owns heap storage is **retired** rather than
made permanent.

**This will make existing code fail to compile until a `move` is written**, and
that is the correct direction: every such site is a place where two names
believed they owned one thing. The sweep gets the 0.8.0/1.1.0 treatment — land
the rule REPORTING, measure the real debt, sweep, then flip it to refusing.

### `pass` MOVES, always — the one implicit transfer, and why it is not an exception

*(1.2.1e.)* `pass v;` hands the value to the caller and control leaves the
frame, so the binding cannot be read again on any path. There is no second
owner to create and no ambiguity to resolve: **`pass` always transfers**, in
every function, for every type. That is one rule with no exceptions, which is
what the blueprint philosophy asks — it is not "sometimes a copy, sometimes a
move", it is a construct that means one thing everywhere.

So a return needs no `move(...)` written on it, and the scope-exit drop skips
what a `pass` carried out, exactly as it skips what `move` took.

The alternative — requiring `move(v)` on every return — was measured before
being rejected: it is **313 of the 459** sites the rule fires at, on the most
common statement in the language, to say something the construct already says
unambiguously. Ceremony that carries no information is a cost with no
purchase.

### The getter answer, and `Clone` (1.2.2)

The "getter over a container of owning values" question closed without new
surface: the 0.8.1 escape refinement already permits a borrow to travel up ONE
frame when rooted at a parameter, so a getter is `T->(C->:c)` returning
`$$i c.field` — the constructor pattern, now named as the getter pattern too.
Where a sentinel return does not fit a borrow, the getter returns an INDEX and
the caller reads in place. And `string` now implements the prelude's `Clone`:
the one spelling for a deliberate second owner — explicit, greppable,
allocating — with `raw string_concat(x, "")` as the same idea inside
`never fails` emitters, since `clone` may fail and an emitter may not.

### The open half: what a read-only parameter of an owning type is

*(1.2.1a, from measuring the rule against the compiler; corrected at 1.2.1e.)*
Wired to every value slot the rule fires at **459 sites**, and at **146** once
`pass` moves implicitly. Those 146 are the genuine ownership transfers, and
they sit in the backend's IR emitters rather than the frontend.

*(The figure first recorded here was 256 across five frontend files. That was
the DIAGNOSTIC CAP, not the debt — `DiagList` holds 256 — so it was the first
256 diagnostics in emission order, which walks the frontend first. A debt
reported at the cap is the cap. Measured again with the cap lifted.)*

Reading them showed a by-value parameter of an owning type means two different
things today:

- `strtab_add(t, data)` **stores** its string, so the caller must transfer and
  `move` is exactly right.
- `string_eq(a, b)` only **reads**, and demanding `move` there would invalidate
  the caller's binding for a call that never took ownership — correct code
  turned into a use-after-move to satisfy a rule about a transfer that did not
  happen.

So the move-only rule needs a companion: a convention for read-only parameters.
`$$i string` — the second-class borrow (D-004) — was ratified for it and then
**tested and found unworkable** (1.2.1b): a borrow needs a place, a literal has
none (D-146), and so a `string->` parameter cannot accept `"main"`. The
compiler is built out of `string_eq(name, "main")`; the convention fails for
exactly the functions it exists to serve.

Which polarity is available turns out to be forced. If the callee OWNS a
by-value parameter, every caller must `move` — thousands of sites, and
`string_eq(move(a), move(b))` destroys the caller's bindings, which is wrong
rather than verbose. So the callee must NOT own by default, exactly as D-065
already says, and the marker needed is the rare one: *this parameter takes
ownership*. It does not exist. `move T:p` does not parse; `nodrop T:p` parses
but is the wrong polarity.

**Recommendation: a parameter modifier spelled `move`** — the same word the
call site already uses for the same event, at the other end of it. **OPEN: a
grammar change, and therefore the user's call. Cycle 1.2 is blocked on it**,
since the drop calls wait on the move-only rule and the rule waits on this. The
rule is implemented and gated meanwhile.

### `move T:p` — the consuming parameter, and what it narrows in D-065

The marker is spelled **`move`**, in declaration position on a parameter: the
same word the call site already writes for the same event, at the other end of
it. `f(move(x))` transfers, and `func:f = R(move T:p)` is where it arrives.

**The default is unchanged and is D-065's own rule**: passing transfers
nothing, so an ordinary parameter is a value the callee may read and may not
keep. Marking the RARE case rather than the common one is what keeps
`string_eq(name, "main")` — and every literal argument in the language —
spelled the way it always was, which the borrow-shaped alternative could not do
(1.2.1b: a borrow needs a place, and a literal has none).

**This narrows D-065's second half, and the narrowing is deliberate.** That
decision is titled "`move` is an operator, not a memory qualifier" and removed
`move` from `MemoryQualifier` on the grounds that it "is not a qualifier and
never was". Both halves of that reasoning survive: `move(place)` remains a
keyword operator with a parenthesized operand, and `move` is still not a MEMORY
qualifier — it says nothing about how storage is managed, which is what
`wild`/`wildx`/`stack` say. What D-183 adds is a different thing that happens
to occupy the same grammatical slot: a statement about **how this binding was
initialised** — by a caller who gave up ownership. D-065 removed it because in
2024's design nothing in declaration position needed it; drops are what create
the need.

Mechanically it rides `p_qualifier_bit`, so it parses wherever a qualifier can
and is REFUSED (`NITPICK-MOVE-004`) anywhere but a parameter — the same
parses-everywhere-refused-where-meaningless shape `nodrop` already has, and the
house rule that the parser never restricts.

### `exit` runs defers and NO drops (amended at 1.2.3)

The earlier text here said drops run on `exit` before D-151's leak check. Built
and measured, that was wrong twice over. The process is ending, so wholesale
reclamation is the kernel's job — Rust's `process::exit` runs no destructors on
the same reasoning — and `exit` is the CONTROLLED SHUTDOWN (D-013), the one
path that must not fail: walking the entire live program state to free it on
the way out adds failure modes to exactly the path that exists to have none.
Stage 1's own exit walked the whole pipeline struct and died in the heap
validator, which is the demonstration. Defers still run on `exit` (D-014's
list is unchanged); the D-151 check still covers what it always covered, the
WILD family, whose discipline is manual.

### Views, and the runtime's obligations to them (1.2.3)

The `cap == 0` ownership bit is load-bearing at runtime, and every runtime
producer of a string header must now answer "who owns this body?" in the
capacity field. `string_slice` and `string_from_bytes` are VIEWS and say
`cap = 0`; `string_concat`, `int_to_string`, `read_file` and `read_stdin` are
owners and say the allocation's true base and capacity — `int_to_string`
having been caught returning an INTERIOR pointer with `cap = len`, which the
first drop handed to `dalloc` and the allocator rightly refused. The SOURCE
TEXT is the canonical case: the manager owns every file's body for the
process's life (the intern table and every span are views into it), so
`srcmgr_text` and everything above it hand out views, and nothing else may
ever own what they return. `npk_dalloc` poisons freed payloads (0xAA) as a
standing instrument, because a freed-while-shared body is otherwise invisible
until the allocator happens to reuse the chunk.

The obligation runs the other way too, and arming aggregate drops found the
one place the compiler violated it: **a helper that derives a string from a
LENT parameter must hand back either an owned body or a view whose base
outlives every client** — `result_ll_value_half` passed a `slice_proven` view
of an operand IrVal's `ll` out to become the RESULT IrVal's `ll`, which was
sound for as long as IrVals never dropped and a use-after-free the moment
they did. Poison alone did not catch it, because the freed chunk was recycled
into a live line-buffer before the stale read. What caught it is the second
standing instrument this incident adds: **`@npk_quarantine` in the runtime**
— when set, a freed small chunk is poisoned, stamped and NEVER reused, so
every stale read yields deterministic 0xAA at its first occurrence and every
stale free traps on the header magic, independent of allocation timing; a
poisoned-source tripwire in `npk_string_concat` (active only under
quarantine) then turns the first stale READ into a trap whose backtrace names
the reader, and one conditional breakpoint names the freer. It ships OFF —
reclaim is the shipped behaviour — and exists because a by-tid bisect of
drop classes proved to be a WEAK oracle: enabling any drop set shifts the
allocator's recycling pattern, so a configuration's cleanliness proves
nothing about the code it enables. Quarantine is timing-independent.

### The `dyn` cell, and the vtable's slot 0 (1.2.4)

Behind `dyn` the concrete type is erased, so scope exit cannot name the drop —
**every vtable's slot 0 is the concrete type's drop** (null when it owns
nothing), and the methods sit at declaration index + 1. D-158's slot IS still
the declaration index; the +1 lives at the one place an index becomes a table
offset. Every (impl, trait) table of one concrete carries the same slot 0,
which is what keeps D-159's widening sound: any retained word can drop the
value.

The coercion itself changed shape. The first lowering spilled the concrete
value to the coercion site's own STACK, and a `dyn` is storable everywhere a
value is (1.0.6e closed that matrix) — so `dyn_slots.npk`'s `choose` returned
one and main dispatched through a dead frame, passing only while nothing
scribbled the stack in between. **A coercion now moves the concrete value into
a managed heap cell the `dyn` owns** (`npk_alloc_managed`, the runtime's
managed entry): `type_drops` answers true for TY_DYN unconditionally — whether
the CONCRETE also owns something is slot 0's business; the cell must be freed
either way — so a `dyn` is move-only like every owner, widening consumes its
source, assignment over a live `dyn` drops the old cell first, and the
generated `dyn` drop calls slot 0 on the cell and hands it to `dalloc`. The
cost is one small allocation per coercion, paid at an explicit type-erasure
site; the alternative was a fat pointer into whichever frame happened to build
it. A `dyn` moved into a coercion ARGUMENT is a temporary, and temporaries do
not drop yet — that cell rides the recorded statement-end-drops debt. Channels
of `dyn` stay behind the owning-element rung until 1.2.5.

### Async bodies drop, and the flag crosses with the value (1.2.4b)

A coroutine ran no drops at all until this subcycle — a drop flag was an
alloca and an alloca dies between `resume` calls, so the machinery was gated
off rather than silently wrong. Three pieces closed it:

- **The suspend walk widened.** A droppable local is USED at its scope's
  exit — the drop reads it there — so its life runs past its last textual
  mention. The checker records which var-decls declare owning types
  (`ExprTypes`, beside the crossing marks), and the walk extends such a
  local's use to the function's end: the same conservative position `defer`
  bodies and `@x` already take, under the walk's doctrine that source order
  may only err toward MORE crossing.
- **A crossing local's flag byte crosses with it.** The frame gains one i8
  slot per owning `move` parameter and per owning crossing var-decl,
  appended after the crossing region so every established index keeps its
  meaning. Parameter flags are seeded `1` in state 0 — it runs exactly once
  per task, so no spawn site has to know the layout; a var-decl's flag needs
  no seed, because its declaration stores it and dominates every read
  dynamically, reused frames included. A local the widened walk did NOT
  frame is suspension-free to the function's end, so its alloca flag and
  every read of it sit inside one resume call.
- **Every unwind runs the same code.** Completion, `pass`/`fail`, and the
  cooperative wind-up (D-062: a wound task learns at its next resume) all
  route through the defers-then-drops walk, so drops-on-cancel cost no
  per-state teardown machinery: the resume that notices the wind-up unwinds
  the body's own scopes.

Driving the wind-up for the first time found two dormant defects beside the
drops: `npk_windup_all` DUE'd a wound sleeper without rousing its owner
executor — a mark nobody looks at while that executor futex-waits toward the
task's own far deadline is not a wind-up — so it now follows the channel
waker's protocol (due-now `1`, park word, futex wake); and the join's GRACE
wait pumped the joiner's own executor even for a THREAD child, which found
nothing ready, nothing sleeping, work outstanding — the deadlock trap. The
grace wait now forks on the child's kind exactly like the first wait.

The rouse SHARPENED the wind-up semantics: **a wound task cannot linger in a
wait.** Due-now is sticky through `npk_sl_push`, every park returns
immediately, and the wind-up propagates into each new awaitee at resume — so
a task that swallows its wind-up (`?|` means one thing everywhere) and has
FINITE waits left drains them and completes cleanly inside the grace, while
one that loops without bound spins until the grace expires and the join
traps `DeadlineExceeded`. Before the rouse, one swallow put the task back to
sleep for a full fresh deadline and the cooperative path was dead on
arrival; executor_windup_trap.npk now spins to earn its trap, and
windup_drain.npk pins the courtesy half.

### The channel's life, and the heap's first lock (1.2.5)

**The creating FUNCTION's exit reclaims its channels** — after its defers,
drops and child joins, so every task that could hold an endpoint has
completed and nobody is stranded mid-drain. Reclaiming is what closing never
was: the generation moves (every outstanding endpoint answers `StaleHandle`
— reachable from source at last), the buffer is freed after a drain that
DROPS any owning element nobody received, and the slot enters a free stack
`open` revives — fresh buffer, ring cleared, generation bumped back to even.
The slot's chan STRUCT is immortal, and every operation re-checks the
generation UNDER the channel's lock, which is what makes a reclaim racing an
escaped handle's in-flight op safe: the blocked op wakes to the re-check,
never to freed memory. Fresh channels start at generation 2, so an all-zero
handle (a zeroed field, a failed call's zeroed value half) aliases nothing.
It is a creation-site finalizer, not a value drop — endpoints are copyable
non-owners by D-182, so no value can say when the life ends. Two named
edges: a `channel()` inside a LOOP refuses (OPEN_DECISIONS C-22 — one stash
per site would reclaim only the last iteration's channel; per-scope joins
are the honest prerequisite), and a function whose RETURN TYPE carries a
channel is a FACTORY — `pool_create` returning the pool that holds one — so
its creations are handed to the caller and live to process end. **Open**: an
explicit ownership marker for factory channels (language surface, the
user's call), and the failed `send`'s element — the move happened, the
transfer did not, and the value leaks; an error that hands the element back
is the shape to weigh.

**Owning elements ride (the 1.1.10-B rung retired), and the heap took its
first lock for them.** A `string` sent across a thread is allocated on the
sender and freed by the receiver's drop — and the allocator's bookkeeping
was single-threaded by exactly the invariant the rung enforced. One futex
mutex now guards `alloc`/`dalloc`/`ralloc`'s table work; uncontended cost is
an atomic exchange each way, and correctness bought its keep first. What
still refuses as an element is a BORROW — `T->`, a slice, anything holding
one (`type_contains_borrow`, computed by layout beside `haspt` and `drops`;
`string`'s owned body excepted, `dyn` refused as erased) — because no move
can carry a borrowed target across a task boundary. The send REQUIRES
`move` of an owning place (D-072's own signature, finally enforced by
TYPE-046), and the reclaim's drain is emitted at the creation site, where
the element type is known.

### Arenas destroy at scope exit, and owning elements wait for views (1.2.5c)

`type_drops` answers true for `arena<T>`: the binding's scope exit drops any
still-live owning element (live is an even, non-zero generation — the
parity `arena_alloc`/`arena_free` keep) and calls `npk_arena_destroy`,
which nulls what it frees and skips what is null — so the explicit
`.destroy()` stays legal and idempotent. The slabs moved to the MANAGED
allocation role: RAII owns them now, and D-151's exit check keeps covering
only what stays manual. A plain arena cannot cross a thread (the channel
element rule refuses it), so the value drop needs no join ordering;
`shared_arena` is a pointer into storage other threads read, and its
teardown waits on the per-scope join machinery with the rest of C-22.

**An owning ELEMENT does not enter an arena yet, and the refusal closed a
live hole**: `get` and `put` copy the element's bytes, and since 1.2.3
armed drops, `string:s = a.get(h)` would free the arena's own body at `s`'s
scope exit and the next `get` would read it freed. `arena_make` (and
`shared_arena_make`) refuse an owning element type at the annotation. The
sound accessor is a **generated deep-VIEW per element type** — the cap==0
discipline made general, a `@"npk.view.<tid>"` family beside the drops —
recorded here as the open item that unlocks owning elements in arenas AND
a general owning-getter story; the drop-side machinery (the live-slot walk
in the generated arena drop, the `free`/`reset`/`destroy` pre-drops) is
already built and waits behind the refusal.

### `gives`, and what a failed `send` does with the element (1.2.6)

Both 1.2.5 open questions closed on the user's ratification, one of them
shaped by a design intent they stated for the record: **the long form
`return Result{…}` was retained alongside the `pass`/`fail` sugars so that a
return could one day carry BOTH a value and an error** — the sugars fill one
half each.

**A failed `send` DROPS its element.** The move handed it to the operation,
the operation never completed, and the language already has exactly one rule
for that shape: a callee that fails drops what it owns, the same thing
`run_param_drops` does for `move` parameters on every fail path. So the send
lowering drops the untransferred element on the error branch and on the
wind-up unwind; on success the ring took the transfer and the value slot's
bytes are a dead duplicate. Retrying with the same value takes an explicit
`.clone()` before the send — a visible cost. The long form is the RECORDED
ROAD to handing the element back (Rust's `SendError(T)` shape) if a real
need arrives: it inverts the taint rule — a value half meaningful on the
error path — so it waits for a named, distinct-type design rather than a
special case of `Result`.

**`gives` — the factory says so.** A function whose return carries a channel
writes `gives` after its parameter list (the `joins`/`never fails` clause
family, one new keyword). Marked, the CALLER owns the reclaim: every call
site stashes the result's channels — bare, or direct fields of the returned
struct; the shape rule refuses anything deeper — and the caller's exit
drains and reclaims them like its own creations, a zeroed failed-call handle
reclaiming as a stale no-op (generation 2 is the floor). Unmarked
channel-returning functions are GETTERS — views of channels owned elsewhere,
no reclaim anywhere new — and CREATING a channel inside one is a type error
naming the clause, which is what makes the silent process-lifetime factory
leak unreachable. Inside a `gives` function, a call to another `gives`
function is legal only in TRANSIT (the direct operand of `pass`/`return`):
anywhere else would scope the channels' reclaim to the middle frame while
the value is bound for the caller's. `drop f()` of a `gives` callee already
refuses through the NIL rule, and a `discard`ed result still stashes — the
channels are reclaimed regardless of what became of the value around them.
This supersedes 1.2.5's return-type-keyed factory exemption and closes its
open item.

### The rule comes BEFORE the mechanism, and the compiler proved it

The plan for this cycle had scope-exit drops landing first and the move-only
rule after, which reads naturally and is backwards. **A drop is only correct in
a language where ownership is unique.** Emitting drops while a copy still makes
two owners is not a leak fix, it is a use-after-free: a `string` handed to a
table is copied into it and then freed at the caller's closing brace, leaving
the table pointing at released storage — and the compiler's own `strtab_add`
does exactly that.

Measured rather than argued, at 1.2.0b: with the drop calls wired and the rule
absent, three string-heavy programs segfaulted, two more failed, and npkc could
no longer compile itself. The machinery is therefore built and deliberately not
called — one commented line, whose comment says why — and the move-only rule
lands first.

### Conditional moves get a drop flag, but only where the analysis cannot decide

A binding moved on one branch and not another can be neither dropped
unconditionally nor skipped. Where the bindings analysis (D-065) proves the
answer, the drop is emitted or elided outright; where it cannot, a one-bit
local is set at initialisation, cleared by the move, and tested at the drop. A
flag per owning local would be simple and uniformly wasteful, so **an
instrument counts the residue** — proving most of them away is most of the
value, and this is the one part of the design where measurement precedes
optimisation.
## D-184 — The reactor: epoll without timerfd; the task-identity rule for every wait — **SETTLED at 1.1.12a; B-3a closed**

**Mechanism.** The reactor is **epoll, and only epoll** — the proposal's
timerfd is not built. The executor already owns a deadline machine (the
sleeper list and the futex wait's absolute timeout); once the reactor is
armed, the idle wait becomes `epoll_pwait` whose millisecond timeout carries
the SAME deadline the futex wait took (rounded up, so a 1ns wait is not a
busy spin). A timerfd would be a second spelling of the sleeper list, one
more descriptor of state to verify, and no capability.

**Arming.** The first `io_ready` on an executor creates the epoll set and an
eventfd (both CLOEXEC; the eventfd nonblocking) and stores them in the
executor (`epfd`/`evfd`, slots 11/12; 0 = unarmed, safe because fd 0 is
stdin). The eventfd rides the interest set with payload 0 — the drain
marker. From then on that executor's idle wait is `epoll_pwait` forever.
Cross-thread wakers — the channel rouse and the wind-up rouse — write 8
bytes to the owner's eventfd AFTER the park-word/futex protocol they already
carry; the eventfd is published with a release store and read acquire. The
epoll branch re-checks the park word before sleeping — the futex wait's
kernel-side expected-0 check, done by hand — or a rouse landing between the
executor's re-check and the sleep, whose ping was skipped on a not-yet-
visible evfd, would sleep through a due task.

**Registration** is `EPOLLONESHOT` with the **task frame** as the event
payload: ADD, and `-EEXIST` answers MOD, which re-arms a one-shot. Delivery
(in the idle wait, owner thread only) dues the payload frame — `wake_at` 1,
the channel waker's own protocol — and the next sweep runs it.

**Surface.** `suspend_io(fd, events, abs)` is the inline suspension builtin
(register, then park exactly as `suspend_until` — one registration, one
park, one return, the same wind-up tail). The prelude's
`io_ready(fd, events, within)` is its `Result` face: `DeadlineExceeded` past
the deadline; success means READINESS WAS SIGNALLED — the caller re-tries
its syscall, and a retry that still would block calls it again.
`io_unwatch(fd)` (EPOLL_CTL_DEL; ENOENT is a no-op) is **deferred inside
`io_ready`**, so the registration lives exactly as long as the wait — a
one-shot left armed past its frame would fire into freed memory, and every
exit (readiness, deadline, wind-up) runs the defer.

**The kernel declining to watch is not the reactor's error.** A ctl failure
past creation — EPERM (a regular file: always ready by definition), EBADF —
marks the task due NOW and returns: the task resumes before its deadline and
the caller's re-tried syscall reports the same errno as an ordinary
`Result`, where the caller can handle it (D-179's posture). A file behind
`io_ready` therefore reads instead of trapping. Only creating the reactor
itself still traps: no descriptor, no caller, no `Result` to ride.

**The task-identity rule** — the correctness find the rung forced, and it is
not reactor-specific. Awaits drive child coroutines INLINE (D-177), so the
frame a nested wait is lowered in — a prelude helper's, `io_ready` itself
being the first — is NOT the frame the sweep sleeps and wakes: the task
root is. The executor therefore records the frame `npk_step` is running
(`cur_task`, slot 13; same-thread only), and **every waiter registration
resolves it** — the channel/mutex/rwlock/condvar/barrier link and unlink,
and the reactor's event payload — rather than trusting the frame they were
lowered in. Before this, a wake for any wait nested inside a helper
coroutine dued a frame nobody sweeps, and the task slept to its deadline
for a value that had already arrived — latent for channels only because
every existing test waited at task-root level. `nested_wait.npk` regresses
it (a helper-coroutine recv against a cross-thread sender, 40 runs);
`io_ready_basic.npk` proves the reactor (deadline on silence, then
readiness from a writer thread, on a nonblocking pipe, 40 runs).

**`sys` takes pointers.** The syscall trampoline is the one place an address
becomes a number: a `ptr` argument lowers as `ptrtoint` (the emitter
previously `sext`ed every non-i64 — invalid IR, latent because nothing had
passed a pointer). `=>!` still refuses ptr→int everywhere: an opt-out of a
check, not of a meaning.

**io_uring is not going in before Astrée** — a decision, not a deferral. An
SQE holds its buffer past the call's return, which makes the ownership story
categorically larger than epoll's (nothing here outlives the wait), and a
second reactor is a second thing to verify. If it ever lands it is a new
decision with its own verification, behind the same `suspend_io` interface.

## D-185 — Owned descriptors, `Path`, and the byte streams — **SETTLED at 1.1.12b**

**`OwnedFd` (TY 39)** is the owning descriptor: the value is the kernel's
i32, and the ownership is the point — its generated drop closes it at scope
exit, which is what IO_REFERENCE §6's "there is no `close` in the surface"
lowers to. Move-only like every owner (TYPE-046 covers the copy). `own_fd(f)`
takes ownership of a kernel `fd`; `release_fd(move o)` is the inverse for a
caller that must OBSERVE close's verdict — the move defuses the drop, the
floor's `close` reports, and no double close is spellable. `.value` reads
the number without consuming (the streams hand it to the floor quartet and
`io_ready`); writing it is refused — the number is the identity the drop
will close. The drop's own close deliberately swallows the kernel verdict:
a drop has no `Result` to carry it, and on Linux the descriptor is gone
either way; `close(release_fd(move o))` is the observing spelling.

**Scope-boundedness rides the borrow walk.** `type_contains_borrow` answers
true for `OwnedFd` — the same wall, for the same reason, as a `Guard`: the
scope that owns it closes it, and a boundary is where that scope would be
outrun. Consequences, all inherited from existing rules: a `Channel` cannot
carry one (or any struct holding one — the streams included; RUNG-001 at
the element), and a borrow of one cannot cross a spawn. **A MOVE into a
spawn is legal**: the join (D-062) bounds the thread by the creating scope,
so closing responsibility stays inside it — the owning-string precedent
(1.2.5b), and the sanctioned way to hand a stream's work to a thread. The
raw-number-then-own pattern (`own_fd` on the far side) is the cross-thread
construction form, exercised by `streams_pipe.npk`.

**`Path` is parsed, not passed around as a string** (D-051/D-054):
`path_parse(string) → Result<Path>` requires absolute, refuses interior NUL
and `..` above the root (an escape, not a path), and lexically normalizes
(`//`, `.`, `..`). Lexical only — D-054's symlink warning stands.

**No static methods exist** (TRAITS_REFERENCE §4.2 rule 1), so the
IO_REFERENCE's `Path.parse` / `ByteReader.open` / `TextWriter.create`
spellings cannot be built as written. **Constructor FUNCTIONS are the
settled idiom** — `path_parse`, `byte_reader_open`, `byte_writer_create` —
exactly as `channel()` and `arena_make()` already construct. The reference
is amended, not the language.

**The traits land as specified with two corrections** both forced by
settled rules: receivers are `Self->` (the spec's by-value `self` predates
move-only owners — a by-value receiver would consume the stream per call),
and deadline parameters are `Duration:within` (D-176's naming). Every
method is `async`, returns through `Result`, and takes a deadline; `read`'s
`uint8[]` slice carries its own length (D-070). EOF is the error `IoEof`
(the floor's E_EOF = −4096, D-141). `ByteReader`/`ByteWriter` hold an
`OwnedFd` and translate nothing; their `read`/`write` are retry loops over
the floor quartet and `io_ready` — `WouldBlock` (EAGAIN, 11) waits on
readiness for the time that remains, `Interrupted` (EINTR, 4) retries
immediately, and everything else — `IoEof` included — forwards verbatim
(`fail r.err`). The two errno names are prelude declarations in the
kernel's own code space; naming more of it is a decision for the code that
needs it. Seek takes the `Whence` enum (§7); `flush` on the unbuffered
byte writer moves nothing but keeps the trait's shape. Opening is the one
synchronous hop (epoll cannot wait for an open; io_uring is refused before
Astrée — D-184).

**Asyncness is part of a trait method's contract** (TYPE-048): an impl may
neither drop nor add the trait's `async` — an await through a bound drives
the impl by the TRAIT's word, and a sync body run through the coroutine
frame protocol is memory corruption, not a slow call. Found by probing at
this rung; latent since bounds could be awaited (1.1.10-D).

**Two defects this rung found and fixed elsewhere:** the awaited-method
child frame seeded a `Self->` receiver BY VALUE (the first pointer-receiver
await dereferenced its own fd number — the 1.0.9b address rule now applies
to the awaited form), and `string_slice`'s VIEW semantics let
`x = string_slice(x, …)` free the body the view points into — a silent
use-after-free the quarantine caught in `path_parse` itself. The prelude
copies before reassigning; the LANGUAGE-level hole (`string` cannot say
"view") is recorded in OPEN_DECISIONS as a pre-Astrée decision.

## D-185 addendum — the text layer, and buffering as a TYPE — **SETTLED at 1.1.12c**

**Composition is types, never modes.** `TextWriter<W: Writer>` translates;
`LineBufWriter<W: Writer>` buffers by line; stacking them is spelling the
policy in the type — a `Buffering` mode field would be the tag-selects-
behaviour shape D-072 rejected for channels, and D-076's "never inferred
from a terminal" is honored by construction: the std constructors BAKE §4's
fixed policies into their return types. `LineEnding` (`Lf`/`CrLf`) is a
creation parameter held in the writer; readers translate unconditionally —
`\r\n`, `\n` and a lone `\r` are one break, a `\r` split from its `\n` by a
refill boundary included (the reader carries the pending flag;
`text_pipe.npk` forces the split across a live pipe). A final unterminated
line is a line; `IoEof` only when nothing remains. The text interface is
free generic functions (`text_write_str`/`text_write_line`/`text_flush`/
`text_read_line`) — D-161's rule (an inherent impl over a family cannot be
spelled), with the parameter inferred from `@receiver` (D-064).

**The standard streams** (`std_in`/`std_out`/`std_err`) are constructors a
program calls in `main`'s scope and passes down — not globals (§8). Each
OWNS A DUP (`F_DUPFD_CLOEXEC`) of the inherited descriptor, so its
scope-exit close can never close 0/1/2 from under anything else, and
repeated construction is safe by construction. `std_out` returns
`TextWriter<LineBufWriter<ByteWriter>>` (line-buffered always), `std_err`
`TextWriter<ByteWriter>` (unbuffered always), `std_in`
`TextReader<ByteReader>`. The inherited descriptors stay BLOCKING —
`O_NONBLOCK` would ride the shared open-file description into the parent
process — so a write to a stuffed stdout pipe blocks the thread, bounded by
the consumer: the ONE stated exception to D-071, recorded here and in the
prelude. A partial line in a `LineBufWriter` does not flush itself at scope
exit (drops are generated, never user hooks) — finish lines or `flush`;
what a TRAP loses is exactly the partial line (§4.1's posture, unchanged).

**`string_bytes(s) → uint8[]`** joins the floor: the string→slice bridge,
a borrowed view (same pointer, same length, no copy) — D-070's borrow rules
own everything after it.

**Three compiler defects the layer forced out, all fixed:**
- **The raw-template-field-resolve class** (three sites): the drop-body
  generator, `gives_shape_ok`, and escape's `dest_can_hold` each re-resolved
  a struct TEMPLATE's field node with no generic binding — for a generic
  instance's field (`BW:inner`) that reported "there is no type named `BW`"
  against a correct program. Latent until drop generation first cascaded
  into a generic instance (a struct whose field is itself one:
  `TextWriter<LineBufWriter<ByteWriter>>`). All three now go through
  `struct_field`, the walk that binds the instance's arguments first.
- **Struct-literal vs block lookahead**: `joins JOIN_2S {` swallowed a
  thread body whose first statement was `ByteWriter:w = ...` as a struct
  literal — `Ident { Ident :` is the literal's whole signature, and a
  declaration matches it. A field's value can never be followed by `=` or
  `;` (assignment is a statement), so the fourth token settles it.
- **Awaited bound-calls in generic bodies**: the checker records the
  TEMPLATE's method symbol (`Wrap<W>:Writer.write`), and the awaited-call
  frame builder trusted it over substitution — building a frame type
  nothing emits. Substitution now outranks the recorded symbol whenever the
  receiver's type mentions a parameter, with the receiver's pointer level
  peeled before naming, as `.` always peels (D-098).

## D-185 second addendum — async methods behind `dyn` — **SETTLED at 1.1.12d; 1.1.12 COMPLETE**

**The vtable answers what erasure asked.** An async method's slot holds the
concrete coroutine's RESUME function (no thunk — there is no synchronous
shape to wrap), and a SIZE TAIL after the method slots holds one entry per
async method, in declaration order: a pointer to `@"npk.fsz.<frame>"`, an
i64 the frame's definition site emits beside itself (a gep constant over a
named type demands the definition first; a global forward-references
freely — the vtable is emitted before the frames it names).

**The caller builds the frame from the TRAIT's shape.** Every coroutine
frame is header (12 fixed slots) + parameters at 12+, and for one trait
method those offsets are identical across impls — the receiver is a
pointer and the signature is the trait's; only the locals tail differs,
and the size entry covers it. The await site allocates the vtable's size,
writes the header exactly as `fnem_frame_init` does (slot 0 = the slot's
resume), stores the dyn's DATA POINTER as parameter 0 and the arguments
after it through a per-site synthetic PREFIX type, and drives the standard
inline loop — the resume re-read from the frame's own slot 0 at each
entry, so nothing SSA straddles a suspension. Wind-up, result, and
reclamation are the ordinary awaited-child protocol, unchanged.

**Object safety, amended (rule 4 narrowed).** An `async` method is
object-safe WITH a `Self->` receiver — the erased data pointer is what the
frame can carry. A by-value `Self` receiver is the one part of the
signature whose layout stays concrete, so it alone still disqualifies an
async method. And rule 2's receiver exception now admits `Self->` itself
(one pointer level, exactly): pointer-to-Self IS the erased-safe form —
which also unblocked every `Self->`-receiver trait behind `dyn`,
`Reader`/`Writer` included.

**A latent sync defect fixed on the way**: thunks loaded the concrete BY
VALUE for every receiver, so a `Self->` method called through sync `dyn`
dispatch would have been handed its own first bytes as an address. A
pointer receiver now takes `%d` itself, unloaded.

**The §1.1 payoff stands**: one `report(dyn Writer:sink, …)` writes
through erasure, and WHICH writer decides what lands — `dyn_stream.npk`
proves it with a plain `ByteWriter` and a family-instance
`TextWriter<ByteWriter>` (CrLf), the same erased call leaving `\n` in one
file and `\r\n` in the other. Adopting `dyn Writer` inside npkc's own
diagnostics is 1.4 self-hosting work (a compiler-internals refactor), and
is recorded there, not here.

## D-186 — `string_slice` returns an OWNED COPY; overwriting an owning field or managed element drops the old value — **SETTLED (user-ratified)**

**The slice.** `string_slice(s, lo, hi)` returned a VIEW — `cap 0`, a
pointer into the source's body — and `x = string_slice(x, lo, hi)`, three
ordinary tokens, freed that body out from under the result: a silent
use-after-free the type system cannot see, because view and owner share
the type `string` and the ownership bit is runtime state. The quarantine
caught it inside `path_parse` itself (1.1.12b; OPEN_DECISIONS S-1). Of the
candidates — (a) owned copy, (b) a distinct view type, (c) a shape
refusal — the user settled (a): **the copy costs one allocation and
deletes the whole class.** An empty slice allocates nothing (`len 0` is
never dereferenced; `cap 0` gives the drop nothing to free); OOM follows
the allocator's trap posture, as `string_concat`'s does.
**`string_from_bytes` deliberately stays the VIEW primitive**: it wraps a
buffer the CALLER owns (the lexer's decode buffer, a writer's live sink),
which is a different contract, stated at its definition — the one
remaining view-maker, explicit and greppable. *(1.5.1b step 2, D-249: `string_bytes` (1.1.12c) is the other view-maker, and both are borrows to the escape analysis now, by the reference's `Views` column.)*

**The overwrite.** Simplifying the prelude's slice-copy dances exposed
D-183's recorded partial-place item as a LIVE leak: `tr.acc =
string_slice(tr.acc, …)` — a FIELD target — left the old body behind on
every refill, because assignment-drop-old covered only flagged whole
locals. Now **a field target, and an element target whose base is a
managed array, drop the old value unconditionally** before the store:
sound because a live struct's fields are always live (construction fills
every field; partial moves do not exist) and a managed array's elements
likewise. A `wild` pointee stays the manual regime's; an element reached
through a pointer base is exempt. Proven observably by the b-stage
machinery in `overwrite_owned.npk`: 2000 overwrites of an `OwnedFd` field
and 2000 of an `OwnedFd[2]` element survive only because every old
descriptor's drop closed it — without the drop, EMFILE near 1024.
*(1.5.1b step 5, 2026-09-04, two corrections to the paragraphs above. First,
THE `pass` CLEAR IS GATED ON THE VALUE'S TYPE: the emitter's half of "`pass`
moves implicitly" cleared the root binding's drop flag for every `pass`
rooted at an owning local, `x.f` included — the whole-binding rule the
partial-move text states — but it did so for a COPYABLE `f` too, so `pass
h.n` over an `int64` field left `h`'s `OwnedFd` undropped on every call, from
1.2.3 until `List<T>` began to own and a function returning `xs.count` leaked
every list (`list_fds.npk`). The clear now asks whether the passed value's
type drops — substituted first, since inside a generic body the recorded
type is the template's `T` and the first build of the gate freed every
`List<string>` element twice — and a value that does not drop transfers
nothing, so the root keeps its flag; `move(h.n)` and a nested `pass
w.inner.n` are gated the same way (`pass_field.npk`). The whole-binding rule
for an OWNING projection is unchanged and its sibling-leak item stays open.
Second, "EMFILE near 1024" was true of the machine in 1.1.12b and not of the
one in 1.5.1b, whose session sets a soft descriptor limit of 1,048,576 —
every descriptor-exhaustion proof in the suite, `overwrite_owned.npk`
included, passed against the leaking build. Both runners now lower their own
soft `RLIMIT_NOFILE` to `nitpick.toml`'s `[limits] nofile` before spawning
anything (BUILD_REFERENCE §7.1), and `fd_ceiling.npk` measures that the
number reaches a program. Third, THE PARTIAL-MOVE ITEM CLOSES: a `move` or a
`pass` out of a FIELD or an ELEMENT leaves the type's canonical VACANT value
(D-225) in the place, and the aggregate stays live. Until this step the
emitter cleared the whole root's flag for a field move — every sibling
leaked — and, since the field overwrite above drops unconditionally, a field
moved out and then reassigned was dropped a second time: `saved =
move(r.env); r.env = move(frame);` in the resolver's own constant folding
freed `saved`'s list at its second line, a double free the compiler never
saw because its `main` exits without drops, found by the first unit tests
that let a resolver drop (`type_layout`, `type_generic`, `expr_types`, each
a SIGSEGV out of `npk_heap_bad`'s trap route over a corrupted heap). Now the
moved-out field owns nothing, the overwrite drops nothing, the scope-exit
drop releases the siblings, and a vacant List grows from zero
(`list_reserve`, which doubled from zero forever). `partial_move.npk` pins
the three shapes; raised for ratification as S-26 — **ratified 2026-09-04 as D-254**, with D-251's one exception: no move out of a sub-place of a LIMITED binding.)*

What REMAINS of D-183's partial-place item after this: destructuring
ownership and statement-end temporaries, unchanged; the field/element
OVERWRITE half is closed.

## D-187 — `#ptr_add<T>` is element-scaled; `atomic_from_ptr` is fused-only over the turbofish — **SETTLED at 1.1.13a**

**`#ptr_add<T>(ptr, offset)` — SETTLED.** The offset is in **ELEMENTS of
T**: it lowers to `getelementptr T`, so `#ptr_add<int64>(p, 1)` advances
eight bytes and byte arithmetic is `T = int8`. This closes the v3 driver
plan's §3.3 element-vs-byte question in favour of elements — the reading
that matches the lowering primitive and needs no scaling multiply. Typed
beside its `#wild_*` siblings (a `wild`-regime hash-builtin), lowered as a
gep; `ptr_add.npk` pins the two spellings to the same cell.

**`atomic_from_ptr` representation — OPEN, put to the user.** The v3 plan
(§3.2) flagged the storability of an aliased atomic as unspecified and
sidestepped it by "never store it, construct at each use". That resolution
does not survive contact with the parser: **the fused spelling
`atomic_from_ptr<int64>(p).load()` does not parse** — a bare-name builtin
carries no type argument today — so the element type has to come from an
annotation (`atomic<int64>:head = atomic_from_ptr(p);`), which is exactly
the STORED form the plan said not to use. And a uniform pointer
representation for `atomic<T>` is ruled out: it would break inline atomic
FIELDS (`atomic<int64>:hits;`), which are element-typed inline storage,
already lowered and tested (D-182). The live options are therefore a
language-level fork:

- **(A) the fused form, via a small grammar addition** — teach the parser
  to accept a generic type argument on a bare-name builtin call, so
  `atomic_from_ptr<int64>(p).load()` parses and dispatches through the
  pointer with NO stored aliased atomic. Makes "aliased atomics are never
  stored" structural (the plan's own wish), leaves the shipped inline-atomic
  representation untouched, adds no type — at the cost of one parser
  production and its node-kind coverage (a grammar change, hence the user's
  call). **Recommended.**
- **(B) a distinct `AtomicRef<T>` type** — no grammar change, but a new
  type on the concurrency surface and a second atomic-method dispatch path.
- **(C) a place-aliasing binding** — `atomic<T>:x = atomic_from_ptr(p)`
  where `x`'s cell-address is `p`; a new binding kind touching addr_of, the
  drop-flag machinery, and moved-from analysis.

**RESOLVED as option (A), the user's call.** And it needs no grammar
change: the **turbofish** is already the expression-position type-argument
syntax (D-064), and `atomic_from_ptr::<int64>(p).load()` parses on a
bare-name callee today — it only reached TYPE-016 at the typer. So
`atomic_from_ptr::<T>(p)` reads its element from the turbofish, returns
`atomic<T>`, and is **fused-only**: legal solely as an atomic method's
receiver, where the emitter dispatches through the pointer itself (no
`addr_of`, no local holds the cell); storing it — a declaration or an
assignment — is refused (TYPE-007), which makes "aliased atomics are never
stored" structural rather than a discipline. The place requirement the
inline atomics carry is lifted for the aliased receiver, since having no
place IS the property. `atomic_alias.npk` exercises load/store/swap/
compare_exchange through an alias; `atomic_alias_rules.npk` pins the two
store refusals. This unblocks the Bridge's ring access (1.1.13b).

## D-188 — The driver registry: published before the clone, walked on the trap path, and a clean exit never abandons a driver — **SETTLED at 1.1.13a**

The spawn/teardown half of the Bridge (D-149 over D-055; the v3 plan §4/§8)
landed with three properties the plan asked for and one it did not, ratified
by the user's standing "anything that makes things safer":

**The registry entry precedes the child's existence.** `npk_driver_clone_exec`
claims a slot (CAS, preallocated 16-entry .bss — failsafe cannot allocate,
D-014), prefills `pidfd = -1`, and PUBLISHES state 2 **before** the clone;
CLONE_PIDFD's parent_tidptr aims INTO the slot, so the kernel writes the kill
handle into registry storage during the clone itself. There is no instant
where a live child has no killable entry — v3 §4.2's "the entry outlives
every resource it guards", strengthened to birth. Retirement
(`driver_retire`) is teardown's LAST step, after kill/`waitid`; retiring a
slot that is not active is the registry's double-free and traps `-4102`.

**The trap path kills drivers before user `failsafe` runs.**
`npk_driver_kill_all` — SIGKILL via pidfd only, ESRCH-safe, allocation-free,
no reaping, no cleanup — is called by `npk_trap` itself ahead of
`@npk_failsafe`. Safing is mechanism, not policy (D-013); an uncontrolled
driver DURING failsafe is the hazard class D-055 exists for. The first
schedule that ever took the trap path with a live driver (the EPIPE run
below) demonstrated the walk killing the abandoned child.

**A clean exit never abandons a supervised process** — the addition. The
D-151 K-semantics exit rule extends to the second registry: `npk_exit` on
code 0 checks `npk_driver_live_count()` (before the `<wild-live>` check —
the graver defect names the trap) and refuses to report success while a
driver entry stands: trap `-4109`, prelude `error:DriverLeak = 4109`. The
registry, not the leak checker, tracks Bridge resources (v3 §4.5, adopted at
D-187), so this is that tracking made an enforced invariant rather than a
bookkeeping aid. `driver_leak.npk` proves it: spawn, `exit 0`, and the exit
becomes failsafe reporting `DriverLeak` — with the trap path having killed
the mock on the way, no orphan surviving.

**The child path is allocation-free hand-written IR** (the npk_thread
precedent): fork-shape clone (no CLONE_VM — the child continues in ordinary
IR on its COW stack copy), then exactly PDEATHSIG → recorded-parent check →
NO_NEW_PRIVS → the dup3 shuffle onto 0/1/2/3 → execve of a CONSTRUCTED
argv/envp prepared pre-clone — and `exit_group(127)` on any miss. Every
child-bound descriptor is re-homed ≥ 4 in the parent first, so the shuffle
can never clobber a source. The surface is two bare-name builtins
(`driver_clone_exec`, wrapped — the Bridge never traps; `driver_retire`,
void), BUILTIN_REFERENCE-generated like the rest.

**The library tier obeys "the Bridge never traps" literally now.** The first
EPIPE schedule (child's execve failed and it died before the parent's
`sendmsg`; ~1/300 under load) found `lib/nbridge.npk` using `?!` where it
meant "fail by name": `?!` is unwrap-or-TRAP-as (D-179 — right as a test
assert, wrong in this tier, and v3 §4.2 bars it from the Bridge by name).
Every library site is now bind-and-`fail`; the trap semantics themselves are
correct and unchanged. Found by stress: the marker's loop turned out to be
dead in the real-backend harness stage (it lived only in the seed-path
runner, which no concurrency program ever takes) — the loop now lives in
`check_emitted_program`, restoring `// stress: N` for the whole backend
suite. Fixtures joined the harness the same day: `tests/backend/fixtures/`
binaries are built by the real backend, held to a program's checks, never
run directly, and reach tests through `// argv:` substitution
(`mock_driver.npk`, the Nitpick mock the C reference driver will replace at
1.1.13c).

## D-189 — The Bridge's dispatch plane: the ring, the triple wait, the woven stderr drain — and the executor's spent-wake defect — **SETTLED at 1.1.13b**

Stage b of the Bridge (v3 §4.4/§4.5/§6/§10) landed dispatch, `bridge_close`
and the stderr story, with four calls the v3 text left open settled here,
and one executor defect the work flushed out of code that had been green
for a year of tests.

**`io_watch(fd, events)` is the registration half of `suspend_io`, alone**
— two arguments, no deadline: a registration carries none, the deadline
belongs to the one park that follows (`npk_io_register` is the whole
lowering). The prelude's `io_ready2` is the two-descriptor face; the
Bridge's dispatch composes a TRIPLE wait (ctrl reply, pidfd death, stderr
pressure — all EPOLLIN, one park). Every watched descriptor still owes its
`io_unwatch` on every exit.

**A dispatch deadline kills.** v3 §4.4 said "escalation (§5.4)"; the
settled reading is D-055 rule 1 taken literally — a hang is worse than a
crash, and with actuators live it ends NOW: deadline expiry SIGKILLs via
the pidfd, poisons the Bridge, fails `EDriverDeadline`. The graceful ladder
belongs to `bridge_close` alone (SHUTDOWN + write-shutdown → half the
budget → SIGTERM → a quarter → SIGKILL, whose `waitid` is bounded because
SIGKILL cannot be ignored). **A rung ends when the child is DEAD or its
budget is spent — never merely because a wait returned**: a resume says
only that something signalled, and the first draft that treated it as
proof of death sat 28 seconds in a blocking `waitid` on a live driver.
`close_wait_dead` — check, wait, CHECK AGAIN — is the rung shape.

**The stderr drain is WOVEN, not a task.** v3 §10 sketched a standing
drain task; under D-180 a second task cannot borrow the Bridge across a
spawn, and none is needed — the dispatch wait already wakes on stderr
readability, which is exactly when there is something to drain. The drain
runs at dispatch entry, on every wait wake, and post-mortem in teardown
(pipes outlive their writers); the ring is 8 KiB drop-oldest in the
Bridge; and because D-179 errors are nominal and carry no payload, the
tail is READ from the poisoned bridge (`bridge_stderr_tail`, an owned
string) rather than riding the error value.

**Close's ladder phase may be wound up; its release phase may not be
split.** The ladder's awaits are idempotent and leave `closed` unset, so
a wind-up mid-ladder hands the job to the caller's `bridge_reap` backstop
whole; the release (waitid, munmap, memfd close, errbuf free, retire — the
non-idempotent half) is synchronous, entered behind `closed`, and so
cannot be interrupted at a suspension it does not contain.

**And the executor had been due-ing every once-woken task forever.** The
1-stamp a waker writes into `wake_at` (channels 1.1.10-C2, the reactor
D-184, the rouse 1.2.4b) survives the registration-vs-sleep race by
`npk_sl_push`'s exchange — and nothing ever CONSUMED it at the resume it
caused, so the exchange faithfully re-preserved the stale marker at every
later sleep: after its first wake, a task never slept again — every wait a
busy-poll. Invisible under a year of retry-loop waits, which re-poll
correctly, just hot; found the day the mock driver's hang kernel SLEPT
after an io wake and returned instantly, turning a deadline test into a
fault test. The fix is one cmpxchg (1→0) at `npk_step`'s resume site: only
the due marker is consumed, a completed task's −1 and real timepoints pass
through, and a waker's 1 landing after the clear costs exactly the one
spurious resume the retry discipline absorbs. The same hunt found D-186's
`npk_string_slice` copy allocating from the WILD-TRACKED entry — string
bodies are managed-regime and belong to the internal one, as concat's
always did — which made any sliced string alive at `exit 0` (where drops
deliberately do not run, D-183/1.2.3) a phantom `WildLeak`.

## D-189 addendum — the spent-wake fix exempts WOUND tasks

The first full run after the consume-at-resume fix failed exactly one
test, and the failure was the design speaking: `windup_drain.npk`'s
documented contract is "a wound task cannot linger in a wait — every park
returns immediately", which is the persistent due-stamp doing its ONE
legitimate job — a swallowed wind-up must drain, not sleep out its grace.
So the resume-site clear is GUARDED: a task whose windup word is set keeps
its 1 (every park instantly due, the cooperative drain), and every other
task spends the marker (real sleeps after real wakes). The windup word is
stored before the due stamp and read after the sweep's seq_cst load, so a
wound resume always observes it. `windup_drain` (the courtesy half),
`executor_windup_trap` (the spin-forever-earns-its-trap half) and the
Bridge's sleep-after-wake tests now hold simultaneously.

## D-190 — `extern` blocks lower to generated driver stubs; the interface hash; the v1 wire vocabulary — **SETTLED at 1.1.13c**

The Bridge's final stage makes D-149 real: `extern` stopped being a rung
message and became the ONLY foreign-code mechanism the language has ever
shipped. The block is the INTERFACE RECORD — never a declarer: collection
binds nothing from it, and the expansion phase (the derive mechanism's
sibling, `bridge_stubs.npk`) generates ordinary Nitpick source that is
spliced before any name is bound — one `pub async func` stub per method,
plus `<block>_iface_hash()`. The stubs marshal into the sealed ring,
`await dispatch` under the caller's deadline, and unmarshal or fail;
because they are ordinary members, every downstream stage — typing, the
analyses, emission, the coroutine machinery — treats them with ZERO
special cases. The two backend rung refusals retired; the module declaring
a block imports `lib/nbridge.npk`, whose surface the stubs use.

**A method is declared IN FULL** — explicit over implicit: `Bridge->`
first, `Duration` last (the mandatory D-055 deadline), and the v1 wire
vocabulary between: parameters `int32`, `int64`, `int8[]`, `uint8[]`;
returns `NIL`, `int32`, `int64`. Everything else refuses by name
(NITPICK-EXTERN-001), including `opaque` members — the §11 wire-handle
tier rides the reserved LOAD_MODULE work, a settled part of the design
whose implementation belongs to that tier, not a deferral. D-002's
per-method contracts are dead as D-149 scheduled: the parser's mandatory
`fails on`/`never fails` LIFTED (the productions still parse — D-085; the
requirement ended, NITPICK-PARSE-006 retired unreused), and a contract
still written refuses as NITPICK-EXTERN-002. A wire status of nonzero
fails the stub `EDriverError` — the driver's own refusal as a value, with
stderr the diagnostic channel.

**The interface hash** is the connect-time answer to "built against a
different interface": FNV-1a-shaped over each method's canonical spelling
`name=ret(p1,...)` (the structural Bridge/Duration excluded), a `;`
folded per method, declaration order — which is also the kernel_id
space. It rides INIT_REQ (payload now `{u32 version, u64 shm_size, u64
iface_hash}`); a driver computes its own table's number and refuses a
mismatch before ACKing. **The offset basis is D-179's error-identity
seed, 0xCBF5DAE484222325 — deliberately the same constant for every
derived identity in the ecosystem** (and NOT the textbook FNV basis; both
of stage c's cross-implementation defects were transcription slips in the
NEW code — the C header's seed and the ring magic's decimal halves — each
caught by the C driver disagreeing with the Nitpick side, which is the
hash doing at build time exactly what it does on the wire).

**The C SDK and the conformance spine.** `sdk/npkdrv.h` renders the
protocol in C — the wire tables, the ring layout, the hash, and a
reference event loop upholding the §7.5 obligations. The harness builds
`tests/backend/fixtures/*.c` with the system C compiler (test tooling,
never in the artifact — the valgrind rule; a driver is outside the TCB by
definition) and hands the binaries to tests through the same `// argv:`
substitution. `extern_c_driver.npk` demonstrates the four conformance
claims end to end against the real C driver: the echo (scalars marshalled
through the ring's bulk area and summed back), the driver-reported
refusal (status 55 → EDriverError), the hostile tail (a free-running
index far outside [head−cap, head] written AFTER a clean completion — the
next dispatch's [UNTRUSTED] validation kills, EDriverProtocol), and the
stale interface (the same hash offered to a driver built against one
changed spelling — EDriverSpawn at the handshake, the mismatch on
stderr). `extern_stub.npk` pins the generated stubs against the Nitpick
mock; the EXEC_NOTIFY full-buffer retry and the SIGTERM-immune SIGKILL
fixture are recorded on the roadmap as the batched-dispatch tier's
obligations.

## D-190 addendum — D-004 rule 4 retires as an ANALYSIS rule; the wire vocabulary carries it

The first full run after the lowering failed five tests, and every failure
was the old FFI model's residue being cleaned out rather than a defect in
the new one. "A borrow may not cross an `extern` call" (D-004 rule 4,
NITPICK-BORROW-003) had refused for four cycles at the CALL — and D-190
made that subject unrepresentable: extern methods never bind as callable
symbols, so no call can cross into one, and nothing address-shaped can
even be DECLARED in a block (NITPICK-EXTERN-001, at the block, before
names bind). The rule's guarantee is STRONGER now — wire-vocabulary-shaped
instead of call-shaped, D-149's "only copies cross" made structural — and
its analysis mechanism is retired: the escape walk's extern plumbing
removed, BORROW-003's number retired unreused, the two rejection cases
kept as ACCEPTED shapes recording that the exact code which refused now
compiles. The parser's mandatory-contract unit tests flipped the same way
(omission is the legal form), and the defer-body escape case — whose point
was always that defers are WALKED — drives rule 3's direct store instead.

## D-191 — Cycle 1.1 closes: variadics and `..^` land, the await edges settle, G-3 becomes cycle 1.3, and Phase C renumbers again — **SETTLED at 1.1-close (G-3: the user's call)**

**User variadics and the spread are LANDED, not re-homed.** The close-out
sweep of the cycle's rung strings found the checker already implementing the
COMPLETE semantics (arity, element checking, `..^`'s position rules — the
frontend was built in full, exactly as the doctrine says) and only the
lowering missing. The spec's one rule — "`..*T[]:name` is a typed slice,
and a variadic call lowers to building one" — is now real: the callee binds
one more `{ ptr, i64 }` parameter through the ordinary machinery (sync and
async alike); the call gathers trailing arguments into a hoisted `[N x
elem]` (or takes `..^`'s slice whole, or passes the null-based empty
slice); and an AWAITED variadic callee gathers into the CALLER'S COROUTINE
FRAME (a role-30 slot the stage-D pre-scan sizes from the same call node),
because the child suspends with the slice in hand and a resume-stack array
would die under it. Boundaries, refused by name: a METHOD may not collect
(the spec's rule is written for functions, and the four dispatch shapes
window their arguments differently — a trait method likewise), `..^` needs
a variadic tail the checker can see (a builtin's fixed registers are not
one), and a variadic FUNCTION VALUE stays unspellable (the func-type
grammar carries no `..*`; the emitters' TY_FUNC_VARIADIC support waits for
a spelling).

**The await edges settled where design refusals live.** An await in a
`pick` arm's `where` guard refuses in the SUSPEND analysis (SUSPEND-003:
the guard runs between arm selection and entry, and nothing there survives
a suspension); the indirect/builtin await needed NO new rule — TYPE-043
already refuses an await of any non-async call, and an async function
VALUE cannot be spelled — so SUSPEND-004 was drafted and retired unused
the same day. The five backend await-edge rungs and the spread's became
internal belts (`iv_broken`): each guards a frontend promise, and a
compiler that breaks its own promise should say so as a defect, not as a
rung.

**Three latent defects, found by the first programs to exercise them.**
The variadic collector's NAME was never typable inside a body (nothing
before this had a body that read its rest — `sys`, the one variadic, has
none). A RANGE-VIEW was not counted as address-taking, so `arr[lo...hi]`
left `arr` on the resume stack while the framed view pointed at it. And
the deepest: stage D's "lives to the function's end" extension — the
address-taken rule, the defer-use rule — had been a SILENT NO-OP SINCE
BIRTH, because `fn_end` was read off the body block's span, which covers
only the opening region: any address-taken local whose last textual use
preceded a later suspension stayed a resume-stack alloca with live
pointers into it. The fix is the order's ceiling (the settle only compares
within one function), and the class it closes is the exact use-after-free
stage D exists to prevent.

**G-3 is settled, by the user: the exotic numeric tier is NEW CYCLE 1.3**,
inserted before self-hosting — the D-183 renumbering precedent applied a
second time (self-hosting 1.4, verification 1.5, Astrée 1.6), for the
standing reason: everything that is going in lands before the fixpoint
re-close and the one-shot verified artifact. The sweep: cycle folders, the
nine verification rungs (now "1.5"), the G-3 rungs (now "1.3 (G-3)"),
OPEN_DECISIONS' Blocks column and live forward references; `done/`
archives not rewritten. Cycle 1.1 — fourteen subcycles, D-163 through
D-190 — moves to `done/1.1/` with ZERO rung strings naming it.

## D-192 — `sys` is TYPED: `Result<int64>` by contract, register-shaped arguments, the right extension — **SETTLED at the 1.1 interlude (S-3)**

The S-3 close-out rows were two symptoms of one hole: `sys` calls carried
the bare-builtin UNKNOWN type, so a non-register argument (a `Result`, a
struct, a `bool`) sailed through the checker and died at llc as INVALID IR
(`sext { i64, i32 } to i64` is not an instruction), and `?|` over a `sys`
call died as an internal defect — the typer skipped the DEFAULT'S visit on
an unknown lhs, and the emitter had no unknown-operand fallback where `?!`
had carried one since 1.1.2.

Settled, all at the typer where the program can be told what is wrong:

1. **The call types as `Result<int64>`** — D-048's contract stated where
   the checker can consume it. The consumers follow for free: a `?|`
   default is checked against `int64`, `discard(sys(…))` refuses as a
   swallowed error (TYPE-039 — `io_ready_basic`'s wake write was one), a
   wrong annotation refuses at the binding, and the taint analysis sees a
   real `Result`.
2. **Every argument is one kernel register**: integer-family at ≤64 bits,
   a kernel identifier, or a pointer (1.1.12a's one sanctioned crossing).
   Anything else refuses TYPE-007 by name — including an UNKNOWN-typed
   argument (a nested bare-builtin call, whose value shape only the
   emitter would see): bind it to a typed name first. Arity is checked
   (the syscall number, then at most six registers).
3. **The extension matches the type**: a signed integer `sext`s, an
   unsigned one and the kernel identifiers `zext` — the old blind `sext`
   smeared a `uint32` flag word's high bit across the register
   (`sys_typed.npk` proves it with an lseek to 0x80000000). The emitter's
   non-register arm is now `iv_broken`, honestly: after rule 2, anything
   arriving there is the compiler's defect.
4. **`?|` gained `?!`'s unknown-operand fallback** — typer (the default is
   visited, with no expectation, even under an unknown lhs) and emitter
   (the slot's shape derives from the operand's own LL; `{ i32 }` is
   `Result<NIL>`) — because the REST of the builtin surface still types
   UNKNOWN, and the two unwrap forms drifting apart is how S-3b happened
   (`unwrap_unknown.npk` keeps the path exercised, aggregate value half
   included).

**The residue is named, not silent**: over a still-unknown builtin, a `?|`
default whose type disagrees with the callee's actual value shape (and any
argument shape mismatch against the floor's signature) surfaces only at
llc. The clean close is typing the whole bare-builtin surface from a
generated signature table — raised as an open proposal at this decision,
sized as its own subcycle, not slipped into this one.

## D-193 — Float and char `ToString` land: shortest-round-trip Dragon4 in the prelude, total UTF-8 with U+FFFD — **SETTLED at the 1.1 interlude (§6b)**

§6b closes with ZERO new floor surface — both conversions are prelude
Nitpick, reached through the same `ToString` impl lookup as every scalar
(the checker's stringability gate and the emitter's dispatch were already
impl-driven; nothing in the compiler changed).

**Floats** (`flt_bits_shortest`): the Steele–White/Dragon4 discipline over
`uint2048` — exact value = R/S with the half-gap boundaries, an EVEN
mantissa owning its boundaries (IEEE round-nearest-even), the last digit
rounding to the nearer with ties to even, subnormals and the unequal
power-of-two gap handled. Two properties are load-bearing:

- **No wide division exists in the algorithm.** Every ×10 is a
  shift-and-add and a digit is at most nine subtractions — so nothing can
  lower to a compiler-runtime libcall the zero-dependency scan would have
  to provide, by construction rather than by audit. `uint2048` holds the
  worst case (< 2^1130, subnormal floor included) with headroom.
- **The bits travel through memory** (store as float, load as integer):
  `=>!` is a VALUE conversion everywhere and must not quietly mean
  "reinterpret" for one pair of types (the blueprint rule).

Format policy: fixed notation while the decimal exponent is in [-4, 15]
with a mandatory ".0"/fraction; `d[.ddd]e±EE` (exponent two digits
minimum) outside it; "0.0"/"-0.0", "inf"/"-inf", "nan". This matches the
platform repr convention the workbench can generate vectors against:
`flt_tostring.npk` holds 238 flt64 + 115 flt32 known answers — curated
torture cases plus seeded-random BIT PATTERNS, values built from bits so
literal parsing is out of the loop — generated by python (a workbench
tool, D-085: never in the artifact).

**Characters** (`codepoint_to_string` + the three impls): TYPE_REFERENCE
semantics verbatim — `char32` a scalar value, `char16` a UTF-16 code UNIT
(a lone surrogate renders U+FFFD), `char8` a UTF-8 code UNIT (only ASCII
renders as itself; a non-ASCII unit is a fragment). The encoder is TOTAL
and never mints invalid UTF-8: out-of-range and surrogate inputs become
U+FFFD — an innocent-looking total function must not be the way an
invalid `string` enters a program. The owned copy rides D-186's
`string_slice` over a `string_from_bytes` view of a scratch buffer, which
is what unblocked this without the floor primitive §6b once waited on.

## D-194 — `simd<T, N>`: the complete v1 surface — **SETTLED (1.3.0 batch, user-ratified)**

The full proposal is `meta/roadmap/1.3/1.3.0.md` G-4; the operative rules:

- **Elements**: the plain integer widths ≤64, `flt32`/`flt64`, and `bool` —
  admitted because comparisons PRODUCE `simd<bool, N>` and results must be
  bindable. A bool vector supports `== !=`, indexing, `.all()`/`.any()`,
  nothing else. The twisted and ternary families are excluded: per-lane
  sticky-ERR select chains erase the hardware-vector win.
- **Lanes**: comptime N in 2..64, total size ≤ 64 bytes; alignment = next
  power of two ≥ size, capped 64. Odd N legal (vec3 is a real consumer).
- **Constructor**: `simd(…)` is TYPE-DIRECTED from the annotation (the
  `channel()` precedent) — exactly N arguments, or ONE which splats.
- **Operations**: elementwise `+ - * /` (`%`, `& | ^ << >>` integer-only);
  comparisons yield `simd<bool, N>`. Integer vector division carries D-007
  as ANY-LANE checks: a zero lane traps DivByZero, INT_MIN/−1 traps
  DivOverflow — one vector compare + reduce, not per-lane branches.
- **Indexing**: `v[i]` bounds-checked read/write; `.len` is N.
- **Reductions as methods**: `.sum()/.min()/.max()` (numeric),
  `.all()/.any()` (bool). **Implementation amendment (1.3.1)**: lowered as
  extract-and-fold chains rather than the `llvm.vector.reduce.*`
  intrinsics the proposal named — the chains need no per-shape `declare`
  lines, cannot become libcalls by construction, and ARE the ordered
  reduction float `.sum()` requires; `all`/`any` collapse through one
  `bitcast <N x i1> to iN` and one integer compare. A second amendment:
  `%` is admitted on FLOAT lanes too — scalar `flt64 % flt64` is legal
  (frem, 0.9.4), and a vector rule that differed would be the
  context-dependence the blueprint forbids.
- **Casts**: `simd<T,N> => simd<U,N>` elementwise under the scalar rules;
  N never changes; no scalar↔vector casts (splat is the one broadcast).
- **Shuffles are OUT by decision** (not deferral): no consumer in evidence
  (D-143), and a wrong permute API is permanent. Reopens only with a
  consumer on the table.
- No `ToString` — a builtin generic does not render (the channel rule).

## D-195 — `tfp*` lowers to native `iN`; the D-144 discipline; exact-decimal rendering — **SETTLED (1.3.0 batch, user-ratified)**

`tfp32/64/128/256` = Q16.16/Q32.32/Q64.64/Q128.128 (D-036) lowering to
native `i32/i64/i128/i256` — deviating from TYPE_REFERENCE §5's word-struct
tables for the wide pair (byte-identical in memory; direct arithmetic).
ERR = most-negative raw, sticky, branch-free per D-144. mul widens ×2 and
range-checks back; div: zero divisor → ERR (D-007's twisted rule), else
widen, shift by F, divide, range-check. Comparisons ERR-aware per D-008;
`is_err` admits the widths. `.floor()`/`.trunc()` are METHODS (§5's
`tfp256_*` free functions struck). Literals convert to nearest-Q by exact
compile-time integer arithmetic; out-of-range refuses (TYPE-031
discipline). `ToString` renders the EXACT finite decimal expansion (a Q
value is a binary fraction; determinism is the family's point) — trailing
zeros trimmed, mandatory fraction digit, "ERR" for the sentinel.

## D-196 — `dim256<Unit>`: units are exponent vectors; `unit:` declarations join the grammar — **SETTLED (1.3.0 batch, user-ratified)**

A unit is an exponent vector over the seven SI base dimensions; the named
units are names for vectors (`Joules` = mass·length²·time⁻²). Multiply and
divide ADD/SUBTRACT vectors — the algebra is TOTAL, dissolving §5a's "if
registered": `force * dist` IS the Joules vector whether or not anything
names it. Add/sub/compare demand equal vectors, refusing with both units
shown. Everything erases before lowering (`dim256` IS `tfp256` at IR,
D-036; every D-195 rule applies). Casts: `=> tfp256` drops the unit;
`tfp256 =>! dim256<U>` asserts one (the silent unit-gain "warning" in §5a
becomes a refusal without `=>!`). **The user ratified the grammar
addition**: `unit:Hertz = 1 / Seconds;` declares a named unit — the
semantic-intent lever, landing IN 1.3.3 with the rest.

### D-196 implementation record (1.3.3)

Design points fixed inside the ratified frame, at open:

1. **The packed representation**: seven signed `i8` exponents in one
   interned `TY_DIM` row (`a` = 256 so `type_int_bits` answers uniformly;
   `b`/`c` = the vector) — unit equality IS type-id equality, and the
   existing same-type discipline does the enforcement. Exponents refuse
   past ±127.
2. **The ZERO vector IS `tfp256`** — `tt_dim` returns the tfp256 id for
   it, never interning a dimensionless dim. `dist / dist` cancels to the
   bare number, scaling by `tfp256` is the zero vector's algebra with no
   special case, `unit:Radians = 1;` makes `dim256<Radians>` honestly BE
   `tfp256`, and the BARE forms refuse (`dim256:x`, `1.5dim256`) — the
   dimensionless type already has a name, and one meaning gets one
   spelling.
3. **Annotations take a single unit NAME**; algebra is spelled in `unit:`
   declarations only. Evaluation is on-demand and memoized on the
   TypeTable (declaration order never matters), the check walk evaluates
   every declaration eagerly so an unused broken one still refuses, and a
   64-step depth bound is the cycle refusal (the D-057 bounded-expansion
   shape). Resolve deliberately leaves the RHS alone — the unit
   vocabulary is not the symbol table's, and one implementation owns the
   walk.
4. **The seven SI base units are compiler-known names**; the derived set
   (`Newtons` … `Katals`, the dimensionless `Radians`/`Steradians`) is
   PRELUDE `unit:` declarations — one mechanism, exercised by the prelude
   itself. Two names for one vector (`Hertz`/`Becquerels`) is physics,
   not a conflict; diagnostics render the base-product form
   (`Kilograms*Meters^2/Seconds^2`), never a guessed name.
5. **No `ToString` on a dimensioned value, BY DESIGN**: rendering drops
   the unit and drops are explicit — the spelling is `&{x => tfp256}`.
6. **The `pick` ERR-arm demand extends to `tfp` AND `dim256`** — found as
   a 1.3.2 gap: `pick` on a `tfp` selector lowered fine with no ERR-arm
   requirement, so `(*)` swallowed the taint, the exact D-008 hole the
   `tbb` arm closes.
7. **The compound-assignment miscompile** (found by `dim`'s first
   compound test): `x op= v` had its OWN raw `add`/`mul`/`sdiv` tail in
   the emitter since 0.9.5 — a twisted `+=` wrapped instead of
   saturating, a twisted `/=` by zero reached the hardware instead of
   yielding ERR, a plain `/=` skipped the D-007/D-142 division guards,
   and a float `+=` emitted an integer `add` on a double. All latent:
   nothing had ever exercised a non-plain-int compound assign. The fix is
   the house shape — ONE implementation: the arithmetic value core is
   extracted (`emit_arith_value`) and both spellings route through it.
   `dim256` additionally holds `op=` to the SLOT's unit (`d *= d` is
   `Meters^2` into a `Meters` slot — refused; the checker's compound arm
   runs the algebra with the real operator).

## D-197 — the ternary/nonary bases; Kleene logic rides `&` and `|` — **SETTLED (1.3.0 batch, user-ratified)**

`trit`/`nit` = `i8`, `tryte` (10 trits)/`nyte` (5 nits) = `i16` on the
binary rung — 3^10 = 9^5 = 59049 states, why both fit; the representation
is the RUNG'S, never the identity (ternary hardware is a target). ERR = a
binary-spare state (−128 / −32768), sticky, D-144 style; on ternary
hardware ERR is that target's own lowering choice. Ops: `+ - *` all four,
`/ %` at tryte/nyte only, comparisons in balanced order, negation `0 - x`,
overflow → ERR. Digit access `.trit(i)`/`.nit(i)` bounds-checked; `.len`.
**The user ratified the operator spelling for the Kleene logic**: on
`trit`/`nit`, `&` is three-valued AND (min), `|` is OR (max) — True=1,
Unknown=0, False=−1, ERR sticky — and NOT is `0 - x`, which in balanced
ternary IS logical negation. Single gates on a ternary target; no new
spellings.

### D-197 implementation record (1.3.4)

Design points fixed inside the ratified frame, at open:

1. **The binary rung stores the VALUE, not packed digits.** One `TY_TERN`
   kind (a = carrier bits, b = base, c = digit count; the balanced bound
   derives as (base^digits−1)/2). The PROTOTYPE packs trits into
   LUT-driven split bytes with ripple-carry loops — the
   emulation-as-identity §7 warns against, plus runtime LUT state a
   verifier would carry. The value representation makes balanced order
   NUMERIC order, arithmetic native with one bound check, and the ERR
   sentinels (−128/−32768) COINCIDE with the tbb carrier minimums —
   `tbb_min_decimal` serves a third family and `tbb_divlike` runs
   verbatim (a quotient's magnitude never exceeds its dividend's).
2. **The prototype's `trit_add` CLAMP is overruled.** The oracle
   saturates a trit sum to ±1 while its own multiply errs; D-197's text
   ("sticky, D-144 discipline; overflow → ERR") is uniform and outranks
   it — `1 + 1` at `trit` is ERR, exactly as `MAX + 1` is at `tbb`.
   `+ - *` run widened in i32 (two bounds' sum and 29524² both fit) with
   one balanced-bound check narrowing back.
3. **Literals are contextual, any base.** D-147 folds balanced digits to
   the value at scan and drops the base — a base is a SPELLING — so a
   ternary slot takes any unsuffixed integer literal (decimal, balanced,
   hex alike; one value, one check), range-checked EXACTLY against the
   balanced bound in a dedicated arm: a power-of-two `ValueRange` cannot
   spell ±29524, so the family's `type_range` rows overclaim to the
   enclosing power of two — the safe direction, consulted only by the
   leaving-cast classification it can only tighten.
4. **`.trit`/`.nit` join the after-dot keyword interning** (the
   D-056/`acquire` mechanism): the type keywords have no intern, and the
   digit extraction is their one legal home after a `.`. Digits compute
   by the OFFSET TRICK — `(v + bound)` is unsigned 0..base^digits−1;
   digit i = that / base^i mod base − (base−1)/2 — with base^i a
   branch-free select over the ≤10 constants; the index takes the D-070
   bounds guard, and an ERR receiver yields an ERR digit.
5. **Casts**: one family within itself (value-preserving, sentinel maps;
   a smaller-bound target takes the tbb⇄tbb runtime discipline;
   tryte⇄nyte is a pure relabel — same 59049 values); leaving traps ERR
   under BOTH spellings with the value range-classified (D-144 as
   amended); entering is check-or-saturate against the balanced bound —
   the sentinel-forge case lies inside the range guard, so it needs no
   separate check. Cross-twisted-family is IMPOSSIBLE (the D-195
   amendment-4 precedent) — through the plain integer.
6. **`ToString` renders the VALUE** ("ERR" or the number, the tbb shape);
   the balanced digit string is what the digit methods are for.
7. **Found at implementation: the same-text early-out defeated the tbb
   cast matrix at SAME-WIDTH crossings.** `tbb32⇄int32` share the
   carrier text `i32`, and `emit_cast`'s early-out sat ABOVE the tbb
   intercept — so the same-width crossings rode through with NO sentinel
   check: the audit's Theme-A hole D-144's own text says 0.9.5 closed.
   Its tests passed by COINCIDENCE — the only int32 value outside
   tbb32's range is INT_MIN, whose bit pattern IS the sentinel, so the
   unchecked forge looked exactly like the saturation it should have
   been, and the leaving direction quietly read ERR out as a number (the
   laundering 1.3.2b closed at every OTHER width). The tbb intercept now
   precedes the early-out, as tfp's has since 1.3.2 and tern's does from
   birth; `tbb.npk` pins the same-width crossings.

## D-198 — `frac*`: invariant-normalized mixed numbers, exact-or-ERR — **SETTLED (1.3.0 batch, user-ratified)**

`{whole: iN, num: iN, denom: uN}`; after EVERY operation the prototype's
five invariants hold (denom > 0; num ≥ 0 when whole ≠ 0; num < denom;
gcd = 1; sign on whole, or on num when whole = 0) — normalization is
automatic, and §20's function family becomes OPERATORS `+ - * /` with
comparisons. ERR: denom 0, any component at its width's most-negative, or
overflow during normalization — sticky. The family's promise is "exact or
ERR", never "rounded". `int => frac` lossless; `frac =>! flt64` rounds so
it is the acknowledged form (correcting §20); widths widen `=>` / narrow
`=>!`. `ToString`: "whole num/denom".

### D-198 implementation record (1.3.5)

Design points fixed inside the ratified frame, at open:

1. **The algorithms live in the PRELUDE, in Nitpick, in `int256`** — one
   generic core (`npk_gcd256`, `npk_frac_norm`, add/sub/mul/div, `cmp`)
   with the width's balanced bounds as parameters and an `ok` flag for
   "does the reduced form fit". The emitter unpacks a frac's components,
   widens, calls the deterministic prelude symbol (every prelude function
   emits into every program), and narrows back or lands on the canonical
   ERR triple. Grounds: hand-written runtime IR with loops is
   verification-hostile, inline per-site gcd expansion is unreviewable,
   and the D-193 precedent put exactly this kind of careful arithmetic in
   the prelude. `int256` because frac64's cross-products reach 2^127 and
   their SUM overflows i128; the mul/div paths go through the IMPROPER
   form (w·d + n), whose product tops out near 2^254. The core is
   TAINT-BLIND: the emitter tests the operands' ERR and discards the
   result under taint — stickiness is the caller's, exactness the core's.
2. **D-198's layout stands over the prototype's**: denom is UNSIGNED
   (the oracle used all-signed components with positivity as an
   invariant). Canonical ERR is `{minN, minN, 0}`; `is_err` answers the
   ratified disjunction so partially-forged states read as ERR.
3. **Read-only members `.whole`/`.num`/`.denom`** — unlike tfp's hidden
   raw bits, a frac's components ARE the spec's value model and its
   ToString renders them; as values-not-places the invariants cannot be
   broken through them, and the prelude's own ToString reads them.
4. **No literals** (`int => frac` is the entry; a bare integer is not a
   mixed number's spelling — the refusal names the cast) and **no `pick`
   selectors** (ERR is the only nameable case; the scalar pick machinery
   compares one register where a frac is three). TYPE-052 carries the
   family's own rules, `%` included — the ratified operator list is
   exact.
5. **Casts**: widths widen `=>` / narrow `=>!`-absorbing-as-ERR;
   `frac =>! flt64` rounds (flt32 is not a ratified exit); `frac =>! int`
   truncates toward zero (whole plus one when whole is negative and num
   nonzero — the mixed form's edge); ERR traps both spellings on any exit
   (D-144 as amended); a FLOAT NEVER ENTERS — a float is not exact, and
   the exact family will not launder one; cross-twisted-family is
   impossible (the D-195 amendment-4 rule).
6. **Found at implementation: D-169's non-scalar comparison belt.** The
   emitter's aggregate-compare fail-closed belt (right for everything the
   checker refuses) sat ahead of the frac compare — frac is the FIRST
   aggregate the checker ADMITS to comparison, so the belt called a legal
   program a defect. The frac arm now precedes the belt. Also two
   reserved-word collisions in new emitter code (`tid`, `fd` as locals —
   the CLAUDE.md table's own rows) that only npkc's parse catches, the
   seed's checker never building backend files.

## D-199 — `complex<T>` over flt and tfp elements; Smith's division — **SETTLED (1.3.0 batch, user-ratified)**

`T` ∈ {`flt32`, `flt64`, `tfp32`, `tfp64`} (§21's `fix*` names obsolete
per D-036); layout `{T, T}`; constructor `complex(re, im)` type-directed.
Operators `+ - * /` — float division by Smith's algorithm (the naive
formula silently overflows at |denom| ≈ √max), tfp by the direct formulas
under D-195's ERR discipline (any component ERR → both ERR). Methods
`.re() .im() .conj() .abs2()` on every element; `.abs()` float-only
(`llvm.sqrt` is an instruction; a fixed-point square root has no consumer
— refused by the same rule as shuffles). `ToString`: "3+4i" via the
element's rendering.

### D-199 implementation record (1.3.6)

Design points fixed inside the ratified frame, at open:

1. **The arithmetic lives in the PRELUDE, per element type** (the 1.3.5
   architecture, paying twice here): the tfp bodies use the LANGUAGE'S OWN
   tfp operators — Q scaling, saturation and stickiness arrive for free —
   with the any-component-ERR → BOTH canonicalization an explicit `is_err`
   line a verifier reads; the float bodies write Smith's division as
   ordinary branches over IEEE-total operations, and flt32 computes IN
   flt32 (widening through double would double-round). The emitter unpacks
   `{T, T}`, calls the deterministic per-element prelude symbol, and the
   returned pair IS the value.
2. **The constructor reuses the ctor node** — `ExprVectorCtorExpr` already
   records which keyword built it, so `complex(re, im)` is a parse-arm
   extension and two consumer branches, not a new AST kind.
3. **No order** (mathematics, not policy — the refusal rides the shared
   not-ordered code with the mathematics in the message, and suggests
   `.abs2()` for magnitude); **equality only**, per-component: IEEE on
   float elements (a NaN component makes `==` false and `!=` true), and
   tfp elements trap a tainted operand first (D-008 §5). The equality
   emit precedes D-169's non-scalar belt — the 1.3.5 lesson applied at
   the category's second member, this time at design.
4. **`is_err`/`ERR` follow the element family**: the pair disjunction on
   tfp elements; a refusal by name on float elements (a float carries
   NaN, not ERR). No `pick` selectors (the frac posture). TYPE-053
   carries the family's rules.
5. **No casts in either direction** — construction is `complex(re, im)`,
   reading is `.re()`/`.im()`, element conversions happen at the
   components. `.abs()` is float-only (`llvm.sqrt` is an instruction
   there; a fixed-point square root has no consumer — the shuffle
   refusal's rule); `.abs2()` is the total magnitude everywhere.
6. **`ToString` is four concrete-instance prelude impls**
   (`impl:complex<flt64>:ToString` — the instance-impl machinery landed
   at 1.0 and the overlap suite already exercised it): sign-aware
   "3+4i" / "3-4i" via the element's own rendering; a tfp ERR pair
   renders "ERR".

## D-200 — the library tier: nvec, ntensor; rank-9 inline dims; int64 dimensions — **SETTLED (1.3.0 batch, user-ratified)**

`lib/nvec.npk`: `vec2/3/4` as structs over `simd<flt64, N>` with
`.x/.y/.z/.w`, dot/cross/length; `vec9` a struct of nine `flt64` with §25's
`mRC` fields (matrix semantics, not a 9-lane vector). `lib/ntensor.npk`:
`matrix<T>` `{ptr, rows: int64, cols: int64}`; `tensor<T>` with RANK
CAPPED AT 9 and dims INLINE `{ptr, ndims: int64, dims: [9 x int64]}` — one
allocation, and rank 9 is Nikola's manifold by construction (the
prototype's ttensor carries dims[9]). `tmatrix`/`ttensor` are the same
containers over `tryte`. All four OWN their heap cell — the 1.2 managed
regime drops them. Dimensions are int64, not the prototype's int32
(Nikola-scale tensors against a 2^31 ceiling is a foreseeable regret).

### D-200 implementation record (1.3.7)

Landed as ratified — `lib/nvec.npk` and `lib/ntensor.npk`, ordinary
Nitpick — plus the two mechanisms the decision presumed and the language
did not yet have:

1. **`buffer` landed minimally as the managed owning byte cell**
   (TYPE_REFERENCE §23 rewritten to match): `TY_BUFFER`, re-filed from the
   generic stand-in to a simple type; `buffer_new(int64) → buffer`
   (**never fails** — n zeroed bytes, `len == cap == n`; `n <= 0` is the
   EMPTY non-owning buffer, an answer not an error; OOM traps per D-150);
   members `.ptr/.len/.cap` — the string's trio with the string's
   meanings, `cap == 0` the same ownership bit, the DROP BODY shared with
   the string's; move-only (TYPE-046) and `==`-refused (D-169) like the
   string; rides a channel whole under the send's `move`. §23's earlier
   draft rows did NOT land, by decision at the subcycle open: the
   per-width read/write verb family (a second copy of `#ptr_add` + `<-`),
   `buffer_free` (the managed drop IS the free), `resize`, and
   `buffer_bytes` (no consumer yet — add by decision when one exists).
2. **`#sqrt` joined the `#`-builtins** (flt32/flt64 → `llvm.sqrt.*`, the
   machine instruction): `length` is ratified, and a prelude Newton
   iteration would compute a subtly DIFFERENT number than the instruction
   — the drift this language refuses. Everything else refuses by name
   (`tfp` recorded as refused for want of a consumer).
3. **The containers**: `matrix<T>` `{buffer, rows, cols}` with
   `mat_of::<T>`, `tensor<T>` `{buffer, ndims, int64[9] dims}` with
   `tensor_of::<T>(int64[] dims)` — one allocation, rank ≤ 9 enforced at
   creation, row-major offsets on access, EVERY index bounds-checked in
   the library (`fail BadIndex`/`BadShape`, ntensor's own declared
   errors; the buffer body is in-bounds by construction). `tmatrix`/
   `ttensor` shipped as documented instances (`matrix<tryte>`), with the
   ternary ERR semantics proven THROUGH the container in the tests.
4. **Found by the first exercise** (the pattern every subcycle repeats):
   a checker-typed never-fails builtin returning through the floor's
   Result envelope was handed out under its bare recorded type, so the
   binding stored 32 bytes into a 24-byte slot — the generic rt path now
   extracts the value half at the call site when the recorded type is
   non-Result (`buffer_new` was the first such builtin); and the generic
   struct literal's spelling inside a generic factory is the BARE name
   (`matrix{ … }` — instantiation from context, the prelude's own
   `TextWriter{}` idiom), where `matrix<T>{ … }` does not parse.

### D-195 amendments (1.3.2, recorded at implementation)

1. **Comparison on ERR TRAPS — the parenthetical corrected.** The proposal
   said "ERR == x false, ERR != x true, orderings false", copied from
   TYPE_REFERENCE §5's carried-over row. D-008 §5 settled the opposite,
   with its rationale explicit: a comparison is a decision point, and the
   NaN-style alternative was REJECTED for breaking trichotomy and
   obstructing Z3. One taint discipline, both twisted families, one code
   (−4100, `TbbErr`). §5's row is corrected.
2. **Bitwise and shifts on the raw representation are REFUSED**, striking
   §5's "on raw bits — useful for scaling" rows: `ERR << 1` is zero — a
   one-instruction ERR laundry, precisely the hole D-008's stickiness
   exists to close. `tbb` never admitted them; `tfp` does not either.
   Scaling is multiplication by a power-of-two constant, which is
   ERR-correct.
3. **A cast OUT of the family traps on ERR under BOTH spellings.** What
   `=>!` acknowledges is the value's precision loss; ERR is not a value
   (D-008 §1 excludes it from the range), and the sentinel shifted down by
   F is a plausible mid-range number — the worst laundering. NOTE the
   asymmetry this created with tbb, whose `=>!` read the carrier raw
   (landed 0.9.5): the tbb carrier at least surfaced as the recognizable
   INT_MIN. Flagged for the user as a safety observation rather than
   changed under this decision — **and settled by the user at the 1.3.2
   close: tbb aligns to the trap rule. See the D-144 amendment.**
4. **`tbb ↔ tfp` casts are CAST_IMPOSSIBLE** — same carrier, different
   VALUES (the 2^F scale); a reinterpretation would be a forgery.
5. **`is_err`, the `ERR` sentinel-by-context, unary `-` (total under
   balance) and full ORDERING extend to `tfp`** — unlike `tbb`, whose
   values are codes with equality only, a `tfp` is a number.
6. **REACH refined**: a twisted division cannot reach DivByZero/DivOverflow
   (ERR instead), so the failsafe set no longer demands those arms for it —
   and `TbbErr` is demanded wherever either twisted family is touched.
7. **Literal folding runs in subset-1 arithmetic** (u32 limbs + the
   multiply-decimal-by-two bit method, round-half-even), because the seed
   still builds src/ and wide integers are outside its subset (C-13); the
   emitter's `iN` constant and the checker's range answer come from the one
   conversion. The prelude's `ToString` (exact expansion, `uint256` core)
   is npkc-compiled and uses the wide tier freely.

## D-201 — the builtin surface typed from one generated signature table — **SETTLED (1.4.0 batch, user-ratified)**

Closes P-3 (D-192's residue). Full rationale and survey: `meta/roadmap/1.4/1.4.0.md`.

1. **BUILTIN_REFERENCE.md's marked regions are the single signature
   authority.** The Signature column is normalized to one parseable syntax
   (§3 gains the column it lacks; `read`/`write`'s bare `ptr` becomes
   `wild int8->`), and the generator — which already scrapes names and
   never-fails and hard-fails on a missing Fails column — parses it
   strictly, hard-failing on any row it cannot read.
2. **The generator emits the signature table as committed source**
   (`builtins.npk` precedent): `builtin_sig_*` accessors beside
   `is_builtin_name`/`builtin_never_fails`, all derived from the same rows
   so the lists cannot disagree.
3. **Regular builtin calls type through the shared argument path** (arity,
   per-argument fits, `move` where the signature says so, spread refusal),
   the table's type texts interned at check time. The nine irregulars keep
   bespoke arms and are marked `special` in the table — `sys` (variadic),
   `atomic_from_ptr` (turbofish), and the seven annotation-directed
   constructors — so a builtin with neither signature nor special arm
   fails generation. Typing and lowering stay separate concerns: an
   inline-lowered builtin (`suspend_io`, `io_watch`, …) is table-typed.
4. **The typing rule: a `never fails` builtin types as the BARE value; a
   may-fail builtin as `Result<T>`.** This generalizes the settled 13-arm
   convention (D-185's `own_fd`, D-200's `buffer_new`). The floor's ABI is
   untouched — symbols answer in the envelope and the emitter extracts the
   value half at the site, 1.3.7's `buffer_new` mechanism made general.
   Ratified over the everything-wraps alternative: the checker refuses bad
   arguments equally under both, and blanketing ~1,700 provably-cannot-fail
   sites in `raw` would dilute the acknowledgement `raw` exists to be —
   the uniform rule worth having is "`raw` appears exactly where an
   envelope is removed on the strength of a contract".
5. **The emitter's parallel authority retires**: the UNKNOWN-operand
   fallbacks (`result_ll_value_half` and its `?|`/`?!` consumers, the
   typer's `t == 0` bails, `emit_raw`'s wrapped/inner fallback,
   `call_param_want`'s 0-means-skip) and the signedness-blind coercion
   loop. Remaining language-type→symbol-type adaptations (`memset`'s
   `int64` value vs `i32` symbol) become explicit per-row ABI notes in the
   same table, signedness-correct. `rt_sig` keeps only name→symbol+ABI and
   is GENERATED from the same rows — `check_runtime_sigs_agree` then diffs
   npkrt.ll against the reference-derived table, putting the spec in the
   loop for the first time.
6. **A bare name not in the table refuses at type time** (its own code).
   Nothing is left legitimately UNKNOWN: `type_call`'s `pass 0i32`
   fall-through becomes an internal defect, and the unknown-tolerance arms
   in REACH/locks/statement rules retire.
7. **Migration (1.4.2), three steps, fixpoint green at each**: (a) table
   and typing land with a transitional rule — `raw`/`drop` on a now-bare
   never-fails builtin call is the identity; (b) the tree-wide re-spell,
   with the seed's `check.BUILTINS` wrapped-flags flipped and its emitter
   taught the extract-at-site in the same commit; (c) the transitional
   rule removed — `raw` on a non-Result operand refuses, as everywhere.

> **LANDED at 1.4.2** (2026-08-29), in four commits over the three steps
> plus a preparation step. Two departures from the text above, both forced
> by the tree and both recorded in `meta/roadmap/1.4/1.4.2.md`:
>
> - §1's `read`/`write` pointer is **`wild any->`**, not `wild int8->`. The
>   prelude passes a `uint8->` (a slice's `.ptr` is a pointer to its
>   element) and `nbridge` an `int8->`, so a signature naming either refuses
>   the other. It is the reference's own spelling for the same idea —
>   `ralloc` and `dalloc` have carried it since the allocator was written —
>   and `mcpy`/`mmov`/`memset`/`string_from_bytes`/`driver_clone_exec` and
>   the `wildx` trio were given it for the same reason: the floor takes an
>   ADDRESS and a byte count. The acceptance is a named arm of the builtin
>   argument check, not a loosening of `fits`.
> - §7's transitional rule had to cover **`relay`** as well as `raw`/`drop`
>   — 25 `relay string_concat(…)` sites in the prelude, `lib/` and the test
>   programs go through the same `type_unwrap`.
>
> The seed's wrapped-flag flip was three names, not the ~25 estimated: the
> rest were already bare. Four bespoke arms beyond the nine irregulars
> (`string_bytes`, `buffer_new`, `own_fd`, `release_fd`) retired into the
> table rather than being kept, since each has an ordinary signature and
> keeping them would leave the parallel authority this decision collapses.

## D-202 — the fixpoint criterion restated — **SETTLED (1.4.0 batch, user-ratified)**

Closes C-10. The harness has measured the right thing since 0.8.1 — the
seed-built compiler's emission of `src/main.npk` against stage 1's
re-emission, two `.ll` texts byte-compared — while the docs carried
D-079's unsatisfiable sentence ("stage 1 and stage 2 must be
byte-identical": two independent emitters can never satisfy it as
written). The normative criterion, now in BUILD_REFERENCE §6:

> Self-hosting is the fixpoint of the compiler's emission of itself: the
> first stage built from the current source, and the next stage built by
> it, must emit the compiler byte-identically. Binaries are identical
> from that emission onward. Before the 1.4.6 switch the first such stage
> is the seed-built compiler; after it, the snapshot-built one. When the
> builder snapshot is older than the source, the comparison is stage-N vs
> stage-N+1 where stage N is the first current-source compiler — §6.2's
> three-pass shape.

Spec-only; no code changes. D-085/D-079 carry dated annotations pointing
here. (OPEN_DECISIONS' citation "D-085:5747" was off — the operative
lines were 5798/5825/5883 and D-079's 5443.)

> **DECLARED at 1.4.9 (2026-09-02).** The `selfhost` stage measured the
> criterion on every full run of the cycle; at the close it held on the
> final tree in both spellings: stage2 == stage3 at 15,631,627 bytes (sha256
> `9ce0ec8d3de5b2c83da4a1f11d3f89965728f6cf938f70042ea053eff5defaaf`) from `80784f3` by the README's relative refresh — installed as
> the snapshot, so the committed builder IS the fixpoint text — and stage 1
> rebuilt itself byte-identically under the harness, `repro` and `parity`
> green beside it. The record is `meta/roadmap/done/1.4/1.4.9.md`.

## D-203 — the committed bootstrap IR, the `bootstrap/` survival map, and the floor's permanent form — **SETTLED (1.4.0 batch, user-ratified)**

Closes C-11, and settles D-015's open "later" row. Three parts:

1. **`bootstrap/seed/stage1.ll`** — the name LAYOUT and `npkseed.py` have
   always used — is committed at the 1.4.6 switch, holding the FIXPOINT
   emission of the real compiler, beside `bootstrap/seed/STAMP` (source
   commit, toolchain version, sha256 of the `.ll`). Rebuild-from-LLVM-alone
   is `llc stage1.ll` + `llc npkrt.ll` + `ld.lld -static` → npkc, which
   rebuilds itself from `src/` and must close the fixpoint against the
   committed text. **The snapshot is pinned, not tracking**: it refreshes
   at cycle closes with the push, never per-commit; the from-scratch audit
   path for any historical state is the generator plus the pre-switch
   commits (the trusting-trust answer D-085 already records).
2. **The survival map** (LAYOUT §5 amended): `seed/` and `generator/`
   survive indefinitely (the generator regenerates tables and the
   historical seed — needed to regenerate, never to build); `harness/`
   survives until `npkg` parity is proven, retiring under `meta/SWITCH.md`;
   the blanket "all of it is deleted once self-hosting closes" is struck.
3. **The runtime floor's permanent form is reviewed, hand-written LLVM
   IR** — D-015's "later" row is settled as its own "tuned IR" option, and
   a Nitpick rewrite is decided OUT. `npkrt.ll` re-homes to top-level
   `runtime/` at 1.4.6 (it is in every artifact ever linked, including the
   one that ships — it was never bootstrap material, and its THROWAWAY
   header was false). The user's rationale, theirs verbatim: the core
   being LLVM "in the end" was always the assumption — the project's goal
   was removing the C/C++ translation layer ("a pain … for verification"),
   not LLVM itself, since "the compiler pretty much outputs LLVM so
   getting rid of that isn't really a thing in reality"; and maintaining
   multiple versions of the same thing is exactly what they stopped a
   previous build attempt over. If C-19's answer (Astrée's input format)
   forces a C rendering of the floor, that reopens in 1.5 with the real
   constraint on the table — recorded so the interaction is not lost.

> **LANDED at 1.4.6 (2026-08-29).** The first snapshot was taken from
> `2347a8e`: 15,292,234 bytes, sha256 `b4df9fe3081065f7ebe3d35840c621d65fc3b46f80f7f8aee548b86ff4ed94b6`.
> It was verified BEFORE any build path changed — that file alone, through
> `llc` and `ld.lld` with no Python anywhere, produced a compiler that
> re-emitted the whole compiler byte-identically. `runtime/npkrt.ll` re-homed
> with its header rewritten, and `bootstrap/seed/README.md` carries the refresh
> ritual as commands, since the whole point is that no script is required.
>
> **One phrase in part 1 needed reading against its own next sentence.** "npkc,
> which rebuilds itself from `src/` and must close the fixpoint against the
> committed text" is true AT A REFRESH and cannot be true between refreshes —
> the same paragraph says the snapshot is "pinned, not tracking … never
> per-commit". A harness check demanding byte-equality with the committed text
> on every run would make a refresh mandatory on every commit that changes any
> IR, which is the opposite of pinned. So the continuous checks are the two
> that hold between refreshes: **the snapshot still BUILDS `src/`** (`run()`'s
> first act, before any suite, reported as D-205's rule being broken when it
> fails) and **the snapshot matches its STAMP** (sha256 and byte count, so a
> file edited without restamping fails).
>
> Byte-equality is the refresh ritual's step 3, where it belongs and where
> README.md marks it as not optional: a snapshot that compiles the compiler but
> whose output does not rebuild itself works exactly once.

## D-204 — byte-reproducibility defined and checked — **SETTLED (1.4.0 batch, user-ratified)**

Closes C-12. D-078's claim becomes three checked facts (mechanics at
1.4.5):

1. **The toolchain is a pinned input**: `nitpick.toml` gains
   `[toolchain]` with the LLVM version (20.1.2) and the exact llc/opt/
   ld.lld flag sets; the harness (later `npkg`) verifies
   `llvm-config --version` against it and refuses a mismatch. "Same
   inputs" includes the tools (BUILD_REFERENCE §5).
2. **Seed emission is path-independent on every path**: the harness path
   already is (`module_id="test"`, relative paths, zero absolute paths in
   the 11.2 MB emission); `npkseed.py` stops embedding its argv path as
   the ModuleID. The real compiler already emits no ModuleID.
3. **A `repro` harness stage builds twice from different cwds** and
   byte-compares the emissions; after 1.4.6 it also asserts the committed
   `stage1.ll` matches a fresh fixpoint emission and its STAMP sha256 —
   the snapshot cannot silently rot.

> **LANDED at 1.4.5 (2026-08-29), with two departures from the mechanics
> above, both toward less trust in documents.**
>
> **The flags are pinned AND READ.** Point 1 has `[toolchain]` record the
> flag sets; every `llc`/`opt`/`ld.lld` invocation is now BUILT from those
> lists (fifteen call sites, one authority) rather than restating them
> beside a table that describes them. A stated flag nothing consumes is the
> next stale document, and this project has been bitten by that class often
> enough — §26 of TYPE_REFERENCE promising a `fixed` rule enforced nowhere;
> `check_runtime_sigs_agree`'s derived-inner leg found dead on the day it
> was fixed. A fourth key, `llc-opt-flags`, names the 1.3.8 instrument's
> `-O2` leg separately from the build's, because a check is not a build.
> The version probe asks `llc`, `opt` and `ld.lld` themselves rather than
> `llvm-config`, which ships in a `-dev` package the build does not
> otherwise need: the version that matters is the one that will run.
>
> **Read with D-265 (2026-09-06).** Point 1's pin is a VERSION, asked of the
> tools, and a version is not a binary: two builds of 20.1.2 give the same
> emission and may give a different object and binary. The identity claim
> that holds across machines is the emission's (`build/npkc.ll`); the ladder
> prints every intermediate's digest so a difference names its stage.
>
> **Point 2's diagnosis was wrong, and the fix still landed.** `npkseed.py`
> never embedded its argv path. `Module` is built as
> `S.Module(items, path)`, so the path lands in the node's `path` FIELD
> while `_path` is the `Node` base's LOCATION attribute, never set on a
> module node — so `mods[0]._path` evaluated to the class default `"?"` on
> every invocation. Reproducible **by accident**, and one character from the
> opposite: a reader fixing the apparent typo to `.path` would have leaked
> the argv path into the IR. The constant is now explicit and is the same
> one the harness passes.
>
> **Measured before the stage was written**, seed-built compiler over
> `src/main.npk` (15,292,234 bytes): a second run from the same directory
> and a third from a different one both produced IDENTICAL bytes, so H1
> (ASLR / hash iteration order — the class with no controlling flag) and H9
> (build-path leakage — not hypothetical here, since D-179's site tables
> embed source paths) are both clean today. The stage costs one extra
> emission (~3 min) against a ~28-minute run. The fixpoint cannot
> substitute for it: that compares two DIFFERENT binaries, so
> byte-identical there means they agree, not that either is deterministic.
>
> `selfcheck.py` holds the pin's FAILURE path — a mismatched version, an
> absent pin, and the real toolchain passing — because a check only ever
> seen to pass is a check nobody has tested.
>
> **Bullet 3's second half is CORRECTED at 1.4.6.** "After 1.4.6 it also
> asserts the committed `stage1.ll` matches a fresh fixpoint emission"
> contradicts D-205's cycle-close refresh cadence and D-203's own "pinned, not
> tracking"; what the stage asserts instead is the snapshot's STAMP integrity,
> with "can the snapshot still build `src/`" as the continuous anti-rot check.
> The reasoning is under D-203's landing note.

## D-205 — the builder rule and the switch — **SETTLED (1.4.0 batch, user-ratified)**

Closes C-13 — the rule obeyed by discipline since 0.9, whose switch cycle
was named three different ways (1.2, 1.3, "here") and never landed.
Normative, in SUBSET_1 §4 and BUILD_REFERENCE §6:

> **`src/` may not use any construct its current builder cannot
> compile.** Until 1.4.6 the builder is the regenerated Python seed and
> `src/` is subset 1. At 1.4.6 the builder becomes the committed
> `bootstrap/seed/stage1.ll`, and the constraint becomes: a feature
> `src/` wants to use must already be in the snapshot — new features
> enter `src/` only after a snapshot refresh, and the snapshot refreshes
> at cycle closes.

SUBSET_1 §4's adoption table is corrected to what was measured at 1.4.0:
no adoption happened at 0.9–1.2 (`src/` is still fully subset-1 — zero
generics, traits, or async in the 71 seed-built modules; the prelude is
the escape valve, being data); adoption happens once, at 1.4.7, under
D-209's scope. The seed's retirement is the switch itself: after 1.4.6 it
is never the builder again; `check_ll_types_agree` retires WITH it (its
question — do the two emitters agree — ends when there is one emitter),
and `check_runtime_sigs_agree` drops to the two-way diff D-201 sharpened.

> **LANDED at 1.4.6 (2026-08-29).** Both instruments retired as written. Three
> things went further than this text, and the third is the one that mattered:
>
> - `compile_files` — the seed's compile path — is gone, and so are the five
>   generator imports (`diag`, `lex`, `parse`, `check`, `emit`) at the top of
>   the harness. The seed is not merely unused as a builder; it is not on the
>   import path, which is the version of "retired" that cannot quietly return.
> - The `rungs` STAGE retired. `tests/rejection/` was checked twice — as a
>   `[[test]] negative` target against the seed's rungs, and in a stage against
>   the real compiler's — a split that existed only because the seed was the
>   builder. Post-switch both ask one binary the same question.
> - **The `[[test]]` suites moved to the compiler under test, and this decision
>   does not say to.** The plan attributed it here; the actual reason is
>   D-209's: `tests/frontend/` and `tests/backend/` `use` `src/` modules
>   directly, so the moment `src/` adopts a construct outside subset 1 at
>   1.4.7, the seed cannot compile their imports and 37 tests fail for a reason
>   unrelated to what they test. The move is required by the NEXT subcycle.
>
> D-205's rule is mechanical now rather than a matter of discipline: the
> snapshot builds `src/` as the harness's first act, so a construct the builder
> cannot compile fails before a single test runs, naming the file.

## D-206 — `npkg`: build/test, the spawn primitive, the closed-world link — **SETTLED (1.4.0 batch, user-ratified)**

Closes B-4. Scope for 1.4 (subcycle 1.4.8): **`npkg build` and
`npkg test`, minimal and real**; `npkg update` stubs to a named refusal
(nothing to resolve in a single-repo world); `npkg verify` refuses naming
1.5 until the z3 pipeline exists.

1. **`npk_spawn`**, a new floor entry generalizing `driver_clone_exec`'s
   proven shape — fork-shape `CLONE_PIDFD` clone, allocation-free child
   (the copied-futex rule), PDEATHSIG + NO_NEW_PRIVS, the same 16-slot
   registry so the trap route kills outstanding tool children and a clean
   exit with a live child refuses (D-151/D-188 extended to build tools) —
   with caller-directed stdio in the block (stdout to a pipe or file fd,
   not hardcoded `/dev/null`; the fd-3 control channel optional). The
   Bridge's `spawn_driver` becomes a caller of the same primitive.
   Waiting stays `sys(WAITID)` from Nitpick (the nbridge pattern). The
   builtin enters BUILTIN_REFERENCE and D-201's table from birth; the
   block layout is settled at 1.4.8 with the code in front of us.
2. **Directory listing needs no floor addition**: `sys(SYS_GETDENTS64)`
   over an owned buffer, dirent records parsed in Nitpick —
   `lib/nfs.npk`, library-owned like ntensor's bounds.
3. **`npkg` is full Nitpick against the compiler's own modules** — it
   `use`s the real frontend for its source-scanning instruments rather
   than reimplementing the Python regexes.
4. **The closed-world link is law, not a flag**: only npkc-produced
   objects plus the audited runtime allowlist may appear in a link line,
   with no relaxing option — D-011's scan written into BUILD_REFERENCE §4
   as a permanent pipeline step, making "in-process FFI does not exist"
   structural for every user program.
5. **Succession, not replacement**: parity is proven by running BOTH
   runners and comparing verdicts; the Python harness retires under
   `meta/SWITCH.md`, not in 1.4 — never a gap where neither runs. The
   §7.1 self-check obligation transfers to `npkg` explicitly, and 1.4.1
   first wires the existing `selfcheck.py` into the Python run so the
   property is continuously held on both sides.

> **Landed, item 1 (1.4.8 Part A, 2026-09-02).** The floor entry is spelled
> `clone_exec` (symbol `@npk_clone_exec`), not `npk_spawn`: `spawn` is the
> task keyword (D-062), so no bare builtin can carry that name, and the old
> `driver_clone_exec` already said what the primitive does — the
> generalization drops the tenant from the name. The block is ten words as
> planned, and the descriptor rule ("every child-bound fd ≥ 4") is CHECKED by
> the runtime (`-EINVAL` before any slot is claimed) rather than trusted to
> the caller. One addition the item did not foresee: `environ() → cstring[]`,
> a floor builtin measured at `_start` beside `argv` by the one cstring-array
> builder — no syscall returns the environment, and `npkg` needs `PATH`;
> D-089's `main` signature is untouched. The shared syscall vocabulary moved
> to `lib/nsys.npk` (REACH is import-scoped; the tool runner could not import
> the Bridge for six constants), the tool runner is `lib/nproc.npk`
> (`proc_spawn`/`proc_wait`/`proc_reap`, both pipes captured, every wait
> bounded), and `spawn_driver` is a caller of the same primitive. The
> registry's names (`driver_retire`, `DriverLeak`) are kept: a tool child is
> supervised identically, and renaming an error identity costs every
> `failsafe` in the tree an arm for cosmetics.

> **Landed, items 3–5 (1.4.8 Part D, 2026-09-02).** `npkg/` — twelve modules
> of Nitpick built by the compiler under test, over the compiler's own path
> code, list and lexer. `npkg build` runs the README's ladder and produces a
> compiler BYTE-IDENTICAL to the harness's; `npkg test` runs the runner
> self-check (§7.1's obligation, `--selfcheck` alone) and every suite the
> harness runs, unit for unit — 908 verdicts on the first full run, every
> suite count the harness's — with `--only`, `--verdicts`, and `update` and
> `verify` refusing by name. Item 4's scan reads the object's ELF64 symbol
> table itself (`npkg/elf.npk`) against the allowlist derived from
> `npkrt.ll`'s defines plus `main`: `llvm-readelf` is a fourth tool outside
> the `[toolchain]` pin, and a law enforced by an unpinned tool's text is
> weaker than the rule; the harness keeps spawning `llvm-readelf`, and the
> parity stage holds the two readers to each other. Item 5's parity is the
> harness's `parity` stage — `npkg` built by the compiler under test, `npkg
> test --verdicts` run from the manifest root, the two verdict lists diffed
> unit for unit, `build/npkc` byte-compared — and every per-file site in the
> harness now records a verdict. Two questions the port raised are S-9
> (BUILD_REFERENCE §7.1's "unexpected diagnostics fail a test" is a rule
> neither runner enforces; measured at 17 of 131 files, nine of them one
> `resolve_check` defect fixed on the spot) and S-10 (the manifest declares
> four of the fourteen suites). `proc_wait` CONSUMES its `Proc` since this
> landing: a borrow there would taint the captured text under D-004's
> conservative rule, and after the wait nothing is left to hold. The
> stage's first result, on the concluding 1.4.8 harness run: 902 verdicts
> agree between the two runners, npkc byte-identical, every stage green.

## D-207 — per-scope joins — **SETTLED (1.4.0 batch, user-ratified)**

Closes C-22. `join_head` becomes per-scope, threaded through the 1.2
scope-exit walk that already owns drop flags: a scope's exit joins the
tasks that scope spawned (relaying the first child error, D-163 rule 4),
then reclaims that scope's channels, then releases its shared arenas — in
that order, joined-before-freed. Lifts: `channel()` inside a loop
(per-iteration reclaim at the body's exit) and **`shared_arena` teardown**
— `TY_SHARED_ARENA` leaves `type_drops`' excuse table and drops for real,
closing the managed-lowering hole this row was excusing. Creating the
channel outside the loop remains the performance advice, no longer a
language rule. **The third tenant is refused permanently**: a `dyn`
channel element's erased content can hide a borrow, and no join ordering
cures type-level invisibility — BORROW-004's spawn-crossing question
cannot be asked of an erased value. A decision that it is not going in,
not a deferral.

> **LANDED at 1.4.4 (2026-08-29).** The mechanism is a MARK, not a second
> list: the join list is already a LIFO stack, so a scope's children are the
> ones pushed since it was entered, and its exit joins until the head is back
> at the value saved at entry. One pointer per scope, `emit_spawn` untouched,
> and the function's own scope falls out as the mark that is null. The mark is
> frame-resident in a coroutine (role 41 on the block's statement index) for
> the reason drop flags are: a scope spans suspensions and an alloca dies
> between resumes.
>
> **The order this settles, against D-183's:** joins, then defers, then drops,
> then that scope's channel reclaims — innermost scope first. D-183's text
> reclaims "after its defers, drops and child joins", and that put a spawned
> child's borrowed `Mutex` BEHIND the mutex's own drop; `type_drops`' comment
> that "by the join discipline nobody can still hold it at the owner's exit"
> was a claim the lowering did not keep. Joins go in front of the defers as
> well as the drops, because a defer body is user code over the same bindings
> a live child can still reach: a scope's exit ends its concurrency before it
> runs any cleanup. `join_order.npk` regresses it — with inner-scope joins
> gated off it fails on the timing, not merely on luck.
>
> `%npk.join` is gone. It was one block every exit branched to, which is what
> forced the join to run last; the return seam now stores the result BEFORE
> the unwind and returns after it, so the arbitration ("the first child error
> stands as the function's own unless the function already failed") reads the
> same slot it always did. D-136 is untouched — the value is still evaluated
> before the defers; only its store moved.
>
> **Two riders, both narrowings that follow this decision's own reasoning:**
> `exit` runs joins and defers and no reclaims (the drain runs generated drop
> bodies, which is the walk D-183's amendment keeps off the controlled-shutdown
> path); and `.destroy()` on either arena kind now clears the binding's drop
> flag, which is the same mechanism `move` uses, replacing plain arenas'
> reliance on `npk_arena_destroy` being idempotent — `npk_sarena_destroy`
> frees the structure itself and cannot be.
>
> `shared_arena<T>` becoming an owner makes it MOVE-ONLY (TYPE-046 keys on
> `type_drops`), which forecloses handing one to a worker by value. Raised as
> a follow-on question rather than decided here; see
> `meta/roadmap/1.4/1.4.4.md`.

## D-208 — loop-carried moved-from states — **SETTLED (1.4.0 batch, user-ratified)**

Closes S-2. The 0.5 move analysis gains loop-header merge states: a
binding moved anywhere in a loop body is moved-from at the body's head
unless reassigned on every path reaching the back edge — the
read-before-assign fixed point extended, not a new walk. Its own code,
rejection case, and the acceptance case proving reassign-then-move-again
loops still pass (0.5's lesson: these analyses fail closed, so
over-refusal is the likelier defect). The first run over `src/` is
expected to find latent instances of the `modmap_members` class; they are
repairs, not regressions. Lands at 1.4.3, before the adoption sweep
enlarges the code the analysis must be right about.

> **LANDED at 1.4.3 (2026-08-29), and the diagnosis above is CORRECTED.** The
> moved-from analysis is not straight-line and has not been since 0.5.3:
> `assign_loop_body` walks a body twice and `state_absorb_may` unions `moved`
> and `freed` between the passes, which IS the loop-header merge this decision
> asks for. Measured before any change, it already refused a `move` in a
> `while`, a `till`, a counted `for` and a nested loop, and already accepted
> reassign-then-move-again and a binding declared inside the loop.
>
> **The real hole was that PARAMETERS were not tracked at all.**
> `binding_slot` answered −1 for any symbol that was not a statement and every
> rule does nothing on −1, so a `move` parameter moved twice, a pointer
> parameter freed twice, and a read after either were accepted **with no loop
> involved**. That also explains this decision's own evidence better than it
> did: `modmap_members` moved `canon`, a `move` PARAMETER, so it type-checked
> because the parameter was invisible, not because the analysis was
> straight-line — the loop was incidental.
>
> The goal is unchanged and was met: no undetected double move. The first run
> over `src/` found 26, including a live DOUBLE FREE in `emit_pick_chain` (one
> string body handed to two owning `IrVal`s on consecutive lines) and the
> container-element move in `fnem_iter_slot` that D-183 records as open.
>
> One narrowing, user-approved rather than assumed: `invalidate_place` now
> stops when a place reaches its root through a POINTER, because a binding
> holding a pointer does not own the pointee and drops already walk past one
> (D-183). Without it the coarse root-invalidation — tolerable only while
> parameters, which are mostly pointers, were invisible — stopped the compiler's
> own correct code from building in four places, with no spelling that could
> clear the state. Details in `meta/roadmap/1.4/1.4.3.md`.

## D-209 — the adoption scope — **SETTLED (1.4.0 batch, user-ratified)**

What `src/` adopts at 1.4.7, as a list rather than an ambition. **In**:
generics where duplication is real (the concrete collection families
become the 1.0 generic containers — the largest mechanical simplification
available, and the strongest pre-verification exercise generics get);
`dyn Writer` diagnostics (D-075's stated design, and what `npkg test`'s
capture-and-compare expects); the unwrap forms and `for`/`till` where
they replace hand-rolled shapes — judicious, file-by-file. **Out, by
decision (not deferral)**: a mass `&{ }` re-spell of the ~1,700 working
diagnostic concatenations (comfort-only churn; `&{ }` is the idiom for
new code); async in the sequential pipeline; macro/`comptime` adoption in
`src/`. Standing rule: any adoption step that changes EMITTED IR (not
just source spelling) is its own commit with the fixpoint and the full
program suite between it and the next.

## D-210 — plain-integer overflow TRAPS — **SETTLED (coverage-audit batch, user-ratified)**

Closes the audit's G-1 (`research/COVERAGE_AUDIT.md`), its biggest
finding: `intN`/`uintN` `+ - *` emitted bare wrapping `add/sub/mul` —
the Therac-255→0 shape as the DEFAULT integer's behavior, while the
checked family (`tbb`, saturate-to-ERR) required opting in.

1. **Default integer `+ - *` trap on overflow**: lowered through the
   `llvm.{s,u}{add,sub,mul}.with.overflow` intrinsics with the overflow
   bit branching to the D-142 trap route — a new code in the D-141 space
   (`INT_OVERFLOW`), REACH-armed like DivByZero. Unary negation rides
   `0 - x` and so traps on `INT_MIN` (the DivOverflow precedent).
   Shifts and bitwise ops are BIT operations and are unchanged.
2. **`tbb` stays the saturating-ERR family; `tfp` unchanged** — the
   by-type split survives, but the DEFAULT is now the safe side.
3. **Deliberate modular arithmetic has NO dedicated spelling** (a
   decision, not an oversight): the idiom is widen-compute-truncate
   (`=>!` at the narrowing, the acknowledged loss) over the native wide
   integers. If a hot-path consumer emerges, an operator-spelling
   question goes to the user then (D-143's consumer-first rule; operator
   design is the user's domain).
4. **1.5 proves the traps away**: overflow obligations join the D-218
   catalogue; a discharged obligation elides its guard per D-219's
   manifest discipline (`llvm.assume`, never `nsw`/`nuw` — D-218.9).
   "Panics are better than corruptions, but verification is best" —
   the r1 §6.2 row, both halves adopted.

Lands at 1.4.2b with a `src/`/`lib/`/`tests/` sweep expecting few or no
deliberate-wrap sites (audit at implementation; the prelude's hash
mixers ride `tbb`/wide arithmetic already).

## D-211 — module bindings are `const`/`fixed` only — **SETTLED (coverage-audit batch, user-ratified)**

Closes G-7, the audit's second code-verified finding: a PLAIN module
binding lowered to a mutable LLVM global (D-165 constrained its
INITIALIZER, never its mutation), and nothing in the crossing rules
governed a spawned task writing one — a spellable data race, and the
Toyota-globals shape (r7 case 8). **A plain (reassignable) module-level
binding now refuses** (its own TYPE code, "module state is `const` or
`fixed`; mutable process state lives in `main`'s scope and flows
explicitly"); `const` and `fixed` module bindings are unchanged. The
implementation's first step audits `src/`'s own usage (the compiler is
the largest program; if npkc needs no mutable global, the language
doesn't either). Lands at 1.4.2b beside D-210.

## D-212 — the deterministic schedule-exploration harness — **SETTLED (coverage-audit batch, user-ratified)**

Closes G-5 on the r6 evidence (95/4/0 exploration/random/stress on the
Raft corpus; defects surviving 7-day stress; PCT's 1/(n·k^(d−1)) floor
vs stress's ~0% for deep interleavings — digest:
`research/digests/r6-digest.md`). Build the minimal harness of the r6
recipe as cycle 1.5's 1.5.7: a mocked-primitive build of the runtime
(npkc owns every primitive the mock layer intercepts — futex park,
eventfd wake, channel CAS, waker state; no third-party boundary), a
centralized PCT-seeded scheduler stepping one synchronization operation
at a time, a virtualized reactor (synthetic EPOLLIN), and seed-replay
of any discovered schedule. Complements — never replaces — `// stress:`
(which demonstrably catches the shallow class). The companion verdict
(r6): formal models target the PRIMITIVES (waker/park-unpark/one
channel), never the whole executor; BPOR-style preemption bounds if a
model spins.

## D-213 — the file-safety riders in `lib/nfs.npk` — **SETTLED (coverage-audit batch, user-ratified)**

Closes G-2/G-3/G-4 as riders on D-206's 1.4.8: **path containment** —
`open_beneath` over `openat2(RESOLVE_BENEATH|RESOLVE_NO_MAGICLINKS)` as
the default opening API where a path is not a compile-time constant,
plus lexical `path_canon`; **restrictive creation defaults** — 0600
files / 0700 directories, widening an explicit argument; **at-family
operations** so check-then-use never spans a path re-resolution
(the TOCTOU file half). Library-owned, `fail`-based, the ntensor
bounds precedent.

## D-214 — the audit's decided-outs — **SETTLED (coverage-audit batch, user-ratified)**

Recorded so the omissions are decisions, not silences (G-8/G-9/G-10):
**hard-coded-credential linting** is not a language mechanism
(noise-prone, application-domain; revisit as an optional `npkg` lint
post-1.5 if wanted). **Constant-time/side-channel discipline** is out
until a crypto surface exists — and is hereby a GATING requirement for
any future crypto library work (the 19.4% side-channel share in crypto
libraries stands as the reason; that work will need language support
decided then, not discovered then). **General taint tracking** beyond
D-007's Result-taint and the Bridge's [UNTRUSTED] boundary is out —
`limit` + contracts (1.5) deliver the checkable version of the same
intent; revisit only on a demonstrated post-1.5 residue.

## D-215 — `dyn` coercion refuses a channel-carrying concrete — **SETTLED (user-ratified; S-4)**

D-207's reasoning completed at the erasure boundary, where the concrete
type is still known: coercing a concrete whose type
`type_contains_channel` answers true for into ANY `dyn` refuses (its
own TYPE code, message citing the gives/reclaim rules an erased
endpoint would evade). `contains_channel(DYN)` stays `false` — with
this refusal, an endpoint-holding `dyn` cannot exist, and the walker's
answer becomes exact rather than a documented residual. Lands at 1.4.4
with D-207; the types.npk DYN comment updates to cite this decision.

> **LANDED at 1.4.4 (2026-08-29)**, ahead of D-207's own items, which it
> does not depend on. `NITPICK-TYPE-056`, and `types.npk`'s DYN arm now
> states that its `false` is exact. One implementation note: the refusal
> is REPORTED by the mismatch reporters (`te_mismatch_at`,
> `te_assign_mismatch`) rather than by `fits`, which is a predicate with
> no span and twenty-one callers — the same specialisation shape those
> reporters already use for a may-fail function in a `never fails` slot.
> `fits` answers `false` and says nothing.

## D-216 — the consuming `pick` — **SETTLED (user-ratified; S-5)**

An owning enum payload was write-only (TYPE-046 rightly refuses the
copy a binding arm would take; no move form existed in patterns). The
form: **`pick (move(v)) { … }`** — the selector consumed, ownership
transferring into the matched arm: bound payloads become OWNED locals
(dropping at the arm's scope exit like any owner); unbound payloads of
the matched variant drop at the match; `v` is moved-from after the
pick on every path (the S-2/D-208 analysis sees it); the enum's own
drop does not run — the pick took it apart. A non-`move` pick over an
owning enum still refuses at the binding arm exactly as today (lending
picks of owning enums remain legal where no arm binds a payload). No
grammar change — `move(v)` already parses as a selector expression;
the work is checker + move-analysis + emitter. Lands at 1.4.3b, beside
the loop-carried move analysis it interacts with.

> **LANDED at 1.4.3b (2026-08-29), and the premise above is CORRECTED.** This
> decision says TYPE-046 "correctly refuses a `pick` arm binding an owning
> payload". **It did not.** The move-only rule fires on reading an owning
> PLACE in a value position, and a pattern binding is not a read of a place —
> so the lending bind was accepted, and what it produced was a live
> USE-AFTER-FREE, not merely a missing feature. An executed probe returned an
> arm's binding out of its function and read `0xAA`, the allocator's free
> poison, through it: the binding is a bitwise copy carrying `cap`, so it is a
> second owner that aliases the enum's body and outlives it whenever it
> escapes. It was never dropped, which is the only reason it was not also a
> double free.
>
> Both halves therefore landed together, as they had to — refusing the bind
> without the consuming form would have left correct code no spelling at all:
> (1) a non-consuming arm binding an owning payload now refuses, naming
> `pick (move(v))` in the message; (2) the consuming form works, and needed
> less than expected — `move(v)` as a selector already cleared the enum's drop
> flag through the ordinary move path, so only the arm side was missing.
>
> Also found, and fixed: **a payload-less variant constructor was classified as
> a PLACE.** `Msg.Quit` builds a value, but `expr_is_place` walks to its root,
> finds the enum's NAME, and sees an identifier. Latent because this rule only
> runs when `type_drops` is true and an owning enum's drop flag is memoised by
> the layout pass — nothing had asked for one by the time such a declaration
> was checked. The consuming pick asks, and it surfaced at once.
>
> A consequence worth recording rather than discovering later: **an owning
> payload cannot be read through a LENDING pick at all.** There is no accessor
> for a variant's payload without binding it, so a getter over a borrowed enum
> must now consume — `enum_payloads.npk`'s `read_text` became
> `move Msg:m`. That is the D-183 open ("a getter over a container of owning
> values has no correct spelling yet") reappearing for enums; the deep-view
> accessor family D-183 records is what answers it.

## D-217 — NIKOS struck from 1.5 — **SETTLED (user-ratified; B-5)**

A decision, not a deferral-by-silence: Astrée IS the
abstract-interpretation evidence for the one-shot 1.6 trial; an
in-house IKOS fork before then duplicates that evidence class while
consuming pre-trial time — the scarcest resource. The manifest's
`[verify.nikos]` table remains and the tooling refuses it BY NAME
(rung style) until a post-1.6 cycle picks it up; the verification
stack's long-term shape (the user's toolchain: Z3, NIKOS, ESBMC,
Frama-C, K, Astrée) is unchanged — only the ORDER moved: nothing
lands between the fixpoint re-close and the trial that the trial does
not need.

> **PREMISE WITHDRAWN by D-233 (2026-09-01).** "Astrée IS the
> abstract-interpretation evidence" no longer holds — Astrée left the plan.
> The strike's EFFECT on 1.5 stands unchanged (nothing lands there that
> the evidence campaign does not need), and the abstract-interpretation
> evidence class re-homes to D-233's leg A in cycle 1.6, whose bring-up
> gate weighs IKOS — the very fork this decision parked as NIKOS — against
> Clam/Crab. `[verify.nikos]` keeps refusing by name until that gate names
> the engine.

## D-218 — the SMT emitter and invocation architecture — **SETTLED (user-ratified early for the 1.5 handoff; C-17)**

The full normative text lives in `meta/roadmap/1.5/README.md` (the
proposed batch, ratified whole); the record here is the numbered
skeleton: (1) Z3 only, exact version SHA-256-pinned, spawned over
SMT-LIB2 text; (2) the determinism profile — `smt.random_seed=0`,
`sat.random_seed=0`, wall-clock timeout DISABLED, `rlimit` the sole
budget, all in the manifest, verdicts machine-independent; (3) one
fresh solver process per function, push/pop only inside; (4) integers
partitioned — unbounded Int + range axioms for arithmetic, QF_BV for
bitwise with explicit crossing casts; `tbb`/`tfp` as scaled Int with
ERR-sentinel rows, never FP theory; (5) floats two-tier — Z3 QF_FP for
tier-1 obligations, Real-interval abstraction for heavy non-linear,
the manifest recording which tier discharged what, undischarged =
retained runtime guard; (6) ownership-trusting memory encoding — no
global heap; slices as (value, integer length) with arithmetic bounds
obligations, **Seq theory decided OUT** (determinism); (7) the
obligation catalogue — overflow (D-210), div/INT_MIN, bounds, cast
range, exhaustiveness, contracts, `limit`, termination AND
stack-depth/recursion (the audit's G-6 row), twisted-ERR exits, the
D-014 failsafe postcondition — every carried obligation or the
manifest has holes; (8) obligation identity = content hash of
canonical SMT text + module-qualified symbol + kind (cross-build
stable); `:named` tags for model/unsat-core mapping back to spans;
(9) elision via `llvm.assume`, **never `nsw`/`nuw`** (poison is a
refinement hazard, r8 Lesson 1); (10) the emitter's `undef` seeds
become `poison` and a harness grep enforces the ban thereafter;
(11) the TCB statement — verified middle-end plus validated floor,
`llc`/`ld.lld` named trusted, the floor's volatile bottom enumerated
in `meta/specs/TCB.md` (r8 Lesson 2).

> **LANDED at 1.5.0 (2026-09-03), the skeleton with one real obligation
> kind — the record is `meta/roadmap/1.5/1.5.0.md`, whose P-1…P-27 are the
> landing's decisions.** The compiler emits obligations (`--obligations`)
> and reads verdicts (`--elide`); `npkg verify` spawns the pinned z3 (the
> workbench's build of tag `z3-4.16.0`, sha256-pinned in `[verify]` with the
> profile as a READ list) one process per function; `nitpick.obligations` at
> the root is the manifest, written only by `--record`. Three readings the
> landing settled: (10)'s seeds are `zeroinitializer`, not `poison` (the
> emitter mints none); (8)'s canonical text is the obligation's relevance
> cone over the hypotheses that DOMINATE the site, hashed with SHA-256 over
> source-derived SSA names; the verdict pass asks `(check-sat)` alone and
> models/cores are `--explain`'s second pass. The D-007 pair (`div-zero`,
> `div-min`) rides end to end: the compiler's own set is 141 obligations in
> 40 functions, 116 discharged, and the verified compiler rebuilds itself
> byte-identically.

## D-219 — elision ownership — **SETTLED (user-ratified early; C-14)**

Elision is a property of the VERIFIED BUILD recorded in the manifest —
never a flag; `--smt-opt` is struck. The artifact Astrée reads is the
verified build with its elision manifest beside it. An undischarged
obligation retains its runtime guard; the binary differs only with the
manifest saying so, and D-218.2 makes the verdicts themselves
machine-independent — D-039's timeout-dependent-binary hazard is
impossible by construction.

## D-220 — `limit<Rules>` placement, typing, subsumption — **SETTLED (user-ratified early; C-15)**

Checks inject at the three write points (initialization, every
assignment, parameter entry — callee-side, caller discharge is an
elision like any other). `limit<R>` rule names RESOLVE (a typo
refuses); `Rules` bodies TYPE (`$` = subject's type, clauses `bool`).
Subsumption is a Z3 implication obligation. The runtime residue traps
through D-142's route with its own code.

> **LANDED — the typing half (1.5.1, 2026-09-03; `meta/roadmap/1.5/1.5.1.md`).**
> A `limit<name>` resolves through the ordinary scope lookup at all three
> sites (a local, a parameter, a `Rules` refinement): a miss is the
> identifier's own RESOLVE-002, a non-`Rules` hit RESOLVE-011, and the
> resolved declaration is written onto the node (`limit_target`). A
> refinement cycle refuses at resolve (RESOLVE-006). A `Rules` body types
> eagerly: `$` is the subject, every clause a `bool`, every refinement over
> the same subject; a limited binding's declared type is the subject by
> IDENTITY — no widening, no wrap, no type parameter (TYPE-059). The check
> at the three write points and subsumption are 1.5.2's.

> **LANDED — the check, the obligations, the bypass (1.5.2, 2026-09-04;
> `meta/roadmap/1.5/1.5.2.md`; D-251, D-252).** One generated predicate
> function per `Rules` declaration; the check AFTER every write over the
> binding's whole value — its initialiser, every assignment to it or to any
> part of it, the callee's entry (sync and coroutine) — trapping
> `LimitViolated` (−4111) through D-142's route; a limited binding has no
> address (TYPE-063) and a `limit` where no write point exists refuses
> (TYPE-064); `limit` rows at every write point with the rule as a
> HYPOTHESIS on every later version of the binding (a `div-zero` under a
> limited divisor discharges, after a loop included); `limit-subsume` rows
> at every direct call of a sync callee; a discharged `limit` row elides into
> ONE `llvm.assume` over the rule's range clauses; and the caller-side bypass
> — a sync function with a limited parameter emits its body under `.body` and
> its ordinary symbol as the checked entry, and a direct call whose row the
> manifest discharged names the body. The rung is gone.

## D-221 — contract runtime semantics — **SETTLED (user-ratified early; C-16)**

A contract violation is a program-invalid state: the violation channel
is the TRAP route (distinct D-141-space codes for requires/ensures/
invariant), reaching `failsafe` — never a `Result`. In `ensures`,
`result` denotes the SUCCESS value (type T); `old(expr)` is admitted
for COPYABLE values only (entry snapshot), refused for owning types by
name. Contract expressions admit calls only to `never fails` PURE
functions (no allocation, no I/O, no suspension). D-014's injected
`ensures result > 0` on `failsafe` and the non-empty-body check are
implemented at 1.5.3.

> **LANDED — the typing half (1.5.1, 2026-09-03; `meta/roadmap/1.5/1.5.1.md`).**
> Every proposition is a `bool` (`requires`, `ensures`, each `invariant`
> conjunct, `prove`, `assert_static` — TYPE-007, the sentence every `if`
> gets). A contract expression is typed under a CONTEXT that refuses, by
> kind, what a proposition cannot evaluate anywhere (TYPE-060): `await`,
> `move`, `relay`/`?!`/`?|`/`drop`, a `pick` expression, a bare user call
> (the one form is `raw f(…)`; the licence checks `never fails`), a may-fail
> or `effect` builtin, a callee that is not NAMED (a function value, a field,
> a `dyn` — the verifier encodes a call as an uninterpreted function per
> KNOWN symbol), a method on a shared-state receiver, a manufactured view.
> `is_err` is a predicate and passes. `result` and `old(expr)` are keywords
> with their own nodes (D-245, D-243); purity is declared (D-242); `main`
> and `failsafe` carry no contract (D-244); `never fails` may carry a
> contract (D-241). The trap route, the D-014 injection and the obligations
> are 1.5.3's.

## D-222 — `const` retires; `fixed` is the one immutability keyword — **SETTLED (user decision, 2026-08-29)**

Raised by the user while reading 1.4.2b's D-211 work: the message it had just
shipped said "module state is `const` or `fixed`", and the user's recollection
was that `const` had been relegated to `extern "C"` blocks years ago with
`fixed` introduced to replace it. The recollection was right, `TYPE_REFERENCE`
§26 still said so verbatim ("`const` is **ONLY valid inside `extern { }`
blocks**… Use 'fixed'"), and **the implementation had never enforced it** — no
rule anywhere refused `const` outside an extern block.

**What had actually drifted.** A 0.5-era paragraph made `fixed` "the
body-scoped spelling and `const` the module-scoped one" — one rule, two
spellings chosen by context, which is the blueprint philosophy's own target
seen from the other side. `const`'s documented home then vanished under D-149
(no in-process FFI, so no C ABI; the `extern` that returned at 1.1.13c is
driver-wire stubs with no C types), and D-211 had just entrenched the pairing
in a diagnostic every author would read.

**The decision:**

1. **`fixed` is the ONE immutability keyword**, in every position — local,
   parameter, struct field, module binding. It means the value is written once
   and never again; the write may be at compile time or at RUN time, which is
   the half most languages make hard and the half `fixed` was invented for.
2. **`const` is retired from the language.** Not reserved: D-088's rule ("a
   reserved word naming nothing costs a user an identifier and gives a reader a
   keyword they cannot look up") applies, so it is an ordinary identifier again.
3. **The compile-time claim keeps its existing spelling.** `comptime(…)` around
   an initialiser refuses an expression that does not fold
   (`NITPICK-TYPE-004`), so "written once" and "and I know it now" are already
   separable and both checked.
4. **Every module binding lowers to LLVM `constant`.** D-165 requires a
   compile-time-constant initialiser and D-211 requires the qualifier, so the
   writable `global` form had no occupant left. Read-only memory is a stronger
   statement of the same fact than a checker rule alone.
5. **D-211's message becomes "module state is `fixed`"**; the rule it settled —
   a module binding must carry the qualifier — is unchanged.

**Giving `const` a real second meaning was considered and declined.** The
candidate was "the value is known at COMPILE time" (C++'s `constexpr`), and the
case for it was the `never fails` precedent: a checked declaration beats an
inferred fact because it fails where the author wrote the claim. It was declined
because the claim already has that checked spelling (point 3), and because
naming it `const` would carry C++'s meaning rather than C's — the very
collision the rename principle exists to prevent. **The general rule that
decided it, worth applying to the next proposal of this shape: let an author
declare intent the compiler CANNOT infer — write-once is intent — and do not add
a keyword for structure it already derives and already lets them assert.**

**Found while implementing (the reason this was worth doing beyond tidiness):**
`fixed` on a struct FIELD enforced **nothing**. §26 had promised "a field can
never be reassigned after construction" for as long as the keyword existed, and
a direct write, a repeated write and a write through a pointer were all
accepted. There were four `QUAL_FIXED` reads in the entire frontend and none
looked at fields. Now checked in `bindings.npk` beside the other immutability
rules, sharing `NITPICK-ASSIGN-002`. Its sibling — `const` on a LOCAL, accepted
and meaning nothing — is closed by the retirement itself.

## D-223 — A borrow never enters a `wild` slot, and rule B becomes one derivation-aware predicate — **SETTLED (user decision, 2026-08-29)**

Raised by 1.4.6's suite migration. Moving the compiler's own unit tests off
the seed put four of them under `NITPICK-BORROW-001`: helpers that pass
`@`-borrows of their locals together (`parser_init(@toks, @ast, @pd, it)`),
then return the engines by value in a result struct. The stop question was
"over-refusal?" — and the investigation answered NO, then found the real
defects on the other side of the fence. Three probes against the live
checker decided it (reproduced in `meta/roadmap/1.4/1.4.6.md`'s execution
record; each becomes a suite case at landing):

- **The refusal is sound.** The connection rule B fears is CONSTRUCTIBLE in
  fully checked code: a callee takes an interior borrow of one pointer
  parameter (param-rooted, so 0.8.1's exemption leaves the local unmarked)
  and stores it through the other into a `wild int32->:p` field — legal,
  because **qualifiers are not part of the type** (`parse_type.npk:14`):
  `wild int32->` IS `int32->`, so a frame address enters a wild slot with
  no cast and no opt-out. `probe_launder`: accepted, exit 0.
- **The one-borrow exemption is false for self-connectable types.** "With
  nothing else passed in, there is no second borrow to store" — the borrow
  itself is the second borrow. `wire(@n)` doing `p.next = p;`, then
  `pass n;`: accepted, and the returned copy's `.next` dangles into the
  dead frame. `probe_self`: exit 0.
- **F-1's matcher misses derived interior borrows.** `dest_can_hold`
  matches the borrow's exact pointee type, so `Cell{ int32->:slot }` beside
  `@pt` (a `Point` of two int32s) counts as no destination — and the callee
  plants `@(q.a)` into caller-owned memory. `probe_derive`: exit 0.

Two ACCEPTED programs put a frame address in memory that outlives the
frame. The analysis was strict where the compiler's own test idiom lives
and permissive where the danger lives.

**The decision:**

1. **A borrow may not be stored into a `wild`-qualified place** —
   `NITPICK-BORROW-011`, enforced at rule 3's store sites (assignment,
   declaration initialiser, struct-literal field value), where the slot's
   declaration and its `QUAL_WILD` bit are visible. A wild slot holds
   manually-managed memory; a frame address is not manual memory — `dalloc`
   on one frees a stack address, and the 0xAA poison and quarantine
   instruments assume heap. The one door stays D-019's: `=>! wild T->`, the
   explicit surrender of tracking. Classification is syntactic and fails
   closed: wild-provenance is an `=>! wild …->` cast, a read of a
   `QUAL_WILD` place, a builtin whose signature returns wild, or `NULL`; a
   borrow (`@…`, or a binding the holds-table marks) refuses; anything else
   — a plain pointer parameter or plain-slot read — is provenance-unknown
   and refuses, the `=>!` being the acknowledgment where the store is truly
   meant.
2. **Rule B unifies on `can_connect(dest_pointee, src)`**: does
   `dest_pointee` transitively contain a NON-wild pointer slot whose
   pointee is `src` itself **or any type reachable inside `src`** (interior
   places — fields, elements; `any->` matches everything; fuel-bounded,
   failing closed on generics and unknowns). Wild slots are excluded
   because point 1 closes them. One predicate replaces three mechanisms:
   the count-based `borrowed >= 2` arm, `dest_can_hold`'s exact-type match,
   and `escape_connect`'s blanket `carries_pointer` filter.
3. **The pair arm marks a local-rooted borrow argument iff `can_connect`
   holds against some co-source — including itself.** The one-borrow
   exemption becomes "exempt iff not self-connectable", which closes
   `probe_self` while `tt_intern(@t, k)` … `pass t` keeps compiling:
   TypeTable's pointer slots are all wild. The parameter-rooted-destination
   arm keeps its outright refusal (`NITPICK-BORROW-002`), now
   derivation-aware, which closes `probe_derive`.
4. **Amendments**: D-004 rule 3 gains the wild clause; D-117's rule B and
   D-146's F-1 are superseded in MECHANISM (the caller-side
   worst-case-callee model they built stands). The two `escape.npk`
   diagnostics citing them update their citations.

**Why both sides survive, checked against the actual types.** Every engine
table's pointer slots are wild by convention — Ast, SymbolTable, DiagList
(whose own comment states it: "Every other table in this compiler is wild
storage behind a pointer"), TokenList, InternTable — so none is a
destination, no `run()` local marks, and the four migrated tests pass **as
written**. Context structs (Parser, Escape, Resolver, …) keep plain slots
and REMAIN destinations: `BORROW-002` keeps refusing capture-shaped sites,
which are read per site, never annotated past.

**Landing order (inside 1.4.6).** The fix, `BORROW-011`, and the rejection
cases land as their own commit BEFORE the migration commit — the three
probes become rejection cases plus a pair-with-plain-slots case that still
refuses; the migration's minimal reproducer becomes an ACCEPT case. The
selfhost stage is then the audit of `src/`: any site storing a borrow into
a wild slot refuses by name (expected none; one that appears is either a
real hazard or spells its `=>!`). The suite migration finishes on top with
the four files untouched.

**Declined:** restructuring the four helpers around the over-broad blanket
and fixing the two holes separately — strictly more total work for a weaker
analysis in between, and the no-deferral rule bars the "separately".


---

## D-224 — `exit` means process exit in every body, async included — **SETTLED (user decision, 2026-08-30)**

Raised by 1.4.7 step 1. Routing the diagnostic renderers through `dyn Writer`
makes `report` async (every `Writer` operation is async by D-075, which D-071
requires), so `main` awaits it and becomes a coroutine — and `exit` then meant
something different from what it means in a synchronous `main`.

**The two lowerings.** In a sync `main`, `exit N` is `ret N`, and `_start`
(`runtime/npkrt.ll:143`) calls `@npk_exit(%rc)` immediately after — nothing
runs in between. In an `async main` the same spelling stores the code into the
frame's result slot, returns from the RESUME function, and the entry shim
(`fnem_emit_main_shim`) then loads the slot, `npk_dalloc`s the frame, and
returns the code for `_start` to exit with. Three heap accesses AFTER the
`exit` statement has notionally executed.

That window is not theoretical. `src/main.npk:165` calls `wild_release_all()`
immediately before `exit 0i32`, and that release unmaps every chunk — its own
contract is *"after it, only exit; anything still pointing into the heap points
at unmapped pages."* True for a sync `main`, false for an async one: the
coroutine frame is IN the heap just unmapped, and the compiler segfaulted
storing the exit code into it, after writing complete and correct output.

**Why it is a decision and not a repair.** The program is specified to have
exactly two exit paths, both controlled (D-013/D-014, and the safety case
behind them). An async `main`'s exit was a THIRD shape — return through a shim
that later exits — and the fault lived precisely in the gap. One spelling
meaning two things by context is also the blueprint philosophy's own failure
mode.

**The decision:**

1. **`exit` lowers to `@npk_exit` directly in an async body**, after the exit
   defers, followed by `unreachable`. The operand is evaluated first, so D-136
   is untouched (the value is computed before the defers run), and D-014 is
   untouched (a trap still runs no defers). `emit_trap` has emitted exactly
   this shape since 0.10.1; `emit_exit` now matches it in an async body and
   keeps the plain `ret` in a sync one, where `ret` already IS the exit.
2. **The root task's frame moves to the managed allocation entry.** It was
   `@npk_alloc`, which is the WILD-TRACKED entry (`npk_alloc_impl(n, 1)`), so
   it counts toward D-151's `<wild-live>` set — and every `async main` in the
   suite passed the `exit 0` leak check only because the shim freed the frame
   before `npk_exit` ran. Under (1) the shim's exit path no longer runs, so a
   tracked frame would trap −4105 on a successful exit. It cannot be freed at
   the `exit` instead: `wild_release_all()` may legally precede `exit`, and a
   `dalloc` after it touches unmapped pages — the original defect, moved. So
   the frame becomes `@npk_alloc_managed`, which is correct on its own terms:
   the root frame is runtime scaffolding the shim allocates and the program
   never sees, the same class as argv and string bodies, and it should not
   have been in the program's leak accounting in the first place. The shim
   still `dalloc`s it on the ordinary return path; `dalloc` takes a managed
   body back like any other.

D-013 is unchanged — `exit` remains legal only in `main` and `failsafe`, which
is why the root frame is the ONLY coroutine frame an `exit` can execute on.
`failsafe` already exits through `@npk_exit` directly (`emit_trap`).

**Declined:**

- **A — move the root frame off the releasable heap into a dedicated mapping.**
  It fixes the segfault and preserves both existing contracts, but leaves
  `exit` meaning two different things at the lowering level; the third exit
  path survives, and the next construct to reach into that window finds it
  open.

  **Annotation, 2026-08-30, same day: the subsumption claim below was WRONG,
  and the measurement is what said so.** The original text read *"(1) subsumes
  it: with no post-`exit` frame access, where the frame lives stops
  mattering."* There IS post-`exit` frame access. `exit` runs the scope-exit
  sequence — join → defers → drops → reclaims (D-207) — and in a coroutine the
  join mark is FRAME-RESIDENT (role 41, because a scope spans suspensions), so
  the very first thing after `wild_release_all()` is `load ptr, ptr %t6` off
  the frame, followed by the `jn.head` scan. `async_exit_release.npk` still
  took SIGSEGV inside the resume with (1) and (2) applied, and `info proc
  mappings` showed no heap mapped at all. **A and B are complementary, not
  alternatives**: B fixes what `exit` MEANS, A is what lets the code between
  `wild_release_all()` and `@npk_exit` run at all. A is therefore ADOPTED
  alongside B, and (2) above is superseded by it — a frame outside the
  allocator's chunks is in no leak accounting to begin with. The mechanism is
  recorded in 1.4.7.md; the decision that `exit` means process exit is
  unchanged.
- **C — refuse `wild_release_all()` in an async body**, spanned, D-215's shape.
  Honest about the incompatibility, but it leaves an async driver with no leak
  amnesty and so blocks the adoption behind making `src/` free what it
  allocates. That work is worth wanting for its own sake and is NOT what
  defines `exit`; it stays available as a separate item.

---

## D-225 — Declared-uninitialised managed storage holds its canonical vacant value — **SETTLED (user decision, 2026-08-31)**

Found at 1.4.7 step 2, family 11, by a trap the family did not cause.

**The defect.** D-186's fallout made assignment over an owning FIELD drop the
old value. That drop is emitted UNCONDITIONALLY — compare the two `npk.drop.608`
call sites in one module: `graph_init`'s is gated on a drop flag
(`br i1 %t35`), the overwrite has no guard at all. So for a binding declared
without an initialiser — `Front:f;`, the `$$m` out-parameter idiom `src/main.npk`
documents in a comment — the "old value" never existed, and the drop reads
whatever bytes are in the slot. Measured under gdb:

```
drop.3 on 0x7fffffffd420: ptr=(nil) len=0 cap=7ffff7fe0000
```

`cap != 0` is the ownership bit, so the string drop took the free branch and
called `npk_dalloc(NULL)` — heap bad request, trap −4102. Latent since 1.1.12;
`overwrite_owned.npk` does not catch it because it proves the LEAK fix, on
initialised fields. Family 11 changed a layout and surfaced it, which is the
family-10 pattern a second time. **The same shape with non-null garbage in
`ptr` frees an arbitrary address.**

**The decision.** Every `type_drops`-true kind has a STATED canonical vacant
value; its generated drop body is a no-op on that value; and a declaration
without an initialiser WRITES it. This completes a design the drop bodies
already half-implement and document — the Guard's drop says "Null (a zeroed
frame slot) releases nothing", `dyn`'s says "Null DATA is a zeroed frame or an
empty enum arm's payload slot", `shared_arena` calls its null "the belt for a
conditional one", and the string trio gates on `cap`. The invariant was relied
upon everywhere and made true nowhere.

**`OwnedFd`'s vacant is −1, not zero, and this is the landmine.** Its gate is
`icmp slt i32 %f, 0`, and its comment claims "Negative means a zeroed slot" —
which a zero fill does not produce. A zeroed `OwnedFd` holds descriptor 0, a
live one: the unconditional overwrite-drop would `npk_ofd_close(0)` and close
stdin, silently, with no trap. That is strictly worse than the −4102 being
fixed. So the fill is all-zeroes PLUS −1 stored at every `OwnedFd`-typed slot,
walked with the layout the drop generator already walks.

Re-biasing the representation so zero is universally vacant (store `fd + 1`)
was considered and REJECTED: a stored number that lies about the kernel's
number ripples through the whole 1.1.12b surface and taxes every debugger and
analyst forever. **What must be uniform is the invariant — vacant drops as a
no-op — not the bit pattern.** The trio already spells vacant `cap == 0` while
`dyn` spells it null.

**Enums take fixups only along the tag-0 projection.** Variant layouts overlap,
so writing −1 through variant 1's descriptor offset can land in variant 0's
`cap` field and turn a vacant value back into a freeable one.

**`npk_dalloc(NULL)` keeps trapping.** It is what surfaced this defect;
`free(NULL)` permissiveness is the tempting C habit and would have buried it as
silence.

**Instrumented, not asserted in prose.** A whole-tree check builds each
drop-carrying type's vacant value, calls its `npk.drop.<tid>`, and requires no
effect. The `OwnedFd` comment/gate mismatch is the proof that this has to be
executable: the claim was written down, believed, and false.

**Declined — A, eliding the overwrite-drop by definite assignment.** Zero
runtime cost, and wrong as the primary mechanism. Definite assignment is
three-valued (assigned, unassigned, MAYBE) while the emitter needs two, so a
conditional partial fill (`Front:f; if c { f.g = x; } f.g = y;`) lands in the
maybe cell and forces per-FIELD runtime drop flags — a second flag system
beside the one each value already carries in its representation. This project's
defect ledger is exactly "two components agreeing by convention until one
moves" (family 10/11, `fn_end`, the dead `check_runtime_sigs_agree` leg), and
A's divergence mode is an arbitrary free while B's is a missed initialisation
at one emitter site, which is auditable. For 1.5 and Astrée, B also leaves no
undefined bytes in managed metadata at any program point, where A preserves
them and moves the proof into compiler internals an artifact-level analyser
cannot see. **A is DEMOTED, not discarded**: once the representation is true,
eliding a provably-vacant overwrite-drop is a pure optimisation whose
divergence costs cycles rather than memory. Permitted, not planned.

## D-226 — the index type follows the count — **SETTLED (user decision, 2026-08-31; numbered 2026-08-31)**

Ratified during 1.4.7 step 2 and recorded in the subcycle file; given a number
here at the user's direction, because a rule that governs every table the
compiler will ever grow cannot live in a cycle document that moves to `done/`.

**The question.** `List<T>` counts in `int64` (that uniformity is the
collection's own design note — the families it replaced disagreed, and under
D-210 a doubling that overflows TRAPS, which at 64 bits it cannot reach). Most
of those families hand their count out as an `int32` INDEX. So: when the count
widens, what happens to the index?

**Six families in, the same question had three different answers**, which is
the blueprint philosophy's own failure mode arriving inside a migration
undertaken to remove exactly that:

- families 5 and 6 (`InternTable`, `SourceManager`) — a guarded narrow behind
  a NEWLY DECLARED error; family 6's `source.BoundsIndex` grew **38 `failsafe`
  arms** across the tree;
- family 7 (`ModuleGraph`) — a guarded narrow reusing an error the module
  ALREADY declares, at no cost to any `failsafe`;
- `irw_site` — no narrow at all, because the dedup scan's loop counter already
  WAS the index as an `int32`, and D-210 makes its `+ 1i32` trap.

`FnEmitter`'s seven groups — `fnem_pick_push` grows eight arrays in lockstep —
would have invited a fourth.

**The decision:**

> **The index type FOLLOWS THE COUNT — `int64` — unless an external contract
> pins `int32`, and then it is a guarded narrow reusing an error the module
> already declares.**

**The test is whether the `int32` is a real contract or an artifact of the old
representation.** A contract is a width some other component depends on:
family 5's intern id is one, because every AST node stores a name as an
`int32` and the whole table exists to make two names impossible to alias — so
the narrow must be guarded and must keep its own error identity (D-179:
borrowing a builtin error erases the module's unforgeable identity). `Ast` is
the same case and is the family the rule was settled for — every node
reference is an `int32` by design, so `ast_id` narrows through `BoundsId`,
already declared there. `reg_add`'s index in `Suspend` is NOT a contract: it
is an `int32` only because `lcount` was, and three call sites in the same file
consume it. That family needed no narrowing anywhere.

**A newly declared error is the LAST resort, not the default.** It is the one
option whose cost lands on every `failsafe` in the tree, because REACH-002 is
exhaustive over what can reach one — 38 arms for a single file id. Reusing an
already-declared error (family 7, `Ast`) or removing the narrow entirely
(`irw_site`, `Suspend`) spends nothing. The ordering is therefore: **no narrow
> reuse the module's own error > declare a new one**, and the third needs a
reason the first two could not serve.

**A silent `=>!` is never an answer**, at any position in that ordering. The
failure it produces is two distinct entities aliasing one id, which is
precisely the invariant these tables exist to hold; family 5's `int32` counter
got the property for free only because D-210 trapped its increment, and
widening the count is what takes that belt away.

**Scope.** The rule governs `src/`'s tables and any table added later. It is
an engineering rule about representation, not a language rule — nothing in the
grammar or the type system changes — which is why it is recorded here and
carried into `src/frontend/list.npk`'s header rather than into a reference
document.

## D-227 — the query ensures; a memoised layout fact is never read before it is computed — **SETTLED (user decision, 2026-09-01)**

Found at 1.4.7 step 2 while scouting `TypeTable`'s conversion, measured by
experiment three times, and ratified as "compute on demand" before the shape of
the fix was known.

**The defect.** Layout's walk memoises three facts per struct and enum —
`tt_drops`, `tt_haschan`, `tt_hasborrow` — each 0 for "not computed", 1 for
"computed: no", 2 for "computed: yes". Their readers in `types.npk` all end the
same way:

```
if (k == (raw TY_STRUCT()))   { pass ((raw tt_drops(t, id)) == 2i32); }
if (k == (raw TY_ENUM()))     { pass ((raw tt_drops(t, id)) == 2i32); }
```

`== 2i32` turns **both** 0 and 1 into false, so an UNCOMPUTED bit is
indistinguishable from a computed "no" and answers the permissive way:
"owns nothing", "holds no channel", "holds no borrow".

**That is not a corner.** `finish_layouts` runs at `pipeline.npk:494`; stage 2,
the type checker, is at 337. Through the whole of checking most of the table has
no layout at all, and `pipeline.npk`'s own comment says so — calling it "fine
for checking", which is true of the SIZES that sentence is about and false of
the three bits that ride the same walk.

**What the permissive answer costs, by rule:**

| query | decides |
|---|---|
| `type_drops` | **TYPE-046 move-only**, and whether a scope exit drops at all |
| `type_contains_channel` | **D-215**'s dyn-coercion refusal, **D-183**'s `gives` |
| `type_contains_borrow` | borrow containment |

A false "owns nothing" both skips a drop and stops TYPE-046 refusing a COPY of
an owning value — two owners and one double free, which is the exact hazard
that rule exists to prevent.

**Measured, not argued.** Filling `haschan`'s never-zeroed tail with the "yes"
answer made `npkc` refuse to compile ITSELF (two D-215 refusals on
`TextWriter<ByteWriter>`, a generic instance — interned on demand and
interleaved with the passes that ask). Making `type_drops` answer "owns" on 0
made it refuse `LineEnding` under TYPE-046. Both reads land in the window; the
answers the compiler gave were right because those types genuinely own nothing,
not because anything checked.

**D-216 diagnosed this hazard correctly at 1.4.3b** — its comment at
`type_stmt.npk:155` describes it word for word — and fixed it at ONE site,
which is why `ensure_layout` had exactly one caller outside `type_layout.npk`.
A rule enforced only where somebody remembered is sampled, not enforced.

**The decision — the query ensures, and the caller does not have to remember.**

1. `type_layout.npk` gains `type_drops`, `type_contains_channel` and
   `type_contains_borrow` taking a `TypeResolver->` and a scope: they
   `ensure_layout` and then read. The ensure is TRANSITIVE — it recurses through
   arrays, optionals and results, and `struct_layout` walks every field — which
   is what makes delegating to the recorded reader sound.
2. `types.npk` keeps the readers as `type_drops_recorded`,
   `type_contains_channel_recorded`, `type_contains_borrow_recorded`. **The
   UNQUALIFIED name is the correct one**, so a caller who does nothing special
   gets the right answer, and reading the raw bit becomes an explicit, greppable
   choice — the same shape as `raw`, `wild` and `=>!`. Two spellings are
   justified here by a real semantic difference, "what is recorded" against
   "what is true", not by context.
3. The 14 checker sites take the ensuring form; the 30 backend sites take
   `_recorded` because they run after `finish_layouts`; `type_layout.npk`'s own
   six take it because that file IS the computing pass.

**Two more defects fell out of making the query total, and both were invisible
before it:**

- **A payload-less enum never had its three bits written at all.**
  `ensure_layout`'s enum arm set them only inside `if (enum_has_payload(...))`,
  so such an enum got its SIZE written — which makes the re-entry guard
  early-out forever — while the bits stayed 0 permanently. The invariant the
  ensuring query depends on was false for exactly that shape. They are now
  written for every enum.
- **Two live TYPE-046 violations in `src/` itself.** With the fix in, `npkc`
  refuses `ir_expr.npk:4882` and `:7383`, which copied a `PlaceVal` — a struct
  owning a string through `addr` — into `iv_from_place`'s consuming parameter.
  The uncomputed bit had been silently disabling the move-only rule there. Both
  are now `move(...)`, which is what the diagnostic prescribes.

**Instrumented, not asserted in prose** (D-225's rule, applied again). A harness
stage builds a compiler whose three recorded readers treat 0 as the
NON-default answer and requires its emission of `src/main.npk` to be
BYTE-IDENTICAL to the clean one. Byte-identity is stronger than "nothing
refused": it catches a 0-bit read whose flipped answer happens not to trip a
rule. Before the fix that build refuses; after it, it is inert.

**Declined — guaranteeing the pass ordering instead** (lay every type out before
checking). Cleanest to state and hardest to keep: it is a whole-pipeline
invariant with no instrument watching it, and generic instances are interned on
demand DURING checking, so a pre-pass cannot cover them — which is the case that
started this. **Declined — trapping on 0 at a query site.** Right as a belt and
wrong as the mechanism: it needs a new error identity, whose cost under D-226
lands on every `failsafe` in the tree, and a false trap breaks a real compile
where a failing test stage does not. **Declined — treating 0 as "yes".**
Conservative in the safe direction and refuses legal programs, including the
compiler's own two sites.

## D-228 — the orchestration rules are normative — **SETTLED (user decision, 2026-09-01)**

`meta/roadmap/ORCHESTRATION.md`'s R1–R9 and its §4 cumulative-prefix
integration protocol become decision text; the essay stays beside the 1.5 and
1.6 READMEs as rationale, the split D-218 and `1.5/README.md` already use.

The reason it needs the force only a decision has is this project's own
most-repeated failure: **a rule believed in force because a document says so**.
R2 (one writer in `src/`), R5 (a red under parallel load is a stop sign, never
a retry) and R6 (a parallel agent that finds a compiler defect must not work
around it) constrain future sessions' behaviour, and a document nobody is bound
by is the next stale document.

**The three answered sub-questions:**

1. **Fable orchestrates, Opus executes.** It matches the standing split, and
   R8's role — freeze calls, red triage, record composition, no code — is
   judgment-dense and output-light, which is the shape the budget favours.
2. **Calibration is SEQUENCED behind OWED-1**, not run before it. Calibrate at
   6 on a known-green tree requiring all green including the deadline-bearing
   set (`channel_deadline`, `driver_deadline`, `executor_sleep`, anything that
   joins a thread), then 12 only if 6 is clean. Calibrating first would measure
   against a known unknown: the one red the scheme has ever produced is still
   undiagnosed, and under R5 a flake and a real concurrency defect are
   indistinguishable — the worst possible ambiguity for this language.
3. **R6 is ABSOLUTE**, with a clarifying sentence rather than an exception: it
   covers the shipped artifact and its gates — `src/`, `runtime/`, `lib/`, the
   prelude, and the harness's verdict logic. The argument is the record: three
   timing defects that all looked like flakiness first, and R6's own text that
   "a workaround buried in library code outlives the bug, is never removed, and
   is indefensible at verification time". Any named exception is the door the
   rule exists to close.

## D-229 — the diagnostic walk is generic and borrowing, and prints span-sorted — **SETTLED (user decision, 2026-09-01)**

A mechanism correction to D-209's adoption scope and to D-075's design, on the
D-208/D-216/D-224 precedent that a ratified decision's own predictions are
worth testing.

**The defect in the mechanism.** `npkg test`'s capture-and-compare — the thing
D-075 routed diagnostics through `dyn Writer` *for* — cannot be built on an
owned `dyn`. A `dyn` coercion MOVES, so a capture buffer handed to the erased
walk is consumed and cannot be read back. Capture requires the sink to
reference storage the CALLER retains, and every sanctioned way to reference
external state from inside an owned `dyn` is now closed: a channel endpoint is
refused at the coercion (D-215), a borrow inside an erased value is D-207's own
stated hazard, and a `shared_arena` slot is written once before its handle
escapes (D-154) so it cannot back an accumulating buffer. **D-075's premise
predates D-180, D-207 and D-215 and did not survive the language's own
hardening.**

**The decision.** The walk becomes generic and borrowing —
`diag_report<W: Writer>(W->:sink, DiagList->, SourceManager->, Duration)`. The
caller keeps the concrete writer and reads it after the walk returns: the
drivers pass a borrow of their `TextWriter<ByteWriter>`, and `npkg test` passes
a borrow of a capture writer. `diaglist_render(Sink->)` retires into the same
walk, so the second walk OWED-4 recorded goes away rather than being kept.

**What is preserved is the goal that mattered.** The five-copies problem step 1
solved was source duplication, not instance count — ONE walk in source is what
`check_one_renderer` pins, and a generic keeps it. The generic is also the
verification-friendlier shape: monomorphized direct calls instead of a vtable
indirection, which both Z3 and Astrée prefer.

**AND THE WALK SORTS BY SPAN.** `diaglist_render` already sorted and the driver
path never has; once the walks unify, one walk has one ordering and the
question answers itself. The honest argument is NOT D-204: the driver path is
already deterministic run-to-run. It is that sorting makes output a function of
the diagnostic SET rather than of pass discovery order, which is what keeps
1.4.8's capture goldens stable when a future pass reorders — and `diaglist_sort`
is stable, so cause-before-consequence at one span survives.

**Declined — a borrowing `dyn`** (`dyn Writer->` plus admitting borrow-carrying
concretes into `dyn`): two new rules, and it parks a borrow where no analysis
can see it. **Declined — keeping the second walk**: two mechanisms for one job,
and capture would then test a walk production never runs. **Declined — a
read-back method on `Writer`**: pollutes the language's I/O surface for a
harness need.

## D-230 — D-044 implemented: the flag types are real, as one kind — **SETTLED (user decision, 2026-09-01)**

G-1 closed by implementing the settled decision rather than superseding it. Its
premise holds unchanged: `PROT_READ` where an `oflags` belongs compiles today
and fails at run time as an unrelated-looking bug — the D-042 error class,
already eliminated for descriptors — and a user type named `oflags` silently
shadows a decided builtin that exists nowhere.

**The shape.** ONE type kind, `TY_FLAGS`, with the family in the operand
window, exactly as `TY_KERNEL` carries five distinct types in one kind with one
`i32` lowering and compare-only semantics. Members are compiler-known constants
GENERATED from one marked-region authority (the `gen_tables.py` pattern;
D-196's "seven SI base names compiler-known, the derived set in the prelude" is
the precedent). Operations are `| & ~` within a family plus `==`/`!=`; no
arithmetic and no ordering, because a flag set is not a number; mixing families
refuses with its own code. The crossings are `flags => int32` outbound, lossless
and confined to syscall wrappers, and `int32 =>! flags` inbound for the
read-back direction, the acknowledged-loss spelling. **No new grammar
anywhere** — seven builtin type names and N typed constants, zero productions,
which is what makes this cheap before the frontend freeze and expensive after.

**The timeline is what makes it urgent.** 1.4.8's `lib/nfs.npk` grows exactly
the flag-taking surface these types exist for (D-213's `open_beneath`/`openat2`
riders), and landing that API typed beats migrating it later. A new type kind
after "built once, in full" is declared is the token renumbering OPEN_DECISIONS
§5 exists to prevent. B-7's walker instrument makes the sweep enumerable up
front, the way 1.3.1 ran it for `TY_SIMD`.

**Declined — supersede with library enums.** Nitpick enums are tagged unions,
not bitmasks, and there is no operator overloading, so the library spelling is
`oflags_or(a, b)` — two spellings for `|`, which is a blueprint violation, and
strictly weaker than what D-044 already settled.

> **The families answer (user, 2026-09-02) and the landing (1.4.8 step 2).**
> D-044's seven are not all bitmasks: `whence`, `fcmd` and `advice` are ONE
> value per call, never OR-ed, and a flags type would admit `F_GETFL |
> F_SETFL` — the error class D-044 exists to close. So `TY_FLAGS` carries the
> four true bitmask families (`oflags`, `prot`, `mflags`, `fmode`); `whence`
> stays the prelude enum `Whence` it already was (1.1.12b); `fcmd` and
> `advice` are the prelude enums `Fcmd` and `Advice` of the same shape, their
> kernel words mapped in `lib/nsys.npk` exactly as `seek` maps `Whence`. The
> members of the four families are GENERATED `fixed` prelude bindings
> (`pub fixed oflags:O_RDONLY = 0i32 =>! oflags;`) from ONE marked region,
> TYPE_REFERENCE §8 — read as this decision's "compiler-known constants
> generated from one marked-region authority": known through the prelude the
> compiler embeds, with no new resolver category, and a derived set is an
> ordinary module binding that folds. `^` and the shifts are refused for the
> kind (a flag set is named bits, not a word to compute with), and the one
> outbound crossing is `=> int32` exactly. Recorded as an annotation, never
> an edit (the D-085/D-202 pattern).

## D-231 — the integer-width set is split: sub-byte struck, the wide ladder pinned — **SETTLED (user decision, 2026-09-01)**

G-2, answered in two halves because the set contains two different questions.

**STRUCK: the sub-byte widths** (`int1/2/4` and their `u1/u2/u4`/`i1/i2/i4`
suffixes). Measured: zero uses anywhere in `src/`, `lib/`, `tests/` or
`tools/`, and `tt_int` really does compute size 0 and align 0 for them. There
is no semantic story left for them to tell — `bool`, `trit` and `nit` already
OWN the sub-byte meanings, and in this language a type's semantic meaning
outranks its machine representation. A range-limited byte is what
`limit<Rules>` delivers in 1.5, so a stored-as-byte `int4` would be a second
range-limiting mechanism beside it. The suffix table is generated from
LEXICAL_REFERENCE §6.2, so the strike is a marked-region edit plus a
regeneration: one authority, cheap now, freeze-priced later.

**PINNED: the wide ladder** (512/1024/2048/4096). It already lowers natively,
`uint2048` is load-bearing for D-193's Dragon4, and the concern that wide
division would mint libcalls was measured and does not hold: `udiv i512` and
`mul i512` through `llc -O2` on the pinned toolchain produce ZERO undefined
symbols. What is owed is the layout rows TYPE_REFERENCE never had — size =
bits/8, align = min(bits/8, 16), the 0.9.3 rule `int_align` already
implements — and one conformance case exercising arithmetic INCLUDING `/` and
`%` plus a D-210 overflow trap at ≥512 bits, so the pin is executed rather than
asserted. 4096 stays as headroom above the used 2048: it already lowers, and it
costs one table row and one test.

## D-232 — the Astrée input format: C-only is the working default, with a named trigger — **SETTLED (user decision, 2026-09-01)**

C-19's external half stays external — the AbsInt contact is the action, and
asking the question list does not start the 30-day clock. Its internal half is
decidable now, and this is the decision rule rather than a deferral.

**Adopt C-only as the working default with a NAMED TRIGGER: if no first-party
answer has arrived by 1.5's midpoint, the C emitter starts anyway.** The
asymmetry decides it. The research is strong that Astrée ingests C and C++
source only, but cites no first-party documentation. The cost of wrongly
assuming C is bounded — emitter work that still yields a second lowering to
diff the first against, which is evidence in its own right. The cost of wrongly
assuming IR ingestion is discovering a C-emission requirement inside a
non-renewable 30 days, which is the disqualifying branch.

**What proceeds now, without waiting:** the C-emitter design note is written
during the 1.4 close, because it is cheap and it sharpens questions 4–7 for the
contact. Its shape is already determined by decisions in force — the emitter
reads the SAME recorded-type and layout authority the IR emitter does (one
truth, never a second table), the D-150 allocator presents as static pools, the
D-071 mapping is "each thread's executor loop is the analyzed task root", and
the stub list is `npkrt`'s enumerable bottom: `sys` trampolines, futex parking,
clone/execve.

**And the validation instrument is decided with it: differential execution.**
The emitted C, compiled by an off-the-shelf C compiler on the workbench, must
reproduce the full program suite's exit codes. That makes the C emitter's
fidelity measured rather than argued, which is the opt-O2 and absent-fact
philosophy applied a third time — and it is workbench, not artifact, so the
zero-dependency rule is untouched (it governs what ships, never the tools that
check it).

> **SUPERSEDED by D-233 (2026-09-01).** The route this decision hedged — C
> emission feeding Astrée, with the AbsInt contact and the 1.5-midpoint
> trigger — is replaced whole: the evidence moves to the emitted IR itself
> under LLVM-native engines, and the C emitter is struck with the trigger.
> What survives is the asymmetry test this decision applied; D-233 applies
> the same test and it now lands on the other side, because the
> commissioned survey showed the C path's evidence would attach to a
> sibling artifact rather than the shipped one. Recorded as an annotation,
> never an edit — settled text is not rewritten (the D-085/D-202 pattern).

## D-233 — the verification evidence moves to the emitted IR: LLVM-native analyzers supersede Astrée; the C emitter is struck — **SETTLED (user decision, 2026-09-01; supersedes D-232)**

Directed by the user on the commissioned survey
(`meta/roadmap/research/LLVM_Formal_Verification_Tool_Options.md`; the
decision-grade digest with reliability notes is
`meta/roadmap/research/digests/llvm-tools-digest.md` — per the r5/r8 rule,
never cite the report's prose without them). D-232 planned Astrée fed by an
emitted-C rendering; this decision replaces that route whole.

**Why Astrée exits, in the priority order:**

1. **The evidence attached to the wrong artifact.** Astrée ingests C, so it
   would have analyzed the AST→C sibling lowering — a rendering that never
   ships — while the binary comes from the LLVM path. Differential
   execution could TEST the two lowerings against each other, but the proof
   itself would have been about the sibling. Every LLVM-native engine
   analyzes the very IR that becomes the shipped binary; evidence about the
   artifact beats evidence about a model of it. (The report's stronger
   IR→C "semantic chasm" claim — poison and undef distortions — does not
   apply verbatim to D-232's AST→C design; the sibling-artifact objection
   applies regardless, and decides it.)
2. **The one-shot economics inverted.** A single non-renewable 30-day trial
   made verification an all-or-nothing launch, with r3's own literature
   warning that preparation regularly consumes the clock. Open engines
   pinned by commit hash make verification a STANDING INSTRUMENT: run
   continuously, re-run at will, wired into the harness beside opt-O2 —
   the shape every instrument in this project already has.
3. **The C emitter is STRUCK — decided out, not deferred.** Its only
   consumer was Astrée; without one, a second lowering is pure surface. Its
   differential-execution instrument dies with it; leg C below replaces
   that evidence with a stronger form (proof-grade translation validation
   of the real pipeline rather than exit-code agreement of a parallel
   one). The AbsInt contact and D-232's 1.5-midpoint trigger are struck
   with it.

**The replacement — three legs, mapped to evidence classes:**

- **Leg A — whole-program runtime-error absence (the Astrée seat):
  abstract interpretation over our own emitted IR.** The engine is chosen
  at 1.6.0's MEASURED bring-up gate between two candidates: **Clam/Crab**
  (Apache-2.0; master targets LLVM 15, support to 18 today; sea-dsa
  region-based memory — the stronger story for proving D-150's
  header/payload disjointness) and **IKOS** (NOSA 1.3; LLVM 14 today —
  adopting it means the port the toolchain plan always called NIKOS). Gate
  criteria: ingestion of our LLVM-20 emission (or measured port distance),
  alarm quality on three named real programs, runtime, and determinism
  controllability. ONE engine wins; the other is decided out — fewer
  mechanisms. Recorded prediction, for the gate to test rather than trust:
  Clam is likely first-green (the 18→20 textual-IR gap for our
  conservative instruction vocabulary is plausibly nil, while 14→20
  crosses the opaque-pointer break) — measurement decides, not the
  prediction.
- **Leg B — per-obligation proof: the D-218 Z3/SMT architecture,
  UNTOUCHED.** Nothing in this decision moves it; 1.5 proceeds as
  ratified.
- **Leg C — optimizer integrity: Alive2 translation validation beside the
  opt-O2 harness leg.** MIT, tracks LLVM main, so a 20.1-matching commit
  exists by construction. Scoped honestly: per-pass validation where
  Alive2 is competent, its stated inter-procedural blind spot (inlining)
  recorded in the stage's ledger, and the exit-code opt-O2 leg RETAINED as
  the end-to-end net. This moves the prototype's
  optimiser-removed-guarantee class from "tested" to "proved where
  provable, tested everywhere".

**The doctrine extends to every verdict source (D-218.2 generalized).**
Each engine is pinned by commit hash and built locally on the workbench —
auditable, which a licensed binary never was, and the zero-dependency rule
is untouched because it governs the artifact, never the tools that check
it. Verdicts are recorded in the manifest like solver rows: a verdict is a
function of (input, tool build, budget), never of machine load. Alarms
triage into a COMMITTED ALARM LEDGER — baseline plus per-alarm
dispositions, so runs diff instead of restarting, and a new alarm on an
unchanged tree is a stop-the-line event, not noise.

**What transfers from the Astrée preparation** (the 1.6 prep was not
wasted): the data-dictionary idea becomes analyzer-visible range facts in
the emission itself (the D-218.9 `llvm.assume` rows; an engine-specific
assertion intrinsic only if the gate's winner wants one); the stubbing
question becomes the analyzer's model of the enumerated npkrt bottom — the
same TCB.md list 1.5.6 writes; the dry-run discipline stays (a full pass
over a representative program before the evidence package is declared);
and MultiSSE's concurrency seat was never going to fit our runtime — that
evidence is 1.5.7's schedule-exploration harness and the r6 primitive
models, unchanged.

**Depth tools are named candidates with entry criteria, not adoptions:**
SeaHorn (CHC/Spacer) if leg A's triage meets invariants its numeric
domains cannot close — the D-150 chunk-bitmap class is the expected
tenant; SAW/Crux if the Bridge wire marshaling wants
extensional-equality proof beyond 1.5.6's Z3 leg; Heapster and Vellvm
noted as the theorem-proving horizon. This sequencing is a decision, not a
deferral, because TOOL ADOPTION IS MONOTONE: a later analyzer adds
evidence without invalidating any existing proof. That is exactly the
property a language change does not have — it re-opens every touched
obligation — so the standing scheduling rule keeps its force with its
basis restated: **what is scarce is no longer trial attempts; it is proof
invalidation.** Everything entering the LANGUAGE still lands before the
evidence campaign; tools may join the campaign whenever they earn it.

**Declined — adopting the report's five-tier table whole.** Five
heavyweight toolchains at once is breadth-first effort spend against
overlapping evidence classes (SAW's functional-correctness seat overlaps
D-218's contracts; SMACK's bounded search adds nothing sound over leg B —
and the report's "complete path coverage at bound 64" claim is wrong for
this language, see the digest's reliability note 4). **Declined — keeping
the C emitter as a second lowering for differential evidence alone**: an
instrument nobody consumes decays into the next stale document; Alive2
validates the real pipeline instead. **Declined — waiting for AbsInt's
answer before moving**: the survey's sibling-artifact objection stands
whatever AbsInt says about ingestion, so the contact no longer gates
anything.

## D-234 — a `for` captures its bound at entry; a `while` re-reads it — the loop spelling says which — **SETTLED (user decision, 2026-09-01)**

Proposed in 1.4.7's step-3 record and ratified at the subcycle's close. An
engineering rule about `src/` in D-226's shape: nothing in the grammar or the
type system changes, and it is recorded here because it governs every counter
loop the compiler will ever grow.

**The fact the rule rests on, measured by probe before any loop moved.** A
`for (intN:i in lo...hi)` evaluates its range ONCE, at entry — `emit_for`
extracts `hi` into a loop slot — while a `while (i < x.count)` re-reads
`x.count` every iteration. For a bound that is a container's live count the two
are different programs, and this compiler has loops that depend on the
difference: the instance loop in `emit_program` re-reads the table it is
emitting (transitive monomorphization), the escape settle walks, every
`tt_count`/`inst_count` bound under a body that can intern. A conversion by
shape alone would have been silently wrong for exactly those and
indistinguishable at a glance from the loops it was right for.

**The rule:**

> A counter loop is spelled `for (intN:i in 0iN...b)` only where `b` cannot
> change under the loop — a literal, or a local the body never assigns. A loop
> bounded by a container's live count (`x.count`, `raw tt_count(t)`) is spelled
> `while`: that spelling says the bound is re-read, and a reader can trust that
> a `for` in this tree never hides a live bound.

So the two spellings carry MEANING — which is the blueprint argument for keeping
both rather than a style preference for either. 268 of 600 counter loops
converted under the rule at 1.4.7 step 3 and 332 stayed `while`, 175 of them
because their bound is live; the classifier that applied the rule admits zero
further loops on the converted tree.

**Riders.** `..` is the INCLUSIVE range and `...` the exclusive one (OP_REFERENCE
§5); an index loop is therefore three dots. A `for` whose body needs the counter
after the loop, advances it conditionally, or reads it as a search result is not
a plain iteration and stays `while` — the record lists each class with its count.

## D-235 — every kind is decided as a channel element: a simd vector and a function value ride, the sync primitives, atomics and arenas refuse permanently — **SETTLED (user decision, 2026-09-01)**

Closes OPEN_DECISIONS S-6, raised by OWED-8's own instrument: `check_rung_names_
open_cycle` refused a rung message that named neither a cycle nor a row, which
was the instrument saying that an undecided kind is an open decision, not a
state to describe. TYPE-057 (OWED-8) had moved the DECIDED refusals into the
checker — a borrow, a `dyn`, an `OwnedFd`, a `Guard`, an aggregate holding one —
and left a middle class the backend refused as a rung: `Mutex`/`RwLock`/
`CondVar`/`Barrier`, `atomic<T>`, `arena`/`shared_arena`, function values, and
`simd<T, N>`.

**The decision, both halves:**

1. **A `simd<T, N>` and a function value RIDE.** A simd vector is a plain value
   and a code address never dangles; both transfer whole by copying their
   bytes. The simd refusal was an oversight of the 1.3.1 sweep, not a decision.
   `chan_value_kinds.npk` proves the LOWERING: four lanes arrive as four lanes
   and the function arrives callable.
2. **The sync primitives, atomics and arenas refuse PERMANENTLY under TYPE-057,
   with their own message — and so does whatever holds one.** The reasoning is
   D-180's: a borrow of a `Mutex`, an `atomic`, or a `shared_arena` is the
   SANCTIONED way to share one across a spawn, and a holder's borrow is exactly
   what moving the cell through a channel would pull it out from under. What
   other tasks may borrow cannot be moved out from under them; the cell is shared
   as `T->`. A `CondVar` and a `Barrier` are cells of the same family; a plain
   `arena` is owned storage other handles point into.

**Mechanism.** Layout's walk gains a FOURTH memoised bit, `hasshared`, beside
`drops`, `haschan` and `hasborrow` (D-227's discipline: the query ensures, the
reader is `_recorded`, the absent-fact stage flips all four), read by a new
total walker `type_contains_shared_recorded` under an EMPTY excuse table in the
walkers-total instrument. The verdict table in `types.npk` now names every one
of the 47 kinds with a decision; a range answers from its element (two values of
it, D-093); the not-value kinds (an invalid type, a trait, a comptime argument)
are refused before the table is asked. **The backend's admission table names no
rung**: a kind it does not admit is one the checker refused or one that is not
a value, and reaching it is the two tables disagreeing — an internal defect,
never "not lowered yet".

**Declined — admitting the sync primitives under the move analysis alone**
(refuse `move(m)` only while a borrow of `m` is live). The analysis that would
carry it does not exist for spawn-crossing borrows, and a rule enforced by an
analysis nobody wrote is the "believed in force" pattern; the type-level refusal
is total and needs no second mechanism. **Declined — keeping the rung** for the
class: a rung says "later", and there is no later for a language rule (the
standing no-deferral rule).

## D-236 — source paths are recorded relative to the manifest root, for diagnostics and the site table alike — **SETTLED (user decision, 2026-09-01)**

Closes OPEN_DECISIONS S-7, found at the 1.4.7 close. D-179's site table records
each source path AS GIVEN: `npkc src/main.npk` from the tree root emits
`c"src/frontend/token.npk"`, and the same call with an absolute argument emits
the absolute path — 1,489 of the 1,647 site constants in a dry-run refresh of
the snapshot, which the fixpoint (stage2 == stage3) and the STAMP both passed,
because each compares the emission with itself. D-078 says emitted bytes must
not vary with the build tree and D-204's H9 names exactly this leak, but the
`repro` stage tests it with absolute inputs from two cwds — which agree BY
CONSTRUCTION — and the harness's own selfhost emission is invoked absolutely.
The committed snapshot had always been clean only because the README's
commands are relative: a decided property held by the discipline of one line.

**The decision.** The source manager records every file's path RELATIVE TO THE
MANIFEST ROOT — the directory holding `nitpick.toml`, found by walking up from
the main file's directory, or the main file's directory when no manifest is
found — normalised, `/`-joined, and used by diagnostics (`CODE path:line:col`)
and by the site table alike: one spelling, deterministic, and the same bytes
the snapshot carries today when invoked from the root. The path a file is
OPENED by stays as resolved, so `npkc` works from any working directory and
`use` resolution is untouched. A file outside the root shows as `../…`,
relative and machine-independent. With this in force an absolute argument no
longer changes the emission, so the `repro` stage's H9 leg measures what it
was written to measure; the 1.4.7 close's guard (no absolute site path in the
committed `stage1.ll`) stays as the belt.

> **Landed at 1.4.8 step 6 (2026-09-02).** The source manager keeps TWO paths
> per file: the resolved one (what the loader opened by; `use` resolution and
> the module's basename read it) and the shown one, rendered relative to the
> manifest root at the moment the file is added (`path_relative`: segment-wise,
> `..` for a file outside the root). The driver finds the root by walking up
> from the main file's directory probing `nitpick.toml` with the loader's own
> `path_exists`, the working directory coming from `lib/nsys.npk`'s `sys_cwd`
> (a relative argument needs it); a synthesised source — the prelude, a
> derive's or an extern block's generated text — keeps its name. The harness's
> `selfhost` stage now asserts that the compiler's own ABSOLUTELY invoked
> emission carries no absolute site path — the count was 1,479 of 1,637 the
> day before — which is H9's leg measuring at last; the committed-snapshot guard
> stays as the belt over the artifact.

**Declined — as-given paths plus the README's discipline** (the state at the
close): the guard protects the committed artifact and nothing else, and every
emission the harness makes carries the machine's path. **Declined — the
basename alone**: D-156 admits two modules of one basename in different
directories, which a basename-only table could not tell apart, and a
diagnostic that names `util.npk` without saying which is a worse diagnostic.
**Declined — an explicit `--root` flag**: a flag nobody passes is the default
nobody chose, and the manifest already IS the root's declaration.

## D-237 — a rejection test's diagnostics are matched EXACTLY: every reported finding is expected, every expected finding reported — **SETTLED (user decision, 2026-09-02)**

Closes OPEN_DECISIONS S-9, found at 1.4.8 Part D. BUILD_REFERENCE §7.1 has
said since it was written that "unexpected diagnostics fail a test as surely
as missing ones", and no runner ever enforced it: the Python harness matched
expectations as a SUBSET of what was reported from 0.8 on, and `npkg test`
ported the rule as it found it so parity could be measured. The measurement
that came with the port: 17 of 131 rejection files reported a code no
expectation named — nine of them one defect (`tools/resolve_check.npk` never
told the resolver which module is the prelude, so every module-rejection test
carried nine stray `NITPICK-RESOLVE-010` findings against the prelude's own
error declarations; fixed on the spot), the other eight examined one by one:
two `failsafe`s written before D-210 made `IntOverflow` reachable, a `pick`
without the wildcard its selector's type requires, two expectations still
spelling the arity code 1.4.2 retired (`TYPE-007` where `TYPE-054` is
reported), and three genuine second findings the tests never named. A rule
the spec states and nothing enforces is the dormant-rule pattern, in the test
runner this time, and the stale expectations are exactly what it was written
to catch.

**The decision.** On the error channel — findings, with `warning` counted as a
finding — the SET of codes a rejection test reports must EQUAL the set its
expectations name, and every `expect-error-at` still binds its code to its
line and column. A code reported that no expectation names fails the test by
name, as a missing one always has. The note channel keeps its own rule: an
expected note must be reported at its place, an unexpected note is not a
finding and passes (`NITPICK-MACRO-009` says where a body was expanded, and
every expansion test would otherwise have to name a location). An extra is
resolved one of two ways and never a third: a finding the test MEANS is
named with an `expect-error` line beside its construct; an incidental defect
in the test's own text is corrected so the file reports only what it tests.
Both runners change in one step, the parity stage proving they agree, and the
runner self-check gains the case: a negative test reporting a second code its
expectations do not name must FAIL. BUILD_REFERENCE §7.1's measured note is
replaced by the rule as enforced. The eight files' resolutions are
pre-settled in `meta/roadmap/1.4/1.4.8b.md`.

## D-238 — every suite `npkg test` runs is declared in the manifest, and both runners read the one table — **SETTLED (user decision, 2026-09-02)**

Closes OPEN_DECISIONS S-10, found at 1.4.8 Part D. BUILD_REFERENCE §7.1
settled `[[test]]` with three kinds and a `path`; the harness then grew the
real-parser sweep, the five rejection suites, the programs and their
fixtures, the runtime floor's tests and the acceptance suite as hardcoded
loops, and `npkg` mirrors them exactly so the parity diff covers the full
tree. A manifest that declares four of the fourteen suites is a manifest a
reader cannot trust to say what `npkg test` runs — the stale-document shape
D-204 refused for flags — and two runners each carrying the same fourteen
suites in code is two chances to disagree about them.

**The decision.** `[[test]]` grows a `stage` key naming the tool that judges
the suite and what it must say — `parse` (`tools/parse_check`, accepted with
no diagnostic), `resolve` (`tools/resolve_check`, refused with the expected
codes), `check` (`tools/check`, refused with the expected codes), `accept`
(`tools/check`, accepted in silence), `program` (the compiler: emitted,
scanned, assembled, linked, run at -O0 and again through `opt -O2`),
`fixture` (as `program`, built and never run, its name a substitutable
`// argv:` token), `runtime` (a hand-written `.ll` assembled, linked against
the floor and run) — with the three existing kinds staying as `kind` under
the default stage `compile`. `paths` is an array (a single `path` its
one-element shorthand), `recursive` a flag defaulting to false, and a suite
runs in manifest order. The membership rules stay with the stage: a
`resolve`/`check` file with no `expect-error` is a fixture another file
imports and is skipped; a `compile`/`program` file some other file in its
suite imports is skipped. Both runners read the one table and each REFUSES a
stage it does not know, loudly; the hardcoded loops go from both in the same
step, and the parity stage's verdict lists before and after the move must be
identical, suite names included — the move is proven to have changed nothing
before it lands. BUILD_REFERENCE §7.1 carries the schema.

## D-239 — a name the compiler or the prelude owns cannot be declared by a program at any type-namespace declaration, associated types and generic parameters included — **SETTLED (user decision, 2026-09-02)**

Closes OPEN_DECISIONS S-11, found at 1.4.8b step 1 while resolving
`tests/types/rejection/assoc.npk` under D-237. The trait there had declared
`assoc:Error = int32;` at 1.0.6, when `Error` was an ordinary name; D-179 (1.1)
then made `Error` the compiler-known error type, resolved BY NAME ahead of
every user lookup so that it "cannot be shadowed into meaning less"
(`resolve_type.npk`), and from that day one checker read the word two ways:
the impl-signature comparison saw the builtin (so the impl's `int32`
mismatched, TYPE-014, even with `assoc:Error = int32;` written in the impl),
while the object-safety walk, matching assoc names by name, saw the trait's
assoc ("returns an associated type"). The subset matching rule hid it for a
year. Measured with the built checker before deciding: a module-level
`struct:Error = { … };` was ACCEPTED where `struct:Duration` is refused —
RESOLVE-001's prelude clash (`bind_prelude_into`, since 0.3) protects names
the prelude DECLARES, and `Error` is compiler-known, declared by no module —
and `assoc:Duration = int32;` was accepted and shadowed the prelude's
`Duration` inside its trait under D-160's nearer-binding rule, where a
module-level `Duration` is refused.

**The decision.** One rule, no exception by declaration kind: a name the
compiler or the prelude owns cannot be declared by a program at any
type-namespace declaration — every module-scope declaration kind (function,
struct, enum, trait, macro, opaque struct, module binding, unit, error
identity, nested module), an associated type in a trait, a generic parameter
of a function, struct, enum, trait, impl or method. `Error` joins the
protected set explicitly, being the one type name that is neither a keyword
nor a prelude declaration. Members reached through their owner — fields,
variants, methods — shadow nothing and are not covered; the value namespace's
locals and parameters are not covered either (a local `int32:x` cannot make a
type mean less). The rationale is RESOLVE-001's own ("a local one would
silently take over") and blueprint facet 1: `Error` means one thing in every
scope, and a word that the resolver reads one way and another walk reads
another is the defect this rule ends. One code, RESOLVE-001 — every case is
"this name already means something here" — refused by the LOADER, so
`tools/resolve_check` reports it and the test is the module-rejection suite's
(`tests/modules/rejection/owned_names.npk`: six shapes in six scopes). The
object-safety walk's by-name assoc match now agrees with resolution by
construction, since no assoc can carry an owned name. `TRAITS_REFERENCE`'s
sentence carries the extension. Landed at 1.4.8c.

## D-240 — where a sharper refusal fires, the generic one it was written to replace stays silent — **SETTLED (user decision, 2026-09-02)**

Closes OPEN_DECISIONS S-12, found at 1.4.8b step 1: D-237's exact matching
surfaced three sites where two rules reported one mistake — `builtin_args.npk`
36:27 (`TYPE-054` for `..^` into a builtin AND `TYPE-007` for the spread
argument's type), `builtin_args.npk` 95:5 (`TYPE-007` "`drop` needs a
`Result`" AND `TYPE-042` "`drop` discards a VALUE"), and `impl_old_blanket.npk`
22 (`TYPE-012`, whose own header says it exists so the reader is not left with
the generic "is a trait, not a value type" — AND that generic `TYPE-002`).
D-157's rules 1 and 2 are the precedent for one mistake, one report, and
`type_trait.npk`'s no-binding branch already cited it.

**The decision.** One mistake, one report: a rule written to say something
more specific than another is the report, and the generic one is silent where
it fires. Landed at 1.4.8c at the three sites, each fixed at its emitter with
the test's second expectation removed in the same commit: the old blanket
spelling is recognised by ONE probe (`impl_old_blanket_trait`, type_trait.npk)
that `impl_collect` reports through (TYPE-012) and `impl_self_type` asks first,
so TYPE-002 never joins it; `drop` over an operand that is not a `Result` at
all — a never-fails builtin's bare value — is `type_unwrap`'s TYPE-007 alone,
and rule 4's "discards a VALUE" (TYPE-042) speaks only of a `Result` carrying a
value, which is the only case where the sentence is true; a `..^` argument the
TYPE-054 refusal named is not also measured against the register it cannot
become, though its contents are still typed. The principle binds every future
pair of rules the same way.

## D-241 — `never fails` may carry `requires`/`ensures`/`limit<Rules>`: D-163 rule 2's contract row retires — **SETTLED (user decision, 2026-09-03; 1.5.1 S-14)**

D-163 rule 2 refused a contract or a `limit` on a `never fails` function
because "a contract that can be violated at runtime is a failure channel.
C-15/C-16 decide *which*" — the row delegated the channel to two decisions
that had not been taken. D-220 and D-221 took them: a `limit` residue and a
contract violation take the TRAP route (a D-141-space code through D-142's
`npk_trap`, reaching `failsafe`), never a `Result`. A `never fails` body
already admits that channel — `?!`, D-210's overflow trap, D-007's division
trap — so the row's basis is gone, and keeping it would bar contracts from
exactly the functions a contract may CALL (D-242: a contract's callees are
`never fails` and `pure`).

**The decision.** The row retires; `TYPE_NEVER_FAILS_CAN_FAIL` keeps its
other rows (`fail`/`relay`/`return`, `async`, the clause on `main`/`failsafe`).
`never fails ensures result >= 0i32` is legal, and is the shape a contract's
helper takes. Landed at 1.5.1 step 5.

## D-242 — purity is DECLARED: `pure` is a contract clause, checked in the body, read by name at call sites — **SETTLED (user decision, 2026-09-03; 1.5.1 S-15)**

D-221 says a contract expression admits calls only to `never fails` PURE
functions — "no allocation, no I/O, no suspension — the checker's question".
The question was whether the checker DECIDES purity by walking callees or
READS it from a declaration. Inference is what D-163 rejected for `never
fails`, and the reasons transfer word for word: implicit (a store added deep
in a callee silently changes which distant contracts are legal, with the
error far from the cause), non-modular (a fixpoint to verify instead of a
walk over kinds; a function value or a `dyn` can never be licensed), and
unreadable at the declaration.

**The decision.** `pure` is a marker clause in the contract window (`never
fails`'s shape: a keyword, `VerifyPure`, `fn_declares_pure`), orthogonal to
`never fails` — a pure function may `fail`, and a contract writes both words.
A `pure` body may not contain: `pure` on `async`/`thread`/`main`/`failsafe`;
a `move` parameter (a consumed owner is dropped at exit); a call to a callee
not declared `pure`, to a builtin the reference's `Pure` column marks
`effect`, or through a value (a function pointer, a field, a `dyn`); a method
on a lock, an atomic, a channel endpoint or an arena; `wild`/`wildx` storage
or an owning local (its drop frees); a store that reaches memory the caller
can see (through a pointer, a slice, a handle, a `<-` dereference, or a local
holding a borrow); `#wild_ptr`/`#wild_slice`. An impl keeps its trait
method's `pure`. Builtins carry a `Pure` column in BUILTIN_REFERENCE's marked
region, generated into `builtin_pure` (D-163 rule 9's shape), classified by
each row's floor body: five are `pure` (the string views and comparisons),
the rest `effect`. **Purity never rides a function TYPE**: an indirect callee
is refused where purity matters, so D-163 rule 8's identity question does not
reopen — and 1.5.3 can encode a contract call as an uninterpreted function
per KNOWN symbol, which is sound exactly because the body is a function of
its arguments. `NITPICK-TYPE-061`. Landed at 1.5.1 step 4.

## D-243 — `old(expr)` is a keyword operator with its own node; an `invariant` admits it — **SETTLED (user decision, 2026-09-03; 1.5.1 S-16)**

D-221 admitted `old(expr)` in `ensures` for copyable values and said nothing
about its spelling. The language already answered the shape question for
`is_err` (D-096): a call that is not a call confuses every reader and every
tool. **The decision.** `old` is a keyword (VerificationKeyword), `old(expr)`
an `ExprOldExpr` with the operand in slot `a` (`move(place)`'s shape), legal
in `ensures` AND in an `invariant` with one meaning — the operand's value at
the FUNCTION's entry (Dafny's rule; a loop invariant relating a running total
to the entry value is the textbook use) — never nested, never of `result`,
and only of a COPYABLE value: neither owning (`type_drops`) nor
address-bearing (`type_holds_address`, the layout table's pointer predicate
made public). Its type is its operand's. Measured before adding: two locals
named `old` in the tree, both renamed. Landed at 1.5.1 step 3.

## D-244 — `main` and `failsafe` carry no contract — **SETTLED (user decision, 2026-09-03; 1.5.1 S-17)**

Nobody calls them, so a `requires` has no caller to hold to it and its
violation would name the operating system; their exit is not a return, so
`ensures result` names nothing; and D-014's positive-return postcondition on
`failsafe` is the COMPILER's (1.5.3 injects it) and is never spelled by the
author. The same shape as D-163's terminal row. `NITPICK-TYPE-060`. Landed at
1.5.1 step 5.

## D-245 — `result` is a keyword with a leaf node, in `ensures` alone — **SETTLED (user decision, 2026-09-03; 1.5.1 S-18)**

`result` was "an ordinary identifier here, given meaning by the verifier"
(D-002's parse note): the resolver exempted an UNBOUND `result` inside
`ensures`, so a parameter named `result` bound first and meant the parameter
in its own postcondition — one spelling, two meanings by what else was
declared — and 1.5.3's encoder would have met a nameless identifier to
re-match by context. The prototype reserved `result`. **The decision.**
`result` is a keyword, `ExprResultValueExpr` a leaf like `$`, typed as the
function's SUCCESS type under `ensures` and refused anywhere else; the
resolver has nothing to exempt. Its token is `KwResultValue`, the one keyword
whose token name is not its spelling (`Result` owns `KwResult`); the
generator carries a one-entry map for it. Measured before adding: one field
name in about a hundred and ten places, all renamed, and the dead `fails on
result` extern spelling (D-002) still parses. Landed at 1.5.1 step 3.

## D-248 — every file's first declaration is its header, and the entry points are the root module's — **SETTLED (user decision, 2026-09-03; 1.5.1b S-21)**

The workbench reported (their O-N8, DEF-2 in OPEN_DECISIONS §2f) that a root
file whose `mod:` named a SIBLING that existed compiled at exit 0 with two
`define i32 @main`, which `llc` then refused. The loader's own comment said why:
a file's header and an import written first are the same shape — a member-less
`mod:name;` — so the header was resolved like any import; when the name was the
file's own it found the file itself and returned, and when it was a sibling's
it loaded the sibling and merged it. Measured at planning: 525 source files,
240 with no header at all (`src/main.npk`, the tools, `npkg/main.npk`, most of
`tests/rejection/`), three whose first `mod:` was not the basename, all
legitimate then. **The decision, both halves.** (1) The header is mandatory:
every file's FIRST declaration is `mod:<basename>;` (`mod:<dir>;` for a
`dir/mod.npk`), member-less, binding nothing and loading nothing; anything else
first, or a `mod:` naming another module, is `NITPICK-RESOLVE-012` at that
declaration (line 1 for a file with none), one code, two texts. The loader
skips the header slot as an import, so a wrong header can no longer load a
sibling, and RESOLVE-005's 1.0.8 "if this line is the file's own header" hint
retires: the loader says it directly now. (2) `main` and `failsafe` are declared
in the ROOT module only, at its top level; in any other module, or inside an
inline module, either is `NITPICK-RESOLVE-013`, checked in the whole-graph pass
beside D-239's owned names — a library cannot smuggle an entry point, and the
emitter's four by-name `@main`/`@npk_failsafe` sites are safe by the rule
upstream. The alternative — header optional, identified by name — cost no
sweep and closed the reported defect through (2) alone, but left a mismatched
header a silent import whenever the sibling declared no `main`, and left the
loader unable to say "your header is wrong", which cost a cycle at 1.0.8.
Explicit over implicit: every file says what it is. **Landed at 1.5.1b step
1**: the sweep gave 228 files their header by script (`meta/roadmap/1.5/tools/
add_headers.py`, kept for the record) and shifted every pinned line in them by
one, D-237's exact matching proving the shift complete; the fifteen conformance
files numbered `00_…`–`14_…` took a `c` in front, because a module name is an
identifier, and six files whose basename was a KEYWORD — which is why none of
them ever had a header: `mod:derive;` is a parse error — were renamed
(`src/frontend/macro/derive.npk` → `derive_gen.npk`, the programs `arena`/
`shared_arena` → `arena_basic`/`shared_arena_basic`, the rejection tests
`arena`/`assoc`/`wildx` → `arena_rules`/`assoc_rules`/`wildx_rules`); the
sweep script refuses both shapes by name. And four compiler files were renamed
for the SNAPSHOT'S sake (D-205): the committed builder's collector binds a
header as a module symbol in the file's own scope, so a header naming a
function the file declares or imports clashes under it — `src/main.npk`
(its own `main`) is `src/npkc.npk`, the compiler's entry by name;
`resolve_type.npk` (its own `resolve_type`) is `type_resolve.npk`;
`type_access.npk` is `type_members.npk`; `type_decl.npk` (`types.npk`'s
exported `type_decl`) is `type_decls.npk`. Every compiler built from this
source skips the header slot in collection (`collect_file_module`), so the
header binds nothing and a file may declare a function named after itself;
the renames stay, since a file called `main` for a function called `main`
said less than `npkc` does. Both runner self-checks' generated
cases took identifier names and a header; the ecosystem's libraries already
led every file with its header by house rule, so the re-pin costs them
nothing.

## D-249 — a view-maker's result borrows its operand: the `Views` column — **SETTLED (user decision, 2026-09-03; 1.5.1b S-22)**

The workbench reported (their O-N9, DEF-3 in OPEN_DECISIONS §2f) that a
`uint8[]` view of a local string returned out of its frame compiled at exit 0
and the caller read the allocator's 0xAA poison — while returning `@x`, or a
struct literal holding `@local`, was `NITPICK-BORROW-001` beside it (D-004
rule 2). The escape analysis knew `@` and (in the suspend walk only, D-191)
the range-view `arr[lo...hi]`; it named neither `string_bytes` nor
`string_from_bytes`, D-186's one remaining view-maker, anywhere: a call's
argument passed by value is no borrow, so the call's result was not one.
**The decision.** BUILTIN_REFERENCE's marked regions carry a `Views` column
beside `Pure` (the 1.5.1 step-4 shape, the same generator, hard-failing on a
cell it cannot read): `—`, or the 1-based index of the argument whose storage
the result aliases — `string_bytes` 1, `string_from_bytes` 1, every other
row `—` — generated into `builtin_views`. The escape analysis treats a call
whose builtin has an index, and the range-view by kind, AS IF `@` HAD BEEN
WRITTEN AT THAT ARGUMENT: a borrow rooted where the argument is rooted, so
rule 2 (return), rule 3 (store), rule A (launder through a call) and rule B
(a destination among the arguments) apply to it with no arm of their own.
The column is the one authority on a builtin's aliasing — a hard-coded pair
of names in the analysis would be a parallel authority beside the 1.4.2
table, which is what the alternative was and why it was refused. **Landed at
1.5.1b step 2**, with the refinement the compiler's own idioms required: `@`
borrows the FRAME storage under a place, and a view does too when that
storage dies at the frame's exit (a `string`, a `buffer`, an array, a struct
by value), but a view whose place roots at a POINTER-SHAPED binding (a wild
pointer, a slice, a `cstring`, a `Handle`) aliases the pointee, which lives
where the pointer's provenance says — the wild-store rules already govern
that, and a binding that holds a borrow is carried by the identifier rule as
before. A view of a string literal views static storage. A view of a
TEMPORARY is refused outright (`NITPICK-BORROW-012`): `@` of one cannot be
spelled, and the value dies at its statement's end (D-246). Two consequences
fell out: a builtin's result is borrowy only as the column says — rule A had
been ready to call `string_concat` of a view a borrow, which would have
refused every owned copy of one — and a `move` parameter is the callee's own,
so `root_is_param` no longer counts it as the caller's storage; `@` of one
had the same hole. `tests/analysis/rejection/view_escape.npk` carries the
reporter's contrast set (their `0667ecb`), `tests/accept/views.npk` the legal
shapes, and `view_in_frame.npk` their case 6 as a program.
*(Landing note, 2026-09-03: the rule's first pass over the compiler, `npkg`,
the tools and the libraries reported eight sites, and reading them refined it
three times — a view over `#ptr_add`/`#wild_slice` is a view of what the
pointer reaches (the substring idiom), never a temporary, while `#wild_ptr`
roots nothing; a `for`'s BORROW-009 asks whether the iterated ELEMENT type
can carry a pointer, since a range's integers cannot carry a borrow whatever
the bound's operands hold; and the return and rooting walks look through a
struct or array literal to its values, so a literal holding a view of a
parameter travels up as `@param` does. Rule B learned that a view is stored
AS ITSELF — a `uint8[]` or `string` slot is a destination for a view of
exactly its type — which is what the reporter's case 6 needed to fire.
Three programs changed hands: `srcmgr_text_proven` returns the view it
already held, `lexer_init` returns its literal directly, and `npkg`'s
`ctx_init` copies the root of its `move` parameter instead of viewing it.)*

## D-250 — derived comparisons over NAMED types and payload enums — **SETTLED (user decision, 2026-09-03: "ratify S-23 as recommended"; 1.5.1b step 3b)**

The workbench's DEF-4 (their O-N10): `#[derive(Ord)]` on an enum with a payload
compiled to a TAG-ONLY order — `Literal(7).cmp(Literal(9))` was `Equal` — by a
1.0.9d design comment that was right for `Hash` and wrong for an order; and
`#[derive(Eq)]` on the same enum, or on a struct holding a derived struct,
refused with a type error INSIDE `<derived-1>`, a file the user cannot open,
because every comparison the generator wrote was an operator and an operator
is refused on a named type. No test in the tree derived over a named type;
the path was written and never run. **The decision.** (1) A derived `Eq`,
`Ord` and `PartialOrd` on an enum compare the TAG first (declaration order,
as before) and, for equal tags, the payload of that variant — generated as a
`pick` over both operands per variant. (2) An operand's spelling decides how
it is compared, because the generator sees TYPE NODES and not types (derive
runs before collection): a builtin spelling by its operator, a NAMED type
through its own `eq`/`cmp`/`partial_cmp` (relayed — the prelude declares
the three methods may-fail), a `simd` by the lane-collapsing `.any()` as
before, and a generic parameter of the subject by operator (D-161's
no-bound story: the prelude implements `Eq`/`Ord` for no scalar, so
`Box<int32>` has no method to call — `Eq` over a parameter field derives, an
order over one stays refused by the checker as before, and whether the
prelude should implement the traits for the scalars so a synthesized bound
and the method form could serve is S-24). (3) A member no derived comparison can
be written for refuses AT THE USER'S DECLARATION, by name (`NITPICK-DERIVE-006`),
naming the field or variant and its spelling: an OWNING payload (`string`,
`buffer`, `dyn`, `List<…>`, `OwnedFd`, an arena, a sync primitive, a
`Channel`), which the `pick` cannot bind without consuming it (S-5/D-216's
consuming form is the only binding form; a borrowing form is a separate
question), and an array or slice, which has no `==`. A named USER type is
admitted and compared through its method — its drop cannot be known before
collection — so a user type that turns out to own still reports inside
`<derived-N>`: the one gap left, recorded here. (4) `Hash` stays tag-only
(D-123; a colliding hash is correct, if weak). (5) The stale "refused rather
than generated" comment goes. **Landed at 1.5.1b step 3b**: `derive_payload.npk`
pins the reporter's 321 (Less, Equal, Less) and the struct-through-`Inner`
shapes; `payload_owning.npk` the three refusals; `deriving.npk` the accepted
shapes. A payload-less enum's derived bodies are byte-identical to before.

> **[D-258, 2026-09-05]** Clause 2's operator for a builtin spelling is
> amended: a builtin SCALAR member is reached through the PRELUDE's impl of
> the trait being derived (D-257), a `string` FIELD through its own, and
> the float answers the operator got wrong (`Equal` for `nan` under
> `partial_cmp`; a total `cmp` over a float) are the prelude's now. The gap
> this decision recorded closes under D-259.

## D-246 — statement-end temporaries: an owning value no place takes is dropped when its statement ends — **SETTLED (ratified with 1.5.1b; landed at step 4, 2026-09-04)**

The workbench's DEF-1 measured it and D-183's open item named it: an owning
value that no place takes — a call's result lent to a parameter, a receiver,
an operand, a discarded result, a `dyn` cell built for a borrowing slot — was
never dropped, so `t = string_concat(string_concat(t, "b"), "c")` leaked the
inner result every iteration (the nested probe peaked at 9 992× the bound
form). **The decision.** Such a value is a TEMPORARY OF THE STATEMENT THAT
PRODUCED IT: dropped when that statement ends, on every path it leaves by —
its normal end, `relay`, `pass`, `fail`, `return`, `give`, `break`,
`continue` — in reverse order of creation and after the statement's own
effect (a returned value is stored before its statement's temporaries drop:
D-207's seam is store, drop the temporaries, unwind, return). A temporary is
TAKEN, and not dropped by its statement, when a declaration binds it, an
assignment stores it, a `move` parameter or a spawn's argument consumes it, a
struct, array, `Result` or variant literal's slot holds it, an arena or a
channel or a lock cell takes it, a `pass`/`return`/`give` carries it out, or
a consuming `pick (move(v))` takes it apart. A temporary lent to a plain
parameter, a receiver or an operand is borrowed and dropped after; a
condition's temporaries drop where the condition is decided (the verdict is
a copied bit and the branches may run for a long time; a `while`'s every
iteration); a coercion or wrap transfers — the `dyn` cell or the `Optional`
holds what the source held and is the temporary in its place. A trap runs no
drops (D-014). In a coroutine the only temporary alive across a suspension is
an `await`'s own operand (D-178); it lives in a frame region appended after
the flag bytes and drops after the resume. **The lowering** registers a
temporary where it is PRODUCED (`emit_expr`'s one seam: a fresh owning
value of a producing kind, never a place, never a view-maker's result) —
stored to a slot of its own beside an i8 flag that says the producing path
ran, zeroed in the entry block (the first resume, for a frame temporary) so
a path the statement did not take reads "not produced" — and every drop is
flag-guarded and clears the flag, so a second exit path or a loop's next
iteration cannot free it twice; a keeping consumer marks it taken by its SSA
name and no drop is emitted. `temp_drops.npk` runs every shape 2 000 times;
`temp_fd.npk` proves the drop by the descriptor table (2 000 opens lent and
closed); `temp_relay.npk` the relay's error path; `temp_await.npk` the frame
region under `// stress: 40`; the `cost` stage's temporaries probe holds the
nested form to 4× the bound form's peak — measured ×3.0, three live bodies at
the store against two (the plan wrote 2 before the mechanics were known; the
probe's comment carries the count).

## D-247 — `List<T>` is compiler-known and OWNING — **SETTLED (ratified with 1.5.1b; landed at step 5, 2026-09-04)**

The compiler's own growable collection (`src/frontend/list.npk`, 1.4.7's
twenty-two families made one) was a `wild T->` block with a count and a
capacity that nothing ever freed: every pass's tables leaked to process exit,
which is where the compiler's 10 GiB peak over itself came from (the `cost`
stage's `self` unit at the 1.5.1 close), and a by-value copy of a `List`
aliased one block under two headers — the next `list_push` that `ralloc`ed
left the other dangling. **The decision.** `List<T>` is compiler-known and
owning: `type_drops` answers true for it unconditionally (as for `dyn`),
whatever its `wild` field says; its generated drop body drops the `count`
elements it holds through `T`'s drop where `T` owns, then hands the block back
through `npk_dalloc` (`cap == 0` is the vacant List, D-225, which owns
nothing); it is therefore move-only under TYPE-046, which also refuses the
aliasing copy. A struct holding a List drops it through its own generated
body; a List that must outlive its scope is moved, like any owner; growth
stays `ralloc` on the block, and the drop frees whichever block is current.
The precedent is `OwnedFd` (TY 39): a compiler-known type whose drop does the
manual thing on the author's behalf, by decision. **Why not the
alternatives**: a `buffer`-backed List frees its block and leaks its owning
elements; a user-declared drop hook is a destructor design the language does
not have; manual `list_free` + `defer` at 153 holders is the regime the
managed lowering exists to replace. **Landed at 1.5.1b step 5, keyed on the
`list` module** — the loader records the scope of the file module named
`list` and the layout marks a struct there named `List` with exactly the
shape `{ items: a pointer; count; cap }` as owning — because the declaration
cannot move into the prelude until a snapshot carries it (D-205: the builder's
embedded prelude has no `List`, and `src/` must compile under the builder); the
move to the prelude, where D-239 makes the name unique, follows the cycle's
snapshot refresh as step 5b, and `generic_list.npk`'s own same-named test
struct is renamed then. `list_drop.npk` (100 rounds of 10 000 strings, the
`lists` probe holding it to 2× `list_once.npk`'s peak), `list_nested.npk` (a
List in a struct in a List), `list_fds.npk` (1 500 descriptors in three
dropped lists, against a table of 1 024) and `list_moved.npk` (TYPE-046,
TYPE-047) carry it.

The first program to drop a List found the defect recorded under D-183's
landing note of the same day: `pass xs.count` cleared the list's drop flag,
because the emitter's `pass` clear never asked whether the passed value's
type drops — every owning local returned by one of its copyable fields had
leaked since 1.2.3, and the suite's descriptor-exhaustion proofs could not
see it under the session's descriptor limit. Both are fixed in the same step
(`pass_field.npk`, `fd_ceiling.npk`, `[limits] nofile`).

> **[2026-09-04, S-25 ratified by the user ("ratify all seven as
> recommended").]** The second half as landed at step 5b: the struct AND its
> functions (`list_init`, `list_push`, `list_reserve`) live in the PRELUDE —
> a compiler-known owning type whose operations need an import is one
> spelling in the prelude and another at every use, the context-dependent
> shape the blueprint rule refuses — through the bridging build the seed
> README documents; `src/frontend/list.npk` and its forty-odd imports are
> gone.

## D-251 — `limit<Rules>` live: the check after every write, `LimitViolated`, a limited binding has no address, a `limit` where no write point exists refuses — **SETTLED (user decision, 2026-09-04: "ratify all seven as recommended"; 1.5.2 S-28; lands at 1.5.2 steps 1–3)**

D-220's three write points made precise, and the two rules the first
measurement of the surface showed were missing (`meta/roadmap/1.5/1.5.2.md`
§3 L-3, L-4, L-5, L-10, L-14; §4.2 the probes). **(a) The check runs AFTER
the write, over the binding's WHOLE current value**, at its initialiser (a
declaration without one is not a write point — D-225's vacant value is
never read, by definite assignment), at every assignment to the binding or
to any part of it (a field or element store re-checks the root; the
arithmetic's own guards run first on a compound assignment), and at the
callee's entry for a limited parameter — sync and coroutine alike, where a
coroutine checks once per task at the body's start. One helper, one shape,
the division guard's: the value loaded, the rule's generated predicate
called, a branch, `npk_chain_reset`, `npk_trap`. A rule is ONE generated
function per `Rules` declaration (`define i1 @"npk.<module>.<name>"(T)`),
refinements then clauses in source order, short-circuit, so a clause's own
guard sites exist once and have one row each. **(b) The residue is
`LimitViolated`, −4111**, through D-142's route; REACH arms it wherever a
function has a write point and walks the rule's clauses for their own arms;
1.5.3's three are `RequiresViolated`, `EnsuresViolated`,
`InvariantViolated` at −4112, −4113, −4114, reserved here. **(c) A limited
binding has no address**: `@`, `$$m` and `$$i` of a place rooted at a
limited local or parameter refuse (NITPICK-TYPE-063), and so does a
`move`/`pass` out of a proper sub-place of one (D-254's vacate is a write
no rule can be asked to admit); a limited value passes by plain argument
(D-183 §4) and a whole-binding move is a read. Measured before the
decision: all three spellings and a store through each were ACCEPTED — a
write no write point sees, LIVE-1's exact shape. The relaxation considered
and decided OUT: an address as a direct call argument of a call whose
result holds no address, the call then a write point whose row is always
`open` — a second write-point class, a result-shape rule and per-read
hypotheses for escaped names, for a case a by-value rewrite covers. **(d)
A `limit` where no write point exists refuses** (NITPICK-TYPE-064): a
trait signature's parameter (accepted and silently dropped by the impl
before this), a `wild`/`wildx` binding (its writes are the manual
regime's), a `comptime` function's parameters and locals (the folder is a
second evaluator); `main`/`failsafe`'s parameters refuse under D-244's arm.
A `stack` binding, a `move` parameter and an impl method's own parameters
are ordinary. **(e) `limit-subsume` rows are one per DIRECT call site** of
a callee with limited parameters — the caller's knowledge of every
argument against the callee's rules, guard `no`, elision `none` as the
catalogue ratified them (D-252 is the step that gives them a guard). The
rule is a HYPOTHESIS on every version of a limited binding after the
site's own obligation (never inside its own cone), which is what lets a
`div-zero` under a limited divisor discharge, after a loop included.

> **LANDED (1.5.2 steps 1–3, 2026-09-04).** Two amendments taken on
> contact, both recorded in the plan's record: a coroutine callee's call
> sites carry no `limit-subsume` row (its check runs at state 0 and nothing
> at the call could elide it — D-252's reason, applied to the row), and the
> row's goal carries no per-conjunct `:named` tags (`--explain`'s model
> assigns the arguments, which names the parameter).

## D-252 — the caller-side bypass: a discharged `limit-subsume` row lets a direct call skip the callee's entry check — **SETTLED (user decision, 2026-09-04; 1.5.2 S-29; lands at 1.5.2 step 4)**

D-220's "caller discharge is an elision like any other" made real
(`1.5.2.md` L-13). A SYNC function with at least one limited parameter
emits its body under `@"<sym>.body"` and the ordinary symbol as the CHECKED
ENTRY — the entry checks over the arguments, then a `tail call` of the body.
Every non-call reference (a function value, a vtable slot, a spawn or thread
entry, a derive or bridge stub) names the ordinary symbol by construction;
a DIRECT call whose `limit-subsume` row is discharged calls the body; every
other call, and every call of a coroutine (its parameters arrive in a frame
the await site builds, and its check runs at state 0), calls the checked
entry. The row's elision reads `elided` when the call names the body,
`retained` otherwise; D-218.7's catalogue row for `limit-subsume` changes
its guard column from `no` to `yes (the callee's entry check, at that
call)`. A belt in both runners: every `.body` occurrence in an emission is
the callee of a `call`/`tail call` or its own `define`, and the count of
`.body` callees equals the discharged `limit-subsume` rows. Why: without
it, nothing a caller proves ever removes a limited PARAMETER's check — the
common placement pays the full price in every build, and D-068's
"constrained code reaches the speed of unconstrained code" is false for
it; the mechanism is modular (no whole-program elision, no caller-dependent
callee row) and its one hazard class is closed by construction plus one
grep.

> **LANDED (1.5.2 step 4, 2026-09-04).** `.body` sits inside the quotes of a
> D-156 symbol; the checked entry checks over the argument registers (or
> assumes, where the manifest discharged the entry row) and `tail call`s the
> body; `thread` functions keep one symbol with `async` ones; the belt in
> both runners counts defines, tail calls and direct calls.

## D-253 — derived comparisons over a generic-parameter field take the method form under a synthesized bound; the prelude implements `Eq`/`Ord`/`PartialOrd` for every scalar — **SETTLED (user decision, 2026-09-04; 1.5.1b S-24; scheduled as 1.5.2b)**

D-250 made a derived comparison follow the operand's spelling — a named
type through its own `eq`/`cmp`/`partial_cmp` — and left a GENERIC
PARAMETER field comparing by operator (D-161's no-bound story), so
`#[derive(Eq)] struct:Box<T>` works and `#[derive(Ord)]` on the same is
refused inside `<derived-1>` (`<` on an opaque `T`, D-107). **The
decision**: the prelude implements the three traits for every scalar (an
`impl` per width, generated from the width ladder as the `Hash` impls are
— about thirty impls), a derived impl whose subject is generic carries a
synthesized `T: Eq`/`T: Ord`/`T: PartialOrd` bound, and a parameter field
compares by the method form — one rule for every named spelling. The cost
is the truthful one: a bound on a derived impl changes which instantiations
compile (`Box<Point>` needs `Point: Ord`). Nothing is blocked today (the
workbench, asked, wants no order over a parameter yet); it lands as
**1.5.2b**, frontend and prelude only, after 1.5.2 and before 1.5.3, with
no snapshot refresh (the prelude is source the old builder reads).

> **LANDED (1.5.2b, 2026-09-05), as D-256…D-259 refined it.** The three traits
> became seven, generated for every scalar family the prelude can name (D-257);
> the method form became the rule for every member class (D-258); and the
> bound is enforced where the impl is used (D-256), which the planning found
> the language had never done for any family impl. "About thirty impls" is 348
> rows in thirteen families, the hand list's width problem being exactly why.

## D-254 — a `move` or `pass` out of a FIELD or an ELEMENT leaves the type's canonical vacant value; the aggregate stays live — **SETTLED (user decision, 2026-09-04; 1.5.1b S-26; landed at 1.5.1b step 5 as the fix)**

D-183's recorded partial-move item, closed both ways. A `move(place)` or
`pass place` out of a field or an element of an owning aggregate leaves the
type's canonical VACANT value (D-225) in the place; the aggregate stays
live, its later overwrite drops nothing (D-186's unconditional field drop
is then correct), its scope-exit drop releases the remaining fields, and
only a WHOLE-binding move clears a drop flag (D-183). Before it, a field
move cleared the whole root's flag (every sibling leaked) and, because the
field overwrite drops unconditionally, a field moved out and then
reassigned freed the moved-out value a second time — `saved = move(r.env);
r.env = move(frame);` in the resolver's constant folding, invisible to the
compiler (its `main` exits without drops) and a heap fault in three unit
tests the day `List<T>` began to own. A vacant List grows from zero on its
first reservation. The checker's D-065 whole-binding invalidation is
unchanged (conservative). One rule — "after `move`, the source owns
nothing" — for both spellings, no new syntax, no field-granular flags; the
alternative, refusing partial moves, would strike the resolver's own idiom.
`partial_move.npk`; D-183's dated note of 2026-09-04 is this decision's
first record. D-251 adds the one exception: a proper sub-place of a
LIMITED binding may not be moved out of, since the vacate is a write no
rule can admit.

## D-255 — the statement after `wild_release_all()` must be `exit` — **SETTLED (user decision, 2026-09-04; 1.5.1b S-27; landed at 1.5.1b step 5 as the fix, NITPICK-TYPE-062)**

The call unmaps every chunk of both regimes (D-151), so no drop, no
allocation and not even the trap route (which allocates its origin chain)
can run after it; a `main` that released and then RETURNED ran its
scope-exit drops over unmapped memory the day `List<T>` began to own, and
the runtime's refusal then died in its own trap route — an uncontrolled
stop. TYPE-062 requires the statement after `wild_release_all()` in its
block to be `exit`; what must be measured after the release goes into
`exit`'s operand, which is evaluated after the call
(`argv_after_release.npk`, `leak_cleanup.npk`; 45 test files that carried a
stray second call were collapsed). One shape, greppable, and the only one
under which "controlled shutdown" survives the release. The main thread's
TLS block became a raw mapping in the same step so that a trap raised
inside `exit`'s operand after the release reaches `failsafe` (DEF-12).

## D-256 — a family impl applies to an instance only when its bounds hold, decided where the impl is USED — **SETTLED (user decision, 2026-09-05: "I say go with those"; 1.5.2b L-1/L-2, DEF-15; lands at 1.5.2b step 1)**

Found by 1.5.2b's planning (OPEN_DECISIONS DEF-15): `find_method` and
`type_implements` matched a family impl (`impl:<T: Ord>:Box<T>:Ord`, 1.0.4b)
to an instance (`Box<Point>`) by DECLARATION identity alone and read no
bound; only the blanket form (`blanket_applies`, D-111) ever had its bounds
consulted. The checker accepted `Box<Point>.cmp(…)` with `Point`
implementing nothing, the emitter named a method nobody defined, and `llc`
refused the module — an accepted program the compiler could not compile,
D-064 §1's promise broken on the instantiation side. The prelude's two
bounded family impls (`TextWriter<W: Writer>`, `LineBufWriter<BW: Writer>`)
never met an unsatisfying argument, so nothing noticed for a year. **The
decision.** A family impl APPLIES to an instance only when its target
pattern unifies with the instance positionally — a pattern argument that is
one of the impl's own parameters binds the instance's argument and every
bound of that parameter must hold for it (`type_implements`, fuel-bounded
as `blanket_applies` is), a parameter bound twice must bind ONE type
(`Pair<int32, int64>` does not match `Pair<T, T>`), a concrete pattern
argument must be identical, and a nested instance or container unifies
element-wise — and an impl that does not apply is NOT a candidate, in
method lookup and in bound satisfaction alike (`family_applies`). **It is
decided where the impl is used, never eagerly**: an instantiation is never
refused for an impl it does not reach, so `Box<flt64>` under a derived
`Eq` and `Ord` is a fine type until something calls `cmp` — which is what
makes one derived impl per trait, each with its own bound, the right shape
rather than one impl carrying the union. The struct's OWN bounds stay
eager (`check_one_instance`, as since 0.4.7). **The report** is at the call
site under TYPE-017: the impl the call reaches, the parameter and the bound
it fails — and, when the impl carries `DECL_DERIVED`, the derive that wrote
it ("only through the `Ord` that `#[derive(Ord)]` generates for `Box`") —
one report for one mistake (D-240). A bound met through `type_implements`
elsewhere (a function bound, a `dyn` coercion) keeps `bound_unmet`'s
wording. The emitter is untouched: it mirrors the frontend's match and is
reached only for what the frontend admitted.

> **LANDED (1.5.2b step 1, 2026-09-05).** One unifier (`family_unify`,
> `type_trait.npk`) answers three questions — does the impl apply, what does
> each parameter stand for, which bound failed — for `find_method`,
> `type_implements`, `bind_blanket`, the `dyn`-coercion lookup and the
> emitter's `note_family_instance`. The measurement widened the step: nothing
> restricted a family impl's target shape, `bind_blanket` and the emitter bound
> the parameters POSITIONALLY, and `Pair<U, T>`, `Pair<int32, T>` and
> `Box<Pair<T, int32>>` were admitted at their declaration and refused at
> every call by the receiver check with a message about a type positional
> binding had invented — no miscompile was reachable, but the plan's "no
> backend change" did not survive it. `MethodFind` carries five fields, not
> four (`unmet_arg`: what the failing parameter bound to, which a call site
> cannot read positionally). The earlier-impl note is pushed by all three
> overlap reporters, not `coherence_error` alone. The frontend unit files'
> diagnostic counts count errors only, a note being a pointer.

## D-257 — the prelude implements the derivable traits for every scalar it can name, as a GENERATED region — **SETTLED (user decision, 2026-09-05; 1.5.2b S-31, S-34, S-36 and D-253's three; lands at 1.5.2b step 2)**

D-253 said "generated from the width ladder as the `Hash` impls are"; the
`Hash` impls are hand-listed, which is exactly why they stop at 64 bits
(`int128`…`uint4096`, `tbb128/256`, `tfp`, `frac`, the ternary four, the
floats and the flags have none). About two hundred one-line impls is where
a hand list loses a width. **The decision.** `bootstrap/generator/gen_tables.py`
writes them between `// --- scalar-impls:begin` and `// --- scalar-impls:end`
in `src/prelude/prelude.npk` — the flags region's mechanism (D-230) — from
the `BuiltinType` production it already reads, classifying EVERY terminal
into a scalar family or the named non-scalar list, and hard-failing on one
it cannot place (the walkers-total shape at the generator). The families
and what each gets: `Eq` (`self == other`) for every scalar but `flt128`
(a storage format, D-143); `Ord` and `PartialOrd` (three-way by `<`/`>`;
`pass Ordering.Less` wraps into `Ordering?` by D-099) for the ORDERED kinds
exactly as `type_is_ordered` says — the twenty `int`/`uint` widths, the
three `char`s, `tfp`, the ternary four, `frac` — and `PartialOrd` ALONE for
`flt32`/`flt64`, with the `nan` clause first (`if (!(self == self)) { pass
NIL; }`, both operands): a float has no `Ord` because a total `cmp` over
one would have to lie, the reason the trait's own comment gives; `tbb`
(codes, not sorted), `bool`, the kernel identifiers, the flag families and
`complex` get no order, as `why_not_ordered` already says; `Clone` (`pass
self`) for every scalar, `flt128` included; `Debug` as the scalar's
`ToString` (`pass (raw self.to_string())`) for every scalar that has one —
one meaning, and the generator reads the `impl:X:ToString` targets out of
the prelude text so the two sets cannot drift; `Hash` for the mechanical
rest of the ladder — the wide ints, `tbb128/256`, the ternary four, the
flags, by the existing rows' rule (the value truncated to 64 bits: equal
values hash equal, collisions allowed) — and NOT for the floats (`-0.0 ==
0.0` must hash equal, which needs a bits primitive and a rule), `tfp`,
`frac` or `complex` (a canonical-ERR rule): a program that hashes those
writes the impl and says what it means. `complex` per instance (the four
`ToString` lists); `dim256` for every `unit:` the prelude declares and the
seven base units, if `impl:dim256<Meters>:Eq` resolves as a target
(measured at the step; otherwise decided out and a `dim256` member compares
by operator, the one recorded exception to D-258). A twisted or exact
family compares through the operator, so its `eq`/`cmp` on an ERR operand
TRAPS (D-008 §5 as amended at 1.3.2) — the language's rule, inherited,
REACH arming it as at any operator site. Every generated body is `never
fails` (the `Hash` precedent; the trait declarations stay may-fail).
**`string`** implements `Eq` (`string_eq`), `Ord` (byte-lexicographic — the
shorter prefix is `Less`; the one order a byte string has without a
locale) and `PartialOrd`, hand-written beside its `Clone` and `Hash`: no
program can supply `string: Eq` for everyone, and without it `Box<string>`
could never derive `Eq`. Also generated: `src/frontend/scalar_table.npk`
(`builtin_scalar_family(TokenKind)`), so the derive reader (D-258) and the
region are two consumers of ONE classification; `gen_tables.py --check`
regenerates in memory and names the first file that would change, and the
harness's `check_generated_current` runs it (until now "run the generator
and `git status` clean" was a habit, not a check). **A program's own impl
of a pair the prelude covers** is the coherence violation it always was
(TYPE-013), and `coherence_error` now adds a NOTE at the earlier impl's
span so the walk prints its FILE (`prelude.npk:N`); a pair the prelude does
not cover (`impl:bool:Ord`) stays admitted — whether an orphan rule should
close that is OPEN_DECISIONS S-37, for the library era.

> **LANDED (1.5.2b step 2, 2026-09-05).** 348 rows in thirteen families;
> `Debug` exactly where a `ToString` row exists (which is how the generator
> learned `frac` has one); `dim256` per DISTINCT NON-ZERO unit vector, not per
> name — the first generation refused itself with TYPE-013, unit identity
> being the vector: `Hertz`/`Becquerels`, `Grays`/`Sieverts`, `Candela`/
> `Lumens` one type each, `Radians`/`Steradians` the zero vector, which is
> `tfp256` and already had rows; the generator evaluates the unit algebra and
> the compiler checks the mirror on every build (25 vectors, 100 rows). Found
> on both sides of the compiler: the method-call dispatch for `tfp`/`dim256`,
> the ternary family, `complex` and `simd` refused EVERY name outside each
> type's fixed set — in the checker before any impl lookup and in the emitter,
> whose `emit_tfp_method` lowered `self.to_string()` as `.floor()` and handed
> `llc` an `i32` where a string envelope was expected — so the prelude's rows
> could exist and never be called; both now send a name outside the type's own
> set to the impl table, one predicate per kind in `types.npk`. Measured: the
> compiler's own IR grows 2.2% (every prelude impl body is emitted whether
> reached or not — the emitter's reachability question, recorded), the
> frontend over `src/npkc.npk` takes 14% longer (348 more impls parsed and
> scanned per compile). `string`'s three are `never fails`, so a derived body
> reaches them with `raw`; `Clone` alone stays may-fail.

## D-258 — one rule for every member of a derived body, and the synthesized bound on exactly the parameters the body reaches — **SETTLED (user decision, 2026-09-05; 1.5.2b S-32, S-33 and D-253's method form; amends D-250 clause 2, D-161's no-bound story and D-123's `Clone`/`Debug` bodies; lands at 1.5.2b step 3)**

Planning measured the no-bound story's end (OPEN_DECISIONS DEF-16: a
derived `Eq` over `Box<T>` writes `!=` on an opaque `T`, the checker admits
it, and the emitter dies with EMIT-002 at `Box<Point>`), the D-250 class
beyond comparisons (DEF-17: derived `Hash` over a NAMED field, derived
`Clone` over an OWNING field, derived `Hash`/`ToString`/`Debug` over a
generic subject — every one a refusal at a line nobody wrote), a soundness
hole (DEF-18: `impl:<T>:Box<T>:Clone` is `pass self` checked with `T`
opaque, so `Box<string>.clone()` aliases one body under two headers and the
second drop frees it twice — the allocator's instrument caught it), and
D-250's operator form answering `Equal` for `nan` under a derived
`partial_cmp` over a float field. **The rule.** *A derived body reaches
every member through the trait it is deriving — `eq`, `cmp`, `partial_cmp`,
`hash`, `to_string`, `debug`, `clone` — a builtin SCALAR through the
PRELUDE's impl (D-257; `raw`, the region's bodies being `never fails`), a
NAMED type or a PARAMETER of the subject through its own (`relay`, the
trait declaring may-fail); a `simd` by `.any()` under `Eq` and DERIVE-005
under the rest (D-194, unchanged); a POINTER by `!=` under `Eq` (address
identity) and by copy under `Clone`, refused by name under the rest; and a
member no derived body can be written for — the owning builtins D-250
lists, `List`, `dyn`, an array or slice, `Optional`, `Result`, `cstring`,
`any`, `range`, `func`, `NIL`, `complex` over a parameter — refuses at the
user's declaration by name (DERIVE-006, one message per derive).* A
`string` FIELD is a named spelling (the prelude gives it all four, D-257);
a `string` PAYLOAD stays refused, because a `pick` cannot bind an owning
payload (D-216's is the only binding form; D-250 rule 3) — two mechanisms,
not one meaning varying by context. The rule replaces the operator for
scalars, which puts the float `nan` answer and the twisted ERR trap in ONE
place, and makes a derived `Ord` over a `bool`, a kernel identifier or a
float a refusal instead of a lie or a `<derived-N>` type error (`bool` has
no `cmp` to reach; D-259 re-homes the report). **The bound.** A derived
trait X over a generic subject writes `impl:<P1: X, P2, …>:Subject<P1, P2,
…>:X` with `: X` on EXACTLY the parameters that occur in a member the body
reaches through X: a named node that IS a parameter names it, a named node
WITH type arguments is walked into them (`Box<T>` as a member needs
`Box<T>: Eq`, whose impl needs `T: Eq` — conservative, and the checker will
ask), a pointer or a `simd` stops the walk; a parameter no reached member
mentions gets no bound (`Tagged<T> = { int32:id; }` derives `Eq` for every
`T`), and an enum's tag-only `Hash` (D-123) reaches no member. The subject's
own bounds are not copied (its instantiation checks them). **`Debug`**
reaches a named or parameter member through `debug`, bound `T: Debug` —
the trait derived is the trait reached — where today a named field under a
derived `Debug` renders through its `ToString`; the scalars' generated
`Debug` (D-257) is what keeps `Box<int32>` deriving it. **`Clone`** is
member-wise: a struct is `pass Subject{ f: <clone of f>, … }` (every field
listed; the BARE literal for a generic subject, since `Subject<T>{ … }` does
not parse — 1.3.7), an enum with payloads a `pick (self)` per variant
rebuilding it (`(Tn.V(dv0_0)) { pass Tn.V(relay dv0_0.clone()); }`; an
owning payload refused as under `Eq`), a payload-less enum `pass self` byte
for byte; `<clone of f>` by the rule above — `raw`/`relay` `.clone()`, a
copy for a pointer, a `simd` or an array of scalars, DERIVE-006 for an
array of anything else and for an owning builtin that is not `string`. The
bound `T: Clone` is what turns `Box<string>.clone()` into a second body;
a struct with a `string` field derives `Clone` from this step on. **Costs
named**: a call per member at `-O0` (inlined at `-O2` by the harness's opt
leg; the `cost` stage's allocations do not move); a nested named field's
rendering under a derived `Debug` changes (unpinned by any test); a
derived `Ord` over a float field, admitted today by operator, is refused.
`derive_payload.npk` keeps 121; payload-less enums' bodies stay
byte-identical.

> **LANDED (1.5.2b step 3, 2026-09-05).** `dv_class` classes every member
> spelling through `builtin_scalar_family` (the one classification the region
> is written from) and `dv_lic` gives the licence word; `dv_head` synthesizes
> the bound from the members each derive reaches; `Clone` is member-wise;
> `Debug` hoists each member's `debug` into a local and interpolates it. Found
> and fixed: the emitter's `emit_tostring` matched an impl by the RECORDED type
> over per-type impls alone, so interpolating a bound parameter (`func:show<T:
> ToString>`'s `&{v}`) or any value whose `ToString` is a family impl was
> admitted by the checker and refused by the emitter (EMIT-002); it substitutes
> through the specialization and asks `impl_decl_for` now. Found and RECORDED
> (OPEN_DECISIONS DEF-20, the user's decision): a generic enum parses and
> means nothing, so the plan's `Opt<T>` derive test is dropped with the note
> and the generator is written for the form. Derived `ToString`/`Debug` over a
> payload enum keep the `(V)` arm form (measured admitted).

> **1.5.2c step 1 (2026-09-05).** The generic-enum derive test 1.5.2b dropped
> (OPEN_DECISIONS DEF-20) returned with D-261: `derive_generic.npk`'s `Opt<T>`
> section derives `Eq`, `Ord`, `PartialOrd` and `Clone` over `Opt<int32>` and an
> owning `Opt<string>`, the generator's bare variant patterns and constructors
> reading the payload with the instance's arguments bound.

## D-259 — a derived diagnostic is reported at the derive; a `<derived-` path fails a unit in both runners — **SETTLED (user decision, 2026-09-05; 1.5.2b S-35; closes D-250's recorded gap; lands at 1.5.2b step 4)**

D-250 recorded "the one gap left": a named user type that turns out to
own, or to lack the trait, still reports inside `<derived-N>`, a file the
user cannot open; D-258 leaves the same residue (a builtin with no prelude
impl under the derived trait, a user type without it). **The decision.**
The generator emits every line through one seam that records `(line,
subject, member, trait)` into `ModuleGraph.origins`, and `front_run`, on
every return path, RE-HOMES every diagnostic whose span lies in a
`<derived-N>` synthetic file: its span becomes the subject declaration's
(the span DERIVE-006 already uses), its message is prefixed with the derive
and the member ("in the `Ord` that `#[derive(Ord)]` generates for `Box` (at
the field `v`): "), its code and severity are unchanged, so D-237's set
equality is unaffected. No diagnostic may then name `<derived-`, and BOTH
runners' finding parsers refuse one by name as a belt ("a line nobody
wrote"), each with a self-check case in the DIRECTION of the defect
(1.5.2's lesson): the parser handed a canned `<derived-1>:6:12` finding must
report, the same finding at a real path must not. Why re-homing rather than
a wider generator refusal: the residue is a checker verdict with a good
message that only needs the right span and the derive named; a table in the
generator of what the checker will say is the two-places-that-must-agree
shape. The `<extern-N>` files of `bridge_stubs.npk` may adopt the seam
later; recorded as owed, not done.

> **LANDED (1.5.2b step 4, 2026-09-05).** Every generated line goes through
> `dv_put`, which records a `DerivedOrigin` per line (subject, trait, member)
> into `ModuleGraph.origins`; the struct `Clone` literal is written one field
> per line so a refusal names the field; `front_run` is a wrapper that runs
> `rehome_derived` on every return path of the pass sequence; a parse error in
> generated text re-homes too (the step's first run proved it on a field named
> `on`, a keyword). The belt is `parse_findings` (harness) and `Finding.derived`
> (npkg), with `derived-path`/`derived-path-control` in both self-checks by
> name. The first re-homed residues showed TWO codes for one mistake — TYPE-019
> and TYPE-042, the `raw` licence asked about a callee that never resolved —
> so D-240's contract-only rule ("a refused operand answered with its own
> sentence") now holds for every `raw`; six frontend unit cases that had
> counted the pair since the 1.1.2 flip count one.

## D-260 — a `pick` does not select on an `Optional` — **SETTLED (user decision, 2026-09-05: "lets go with your recommendations on those"; OPEN_DECISIONS DEF-19; lands at 1.5.2c step 0)**

Found by 1.5.2b step 2's probes: `pick (o) { (Ordering.Less) { … }, (*) { … } }`
with `o: Ordering?` was admitted by the checker and refused by the emitter
(EMIT-002, "the emitter could not lower this, although the frontend accepted
it"). The checker judges a `pick` by its SELECTOR's kind — a frac and a
complex are refused there by name (D-198, D-199) — and never types a value
pattern's expression against the selector (the emitter's `pattern_const` and
the exhaustiveness pass read the arms), so an `Optional` selector had no arm
and fell through to a switch the emitter cannot build over `{ i8, T }`. **The
decision.** A `pick`'s selector may not be an `Optional`, in the statement
form and in the expression form: refused by name, TYPE-065
(`TYPE_PICK_OPTIONAL`), with the two spellings the language has — the value
is reached through `??` (`pick (o ?? default) { … }`, which is what the suite
writes) or tested with `== NIL`. An `Optional` has no arms of its own (D-099:
no constructor, no readable members, one implicit wrap); admitting the bare
form with a mandatory `(NIL)` arm would be a second binding form for a type
whose one form is `??` — two spellings for one thing, the cost the blueprint
rule exists to avoid — and the emitter would owe it a lowering it has never
had. Not a soundness defect (the compiler refused, it did not miscompile);
the recommendation and the ratification are the small language.

> **LANDED (1.5.2c step 0, 2026-09-05).** `TYPE_PICK_OPTIONAL` is
> `NITPICK-TYPE-065`, reported at the selector in the statement form and the
> expression form before the arms are read (`tests/types/rejection/
> pick_optional.npk`: both forms refused, the `??` form accepted). At step 1 the
> refusal moved into `type_pick_rules`, the ONE function both spellings of a
> `pick` call (see D-261's note) — the rule is written once.

## D-261 — a generic enum is a family, exactly as a generic struct is — **SETTLED (user decision, 2026-09-05: "lets go with your recommendations on those"; OPEN_DECISIONS DEF-20; lands at 1.5.2c step 1)**

Found by 1.5.2b step 3's tests: `enum:Opt<T> = { Some(T); None; };` parsed
and meant nothing. The parser gives an enum the item generics window a struct
has, and no reader of a variant's payload type bound it — the layout, the
pattern bindings, the constructor's payload checks, the emitter's payload
slots and the drop bodies all resolved the payload node in the enum's home
scope with no parameters bound ("there is no type named `T`", TYPE-001) —
and a variant constructor or a bare variant reference yielded the BARE
declaration type ("found `Opt`", TYPE-007). No generic enum existed in
`tests/`, `src/`, `lib/`, `npkg/` or `tools/`, and none in TYPE_REFERENCE,
TRAITS_REFERENCE or AST_REFERENCE: the form had never been decided in or
out, D-085's "parses and means nothing" shape. **The decision.** Generic
enums are IN. `Opt<T>` is a template and `Opt<int32>` an instance — D-090's
identity with its arguments, its bounds judged by `check_one_instance` as a
struct's are, its header emitted by the instance walk that already admits
the kind — and EVERY read of a variant's payload type binds the instance's
arguments to the declaration's parameters through the one binding a struct's
field walk already uses (`bind_instance`, `type_layout.npk`): the layout, per
instance, so `Opt<string>` owns and `Opt<int32>` does not; the pattern
bindings; the constructor's payload checks; and the emitter's payload slots,
which construction, `pick` binding and the drop body read from one function
(`variant_payload_slot_at`) — in a generic body the enum type is substituted
through the specialization first, as `ll_type` substitutes it. **The
instance of a constructor or a bare variant reference** (`Opt.Some(3i32)`,
`Opt.None`) is the EXPECTED type when that is an instance of the same
declaration — an annotated binding, a parameter, a return, a payload, a
field — else INFERRED from the payload arguments by the generic call's own
unifier (`unify_into`: `Some(T)` against an `int32` argument teaches `T`,
`Some(Box<T>)` against `Box<int32>` teaches it too, and an argument that
takes its type from context teaches nothing, as for a call), and a parameter
neither supplies is `TYPE_CANNOT_INFER` naming it with the advice to
annotate the binding — the rule a generic function call has had since D-108,
applied to the one other generic value a program builds. A payload-less
variant of a generic enum (`Opt.None`) therefore needs the expected type,
which its common spelling gives (`Opt<int32>:o = Opt.None;`); in pattern
position a bare variant is read against the selector and is never typed as a
value. A non-generic enum binds an empty window, so no existing program
changes. Derive over a generic enum works as D-258's generator already writes
it (bare variant patterns and constructors under the impl's parameter bound);
the test 1.5.2b dropped returns. Why in and not out: an enum with a payload of
a parameter type is the shape every `Optional`/`Result`-like user type takes,
the struct machinery it needs is small and already written, and deciding it
out would mean refusing the window on an enum by name for no reason a reader
could be given.

> **LANDED (1.5.2c step 1, 2026-09-05).** As written: `bind_instance` is `pub`
> and binds a struct's OR an enum's window; the layout's enum arm, the pattern
> bindings (`type_enum_bindings`), the constructor's payload checks
> (`check_ctor_args`) and the emitter's `variant_payload_slot_at` read every
> payload type under it; `enum_instance_for` (`type_members.npk`) supplies a
> constructor's or a bare variant's instance — expected, else inferred through
> `unify_into`, else `NITPICK-TYPE-022` naming the parameter — and builds it
> through `make_instance` (the tail `subst_instance` always had, factored) so
> `check_instantiations` judges it; the emitter substitutes a generic body's
> enum type at `emit_ctor` and through `pick_sel_tid` at every `pick` reader.
> Tests: `generic_enum.npk`, `generic_enum_infer.npk`, `derive_generic.npk`'s
> returned section, `type_generic.npk` ge1..ge3. **Found on the way, fixed in
> the step:** the `pick` EXPRESSION form typed no arm binding — only the
> statement form called the binding typer and the lending-form refusal — so a
> bound payload's member read was accepted unchecked and died as EMIT-002 (on a
> PLAIN enum), a struct given where an `int32` was expected passed the fit
> check against nothing, and an owning payload could be copied out of a lending
> pick expression (1.4.3b's hole, open in one of the two spellings). The
> statement form's whole prelude is ONE function now, `type_pick_rules`, called
> by both forms (`pick_expr_bindings.npk` pins the four rules that became live).
> Also fixed: two `pick` binding sites in `ir_stmt.npk` resolved the payload
> node by hand (a `T` unbound) and now read `variant_payload_slot`. The
> compiler, `npkg` and the tools check clean under the shared rules;
> `nitpick.obligations` did not move.

## D-262 — an unreferenced prelude item is not emitted, and the frontend's per-program cost is bounded by the program, not by the prelude — **SETTLED (user decision, 2026-09-05: "lets go with your recommendation"; OPEN_DECISIONS S-38; lands at 1.5.2d)**

Found by the library workbench an hour after the 1.5.2c close, measured on the
two pinned compilers with the same inputs: a 14-line program that does nothing
but `exit 0i32` with a `failsafe` emits 845,282 bytes of IR where it emitted
456,517 at the 1.5.1b close, in 0.85 s where it took 0.10 s, peaking at 102 MB
where it peaked at 21 MB — a delta constant to the byte across 22 of 30 library
programs, because D-257's generated scalar impls (348 rows in thirteen families)
are emitted into every program whether reached or not, and every program pays
for typing them. 1.5.2b measured the compiler's own build (+2.2% IR, +14%
frontend) and never the fixed per-program cost, which is what a harness
compiling many small programs pays. **The measurement the recommendation asked
for** (on the same probe, this tree): the FRONTEND holds 0.72 s of the 0.82 s
and emission 0.10 s; 587 of the 608 functions in the emitted IR are prelude
bodies; and a profile puts the frontend's time not in the prelude's size as such
but in three scaling defects — the bindings analysis (definite assignment and
the moved-from state) allocating one state slot per STATEMENT AND DECLARATION
OF THE WHOLE PROGRAM for every function and copying that state at every branch
(57% of the run), the type table deduplicating by a linear scan of every type
(13%), and the string interner deduplicating by a linear scan with a string
comparison at every identifier the lexer meets (inside the lexer's 12%). Parsing
the prelude is 16% and typing it 16%.

**The decision, in two parts.** (1) **An unreferenced prelude item is not
emitted.** The emitter brackets each non-generic PRELUDE function and each
prelude impl's vtable as an ITEM of the module text; when the module is
complete, an item is kept only if a symbol it defines is referenced by the
text outside every item or by a kept item — a fixpoint over the emitted IR
itself, decided on the artifact and never by an AST walk that would have to
enumerate every kind of reference (a direct call, a method call, a vtable slot,
a function value, a spawn, a drop body, an interpolation's `to_string`) and
would fail open on the kind it missed; the textual rule fails closed, since a
reference is a token, and it is deterministic. Everything outside the prelude
is emitted as before, so a program's own IR does not change; generic prelude
instances are already emitted on demand and stay roots. (2) **The frontend's
per-program cost is bounded by the program's own size**, which the three
defects violate as engineering, not as design: the bindings analysis numbers
each function's locals and parameters densely at resolution (a per-function
ordinal, recorded in the symbol table) and sizes its state by that count; the
type table and the string interner each carry a hash index over their keys.
No verdict, type or name changes — the fixes are measured by every existing
test reporting exactly what it did and the compiler's own IR staying
byte-identical. **Out, by decision:** a checked-once (precompiled) prelude. The
remaining per-compile prelude cost after the fixes is parsing and typing it,
measured at the landing and recorded; if that residue is ever the bound that
matters, it is a new question with its own row, not this one. The workbench's
canary is `nitpick-time/tests/probe/probe11d_floor_only.npk`, 845,282 bytes of
IR at `0dfddac`; the landing notice carries its after-value.

> **LANDED (1.5.2d, 2026-09-05; steps 1, 2, 2b and 3, each under a full
> harness).** §2 first: `SymbolTable.local_ord`/`stmt_ord`/`fn_slots` written
> by the resolver (`Resolver.local_next`), `state_new` sized by the function,
> `symbol_slot` answering the ordinal; hash indexes on `TypeTable` and
> `InternTable`. Measured on the floor-only probe: the frontend 0.72 s → 0.07 s
> and 95.7 MB → 4.4 MB, the whole compile 0.82 s → 0.145 s; the compiler's own
> check 210 s → 22 s; its build under the cost stage allocated 355,927,682
> bytes with a 112,913,630 peak in 20.0 s where it had allocated
> 13,669,309,722 with a 13,428,902,379 peak in 241.8 s -- the 13 GB was the
> bindings analysis copying the whole program's slot space. No verdict moved
> (153 rejection files identical under both checkers) and the IR of every
> program that compiles no changed source is byte-identical. §1 next: the
> writer's items and `irw_trim_items`, the emitter's brackets (each non-generic
> prelude function, each prelude impl's methods and vtable, each prelude
> generic instance), the trim run last; the probe's IR 845,283 → 50,561 bytes,
> 608 → 14 functions, 587 → 0 prelude bodies, `llc` 0.46 s → 0.02 s; the
> compiler's own IR keeps 101 of 685 prelude functions, every one referenced.
> Found and fixed on the way: the first prelude function's item straddled the
> head-to-tail switch; the prelude's generic instances are recorded by call
> sites in dropped bodies and are items too; `src/frontend/prelude.npk`, a
> compiler module named `prelude`, shared the language prelude's qualifier and
> is `prelude_names.npk` now; the elision cross-check counts a row only for a
> function the emission holds. The belts `check_prelude_trimmed` and
> `ir_prelude_trimmed` in both runners, with self-check cases. Step 2b, found
> by the workbench meanwhile (DEF-21): the undefined-symbol allowlist is the
> runtime's EXPORTS. `nitpick.obligations` never moved. The workbench's canary
> reads about 50 KB after the landing.

## D-263 — the prelude's `List<T>` stores through the managed heap's untracked entry; D-151 keeps counting every `wild` block — **SETTLED (user decision, 2026-09-05: "i am fine with your recommendation"; OPEN_DECISIONS S-39; lands at 1.5.2e step 1)**

Found at 1.5.2d's close writing `generic_move_out.npk`: an owning `List<T>`
local alive in `main` at `exit 0` is reported by D-151 as `WildLeak` (exit 94),
under the 1.5.2c-close compiler as well; every `List` test in the tree kept its
list inside a function that RETURNS, which is why it stayed latent. Two settled
decisions meet: D-183's amendment keeps the scope-drop walk off the `exit` path
("walking the entire live program state to free it on the way out adds failure
modes to exactly the path that exists to have none"; 1.4.4's rider — `exit`
runs joins and defers and nothing else), and D-151 counts every WILD allocation
alive at `exit 0`. A `List<T>` is MANAGED by D-247 — owning, move-only, dropped
at scope exit — but its buffer was spelled `wild` in the prelude (`alloc` in
`list_init`/`list_reserve`), making it the one managed value D-151 counted; a
string leaked the same way is invisible, since the managed heap is not the
tracked one. **The decision.** The prelude's `List<T>` allocates its buffer
through the managed heap's untracked entry — the floor's `@npk_alloc_managed`,
the same entry the emitter's own managed cells and a channel's ring use (D-183
1.2.5b: managed storage the kernel reclaims at exit and D-151 never counted) —
spelled in the prelude as the builtin `alloc_managed`, and `ralloc` keeps a
block's role (the floor's `npk_ralloc` allocates the moved block with the old
block's role, measured), so a grown list stays managed. The exit path stays
free of the drop walk as D-183 decided; a `List` in `main` at `exit 0` is what
every other managed value already is. **Scoped.** `alloc_managed` is the
PRELUDE's own: a call from any other module refuses (TYPE-054, the builtin's
own call rule, generated from the reference's `**Prelude-only**` marker),
because D-151's count IS the enforcement a hand-written `wild` container relies
on — the library workbench's `Vec<T>` chose `wild` so an unpaired free traps
at exit, and nobody should "fix" a `Vec` that traps. Not decided here, and
recorded as its own question if it ever matters: drops with SEMANTICS at a
normal `exit` (a buffered writer's flush, an `OwnedFd`'s close) — today they
do not run, exactly as at a crash, and the kernel closes the descriptors.

> **LANDED (1.5.2e step 1, 2026-09-05).** As written: the `alloc_managed` row
> (`**Prelude-only**`, generated into `builtin_prelude_only`; the hand-written
> declare in `emit_runtime_declares` retired for the table's), `type_call`'s
> refusal from any module but the prelude (TYPE-054), `list_init` and
> `list_reserve` on it, `ralloc` untouched (the floor's move paths allocate
> with the old block's role, measured). `list_in_main.npk`: a `List<string>`
> grown past its capacity and a `Holder<string>` alive in `main` at `exit 0`
> exit 0 where every compiler before exited 94; `prelude_only_builtin.npk`:
> the refusal. The compiler, `npkg` and the tools use no `alloc_managed`
> outside the prelude; every List, derive, dyn and generic program keeps its
> exit; `nitpick.obligations` did not move.

## D-264 — a bare type parameter is move-only in the body that names it — **SETTLED (user decision, 2026-09-05: "the recommendation for the new decision sounds fine to me. lets ratify it"; OPEN_DECISIONS S-40; lands at 1.5.2f step 1)**

Found by the library workbench (`nitpick-time`, its O-N19) on every pin:
inside a GENERIC body the move-only rule (D-183, TYPE-046) was never asked of
a bare type parameter. `require_move_if_owning` asked `type_drops`, which
answers false for an unsubstituted `T`, so `T:x = s.items[i]` at an owning `T`
compiled, linked and ran with two owners of one heap body — the workbench's
probe drops the first and reads the second, exit 170, the allocator's 0xAA
poison — while the same statement with `string` written out was refused. Not a
regression; 1.5.2d step 4 only made the consequence RUNNABLE (before it the
same program stopped at `llc`). The workbench's own `vec_pop<T>` shipped this
shape, reviewed and verified, because every gate a library owns is a leak gate
and a leak gate cannot see a managed body. **The decision.** A bare type
parameter — and `Self` in a trait's default body — is MOVE-ONLY in the body
that names it: a generic body is checked ONCE for every type it is instantiated
at, some of which own storage, so the only sound answer for the type it does
not know is "owns". A copy of a `T` place is spelled `move(...)`, a plain copy
at a scalar, or `.clone()` under a `Clone` bound — the same at every
instantiation. This is D-183 applied to the one type a template cannot see, not
a new rule; it changes what the checker accepts, which is why it is a decision.
**Consequences.** A lending `pick` may not bind a payload of a bare `T` (the
D-216 rule, the same question); the derive generator refuses a `T` payload
under `Eq`, `Ord`, `PartialOrd` and `Clone` exactly as it refuses a `string`
payload (DERIVE-006) — the generated lending `pick` over `Opt<T>`'s payload,
admitted since 1.5.2c, was this hole in generated form — while `Hash`,
`ToString` and `Debug` bind nothing and still generate for a generic enum, and
a parameter FIELD read in place derives fine. Measured before the rule was
written: the compiler, `npkg`, the tools and every suite but five backend
programs and `lib/ntensor.npk` refuse nothing new; the seven sites are all a
by-value `T:v` parameter STORED into an owning slot — a constructor payload, an
element, a field, a channel send, two matrix cells — the second-owner hazard
itself, each spelled `move T:v` and `move(v)` now. What this leaves open, S-41:
a BORROWING `pick` binding form, which would let a generic enum with payloads
derive the four again and let a program compare two `Opt<string>`s without
consuming one.

> **LANDED (1.5.2f step 1, 2026-09-05).** `type_owns_for_move`
> (`type_expr.npk`): drops, or `TY_PARAM`, or `TY_SELF`; `require_move_if_owning`
> and the lending-pick binding check ask it; the parameter case has its own
> TYPE-046 text. `dv_refusal` refuses a `DVC_PARAM` payload under the four
> binding traits (DERIVE-006). The seven sites respelled `move T:v`/`move(v)`;
> `derive_generic.npk`'s `Opt<T>` section derives `Hash`/`ToString`/`Debug`.
> Tests: `generic_owning_copy.npk` (TYPE-046 ×3), `derive_generic_payload.npk`
> (DERIVE-006 ×2), `generic_owning_move.npk` (exit 0 at `string` and `int32`).
> Zero new refusals anywhere else, measured before and after.


## D-265 — the toolchain pin is a version; the emission is the cross-machine identity claim — **SETTLED (user decision, 2026-09-06: "lets go with your recommendation on S-42 and ratify it"; OPEN_DECISIONS S-42; lands at 1.5.2g)**

Found by the library workbench's first CI run (`nitpick-time`, 2026-09-06):
the pinned compiler commit `aaffb87` built on GitHub's runner gave a
`build/npkc` of `3c05818c…` where this machine gives `a3b0dadc…`, while
`build/npkrt.o` is `c9ddbcff…` on both. Nothing the compiler claimed is
contradicted — BUILD_REFERENCE §5 says the same inputs, the tools included,
give the same bytes, and D-204's pin is the VERSION 20.1.2 asked of the
tools — but the sentence had been read as a claim about builds of one commit
anywhere, and a version is not a binary: two builds of 20.1.2 (this
machine's Ubuntu `1:20.1.2-0ubuntu1~24.04.3`, the runner's apt source)
differ in distribution patches and configure-time defaults, and `repro`
measures one machine (working directory, `llc` twice, absolute site rows).
**The decision.** (1) **The pin stays a version.** A tool-binary digest in
`[toolchain]` would refuse every machine but one, for a property that belongs
to the toolchain and not to the compiler; the LLVM tools' OUTPUT is what the
checks hold — `repro`, the fixpoint, the zero-dependency scan on the object,
and the translation validation the 1.6 plan names — and the evidence campaign
attaches to the emitted IR (D-233), not to the object. This is the deliberate
asymmetry with z3, whose binary IS digest-pinned (D-218.1): the solver's
output is a VERDICT the tree commits and must re-decide identically, so its
identity must be the binary's; the toolchain's output is checked bytes. (2)
**The compiler's emission is the cross-machine identity claim.**
`build/npkc.ll` — same source, same committed snapshot — is the same text on
any machine (D-078, D-236: no path, time, host or environment value reaches
it), and a difference THERE, between any two machines, is a compiler defect to
report with the two files. The object's and the binary's identity is per
toolchain build. (3) **The ladder reports its digests.** Every `npkg` ladder
run — `build`, `test`, `verify` alike — prints the SHA-256 (`lib/nhash.npk`,
the project's one hash) and byte count of each intermediate it produced
(`builder.o`, `builder`, `npkrt.o`, `npkc.ll`, `npkc.o`, `npkc`, and
`npkc.opt.ll` when `[build] opt-level` is 2), so the first digest that differs
between two machines names the stage; the harness's `parity` stage
cross-checks the printed lines against an independent SHA-256 of the same
files, so the report cannot name the wrong file. (4) **Every pin notice
carries the emission's digest** beside the binary's; the emission's is the
one a consumer on another machine can expect to reproduce. (5) **The first
cross-machine comparison of `npkc.ll`** — the workbench's runner against this
machine — is the measurement that decides whether a compiler item exists at
all; nothing is opened on the binary's difference alone. Mechanics at 1.5.2g.

> **LANDED (1.5.2g step 1, 2026-09-06).** `ladder_digests`/`digest_line`
> (`npkg/build.npk`), printed by `main.npk` after the `builder` stage line on
> every run; `check_ladder_digests` in the harness's `parity` stage over
> `LADDER_INTERMEDIATES`. Measured: the six lines equal `sha256sum`'s digests
> and `stat`'s counts; the check passes on the real output and fails a
> renamed line twice and an empty output six times. The workbench re-pinned
> to `3d15ac9` against the six digests the same night and found the floor
> series flat (14 defines at `aaffb87` and at `3d15ac9`).

## D-266 — a lending `pick` binds views; the selector is frozen while a view is live — **SETTLED (user decision, 2026-09-06: "yes, ratify it as stated with the frozen-selector rule"; OPEN_DECISIONS S-41; lands at 1.5.2h)**

The question D-264 left. A `pick` binds a payload to a name in its arm, and
the language had two forms: the lending `pick (v)`, whose binding was a
bitwise COPY of the payload — refused outright when the payload owns
(TYPE-046, 1.4.3b: a copy is a second owner of one body) — and the consuming
`pick (move(v))` (D-216), whose binding OWNS. Under D-264 a bare `T` counts as
owning, so a generic enum with a payload could not bind it without consuming
the value, no program could compare two `Opt<string>`s without destroying one,
and the derive generator — whose `Eq`, `Ord`, `PartialOrd` and `Clone` bodies
are lending picks over both operands — refused those four over a `T` payload
(DERIVE-006) as it always had over a `string`'s. Measured before the decision
(2026-09-06, `0ba21ef`): the compiler, `lib/`, `npkg/` and `tools/` hold 17
lending picks and not one binds a payload; the suites hold 433, 27 of which
bind (34 names, every one copyable; none assigned, addressed or moved; 14
passed or given); the library workbench 122 and 8 (32 names); no pick in any
tree writes its selector inside its own arms. The borrow machinery exists —
`@x`, `$$i x`, `$$m x` are parsed, typed and tracked by the escape, bindings
and suspend analyses — and BORROW-005 (a borrow across an `await`) has never
been emitted: the suspend walk makes an address-taken local frame-resident to
the function's end instead, so that crossing is made safe, not refused.

**The decision.** (1) **A lending `pick` binds VIEWS.** `pick (v)` — the
selector not spelled `move(v)` — binds each pattern name, enum payload or
destructured field, as a read-only view of the payload IN PLACE, in the
selector's own storage: nothing is copied at the bind, nothing is consumed,
the selector stays live and owns what it owned. The consuming form keeps its
meaning: its bindings own. The selector's spelling says which; no new syntax.
(2) **A view is typed as its payload and read by value**: a scalar's value, a
field, an element, a plain by-value argument (D-065's lend), interpolation,
`give`/`pass` of a copyable value. A view of an OWNING type is a place of an
owning type, so a copy of it is TYPE-046 as everywhere (`.clone()` under a
bound is the copy), and `move(x)` of a view is TYPE-047 (the value was lent by
the `pick`, not given — the non-`move` parameter's sibling). (3) **A view is
read-only and has no address.** The language has one pointer type and it
carries no mutability, so every address of a view is a write path into a lent
value: an assignment or compound assignment to the view or any part of it,
`@`, `$$i`, `$$m`, `.destroy()`, and the IMPLICIT address a method or UFCS
call takes for a pointer-typed receiver are refused, TYPE-066. A by-value
receiver borrows the bits as any plain argument does. (4) **The selector is
frozen while a view of it is live.** Inside an arm that binds at least one
name — its guard and its body — no write may reach the selector's ROOT
binding (through views: `pick (s.kind)` freezes `s`, a view's selector's root
is the outer selector's root): an assignment to it or any part, a `move` of
it or any part, `@`/`$$i`/`$$m` of it or any part, `.destroy()`, a
pointer-receiver call on it, a nested `pick (move(…))` over it — TYPE-067. An
arm that binds nothing (`_`, or a payload-less variant) lends nothing and is
not frozen. A selector with no root — a temporary — has nothing to freeze and
lives to its statement's end (D-246), which contains the arms. (5) **A view
across an `await` is sound by residency**, exactly as `@x` is: the suspend
walk extends the selector's root to the function's end when an arm binds, a
temporary selector's spill is a frame slot, and the binding's slot — which
holds the payload's ADDRESS, in a sync body and a coroutine alike — is
frame-resident where the arm's copies were. BORROW-005 stays as it is. (6)
**The derive generator's lending picks are sound as written**: `Eq`, `Ord`,
`PartialOrd` and `Clone` generate over `string` and bare-`T` payloads (the
D-250 and D-264 refusals for those two classes lift; DERIVE-006's other
classes stand). (7) **One fact, recorded once**: the resolver links a pattern
symbol to its pick's selector, and "is a view" is derived from the selector's
spelling by one predicate the checker, the analyses and the emitter read.

**What this is not.** A read-only pointer type is not proposed: a view's
address is refused, not postponed. Views read by value, and the four derives
and every measured use need no address; a method that must write a payload in
place is written against the consuming form or a by-value receiver.

**Found by planning — DEF-24.** TYPE-063 (D-251) refused `@`, `$$i` and `$$m`
on a limited binding but not the implicit address a method call takes for a
pointer-typed receiver: `drop p.bump();` with `bump = NIL(Pt->:p)` writing
`p.x = -5` under `Rules<Pt>:r_px = { $.x > 0i32 }` compiles, runs, violates
the rule and traps nothing (exit 7), while `drop bump(@p);` is refused at the
`@`. Fixed at 1.5.2h step 0 — the site that then refuses a view its address
(rule 3) is the same site. Mechanics at 1.5.2h.

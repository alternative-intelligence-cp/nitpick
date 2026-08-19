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

## D-002 — FFI must map C failures into `Result.error` — **SETTLED**

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

## D-004 — Escape rule for `@local` — **SETTLED**

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
| `int32`, `uint64`, `flt64`, … | **trap to `failsafe`** | ordinary code, parsing, setup, tooling |

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
| `ok(x)` | `T` | explicitly clears the taint; caller assumes responsibility |

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
int32:val = read_file() ?! 99i32;   // on error: failsafe(99i32)
```

`?!` triggers `failsafe` on a failed unwrap, and `failsafe` has the fixed
signature `func:failsafe = int32(tbb32:err)` — exactly one argument. `?!`
therefore takes exactly one, and it is the error code handed to `failsafe`.
The argument is typed `tbb32` to match.

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
| **Status / error** | `mem_free = int64(int64:ptr)` | `NIL`, with failure carried in `Result.error` |

Parameters take the same treatment: `mem_free = int64(int64:ptr)` becomes
`mem_free = NIL(wild int8->:ptr)`.

### The status category is a double encoding

Worth calling out separately. Every function already returns `Result<T>`
implicitly — the declared type is the *success* type. So a function whose `int64`
return value **is** a status code encodes failure twice: once in `Result.error`
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

## D-014 — `defer` does not run on a trap; `failsafe` requirements — **SETTLED**

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
   Negative values are reserved for system errors by the `Result.error`
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

## D-017 — Arenas and threads: two types, one discipline each — **SETTLED**

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
wild int8->:page = #wild_ptr<int8->>(addr);
```

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

## D-028 — `assoc` declares associated types; `Type` is namespace-only — **SETTLED**

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

## D-031 — `impl` syntax: no connector, type first — **SETTLED**

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

## D-035 — `wildx` is retained; the verification boundary is stated, not removed — **SETTLED**

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
| no manifest present | one is generated; the build is marked *not reproducibility-verified* |
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
failure goes to `Result.error` and never reaches the `fd` type at all. The
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
| `err_from_syscall` | — | **delete** — the builtin already produces `Result.error`; converting a negative return by hand is exactly the double-encoding D-012 objected to |

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
| **`to_cstring(s)`** for a runtime `string` | runtime — interior NUL is `Result.error` | one scan |

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

## D-055 — GPU and GUI run out of process; `#[gpu_kernel]` is a codegen target, not a call — **SETTLED**

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

## D-060 — Nitpick is statement-oriented; the expression forms are a closed list — **SETTLED**

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

## D-065 — `move` is an operator, not a memory qualifier — **SETTLED**

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

## D-066 — `opaque struct:Name;` is the one form, and is `extern`-only — **SETTLED**

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

---

## D-069 — `Result` stores the error once; `is_error` becomes derived — **SETTLED**

Settles the D-005 follow-up: *"how `tbb32`'s sticky ERR state is encoded within
the 4-byte error field. A bare `i32` either loses the ERR encoding or implies an
undocumented reserved bit pattern."*

### There is no undocumented bit pattern

The premise was wrong in a useful way. `tbb32`'s ERR sentinel **is** a documented
bit pattern — `INT32_MIN`, per `TYPE_REFERENCE.md` §6 — so storing the field as
`i32` loses nothing. The real question is what that value *means* when it appears
in `Result.error`, which nothing answered.

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
invariant relating them.** Nothing says what `{error: 0, is_error: true}` or
`{error: 5, is_error: false}` mean, nothing rejects them, and both are
constructible today through the explicit-literal form the spec documents:
`return Result{error: errCode, value: retVal, is_error: true};`.

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

`r.is_error` **remains valid source** as a derived accessor for `r.error != 0i32`,
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
  `pass(v)` → `Result{value: v, error: 0i32}`; `fail(e)` → `Result{value: zero,
  error: e}` with `e` checked non-zero and non-ERR.
- **`fail(0i32)` is rejected** at compile time where the code is a literal, and
  traps where it is computed. A failure with no code is the same unidentifiable
  error as an ERR code, arrived at from the other direction.
- **Returning a value *and* an error remains expressible** —
  `Result{value: retVal, error: errCode}` — since that never depended on the
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
second-class borrows of it. They cannot outlive that scope, which is the same rule
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
| `Thread.sleep_ns/ms` | reimplemented — the prototype's are `pass NIL;` with no syscall, so every sleeping loop is a spin loop (audit §2) |
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
kernel identifiers are distinct types, and **POSIX's `-1` goes to `Result.error`
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
> unchanged and carry forward.

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
| `raw` / `_!` | bypasses the `Result` discipline entirely — the one escape hatch the language has |
| `?!` | escalates a **recoverable** error into whole-program shutdown |
| `?` with a default | substitutes a value for an error — the D-002 failure mode, silent success |

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
  `Result{ value: zero, error: <the same code, verbatim> }`.
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
func:apply = int32(func int32(int32):f, int32:x) { pass raw f(x); };
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

## D-089 — `main` takes `cstring[]:argv` and nothing else; the declaration-site `_~` is restored — **SETTLED**

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
| `pass(retVal);` | `return Result{error: 0tbb32, value: retVal};` |
| `fail(errCode);` | `return Result{error: errCode, value: zero};` |
| `return Result{error: e, value: v};` | (literal, no desugar) |

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

## D-114 — An `extern` function has a type, and it is a `Result<T>` like every other

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

- **A caller writes `raw`, `relay`, `drop` or `?` on a foreign call, exactly as on
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
language-surface decision, and it has to be settled before Phase B, because
`nlibc` and every syscall wrapper are `extern` declarations.

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

## D-117 — A borrow does not need a second-class parameter; the caller tracks what comes back

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

## D-127 — Something writes past its own allocation, and the seed's rounding hides it — **OPEN, scheduled**

Two defects, found together in cycle 0.6.2, and the second is the one that matters.

### The read

`npk_ralloc` copies the **new** size out of the **old** block:

```llvm
define ptr @npk_ralloc(ptr %old, i64 %n) {
  %p = call ptr @npk_alloc(i64 %n)
  call void @llvm.memcpy.p0.p0.i64(ptr %p, ptr %old, i64 %n, i1 false)
```

Every array growth in the compiler therefore reads past the end of the block being
grown — twice the block's length, since growth doubles. It has never faulted
because a bump allocator over 1 MiB chunks usually has something mapped after the
block; it faults the first time the block sits near a chunk's end.

### The write, which the fix for the read uncovered

Giving every allocation a size header so `ralloc` can copy the smaller of the two
sizes **breaks the seed**. The bisect is exact:

| variant | result |
|---|---|
| pad every allocation by 16 bytes, return the same pointer | passes |
| move the returned pointer 16 bytes forward, write nothing | passes |
| write the header only when a new chunk is mmap'd | passes |
| write 8 bytes at the start of each bump-allocated block | **segfaults** |

Moving every address is harmless. **Writing into the 16-byte rounding gap is not.**
The only reading of that is that something writes past the end of its own
allocation and lands in the gap the allocator's round-up-to-16 leaves behind, and
has been getting away with it for as long as the gap has been dead space.

It kills exactly three tests — `tests/frontend/type_cast.npk`, `type_expr.npk` and
`type_result.npk` — and the visible symptom is an `ExprTypes` whose `items` pointer
reads back as null, which is a symptom of the corruption rather than its location.

### Why this is not "a throwaway-seed problem"

The seed is throwaway (D-085) and the runtime goes with it. The **writer** may not
be: the seed compiles this compiler, so the out-of-bounds write is either in the
code the seed emits or in `src/` itself, and only one of those disappears with the
seed. Until it is found, which it is cannot be stated.

An out-of-bounds write is also precisely what the Astrée run exists to find, and
finding it there rather than here would spend the one attempt on it.

### What unblocks it

A **poisoning allocator**: fill the rounding gap with a known byte pattern at
allocation and verify it at every later allocation. That names the writer at the
moment it happens instead of leaving a bisect to infer it from a crash three
allocations later. It is a change to `npk_alloc` alone, it needs no source changes,
and it can be run under the existing harness.

Scheduled before Phase A closes. **Neither defect is fixed yet** — the read's fix
is blocked on the write, because the header is what exposes it.

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

## D-129 — Eight expression kinds are never typed, and the checker says nothing — **OPEN, scheduled 0.6.7**

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

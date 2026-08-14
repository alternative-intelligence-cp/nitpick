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
- **Still open:** how `tbb32`'s sticky ERR state is encoded within the 4-byte
  error field. `PRE_PLANNING_REVIEW.md` §2.5 raised this and D-005 does not
  resolve it — a bare `i32` either loses the ERR encoding or implies an
  undocumented reserved bit pattern. Needs settling before `Result` lowering is
  implemented.

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
`fail`, and `exit` — never on a trap.

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

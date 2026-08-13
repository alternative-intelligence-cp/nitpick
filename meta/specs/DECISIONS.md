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

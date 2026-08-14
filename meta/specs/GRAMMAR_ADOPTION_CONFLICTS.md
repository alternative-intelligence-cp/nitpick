# Grammar Chapter Adoption — Conflict List

Full read of `FORMAL_DRAFT` chapters **01 (Lexical)**, **04 (Expressions)**, and
**05 (Statements & Control Flow)** against D-001…D-021 and the carried-over
reference specs.

> **Correction.** `FORMAL_DRAFT_AUDIT.md` §6 recommended adopting these three
> "immediately — no conflicts found". That assessment came from a structural skim,
> not a full read, and it was **wrong**. The chapters carry ~25 conflicts. Four are
> semantic disagreements with the existing reference specs that require a decision;
> the rest are mechanical.

---

## Part A — Semantic conflicts requiring a decision — **ALL RESOLVED**

These were not stale text. Two documents described **different languages**, and one
of each pair had to be chosen.

| | Question | Resolution |
|---|---|---|
| **A1** | `till` / `loop` semantics | **D-022** — both are counted. `till(limit, step)` ascends from 0; `loop(start, limit, step)` infers direction from the bounds. **`step` is always positive.** `FORMAL_DRAFT` 05's do-while/infinite reading is struck. |
| **A2** | `prove` compile-time or runtime | **Compile-time Z3 obligation.** `FORMAL_DRAFT` 05 §5.8 is the outlier and is struck. |
| **A3** | C-style `for` | **D-023** — range form only, with a required typed binding. |
| **A4** | raw / multi-line strings | **D-024** — both retained. Chapter 01's omission described a prototype parser gap, not a design decision. |

### A1. `till` and `loop` mean completely different things

| | `CONTROL_REFERENCE.md` §2.4 | `FORMAL_DRAFT` 05 §5.4.3–5.4.4 |
|---|---|---|
| `loop` | **counted iteration** — `loop(start, limit, step)`, counter exposed as `$` | **infinite loop** — `loop { }`, exited via `break` |
| `till` | **counted iteration from 0** — `till(limit, step)`, counter as `$` | **do-while** — condition evaluated *after* the body |

Both spellings cannot be right. This affects the parser directly and it changes
what `$` means.

### A2. `prove` — compile-time proof or runtime assertion?

- `VERIFICATION_REFERENCE.md` §1.2 and `FORMAL_DRAFT` 12.5.1: `prove` is a
  **compile-time Z3 proof obligation**, path-condition aware; failure to prove
  **rejects compilation** with a counterexample.
- `FORMAL_DRAFT` 05 §5.8: "**`prove(cond);`**: A **runtime** assertion. If the
  condition is false, the program panics immediately."

These are different features. The verification chapters are far more detailed and
internally consistent, which argues they are correct — but a runtime-checked
variant may also be wanted, under a different name.

### A3. C-style `for` loops

`FORMAL_DRAFT` 05 §5.4.2 shows both `for (int32:i = 0; i < 10; i++)` and
`for (i in 0..10)`. `CONTROL_REFERENCE.md` §2.3 shows only the range form, and
with a **typed** binding: `for (int64:i in 1..3)`.

Two sub-questions: is the C-style three-clause form supported at all, and does the
range form require an explicit type on the loop variable?

### A4. Raw and multi-line string literals

- `OP_REFERENCE.md` §9 lists `r"..."` (raw) and `"""..."""` (triple-quote,
  multi-line) as **current** operators.
- `FORMAL_DRAFT` 01 §1.6.3: "While older versions of Nitpick supported raw strings
  and multi-line strings, the current compiler (v0.61.82) parser throws a syntax
  error if these are encountered. They have been **intentionally omitted** from
  this grammar."

---

## Part B — Conflicts with settled decisions (mechanical)

### Chapter 01 — Lexical Analysis

| # | Location | Issue | Fix |
|---|---|---|---|
| 1 | §1.4 `MemoryQualifier` | includes `"gc"` | remove (D-003) |
| 2 | §1.5 `Operator` | `=>` present, **`=>!` missing entirely** | add — `=>!` is one of only two cast forms (D-021) |
| 3 | §1.5 `Operator` | `#` listed among value operators | reclassify as the compiler-directive sigil (D-020) |
| 4 | §1.6.1 `SentinelLiteral` | includes `"unknown"` as a writable literal | `unknown` is compiler-assigned only (`TYPE_REFERENCE` §27) |
| 5 | §1.4 `BuiltinType` | `fix256` | rename to `dim256` (`SPEC_GAPS` §3 — rename still owed project-wide) |
| 6 | §1.4 `BuiltinType` | `tbb8`…`tbb64` only | add `tbb128`, `tbb256` (`TYPE_REFERENCE` §6) |
| 7 | §1.4 keywords | no `fails`, `on`, `never` | required by the FFI error contracts (D-002) |
| 8 | §1.4 `BuiltinHelper` | no `is_err` | required by `tbb` ERR checking (D-008) |
| 9 | §1.6.2 vs `FORMAL_DRAFT` 02 §2.4 | ternary literal is suffix-form here (`01Tt` + `"t"`), prefix-form there (`0t1T0`) | **SETTLED — same conflict as 47: suffix form wins**, uniform with hex/binary/octal |

### Chapter 04 — Expressions

| # | Location | Issue | Fix |
|---|---|---|---|
| 10 | §4.2 precedence 1 | `->` listed as a **postfix member access** operator | remove — `->` is type-position only (D-006) |
| 11 | §4.5 | "**`->` (Dereferencing Member Access)**: Dereferences a pointer and accesses the member field" | strike — `.` handles all member access and auto-dereferences (D-006) |
| 12 | §4.2 precedence 4, §4.5 | `#` as a unary **pin** operator on GC objects | strike (D-020) |
| 13 | §4.2 precedence 3 | cast level lists `=>` only | add `=>!` (D-021) |
| 14 | §4.5 | "`@` … yielding a typed pointer (`T->`)" | `@` yields a **second-class borrow**, not a first-class pointer (D-004) |
| 15 | §4.4.1 | "**`result ?! `**" — shown with **no argument** | takes exactly one `tbb32` error code (D-009) |
| 16 | §4.6 | `sys!!!` documented | removed (D-001) |
| 17 | §4.4 | "TBB (**Three-State Bit**)" | TBB is **Twisted Balanced Binary** |
| 18 | §4.1 | "chained assignments (`a = b = 5`)" | **SETTLED by D-060.** Assignment is a **statement**; `a = b = 5` does not parse. 05 §5.3.1's rule rejecting `=` in conditions becomes unnecessary — `if (a = b)` is no longer expressible |

### Chapter 05 — Statements and Control Flow

| # | Location | Issue | Fix |
|---|---|---|---|
| 19 | §5.9 | "`defer` … executed when the scope exits (whether normally, by a `return`, or **via a panic**)" | **`defer` does not run on a trap** (D-014) |
| 20 | §5.7 | "`pass expr;` … sugar for `return ok(expr);`" and "`fail expr;` … `return err(expr);`" | `ok()` is the taint-clearing builtin, not a `Result` constructor; `err()` is undefined. `pass`/`fail` construct `Result` directly (`TYPE_REFERENCE` §11.2) |
| 21 | §5.6 | `pick` arms shown **without** separating commas | `CONTROL_REFERENCE` §1.2 requires commas between arms |
| 22 | §5.6.3 | `(!)` "unreachable" marker introduced | **SETTLED by D-061: removed.** It would let the D-008 required `ERR:` arm be elided — asserting a `tbb` cannot be ERR, the least safe assumption in the type. Write the arm with `#unreachable()`, which traps |
| 23 | §5.6.3 | `give` yields a value out of a `pick` | **SETTLED by D-059: kept.** No contradiction with §5.1 — a `pick` whose arms `give` **is** an expression (D-060 enumerates the expression forms) |
| 24 | §5.1 vs 04 §4.1 | "statements do not yield values" vs "almost every construct is an expression" | **SETTLED by D-060.** 05 §5.1 is authoritative and 04 §4.1 is struck; Nitpick is statement-oriented with a **closed list** of expression forms |
| 25 | §5.8 | `prove` — see **A2** | **SETTLED per A2:** compile-time Z3 obligation; 05 §5.8's runtime-assertion reading is struck |

---

## Part C — What these chapters add that is genuinely new and wanted

Not everything is a conflict. These are worth keeping:

- **Full W3C-EBNF token grammar** (01) — the parser foundation, and nothing in the
  carried-over set covers it.
- **Complete operator precedence table** (04 §4.2), 18 levels — likewise absent.
- **Loop labels** (05 §5.5) — `outer: loop { … break outer; }` for breaking out of
  nested loops.
- **`pick` destructuring** (05 §5.6.1) for enums and structs.
- **Template literal tokenization** (01 §1.6.4) — `TEMPLATE_START` / `TEMPLATE_PART`
  / `INTERP_START` / `INTERP_END` / `TEMPLATE_END`.
- **Numeric literal bases** (01 §1.6.2) — suffix-form hex/binary/octal/ternary/nonary
  with `_` separators.
- **`if` rejects assignment in conditions** (05 §5.3.1) and **stray-`else` detection**
  (§5.3.2) — both good pedantry, consistent with the language's posture.
- **`?|` / `defaults`** scoped expression fallback (04 §4.4.2).

---

## Recommended sequence

1. Decide **A1–A4** — they change what the parser must accept.
2. Apply the mechanical fixes in Part B during adoption.
3. Fold the Part C additions into the existing reference docs, or adopt the
   chapters wholesale and retire the overlapping sections of
   `CONTROL_REFERENCE.md` and `OP_REFERENCE.md`.

Note that chapter 05 overlaps `CONTROL_REFERENCE.md` substantially, and chapter 04
overlaps `OP_REFERENCE.md`. Adoption is a **merge**, not an addition — which is
why the A-list has to be settled first.

---

# Chapter 13 (Traits & Generics) + Chapter 06 (Functions) — Conflict List

Read in full, together, because they overlap and disagree.

## Part D — Design problems requiring a decision

### D1. `Type` means two different things

Same syntax, two unrelated constructs, disambiguated only by where it appears:

| Context | Meaning | Source |
|---|---|---|
| top level | **namespace / module grouping** — `Type:Counter = { … };`, holds functions and structs, no member variables | `FORMAL_DRAFT` 02 §2.7.4 |
| inside `trait` / `impl` | **associated type** — `Type:Item;` declares, `Type:Item = int32;` binds | `FORMAL_DRAFT` 13 §13.2.3 |

This is a direct blueprint violation — a construct changing meaning by context is
the exact thing the philosophy exists to prevent. It is also genuinely ambiguous
to parse: inside a trait body, `Type:Foo = { … };` could be either an associated
type bound to an anonymous struct, or a nested namespace.

### D2. Traits are combined with `&` in some places and `+` in others

| Purpose | Symbol | Source |
|---|---|---|
| supertrait requirement | `&` — `trait:Ordered = Equatable & { … };` | 13 §13.2.2 |
| generic bound | `&` — `T: Renderable & Serializable` | 06 §6.3 |
| multi-bound `dyn` | **`+`** — `dyn Drawable + Serializable:obj` | 13 §13.5.3 |

Three places meaning "this type satisfies several traits", two different symbols.

## Part E — Conflicts with settled decisions (mechanical)

| # | Location | Issue | Fix |
|---|---|---|---|
| 26 | 13 §13.2.5 | `@derive(Default, Eq, …)` | `#[derive(…)]` — `@` is address-of only, and derive annotates a declaration, so it takes the attribute form (D-020) |
| 27 | 13 §13.3.3 | arena access shown as `ptr->node_arena.alloc(…)` | `.` handles all member access and auto-dereferences (D-006) |
| 28 | 13 §13.6.1–13.6.2 | `@cast<T>` / `@cast_unchecked<T>` presented as equal alternatives to `=>` | both removed; `=>` and `=>!` are the only cast forms (D-021) |
| 29 | 13 §13.6.1 | destructive casts "emit warnings" (`NITPICK-062`, `NITPICK-063`) | `=>` is a **compile-time error** where loss is possible; `=>!` is the opt-out (D-021) |
| 30 | 13 §13.6.3 | "use `int64` to track opaque C addresses rather than casting to typed pointers" | struck — `int64` addresses defeat leak checking and escape analysis (D-012); use `#wild_ptr<T>(addr)` (D-019) |
| 31 | 06 §6.1, §6.6 | examples use `return a + b;` / `return bytesRead;` | `pass` / `fail` are the return keywords |
| 32 | 06 §6.1, §6.3 | `void` used as a Nitpick return type | `void` is `extern`-only; use `NIL` |
| 33 | 06 §6.1.2 | "`extern` … return bare types (`T`) … not wrapped in `Result<T>`" | all functions including `extern` return `Result<T>` with mandatory error contracts (D-002) |
| 34 | 06 §6.4 | lambdas/closures capturing environments via `npk_gc_alloc` | closures removed (D-018) |

## Part F — Chapter 06 and 13 disagree with each other

| Construct | Chapter 06 | Chapter 13 |
|---|---|---|
| trait declaration | `trait:Reader { … };` — **no `=`** | `trait:Serializable = { … };` |
| impl | `impl Reader for FileStream { … }` — space-separated | `impl:Serializable:for:Message = { … };` — colon-separated |
| generic parameters | **before** the name, with bounds: `func<T: Renderable & Serializable>:process = void(T:item)` | **after** the name, no bounds shown: `func:extract_value<T> = T(Container<T>:c)` |

Chapter 13's forms match the house style used everywhere else — `func:name = `,
`struct:name = `, `Rules<T>:name = ` — and `SPEC_GAPS` §3 independently specifies
the after-the-name placement (`struct:Name<T> = { … }`, `func:my_func<T> = …`).
**Chapter 06 is the outlier on all three.**

Note chapter 13 never shows a *bounded* generic, so wherever the bound syntax
lands it has to be written, not copied.

## Part G — Gaps

- **`Self` is not a keyword.** Used six times in chapter 13 (`func:to_bytes = buffer(Self:self);`)
  but absent from the chapter 01 keyword list and from `LEXICAL_REFERENCE.md`.
- **`where` has two unrelated syntactic forms**: a `pick` arm guard,
  `MyMacro!(a, b) where (a > b)`, and a path component in blanket impls,
  `impl:Loggable:for:T:where:Printable`. Both express constraints, but one is a
  parenthesized expression and the other a colon-separated segment.
- **`>>` splitting**: `Handle<Node<int64>>` requires the lexer to split `>>`,
  which is also the right-shift operator. A known parser interaction that needs
  stating explicitly rather than being left to the implementer.

## Part H — What chapter 13 adds that is wanted

Default methods, supertraits, associated types, inherent impls, derive macros,
blanket impls, `opaque` types, coherence (at most one impl per trait/type pair),
object-safety rules for `dyn`, monomorphization, and multi-bound `dyn` with
widening. None of this exists in the carried-over set, and traits are referenced
by `TYPE_REFERENCE.md` §18 with no governing document.

Also confirms **UFCS** independently: 13 §13.2.4 states inherent methods are
"dispatched statically via UFCS: `Point_magnitude(p)`" (D-006).

---

# Chapter 11 (Concurrency) — Conflict and Gap List

Chapter 11 is **82 lines** and predates D-003. Most of what it needs is not
correction but content that does not exist yet: it was written when a garbage
collector answered every ownership question, and it answers none of them now.

## Part I — Conflicts with settled decisions

| # | Location | Issue | Fix |
|---|---|---|---|
| 35 | §11.3 | threading "encapsulated within the standard library (`stdlib/concurrent`) interfacing directly with **`nitpick-libc`**" | `nitpick-libc` is a musl tree. The native library is **`nlibc`** — which currently has **no thread module** at all (`syscall`, `mem`, `str`, `io`, `proc`, `fs`, `time`, `math`). Threading lives in the archived `nthread` (3 files) and `nsync` (5 files). |
| 36 | §11.4.1 | `atomic_from_ptr<int32>(header_ptr + 24i64)` — raw pointer arithmetic on an integer offset | pointer arithmetic goes through `#ptr_add<T>(ptr, offset)` (`FORMAL_DRAFT` 08 §8.1.1, corrected for D-020); constructing a pointer from an integer requires `#wild_ptr<T>(addr)` in `wild` context (D-019) |

## Part J — Gaps: things D-003 broke that were never revisited

### J1. `atomic_new` heap-allocates with no owner

```nitpick
atomic<int32>:counter = atomic_new(0i32);
```

Chapter 11 §11.4.1 describes this as "heap allocation". With no collector, **who
frees it?** The chapter does not say, because when it was written nothing had to.

**Recommendation: remove `atomic_new` and keep only `atomic_from_ptr<T>`.**
The aliasing form already exists precisely to place an atomic inside memory that
something else owns — a struct field, an arena slot. Removing the allocating form
eliminates the ownership question rather than answering it, adds no new
allocation path to verify, and leaves one way to obtain an atomic instead of two.

### J2. Coroutine frames have no owner either

`async` lowers to `@llvm.coro` state machines (`FORMAL_DRAFT` 07 §7.4), whose
frames are heap-allocated so they can survive suspension. Same question, same
silence.

**Recommendation: coroutine frames are allocated from a runtime-owned arena**,
released when the task completes. Task completion is a well-defined free point,
arenas are already the mechanism for batch-lifetime data (D-003), and it keeps
the async runtime's memory deterministic rather than introducing a second
discipline.

### J3. Nothing connects threads to the memory model

The chapter says nothing about:

- **Borrows and threads.** D-004 already forbids a borrow from crossing a thread
  spawn or an `await` point. That *is* the data-race-freedom story — it
  eliminates shared stack references structurally, at compile time — and it is
  absent from the document that should state it.
- **Arenas and threads.** D-017 splits `arena<T>` (single-threaded, full
  operations) from `shared_arena<T>` (allocation-only, non-moving chunks, atomic
  bump). Chapter 11 predates it entirely.
- **`--verify-concurrency`** was listed in `VERIFICATION_REFERENCE.md` §5 as
  verifying "data race & deadlock freedom" with **no mechanism described
  anywhere** for either. **Both now have one.** Race freedom follows from D-004,
  D-017 and D-032; deadlock is split by D-056 into lock-order freedom, proven
  from a compile-time `LEVEL` carried by every blocking primitive, plus mandatory
  deadlines containing what the analysis cannot reach. The flag's documented
  claim is narrowed accordingly.

### J4. `Future<T>` is never mentioned — **settled by D-058**

`TYPE_REFERENCE.md` §17 defines it as `{ coroutine_handle, result_slot }`.
Chapter 11 contains **zero** occurrences. The relationship between `async
func`, `await`, and `Future<T>` is unspecified — in particular whether `Future<T>`
is ever user-visible or purely an internal lowering artifact.

It is **purely internal**: `await f()` yields `T` and `drop work()` discards, so
no surface construct produces one. D-062 draws the consequence — with no way to
name a task there is nothing a cancellation operation could take as an argument,
which is one of the three reasons the prototype's preemptive `Executor::cancel`
is not ported.

### J5. Task spawning was dropped — **settled by D-062**

The prototype's `concurrency_specs.txt` §1.3 states that a task may be spawned on
the runtime executor with `drop work();` — that is, calling an `async` function
without `await` and discarding the `Future`. Chapter 11 omits it, leaving **no
documented way to start a concurrent task at all**, only to await one.

`drop work()` is restored, with a lifetime the prototype never stated: **the task
cannot outlive the scope that spawned it.** Scope exit joins it under a mandatory
deadline, and expiry traps rather than detaching. That is what makes D-034's
frame arena correct — nesting lifetimes are what an arena is for.

## Part K — The consequence nobody has costed

§11.2 states the async runtime "multiplexes coroutines over a **configurable pool
of system threads**".

That means **a task can resume on a different OS thread than it suspended on.**
Which in turn means everything a task touches across an `await` must tolerate
cross-thread access — including `arena<T>`, whose D-017 contract says
*single-threaded*.

This is survivable but the contract needs restating. A work-stealing runtime
establishes a happens-before edge when it migrates a task, so **sequential**
access from different threads is safe; what is unsafe is **concurrent** access.
So `arena<T>`'s guarantee should be phrased as *no concurrent access* rather than
*same OS thread* — otherwise the rule as written is violated by any task holding
an arena across a suspension point, which is the normal case.

The alternative is **pinning tasks to threads**, which makes the original phrasing
true and removes the class of problems, at the cost of load balancing across
cores. For a language whose users include robotics, that trade may be worth
taking deliberately rather than inheriting work-stealing by default.

## Part L — What chapter 11 gets right

- **SeqCst enforcement** (§11.4.3) — independently confirms D-016, with the same
  justification (determinism for the physics engine and AGI substrate).
- **The restricted atomic method set** (§11.4.2) — `load`, `store`, `swap`,
  `fetch_add`, `fetch_sub`, `compare_exchange`, and nothing else.
- **No `spawn` / `go` keyword**, keeping the thread model out of the language core
  so bare-metal and embedded targets are not forced to carry it.
- **No `sync` keyword** — explicitly rejected by the compiler.
- **`await` restricted to `async func`** (`NITPICK-040`).
- `atomic<T>` methods use **UFCS** — another independent confirmation of D-006.

---

# Chapter 12 (Safety & Verification) — Conflict and Gap List

The most valuable chapter for the safety story, and the one most out of date with
respect to D-003, D-007, D-008, D-013, and D-014.

## Part M — Conflicts with settled decisions

| # | Location | Issue | Fix |
|---|---|---|---|
| 37 | §12.1, §12.3 | Layer 2 is "an `unknown` sentinel that gracefully degrades mathematically undefined operations (e.g. division by zero)" | **D-007** — behaviour is *type-directed*. `tbb` yields sticky ERR and keeps flowing; plain types trap to `failsafe`. `unknown` is narrowed to `Result.value` taint. |
| 38 | §12.4 | `failsafe` called "the un-bypassable **Layer-1** safety net" | it is **Layer 3**, per §12.1 four paragraphs earlier. Internal contradiction. |
| 39 | §12.4 | "Every Nitpick **program** must implement" | **D-013** — every **executable**; libraries must not define one. Exactly one per program, no chaining. |
| 40 | §12.4 | no mention of trap semantics or handler requirements | **D-014** — `defer` does not run on a trap; `failsafe` must not assume a healthy system and should use preallocated resources; it must exist, be non-empty, and return a **positive** value via a compiler-injected `ensures result > 0i32`. |
| 41 | §12.2 | "Functions that **might fail** must return a `Result<T>`" | **all** functions return `Result<T>` except `main` and `failsafe`. |
| 42 | §12.6.2 | "If a function defines a `requires` block, Nitpick **implicitly transforms** its return type into a `Result<T>`" | already universal — this phrasing implies conditional wrapping. |
| 43 | §12.7.2 | "All widenings must be cast explicitly using **`as`**" | **D-021** — `as` is the module-alias keyword. The cast forms are `=>` and `=>!`. |
| 44 | §12.5.4 | `--verify-concurrency` — "data race & **deadlock** freedom" | **SETTLED by D-056.** Race freedom via D-004/D-017/D-032; lock-order freedom proven via typed `LEVEL`s on every blocking primitive; residual deadlock contained by mandatory deadlines. The flag's claim is narrowed to what it proves |

## Part N — Gaps: Layer 2 is incomplete

### N1. `tbb` sticky ERR is absent entirely

D-008 makes it the **primary fail-operational mechanism** and D-007 makes it the
divide-by-zero answer for `tbb` types. It is a Layer 2 mechanism and chapter 12
does not mention it once.

### N2. Memory safety is absent

`--verify-memory` claims to verify "use-after-free & recursion bounds", but
nothing in the chapter describes the mechanisms that deliver it: static ownership
(D-003), second-class borrows (D-004), generation-counted `Handle<T>` and the
`arena<T>` / `shared_arena<T>` split (D-017), or the K-semantics `exit` rule that
makes a leak a *detected* condition.

### N3. The escape-hatch catalogue is incomplete

§12.7.4 covers `wild` / `wildx` only. Missing: `raw` / `_!`, `=>!`, and
`#wild_ptr<T>(addr)` (D-019). Also worth noting `sys!!!` and `asm!!!` were
escape hatches and are now removed (D-001).

### N4. `--extra-picky` needs two rules that follow from later decisions

- **require `tbb` arithmetic** in designated real-time code, making the
  fail-operational path a compile-time guarantee rather than a convention (D-007);
- **ban allocation inside `failsafe`**, partially enforcing the preallocation
  discipline (D-014).

### N5. Runtime constraint violation is unspecified — **settled by D-068**

§12.6.1 says that without `--verify`, `limit<Rules>` constraints are "enforced
dynamically at runtime" — but not what a runtime violation *does*. Presumably it
traps to `failsafe`; it should say so.

It does trap to `failsafe`, and `VERIFICATION_REFERENCE.md` §2 already said so.

The larger issue this exposed: §2 read as though the runtime check existed *only*
under `--verify`, which would mean `limit<Rules>` constrains nothing in a build
that omits the flag. **D-068 rejects that reading** — constraints are enforced in
every build, and `--verify` decides only whether a check is discharged statically
and therefore elided. A safety property must not depend on a compiler flag, and
the literal reading made the shipped binary the weakest one. The useful corollary
is that proving a constraint *removes* its runtime check, so verification pays for
itself in speed.

## Part O — What chapter 12 gets right, and adds

- **§12.8 dual verification backends** — Z3 for programs, K Framework / kprove for
  the language metatheory, with operational semantics in `k-semantics/nitpick.k`.
  The framing is worth keeping verbatim: *"Z3 ensures your program is correct.
  K ensures the language is correct."* Neither carried-over doc mentions this.
- **§12.7 the `--extra-picky` catalogue** — `literal-suffixes`,
  `explicit-widening`, `shadow`, `wild`, with `warn-`/`no-` parameterisation.
- **§12.5.1** `prove` path-condition accumulation and counterexample extraction.
- **§12.5.5** Z3 borrow-checker integration proving index disjointness — an
  independent argument for static ownership over a collector (D-003).
- **§12.1** names Nikola explicitly alongside robotics, medical devices,
  aerospace, and nuclear control as target applications.

---

# Chapter 02 (Types & Data Model) — Conflict and Gap List

## Part P — Conflicts

| # | Location | Issue | Fix |
|---|---|---|---|
| 45 | §2.3.3 | `FixedPointType ::= "fix256"` | **D-036** — renamed `dim256`; `fix256` is obsolete |
| 46 | §2.3.2 | `TFPType ::= "tfp32" \| "tfp64"` only | **D-036** — four widths exist: `tfp32`, `tfp64`, `tfp128`, `tfp256` (`TYPE_REFERENCE.md` §5) |
| 47 | §2.4 vs ch. 01 §1.6.2 | balanced literals use **prefix** form (`0t1T0`, `0n2A`) here, **suffix** form there (`1T0t`, `2An`) | suffix form wins — uniform with hex/binary/octal, which all use suffixes |
| 48 | §2.5 | `complex<T>` laid out as `{ *T:real; *T:imag; }` | `*` is C pointer syntax, reserved for `extern` only. Also real/imag are **values**, not pointers: `{ T:real; T:imag; }` |
| 49 | §2.7.1 | `StructType ::= "struct" \| "opaque" "struct"` | **settled by D-066** — `opaque struct:Name;` wins. The standalone form has zero prototype usage; the modifier form has a test directory, a negative test (`OPAQUE-COPY-001`), and a K `loadStructs` rule. `extern`-only, no value semantics. |
| 50 | §2.1 | "there are zero implicit **implicit** conversions" | typo |

## Part Q — `tryte` / `nyte` packing: chapter 02 is right, `TYPE_REFERENCE` is wrong

| | `FORMAL_DRAFT` 02 §2.4 | `TYPE_REFERENCE.md` §7 |
|---|---|---|
| `tryte` | **10 trits** (3¹⁰) in a `uint16` | 6 trits |
| `nyte` | **5 nits** (9⁵) in a `uint16` | "packed nits" |

Chapter 02 is correct and it is checkable: 3¹⁰ = 59,049 fits a `uint16` (65,536)
while 3¹¹ = 177,147 does not, so ten is the maximum. Likewise 9⁵ = 59,049 fits
and 9⁶ does not.

Note 3¹⁰ = 9⁵ = 59,049 exactly — `tryte` and `nyte` have **identical cardinality**,
which is presumably why both are `uint16`. Worth stating in the spec rather than
leaving as a coincidence a reader has to notice.

`TYPE_REFERENCE.md` §7 needs correcting.

## Part R — **Open question**: do LBIM integers carry sticky ERR?

§2.2.1 states that `int1024` … `int4096` (and unsigned counterparts) are Large
Binary Integer Math types for post-quantum cryptography, and:

> The `ERR` sentinel is defined as `0x8000...0000` set solely in the highest-order
> limb … **Operations on LBIM types implicitly propagate this sticky `ERR` state.**

**This conflicts with D-007.** Plain integer types trap; only `tbb` degrades. As
written, `int4096` would behave like a `tbb` while `int32` traps — and the `int`
prefix gives the reader no way to know that.

The blueprint problem is precise: D-036 established that the **"twisted" prefix
means "reserves a value as a sticky error state"** (`tbb`, `tfp`). A large integer
with a sticky ERR sentinel *is* a twisted type by that definition, but is not
named like one.

Three ways out:

1. **LBIM loses sticky ERR** — `int*`/`uint*` behave identically at every width,
   and anything wanting ERR semantics uses `tbb` at that width (extending D-008's
   table beyond `tbb256`). Most uniform; may not suit crypto, which often wants
   defined modular arithmetic rather than an error state.
2. **LBIM keeps sticky ERR under a twisted name** — `tbb1024`, `tbb4096`. Honest
   naming, but `tbb` is *balanced* (symmetric about zero), which is an odd fit for
   the unsigned moduli crypto actually uses.
3. **LBIM keeps sticky ERR as `int*`** — accept that `int` semantics vary by
   width, and document it loudly. Cheapest, and the one that violates the
   philosophy.

**This affects `ncrypto` (34,925 lines)**, so it should be settled before that is
ported.

## Part S — What chapter 02 confirms and adds

Confirms: `tbb` ranges and sentinel (D-008), the **Sentinel Discontinuity**
warning and `tbb_widen<T>()` intrinsic (D-008 §6), `atomic<T>` defaulting to
SeqCst (D-016), `void`/`NIL`/`NULL`/`ERR` roles (D-005), and `Type` as a
namespace construct (D-028).

Adds, with no counterpart elsewhere:

- **`simd<T, N>`** — power-of-two lane counts, enforced 16/32/64-byte alignment,
  and notably **ERR mapped to masked operations rather than branches**, so
  error states survive vectorisation without destroying throughput.
- **`frac8/16/32/64`** — exact rational fractions (whole, numerator, denominator).
- **Sub-byte integers** — `int1`, `int2`, `int4` and unsigned counterparts.
- **LBIM** — `int1024` … `int4096` as limb arrays for post-quantum work.

---

# Chapters 00b, 10, 14, 15 — Final Conflict List

Completes the `FORMAL_DRAFT` review. Chapters 00, 03, 06, 07, 08, and 09 were
catalogued in `FORMAL_DRAFT_AUDIT.md` §3–§4.

## Part T — Chapter 15 (Standard Library): the most out-of-date chapter

| # | Location | Issue | Fix |
|---|---|---|---|
| 51 | §15.1.1 | "Nitpick **deliberately omits a discrete `char` type**. Single-character literals evaluate directly to an `int8` or `uint8` scalar." | **Flatly contradicted.** `TYPE_REFERENCE.md` §2 defines `char8`/`char16`/`char32` as semantically distinct, with arithmetic and bitwise operations **rejected at compile time**. This is also the worked example behind D-005's governing principle — a character is not an integer at the semantic level. **Strike §15.1.1 entirely.** |
| 52 | §15.1.3 | "`int8->` is a **Fat Pointer** containing bounds metadata. `int8*` is a standard Thin Pointer" | conflicts with `TYPE_REFERENCE.md` §10, which states the LLVM IR for pointer kinds is **identical** and the distinction is enforced by the type checker. See **Part W** — this is a real open question, not stale text. |
| 53 | §15.1.3 | "`string` guarantees internal null-termination … `extern` signature **must** use `string`" | **SETTLED by D-049.** `string` is `{ptr, len, cap}` and is not NUL-terminated; native `string` is not passed to C. The resolution is the **`cstring`** type, not the interim `as_cstring → char8[]`, since a char array carries no termination guarantee. Recorded in `PROTOTYPE_DELTA.md` §4 |
| 54 | §15.1.2 | raw and multi-line strings "currently unsupported" | **D-024** — both retained; that note described a prototype parser gap |
| 55 | §15.2 | collections "managed via opaque `int64` handles" | **D-012** — `int64` handles defeat leak checking, escape analysis, and Z3 pointer reasoning. Use `Handle<T>` or a typed handle. |
| 56 | §15.2.1–15.2.4 | `apush`/`alset`/`ahset` return **`0` on success, `-1` on overflow**; `apop`/`alget`/`ahget` return the **`unknown` sentinel** on underflow | violates the universal `Result<T>` rule twice over, and `unknown` is narrowed to `Result.value` taint (`TYPE_REFERENCE.md` §27). These should return `Result<T>`. |

§15.2.5 (arenas and generational handles) is correct and consistent with D-017.

## Part U — Chapter 10 (ABI & Hardware)

| # | Location | Issue | Fix |
|---|---|---|---|
| 57 | §10.2.3, §10.4.2 | `sys!!!` and `asm!!!<T>` documented as raw tiers returning bare values | **removed** (D-001) |
| 58 | §10.3 | "`->` : Type Annotation **& Field Access** … in an expression context it dereferences a pointer and accesses a field" | **D-006** — `->` is type-position only; `.` handles all member access |
| 59 | §10.5.2 | "the runtime forcibly invokes **the C standard library `exit(code)`**" | ⚠️ **zero-dependency violation.** Process exit must go through `nlibc`'s syscall layer, not libc. |
| 60 | §14.2.1 (ch. 14) | `extern` functions "**do not return `Result<T>`**"; callers "assign them directly without `raw` or `?`" | **D-002** — all functions including `extern` return `Result<T>`, with mandatory `fails on` / `never fails` contracts |
| 61 | §14.2.1 | `?*` / `?->` for opaque pointers | `any->` (`TYPE_REFERENCE.md` §27) |
| 62 | §14.5.2 | `--extra-picky` valid rules list | add `require-tbb` (D-007) and `no-failsafe-alloc` (D-014) |

### Chapter 10 adds three things of real value

- **§10.6.1 Left-to-right evaluation order**, strictly enforced for all binary
  operations, function arguments, and assignment sequences. `foo() + bar()`
  guarantees `foo()` completes first. This eliminates a well-known class of C/C++
  undefined behaviour and **appears in no other document**.
- **§10.5.1 Symbol mangling** — generics as `Vec_int8`, trait methods as
  `Point_display`, drop glue as `T_drop`, base functions unmangled unless generic.
  Directly relevant to D-015's deferred symbol-binding question.
- **§10.5.2 Failsafe error codes** — `45` out-of-bounds, `46` null dereference,
  `50` `requires` violation, `51` `ensures` violation. A reserved numbering scheme
  that should be extended rather than reinvented.

## Part V — Chapter 14 (Modules & Build) additions

Mostly consistent. Adds, with no counterpart elsewhere:

- **`--seccomp`** — embeds a seccomp-bpf sandbox with a syscall allowlist into the
  binary. Directly relevant to Nikola's mini-VM isolation.
- **`--guard-pages`** — injects guard pages around `wild` allocations.
- **`--verify-nikos` / `--analyze`** — NIKOS abstract interpretation for
  division-by-zero, out-of-bounds, and deep logical errors.
- **`--emit-ptx`, `--target=gpu`, `--emit-wasm`** — GPU and WebAssembly targets.
- **`-test` with `#[test]`** — synthesised test-runner `main`.

## Part W — **Open question**: are pointers fat or thin?

Chapter 15 §15.1.3 states:

> `int8->` is a **Fat Pointer containing bounds metadata**. `int8*` is a standard
> Thin Pointer that is strictly only valid inside an `extern` block.

`TYPE_REFERENCE.md` §10 states the opposite — the LLVM IR for `wild` and borrow
pointers is **identical**, with the distinction enforced entirely by the type
checker.

These cannot both be true, and the answer changes a great deal:

| | Fat pointers | Thin pointers |
|---|---|---|
| Bounds checking | **carried at runtime**, so out-of-bounds is detectable on any pointer | only where the compiler can prove or inject a check |
| Size | 2–3 words | 1 word |
| C ABI | **incompatible** — every FFI boundary needs conversion | direct |
| `--verify-memory` | much of it becomes a runtime guarantee | must be proven statically |

It also interacts with D-004: if borrows are second-class and cannot escape, much
of what fat pointers buy is already provided statically at zero cost.

**This needs deciding before any pointer lowering is implemented**, and it is an
ABI decision, so it is expensive to change later.

## Part X — ~~**Open question**~~: the LLVM and Z3 dependency boundary — **settled by D-067**

Chapter 00b §0 describes the native compiler as enforcing zero C/C++ dependencies
**"with the only explicit and isolated exceptions being the LLVM IR generator and
the Z3 SMT Subsystem."**

Both LLVM and Z3 are large C++ codebases. That exception is either a significant
qualification of the zero-dependency rule or a description of the *prototype*
rather than the target. Which it is has never been stated.

There is a meaningful distinction available:

- **LLVM as a tool we invoke** (`llc`, `opt`, `llvm-as` as subprocesses over
  hand-written IR) — no linking, nothing in the TCB at runtime. This is what
  D-011 and D-015 already assume.
- **LLVM as a library we link** (`libLLVM`) — C++ inside the compiler binary.

The same split applies to Z3: invoking it over SMT-LIB2 text versus linking
`libz3`. Note `--debug-z3` already dumps SMT-LIB2, so the text interface exists.

Worth settling explicitly, since "no C/C++ dependencies" and "except LLVM and Z3"
cannot both stand unqualified — and an auditor will ask.

**Settled: invoked, never linked.** There is no exception, because neither is a
dependency in the sense the rule means — the compiler emits text (LLVM IR,
SMT-LIB2) and `llc` / `opt` / `z3` are subprocesses, so nothing C++ enters the
compiler binary and nothing enters a compiled Nitpick program either way. This is
D-055's argument applied to the toolchain: a subprocess boundary is not an FFI
barrier, so a crash in the tool is a nonzero exit status rather than an
uninterceptable fault. D-067 also records what this does *not* claim — LLVM's
IR-to-machine-code translation stays outside the verified boundary, which is
inherent to any toolchain short of a CompCert-style verified compiler.

# Pre-Planning Review: Safety Concerns, Contradictions, and Open Decisions

Findings from a full read of the carried-over specs in `meta/specs/` against the
prototype specs in `../nitpick-docs/specs/`. Ordered by how much downstream work
each one blocks. This is input to planning, not a plan.

Terminology note: "carried-over" means the ten `.md` files copied from
`nitpick-next`. Several of them disagree with each other, so "the spec says" is
often not a well-formed statement — that is itself the main finding.

---

# Part 1 — The garbage collector decision

> # ⛔ SETTLED BY D-003 — AND NOT THE WAY THIS PART RECOMMENDS
>
> **This Part is a historical record of the review, not current guidance.** It is
> preserved because the reasoning is still worth reading; it is fenced because
> `CLAUDE.md` names this document as required reading before planning, and §1.5
> below recommends **option D — keeping `gc` as an opt-in modifier**.
>
> ⚠️ **That recommendation was NOT taken. D-003 settled this the other way**:
> the tracing collector is dropped entirely, `gc` with it, and arenas with
> `Handle<T>` cover the graph-shaped and cyclic data option D was reaching for.
> `gc` is not a keyword in this implementation. D-003 names this section as the
> suggestion it supersedes — read it as the argument that was considered and
> rejected, not as a plan.
>
> **D-003 chose no collector at all.** There is no `gc` keyword, no `gc` memory
> qualifier, and no collector anywhere in the language. `MemoryQualifier` is
> `wild | wildx | stack | defer` (D-065 also removed `move` from that list). The
> four regimes are the default managed/RAII one, `stack`, `wild`, and `wildx`.
>
> The escape question §1.5 leaves open — *"what stops someone taking `@local` and
> passing it out of scope"* — was answered by **D-004**: borrows are
> **second-class**, they pass down the call stack and never up, and the same
> escape analysis that governs `stack` enforces it. That is also property 1 of the
> three that give data-race freedom (`CONCURRENCY_REFERENCE.md` §5.3), and D-062
> and D-072 later extended the identical rule to task frames and channel
> endpoints.
>
> Read the rest of this Part as *why the question was hard*, never as *what the
> language does*.

This is the largest open question and it gates the memory subsystem, the type
checker, and most of the backend.

## 1.1 How the current position was arrived at

`SPEC_GAPS_AND_AMBIGUITIES.md` §1 asks what actually frees memory, and resolves
it as: **hybrid generational GC** (nursery / old generation, safepoints,
shadow-stack root scanning, card-table write barriers). It then states that
**the borrow checker was removed precisely because the GC provides memory safety
by default**, and that dangling pointers are impossible in safe code because the
GC retains everything reachable.

So the GC is currently load-bearing: it is the stated answer to the dangling
pointer question, and the stated justification for deleting the borrow checker.
Those two things move together.

## 1.2 The case against keeping it

**Nondeterministic pause times are a physical safety hazard here.** A
stop-the-world pause is unbounded from the program's point of view. With
actuators mid-motion near a child, an unpredictable multi-millisecond stall is
in the same hazard family as the uncontrolled crash the whole `failsafe`
architecture exists to prevent. `failsafe` gives a *controlled* shutdown path; a
GC pause gives an *uncontrolled* stall with no trap and no handler. Nothing in
the current specs bounds it.

**It is expensive in exactly the way this project cannot afford.** Card-table
write barriers cost on every pointer store; safepoint polls cost at loop
back-edges and calls. Performance is a first-class requirement — demonstrating
viable speed is what funds the hardware — and a tracing collector taxes the hot
path continuously, not occasionally.

**It enlarges the trusted computing base at the worst possible spot.** A
generational collector with card tables, safepoints, and shadow-stack scanning
is thousands of lines of subtle, concurrency-adjacent code that must be written
in Nitpick with zero dependencies *and* formally verified. Tracing collectors
are among the hardest things to verify: the heap becomes global mutable state
mutated by a second logical actor. This works directly against the reason C/C++
was abandoned.

**Verification of user code gets harder too.** Z3 reasoning about object
lifetimes is far cleaner when lifetimes are lexical and deterministic. "This
object is freed at scope exit" is a statement the solver can use. "This object
is freed at some unspecified future collection" is not.

**Peak memory becomes unpredictable.** Reclamation is deferred by construction,
so a long-running companion process has a higher and less predictable high-water
mark. On embedded/robotic targets that is a hard constraint, not a preference.

**Most of its job is already covered.** The language already has four
deterministic mechanisms: `stack` (scope-bound), arenas with `Handle<T>`
(generation counters that make use-after-free a safe `Result` error rather than
a crash), `wild` + `defer` + the leak check at `exit`, and RAII with `nodrop` as
the opt-out. The `Handle<T>` design in particular already solves use-after-free
*deterministically* — which is most of what a GC is being asked for.

This matches the user's own read: the GC predates the `Result` system and the
rest of the safety architecture, and looks like a solution that arrived before
the problem it is now credited with solving.

## 1.3 The case for keeping something

**Cycles.** Scope-based and reference-counted schemes cannot reclaim reference
cycles. This is not hypothetical for Nikola: the knowledge/memory subsystem —
ingestion, the manifold's associative structure — is graph-shaped and very
likely cyclic. Arenas sidestep this (drop the whole arena), but only where
lifetimes are batch-shaped.

**Removing both leaves a real hole.** `SPEC_GAPS` asked the right question:
without a borrow checker, what stops someone taking `@local` and passing it out
of scope? Today the answer is "the GC." Delete the GC and that question is
unanswered again. **GC and borrow checker are alternatives — at least one must be
present**, or dangling pointers come back.

## 1.4 The shape of the workload argues for determinism

Worth weighing: Nikola's heavy allocation is mostly **bulk numeric** — manifold
state, Mamba state, tensors, waveform buffers. Those have predictable, often
batch lifetimes and are arena- and buffer-shaped, not graph-shaped. The
genuinely cyclic, unpredictable-lifetime portion is the knowledge graph and
ingestion layer, which is a minority of the memory traffic and is *not* on the
real-time robotics path.

That asymmetry is the most useful fact for deciding this.

## 1.5 Options

| | Model | Dangling-pointer answer | Determinism | TCB cost |
|---|---|---|---|---|
| **A** | GC, no borrow checker *(current spec)* | GC retains reachable | ✗ unbounded pauses | high (collector) |
| **B** | Borrow checker + RAII/NLL, no GC *(prototype)* | static analysis | ✓ | high (checker, but compile-time only) |
| **C** | Arena/region-first, `Handle<T>` as the safe path | handles + generation counters | ✓ | low |
| **D** | Deterministic default, `gc` as opt-in modifier | static + GC only where opted in | ✓ on the real-time path | medium, confined |

**Recommendation: D**, which is close to C with an escape hatch.

Make deterministic ownership the default — `stack`, arenas + `Handle<T>`, RAII
with `defer`/`nodrop`. Keep the existing `gc` modifier as an **explicit, opt-in**
annotation confined to the genuinely cyclic subsystems (knowledge graph,
ingestion). Then add a picky-rule (`--extra-picky=no-gc`, alongside the
prototype's `no-wild`) that **hard-bans `gc` in real-time and robotics-facing
code**, the same way `no-wild` bans manual memory in high-level code.

This keeps the collector out of the actuator path entirely, confines its
verification burden to one non-real-time subsystem, preserves a cycle story, and
costs nothing on the numeric hot path. Note that the `gc` keyword already exists
in the specs for exactly this kind of explicit opt-in — the change is making it
opt-*in* rather than the implicit default.

**Consequence to accept:** option D still needs an answer to the escape
question for non-`gc` code. Either the borrow checker comes back in some reduced
form, or `@` on locals is restricted (e.g. address-of-local may not outlive its
frame, checked lexically). That decision should be made together with this one,
not after.

---

# Part 2 — Safety-critical concerns

Ranked by consequence, independent of the GC question.

## 2.1 FFI silently defaults to success

`TYPE_REFERENCE.md` §11.2:

> If the C function does not provide error information, the result defaults to `Ok(val)`.

A C function that signals failure by returning `-1`, returning `NULL`, or setting
`errno` gets wrapped as **success**. The `Result<T>` machinery is then satisfied,
and no `failsafe` path is taken.

This is the one finding that directly contradicts the stated safety model. The
whole reason `extern` was changed to return `Result<T>` was to pull the FFI
boundary inside the controlled-failure regime; defaulting to `Ok` pushes it back
out, while *looking* safe at the call site. Silent success is worse than no
wrapper, because it defeats review.

Options: require an explicit error-mapping clause on every `extern` declaration
(`errno`, `negative-is-error`, `null-is-error`, `none`), or default to error and
force an explicit opt-out. Given the posture, the second is more in keeping.

Related question: **why is FFI specified this carefully at all** if C libraries
are barred? Presumably for the public release and for OS/hardware access. Worth
stating explicitly, because "no C dependencies" and a detailed FFI chapter read
as contradictory to a newcomer.

## 2.2 Three documented holes in "everything returns `Result<T>`"

`TYPE_REFERENCE.md` §11.2 states the rule absolutely ("This is NOT optional —
the compiler enforces it"), with `main` and `failsafe` as the only exceptions.
But `BUILTIN_REFERENCE.md` documents three constructs that return bare values:

| Construct | Returns | Source |
|---|---|---|
| `sys!!!(expr, args...)` | bare `int64` | `BUILTIN_REFERENCE.md` §3 |
| `asm!!!<T>(...)` | bare `T` | `BUILTIN_REFERENCE.md` §5 |
| `raw` / `_!` | bare `.value` | by design |

`raw` is intentional and auditable. The other two are raw-tier syscalls and raw
inline assembly — the two places where an unchecked failure is *most* likely to
be catastrophic, and they are the two places with no `Result` wrapper. They also
permit an arbitrary expression as the syscall number.

These need to be either brought under `--extra-picky` (bannable in
robotics-facing code, like `no-wild`), or justified explicitly in the safety
architecture. Right now they are documented in the builtins reference and
invisible to the safety document.

## 2.3 Division by zero: trap or taint?

`OP_REFERENCE.md` §1 says `/` "triggers failsafe on divide-by-zero."
`TYPE_REFERENCE.md` §27 defines `unknown` as a propagating taint, and the
prototype used exactly div-by-zero as its motivating example for continuing in a
degraded state rather than trapping.

These are opposite behaviors for the same event. For a system where an
uncontrolled stop is itself a hazard, "trap and shut down" is not obviously the
safe choice — that is the fail-operational argument the prototype made. Needs an
explicit decision, and it likely differs by context (a control loop wants taint;
a configuration parse wants a trap).

## 2.4 SeqCst atomics vs the performance requirement

The prototype's `concurrency_specs.txt` §3.3 enforces **strict sequential
consistency** on all high-level `atomic<T>` methods, explicitly justified by
"Nitpick's AGI and physics engine goals," with weaker orderings relegated to
intrinsics for core framework developers.

That is the right *safety* call and it is expensive — SeqCst is the strongest and
slowest ordering, and on the ZMQ spine and multimodal layers it will be on hot
paths. This is a genuine safety-vs-performance tension that should be decided
deliberately rather than inherited. Note it is decided *correctly* for safety
today; the question is only whether the framework-developer escape hatch is
sufficient for the throughput Nikola needs.

## 2.5 `tbb32:error` is stored as a plain `i32`

`TYPE_REFERENCE.md` §11.2 gives `Result<T>` as `{ T value, i32 error, i8 is_error }`.
*(Superseded by **D-069**: the stored `is_error` is removed, leaving
`{ T value, i32 error }`. `r.is_error` remains valid source as a derived accessor
for `r.err != 0i32`.)*
But `tbb` types are specified elsewhere as having a **sticky ERR state** on
overflow — that state has to live somewhere in the representation. Storing the
error field as a bare `i32` either loses the ERR encoding or implies a reserved
bit pattern that is not documented. Given `tbb` sticky-ERR is the primary
numerical-drift defense, its exact representation needs pinning down.

---

# Part 3 — Cross-document contradictions

Each of these is a case where two carried-over documents state incompatible
things. All block frontend work.

| # | Conflict | Sources | Notes |
|---|---|---|---|
| 3.1 | **RAII vs GC** | `MEMORY_REFERENCE.md` §1.1 ("Implicit RAII/Scope-based", "deterministic destruction") vs `SPEC_GAPS` §1 (generational GC) | Superseded by Part 1's decision, whichever way it goes. |
| 3.2 | **UFCS banned but used** | `SPEC_GAPS` §3 ("does NOT support method calls", "`.` is strictly struct field access") vs `MEMORY_REFERENCE.md` §5.2 (`my_arena.alloc()`) vs prototype `concurrency_specs.txt` §3.2 (`counter.fetch_add(1i32)`) | Arenas **and atomics** both use method syntax. Either UFCS survives for compiler-known types, or both need respec. |
| 3.3 | **`?!` arity** | `TYPE_REFERENCE` + `AST_REFERENCE` + prototype say `expr ?! errCode`; `OP_REFERENCE` says niladic; `MEMORY_REFERENCE` uses it as a default value | 3–1 for `errCode`. `OP_REFERENCE` is likely the error. |
| 3.4 | **`free`/`realloc` aliases** | `BUILTIN_REFERENCE.md` §1 preserves them; `MEMORY_REFERENCE.md` §4 omits them | Trivial, but they are the *memory* docs. |
| 3.5 | **`raw` syntax** | `AST_REFERENCE` says `raw(expr)`; `OP_REFERENCE`/`TYPE_REFERENCE` say both `raw expr` and `raw(expr)` | Decide keyword-prefix vs call form. |
| 3.6 | **`unknown` scope** | `TYPE_REFERENCE.md` §27 defines it as a Layer 2 mechanism; `SAFETY_ARCHITECTURE.md` describes Layer 2 without mentioning it | The layer document omits one of the layer's mechanisms. |

---

# Part 4 — Missing specifications

The carried-over set has ten documents. The prototype had roughly twenty-eight.
These topics existed in the prototype and have **no carried-over counterpart**,
yet are referenced by the documents that were carried over:

| Missing | Referenced from | Why it matters |
|---|---|---|
| **Concurrency** | `TYPE_REFERENCE` §13 (atomics), §17 (`Future<T>`); `--verify-concurrency` flag | async/await, `atomic<T>`, SeqCst policy, threading model — all undocumented in the new set. A verification flag exists for properties nothing specifies. |
| **Traits / OOP** | `TYPE_REFERENCE` §18 (`dyn Trait`) | Prototype had supertraits, associated types, coherence, object safety, blanket impls, derive macros. Traits also interacted with the removed borrow checker (`TRAITS_AND_BORROW_SEMANTICS_RFC.md`). |
| ~~**Generics**~~ — **settled by D-064** | `SPEC_GAPS` §3 resolves *declaration* syntax only | Nested generics, implicit call syntax, the `Type` keyword, monomorphization strategy — all unspecified. **Now:** bodies are checked at their definition against bounds alone, not per instantiation; expression-position type arguments are always the turbofish, which confines `>>`-splitting to a precisely delimited type-argument context; `comptime` value parameters are added for D-056's lock levels; monomorphization is depth-capped at 64, deduplicated, and reversibly mangled with no hash. The `Type` keyword was already namespace-only (D-028). |
| **Macros / metaprogramming** | `AST_REFERENCE` §5 (`MacroInvocationExpr`), `CONTROL_REFERENCE` §1.3 (macro patterns in `pick`) | `AST_REFERENCE` states the `pre()` text preprocessor was dropped and macros are now AST-native — a significant change with no spec behind it. `comptime` and `cfg` are also unspecified. |
| ~~**Streams / IO**~~ — **written: `IO_REFERENCE.md`** | — | Needed for the driver and diagnostics. `Reader`/`Writer` traits so diagnostics are capturable, every operation `async` per D-071, end-of-input as an error code rather than a sentinel, fixed buffering that is never inferred from `isatty`, lexical stream lifetime. |
| ~~**Build system**~~ — **written: `BUILD_REFERENCE.md`** | — | Needed early for the bootstrap ladder. One manifest schema (two were in use), no network during a build, byte-reproducible output, and a three-stage bootstrap whose stage-1/stage-2 fixpoint check is what proves self-hosting. |
| **Casting / literals / strings / pointers / collections / asm / hardware** | scattered | Partially absorbed into `OP_REFERENCE`, `TYPE_REFERENCE`, `BUILTIN_REFERENCE`; coverage is thinner than the prototype's. |

Given the **full-frontend-first** bootstrap strategy, the missing *frontend*
specs are the blocking ones: generics, traits, and macros all affect the parser
and AST, and the whole point of the strategy is not to rewrite the parser later.

## 4.1 `AST_REFERENCE.md` is not yet adequate for that strategy

It states its own bar — "the AST must be perfectly defined to encapsulate the
entire language grammar from Day 1" — and does not currently meet it:

- **No nodes** for pipe operators (`|>`, `<|`), the `is` ternary, `discard`,
  labelled `fall`, `break`/`continue`, range expressions (`..`, `...`), the `$`
  iteration variable, `await`, or string interpolation (`&{ }`).
- **Two incompatible naming conventions** in one document: `IfStmt` / `BlockStmt`
  / `VarDeclStmt` alongside `WHEN_STMT` / `LOOP_STMT` / `PICK_STMT` with
  positional `.a` / `.b` / `.c` operand slots. The second style looks lifted from
  the prototype's implementation.
- **`LOOP_STMT` and `TILL_STMT` carry an optional `end` block**, which
  `CONTROL_REFERENCE.md` documents only for `when`.
- `PICK_CASE` mentions an `ERR:` match label that appears nowhere else.

## 4.2 Minor but worth fixing while in there

- `TYPE_REFERENCE.md` has **two sections numbered 27** ("Special Values" and
  "Operator Reference"), and skips §8 and §24.
- `TYPE_REFERENCE.md` §13 cites an **`AGENTS.md` rule**; no such file exists in
  this repository.
- `@cast<T>(p)` is used in `TYPE_REFERENCE.md` §27 but is absent from
  `BUILTIN_REFERENCE.md`.
- `TYPE_REFERENCE.md` §11.2 documents a third `Result` construction —
  `return Result{ err: errCode, value: retVal, is_error: true }`, returning a
  value *and* an error simultaneously. Partial success has real safety semantics
  and is not discussed in `SAFETY_ARCHITECTURE.md`.

---

# Part 5 — Suggested decision order

Later items depend on earlier ones.

1. **Memory model** (Part 1) — GC, borrow checker, or the hybrid. Everything
   downstream in the type checker and backend depends on it.
2. **Escape/aliasing rule** — the answer to "what stops `@local` outliving its
   frame," which follows directly from (1).
3. **FFI error mapping** (2.1) and the **raw-tier holes** (2.2) — both are safety
   model corrections, both are cheap to decide now and expensive to retrofit.
4. **Div-by-zero and the scope of `unknown`** (2.3) — fail-operational vs
   fail-stop, possibly context-dependent.
5. **UFCS** (3.2) — affects the parser directly; arenas and atomics both hinge on it.
6. **`?!` arity, `raw` form, remaining doc conflicts** (3.3–3.6) — mechanical
   once the above are settled.
7. **Write the missing frontend specs** — generics, traits, macros, concurrency —
   before the parser is built, per the full-frontend-first strategy.
8. **Rebuild `AST_REFERENCE.md`** from the completed grammar (4.1).

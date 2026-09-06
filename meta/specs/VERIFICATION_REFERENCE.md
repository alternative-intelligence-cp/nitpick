# Nitpick Formal Verification & Design by Contract

Nitpick fundamentally rejects unsafe behavior. To achieve this, it deeply integrates with the Z3 SMT solver to mathematically prove the correctness of the code before it is allowed to execute.

> **The pipeline is real since 1.5.0 (2026-09-03; D-218, D-219; the record is
> `meta/roadmap/1.5/1.5.0.md`).** The compiler emits every function's proof
> obligations as SMT-LIB2 text (`npkc --obligations DIR`) and reads a
> manifest of verdicts (`npkc --elide nitpick.obligations`); `npkg verify`
> spawns the pinned z3 — one fresh process per function under the pinned
> determinism profile — decides every obligation, holds the rows to the
> committed `nitpick.obligations`, and emits the VERIFIED build, where every
> guard the manifest discharged gives way to `llvm.assume` of the proven fact.
> §7b is the obligation catalogue; §8's manifest is the D-218 schema now. The
> `--verify*` and `--smt-*` compiler flags this file once tabulated are
> STRUCK (§5): verification is a property of the project, in `[verify]`,
> never of a command line (D-077, D-219). 1.5.0 produces the D-007 division
> pair; `limit`, contracts, `prove`, overflow and the rest follow in
> 1.5.1–1.5.8.

## 1. Formal Proofs (`prove` and `assert_static`)

### 1.1 `assert_static`
The `assert_static` builtin allows developers to encode compile-time logic checks. If the expression evaluates to false, compilation immediately halts.

```nitpick
assert_static(1i32 == 1i32);
```

### 1.2 `prove`
The `prove` keyword interacts directly with the Z3 verification backend (when the `--verify` compiler flag is set). It forces the SMT solver to construct a mathematical proof that the subsequent expression holds true across all possible control flows and variable states. 

If the solver finds a path where the expression is false, compilation fails. The compiler extracts a counterexample showing the failing variable assignments (visible via `--prove-report`).

```nitpick
int32:x = get_val();
if (x > 0i32) {
    prove(x != 0i32); // Mathematically verified at compile time.
}
```

**Path Condition Accumulation:**
The `prove` keyword is path-condition-aware. Branch guards from enclosing `if`, `while`, and other control flow are accumulated and asserted as Z3 axioms before checking the proof obligation. This means `prove(x != 0i32)` inside `if (x > 0i32)` automatically benefits from the guard `x > 0i32`.

---

## 2. Value Constraints: `limit<Rules>`

Nitpick allows you to define constraints on value ranges called `Rules`. You can then bind these rules to variables using the `limit<RuleName>` syntax. 

```nitpick
// 1. Define a rule for an integer
// The '$' variable represents the value being constrained
Rules<int32>:r_positive = { $ > 0i32 };

func:main = int32() {
    // 2. Bind the rule to a variable
    limit<r_positive> int32:x = 5i32;
    exit 0i32;
};
```

> **Typed since 1.5.1 (D-220; `meta/roadmap/1.5/1.5.1.md`).** The rule name in
> `limit<…>` RESOLVES like any name — a typo is `NITPICK-RESOLVE-002`, a
> name that is not a `Rules` block `NITPICK-RESOLVE-011` — at a local, a
> parameter, and a refinement. A `Rules` body types eagerly: `$` is the
> subject's type, every clause a `bool`. A limited binding's declared type is
> the rule's subject BY IDENTITY — `limit<r_positive> int64:x` refuses
> (`NITPICK-TYPE-059`), and so does a type parameter: the check runs on the
> binding's own value at every write, so the two are one type. A `Rules`
> clause is a contract expression and follows §3's rules for what it may
> contain.

**`limit<Rules>` is enforced in every build.** `--verify` decides only whether a
given check is *discharged statically and therefore elided*, never whether it
exists (D-068). With `--verify`, the integrated Z3 solver proves that the assigned
value (`5i32`) satisfies the constraint (`$ > 0i32`) and the check is removed.
Where it cannot be proven statically — reading user input, say — the check remains
at runtime, and a violation **traps to `failsafe`**.

> A safety property must not depend on a compiler flag. The useful consequence is
> that **proving a constraint removes its runtime check**, so `--verify` is also
> the mechanism by which constrained code reaches the speed of unconstrained code.

> **Live since 1.5.2 (D-251, D-252; `meta/roadmap/1.5/1.5.2.md`).** THE
> WRITE POINTS: a limited binding is checked AFTER every write, over its
> whole current value — its initialiser (a declaration without one is not a
> write point: the vacant value is never read), every assignment to it or to
> any part of it (a field or element store re-checks the root), and the
> callee's entry for a limited parameter, once per call in a sync function
> and once per task at state 0 in a coroutine. The check is one generated
> predicate per `Rules` declaration (`@"npk.<module>.<name>"`, the
> refinements then the clauses in source order, short-circuit) called on the
> value; a false verdict traps `LimitViolated` (−4111) to `failsafe`. **A
> limited binding has no address**: `@`, `$$m` and `$$i` of it, and a move
> out of an owning field or element of it, refuse (NITPICK-TYPE-063) — a
> write through an alias would be one no write point sees; pass it by value.
> **A `limit` where no write point exists refuses** (NITPICK-TYPE-064): a
> trait signature's parameter, a `wild`/`wildx` binding, a `comptime`
> function; `main`/`failsafe`'s parameters under D-244. THE OBLIGATIONS:
> every write point is a `limit` row whose goal is the rule over the new
> value, with the rule asserted as a HYPOTHESIS on every later version of
> the binding — so a division by a limited divisor discharges, after a loop
> included; a subject outside the encoder's fragment (§7b's tiers) is an
> `unencoded` row whose guard stays. Every direct call of a sync callee with
> limited parameters is a `limit-subsume` row: the caller's knowledge of
> every argument against the callee's rules — the spec's "one `Rules`
> implies another at a boundary". ELISION: a discharged write point emits
> ONE `llvm.assume` over the rule's range clauses (a comparison between `$`
> and an integer literal, `&&`-composed) and no check; a discharged
> `limit-subsume` row lets the call name the callee's BODY past its checked
> entry (D-252): a sync function with a limited parameter is emitted as
> `<symbol>.body` plus its ordinary symbol as the checked entry, which every
> function value, vtable slot and spawn names by construction.

> `FORMAL_DRAFT` 12.6.1 says only that constraints are "enforced dynamically at
> runtime" without saying what a violation *does*. It traps to `failsafe`, as
> above — stated explicitly here because an unspecified failure mode in a
> constraint system is worse than no constraint system.

### 2.1 Z3 and the borrow checker

The borrow checker is **retained** (D-003) — dropping the garbage collector made
static ownership the memory model rather than an alternative to it. Z3 resolves
aliasing patterns that lifetime analysis alone cannot: when two mutable borrows
(`$$m`) target the same array through index variables constrained by different
`limit<Rules>`, the solver proves the indices unequal and the borrows disjoint,
suppressing a false-positive aliasing error.

```nitpick
Rules<int32>:EvenIdx = { $ % 2i32 == 0i32 };
Rules<int32>:OddIdx  = { $ % 2i32 == 1i32 };

func:update = int32(limit<EvenIdx> int32:i, limit<OddIdx> int32:j, int32[100]:arr) {
    $$m int32:a = arr[i];   // mutable borrow at even index
    $$m int32:b = arr[j];   // Z3 proves i != j → borrows are disjoint
    pass(a + b);
};
```

This synergy is a direct argument for static ownership over a tracing collector:
lexical lifetimes give the solver facts it can use, whereas object validity under
a collector is a global reachability property.

Borrows are **second-class** (D-004): they pass down the call stack and never up.
A borrow may not be returned, stored into anything outliving the frame, captured,
or carried across an `extern` call, a thread spawn, or an `await` point. This
removes the need for lifetime variables entirely — validity is bounded by the
callee frame, structurally.

### 2.2 Rules Composition and Subsumption

Rules can reference other Rules via `limit<OtherRule>`, creating a constraint hierarchy:

```nitpick
Rules<int32>:r_positive = { $ > 0i32 };
Rules<int32>:r_small_positive = { limit<r_positive>, $ < 100i32 };
// r_small_positive requires: $ > 0i32 AND $ < 100i32
```

The Z3 solver can prove that one Rules block subsumes another (e.g., `r_small_positive` mathematically implies `r_positive`), enabling safe narrowing at call sites without redundant checks.

> **Since 1.5.1 (D-220):** a refinement names a rule over the SAME subject —
> a `Rules<int64>` refining a `Rules<int32>` is `NITPICK-TYPE-059`, since a
> refinement is a conjunction and a conjunction has one subject — and a
> `Rules` block that refines itself, directly or through a chain, is refused
> at resolve (`NITPICK-RESOLVE-006`): 1.5.2 discharges a `limit` by
> expanding its refinements into one conjunction, and a cycle would never
> finish expanding. *(Landed at 1.5.2: the conjunction is `enc_rule`, the
> implication is a `limit-subsume` row decided by z3 with the source
> binding's rule as its hypothesis, and a discharged row is the "narrowing
> without redundant checks" above — the call names the callee's body.)*

---

## 3. Function Contracts: `requires` and `ensures`

Nitpick implements classic Design by Contract (DbC) on function boundaries.

*   **`requires`**: Preconditions that must be true when the function is called.
*   **`ensures`**: Postconditions that the function guarantees will be true when it returns. (Use the special `result` keyword to reference the return value).

```nitpick
func:divide = int32(int32:a, int32:b) 
    requires b != 0i32 
    ensures result > 0i32 
{
    pass(10i32); // Hardcoded for example
};
```

When you compile with the `--verify-contracts` flag, the compiler translates these contracts into Z3 assertions to prove they are mathematically valid. If you don't use the static verifier, Nitpick automatically enforces these contracts at runtime.

> **Typed since 1.5.1 (D-221, D-241…D-245; `meta/roadmap/1.5/1.5.1.md`).**
> Every proposition — `requires`, `ensures`, each `invariant` conjunct,
> `prove`, `assert_static` — is a `bool` (`NITPICK-TYPE-007`). **`result`**
> is a keyword with its own node: the SUCCESS value, typed `T`, legal in
> `ensures` alone, so no binding can shadow it. **`old(expr)`** is a keyword
> operator: the operand's value at the function's ENTRY, legal in `ensures`
> and in an `invariant` with that one meaning, never nested, never of
> `result`, and only of a COPYABLE value — neither an owner (a `string`, a
> `buffer`, an arena) nor an address (a pointer, a slice) — refused by name
> otherwise. `main` and `failsafe` carry no contract (D-244); a `never
> fails` function may (D-241).
>
> **A contract expression admits only what a proposition can evaluate
> anywhere** (`NITPICK-TYPE-060`, the message names which): no `await`, no
> `move`, no `relay`/`?!`/`?|`/`drop`, no `pick` expression, no store, no
> manufactured view (`#wild_ptr`/`#wild_slice`), no method on a lock, an
> atomic, a channel endpoint or an arena. A call is to a NAMED function —
> a user function spelled `raw f(…)` (the licence checks `never fails`) or a
> never-fails builtin bare — that is **`pure`**; a function value, a field
> or a `dyn` is refused, because the verifier encodes a contract call as an
> uninterpreted function per KNOWN symbol, which is sound only of a body
> that is a function of its arguments. `is_err(x)` is a predicate and passes.
>
> **Purity is declared** (D-242): `pure` is a marker clause in the contract
> window, orthogonal to `never fails` (a pure function may `fail`; a
> contract's helper writes both words), checked in the body
> (`NITPICK-TYPE-061`): no `async`/`thread`, no `move` parameter, no callee
> that is not itself a named `pure` function or a `pure` builtin (the
> reference's `Pure` column: the string views and comparisons; everything
> that allocates, touches a descriptor, the clock, the environment or a
> registry, suspends, or aliases a cell is `effect`), no method on a
> shared-state receiver, no `wild`/`wildx` storage, no owning local, no
> store that reaches memory the caller can see, no manufactured view. An
> impl keeps its trait method's `pure`. Purity never rides a function type.

> **Live since 1.5.3 (D-221, D-267, D-268; `meta/roadmap/1.5/1.5.3.md`).**
> A contract violation is a TRAP: `RequiresViolated` (−4112),
> `EnsuresViolated` (−4113), `InvariantViolated` (−4114), through the D-142
> route with the origin chain restarted at the clause that failed, reaching
> `failsafe` like every trap. **A `requires` is checked at the CALLEE's
> entry** in a generated predicate `<symbol>.req` over the function's
> parameters (the `Rules` shape: one trap per clause, the clause's own site
> in the chain), called from the checked entry of a sync function — which
> therefore splits into `<symbol>.body` and its ordinary symbol exactly as a
> limited parameter makes it (D-252) — or at state 0 of a coroutine with the
> parameters loaded from the frame; so every caller is covered, direct,
> indirect, through `dyn` and through `await`. **An `ensures` is checked at
> every return seam** (`pass v`, and `return Result{…}` when its error field
> is 0), before the value is stored: `result` is the value in register,
> `old(e)` a snapshot taken once at the body's start (an alloca, or a frame
> slot in a coroutine), one trap per return point. **`failsafe`'s own
> postcondition** (D-014 §3.3) is the compiler's `ensures`: `<code> > 0` at
> every `exit`, a literal that is not positive refused by the checker
> (REACH-004), a computed one guarded and trapping `EnsuresViolated` —
> which, inside `failsafe`, re-enters it and ends the process at 70 (§4.6).
>
> **The obligations.** One `requires` row per CALL with a recorded callee —
> the callee's clauses over the argument terms: `bypass` at a direct sync
> call (discharged with every other bypass row of the call, the call names
> `.body` past the checked entry), `held` at an `await` or through a `dyn`
> (recorded; the word `retained` whatever the verdict, since the guard is
> the callee's own entry and no build bypasses it; D-268) — and one per
> function ENTRY, the conjunction over the parameters under their range
> axioms: discharged, the predicate is uncalled, and the precondition is the
> body's first hypothesis either way. One `ensures` row per return point,
> `result` the value's term and `old(…)` the entry version — discharged, the
> seam's check is elided. A callee's `ensures` is KNOWLEDGE at every unwrap
> that continues only on success (`raw f(…)`, `f(…) ?! E`, `relay f(…)`,
> plain or awaited; never at `?|`), and a `pure never fails` callee is an
> UNINTERPRETED FUNCTION `|uf.<name>.<decl>|` (equal inputs, equal outputs —
> D-242's purity is exactly the soundness condition), so `sq` in a contract
> and `sq` in the body are one symbol while `sq(3)` has no value.
> CONFORMANCE is two rows per impl method whose trait method carries a
> contract, in a space of their own with no guard (`none`): the trait's
> `requires` implies the impl's (an impl may weaken), the impl's `ensures`
> implies the trait's (an impl may strengthen) — an open row is reported,
> never a refusal: the impl's own entry traps the argument the trait admits.
> A guard inside a clause (a division in a `requires`) is the function's own
> site, lowered once in the predicate; inside an `ensures` it is keyed under
> the seam it is checked at, since the clause is lowered at every seam.

### 3.1 The `Result<T>` Intercept

> **Dead by D-221 (ratified for 1.5, recorded 1.5.0):** a contract violation
> is a PROGRAM-INVALID state, not a value — the violation channel is the trap
> route (distinct D-141-space codes for `requires`/`ensures`/`invariant`),
> reaching `failsafe` like every trap, never a `Result`. The paragraphs below
> describe the pre-D-084 framing and stay as the record of what was replaced;
> 1.5.3 implemented the trap route (the note above §3.1).

One of the most powerful features of Nitpick's DbC implementation is how it interacts with the type system. If a function declares a `requires` clause, **Nitpick implicitly ensures its return type is wrapped in a `Result<T>`**. 

If a caller violates the precondition at runtime, the function immediately intercepts execution and returns a `Result` error rather than crashing or triggering the failsafe. This heavily intertwines contract programming with Nitpick's sticky error propagation system: the caller handles the potential contract violation like any other error — `?|` a default, `?!` to trap, `relay` it, or branch on it. (`raw` is D-163's checked unwrap of a `never fails` callee, and a function with a `requires` can refuse its inputs, so it is never `never fails` — `raw` does not apply here.)

```nitpick
func:main = int32() {
    // Because `divide` has a `requires` contract, the call can fail, and
    // the failure is handled like any other -- here, trapped.
    int32:y = divide(10i32, 2i32) ?! 7tbb32;
    
    exit 0i32;
};
```

---

## 4. Loop Invariants (`invariant`)

Loop constructs (`loop`, `while`, `till`, `when`) support an `invariant` clause specifying conditions that must hold at every iteration boundary.

```nitpick
func:sum_range = int32(int32:n)
    requires n > 0i32
    ensures result >= 0i32
{
    int32:total = 0i32;
    int32:i = 0i32;
    while (i < n) invariant total >= 0i32, i >= 0i32 {
        total = total + 1i32;
        i = i + 1i32;
    }
    pass(total);
};
```

When compiled with `--verify-contracts`, the Z3 solver verifies the inductive step: if the invariant holds at the start of an iteration and the loop condition is true, then the invariant still holds at the end of the iteration.

> **Typed since 1.5.1:** each conjunct is a `bool` under §3's contract
> rules; a counted loop's invariant may name `$`, and an invariant may name
> `old(expr)` — the value at the FUNCTION's entry (D-243), the textbook
> relation between a running total and the bound it started from.
>
> **Live since 1.5.3.** The invariant is CHECKED AT THE LOOP HEAD, before
> the condition, in every build — at entry and after every iteration, one
> trap per clause (`InvariantViolated`, −4114) — for every loop form (a
> counted loop's frame is pushed before its head so `$` reads the counter).
> Its rows: the ENTRY row at the loop statement over the versions before the
> loop, the PRESERVATION row at the body block over the versions at its end,
> and one per `continue` that re-enters the loop; the head's check is elided
> only when all of them are discharged. Inside the body the invariant and,
> for `while`/`when`, the condition are hypotheses over the havoced versions
> (a divisor guarded by `while (i > 0)` proves); after a loop nothing
> `break`s out of, the invariant is a hypothesis again (the exit is a head
> visit) — the condition's negation is not (1.5.4's path condition). A
> counted loop's `$` and a `for` binding are opaque outside a rule until
> 1.5.4, so an invariant naming them is `open`: recorded and checked, never
> refused, never silently true.

---

## 5. Verification Compiler Flags

> **STRUCK at 1.5.0 (user-ratified, 2026-09-03; D-077, D-218.2, D-219).**
> None of the flags below exists in `npkc`. Verification configuration is the
> PROJECT's, in `nitpick.toml`'s `[verify]` (the solver pin and the
> determinism profile, read by every invocation), and the command is
> `npkg verify` (BUILD_REFERENCE §7): `--smt-opt` (D-219 — elision is a
> property of the verified build, recorded in the manifest, never a flag),
> `--smt-manifest` (the manifest is `nitpick.obligations`, at the root, by
> convention), `--smt-timeout` (D-218.2 — the wall-clock timeout is DISABLED
> and `rlimit` is the sole budget; a knob that would re-enable it is refused
> by name), `--verify-level` and the `--verify-*` family (every obligation
> kind is attempted, always, under the one budget — a level would be a
> default that varies by invocation). What survives, re-homed:
> `--prove-report` and `--debug-z3` are `npkg verify --explain` (a model per
> open row, the reason per budget row, an unsat core per discharged one) and
> the retained `build/verify/obl/` directory. The compiler's own flags are
> `--obligations DIR` and `--elide FILE`, driven by the runner. The table is
> kept as the record of the prototype's surface.

| Flag | Purpose |
|------|---------|
| `--verify` | Enable Z3 Rules/limit verification |
| `--verify-contracts` | Verify requires/ensures/invariant contracts |
| `--verify-overflow` | Verify integer arithmetic overflow |
| `--verify-concurrency` | Verify data-race freedom and **lock-order freedom** (D-056). Deadlock freedom is *not* claimed outright — residual deadlock is contained by mandatory deadlines, not proven absent |
| `--verify-memory` | Verify use-after-free & recursion bounds |
| `--verify-level=N` | Controls verification depth (see table below) |
| `--smt-opt` | Enable SMT-guided optimizations (eliminates proven-safe checks). **Writes an elimination manifest** — see §8 |
| `--smt-manifest=<path>` | Path to the elimination manifest `--smt-opt` records against |
| `--smt-timeout=N` | Per-query Z3 solver timeout in ms (default: 5000) |
| `--prove-report` | Emit prove/assert_static outcomes with counterexamples |
| `--debug-z3` | Dump SMT-LIB2 for proof obligations |

### 5.1 Verification Levels

The `--verify-level=N` flag controls verification depth:

| Level | What is verified |
|-------|-----------------|
| `0` | Rules consistency only |
| `1` | Level 0 + value constraints + `prove` / `assert_static` |
| `2` | Level 1 + function contracts + arithmetic overflow |
| `3` | All verification (contracts, overflow, concurrency, memory, SMT optimizations) |


---

## 6. Verification Backends

Two complementary systems, operating at different levels.

### 6.1 Z3 SMT Solver — proves *programs* correct

Invoked during compilation (`npkc --verify`). Translates AST nodes into SMT
formulas and decides concrete proof obligations for one specific program.

Covers: `limit<Rules>` constraints, function contracts, loop invariants,
`prove` / `assert_static` assertions, arithmetic overflow, memory safety,
concurrency, and borrow-checker index disjointness.

### 6.2 K Framework / `kprove` — proves the *language* correct

Used offline during language development, not during compilation. Proves
**metatheoretic** properties about the semantics themselves — for example that
erasing verification constructs is sound, that the borrow rules preserve memory
safety invariants, and that `Result<T>` propagation is correct.

The full operational semantics live in `k-semantics/nitpick.k`, with proof claims
in `k-semantics/proofs/`.

### 6.3 Why both

> **Z3 ensures your *program* is correct. K ensures the *language* is correct.**

Together they give an unbroken chain from language specification to compiled
binary. A verified program on top of unverified semantics proves less than it
appears to — Z3 answers "does this code satisfy its contracts", K answers "do the
rules Z3 is reasoning about actually mean what we think".

This is also why the zero-dependency constraint reaches as far as it does: a
C library inside the trusted computing base sits outside **both** backends. Z3
cannot see its contracts and K cannot model its semantics.

### 6.4 The `wildx` verification boundary (D-035, built in 0.10.5)

Runtime-generated code (`wildx`, the JIT) is the one place inside a Nitpick
program that sits outside both backends — for the same reason an FFI call
does: **the code does not exist when the verifier runs**, so there is no AST
for Z3 to translate and no term for K to reduce. This is inherent to runtime
code generation, not to Nitpick's design; no language can verify code that
does not yet exist.

What IS verified is the **container**, and it is verified structurally, not
by a runtime check:

- **W^X is a one-way transition.** `wildx_seal` moves a page RW→RX and there
  is no reverse; a page is never writable and executable at once. The
  analysis (bindings.npk, 0.10.5) refuses any write after seal
  (`NITPICK-WILDX-001`) and any execute before it (`NITPICK-WILDX-002`), so
  the transition cannot run backwards in a program that type-checks.
- **The lifecycle is a state machine** — `alloc → write → seal → execute →
  free` — with seal-after-free, double-free, and use-after-free falling out
  of the move machinery (a free is a move, D-065) and no-live-pages-at-exit
  out of the `<wild-live>` registry (D-151). Guard pages turn an
  over/underrun into a fault, and the page is placed by the kernel's mmap
  randomisation (ASLR).

The guarantee `wildx` delivers is therefore **containment**: the JIT cannot
corrupt the host program's memory safety. The *contents* of the generated
bytes are validated by Nikola's sandbox and oracle rounds, not by these
backends — exactly the division of labour D-035 settled.

**Certification note.** A program containing `wildx` will not reach the
highest assurance levels of DO-178C, IEC 61508, or ISO 26262, which require
structural coverage over code that exists before execution. That is a
property of the *program*, not the language: **`--extra-picky=no-wildx`**
(0.10.5) is a build mode that excludes runtime code generation entirely, so
the same language serves both the JIT-using and the highest-assurance
audiences without changing. It is a rule separate from `no-wild` because
manual memory and executable memory are different risks.

---

## 7. Deadlock: proven where possible, contained otherwise (D-056)

Data-race freedom is accounted for by three structural properties
(`CONCURRENCY_REFERENCE.md` §5.3): borrows cannot cross a thread spawn or
`await` (D-004), tasks do not migrate between threads (D-032), and shared arenas
never move memory or reuse slots (D-017).

**Deadlock is addressed in two layers**, because the second is what makes the
first honest:

1. **Lock-order freedom is proven.** Every blocking primitive — `mutex`,
   `rwlock`, `condvar`, `channel`, `barrier` — carries a compile-time `LEVEL` in
   its type, and acquisition must strictly increase. Circular wait is impossible
   by construction. A whole-program analysis computes each function's transitive
   acquisition set; dynamically dispatched methods declare a maximum level and
   are checked against it, and an undeclared method may not acquire at all.

2. **Residual deadlock is contained, not proven absent.** Every blocking
   operation takes a deadline and returns `Result`; there is no infinitely
   blocking acquire. What the analysis cannot cover — priority inversion, an
   unresponsive peer process, a declared-but-broad dynamic bound — surfaces as a
   timeout error at a known point rather than a wedged process.

The flag is documented as verifying **data-race and lock-order freedom**
specifically. It does not claim deadlock freedom, and the previous wording that
did has been corrected: a safety claim nothing backs is worse than an absent one,
because it invites reliance.

---

## 7b. The obligation catalogue (D-218.7; landed 1.5.0)

Every kind the manifest's `kind` column may carry, exhaustively — the list
`src/backend/smt/smt_kinds.npk` spells and the harness diffs against this
table (`check_obligation_kinds_agree`): two lists that must agree are an
instrument. A kind's `guard` says whether a runtime check exists for it to
elide (D-219); the subcycle column says where its rows are produced.

<!-- BEGIN obligation-catalogue -->
| kind | what the obligation states | guard | rows from |
|---|---|---|---|
| `div-zero` | the divisor of an integer `/` or `%` is not zero (D-007, D-142) | yes | 1.5.0 |
| `div-min` | a signed division is not `INT_MIN / -1` (D-142) | yes | 1.5.0 |
| `overflow` | a plain-integer `+ - *` stays in range (D-210) | yes | 1.5.8 |
| `bounds` | an index is inside its array, slice or buffer (D-070) | yes | 1.5.8 |
| `cast-range` | a checked cast's value fits its target (D-148) | yes | 1.5.8 |
| `exhaustive` | a `pick` covers its domain (checker-discharged) | no | 1.5.4 |
| `requires` | a callee's precondition holds at the call (D-221) | yes | 1.5.3 |
| `ensures` | a body's postcondition holds at its return (D-221) | yes | 1.5.3 |
| `invariant` | a loop invariant holds at entry and is preserved (D-221) | yes | 1.5.3 |
| `limit` | a `limit<Rules>` binding satisfies its rule at every write point (D-220) | yes | 1.5.2 |
| `limit-subsume` | one `Rules` implies another at a boundary (D-220): the caller's knowledge of every argument against the callee's rules, at a direct call of a sync callee | yes | 1.5.2 |
| `terminate` | a recursion or unbounded loop has a decreasing variant (D-218.7) | no | 1.5.8 |
| `stack-depth` | the recursion depth is bounded (the audit's G-6 row) | no | 1.5.8 |
| `err-exit` | a twisted-family value leaving its family is not ERR (D-144) | yes | 1.5.8 |
| `failsafe-post` | `failsafe` returns a positive value (D-014) | yes | 1.5.3 |
| `prove` | a `prove(...)` holds under its path conditions | no | 1.5.4 |
| `assert-static` | an `assert_static(...)` folds to true (the frontend) | no | 1.5.4 |
<!-- END obligation-catalogue -->

> **[D-267, 1.5.3 step 1 (2026-09-06).]** `failsafe-post`'s guard column
> read `no` as ratified: D-014 stated the postcondition and nothing checked
> it. S-43's decision gave it a guard -- `failsafe`'s `exit` operand is
> checked positive at the `exit` (EnsuresViolated, the trap route's re-entry
> rule ending the process at 70), a literal that is not positive refused by
> the checker (REACH-004) -- so a discharged row elides that check and the
> column reads `yes`.

> **[D-252, 1.5.2 step 4 (2026-09-04).]** `limit-subsume`'s guard column
> read `no` as ratified. The guard a discharged row elides is the CALLEE's
> entry check, at that call: a sync function with a limited parameter emits
> its body under `<symbol>.body` and its ordinary symbol as the checked entry
> (the entry checks, then a tail call of the body); a direct call whose row
> the manifest discharged names the body, every other call -- and every
> function value, vtable slot and spawn, which never learn of `.body` -- names
> the entry. A coroutine callee keeps one symbol and its call sites carry no
> row: its check runs at state 0 and nothing at the call could elide it. The
> runners hold the belt: every `.body` occurrence in an emission is its own
> define, the wrapper's tail call, or the callee of a direct call, and the
> direct calls equal the discharged rows.

The verdict column is `discharged` (unsat), `open` (sat — a counterexample
exists under the encoding's hypotheses; not a refutation of the program, a
guard that stays), `budget` (unknown under the pinned `rlimit`), `unencoded`
(a site the encoder could not express — listed so the manifest is an
INVENTORY of guards), or `checker` (discharged by the frontend). The elision
column is `elided`, `retained`, or `none` for a kind with no guard.

## 8. The SMT elimination manifest

> **The schema is D-218's since 1.5.0 (P-10 in `meta/roadmap/1.5/1.5.0.md`):**
> the file is `nitpick.obligations` at the manifest root, committed, written
> only by `npkg verify --record` (D-040's "generated when absent" row is
> amended: a file that governs the artifact is written on purpose or not at
> all); its header carries the pinned z3 and the profile; each row is
> `<sha256> <kind> <tier> <verdict> <elision> <symbol>` — the hash over the
> obligation's canonical SMT text plus the module-qualified symbol and the
> kind (D-218.8), the kind from §7b, the tier the theory that decided it
> (`int` in 1.5.0), the verdict `discharged`/`open`/`budget`/`unencoded`/
> `checker`, the elision `elided`/`retained`/`none`. Rows sort by symbol then
> hash. The v1 sketch below is superseded; its rule — divergence is
> detectable and fatal, never silent — is unchanged.

> **The elision word by ROLE since 1.5.3 (L-13):** the compiler's
> `rows.txt` (`--obligations`) names each row's site (`space:index`), its
> ROLE — `guard` (a check in the row's own function), `bypass` (a call-site
> row), `held` (a call-site row whose guard is the callee's own entry: a
> coroutine's state 0, a `dyn` call's vtable target), `conform` (no guard) —
> the GROUP of rows one guard shares (a loop's three rows, a function's entry
> row, a call's bypass rows), and the TRAPS that guard keeps while any row of
> the group is retained. A `guard` or `bypass` row reads `elided` when
> discharged and `retained` otherwise; a `held` row reads `retained` whatever
> its verdict (D-268); a `conform` row and a guard-less kind read `none`.
> Both runners derive the word from the row, never from the kind alone.

`--smt-opt` is the only verification flag that changes generated code: where Z3
**proves** a runtime check unnecessary, the check is removed; where it cannot
prove it, the check stays and runs at runtime. Proof can only ever remove
something provably redundant.

That creates a reproducibility hazard. `--smt-timeout` defaults to 5000 ms, so a
proof succeeding on one machine and timing out on another would emit **different
binaries from identical sources**. For certification, where the artifact analysed
must be the artifact shipped, that is not acceptable.

**Every elision is therefore recorded in a manifest, and the manifest is
authoritative on subsequent builds** (D-040):

```
# nitpick-smt-manifest v1
# compiler: 0.1.0   z3: 4.13.0   target: x86_64-linux   timeout-ms: 5000
a3f1…  npk_parse_expr   bounds     discharged
b7c2…  npk_hash_mix     overflow   discharged
```

| Situation | Outcome |
|---|---|
| manifest matches exactly | build proceeds, binary reproducible |
| Z3 proves **more** than recorded | **build fails** |
| Z3 proves **less** than recorded | **build fails** |
| no manifest | generated; build marked *not reproducibility-verified* |

Obligations are identified by a **hash of their normalised SMT-LIB2 form**, not by
source location — line numbers shift with any edit above them, while the
obligation itself does not.

This does not make Z3 deterministic. It makes divergence **detectable and fatal**
rather than silent, which is the achievable guarantee.

It is also an **audit artifact**: the manifest is evidence that every removed
check had a proof, reproducible on demand. Certification runs may record full
proof certificates (unsat cores) rather than outcomes alone.

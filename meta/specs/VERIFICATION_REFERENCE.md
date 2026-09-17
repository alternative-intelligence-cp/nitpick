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

> **Live since 1.5.4 (L-17, L-18; `meta/roadmap/1.5/1.5.4.md`).** The
> proposition must FOLD to a constant `bool` through 0.6's evaluator: one
> that reads a value the evaluator cannot see is refused
> (`NITPICK-TYPE-069` — `prove` is the claim about a run-time value), and
> one that folds to `false` halts compilation under the same code. In a
> `comptime` body the evaluator folds it per call, and a false `prove` there
> is a counterexample reported once. The statement lowers to nothing; its
> row in the manifest is the catalogue's `checker` entry (§7b, §8) — an
> inventory line, no query, no guard.

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

> **Live since 1.5.4 (L-1…L-16; S-45/D-269, S-48/D-272; `meta/roadmap/1.5/1.5.4.md`).**
> THE PATH CONDITIONS the encoder accumulates, for every obligation and not
> only a `prove`'s: an `if`'s condition inside its then-arm and its negation
> inside the else-arm; after an arm that never falls through (a syntactic
> terminator — `pass`, `fail`, `return`, `exit`, `trap`, `break`, `continue`,
> `give` — or a block, `if` or `pick` made only of those) the other arm's
> condition; a `pick` arm's pattern (a value, a range, a wildcard as the
> negation of the arms before it), the negations of the earlier arms and its
> `where` guard — none when a `fall` sits in the pick, only the pattern when
> any arm carries a guard; a loop's negated condition after a loop nothing
> `break`s out of; `when`'s `then` and `end` on whether the body ran; the
> right-hand side of `&&`/`||` and a ternary's branches under theirs. An
> arm's facts survive it guarded by its condition, so the versions after an
> `if`, a `when` or a `pick` MERGE as `(ite c v_then v_else)` and a pick
> expression's value is the chain of its arms' `give` terms (L-7…L-10). A
> counted loop's `$` and a range `for`'s binding are terms with their bounds
> (§4).
>
> **`prove(e)` is a row of kind `prove`** — guard-less, its proposition over
> those hypotheses, walked quiet (nothing executes, so a division inside it
> is not a site) — and, once discharged, **knowledge for every site after
> it** (S-48, D-272): a proof outline in source. The plain build lowers the
> statement to nothing and claims nothing. **The VERIFIED build refuses an
> unproven claim** (S-45, D-269): under `--elide`, a `prove` whose row the manifest
> does not discharge — `open`, `budget`, `unencoded`, or absent — is
> `NITPICK-VERIFY-001` at the statement, naming the row's word; a verified
> artifact carries no unproven claim, and `npkg verify --explain` writes the
> model of an open one. A `bool` proposition always has at least an opaque
> term, so a `prove` row is `open` rather than `unencoded` when the encoder
> cannot express it. The `--verify` and `--prove-report` flags above are the
> pre-D-219 spelling: the verified build is `npkg verify`'s, and the report
> is `--explain`'s.

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
> included; a subject outside the encoder's fragment (§7c: since 1.5.4b every
> scalar family is inside it -- the twisted kinds by D-278, the floats by
> D-281, a `simd` per lane by D-282 -- and a string, a struct or an array is
> not) is an `unencoded` row whose guard stays. Every direct call of a sync callee with
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

func:update = int32(limit<EvenIdx> int32:i, limit<OddIdx> int32:j, int32[8]:arr) never fails {
    int32->:a = $$m arr[i => int64];   // an exclusive claim at an even index
    int32->:b = $$m arr[j => int64];   // z3 proves i != j: the `disjoint` row discharges
    <-a = 7i32;
    <-b = 9i32;
    pass ((<-a) + (<-b));
};
```

> **Landed at 1.5.5 (D-286, D-287; `meta/roadmap/1.5/1.5.5.md`).** The
> example above is in the OPERATOR form, which is the language's: `$$i` and
> `$$m` are unary operators yielding a pointer (`T->`), and the
> declaration-qualifier spelling the prototype text carried (`$$m int32:a =
> arr[i];`, AST_REFERENCE's `borrow_imm`/`borrow_mut`) never existed in
> this compiler. THE RULES. A `$$i place` is a SHARED claim (many readers),
> a `$$m place` an EXCLUSIVE one (one writer, no other name); `@place` is a
> plain address that claims nothing and says nothing about direction — and
> counts as a write-capable access, since the pointer type carries no
> mutability. A claim's lifetime is LEXICAL: a whole call argument or
> receiver lives for that call and conflicts with the call's other arguments
> and receiver; the whole initialiser or assigned value of a pointer local is
> held by that local from its declaration to the end of the block that
> declares it; a `defer` body sees every claim of its enclosing blocks.
> Non-lexical lifetimes and two-phase borrows are decided OUT (D-004's "no
> lifetime inference"). A claim stands only as a whole call argument or a
> pointer local's whole value, and a holder is used, not copied: any other
> position — a literal, a `pass` value, a nested expression, a non-local
> assignment target, an argument of a call whose result can carry a pointer
> (D-117 rule A), a place with no named root — is `NITPICK-BORROW-014`, and
> the fix is to spell `@` for an address that claims nothing. Every access
> whose root a live claim covers is classified by the paths' common prefix
> (a field that differs or two unequal numerals: disjoint; nothing computed:
> statically overlapping; else computed) and by the table — a read or a
> write-capable access under `$$m`, a write-capable access under `$$i`, a
> claim on storage a held `@` reaches, a write through a shared claim's
> holder, a call's arguments among themselves: a STATIC conflict is
> `NITPICK-BORROW-013` naming the claim, its site and its lifetime. A
> COMPUTED conflict is a RUNTIME GUARD in every build (D-068's shape): at
> the access the emitter compares the storage it names and every party's
> as BYTE RANGES — `p < q+size_q && q < p+size_p` on `ptr` operands, exact
> for elements, rows and fields, with no captured index — and traps
> `BorrowOverlap` (−4116, an identity the reach analysis arms) when they
> intersect; the obligation is the `disjoint` row (§7b, kind 20): the `and`
> over the parties of the `or` over the computed index pairs of `(not (=
> tP tQ))`, the party's index terms captured when its claim was encoded,
> one row per site, its fact a hypothesis after the site; a discharged row
> removes the compare (no `llvm.assume` over pointers). In the example the
> two rules make `(not (= i j))` `unsat` under the Int forms of `%`
> (1.5.4b), so the verified build carries no compare where the plain build
> carried one; `disjoint_open.npk` is the same shape without the rules —
> `open`, the compare kept. THE LIMIT, stated: exclusivity is decided among
> accesses that spell the SAME ROOT; two pointer bindings that alias one
> storage are two roots, and a claim through one is not seen from the other
> — a syntactic path analysis cannot know two names alias, and making every
> pointer parameter exclusive would refuse the compiler's own architecture.
> Beside it, D-287: a `fixed` binding has no address (`NITPICK-TYPE-071` for
> `@`, `$$i`, `$$m` and the implicit pointer-receiver address, a `fixed`
> field included) — a write through such a pointer was one no rule saw, and
> through a `fixed` module binding it was a SIGSEGV.

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
> visit) AND the condition's negation (1.5.4, L-4).
>
> **The counters are terms (1.5.4 step 3, L-11, L-12).** A counted loop's
> `$`: its start at the entry row; in the body a fresh symbol inside `start
> <= $ < limit` (descending, `limit < $ <= start`) and, for a numeral step,
> in its residue class; the incremented value at the preservation and
> `continue` rows (the head sees it); after a loop nothing `break`s out of,
> the exit value — past the limit by less than a numeral step, past the
> start. A range `for`'s binding likewise, its bounds captured at entry
> (D-234) and its post-loop version exactly at its bound. The bounds and the
> step are captured as the emitter's slots hold them, so a body that assigns
> a name a bound mentions moves nothing. A COMPUTED step's positivity is the
> `loop-step` row (S-46, D-270): the compare at the loop's entry is its guard,
> elided when discharged; a literal step is the checker's (TYPE-068, D-022)
> and has neither. An invariant naming `$` or the binding is decided on the
> merits now, where until 1.5.4 it was `open` because the counter was opaque.

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
| `div-zero` | the divisor of an integer `/` or `%` is not zero (D-007, D-142); a `simd` division's any-lane guard is ONE row over the lanes' conjunction (D-282) | yes | 1.5.0 |
| `div-min` | a signed division is not `INT_MIN / -1` (D-142); one row over the lanes for a signed-element `simd` (D-282) | yes | 1.5.0 |
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
| `err-exit` | the `TbbErr` guard's condition (D-144 as amended, D-278): neither operand is ERR at a comparison on a twisted value, the operand is not ERR at a cast out of its family (both spellings), a checked crossing into or within a family lands in the target's range; a twisted division has no row (a zero divisor is ERR) | yes | 1.5.4b |
| `failsafe-post` | `failsafe` returns a positive value (D-014) | yes | 1.5.3 |
| `loop-step` | a counted loop's computed step is positive (D-022): the compare at the loop's entry, `BadStep`; a literal step is the checker's (TYPE-068) and has no row | yes | 1.5.4 |
| `shift-range` | a shift's COMPUTED amount is inside `0..width-1` (D-277): the compare before the shift, `ShiftRange`; a known amount is the checker's (TYPE-070) and has no row; a `simd` shift's any-lane guard is one row over the lanes (D-282) | yes | 1.5.4b |
| `prove` | a `prove(...)` holds under its path conditions | no | 1.5.4 |
| `assert-static` | an `assert_static(...)` folds to true (the frontend) | no | 1.5.4 |
| `disjoint` | two accesses of one root through computed indices name disjoint storage while a `$$i`/`$$m` claim is live (D-286): the byte-range compare at the second access, `BorrowOverlap`; a static overlap is the aliasing analysis's (BORROW-013) and has no row | yes | 1.5.5 |
| `floor-spec` | a clause of a floor symbol's section in `runtime/npkrt.spec` holds of the symbol's IR (D-288, §9): an `ensures` on the returning, non-trapping paths, the trap outcome, the frame, a loop invariant established at entry and preserved by the body, an unrolled loop's bound where the spec claims it exact, a summary callee's `requires` at the call; rows from the floor writer (1.5.6), never the compiler, in `runtime/npkrt.obligations` | no | 1.5.6 |
| `floor-model` | a bounded protocol model's bad predicate is unreachable within its depth and preemption bound (D-289, §9); rows from the floor writer over `runtime/models/`, in `runtime/npkrt.obligations` | no | 1.5.6 |
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

> **[1.5.4 (2026-09-06).]** Three more kinds produce rows. `exhaustive` (one
> per `pick`, both spellings) and `assert-static` (one per statement) carry
> the verdict `checker`: the 0.5 exhaustiveness analysis and the frontend's
> fold decided them, so the row is an inventory line — `c` in `rows.txt`, no
> query, tier `-`, word `none`. `prove` is decided by z3 like a guarded kind
> and is the ONE kind whose non-discharge refuses the verified build (S-45, D-269),
> since it has no guard to retain. `loop-step` joined the table at step 3
> (S-46, D-270): a computed step's compare, a literal step being the checker's.

> **[D-278, 1.5.4b step 2 (2026-09-10).]** `err-exit` produces rows, and the
> twisted kinds are terms. A `tbb`/`tfp`/`dim256`/`trit`/`tryte`/`nit`/`nyte`
> value is an Int in the carrier's range whose ERR is the carrier's most
> negative value — a VALUE the terms carry (D-008): `ERR` and `is_err(x)` are
> `MIN` and `(= x MIN)`, a symbol's axiom is "ERR, or inside the valid range"
> (`MIN+1 ..= MAX`; the balanced `-B ..= B` for the ternary kinds), a literal
> its carrier value (a `tfp`'s exact Q value), and every operation the
> emitter's own `ite`: saturate-to-ERR on `+ - *` and negation, the `tfp`
> floor multiply `(div (* a b) 2^F)` and truncating divide `(npk_sdiv (* a
> 2^F) b)` narrowed by the range test, a zero divisor ERR, the ternary
> digits' `&`/`|` as min/max; `dim256` is `tfp256`. The row is the `TbbErr`
> guard's condition, one per `-4100` site: neither operand ERR at a
> comparison, the operand not ERR at a cast out of its family (both
> spellings), the value in the target's range at a checked crossing into or
> within a family; its fact is a hypothesis after the site (every continuing
> path passed the trap), and a discharged row's guard is one `llvm.assume`.
> A `frac` or a tfp-element `complex` guard is a row over an aggregate the
> walk has no term for — `unencoded`, its trap kept — and so is a checked
> crossing from a float until the floats' step gives the float a term. A
> twisted division has no row: it never traps. A `limit` over a twisted
> subject encodes (its `$` the Int term), and a `Rules` body's own walk reads
> `$` as the subject and each clause as a fact for the next (the predicate
> traps on the first false clause). A guard's fact is never pushed under a
> QUIET encoding — a contract clause at a call site, a rule instantiation —
> where its row is not recorded either, nor from inside a contract clause
> encoded for its own row (a `requires` at an entry, an `ensures` at a seam,
> an `invariant` at a head): the check that runs the guard is what that
> row's discharge removes (DEF-33).

> **[D-281, 1.5.4b step 3 (2026-09-10).]** FLOATS ARE TERMS IN TWO TIERS, and
> the manifest's tier column is fed by the encoder. Tier 1: a `flt32`/`flt64`
> value is a term of the IEEE sort (`(_ FloatingPoint 8 24)` / `(_
> FloatingPoint 11 53)`) and every operation is the emitter's instruction
> under SMT-LIB's IEEE semantics — `fp.add`/`sub`/`mul`/`div` under RNE,
> `fp.neg`, `#sqrt` as `fp.sqrt RNE`, the ORDERED predicates the emitter's
> `fcmp` writes (`==` is `fp.eq`, `!=` its negation, so NaN compares as the
> machine does), a literal `to_fp RNE` of the exact rational its decimal
> text denotes (a `flt32` literal rounded twice, as the emitter's double-then-
> `fptrunc` road does), an integer entering `to_fp RNE (to_real x)`, a
> widening exact, a narrowing `=>!` rounded; `%` (`frem`) and a float LEAVING
> to an integer stay opaque (1.5.8's `cast-range`); `flt128` is storage
> (D-143) and has no term. Every float value is NAMED and its definition
> recorded, so the twin below reads a flat list. Floats never trap: no row
> is theirs — what the terms buy is that a `limit`, a contract, an
> `invariant`, a `prove` and a `TbbErr` guard over a float entering `tbb` or
> a ternary kind are encoded rows. THE TIER COLUMN: `rows.txt`'s eleventh
> field, `int` for a cone in Int/Bool, `bv` where a bit-vector crossing is in
> it, `fp` where a float sort is, `-` for an unencoded or checker row; both
> runners carry it into the manifest. TIER 2, the Real-interval abstraction
> (D-218 (5)): for every `fp` row the encoder also writes a twin query
> (`NNNN.t2.smt2`, `index.t2.txt` naming its rows) in which every float is a
> Real — an operation a fresh Real within `eps·|v| + eta` of its exact
> result (`2^-53`/`2^-1074`, `2^-24`/`2^-149`), a square root `r >= 0` with
> `r²` inside `v·(1 ∓ eps)²`, negation and widening exact — under THREE
> CONDITIONS, else no twin: (i) every float symbol no hypothesis defines is
> bounded below and above by comparisons against literals (a rule, a
> `requires`, a path condition — false for NaN and, both together, for the
> infinities); (ii) every operation's magnitude within the normal range, a
> divisor nonzero, a root's argument non-negative, CONJOINED to the goal, so
> `unsat` proves no overflow, infinity or NaN arises along with the property;
> (iii) the goal a comparison or a Boolean combination of comparisons — an
> `fp.eq`, with its NaN reading, stays tier 1. The runner asks tier 1 first
> and, for a `budget` row with a twin, the twin once under the same profile:
> `unsat` discharges it and the tier reads `real`; a tier-1 `sat` is a
> countermodel and is never retried. `flt_tier2.npk` is the shape D-218 (5)
> was written for: `#sqrt(a*a + b*b) >= 0.0` under bounded `a`, `b` —
> `unknown` in QF_FP at the rlimit, `unsat` in the twin in milliseconds.

> **[D-282, 1.5.4b step 4 (2026-09-10).]** A `simd<T, N>` value is N scalar
> terms, and its any-lane guards are one row each. The lanes ride EXPRESSIONS
> (a side table by expression, built by the walk: the constructor's arguments,
> a splat's one term N times, an elementwise operation lane-wise under the
> scalar rule of the element's kind — a word's arithmetic, bitwise and
> compares, a float's IEEE operations, a `bool` lane's `==`/`!=`/`&`/`|`/
> `^` — `[i]` with a numeral index the lane's term, `.len` the count,
> `.any()`/`.all()` the disjunction/conjunction, `sum`/`min`/`max` folded in
> the emitter's order with its `select` over the strict compare, an
> elementwise cast the scalar cast per lane) AND BINDINGS: a `simd` local's
> lanes are N symbols of the element type, a new set at every write, defined
> equal to the written value's lanes where those are known, and fresh and
> opaque at every invalidation, restore and merge — the conservative answer
> wherever a path could differ. Anything else (a call's value, a computed
> index) is N opaque lanes. The vector itself has no scalar term. THE ROWS: a
> `simd` division's any-lane guard is ONE `div-zero` row over the conjunction
> of the lanes' conditions and, for a signed element, one `div-min` row
> likewise; a `simd` shift's any-lane guard one `shift-range` row over its
> conjunction — the emitter's one trap per site, one group — its fact a
> hypothesis as a scalar shift's is. The `unencoded` producers of 1.5.0 and
> step 0 (a `simd` division, a `simd` shift) retire; the verdict's remaining
> producers are a `limit` over a subject no theory covers (a string, a struct,
> an array) and the `TbbErr` guards over a `frac` or a tfp-`complex`.

The verdict column is `discharged` (unsat), `open` (sat — a counterexample
exists under the encoding's hypotheses; not a refutation of the program, a
guard that stays), `budget` (unknown under the pinned `rlimit`), `unencoded`
(a site the encoder could not express — listed so the manifest is an
INVENTORY of guards), or `checker` (discharged by the frontend). The elision
column is `elided`, `retained`, or `none` for a kind with no guard.

## 7c. The theories (D-218 (4) and (5); landed 1.5.4b, 2026-09-10)

What a value IS to the solver, family by family — the partition D-218 (4)
and (5) ratified, as landed (D-277…D-282). One rule stands above every
family: **a proposition holds only where its evaluation does not trap**
(DEF-33, 1.5.4b step 2). Every guard met inside a contract clause, an
invariant conjunct, a rule's clause or a `prove` — a shift's amount, a
division's pair, a twisted compare's or cast's operands, a called callee's
own entry conditions, an opaque stand-in for a guard the walk has no term
for — is CONJOINED into the proposition's term and pushed as a hypothesis
nowhere, so the row over the proposition proves its guards pass, eliding its
check removes no live guard, and the proposition as a hypothesis (the
callee's precondition in its body, a rule after a write, a proven claim)
carries them. An encoder that pushes a guard's fact from inside a clause
proves the clause from itself; measured twice before the rule was written.

**Plain integers: unbounded `Int` with range axioms (1.5.0).** Every
`intN`/`uintN` symbol is an `Int` with the axiom of its range; `+ - *` are
the Int operations (D-210's overflow rows are 1.5.8's), `/` and `%` the
truncating `npk_sdiv`/`npk_srem` as the machine's, with the D-007 pair as
rows. A `bool` is `Bool`, a pattern's literal is read under the selector's
type, a path condition is a hypothesis in its arm (§7b's 1.5.4 note).

**A shift's amount (D-277; step 0).** `x << n` and `x >> n` are defined for
`0 ≤ n < width(x)` and nothing else, in every evaluator the language has: a
known amount outside the range is `NITPICK-TYPE-070` at the shift (both
operators, both spellings, the folder's bound the type's width); a computed
amount is one unsigned compare on its carrier (`n <u W`, a negative amount
reading as huge) trapping `ShiftRange` (−4115), the `shift-range` row's goal
`(and (>= n 0) (< n W))` and, discharged, one `llvm.assume`. The goal is a
hypothesis after the site either way — what lets a shift by a bounded
amount cross exactly below. Masking and saturating were rejected: each
silently performs a different shift than the author wrote.

**Bitwise operations: the Int forms first, the bit-vector crossing bounded
(D-279, D-280; step 1).** Wherever an operand is a numeral the encoder knows
— a literal, a folded expression, a `fixed` global's constant (so a flag
member or a named mask) — the operation is Int arithmetic and no theory is
crossed: `x << k` is `(mod (* x 2^k) 2^W)` re-signed by the machine's wrap
for a signed word, `x >> k` is `(div x 2^k)` (SMT-LIB's floor division by a
positive numeral IS `ashr` on a signed value and `lshr` on an unsigned one),
`x & (2^j − 1)` is `(mod x 2^j)`, `x & 2^j` is `(* (mod (div x 2^j) 2)
2^j)`, `x & ~(2^j − 1)` is `(- x (mod x 2^j))`, `~x` is `(- (- x) 1)`
signed and `(- (2^W − 1) x)` unsigned — any width, 4096 bits included,
at 504–1,287 rlimit per row. Every other shape crosses at a word of at
most `BV_CROSS_MAX_BITS = 64` bits (`smt_encode.npk`, the measurements
beside the constant): `(bv2nat (bvop ((_ int2bv W) a) ((_ int2bv W) b)))`
captured in a fresh symbol and re-signed for a signed word — `int2bv` of a
negative Int is its two's-complement pattern by definition, so the crossing
is exact — and `bvshl`/`bvlshr`/`bvashr` for a shift by a non-numeral
amount under the `shift-range` hypothesis `0 ≤ n < W`. Above 64 bits a
general operation stays opaque, safe and unproven, because the crossing
measured 191 rlimit at 32 and 64 bits, 883,930 at 128 (4% of the budget),
3,591,364 at 256 (18%) and the budget exhausted at 2048, while the Int
forms cover the wide widths' real shapes (the `ToString` tables' masked
divisors, the hash rotations); a bit test with an OPAQUE mask read by an
Int inequality exhausts the budget even at 32 bits. Moving the constant is
a measurement, not a decision. A function with a crossing emits `(set-logic
ALL)`; one without emits the file it emitted before, so no untouched row's
hash moves. A flag family (D-230) is an unsigned 32-bit word to the
encoder: its symbols carry `[0, 2^32)`, `|`/`&` cross at 32 bits, `==`/`!=`
compare, and `int32 =>! oflags` / `oflags =>! int32` re-sign the one bit
pattern each way — the first arm the unchecked cast has; every other `=>!`
is opaque until 1.5.8's `cast-range` rows. THE GATE (D-280): every row
discharged before a crossing is discharged after it — a term newly in a
cone can only add facts, so a `budget` where a `discharged` was is a
regression the step reverts; measured by (symbol, kind, verdict) counts over
the re-recorded manifest, it held at every step of 1.5.4b.

**The twisted kinds: scaled `Int` with ERR a value (D-278; step 2).** A
`tbb`/`tfp`/`dim256`/`trit`/`tryte`/`nit`/`nyte` value is an `Int` in the
carrier's range whose ERR is the carrier's most negative value — a VALUE
the terms carry (D-008), never a side condition: `ERR` is `MIN`, `is_err(x)`
is `(= x MIN)`, a symbol's axiom is "ERR, or inside the valid range"
(`MIN+1 ..= MAX`; the balanced `−B ..= B` for the ternary kinds), a literal
its carrier value (a `tfp`'s exact Q value through the checker's and the
emitter's one conversion), and every operation the emitter's own `ite` in
the emitter's own shape: `+ - *` and negation saturate to ERR outside the
valid range (a result landing ON the sentinel is ERR by the same test),
`tfp`'s `*` is `(div (* a b) 2^F)` and its `/` `(npk_sdiv (* a 2^F) b)`,
each narrowed by the range test, a zero divisor is ERR, `/` and `%` at the
same scale are the `ite` over the taint and a zero divisor to `MIN` else
the truncating quotient or remainder, the ternary digits' `&`/`|` are the
Kleene min/max, `dim256` is `tfp256`. Each raw result is named once
WITHOUT an axiom before the test reads it — the raw sum may lie outside the
type's range, which is what the test decides, so a typed symbol's axiom
would be a false hypothesis. The rows are `err-exit`'s (§7b): one per
`TbbErr` guard, elided through the one guard shape. A twisted division has
no row: it never traps. A `limit` over a twisted subject encodes, and a
`Rules` body's own walk reads `$` as the subject with each clause a fact
for the next (the predicate traps on the first false clause). A `frac` or a
tfp-element `complex` is an aggregate the walk has no term for: its guards
are `unencoded`, their traps kept.

**Floats: two tiers (D-281; step 3).** TIER 1: a `flt32`/`flt64` value is
a term of the IEEE sort (`(_ FloatingPoint 8 24)` / `(_ FloatingPoint 11
53)`) with NO range axiom — NaN and the infinities are values of it, and a
goal's own conditions say otherwise where they do — and every operation is
the emitter's instruction under SMT-LIB's IEEE semantics: `fp.add`/`sub`/
`mul`/`div` under RNE, `fp.neg`, `#sqrt` as `fp.sqrt RNE`, the ORDERED
predicates the emitter's `fcmp` writes (`==` is `fp.eq`, `!=` its negation,
so NaN compares as the machine does), a literal `to_fp RNE` of the exact
rational its decimal text denotes (what LLVM's own parser rounds from, so
the two agree by correct rounding; a `flt32` literal rounded twice, as the
emitter's double-then-`fptrunc` road does), an integer entering `to_fp RNE
(to_real x)`, a widening exact, a narrowing `=>!` rounded; `%` (`frem`, a
truncated fmod, not IEEE's `fp.rem`) and a float LEAVING to an integer stay
opaque (1.5.8's `cast-range`); `flt128` is storage (D-143) and has no term.
Every float value is NAMED — a fresh symbol defined equal to the operation,
its definition recorded by shape — so the twin below reads a flat list.
Floats never trap (D-007): no row is theirs; what tier 1 buys is that a
`limit`, a contract clause, an `invariant`, a `prove` and the `err-exit` row
of a float entering `tbb` or a ternary kind are encoded rows. Measured: a
bounded quotient's `prove` discharges in QF_FP in 3.9 s under the profile.
TIER 2, the Real-interval abstraction: for every row whose cone holds a
float the encoder ALSO writes a twin query — `NNNN.t2.smt2` beside the
tier-1 file, `index.t2.txt` naming its rows — in which every float is a
Real: an input symbol a free Real, a named operation a fresh Real `r` with
the axiom `|r − v| ≤ ε·|v| + η`, `v` the exact Real of the operation over
its operands' Reals (ε = 2^−53 / 2^−24, η = 2^−1074 / 2^−149 — the standard
model of a correctly rounded operation, the subnormals under η; a literal an
operation over its rational, so its own rounding is in the model), a square
root `r ≥ 0` with `r²` inside `v·(1 ∓ ε)²`, negation and widening exact, a
comparison the Real comparison, the Int fragment passed through verbatim.
The twin is written at all only under THREE CONDITIONS, else the row stays
`budget`: (i) every float symbol in the cone that no hypothesis defines is
bounded below AND above by hypotheses that are float comparisons against
literals among a hypothesis's top-level conjuncts — a `limit` rule, a
`requires` clause, a path condition; a negated comparison is not a bound,
since it holds of NaN, and both bounds together exclude the infinities, so
"the inputs are finite reals" is the hypotheses' own premise; (ii) every
operation's magnitude within the normal range (`|v| ≤ MAX_NORMAL` per
definition), a divisor nonzero and a root's argument non-negative, CONJOINED
to the goal — so an `unsat` proves no overflow, no infinity and no NaN arise
along with the property; (iii) the goal a comparison or a Boolean
combination of comparisons over symbols and the Int fragment — an `fp.eq`,
with its NaN reading, stays tier 1 only, and an uninterpreted function in
the cone excludes the row. The runners ask tier 1 first; for a `budget` row
with a twin, the twin once under the same profile and wall-clock net —
`unsat` discharges it and the tier reads `real`, a tier-1 `sat` is a
countermodel in the real semantics and is never retried, `--explain` names
the tier that decided. Measured: `#sqrt(a*a + b*b) >= 0.0` under bounded
`a`, `b` is `unknown` in QF_FP at the rlimit and `unsat` in the twin
(`flt_tier2.npk`, the shape D-218 (5) was written for).

**`simd` lanes (D-282; step 4).** A `simd<T, N>` value is N scalar terms
under the element's theory above and no term of its own — a side table by
expression: the constructor's arguments, a splat's one term N times, an
elementwise operation lane-wise (a word's arithmetic, bitwise and compares;
a float's IEEE operations; a `bool` lane's `==`/`!=`/`&`/`|`/`^`), `[i]`
with a numeral index the lane's term (a computed index opaque; its `bounds`
row is 1.5.8's), `.len` the count, `.any()`/`.all()` the disjunction and
conjunction, `sum`/`min`/`max` folded in the emitter's order (`sum` left to
right; `min`/`max` as its `select` over the strict compare, `fcmp olt`/`ogt`
on floats — false on NaN, so a NaN lane is passed over as the machine does;
a float sum named lane by lane for the twin), an elementwise cast the
scalar cast per lane — AND through bindings: a `simd` local's lanes are N
symbols of the element type, a new set at every write, defined equal to the
written value's lanes where those are known, and fresh and opaque at every
invalidation, restore and merge (the reading recorded as S-58, since D-282's
text named expressions). Anything else — a call's value, an escaped binding
— is N opaque lanes. A `simd` division's any-lane guard is ONE `div-zero`
row over the conjunction of the lanes' conditions and, for a signed element,
one `div-min` row; a `simd` shift's any-lane guard one `shift-range` row —
the emitter's one trap per site, one group — each fact a hypothesis after
the site as a scalar's is — and a discharged any-lane row elides its guard
into one `llvm.assume` of the negated any-lane test, the scalar guards'
shape. (Step 4's first harness found the vector guards ignorant of the
manifest — rows discharged, traps kept, six `assume`s for seven discharged
sites — and the elision belts are the instrument that caught it: a check of
rows and verdicts alone had been green.)

**The tier column (D-281).** `rows.txt`'s eleventh field, read off the
row's canonical text by the encoder and carried into the manifest's third
column by both runners: `int` for a cone in Int/Bool alone, `bv` where a
bit-vector crossing (`int2bv`) is in it, `fp` where a float sort or
operation is, `-` for an `unencoded` or `checker` row; `real` is written by
the runner for a row tier 2 discharged. Until 1.5.4b the column was the
constant `int`.

**What is still outside the fragment.** A `limit` over a string, a struct
or an array (P-12's residue) and the `TbbErr` guards over a `frac` or a
tfp-element `complex` are `unencoded`, their guards kept; `frem`, a float
leaving to an integer and every other `=>!` are opaque until 1.5.8's
`cast-range` rows; D-210's overflow rows are 1.5.8's, over the lane
conjunction for a `simd` (whose integer lanes trap as scalars do since
1.5.4e, D-284). The compiler's own
manifest at the close: 368 rows in 197 function files, decided in 3.9 s
under the profile — 329 `int`, 11 `bv` (step 1's crossings), 28 `-`, no
`fp` row, since neither the compiler nor the prelude functions its emission
holds compute in floats or vectors.

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

> **[1.5.4 (2026-09-06).]** `rows.txt`'s fifth column is `1` (a row with a
> `(check-sat)`), `0` (`unencoded`) or `c` (a row the frontend decided:
> verdict `checker`, tier `-`, word `none` — no answer consumed). The
> verified build refuses an undischarged `prove` (`NITPICK-VERIFY-001`): the
> one row whose retention is a refusal, since a `prove` has no guard; a
> verify test names that refusal with `expect-error:` and ends there.

> **[1.5.4b (2026-09-10).]** The tier column is the encoder's word (§7c):
> `int`, `bv`, `fp`, `-`, and `real` written by the runner for a tier-2
> discharge — where a constant `int` stood since 1.5.0. `index.t2.txt`
> beside `index.txt` names the rows that have a Real-interval twin; a row's
> twin is asked only after a `budget` at tier 1, and only its `unsat` moves
> a verdict.

> **[1.5.5 (2026-09-11), D-286.]** `disjoint` produces rows (kind 20, guard
> `yes`, trap `-4116` in both runners' tables): one per guard site of the
> aliasing analysis — an access whose path is computed against a live
> `$$i`/`$$m` claim — with the goal over the index pairs of the paths'
> common prefix; its discharge removes the byte-range compare, and it is
> not an assume kind (`loop-step`'s precedent). A static overlap is the
> analysis's own refusal (BORROW-013) and has no row.

> **[1.5.4e (2026-09-11), D-285.]** The belts count a guard's trap by its
> text, `@npk_trap(i32 CODE)`; a PROGRAM's raise — `?!`, `!!!` — is
> `@npk_raise(i32 CODE)`, one call of `npk_trap` under its own name, which
> the belts do not count. So a verified build of a program that unwraps
> with a system code (`?! DivByZero`) no longer reads as a guard (DEF-36),
> and both runners' self-checks hold the pair.

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

---

## 9. The floor's obligations (D-288, D-289; landed 1.5.6)

The runtime floor (`runtime/npkrt.ll`, hand-written LLVM IR, permanent under
D-203) is specified beside itself and decided by the same solver, under the
same profile, through the same deciders as a program's obligations — and its
evidence lives in its own files, never in `nitpick.obligations`:

| File | What it holds |
|---|---|
| `runtime/npkrt.spec` | the specifications, in SMT-LIB2 syntax: one `(symbol @name …)` section per specified define (§9.1), and the `(shared …)` classification of every word two threads can reach (D-290; CONCURRENCY_REFERENCE §5.3) |
| `runtime/models/*.model` | the executor primitives as bounded transition systems, one protocol per file (D-289; lands at 1.5.6 step 5) |
| `runtime/npkrt.obligations` | the floor's manifest, in the `nitpick.obligations v1` format of §8 — written only by `npkg verify --record`, held by both runners exactly as the compiler's manifest is; its rows carry the two floor kinds of §7b (`floor-spec`, `floor-model`), tier the writer's word, elision `none` (nothing in the floor elides: the verified build changes no floor byte) |

**The writer is `npkg/floor_smt.npk`** — the translator — driven by
`npkg/floor.npk` (`floor_emit`, writing `build/verify/floor/` in the
compiler's directory shape: `NNNN.smt2`, `index.txt`, `rows.txt`) with
`npkg/floor_ir.npk` (the floor's text as defines, blocks and lines),
`floor_instr.npk` (each line read into its parts), `floor_cfg.npk` (loops,
the dominance order, inlining and unrolling as text transforms) and
`floor_spec.npk` (the S-expression reader). `tools/floorspec.npk` is the
harness's entry to the same modules (`floorspec ROOT --emit DIR`); the
harness's `verify` stage runs it after the compiler's own obligations, and
its `parity` stage byte-compares the two runners' `rows.txt`. A manifest row
of a kind its file does not carry — a floor kind in `nitpick.obligations`, a
program kind in `runtime/npkrt.obligations` — is refused by name in both
runners.

**The theory is the program's (§7c, D-218 (4)):** every integer or pointer
value an unbounded Int carrying its width's range axiom, read unsigned (the
bit pattern's value), the signed operations over the signed view where they
must; `add`/`sub`/`mul` wrap explicitly (`(mod … 2^w)` — the floor's IR
carries no `nsw`/`nuw`, and its address arithmetic wraps on purpose);
`udiv`/`urem` are `div`/`mod`; an `i1` is a Bool; a float is an IEEE term
(tier `fp`); an aggregate is its fields. The bit operations take D-279's Int
forms with a numeral operand and D-280's crossing at 64 bits or less; an
`or` of two symbols is a fresh symbol carrying every fact that holds of a
bitwise or (`a ≤ o`, `b ≤ o`, `o ≤ a + b`, `o < 2^w`) and the exact reading
`o = a + b` under each side condition the operands' shapes suggest (`a`'s
low k bits clear and `b < 2^k`) — the division loop's low-bit insert reads
as `2r + bit` under the fact that makes it sound, and nothing is assumed.
**Memory is the uninterpreted function `(mem Int) Int`**, a byte per
address with the range axiom `0 ≤ mem(a) < 256` at every application the
translation writes (a constant global's byte is the fact `mem(a) = v`
instead); a `store` a fresh memory function defined pointwise over the
previous (little-endian, one `ite` per byte), a `load` the reassembly
`Σ mem(a+j)·256^j` — FORWARDED through the stores above it (a load at a
store's own address text is the value stored; a store of another size the
`ite` on the two ranges' overlap), and every word-sized load a named symbol
`|ld:N|` shared by every later reader, so a load whose address is another
load composes by name; no array sort, no quantifier, no `Seq` (D-218 (6)).
An `alloca` is a fresh address, a global a fixed symbolic address per name,
the per-thread constants `exec` and `tls` (`npk_exec()`, `npk_tls_self()`)
addresses of their struct's size, and every pair of objects a function
touches is disjoint (a fact of the layout, asserted). **An address computed
by the body wraps once** (`getelementptr`, `add`, `sub`: the floor's IR
carries no `inbounds`), and the wrap is spelled as an `ite` over up to four
atoms (`(ite (< (+ a b) 2^64) (+ a b) (- (+ a b) 2^64))` — every symbol is a
word by its range, so one comparison per possible multiple decides the case
where a `mod` would cost the solver a division's axioms at every address
compared) or as `mod` for anything larger; **every wrap is a named word
`|w:N|`**, one per distinct wrapped text, so an address is an atom wherever
it is read and a wrap never nests inside another's text. Every sum the body
computes carries its LINEAR SHAPE across registers — a root, its symbolic
parts, one numeral offset — so a chain of additions and address computations
is one flat sum wrapped once (`ch + go` then `+ 8` is `(+ ch go 8)`; an
unrolled counter's twentieth value is `(+ j 20)`, never twenty nested wraps;
an offset past half the width is the subtraction of its complement, the
spelling a clause uses), a `getelementptr` is that sum with its numeral
indices folded, and a sum whose root and offset lie inside an object the
section declares, a block a summary callee handed out (`ensures-fresh`) or
the stack stays PLAIN — no wrap can happen there, and the plain sum is the
text a clause writes; a sum's atoms are written in ONE order (lexicographic,
the numeral last) on both sides, so `line + (k + pos)` and a clause's
`(+ line pos k)` are one text. A register defined as a symbol or as a plain
sum READS AS ITS TEXT wherever the body uses it (its `define-fun` stays, for
a clause that names the register), and in the other direction a clause's
composite text that IS some other register's definition — a quotient, a
cleared-low-bits form, a product — reads as that register; a clause's
product of two symbols is the body's `mul`, wrapped as its own word. So the
body's load, store, crossing and table-index memo keys are the clause's own
— `|ld:N|`, `|bv:N|`, `|tv:N|` and `|w:N|` unify by text — and a
validation's `xor64` over the body's address is the clause's one term where
two congruent symbols with distinct bodies cost the solver twenty seconds
and `unknown` (`npk_chunk_guard_check`, `npk_large_check`: 0.1 s once
unified; `npk_arena_free`'s slot address `base + idx·stride`, non-linear
to relate between a `mod` of the sum and a sum of a `mod`, 3 s as one
word). A chain of unsigned divisions by
numerals folds into one divisor, and a division by a power of ten is
RENDERED as the chain of divisions by ten the digit loop computes
(`(div v 100)` is `(div (div v 10) 10)` on both sides): each quotient a
linear relation with a one-digit remainder to the one before it, where
twenty flat quotients of one value related through twenty independent
remainders ran past the budget (`npk_hs_put_dec`: the ten-digit row 4 s →
1.7 s, the nineteen-digit row unanswered → 15 s). **A load of an element of
a constant table** (`[N x iW]` with an
initialiser, one scaled index) reads as the `ite` over the index of the
table's values — a named word (`|tv:N|`), so a clause's `(load64 mem (+
npk_cls_size (* 8 ci)))` and the body's register are one term — and a
division by such a value — or by it plus a numeral —
distributes over the cases, every branch a division by a numeral (linear),
the out-of-table tail a fresh word. Control flow is a path condition per
block (`|pc:B|`), an edge condition per branch, an `ite` per phi at a join
(a phi with one incoming carries the incoming's shape: its folded chain, its
numeral; a phi whose incomings agree is that value), an `ite`-merged memory
at a join; a call to a trap symbol ends its
path with the outcome `trap` and `trap_code`; `result` and `mem2` are merged
over the `ret` blocks.

**The bit-vector crossing is a symbol** (`|bv:N|`, one per distinct crossed
text) whose equation `(= |bv:N| (bv2nat (bvop …)))` is a hypothesis of the
`bv` tier: a row whose goal names no crossing is decided with every crossed
value opaque — its cone keeps the `bv` equations out — so the solver's linear
strategy serves it (with them in, the mixed logic sends a two-line geometry
fact past the budget); a goal that writes `xor64` or names a crossed word is
decided with them (its tier `bv`). **A memory the translation cannot define
pointwise is a fresh function with a TEMPLATE** — what a summary callee's
frame, an allocation, a memory intrinsic or a syscall promises of it at any
point `|pt|`: unchanged outside the callee's ranges, the caller's objects
kept, a mapping's bytes zero — instantiated after the translation at every
point the file applies that function to (a definition, a hypothesis, a
goal), and at every point that reaches it: an instance applies the previous
memory at the same point, and a pointwise definition applied at a point
applies every memory its body applies there (a store's bound variable is no
point). A loop invariant over an `Addr` free symbol is instantiated the same
way, at every point the loop's memory is applied to — rendered at the point
(only the conjuncts that mention the symbol: the rest stands once, in the
header's own hypothesis), so its loads forward and its applications carry
their range axioms.

### 9.1 The spec file's grammar

One `(symbol @name …)` section per specified define. Names: a parameter by
its IR name without `%` (`dst`, `n`; an aggregate parameter's fields as
`a.0`, `a.1`, …); a register likewise (a loop's header phi in an invariant,
a value on a path in an `ensures`); `result` (an aggregate's members
`result.0`, …); `mem` and `mem2`, the memory at entry and at exit (at a loop
header, `mem2` is the memory THERE); `trap` and `trap_code`; `exec` and
`tls`, the per-thread constants; a global by its name without `@` as its
ADDRESS, its contents read through `mem`; `(free j Int)` a free symbol of
the section and `(free x Addr)` one that ranges over the address space —
the universal an invariant's frame conjunct or the frame row states is over
addresses, and the free `x` is the frame row's point when the section
declares one; `(load8 M a)`, `(load16 M a)`, `(load32 M a)`, `(load64 M a)`
the reassembly of the bytes at `a` in memory `M`, `a` wrapped when it is
computed (`(+ exec 48)`) and as it is when it is a symbol or a loaded word
— the two sides of a claim match term for term; `(xor64 a b)`, `(and64 a
b)`, `(or64 a b)` the 64-bit bit operations, a computed operand wrapped
likewise, `and64` with a mask in the body's own Int forms; `(s8 x)`, `(s32
x)`, `(s64 x)`, `(s128 x)` the signed view; `div` and `mod` distribute over
a table value's cases as the body's do. A name SMT-LIB owns (`abs`, `mod`,
`store`, …) or the writer uses is spelled `|r:name|`. Addresses wrap at 2^64
exactly as the IR's do, so a symbol that walks a range states the range is
in the address space — `(objects (p n))`, or `(<= (+ p n)
18446744073709551616)` as a `requires` where a weaker condition than
disjointness is the true precondition (memcpy's forward copy).

| Clause | Rows |
|---|---|
| `(requires P)` | none — a hypothesis over the entry state the floor's callers keep (the emitter's call discipline is the program encoder's business under D-201's table); recorded in the row's cone; at a `(summary)` call, a row |
| `(objects (lo len) …)` | none here — the caller's objects: each range lies in the address space (`0 ≤ lo`, `lo + len ≤ 2^64`) and the listed ranges are pairwise disjoint (a null or an empty range is no object and overlaps nothing — a list's tail may be null), hypotheses of the section and rows at a summary call; the ranges an allocation inside the section is disjoint from and a callee's `objects` frame keeps |
| `(ensures P)` | one row per clause: `P` on the returning, non-trapping paths (`|pc:exit| ∧ ¬trap ⇒ P`) — the exit edge's condition is what a postcondition is decided under |
| `(ensures-trap P)` | one row: `trap ⇔ P`; a symbol with trap sites and no such clause gets the row `¬trap` (it claims never to trap) |
| `(frame (lo len) …)` | one row: for the free symbol `x` (an `Addr`; the writer's own `|frame:x|` when the section declares none), `x` outside every range and outside the section's allocas (the stack's own cells die with the call and are no part of the claim) ⇒ `mem2(x) = mem(x)`; a symbol with no `frame` claims memory unchanged everywhere, a row; with the atom `objects` (`(frame objects)`, `(frame (lo len) objects)`) the claim is over the caller's declared objects only — a symbol that allocates changes the heap's own words and its fresh blocks, which no caller can name, and promises the objects it was handed (none declared: nothing claimed, no row); at a summary call the atom names the CALLER's objects, kept outside the callee's ranges |
| `(ensures-fresh LEN)`, `(ensures-fresh LO LEN)` | none here — a summary allocator's promise at every call: the block `[result, result+LEN)` (or `[LO, LO+LEN)`, the header included) is disjoint from the caller's objects, its loop objects and every earlier fresh block of the caller, and those objects are unchanged (a template); everything else the allocator's |
| `(loop LABEL (invariant I) [(inst SYM e [SYM e]…)…] [(objects (lo len) …)])` | the loop cut at its header: the header's phis and — when the loop writes (a store, an atomic, a call the kernel-effect table says writes) — its memory fresh symbols constrained by `I`; the rows `invariant-init:LABEL` (every entry edge establishes `I`) and `invariant-preserve:LABEL` (every back edge re-establishes it); everything after the loop decided under `I` and the exit edge's condition; `(inst …)` substitutes its pairs together; the loop's `(objects …)` are objects on every visit of the header — their facts join `I`, an allocation inside the loop is disjoint from them, a call's `objects` frame keeps them |
| `(loop LABEL (unroll N))` | the loop's blocks copied N+1 times (N is the most times the back edge is taken), copy N's back edges assumed not taken — a hypothesis every row it reaches carries as `[<=N]` in its site; `(unroll N exact)` claims the bound exact: the row `unroll-exact:LABEL` proves it (its cone excludes the hypothesis), and a claim the profile does not discharge is a red run |
| `(summary)` | the symbol is inlined nowhere: at a call, the callee's `requires` and `objects` facts are rows at the call (`spec:call:<callee>:<block>:<line>:requires:K`, `…:objects:K`), its `ensures` hypotheses over fresh result symbols, its `ensures-trap` joins the caller's trap outcome and narrows the path past the call (a condition over the callee's own registers is a condition nobody knows at the call: a fresh Boolean, both outcomes live; `(ensures-trap true)` ends the path), and memory after the call is a fresh function with the callee's `frame` as its template |
| `(residue "why")` | no rows; the sentence is copied into TCB.md's disposition column |
| `(boundary "what")` | a syscall-class symbol's promise at the kernel boundary, copied into TCB.md; the section is never translated, and its other clauses are assumed at its `(summary)` calls (an allocator's freshness, a free's frame) |

A callee without `(summary)` is INLINED before translation (the call graph
is acyclic; a cycle is a refusal): its blocks are spliced at the call with
its registers and labels prefixed `<callee>#<k>.` and its parameters
replaced by the arguments, and its own `(loop …)` annotations apply to the
copy. `llvm.memcpy`/`llvm.memset` are built-in summaries (the intrinsic
requires disjoint, in-space operands — rows at the call — and copies or
fills every byte). A call of `npk_sys6` is the uninterpreted `sys` under
the kernel-effect table (§9.2); an opaque call (`npk_failsafe`, an indirect
call) a fresh result and a fresh memory, admitted only in a section that
names its `(residue …)`. A loop with neither annotation, an instruction
form outside the floor's vocabulary, a `switch`, a syscall number the table
lacks, a boundary callee inlined, are refusals naming the line: a floor
edit outside the subset is a red run, never a silently unencoded symbol.

**THE INSTANTIATION RULE (S-65, generalised at step 4).** An invariant's
universal is written over the section's free symbols, and the writer
asserts the hypothesis at those symbols and demands the conclusion at the
same symbols. Sound: a proof of `I′(j)` from `I(j)` for an arbitrary `j` is
a proof of `∀j.I′(j)` from `∀j.I(j)` when the step needs `I` at no other
point. A step that needs the hypothesis at another point gets it two ways:
over an `Addr` symbol the hypothesis is instantiated at every point the
loop's memory is applied to anywhere in the file (the body's reads, the
rows' points — the same fixpoint the memory templates use), and at any
other point the author names it — `memcpy`'s step reads the source byte at
`src + i`, so `(inst x (+ src i))` instantiates the hypothesis there (never
the conclusion); several pairs substitute together. An instance is a
hypothesis tagged with its loop, and the loop's own `init` row excludes the
tag (the base case is decided without the claim it establishes).

**THE CONE.** A row's cone is the relevance closure the program encoder
computes — every definition the goal's symbols reach (a backward pass), and
every hypothesis sharing a symbol with the closure, to a fixpoint — with two
readings the floor needed: a hypothesis is about everything its defined
symbols are made of (a fact over a loaded word joins a cone that names the
address the word was read at; a bound on a loop's stop block, a sink nothing
is defined from, names what its path condition is made of), and a memory
function is no shared symbol (`mem` and its successors stand in nearly every
hypothesis; a memory fact is about the addresses it applies the function to).

### 9.2 The kernel boundary

A call of `npk_sys6(nr, a1…a6)` is `(sys nr a1 … a6 k)` — `sys` an
uninterpreted function, `k` the call's sequence number (two calls with the
same arguments are two answers) — with **the kernel's answer shape**: the
result is a value or an errno in `[-4095, -1]` (the ABI every libc reads),
and for `read`, `write`, `getrandom` a non-negative answer never exceeds the
count asked, for `epoll_pwait` the events room. **The kernel-effect table**
(`tx_syscall`; a number outside it is a refusal by name) says what each
does to memory: `read` (0), `getrandom` (318) write `[buf, buf+result)` on a
non-negative answer and nothing otherwise; `clock_gettime` (228) writes the
16-byte timespec; `sched_getaffinity` (204) the mask up to its length;
`rt_sigaction` (13) the old action (a null pointer written nowhere);
`epoll_pwait` (281) `12·result` bytes of events; `mmap` (9) answers an
address or an errno past `2^64 − 4096`, the mapping's bytes fresh and zero
when anonymous (`MAP_ANONYMOUS` in the flags) — each a template over the
memory after; `exit` (60) and `exit_group` (231) end the path (a trap site
with the status); `write`, `close`, `openat`, `futex`, `epoll_ctl`,
`eventfd2`, `epoll_create1`, `dup3`, `prctl`, `arch_prctl`, `getppid`,
`munmap`, `mprotect`, `pidfd_send_signal`, `tgkill`, `getpid` (1, 3, 257,
202, 233, 290, 291, 292, 157, 158, 110, 11, 10, 424, 234, 39) leave memory
as it was. The table IS TCB.md §3's kernel row made concrete (§5's eighth
acceptance): a syscall whose effect the table understates is an unsound
proof, and the table is the reviewer's to read against the kernel's
documentation.

**The envelope symbols** (`npk_open`, `npk_close`, `npk_read`,
`npk_write`, `npk_ofd_close`, `npk_mono_now`, `npk_path_exists`,
`npk_write_file`, `npk_read_file`, `npk_read_stdin`, `npk_to_cstring`,
`npk_string_concat`, `npk_string_slice`, `npk_int_to_string`) are decided
under it: a negative kernel answer is the error field with a zeroed value
slot, a non-negative one the value with error 0; their allocator calls are
`(summary)` calls of `npk_alloc_internal` — a boundary symbol whose
clauses are its promise: a fresh 16-aligned block of `n` bytes disjoint
from the caller's objects, its loop objects and its earlier blocks, those
objects unchanged, the block's header word `n`, `n` below the size ceiling,
or the trap outcome under a condition nobody names — and a superseded
buffer's return is a `(summary)` call of `npk_dalloc` (the block's bytes
and the heap's the allocator's, the caller's objects kept, the trap outcome
likewise unknown).

### 9.3 The rows and the verdicts

A row per clause as the table says; `kind` `floor-spec`; `symbol` the
define (`@memcpy`); `hash` the SHA-256 of the row's canonical text (the
cone's definitions, its hypotheses, `(assert (not goal))`) plus the symbol
and the kind (D-218 (8), through `lib/nhash.npk`); `site` `spec:<clause>:<K>`
or `spec:<clause>:<block label>` with `[<=N]` appended when a bound is in
the cone; `role` `conform`, `group` and `traps` `0`, `encoded` `1`, `tier`
the writer's word (`int`, `bv`, `fp`). The cone of a row is the relevance
closure the program encoder computes: every hypothesis sharing a symbol
with the closure, every definition the closure names.

**Verdicts.** `discharged` is a proof. `budget` is RESIDUE — the row and
its `--explain` reason appear in TCB.md §5. **`open` in the floor is a run
failure by name** (`floor: @sym: <site> is refuted …`): nothing in the
floor is a guard to retain, so a `sat` is a defect or a misstatement, both
stop-the-line; `npkg verify --record` refuses to write an `open` floor row
and both runners refuse a committed one. `--explain` covers the floor's
rows in `build/verify/explain-floor.txt` exactly as the compiler's in
`explain.txt`.

**The profile carries `lp.dio=false` since step 4 (S-71).** z3 4.16.0's
Diophantine-equation sub-solver (`lp.dio`, on by default) undoes the terms
it added at every `(pop)` by a big-rational matrix elimination — traced in
gdb to `lp::dioph_eq::imp::undo_add_term_method` under
`smt::theory_lra::pop_scope_eh`: `npk_hs_put_dec`'s fourteen-digit row
answers `unsat` in 8 s and returns from its pop 200 s later, its
eighteen-digit row had not returned after 22 minutes, and `(exit)` runs the
same pop. A solver that has answered and does not return is a wedged solver
under P-13 — a build failure by rule, and the harness has no clock to cut
it — so the sub-solver is off. Measured on the step's final emission: the
compiler's 411 encoded rows and the floor's 350 give the same verdict row
for row with it on and off, the floor's set in 325 s against 587 s; the
non-returning pops belong to the step's earlier encoding (the flat
`(div v 10^k)` quotients), one rendering change away, which is the case
for keeping the option rather than a condition of today's rows. The
amendment was ratified by the user on 2026-09-17 (S-71; D-218 (2) as amended).

**The belts, before a solver is spawned** (`floor_spec_current` in npkg,
`floor.check_spec` in the harness): every section names a define; a
`(loop LABEL …)` names a block of it with exactly one treatment; every
section claims something or says why not; a `(summary)` section has an
`ensures`; no symbol twice; no clause head outside the grammar; no free
symbol shadowing a parameter. TCB.md's floor table is generated from the
floor, the spec and the committed floor manifest (`tcb_floor_current`,
`check_tcb_floor_current`; `bootstrap/harness/tcb_floor.py --write`
regenerates it): `trusted (inline asm)`, `specified (N discharged, M
residue)`, the `residue`/`boundary` sentences, the class default for a
symbol no section names yet.

**What lands at 1.5.6 step 3** (the first specs, 86 rows over 25 symbols,
every row discharged under the profile, every file decided in under 0.2 s):
`memcpy`, `memset`, `npk_zero` (a byte loop each, by invariant with
`(inst …)`), `memmove` (the forward path memcpy's body inlined under
memcpy's invariant, the backward loop its own), `npk_string_equals` (the
prefix equal below the counter; the mismatch index named on the `0` path),
`npk_string_from_bytes`, `npk_environ`, `npk_frozen_get`, the ten `npk_m_*`
mixing formulas (through `xor64`), `npk_udivmod128` as a `(summary)` with
the remainder below the divisor by invariant, its four wrappers by the
summary with the sign rules stated, and `fmod`/`fmodf`'s special values.
The residue named: the division identity and the `b = 0`/`b = 1` answers
(the identity needs `2^i`, which the IR does not compute, and the writer
invents no ghost variable; unwound 127 times — the bound proven exact in
0.03 s — the rows exhaust the rlimit), and `fmod`'s reduction (`|result| <
|b|` unknown with the loops unwound 2 and 8 times).

**What lands at 1.5.6 step 4** (350 rows over 79 symbols in
79 files: 343 discharged, 7 residue, 0 open, every
`unroll-exact` row discharged): the chain ring, the park words, the run
queue, the waiter list (the unlink's walk unwound eight times over the
first nine waiters as objects), the wind-up words, the heap-stats writers
(`npk_hs_put_dec` unwound in full with one row per decimal length), the
arenas, the frame arena, the heap's tables and lists (the searches as
summaries under sortedness, the removal by invariant), the validations
(`npk_small_check` as a summary whose ensures say what validated means),
`npk_small_free`, the traps' `(ensures-trap true)`, the boundary summaries,
the fd quartet, the file helpers, the clock and the string builders under
the kernel-effect table, and the `(boundary "…")` promise of every other
syscall-class symbol. The residue named: `npk_small_free`'s five ensures and its frame (six budget rows -- each claim over the state after the poison loop must separate its address from the tail's stores through the class table's geometry, a fourteen-way case over the class index the solver does not finish at the budget or at ten times it), `npk_int_to_string`'s sign row (one budget row -- the sign byte reaches the block's base through the rehome copy's twenty unrolled loads, whose addresses the solver must relate to the sign store's wrapped one), and `npk_wildx_call`'s sentence (no row: an indirect call into a sealed wildx page, whose contents are outside verification by construction -- D-035 -- so its effect on memory and its result are opaque and no clause claims either). Found on the way:
DEF-51 (`npk_read_file`/`npk_read_stdin` leaked every buffer they outgrew;
fixed in the floor, held by `tests/cost/read_file.toml`); the spec's own
wrong statements, each a refuted row corrected (among them
`npk_hs_put_dec`'s precondition, which admitted `pos = 2^64 − 20`, where
the twenty-digit row's `result = pos + 20` is a word that wraps to 0 — the
position after the digits is a word now); S-71, the profile's
`lp.dio=false` (above); and one refutation of the translator's own making
(a `getelementptr` over a shaped index dropped the index's parts for one
round — caught by `npk_hs_put_str`'s preservation row, which is what an
`open` floor row is for). Step 6 adds the syscall table.

### 9.4 The protocol models (D-289; landed 1.5.6 step 5)

A specification says what one function does to memory on one thread. The
floor's hazards are not there: they are in the PROTOCOLS two threads run
between them — a wake that arrives before the sleep, a publish read before
its payload, a slot claimed twice. The r6 verdict is to model the
primitive, never the whole executor, so each protocol is a small
transition system in its own file under `runtime/models/`, unrolled to
SMT-LIB2 in QF_LIA by `npkg/floor_model.npk`:

```
(model NAME
  (of "one sentence: what the model is of")
  (state (VAR LO HI) ...)                ; Int variables with their ranges
  (init PROPOSITION)
  (thread THREAD
    (step STEP
      (ir @sym BLOCK ...) ...            ; the floor blocks this step abstracts
      (guard PROPOSITION)                ; absent: true
      (next (VAR EXPR) ...)              ; the rest keep their values
      (kernel WORD ARGS ...)))           ; a library rule, expanded into `next`
  (bad PRED PROPOSITION) ...
  (depth K) (preempt D)
  (control CNAME PRED MUTATION ...))     ; (replace THREAD STEP ...) | (remove THREAD STEP)
```

The unrolling asserts one step per tick for K ticks, every variable inside
its range at every tick, and — BPOR's idea in the symbolic setting — that
the running thread changes at most `D` times over the K steps: a bug that
needs more context switches than `D` is outside the bound, and the row's
site says so (`bad:<predicate>:K<k>:D<d>`). **A tick may also STUTTER**, the
state unchanged. That is not a convenience: without it, a protocol that
runs out of enabled steps before tick K makes its own unrolling
unsatisfiable, and every bad predicate is then unreachable for the wrong
reason — "the protocol halted" read as "the protocol is safe". With it the
unrolling is monotone in K, so what is reachable in fewer than K steps is
reachable at K, and a stutter reaches no state a step did not. The hazard
is not hypothetical: the runner self-check's own toy model halts at its
third tick, and its `seeing` control — the mutation that MUST reach the bad
state — answered `unsat` until the stutter existed. A row is one bad predicate,
asserted reachable at some tick and refuted: `kind` `floor-model`, `symbol`
`model:<name>`. The kernel rules a step may name are the library's, so no
model writes its own: `futex-wait` (the wait returns at once unless the
word still holds the expected value), `futex-wake` (a parked waiter is
unparked, and a wake with nobody parked is lost), `spurious` (a parked
waiter returns for no reason), `eventfd-write`/`eventfd-read`, and
`signal` (a delivered stop parks the thread in its handler).

**A model that claims nothing is worth nothing, so every model carries
CONTROLS.** A control names a bad predicate and mutates the model — a
step's guard and updates replaced, or a step removed — and the mutated
system must REACH that state: the query is `sat`, and a control that is not
is a run failure by name (`floor-control-blind`), listed in `controls.txt`
beside the rows. The controls are the defects the protocol was written
against: the executor's second sweep dropped, the park word cleared after
it instead of before, `sl_push`'s keep path dropped, the epoll path's
re-check of the word dropped, a mutex released by a plain store, a waiter
waiting on the wrong expected value, the channel table's count published
before its pointer, the reclaim's under-lock re-check dropped, an arena
index read-then-written instead of one atomic add, a chunk linked without
the compare-and-swap, a driver slot claimed by a plain store, a slot
published before its pidfd word is prefilled, and the whole pre-step-1 trap
route (the holder read in one step and written in the next, the winner into
`failsafe` with no stop, a losing thread exiting the process) and the
pre-step-2 allocator (the `failsafe` body taking the heap mutex a stopped
thread holds). Each is `sat`; each is a defect the floor does not have.

**Soundness of the sequential-consistency reading.** The unrolling
interleaves steps in SC order, and that is sound for these protocols
because §2.9's shared-state table says every cross-thread datum they touch
travels through a `seq_cst` operation or a `release`/`acquire` pair whose
payload is read only after the acquire; x86-TSO and LLVM's ordering rules
make an SC interleaving of those operations the only observable order. The
belt holds the models to the floor in the other direction:
`floor_models_current` (npkg) and `check_floor_models_current` (the
harness) read every `(ir @sym BLOCK ...)` form, refuse a symbol or a block
the floor does not have, and refuse a block of a NAMED symbol that holds an
atomic operation or an `npk_sys6` call which no step names — **a model can
be wrong about what a step means, but it cannot be silent about a step.**
TCB.md's disposition column reads `modelled (a, b)` for a symbol some
model's steps name.

**What lands at 1.5.6 step 5** (20 rows over 6 models, every one
discharged; 16 controls, every one `sat`): `park-unpark` (the executor's
idle path against a channel waker, the registration race, the wind-up
sweep, the eventfd path; K 14, D 5 — wake-before-sleep,
absorbed-notification, spent-marker, wound-but-asleep), `futex-mutex`
(Drepper's mutex2 over three threads and the kernel; K 11, D 6 — mutual
exclusion, lost wake, unlock-without-wake), `channel-table` (the publish
and the reclaim; K 12, D 6 — torn publish, double reclaim),
`shared-arena` (the lock-free bump and the chunk walk; K 12, D 5 —
duplicate index, wrong chunk, unlinked forever), `driver-registry` (the
claim, the publish and the kill walk; K 10, D 5 — double claim, unkilled
live driver, stale pidfd signalled, retire of a free slot) and
`trap-route` (the whole-program stop and the failsafe region; K 14, D 6 —
two failsafes, a task step after `failsafe` began, an exit mid-failsafe, a
`failsafe` blocked on the heap). Each depth is the SMALLEST that decides
every row with a margin under the profile, measured: the largest row of the
set is `futex-mutex`'s mutual exclusion at 13.0M of the 20M rlimit (the
next is `park-unpark`'s wound-but-asleep at 12.9M, and every other row is
under 8M), and every control decides in under a second. **Liveness is residue by name** — that a due task is
eventually run, and that the arena walker's spin ends, need a fairness
assumption a bounded unrolling cannot state; 1.5.7's schedule-exploration
harness is the instrument for the real code, and every model here is the
shape its mocked primitives will be driven through.

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

### 2.1 A field's own rule (D-308; landed 1.5.8b steps 6 and 6c)

A struct field may carry `limit<R>` — `sealed limit<Len> int64:count;` — a rule
about that ONE field, spelled after the field's qualifiers and before its type.
Its subject is the field's type by identity (TYPE-059). **The write points** are a
struct literal's value for the field, an assignment through any path (`s.f = v`,
`p.f = v` through a pointer) and a compound assignment; each is checked after the
write, in every build, trapping `LimitViolated`, and is a `limit` row keyed on the
WRITTEN expression (a limited root's own row keeps the statement's key, so a write
to a limited field of a limited binding is two checks at two keys). **Every read of
the field is a fact**: the rule over the read's term is a hypothesis for later
rows, and over a `List`'s length SYMBOL at each read of `l.count` as well, which
is what lets `l.count + 1` decide. **The rule must hold of the field's vacant
value** (D-225: `0`, `false`), decided by the constant folder at the declaration —
TYPE-077 otherwise, and a rule the folder cannot decide there is refused too, so
the subject is a plain integer, a `bool` or a `char`. **A limited field has no
address** (TYPE-063, through a pointer to its struct as well): `@s.f`, `$$m`/`$$i`
of it and a pointer-receiver call on it refuse, because a write through an alias is
one no write point sees. REACH arms `LimitViolated` at the writes and nowhere else.
The prelude's `List` carries `pub Rules<int64>:ListLen = { $ >= 0i64, $ <=
140737488355328i64 };` on `count` and `cap` (D-308 §6), a name D-239 reserves; the
built-in lengths of `string`, `cstring`, a slice and a `buffer` carry the same
bound as a FACT at every read without a rule (D-308 §7: the allocator refuses a
larger block, the two producers of a caller-supplied length are guarded, and no
program may write a header — TYPE_REFERENCE §9.2.1, MEMORY_REFERENCE §3).

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
> Inside an `invariant` it is keyed under the loop, and since 1.5.8b step 3
> (DEF-81) it has a row in every context the head's one check runs in — the
> entry, the back edge, each `continue` — and is elided only when every one
> is discharged; a guard inside a check that is not emitted goes with it (§8).

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

### 4b. Termination: `decreases` and `unbounded` (D-304; landed 1.5.8c step 1, 2026-09-24)

A `while` or `when` loop says why it ends, or says that it may not:

```nitpick
while (i < n) decreases n - i invariant total >= 0i32 { … }   // a measure, then the invariant
while (true) unbounded { … }                                   // an event loop: the acknowledgment
```

- **The measure** `E` is a plain integer (`intN`/`uintN`, TYPE-073) over the
  values at the head, a contract expression (§3's admission, TYPE-060) under its
  own context: neither `result` nor `old(…)` exists in it. The clause comes
  BEFORE `invariant`, once; `unbounded` and `decreases` never both; `for`,
  `loop` and `till` are bounded by construction (D-234, D-022) and take
  neither -- each of those shapes is TYPE-072 by name. A `while`/`when` with
  no clause is accepted until 1.5.8c step 4 sweeps the tree (D-304 (6)).
- **The check, in every build**: at the top of the body, each time the
  condition holds, `E` is evaluated at its own width; a signed measure below
  zero traps `DecreasesViolated` (4119), and from the second visit on a measure
  not below the previous visit's traps the same. Two slots per loop, never a
  sentinel (DEF-69): the previous measure in the measure's type and an `i8`
  first-visit flag -- allocas in a sync body, frame slots at roles 42 and 43
  in a coroutine, since a loop spans suspensions. `continue` re-enters the
  head and is checked; `break` and `exit` leave without one. `unbounded`
  emits nothing.
- **The `terminate` rows**: the ENTRY row at the loop statement, `E >= 0`
  (signed measures only), inside the body's region with the invariant and the
  condition as hypotheses; the PRESERVATION row at the body block, `E' < E`
  over the versions at the body's end against those at its start; one per
  `continue` that re-enters the loop, against the body-start value. All share
  the loop's group and its trap count (two signed, one unsigned); the check's
  compares become one `llvm.assume` each only when EVERY row is discharged,
  and the evaluation and the store stay (a later visit's compare reads the
  slot). The measure's own guards -- an overflow in `n - i` -- record their
  rows in each context the head's evaluation runs in (the entry, the back
  edge, each `continue`), at clause context 0: the evaluation is never removed,
  so they are ordinary guards with one row per visit context, and the entry's
  fact discharges the back edge's row where the entry's own stays `open`
  (nothing bounds `n - i` below `INT_MAX` on the first visit).
- **What discharges**: a counter loop's `bound - v` against `v = v + 1` (the
  preservation outright, the entry from the condition -- 1.5.4's path
  condition); a halving `n` under `n > 0` (the Int division form, D-279). A
  `List`'s `count` as the bound has no length term (DEF-14, E-4): the row is
  `unencoded`, the check stays, D-309's rule.
- **Recursion** (D-304 (2), (5)): a function's `decreases E` is parsed as a
  contract of kind `decreases` and refused as TYPE-075 until step 4 computes
  the recursive groups -- the measure is compared at a call inside the
  function's group, and until then the compiler sees none.
- **The sweep (1.5.8c step 3, D-304 (6))**: every `while` and `when` of the
  compiler's tree -- `src/`, the prelude, `lib/`, `npkg/`, the tools and the
  tests, 977 loops as the parser counts them -- states its clause. A TOOL
  (`meta/roadmap/1.5/tools/decreases_sweep.py`, over the parser-driven dump
  `loop_dump.npk` writes) wrote the shape it can PROVE monotone (392): a
  counter stepped by a positive literal against a bound nothing in the body or
  its function writes or takes the address of, a widened counter `(v => T)`,
  a `&&` conjunction with exactly one such comparison. Every other loop was
  READ, and the reading is a committed file beside the tool,
  `decreases_read.txt`, one line per loop: `stable` (the tool's shape after
  a reader saw the bound is stable), `hoist NAME` (a call's result captured
  once before the loop), `measure EXPR`, `unbounded REASON` (the reason goes
  on the line above the loop, D-316) or `manual` (restructured by hand). The
  idioms: a scan's measure is the bytes left, `len - pos`; a walk along a
  chain built in order is `count - c`; a parser's is the tokens left
  (`raw p_left(p)`, pure); a doubling `c = c * 2` under `c < cap` is
  `cap - c`; a hash probe carries a counter bounded by the table's size;
  `while (v > 0)` is `decreases v`; a worklist that grows as it drains, a
  fixed point, a retry or refill loop bounded by a deadline, a spin on the
  clock and an event loop are `unbounded` with the reason. A measure may
  call a `pure never fails` function (TYPE-060), which is why `p_left`,
  `tokenlist_count`, `item_member_count`, `d_at`, `ast_id_at` and the three
  round constants are declared `pure`. The compile-time evaluator checks a
  measure of a loop it runs (`fold_while`), as it evaluates a `prove` (1.5.4,
  L-18): a measure that does not shrink, or a signed one below zero, is a
  counterexample reported once as TYPE-069. Every `failsafe` in the tree
  names `(DecreasesViolated)`, the runners' generated ones included, since
  the prelude's loops reach nearly every program.

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
| `overflow` | a plain-integer `+ - *` or negation stays in range (D-210): the intrinsic's overflow bit at the guard's own site (K-9) -- a binary node's, a compound's target's, a negation's own node; ONE row over the lanes for a `simd` integer operation, and ONE with N-1 traps for an integer `.sum()`; a node the folder writes as its constant has no guard and no row (D-310) | yes | 1.5.8b step 3 |
| `bounds` | an index is inside its array, slice, buffer or `List` (D-070, D-314): `0 <= i < len` at every checked element access, and ONE row for a range slice's pair (`lo <= hi <= len`) at the RANGE's node; the length is the term `(|npk.len| base)` wherever it is named, so a loop written over `xs.len` or `l.count` proves the accesses inside it; a fixed array's length is its type's constant | yes | 1.5.8b step 5 — and, since 1.5.8b step 6c (D-308 §7), ONE row at each call of `string_from_bytes(p, len)` and `#wild_slice<T>(p, len)`, the two producers of a length no allocation bounds: `0 <= len <= 2^47`, the emitter's guard read back, keyed on the call's own node; a discharged row elides the guard into one `llvm.assume` |
| `cast-range` | a float's `=>!` cast to an integer has an integer meaning (D-306): the value is not NaN or an infinity, and its truncation toward zero lies inside the target -- the two ordered compares before the conversion, `CastRange`; a `simd` cast's any-lane guard is one row over the lanes | yes | 1.5.8b step 5 |
| `exhaustive` | a `pick` covers its domain (checker-discharged) | no | 1.5.4 |
| `requires` | a callee's precondition holds at the call (D-221) | yes | 1.5.3 |
| `ensures` | a body's postcondition holds at its return (D-221) | yes | 1.5.3 |
| `invariant` | a loop invariant holds at entry and is preserved (D-221) | yes | 1.5.3 |
| `limit` | a `limit<Rules>` binding satisfies its rule at every write point (D-220), and a limited FIELD at each of its three (D-308): a struct literal's value for it, an assignment through any path including a pointer's, a compound assignment -- the field's row keyed on the WRITTEN expression, a limited root's on the statement, so a write to a limited field of a limited binding is two rows at two keys | yes | 1.5.2; fields 1.5.8b step 6 |
| `limit-subsume` | one `Rules` implies another at a boundary (D-220): the caller's knowledge of every argument against the callee's rules, at a direct call of a sync callee | yes | 1.5.2 |
| `terminate` | a `while`/`when` loop's `decreases E` measure is at least zero at every head visit (a signed measure; an unsigned one cannot be below zero and records no such row) and smaller than at the previous visit (D-304, D-218.7): the head's check, `DecreasesViolated` -- the entry row at the statement, the preservation row at the body block, one per `continue` re-entering the loop; a recursive call's row against the caller's measure lands at 1.5.8c step 4 | yes | 1.5.8c |
| `stack-depth` | the recursion depth is bounded (the audit's G-6 row) | no | 1.5.8c |
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

> **[1.5.8's close (2026-09-19).]** The five kinds this table assigned to
> "1.5.8" are produced by the two subcycles planning split it into: `overflow`,
> `bounds` and `cast-range` at 1.5.8b, `terminate` and `stack-depth` at 1.5.8c
> (with D-304's `decreases`). `cast-range` read "a checked cast's value fits
> its target (D-148)" until D-306 gave it its guard: a checked integer `=>`
> already refuses a lossy crossing at compile time (D-095), and the one cast
> that could produce a value with NO meaning -- a float's `=>!` to an
> integer, LLVM poison until 1.5.8 step 1 (DEF-58) -- now traps `CastRange`
> (4117) where its row is not discharged.

> **[1.5.8b step 5 (2026-09-19).]** Both kinds are live, and three things
> about them are worth stating where the catalogue can be read against the
> manifest. (1) A CONTAINER'S LENGTH IS ONE TERM. `.len` on a slice, a
> string, a cstring or a buffer and `.count` on a `List` are
> `(|npk.len| base)` -- an uninterpreted function of the base, anchored to
> the BINDING it was read from and invalidated with it -- so the bound in
> `for (int64:i in 0i64...xs.len)` and the length an element's row compares
> against are the same symbol and the row discharges. An escaped name has no
> length term (DEF-14's rule: a name a pointer may write is never named), and
> neither does a length read through a call, which is why a row over a
> parameter's element is `open` unless the loop bounds it.
> (2) A RANGE SLICE IS ONE ROW for the pair the emitter tests in one `and`,
> recorded at the RANGE node -- the index expression's rhs, not the index
> expression, which is the key the emitter must ask with. Asked with the
> wrong key the row reads `discharged` and its guard stays in the verified
> build; the belt that counts a kind's assumes against its elided guards is
> what caught it.
> (3) A GUARD INSIDE A `defer` BODY IS ONE ROW AND SEVERAL TRAPS (DEF-82).
> The walk sees a `defer` body once; the emitter writes it at every exit that
> runs it. So a row recorded inside one carries the body's statement id, the
> emitter reports each copy it writes, and the row's `traps` field is
> computed when the table is written -- one copy's traps times the copies.
> `defer_guard_copies.npk` holds both directions; before the fix the row said
> one where the build held two, in both.

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
> to an integer stay opaque (the cast's RESULT has no term; the crossing
> itself carries a `cast-range` row over the operand since 1.5.8b step 5);
> `flt128` is storage (D-143) and has no term. Every float value is NAMED and its definition
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
opaque as VALUES -- the crossing itself carries a `cast-range` row over the
operand since 1.5.8b step 5, whose goal is the emitter's own two ordered
compares; `flt128` is storage (D-143) and has no term.
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
with a numeral index the lane's term (a computed index opaque, its `bounds`
row live since 1.5.8b step 5), `.len` the count, `.any()`/`.all()` the disjunction and
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

> **[1.5.8b (2026-09-19).]** Three of the four things this paragraph lists as
> outside the fragment came inside it: D-210's overflow rows at step 3, and
> `bounds` and `cast-range` at step 5 -- the last over the float OPERAND (the
> resulting integer is still opaque, which is what "a float leaving to an
> integer is opaque" goes on meaning). A `limit` over a string, a struct or an
> array and the `TbbErr` guards over a `frac` or a tfp-element `complex` are
> still `unencoded` with their guards kept.

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

> **Only what the emission holds (1.5.8b step 3; D-262 carried to the
> obligations).** `--obligations` writes a function's file and rows only when
> the emitted module holds the function: its symbol is defined, or its
> coroutine's resume is (D-177), or its checked entry's `.body` twin is
> (D-252). A prelude function the trim dropped (D-262) is not in the artifact,
> so its rows are no guard of it. The runners' belts had skipped them since
> 1.5.2d, and the solver decided them for nothing. Measured at the step that
> gave the prelude's arithmetic its `overflow` rows: a small verify program
> emitted 14 functions and decided 168, 260 of its 267 seconds going to one
> function (`flt_bits_shortest`) it never references. The files are
> renumbered in order. A row's hash is its problem text, so no hash or verdict
> moves, and the compiler's own manifest loses exactly the rows of the prelude
> functions it does not reference.

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

> **[Before 1.5.7 step 0 (2026-09-17), D-297 (S-77).]** THE HANG NET. Every
> z3 process a runner spawns runs under a wall-clock net that is a HANG NET
> and never a verdict (P-13, 1.5.0): a solver killed at it fails the run by
> name — "z3 exceeded the wall-clock net of N s on F" — and no kill can
> become a row, so a verdict stays a function of (obligation, solver build,
> budget) and never of the machine (D-218.2). The net per file is `120 +
> 10·checks + 60·B` seconds, B the number of the file's rows the manifest
> the run is held to records `budget` — `nitpick.obligations` for the
> compiler's leg, `runtime/npkrt.obligations` for the floor's, matched by
> hash, kind and symbol — because a `budget` row burns the whole rlimit by
> definition (about 33 s under the profile on the workbench's machine) and
> the flat `120 + 10·checks` left the floor's `npk_small_free` at 81% of its
> bound on every run. A run with no manifest to trust — `--record`, a verify
> test held to its own `expect-obligation:` lines, a planted self-check case
> — takes the larger bound for every row (B = checks); a tier-2 twin counts
> its own rows so recorded; a model's control (one `(check-sat)` that must
> answer `sat`) keeps the one-row floor, 130 s. Both runners compute it from
> one formula each, and both self-checks hold the formula to one planted
> text (`hang-net`, `hang-net-untrusted`). A red on the net is a build
> failure to READ, never a flake to re-run: the solver it names either
> wedged (S-71's class) or met a file the manifest does not justify.

> **[1.5.8b step 3 (2026-09-19), DEF-81, D-309.]** A GUARD INSIDE A CLAUSE
> CHECK. `rows.txt` has a twelfth field, the row's CLAUSE CONTEXT. It is 0,
> or it names the loop statement or return seam whose check holds the row's
> guard: a division, a shift or an `overflow` inside an `invariant` or an
> `ensures` clause. The rules follow from where the check runs.
>
> - **A guard in a check exists only where the check is emitted.** When every
>   row of the check is discharged (a seam's `ensures` row; a loop's entry,
>   back-edge and `continue` rows), the check is not emitted, and the guards
>   inside it go with it. Such a guard holds no trap and no assume. Its
>   condition is proven by the check's own rows, which carry it as a conjunct
>   (DEF-33).
> - **A loop head's check runs at every visit.** So a guard inside the
>   invariant has a row in every context the head is reached from: the entry,
>   the back edge and each `continue`. All of them share one site, kind and
>   clause context. The guard is elided only when every one of them is
>   discharged.
>
> Until this step only the entry recorded those rows. The back edge and the
> `continue`s were encoded quiet. A guard at the head was therefore elided
> on the proof for its first visit alone. `inv_inner_guard.npk` holds the
> case: at entry the divisor is 1, and on the back edge it has been
> decremented. The plain build traps `DivByZero` (36). The verified build
> divided by zero with no guard, a machine fault (98) at -O0 and
> `InvariantViolated` (34) where -O2 had used the undefined division.
>
> A return seam's check runs in one context, its own. So a seam's inner rows
> are decided exactly where the check runs. A discharged seam takes its inner
> guards with it (`ens_inner_guard.npk`).
>
> THE BELTS COUNT GUARDS, NOT ROWS. A guard's rows share (symbol, kind,
> clause context, group). A guard of an assume kind that is elided becomes
> one `llvm.assume` per trap: 1 for most, N−1 for an integer `.sum()`, whose
> fold steps are N−1 guards at one site. A retained guard keeps its traps.
> Bypass call sites are keyed with their clause context too. Both
> self-checks hold five cases. Two are the gone and kept seam. One is the
> kept seam with its assume missing. The last two are the head kept by an
> open back edge, and the defect's own shape: a head guard elided on the
> entry's row, which must fail.
>
> THE TRAP CODES ARE COMPLETE. Every kind the catalogue (§7b) marks guarded
> and producing rows has a code in both runners' trap tables, or is a
> bypass kind (`limit-subsume`, whose guard is the callee's entry). A kind
> the catalogue marks `pending` must appear in no row. `overflow` traps
> `-4110`, `bounds` `-4099` and `cast-range` `-4117`. `overflow`, `bounds`
> and `cast-range` elide into assumes, and the belts' assume kinds had
> lacked `cast-range` since D-306 gave it a guard.
>
> **1.5.8c step 1 (D-304):** `terminate` gained its guard -- the loop head's
> `DecreasesViolated` check, `-4119` in both runners' trap tables, an assume
> kind (two assumes for a signed measure's discharged loop, one for an
> unsigned one's), and OFF the guard-less set (`ok_has_guard(12)` true, the
> catalogue's column `yes`). `stack-depth` stays guard-less and pending step 4.

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
| `(objects (lo len) …)` | none here — the caller's objects: each range lies in the address space (`0 ≤ lo`, `lo + len ≤ 2^64`) and the listed ranges are pairwise disjoint (a null or an empty range is no object and overlaps nothing — a list's tail may be null), hypotheses of the section and rows at a summary call; the ranges an allocation inside the section is disjoint from and a callee's `objects` frame keeps. **`(lo len apart-when COND)`** (1.5.6c): a range that lies in the address space ALWAYS and is set apart from the section's other objects only where `COND` — over the entry state — holds (a pair is apart where both ranges' conditions hold); the form for an object the body touches on some paths alone, because an apartness asserted where the body does not need it is a hypothesis that may be FALSE for a legal caller, and then every row of the section claims nothing for that caller (`npk_small_free`'s list neighbours and the partial list's head are three other chunks only when the chunk was full; asserted apart unconditionally they were false for the ordinary free, into a chunk already on the partial list). Under `(frame objects)` the range counts among the symbol's objects where `COND` holds. Section-level only: a loop's `(objects …)` takes `(lo len)` |
| `(views (lo len) …)` | none of its own — ranges the symbol is handed to READ (1.5.6c): each lies in the address space and is disjoint from every OBJECT of the section, and **no view is set apart from another** — two views of one string overlap, and `string_concat(s, s)` is a legal program (the `(objects (a.0 a.1) (b.0 b.1))` this form replaced asserted its two inputs apart: false for that call, so its rows claimed nothing for it; with the hypothesis deleted all twelve still discharged). Recorded as an object is — an allocation inside the section is disjoint from a view, a sum inside one does not wrap, `(frame objects)` counts it among what the symbol was handed. **Read-only is PROVEN, not named**: the section's frame row exempts no byte of a view — a point inside a non-null view is claimed unchanged whatever frame range it also lies in (`¬inr` becomes `¬inr ∨ in-a-view`; a section with no `frame` already claims every byte) — so a body that writes one is a refuted row. A null view is no view. Rows at a summary call, as objects' facts are. Section-level only, and always `(lo len)`: the conditional form is an object's |
| `(ensures P)` | one row per clause: `P` on the returning, non-trapping paths (`|pc:exit| ∧ ¬trap ⇒ P`) — the exit edge's condition is what a postcondition is decided under |
| `(ensures-trap P)` | one row: `trap ⇔ P`; a symbol with trap sites and no such clause gets the row `¬trap` (it claims never to trap) |
| `(frame (lo len) …)` | one row: for the free symbol `x` (an `Addr`; the writer's own `|frame:x|` when the section declares none), `x` outside every range and outside the section's allocas (the stack's own cells die with the call and are no part of the claim) ⇒ `mem2(x) = mem(x)`; a symbol with no `frame` claims memory unchanged everywhere, a row; with the atom `objects` (`(frame objects)`, `(frame (lo len) objects)`) the claim is over the caller's declared objects only — a symbol that allocates changes the heap's own words and its fresh blocks, which no caller can name, and promises the objects it was handed (none declared: nothing claimed, no row); at a summary call the atom names the CALLER's objects, kept outside the callee's ranges |
| `(ensures-fresh LEN)`, `(ensures-fresh LO LEN)` | none here — a summary allocator's promise at every call: the block `[result, result+LEN)` (or `[LO, LO+LEN)`, the header included) is disjoint from the caller's objects, its loop objects and every earlier fresh block of the caller, and those objects are unchanged (a template); everything else the allocator's |
| `(loop LABEL (invariant I) [(inst SYM e [SYM e]…)…] [(objects (lo len) …)])` | the loop cut at its header: the header's phis and — when the loop writes (a store, an atomic, a call the kernel-effect table says writes) — its memory fresh symbols constrained by `I`; the rows `invariant-init:LABEL` (every entry edge establishes `I`) and `invariant-preserve:LABEL` (every back edge re-establishes it); everything after the loop decided under `I` and the exit edge's condition; `(inst …)` substitutes its pairs together; the loop's `(objects …)` are objects on every visit of the header — their facts join `I`, an allocation inside the loop is disjoint from them, a call's `objects` frame keeps them |
| `(loop LABEL (unroll N))` | the loop's blocks copied N+1 times (N is the most times the back edge is taken), copy N's back edges assumed not taken — a hypothesis every row it reaches carries as `[<=N]` in its site; `(unroll N exact)` claims the bound exact: the row `unroll-exact:LABEL` proves it (its cone excludes the hypothesis), and a claim the profile does not discharge is a red run |
| `(summary)` | the symbol is inlined nowhere: at a call, the callee's `requires` and its `objects` and `views` facts are rows at the call (`spec:call:<callee>:<block>:<line>:requires:K`, `…:objects:K` — the views' facts follow the objects' under the same name), its `ensures` hypotheses over fresh result symbols, its `ensures-trap` joins the caller's trap outcome and narrows the path past the call (a condition over the callee's own registers is a condition nobody knows at the call: a fresh Boolean, both outcomes live; `(ensures-trap true)` ends the path), and memory after the call is a fresh function with the callee's `frame` as its template |
| `(residue "why")` | no rows; the sentence is copied into TCB.md's disposition column |
| `(boundary "what")` | a syscall-class symbol's promise at the kernel boundary, copied into TCB.md; the section is never translated, and its other clauses are assumed at its `(summary)` calls (an allocator's freshness, a free's frame) |

**`requires`, `(objects …)` and `(views …)` are HYPOTHESES ABOUT THE CALLER
(1.5.6c, leads E-1 and E-2).** A section's rows are decided under them, so
for a call that does not keep them the rows say nothing — and only a
translated caller of a `(summary)` symbol proves them, as rows at the call.
A caller that is a boundary symbol, a symbol with no section, or emitted
code is checked by nothing. **TCB.md §4d, GENERATED** (`floor.callers_region`;
`tcb_callers_region` in `npkg/floor.npk`; held current by both runners, a
stale region a red run), lists per section which callers a row covers — a
translated caller of a `(summary)` symbol, at the call; a translated caller
of any other symbol, which inlines it — and which nothing does: the floor's
untranslated callers, and emitted code wherever the symbol is EXPORTED (its
`define` is not `internal`: the emitter's calls and LLVM's own lowering reach
it). A structural assumption is ARGUED in the spec beside the clause, and
TCB.md §5's sixteenth acceptance is what a reader is asked for. Two
consequences a spec author owes the file: a range that may legitimately
coincide with another
is a VIEW or stays a bare `requires` (memmove's), never an object; and an
object the body touches only on some paths is set apart only there —
`(lo len apart-when COND)` (`npk_small_free`'s list neighbours, apart only
when the chunk was full). Spell the condition with the form, not with an
`ite` inside the range: a range made null or empty by an `ite` says the
same thing and took that file from 202 s to 357–908 s in the runners' own
mode, past its hang net.

**What is read once is written once.** The translator reads the FIRST
`objects`, `views`, `frame`, `ensures-trap`, `ensures-fresh`, `residue`,
`boundary` and `summary` of a section and the first `objects` of a loop, so
a second was dropped in silence — for `frame`, `ensures-trap` and
`ensures-fresh` a claim written and never proven. Both runners refuse one
by name (`a clause read once and written twice in …`), and hold a `(loop …)`
clause to the four sub-clauses the translator reads — `invariant`, `unroll`,
`inst`, `objects` — (`a loop sub-clause the grammar does not know …`). Found
by reading, at 1.5.6c's planning; the spec had neither.

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
and where a row below names a `bound`, a non-negative answer never exceeds
that argument.

**The kernel-effect table is ONE authority, and it is this region** (D-288 as
amended, 1.5.6b; D-201's pattern). `bootstrap/generator/gen_tables.py` parses
it STRICTLY — a row it cannot read stops the generator — and writes
`npkg/floor_kernel.npk`, which is all the translator knows about a syscall:
`tx_syscall` and `sys_writes` keep no list of their own, and the harness reads
the same region. One row per number the floor issues:

- **effect** — `none` (memory is as it was), `writes` (the kernel writes
  `length` bytes at `buffer`), `maps` (`mmap`: an address, or an errno past
  `2^64 − 4096`; the mapping's bytes fresh, and zero when anonymous), `ends`
  (the path ends: a trap site with the status), `asm` (issued only by a
  symbol the translator never reads — inline asm — so no effect is modelled
  and none is needed; the row exists so that EVERY issued number has one).
  Since 1.5.8 step 2b (DEF-64) "issued" includes a `module asm` symbol's own
  syscalls. The census reads a `syscall` there from the `mov $N, %eax`
  immediately before it, and nothing else (`floor-asm-syscall-unread`
  otherwise), so `rt_sigreturn` (15) and the trampoline's `exit` (60) are
  issued numbers like any other.
- **buffer**, **length** — for `writes`: the argument holding the address,
  and `result`, `result*12` or a numeral. ONE RULE conditions every write and
  so no row carries its own: **an error answer writes nothing, and a NULL
  buffer is written nowhere** (`rt_sigaction`'s `oldact`).
- **bound** — the argument a non-negative answer never exceeds.
- **option** — for a number whose effect DEPENDS on an option argument
  (`arch_prctl(ARCH_GET_FS)` and `prctl(PR_GET_NAME)` WRITE user memory where
  the options the floor passes do not), the argument and the numerals the row
  speaks for. A call site whose option is not a numeral in that set is refused
  by name, in the translator and by the belt over every `npk_sys6` site of the
  floor, translated or not — as a number without a row always was.

<!-- BEGIN kernel-effects -->
| nr | name | option | effect | buffer | length | bound |
|---|---|---|---|---|---|---|
| 0 | `read` | — | writes | arg2 | result | arg3 |
| 1 | `write` | — | none | — | — | arg3 |
| 3 | `close` | — | none | — | — | — |
| 9 | `mmap` | arg4 in 34 | maps | — | — | — |
| 10 | `mprotect` | — | none | — | — | — |
| 11 | `munmap` | — | none | — | — | — |
| 13 | `rt_sigaction` | — | writes | arg3 | 32 | — |
| 15 | `rt_sigreturn` | — | asm | — | — | — |
| 39 | `getpid` | — | none | — | — | — |
| 56 | `clone` | — | asm | — | — | — |
| 59 | `execve` | — | asm | — | — | — |
| 60 | `exit` | — | ends | — | — | — |
| 72 | `fcntl` | arg2 in 1 | none | — | — | — |
| 110 | `getppid` | — | none | — | — | — |
| 131 | `sigaltstack` | — | writes | arg2 | 24 | — |
| 157 | `prctl` | arg1 in 1 38 | none | — | — | — |
| 158 | `arch_prctl` | arg1 in 4098 | none | — | — | — |
| 202 | `futex` | arg2 in 9 128 129 137 | none | — | — | — |
| 204 | `sched_getaffinity` | — | writes | arg3 | result | arg2 |
| 228 | `clock_gettime` | — | writes | arg2 | 16 | — |
| 231 | `exit_group` | — | ends | — | — | — |
| 233 | `epoll_ctl` | arg2 in 1 2 3 | none | — | — | — |
| 234 | `tgkill` | — | none | — | — | — |
| 257 | `openat` | — | none | — | — | — |
| 281 | `epoll_pwait` | — | writes | arg2 | result*12 | arg3 |
| 290 | `eventfd2` | — | none | — | — | — |
| 291 | `epoll_create1` | — | none | — | — | — |
| 292 | `dup3` | — | none | — | — | — |
| 318 | `getrandom` | — | writes | arg1 | result | arg2 |
| 424 | `pidfd_send_signal` | — | none | — | — | — |
<!-- END kernel-effects -->

The option sets are the floor's own: `mmap` flags 34 (`MAP_PRIVATE |
MAP_ANONYMOUS`, never `MAP_FIXED`), `prctl` 1 and 38 (`PR_SET_PDEATHSIG`,
`PR_SET_NO_NEW_PRIVS`), `arch_prctl` 4098 (`ARCH_SET_FS`), `futex` 9, 128,
129, 137 (wait-bitset, and wait, wake and wait-bitset under
`FUTEX_PRIVATE_FLAG`), `epoll_ctl` 1, 2, 3 (add, delete, modify), `fcntl` 1
(`F_GETFD`, startup's probe of the three standard descriptors, DEF-69 --
it takes no address, and the option set is what keeps it so: `F_GETLK` and
its kin write a `struct flock`).

**The write-region rows are HELD TO THE RUNNING KERNEL, not accepted**
(1.5.6b). `tests/backend/programs/kernel_effects.npk` — an ordinary program,
so both runners run it — calls each `writes` row's raw syscall over a
sentinel-filled buffer, twice with two sentinels so a written byte that
equals one is still seen, and demands that the bytes the kernel changed are
EXACTLY the row's on a success and NONE on a failure. Equality, not
containment, because an over-approximating row is sound and is what hid a
defect: two rows of this table were measured wrong on the day it became an
authority. `sched_getaffinity` claimed the REQUESTED length where the raw
call writes `result` bytes — 8 of 128 on the measuring machine, the tail
untouched; it is glibc's wrapper that zero-fills it — and behind that row
`npk_hardware_concurrency` popcounted 120 bytes of stack nobody had written
(DEF-52: 1008 hardware threads on a 48-thread machine); it also lacked its
bound, without which no caller's frame claim can be proven. `rt_sigaction`
claimed the 8-byte sigset size where the kernel writes the whole 32-byte
action to a non-null `oldact` — an UNDER-approximation, the unsound
direction, latent only because the floor's one call passes `oldact = 0`. No
recorded verdict rested on either. What no probe reaches a reader still
accepts, and TCB.md §5 says so in words: `exit`/`exit_group` (the path
ends), the `asm` rows, failure paths beyond the ones probed, that the claim
is about the kernel the suite ran on, and that "no memory effect" speaks of
BYTES and not of mappings — a load after `munmap` is modelled as the old
bytes, so a use-after-unmap is outside the model by construction.

**The envelope symbols** (`npk_open`, `npk_close`, `npk_read`,
`npk_write`, `npk_ofd_close`, `npk_mono_now`, `npk_path_exists`,
`npk_write_file`, `npk_read_file`, `npk_read_stdin`, `npk_to_cstring`,
`npk_string_concat`, `npk_string_slice`, `npk_int_to_string`) are decided
under it: a negative kernel answer is the error field with a zeroed value
slot, a non-negative one the value with error 0; their allocator calls are
`(summary)` calls of `npk_alloc_internal` — a boundary symbol whose
clauses are its promise: a fresh 16-aligned block of `n` bytes disjoint
from the caller's objects, its loop objects and its earlier blocks, those
objects unchanged, the block's header word `n`, `n` at most 2^47 — D-308's
CEILING, the one compare in the allocator's three entries (1.5.8b step 6c) —,
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

**A NAMED BLOCK IS NOT A MODELLED ONE — what the belt cannot see, and the
seventh model (1.5.6b step 2; lead E-3 of OPEN_DECISIONS §2g).** The
correspondence belt proves that every block holding an atomic operation or a
syscall is NAMED by some step. It cannot prove that the step's `next` bindings
MEAN what the block does, and a step may name a block while saying nothing
about it — or give a named block's PATH no transition at all. Reading
`park-unpark` against `npk_park_sleep`, case by case, found all three kinds.
(1) A deviation: the floor's `epoll` block returns WITHOUT draining the eventfd
when the park word is already set — the read is only in `drain` — so the
eventfd's readability survives and ends the NEXT wait at once; the `epwait`
step cleared it on that path too. Fewer wakes than the code has, and not what
the blocks do; the step is faithful now. (2) A missing step: the wait returns
with NOTHING at its timeout (the sleeper deadline it carries) or on EINTR —
`n <= 0`, straight to `out`, no event read and no drain — and the model let a
parked epoll sleeper return only when the eventfd was readable, where its futex
path has had the library's `spurious` return since it was written. Every round
AFTER an empty return was behaviour the floor has and the model did not: the
reachable states of the model went 263 → 275 with (1) → 358 with `epempty`.
All four rows stay `unsat` and all four controls `sat` at K 14, D 5, the
largest row (wound-but-asleep) at 15.3M of the 20M rlimit in the runners' own
mode. (3) A gap: `epwait` named the `due` block (a descriptor's event
stamps its frame due) and `arm` named all ten blocks of `npk_io_register` for
one bit, while the model has no variable for a descriptor's readiness — so no
predicate of it could speak about an event, a one-shot, or the frame an event's
payload points at. The belt was green, correctly, and the I/O wake path had no
evidence. It has its own model now, not a bigger `park-unpark` (r6: model the
primitive; that model's largest row sat at 12.0M of the 20M budget before this
step, measured the same way, and sits at 15.3M after (2)):
**`reactor-io`** — one executor, one task waiting on one descriptor, and the
kernel, over `npk_io_register`, `npk_park_sleep`'s epoll blocks,
`npk_io_unwatch`, `npk_sl_push`, `npk_sl_wake_due` and `npk_step`'s `finished`;
K 16, D 7, its three rows deciding in 1.2 s at under 2M of the rlimit each. Three bad predicates,
unreachable: **stamp-after-free** (a due stamp written into a freed frame — the
hazard `npk_io_unwatch`'s own comment names, and the reason `io_ready` DEFERS
the unwatch so a registration lives exactly as long as its wait),
**event-consumed-task-asleep** (the one-shot fired and disarmed, its event
consumed, the task neither due nor woken and the executor parked again), and
**declined-but-asleep** (the kernel refused the watch — EPERM for a regular
file, EBADF — and the task sleeps on a descriptor nothing will ever report:
`duenow`'s reason to exist). Three controls, each `sat`: the delivery loop
stamping nothing, the frame freed BEFORE its unwatch (a task resumed by its
deadline leaves an armed one-shot behind), a declined watch that does not make
the task due. One modelling fact worth its sentence: the event is delivered
INSIDE `npk_park_sleep`, right after the wait returns, so nothing else of that
thread can run in between — every other executor step's guard asks for `evt =
0`, and without that the model lets the sweep interleave there and reports a
use-after-free the code cannot reach. That makes 23 rows over 7 models and 19
controls. 1.5.7's synthetic EPOLLIN drives this same path over the real code.

**What the bounds cover, measured (1.5.6b step 2).** A row holds to its K and
its D and no further, and "the smallest depth that decides with a margin" says
nothing about how much of the model that depth reaches. An explicit-state
reading of the same model texts (`meta/roadmap/1.5/tools/model_bfs.py`: the
unroller's semantics mirrored, then breadth-first search with no depth bound,
no preemption bound and no solver — a MEASUREMENT, outside every gate; its
table is in 1.5.6b's record) says: the models are small — 68 to 1,086
reachable states — and in four of the seven (`futex-mutex`, `reactor-io`,
`shared-arena`, `trap-route`) the bounds reach EVERY reachable state, so their
rows are complete for state predicates; in three the depth is smaller than the
model's diameter (`channel-table` K 12 against 15, `driver-registry` 10
against 11, `park-unpark` 14 against 19 — 70 reachable states in all lie
outside the bounds), and `park-unpark` cannot be unrolled to its diameter under
the profile (K 19 answers `unknown`). In ALL seven the search finds no bad
state anywhere in the reachable space, every control's bad state inside the
bounds, no step ever blocked by a variable's range, and no disagreement with
z3 wherever both speak. At step 2 that was a probe, in no runner, and nothing
in TCB.md rested on it.

**The second reading is a BELT (1.5.6b step 4d; D-295).** The search runs on
every full run in both runners — `floor.check_models_explicit` (the harness's
`check_floor_models_explicit`, beside the correspondence belt) and
`npkg/floor_explore.npk` (`floor_models_explicit`: both of `npkg verify`'s belt
sites and `tools/floorspec.npk`'s default mode) — with the same findings, byte
for byte: `floor-model-bad-reachable` (a bad state reachable ANYWHERE; the
message gives the least depth and the least thread changes and says whether it
lies inside (K, D), where a discharged row says the opposite and one of the two
readers is therefore wrong), `floor-model-control-unreachable` (a control whose
bad state is not reachable inside the bounds — the solver's `sat` says the
opposite), `floor-model-range-blocks` (a step whose successor leaves a
variable's range: the unrolling asserts the ranges at every tick, so it DROPS
that transition silently — a model that loses behaviour without saying so),
`floor-model-too-large` (the state product does not fit one 64-bit key, or more
than 200,000 states: refused by name, never skipped) and
`floor-model-unreadable`. THE SEMANTICS ARE THE UNROLLER'S, MIRRORED — one step
per tick or a stutter, `next` reads the pre-state, the first binding of a
variable wins, the kernel library's six rules expanded as `read_kernel` expands
them, thread changes counted over real steps, the initial state read at tick 1
through the stutter — and the two are INDEPENDENT readers of a model's meaning,
the first two it has had: the unroller writes the SMT text for both runners,
and the harness's Python side read only the `(ir …)` forms. No verdict passes
between them: the run is green only when the solver's rows hold AND the search
finds nothing, which is the disagreement check (1.5.6's stutter hole — an
unroller under which a halted protocol read as a safe one — would have been a
red run on its first day). The twins were held to each other on twelve planted
texts (a bad state inside the bounds and one outside them, a blind control and
one that reaches only outside the bounds, a blocked range, every kernel rule, a
free initial variable, an unknown variable, a wrong arity, an unknown operator):
identical findings, byte for byte. **Its first finding was in the runner
self-check's own fixture**: the toy model the two `floor-control-blind` cases
run was commented as safe and was not — `tick` then `set` reaches `both` in two
steps — and nothing had ever decided its row, because those cases decide its
CONTROLS; `set`'s guard asks `y = 0` now, in both runners' copies. TCB.md's
generated residue paragraph prints, per model, its bounds, its reachable states
and how many of them the bounds reach (both generators), and §5's thirteenth
acceptance is narrowed: the bounds are no longer where the SAFETY claims stop.
Expressions are validated whole and up front, in one order in both twins (an
evaluation short-circuits; a validation does not), and every arithmetic operand
is held under 2^31 in magnitude so that the Nitpick twin's plain integers, which
trap on overflow, and Python's, which do not, cannot disagree.

### 9.5 The floor's stack (D-305; landed 1.5.8 step 2, 2026-09-19)

**Every function the compiler emits checks its stack; the floor's functions do
not, and a belt proves they need not.** An emitted `define` carries
`"split-stack"` (`ll_fn_open`, one text for all nine sites), so LLVM's prologue
compares the stack pointer less the function's EXACT frame against the thread's
limit word at `%fs:0x70` before the frame exists, and calls `__morestack` when
it would cross: the floor's `module asm` stub, which enters the trap route as
`StackExhausted` (−4118). The floor's own functions carry no prologue. They run
before `%fs` exists, inside signal handlers on signal stacks and inside the trap
route, where a check would have nothing to answer to. So every stack the floor
maps keeps a 64 KiB RESERVE below its limit word, and the floor object carries
both linker notes: `.note.GNU-split-stack`, so the link accepts emitted callers,
and `.note.GNU-no-split-stack`, so ld.lld does not rewrite a floor function's
calls as though it had a prologue ("couldn't adjust its prologue", measured with
the first note alone). `__morestack_non_split`, which that rewrite would reach,
traps −4102.

**`floor-stack-reserve`, in both runners** (`floor.py`'s `reserve_measure`,
`npkg/floor_stack.npk`), holds the claim the notes make. The floor is compiled
with `llc -stack-size-section` under the pinned flags, and each function's frame
is read back: the harness through `llvm-readobj --stack-sizes`, npkg from the ELF
`.stack_sizes` section itself (`npkg/elf.npk`). It walks the floor's direct-call
graph, which may not recurse except through the trap route (a cycle anywhere
else is a failure by name). The deepest chain, plus one more pass of the trap
route (the re-entry rule bounds the route to one repetition), plus 384 bytes of
slack for the emitted leaf above it (a 256-byte leaf and the 128-byte red zone),
must fit in a QUARTER of the reserve, 16,384 bytes. A chain that reaches an
indirect call stops there: a resume function is emitted and checks itself, and
JIT code is `wildx`'s (TCB.md §5, item 18). Measured at 1.5.8 step 2 and
unchanged through 3c: **1,032 bytes** (`npk_arena_alloc` → `npk_ralloc` → … →
`npk_heap_oom` → `npk_trap` → `npk_exit` → `npk_wild_live_count` →
`npk_m_livew`), +296 for the second pass of the route, +384: **1,712 of
16,384**. Planted cases in each runner's self-check (`reserve-over`,
`reserve-recurses`, and `reserve-control`, which must pass) show it refusing.

**What the belt does not say** is TCB.md §5's item 18: that `llc` reports the
frames it emits; that no signal frame lands in the reserve (every action the
floor installs says SA_ONSTACK, and `sigaltstack` itself refuses a stack below
the machine's minimum, which the floor turns into a startup trap); that JIT
code checks nothing; and that the switches' assembly (`npk_switch_stack`,
`npk_fs_switch_call`, `__morestack`) does what its comments say.

## 10. The schedule explorer (D-212, D-298…D-303; landed 1.5.7, 2026-09-18)

A proof decides what a model says, and a protocol model says what its author
wrote (§9.4). A stress run executes the real code, but under the kernel's
scheduler, which reaches a narrow interleaving by luck: `// stress: 40` ran
`trap_one_failsafe` forty times and never reached the two-instruction window of
DEF-57. The explorer runs the REAL code under a scheduler that CHOOSES the
interleaving, one synchronization step at a time, from a seed, so every
schedule it tries can be replayed exactly. It complements `// stress:` and
never replaces it (X-12): stress runs real time, the real kernel scheduler and
weak memory on real cores, and the explorer runs the interleavings stress
never reaches.

**The explored build is the real one, transformed (X-1, X-9, X-20).** One
transformer (`npkg/explore.npk`, built into `tools/explored.npk` for the
harness and linked into `npkg`) rewrites text: a `call void @npkx_point(i32
SITE)` before every atomic step line (`atomicrmw`, `cmpxchg`, `fence`, `load
atomic`, `store atomic`, the models' census, §9.4), every `@npk_sys6(` call
routed to `@npkx_sys6(`, the thread lifecycle hooked around the clone and
inside `@npk_thread_entry`, and every other line byte for byte. It rewrites the
FLOOR once per run (142 step lines at 1.5.7's close: 79 atomic, 63 syscalls)
and every explored unit's OWN IR (the program mode, step 6), because
`atomic<T>` and `atomic_from_ptr` lower inline and the `sys` builtin calls the
trampoline from the program. A program's sites are numbered from 1,000,000,
beyond the floor's and beyond a syscall's identity (`n | 2^31`), so one
schedule hash reads both modules without ambiguity. The shim's module is
never transformed. THE TOTALITY BELT, in both runners: the step lines of the
source equal the points plus the routed calls of the output, and every atomic
step of the output has its point immediately before it
(`explore-step-escapes`, by name, for the floor and for each program). It is
the second reader of the transform, and a count. **What no point can precede
is STATED (1.5.8 step 2b; DEF-64).** A syscall written in a `module asm`
block is neither a step line nor a routed call: the transformer rewrites IR,
and assembly is not IR. Such a syscall runs for real with no point before it.
These are the clone trampoline's `clone` (56) and the child's `exit` (60),
and the stop handler's restorer `rt_sigreturn` (15), which is never executed.
`runtime/explore/unrouted.txt` states each one — `@SYMBOL NUMBER` and the
reason the explored build stays sound — and both runners hold the file to
the floor's `module asm` census exactly (`explore-asm-unlisted`,
`explore-asm-stale`, `explore-asm-malformed`, by name). The stage prints the
list on every run.

**The shim is hand-written IR (D-298; `runtime/explore/npkx.ll`), linked into
explored test binaries only, under the floor's own belts.** One thread holds the
BATON, and every other sits in a real private futex wait on its slot's grant
word. What would block is virtual. A futex WAIT blocks virtually while the word
holds the expected value, a WAKE releases the longest-blocked first, and a wait
whose absolute deadline has already passed returns `ETIMEDOUT` at once, as the
kernel does (X-14). `epoll_pwait` is a real poll with a zero timeout under the
baton, re-probed only after another thread has stepped. `clock_gettime` is a
virtual clock: +1 µs per read, and a jump to the earliest deadline when nobody
can step. Signals are virtual (step 2): `rt_sigaction` is remembered, `tgkill`
marks the target and makes a blocked one runnable, and the handler runs in the
target's own context at its next grant, so the trap route's stop walk is
explored like anything else. A MACHINE FAULT is real (1.5.8 step 3, D-307):
`rt_sigaction` passes through to the kernel as well as being remembered, so the
four fault signals' actions are installed for real; a fault in program or JIT
code is delivered by the kernel to the floor's handler on the thread's signal
stack, and the trap route it enters is explored like any trap's
(`machine_fault_thread`, explored). The shim itself is not reentrant (K-13):
every entry the floor calls at an arbitrary point, a point or a routed syscall,
raises a per-thread mark while the shim's own code runs and lowers it before
any floor code runs inside the shim. An entry that finds its thread's mark
raised was reached from a fault inside the shim, and is reported as the shim's
defect. The address space is virtual (X-13): an anonymous
private mapping with no hint is placed at a 64 KiB-aligned bump pointer with
`MAP_FIXED_NOREPLACE`, because the floor's chunk trim made a step count depend
on an ADDRESS. A thread's end is settled before anyone else steps (X-19): the
kernel's clear of the `CHILD_CLEARTID` word is waited for, because the joiner
reads that word. Everything else is the real syscall, after a point.

**The scheduler is PCT with a fairness rule.** A seed-0, depth-1 run measures
the schedule's length k. Each seeded run then assigns random priorities and
d − 1 change points (d = 3 unless `// explore: N d=D` says otherwise), and
always runs the highest-priority thread that can step. The bands are ORDERED:
initial above change-point above demotion, each demotion below every earlier
one. A thread that has run 4,096 consecutive steps while another could step is
demoted, the bound jittered by 0 to 63 steps from the seed's own stream (X-16:
a fixed bound resonated with a periodic thread). PCT's guarantee is a
probability per run, 1/(n·k^(d−1)) for a depth-d bug in a program of n threads,
and the stage prints it per unit from the measured n and k. The same seed gives
the same schedule: the stage runs a unit's first seed twice and requires one
schedule hash (`explore-replay-differs`, X-7). D-303's alternating sweep is the
stronger test of determinism, since it found X-13 and X-19 where the replay belt
could not.

**The verdicts (D-301, D-302).** A run is red when the program's exit is not its
`expect-exit:`, and when the shim reports `DEADLOCK` (nobody can step and no
deadline is pending), `STEP BUDGET`, `MMAP`, `LOST-FUTEX-WAKE` (at quiescence, a
virtual waiter's word no longer holds the value it waited on), `LOST-WAKE` (at
quiescence, a blocked thread's executor holds a frame stamped due),
`SHIM FAULT` (the shim entered from its own code: a fault inside the shim, K-13)
or `ASSUMPTION <symbol>: <clause>`. The last is a caller hypothesis of the floor's
spec found false at a call: the transformer writes one ENTRY CHECKER per section
of `runtime/npkrt.spec` that has rows and a `requires`/`objects`/`views` clause,
and 236 of the 240 hypotheses are evaluated at every call of every explored
schedule; the 4 that name a free symbol are listed by name (TCB.md §4d). The
two quiescence oracles are red even when the exit code is right, because every
wait in this runtime has a deadline (D-071): a lost wakeup degrades to
LATENESS, which an exit code cannot see and virtual time hides completely.

**The units (D-299, D-300, X-11).** Every `// stress:` program says
`// explore: N` (1,000 seeds per run) or `// explore: no <reason>`, and a
program that says neither is `explore-unmarked`, red by name. A seed that once
found a defect is kept as `// explore-seed: S` and runs first, forever:
`trap_one_failsafe` keeps 371, DEF-57's schedule. *[1.5.8 step 2c (DEF-67): a
kept seed is a SCHEDULE only until a step is added before its window. Seed 371
stopped being DEF-57's schedule when 1.5.8 step 2 gave each thread one more
routed step, and no run could notice, because a kept seed is replayed on the
fixed floor and never checked to still reach anything. A kept seed now claims
nothing. A window a seed once reached is held by a control that plants the
defect and is decided on every run; DEF-57's is `frozen-traps.ctl`, a HELD
control (below).]* At 1.5.7's close, 39 programs
are explored and 10 are marked `no`: nine spawn real child processes (a virtual
clock cannot share a real child's real time), and `driver_spawn_fail` forks one.

**The negative controls (X-10, X-15, X-17, X-18, X-21; `runtime/explore/controls/`).**
An instrument that claims to find a defect proves nothing until it is shown
finding one. A control plants a defect by exact text substitution, and each
`old` block must occur exactly once. It can plant in the floor (`old:`/`new:`),
in the spec (`spec-old:`/`spec-new:`: a false caller hypothesis, verdict
`ASSUMPTION`) or in the named program's source (`program-old:`/`program-new:`).
It names the verdict the explorer must reach within N seeds: a word of the
shim, `wrong-exit`, `exit N`, or `late N` (the virtual run time at exit, the
lens for a mark that was overwritten rather than left unread). A DIRECTED
control names up to four atomic sites at which each arriving thread is demoted
below every other: a change point at a place, for a window one step wide that
blind PCT cannot land on. A directed site cannot REORDER two arrivals at the
same place, since the later one is demoted below the earlier. A HELD control
(1.5.8 step 2c; DEF-67) can. It names up to four atomic sites (`hold-at:`,
`NPKX_HOLD1..4`). The FIRST thread to arrive at one is held and not scheduled
until another thread arrives at the same site; that thread PASSES, keeping the
baton through the site's instruction, and only then may the held one run. A
held thread is also released when nothing else can step, before virtual time
may jump, so a hold never deadlocks a program that would not deadlock without
it. DEF-57's window — one point between a trapper's frozen store and its claim,
with both parties' next step the same claim — is `frozen-traps.ctl`, held at
the claim. A control the
explorer never reaches is `explore-control-blind`, red by name, the models'
rule (§9.4) applied to the explorer. There are fourteen at 1.5.7's close. The
nineteen controls of the models were walked at step 4 and measured against the
floor: eleven became `.ctl`s, five were found not to be floor bugs, one is not
observable and two are not explorable, each with its measurement in the
directory's README (DEF-57 then added two controls to `trap-route` for the error code the model had lacked; both are the model's). The lesson of DEF-57 is the instrument's own: two of those
directed controls had reached their verdicts THROUGH DEF-57's window, not through
the defect they planted. A control proves sight of its OWN defect only when the
schedule that finds it goes through that defect.

**The reference (D-303).** The planning prototype's C shim is kept outside
every gate, under `meta/roadmap/1.5/tools/explore_prototype/`, as the IR shim's
behavioural reference. `hashcmp.sh` runs both shims alternately on every
explorable program and requires the same exit, step count, schedule hash and
verdict word per seed. At 1.5.7's close all 39 agree on 20 seeds each;
`driver_spawn_fail` is the real-child exception.

**What it found.** The explorer's first floor defect was DEF-57, at step 4:
`npk_step`'s `frozen:` block answered the trap flag by trapping `Unreachable`
itself, and could win the failsafe holder in the window between a trapper's
frozen store and its claim, so `failsafe` ran with the wrong error. The
`trap-route` model had no error code and could not see it. The instrument's
own defects found on the way were X-13, X-14, X-16 and X-19, each fixed in both
shims.

**What it does not claim.** WEAK MEMORY: the baton serializes, so every atomic
is explored as sequentially consistent, and an ordering bug in a
release/acquire pair is invisible here; D-290's shared-state belt and the
models speak to it. Programs with REAL CHILD PROCESSES: the ten `explore: no`
programs, each with its reason, printed by the stage on every run. The clone
trampoline and the asm bottom. Schedules beyond the seeds: PCT's per-run
bound is a probability, not a proof. LIVENESS: that a due task eventually runs
is not claimed. TCB.md §5 carries these as acceptances.

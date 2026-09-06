# Nitpick Control Flow Reference

Nitpick provides a robust set of control flow primitives — condition-based and range-based loops, direction-inferring counted loops, state-tracked conditional loops, and advanced pattern matching. C-style three-clause `for` loops are deliberately **not** among them (D-023).

> **⚠️ CRITICAL SYNTAX RULE**: Unlike functions, structs, and traits (which must be terminated with `};`), **control flow blocks do NOT end with semicolons**. Adding a semicolon after the closing brace of an `if`, `while`, or `pick` block will cause a syntax error.

---

## 1. Branching

### 1.1 `if` / `else if` / `else`
Standard conditional branching. Parentheses around the condition are required.

```nitpick
if (x == 1i32) {
    // executes if x is 1
} else if (x == 2i32) {
    // executes if x is 2
} else {
    // catch-all fallback
}
```

### 1.2 `pick` (Switch/Match)
Nitpick's equivalent to `switch` or `match`. Cases are evaluated against the target variable.
*   Case patterns must be wrapped in parentheses: `(value) { body }`
*   Cases must be separated by commas `,`
*   The default/catch-all case is designated by `(*)`
*   **Fallthrough:** Nitpick does not implicitly fall through. To fall through to another case, you must label the target case and use the `fall label;` keyword.
*   **The selector may not be an `Optional`** (D-260, 1.5.2c; `NITPICK-TYPE-065`): an `Optional` has no arms of its own. Reach the value with `??` — `pick (o ?? default) { … }` — or test it with `== NIL`. A frac (D-198) and a complex (D-199) are refused at the selector by the same rule.
*   **One rule set for both spellings** (1.5.2c): the statement form and the expression form (`int32:v = pick (s) { (A) { give 1i32; }, (*) { give 0i32; } };`) apply the same selector rules, type their arm bindings the same way, bind views and freeze the selector alike (D-266), and refuse `move` of a selector that owns nothing alike. Until 1.5.2c the expression form typed no arm binding at all.
*   **Two forms, and what each binds** (D-266, 1.5.2h; `NITPICK-TYPE-066`, `NITPICK-TYPE-067`). A LENDING `pick (v)` binds each pattern name as a **view**: the payload (or destructured field) in place, in the selector's own storage — read-only, typed as the payload, nothing copied at the bind, nothing consumed, no drop of its own. A view is read by value: a field, an element, a plain by-value argument (the lend of D-065), `give`/`pass` of a copyable value; a copy of an owning view is TYPE-046 as everywhere (`.clone()` under a bound is the copy), and a `move` of one, or a `pass` of one out of the function, is TYPE-047 — the value was lent. **A view has no address**: an assignment or compound assignment to it or a part of it, `@`, `$$i`, `$$m`, a call whose receiver is a pointer (the implicit address a method or UFCS call takes), an operation of a stateful kind (an arena, a lock, a guard, an atomic, a channel, a `dyn`), and binding a view of such a kind at all are TYPE-066 — the language has one pointer type and it carries no mutability, so every address of a view is a write path into a lent value. **The selector is frozen while a view of it is live**: inside an arm that binds at least one name (`_` binds nothing), no write may reach the selector's root binding — an assignment to it or any part of it, a `move` of it or a part, `@`/`$$i`/`$$m`, `.destroy()`, a pointer-receiver call, a stateful operation, a nested `pick (move(…))` — TYPE-067; an arm that binds nothing may write it, so `pick (state) { (Idle) { state = Running; } }` stays legal. A CONSUMING `pick (move(v))` (D-216) takes the value apart: its bindings OWN their payloads and drop at the arm's exit, and `v` is moved-from after. A view across an `await` is sound: the selector's root lives to the function's end, exactly as an address-taken local does.

```nitpick
pick (x) {
    (0i32) { println("Zero"); },
    one: (1i32) { fall two; },            // Explicit fallthrough
    two: (2i32) { println("One or Two"); },
    (*) { println("Other"); }             // Default case
}
```

### 1.2.1 `pick` Destructuring

`pick` supports destructuring for structs and enum variants:

```nitpick
pick (event) {
    (MouseClick { x, y }) { ... },
    (Net.Disconnect(reason)) { ... },
    (*) { ... }
}
```

In a lending `pick` these names are views of `event`'s fields and payload
(D-266); in `pick (move(event))` they own them.

### 1.2.2 `pick` Control Modifiers

*   **`fall label;`** — falls through to the labelled arm. There is no implicit fallthrough.
*   **`give expr;`** — yields a value out of the `pick` block when it is used as an expression.

> ⚠️ **`(!)` is removed** (D-061). An earlier revision of this section listed it as
> the *unreachable* marker. It let an arm be **elided from exhaustiveness**, and
> the arm most likely to be elided is the one D-008 requires for a `tbb`
> selector — asserting that a value cannot be ERR, which is the least safe
> assumption available in the type.
>
> An arm the author believes cannot occur is written as an ordinary arm whose body
> is `#unreachable()`: explicit, greppable, and it traps rather than silently
> proceeding. That also retires a second spelling of one idea.

> **`pick` must be exhaustive**, and a `tbb` selector additionally **requires an
> explicit `ERR:` arm** (D-008 §5.1). `(*)` may not absorb the ERR case, or a
> tainted value ends up steering a branch.

### 1.3 `pick` Guards and Macros
The `pick` construct can also match on macro invocations, and individual arms can be guarded by a conditional `where` clause:

```nitpick
pick (ast_node) {
    MyMacro!(a, b) where (a > b) {
        // executes if it matches MyMacro! AND a > b
    }
}
```

### 1.4 Ternary Operator (`is`)
Nitpick supports a ternary conditional operator for concise expression branching. To avoid overlap with the safety/unwrap `?` operator family, Nitpick explicitly uses the `is` keyword rather than `?`.

```nitpick
// Syntax: is (condition) : true_expr : false_expr
int32:max = is (a > b) : a : b;
```

---

## 2. Iteration

Nitpick supports `break;` to exit the innermost loop, and `continue;` to skip to the next iteration across all loop types.

### 2.1 `while` Loop
Standard condition-based loop.

```nitpick
while (x < 10i32) {
    x += 1i32;
}
```

### 2.2 `when` / `then` / `end` (State-Tracked Loop)
A specialized `while` loop that inherently tracks **whether the body ever executed**. It eliminates the need for external state-tracking boolean flags.

```nitpick
when (x > 0i32) {
    // Loop body
    x -= 1i32;
} then {
    // Executes if the body ran AT LEAST ONCE —
    // including when the loop was exited early via `break`
} end {
    // Executes ONLY if the condition was false to begin with,
    // so the body never ran at all
}
```

**`then` and `end` partition the outcomes exactly.** One of them always runs, and
never both:

| Outcome | Clause |
|---|---|
| body ran ≥ 1 time, condition later became false | `then` |
| body ran ≥ 1 time, exited early via `break` | `then` |
| condition false initially — body never ran | `end` |

Both clauses are optional.

> **This corrects an earlier revision** which sent `break` to `end` and reserved
> `then` for normal completion. That grouping made `end` mean *either* "never
> ran" *or* "broke out" — two unrelated outcomes — so telling them apart required
> a boolean flag, which is the exact workaround `when` exists to eliminate.
>
> The rule is simply **"did the body execute?"**, which is the property that
> cannot otherwise be recovered without tracking it manually. Whether a loop
> completed or broke out is normally evident from what the body did or from the
> break condition; whether it ran at all is not. (D-027)

### 2.3 `for` Loop
A range-based iteration loop utilizing type annotations and an iterable or range literal.

```nitpick
for (int64:i in 1..3) {
    // ...
}
```

> **Range form only, with a required typed binding** (D-023). Two forms are
> rejected:
>
> ```nitpick
> // for (int32:i = 0; i < 10; i++)   // C-style three-clause — not supported
> // for (i in 0..10)                 // untyped binding — not supported
> ```
>
> `loop` and `till` already cover everything the three-clause form provides, with
> better safety properties, so supporting both would mean two constructs for one
> job. Keeping `for` to ranges also draws a sharper line: **`for` iterates,
> `loop`/`till` count.**
>
> The typed binding is required because the language forbids implicit type
> inference outright — there is no `auto`, `var`, or `let`.
>
> **What `for` iterates (D-166, 1.0.9):** a range, a slice, an array, or a
> value whose type implements the prelude trait `Iterator` —
> `assoc:Item; func:next = Item?(Self->:self);`, `NIL` ending the loop (the
> `Iterator` form lands at 1.0.9d). **The binding's type must equal the element
> type**: no wrap, no widening. Anything else is refused at the checker by
> name (`NITPICK-TYPE-033`), never at a backend rung.

### 2.4 Counted Iteration (`loop` and `till`)
For rapid, highly-optimized counted iteration, Nitpick offers `loop` and `till`. They automatically manage the iteration counter and expose it inside the block via the special `$` keyword.

> `$` has one other home (1.5.1, V-5; lowered at 1.5.2): inside a `Rules`
> body it is the SUBJECT — the value the rule is asked of (VERIFICATION §2).
> The same node, one meaning, "the value under consideration"; the two
> constructs cannot nest, so no reader has to remember which `$` this is.

**`till(limit, step)`** — the simple form. Counts **up from 0** to `limit`.
```nitpick
till(10i32, 1i32) {
    x += $;  // '$' ranges from 0 to 9
}
```

**`loop(start, limit, step)`** — the controlled form. **Direction is inferred**
from `start` and `limit`.
```nitpick
loop(0i32, 10i32, 1i32) {
    x += $;  // ascending:  0, 1, ..., 9
}

loop(10i32, 0i32, 1i32) {
    x += $;  // descending: 10, 9, ..., 1
}
```

> ### `step` is always positive
>
> Because direction is inferred from the bounds, `step` controls **only the size
> of the jump**. A negative step is a **compile error**; so is a zero step, which
> could not terminate. Where the step is not a literal, `--verify` discharges
> `step > 0` as a proof obligation, falling back to a runtime check that traps to
> `failsafe`.
>
> This makes a whole bug class unrepresentable. In C-style loops, pairing an
> ascending range with a negative step produces an infinite loop, and the mistake
> is invisible because the sign lives in a different clause from the bounds. Here
> the programmer never states the direction, so it cannot fall out of sync with
> the bounds. (D-022)

| Case | Behavior |
|---|---|
| `step` negative or zero | compile error |
| `start == limit` | zero iterations |
| `till` with `limit <= 0` | zero iterations — `till` ascends from `0` |
| a bound is `tbb` holding ERR | traps to `failsafe` — a loop bound is a control-flow decision (D-008 §5) |

`till` and `loop` are **not redundant**: `till` ascends from zero only, while
`loop` handles arbitrary start points and both directions.

**There is no `loop { }` infinite form and no do-while construct.** `while (true)`
is the idiom for an unbounded loop. `FORMAL_DRAFT` 05 §5.4.3–5.4.4 defines `till`
as do-while and `loop` as infinite; that reading is **struck** (D-022).

### 2.5 Loop Labels

To break out of nested loops, a loop may be labelled with an identifier and a
colon:

```nitpick
outer: while (true) {
    while (true) {
        if (fatal_error) {
            break outer;
        }
    }
}
```

`break label;` and `continue label;` both target a labelled loop.

---

## 3. Discarding Values

To deliberately ignore an expression value, unused variable, or unused parameter and suppress compiler warnings, Nitpick provides the `discard` statement.

*   **`discard(expr);`**: Discards the provided expression.
*   **`_~ expr;`**: A shorthand prefix operator that desugars to `discard()`.

```nitpick
int32:unused_val = 100i32;
discard(unused_val);

// Or using the shorthand operator
_~ 42i32;
```

---

## 4. Statement-Level Constructs

Adopted from `FORMAL_DRAFT` 05 with corrections. See
`GRAMMAR_ADOPTION_CONFLICTS.md`.

### 4.1 Block Statements

A block is zero or more statements in braces `{ … }`. Blocks introduce a lexical
scope: variables declared inside are invisible outside, and scope-managed
bindings are destroyed at the closing brace.

### 4.2 Condition Pedantry

Two checks that the compiler enforces aggressively:

*   **`NITPICK-IF-002`** — an `if` condition must evaluate to a strict `bool`, and
    the assignment operator is **rejected** inside it. `if (x = 3)` is a compile
    error; use `==`.
*   **`NITPICK-IF-001`** — an `else` without an immediately preceding `if` is an
    error rather than a cascade into vague expression parsing.
*   **`NITPICK-WHEN-001`** — an orphaned `then` or `end` without a preceding `when`.

### 4.3 Returns

*   **`pass expr;`** — returns a successful `Result<T>`.
*   **`fail errCode;`** — returns an errored `Result<T>`.
*   **`relay expr`** / **`_^ expr`** — *(expression, not a statement)* propagates:
    if `expr` is an error the enclosing function returns immediately **with that
    same error code, verbatim**; otherwise the expression yields `.value`
    (D-080). This is the ordinary way to forward a failure, and it exists because
    hand-writing the equivalent — `if (r.is_error) { fail r.err; }` — is
    written wrong often enough to matter: the prototype's own stdlib forwards the
    original code in **0 of 19** such sites, substituting `fail 1;`.
*   **`return Result{ … };`** — the literal form, the only way to return a value
    *and* an error simultaneously.

> `FORMAL_DRAFT` 05 §5.7 describes `pass` as sugar for `return ok(expr);` and
> `fail` as `return err(expr);`. **Both are wrong.** `ok()` is the taint-clearing
> builtin, not a `Result` constructor, and `err()` does not exist. `pass` and
> `fail` construct `Result` directly — see `TYPE_REFERENCE.md` §11.2 for the
> desugaring.

### 4.4 Assertions

*   **`prove(cond);`** — a **compile-time** proof obligation discharged by Z3 under
    `--verify`. Path-condition aware: enclosing branch guards are asserted as
    axioms first. If the solver finds a counterexample, **compilation fails**.
*   **`assert_static(cond);`** — compile-time constant evaluation; halts
    compilation if false.

> `FORMAL_DRAFT` 05 §5.8 calls `prove` "a **runtime** assertion" that "panics
> immediately". **That is wrong** and is struck — it contradicts
> `VERIFICATION_REFERENCE.md` §1.2 and `FORMAL_DRAFT` 12.5.1, both of which
> specify it in far greater detail as a compile-time Z3 obligation.

### 4.5 `defer`

Pushes a block onto a stack to run when the enclosing lexical scope exits.

```nitpick
wild int8->:buf = alloc(16i64);
defer { dalloc(buf); }
```

Runs on **every normal exit path** — scope end, `return`, `pass`, `fail`, `relay`, `exit` — **after the exit's value is evaluated** (D-136): `pass v` returns the `v` that was read at the `pass`, whatever the defers then do. LIFO, innermost scope first.

> **`defer` does NOT run on a trap** (D-014). `!!!` and `?!` transfer control
> directly to `failsafe` without unwinding. At trap time the state of the system
> is unknown, including how degraded it is, so no cleanup runs before the handler
> that understands the situation gets control.
>
> `FORMAL_DRAFT` 05 §5.9 says `defer` runs "via a panic". **Struck.**

### 4.6 `exit` and K-Semantics

**`exit code;`** terminates the process, and may appear only in `main` or
`failsafe`.

A successful `exit` requires that **no unchecked manual memory remains
allocated** — the `<wildx-states>` map must be empty. Reaching `exit` with live
`wild` or `wildx` memory triggers the `failsafe` trap instead of returning;
`failsafe` is then permitted to clean up and exit cleanly.

This is why a memory leak is a *detected, controlled* condition in Nitpick rather
than silent corruption — and it is what lets arenas replace a collector without
losing the leak guarantee.

> **Made real at 0.10.1 (D-151).** "Successful" is load-bearing: the check
> runs on `exit 0` — a failure exit keeps its code, because overwriting an
> error report with a leak trap would destroy the error it was raising. A
> non-empty `<wild-live>` routes `-4105` to `failsafe`, which may call
> `wild_release_all()` and exit positive; `failsafe`'s own exit is exempt
> (the in-failsafe flag — the check runs once, at the program's exit), and a
> trap raised *inside* `failsafe` exits 70 directly rather than recursing.
> `wild_live_count()` is the program-visible view of the set. Managed-regime
> storage (string bodies — runtime-internal until its RAII lowering lands)
> is not in the set: the rule is about the `wild` regime, exactly as the
> paragraph above scopes it.

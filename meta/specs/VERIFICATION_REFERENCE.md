# Nitpick Formal Verification & Design by Contract

Nitpick fundamentally rejects unsafe behavior. To achieve this, it deeply integrates with the Z3 SMT solver to mathematically prove the correctness of the code before it is allowed to execute.

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

When you compile with `--verify`, the compiler's integrated Z3 solver will mathematically prove that the assigned value (`5i32`) satisfies the constraint (`$ > 0i32`). If it cannot prove it statically (for instance, reading user input), it will enforce the check at runtime. If the runtime check fails, it triggers the `failsafe` handler.

### 2.1 Rules Composition and Subsumption

Rules can reference other Rules via `limit<OtherRule>`, creating a constraint hierarchy:

```nitpick
Rules<int32>:r_positive = { $ > 0i32 };
Rules<int32>:r_small_positive = { limit<r_positive>, $ < 100i32 };
// r_small_positive requires: $ > 0i32 AND $ < 100i32
```

The Z3 solver can prove that one Rules block subsumes another (e.g., `r_small_positive` mathematically implies `r_positive`), enabling safe narrowing at call sites without redundant checks.

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

### 3.1 The `Result<T>` Intercept

One of the most powerful features of Nitpick's DbC implementation is how it interacts with the type system. If a function declares a `requires` clause, **Nitpick implicitly ensures its return type is wrapped in a `Result<T>`**. 

If a caller violates the precondition at runtime, the function immediately intercepts execution and returns a `Result` error rather than crashing or triggering the failsafe. This heavily intertwines contract programming with Nitpick's sticky error propagation system, forcing the caller to explicitly unwrap or handle potential contract violations using `raw`, `drop`, or `.is_error`.

```nitpick
func:main = int32() {
    // Because `divide` has a `requires` contract, we must 
    // unwrap it with `raw` or handle the potential failure.
    int32:y = raw divide(10i32, 2i32);
    
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
    };
    pass(total);
};
```

When compiled with `--verify-contracts`, the Z3 solver verifies the inductive step: if the invariant holds at the start of an iteration and the loop condition is true, then the invariant still holds at the end of the iteration.

---

## 5. Verification Compiler Flags

| Flag | Purpose |
|------|---------|
| `--verify` | Enable Z3 Rules/limit verification |
| `--verify-contracts` | Verify requires/ensures/invariant contracts |
| `--verify-overflow` | Verify integer arithmetic overflow |
| `--verify-concurrency` | Verify data race & deadlock freedom |
| `--verify-memory` | Verify use-after-free & recursion bounds |
| `--verify-level=N` | Controls verification depth (see table below) |
| `--smt-opt` | Enable SMT-guided optimizations (eliminates proven-safe checks) |
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

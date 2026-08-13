# Nitpick Control Flow Reference

Nitpick provides a robust set of control flow primitives ranging from standard C-style loops to state-tracked conditional loops and advanced pattern matching.

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

```nitpick
pick (x) {
    (0i32) { println("Zero"); },
    one: (1i32) { fall two; },            // Explicit fallthrough
    two: (2i32) { println("One or Two"); },
    (*) { println("Other"); }             // Default case
}
```

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
A specialized `while` loop that inherently tracks *how* the loop terminated. It eliminates the need for external state-tracking boolean flags.

```nitpick
when (x > 0i32) {
    // Loop body
    x -= 1i32;
} then {
    // Executes ONLY if the loop completed normally (i.e. condition became false)
} end {
    // Executes ONLY if the condition was false to begin with, 
    // OR if the loop exited prematurely via a `break`
}
```

### 2.3 `for` Loop
A range-based iteration loop utilizing type annotations and an iterable or range literal.

```nitpick
for (int64:i in 1..3) {
    // ...
}
```

### 2.4 Counted Iteration (`loop` and `till`)
For rapid, highly-optimized counted iteration, Nitpick offers `loop` and `till`. They automatically manage the iteration counter and expose it inside the block via the special `$` keyword.

**`loop(start, limit, step)`**
```nitpick
loop(0i32, 10i32, 1i32) {
    x += $;  // '$' resolves to the current iteration counter (0, 1, ..., 9)
}
```

**`till(limit, step)`** (Shorthand when starting from 0)
```nitpick
till(10i32, 1i32) {
    x += $;  // '$' ranges from 0 to 9
}
```

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

# For Loops

**Category**: Control Flow → Iteration  
**Keywords**: `for`, `in`, `..`, `...`  
**Philosophy**: Two forms — C-style for precise control; range-based for clean, safe iteration

---

## Overview

Aria has two kinds of `for` loops:

| Form | Syntax | Best for |
|---|---|---|
| C-style | `for(init; cond; update)` | Counting, custom step, complex control |
| Range-based | `for (i in start..end)` | Iterating over a known range of values |

Both forms accept a typed variable declaration using Aria's `type:name` syntax.

---

## C-Style For Loop

### Basic Syntax

```aria
for(int32:i = 0i32; i < 10i32; i += 1i32) {
    // body — i goes 0, 1, 2, ..., 9
}
```

The three clauses:
- **init**: variable declaration, e.g. `int32:i = 0i32`
- **condition**: boolean expression evaluated before each iteration
- **update**: expression executed after each iteration body, e.g. `i += 1i32`

> **Note**: `i++` is not supported. Use `i += 1i32` or `i = i + 1i32`.

### Count Up

```aria
for(int32:i = 1i32; i <= 5i32; i += 1i32) {
    drop(println(int32_toString(i)));
}
// Output:
// 1
// 2
// 3
// 4
// 5
```

### Count Down

```aria
for(int32:i = 5i32; i >= 1i32; i = i - 1i32) {
    drop(println(int32_toString(i)));
}
// Output:
// 5
// 4
// 3
// 2
// 1
```

### Custom Step

```aria
for(int32:i = 0i32; i <= 20i32; i += 5i32) {
    drop(println(int32_toString(i)));
}
// Output: 0  5  10  15  20
```

### All Clauses Optional

```aria
// No initializer — use existing variable
int32:i = 3i32;
for(; i < 6i32; i += 1i32) {
    drop(println(int32_toString(i)));
}
// Output: 3  4  5
```

---

## Range-Based For Loop

### Basic Syntax

```aria
for (i in start..end) {
    // i goes start, start+1, ..., end  (inclusive)
}

for (i in start...end) {
    // i goes start, start+1, ..., end-1  (exclusive upper bound)
}
```

- `..` — **inclusive** range: both `start` and `end` are visited
- `...` — **exclusive** range: `start` is visited, `end` is not

The iterator variable is `int64` by default.

### Inclusive Range

```aria
for (i in 1..5) {
    // i = 1, 2, 3, 4, 5
    drop(println(int64_toString(i)));
}
// Output:
// 1
// 2
// 3
// 4
// 5
```

### Exclusive Range

```aria
for (i in 1...5) {
    // i = 1, 2, 3, 4  (5 not visited)
    drop(println(int64_toString(i)));
}
// Output:
// 1
// 2
// 3
// 4
```

### Range with Variables

```aria
int64:start = 0;
int64:finish = 3;
for (i in start..finish) {
    drop(println(int64_toString(i)));
}
// Output: 0  1  2  3
```

### With Type Annotation

You can declare the iterator type explicitly:

```aria
for (int64:i in 1..3) {
    // i is explicitly int64
}

for (int32:j in 10...13) {
    // j is int32, range values are narrowed
}
```

---

## Break and Continue

Both `break` and `continue` work in both for forms:

```aria
// Skip even numbers
for(int32:i = 1i32; i <= 10i32; i += 1i32) {
    if (i % 2i32 == 0i32) { continue; }
    drop(println(int32_toString(i)));
}
// Output: 1  3  5  7  9

// Stop at first match
for (i in 1..100) {
    if (i % 7 == 0) {
        drop(println("First multiple of 7:"));
        drop(println(int64_toString(i)));
        break;
    }
}
// Output:
// First multiple of 7:
// 7
```

---

## Nested For Loops

```aria
for(int32:i = 1i32; i <= 3i32; i += 1i32) {
    for(int32:j = 1i32; j <= 3i32; j += 1i32) {
        // Process (i, j) pair
    }
}
```

---

## For vs Other Loop Forms

| Loop | Use when |
|---|---|
| `for(;;)` | You need a counter with precise init/step/condition control |
| `for (i in a..b)` | Iterating a straightforward numeric range |
| `while(cond)` | Condition depends on external state, no counter needed |
| `loop(start,end,step)` | You want auto-tracked `$` index without a named variable |
| `till(end,step)` | Same as `loop` but starts from 0 |

---

## Common Patterns

### Accumulate

```aria
int32:sum = 0i32;
for(int32:i = 1i32; i <= 100i32; i += 1i32) {
    sum = sum + i;
}
// sum = 5050
```

### Fizzbuzz (Range-Based)

```aria
for (i in 1..30) {
    bool:fizz = (i % 3 == 0);
    bool:buzz = (i % 5 == 0);
    if (fizz && buzz)      { drop(println("FizzBuzz")); }
    else if (fizz)         { drop(println("Fizz")); }
    else if (buzz)         { drop(println("Buzz")); }
    else                   { drop(println(int64_toString(i))); }
}
```

---

## Related Topics

- [For Syntax](for_syntax.md) — Complete syntax reference
- [While](while.md) — Condition-based loops
- [Loop](loop.md) — Counted loop with auto `$` index
- [Till](till.md) — 0-based counted loop with auto `$` index
- [Break](break.md) — Exit loops early
- [Continue](continue.md) — Skip to next iteration

---

**Remember**: Use `for(;;)` when you need a counter with explicit control. Use `for (i in a..b)` for clean range iteration. Both are fully implemented and compile to efficient LLVM IR.

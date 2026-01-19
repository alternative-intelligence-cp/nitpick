# The `till` Loop (Automatic Iteration Variable)

**Category**: Control Flow → Loops  
**Syntax**: `till(limit, step) { body }`  
**Purpose**: Loop with automatic iteration tracking via `$` variable

---

## Overview

The `till` loop provides automatic iteration counting with the special `$` variable. The direction (counting up or down) is determined by the **sign of the step** parameter.

**Philosophy**: Minimal syntax with automatic iteration tracking, bidirectional based on step sign.

---

## Basic Syntax

```aria
till(limit, step) {
    // $ variable automatically available
    // Contains current iteration value
}
```

### Direction Rules

| Step Sign | Direction | Start | End | Example |
|-----------|-----------|-------|-----|---------|
| Positive (`+`) | Count UP | 0 | limit | `till(100, 1)` → 0 to 100 |
| Negative (`-`) | Count DOWN | limit | 0 | `till(100, -1)` → 100 to 0 |

**Key**: Step sign determines direction, step magnitude determines increment size.

---

## Counting Up (Positive Step)

### Example 1: Count from 0 to 100

```aria
// Positive step: counts UP from 0 to limit
till(100, 1) {
    print(`Iteration: &{$}`);
    // $ goes: 0, 1, 2, 3, ..., 99, 100
}
```

### Example 2: Count by 2s

```aria
// Step of 2: counts by 2s
till(10, 2) {
    print(`Even: &{$}`);
    // $ goes: 0, 2, 4, 6, 8, 10
}
```

### Example 3: Count by 5s

```aria
// Step of 5: counts by 5s
till(100, 5) {
    print(`Five: &{$}`);
    // $ goes: 0, 5, 10, 15, 20, ..., 95, 100
}
```

---

## Counting Down (Negative Step)

### Example 1: Count from 100 to 0

```aria
// Negative step: counts DOWN from limit to 0
till(100, -1) {
    print(`Countdown: &{$}`);
    // $ goes: 100, 99, 98, ..., 2, 1, 0
}
```

### Example 2: Count down by 2s

```aria
// Negative step of -2: counts down by 2s
till(100, -2) {
    print(`Down by 2: &{$}`);
    // $ goes: 100, 98, 96, ..., 4, 2, 0
}
```

### Example 3: Countdown Timer

```aria
// 10-second countdown
till(10, -1) {
    print(`T-minus &{$} seconds`);
    sleep(1000);  // 1 second
}
print("Liftoff!");
// Output: T-minus 10, 9, 8, ..., 1, 0, Liftoff!
```

---

## The `$` Variable

### Automatic Declaration

The `$` variable is **automatically declared** within the loop body:

```aria
till(5, 1) {
    // $ is automatically available
    // No need to declare it
    int64:doubled = $ * 2;
    print(`$ = &{$}, doubled = &{doubled}`);
}
```

### Read-Only

The `$` variable is **read-only** - you cannot modify it:

```aria
till(10, 1) {
    // WRONG: Cannot assign to $
    $ = 5;  // ❌ Compile error
    
    // CORRECT: Use $ as input to calculations
    int64:value = $ * 10;  // ✅
}
```

### Scope

`$` is only valid **inside** the loop body:

```aria
till(5, 1) {
    print(`&{$}`);  // ✅ Valid
}

print(`&{$}`);  // ❌ Error: $ not defined outside loop
```

---

## Common Patterns

### Pattern 1: Array Initialization

```aria
int64[100]:array;

till(100, 1) {
    array[$] = $ * 2;  // Fill with even numbers
}

// array = [0, 2, 4, 6, ..., 198]
```

### Pattern 2: Sum of Range

```aria
int64:sum = 0;

till(100, 1) {
    sum += $;
}

// sum = 0 + 1 + 2 + ... + 100 = 5050
```

### Pattern 3: Countdown Timer

```aria
till(10, -1) {
    print(`&{$}...`);
    sleep(1000);
}
print("Done!");
```

### Pattern 4: Skip by N

```aria
// Process every 10th item
till(100, 10) {
    processItem($);
    // Processes: 0, 10, 20, 30, ..., 100
}
```

---

## Comparison with `loop`

Aria has two automatic iteration constructs:

### `till` - Direction from Step Sign

```aria
// Positive step: count UP from 0
till(100, 1) {
    // $ = 0, 1, 2, ..., 100
}

// Negative step: count DOWN from limit
till(100, -1) {
    // $ = 100, 99, 98, ..., 0
}
```

### `loop` - Direction from Start vs Limit

```aria
// start < limit: count UP
loop(1, 100, 1) {
    // $ = 1, 2, 3, ..., 100
}

// start > limit: count DOWN
loop(100, 0, 2) {
    // $ = 100, 98, 96, ..., 2, 0
}
```

**Key Difference**: 
- `till` uses **step sign** for direction, always starts at 0 or limit
- `loop` uses **start vs limit** for direction, step is always positive magnitude

---

## Edge Cases

### Step of 0

```aria
// Infinite loop (step doesn't advance)
till(100, 0) {
    print("Infinite!");  // ⚠️ Runs forever
}
```

### Negative Limit

```aria
// Still counts from 0 to negative number
till(-10, -1) {
    print(`&{$}`);
    // $ goes: -10, -9, -8, ..., -1, 0
}
```

### Fractional Step (Future)

```aria
// May support in future for floating-point iteration
till(1.0, 0.1) {
    // $ = 0.0, 0.1, 0.2, ..., 1.0
}
```

---

## Break and Continue

### Break Early

```aria
till(100, 1) {
    if ($ > 50) {
        break;  // Exit loop when $ exceeds 50
    }
    print(`&{$}`);
}
// Prints: 0, 1, 2, ..., 50
```

### Skip Iterations

```aria
till(20, 1) {
    if ($ % 2 == 0) {
        continue;  // Skip even numbers
    }
    print(`Odd: &{$}`);
}
// Prints: 1, 3, 5, 7, 9, ..., 19
```

---

## Nested `till` Loops

### Nested Iteration

```aria
// ⚠️ IMPORTANT: $ is shadowed in nested loops
till(3, 1) {
    int64:outer = $;  // Save outer $ value
    
    till(3, 1) {
        // Inner $ shadows outer $
        print(`Outer: &{outer}, Inner: &{$}`);
    }
}
```

### Matrix Initialization

```aria
int64[10][10]:matrix;

till(10, 1) {
    int64:row = $;
    till(10, 1) {
        int64:col = $;
        matrix[row][col] = row * 10 + col;
    }
}
```

---

## Performance

### Zero Overhead

The `till` loop compiles to the same machine code as a manual counter:

```aria
// This till loop...
till(100, 1) {
    process($);
}

// ...compiles identically to:
for (int64:i = 0; i <= 100; i++) {
    process(i);
}
```

**No runtime cost for automatic iteration.**

---

## Use Cases

### Use Case 1: Simple Iteration

```aria
// When you just need to iterate N times
till(iterations, 1) {
    performTask();
}
```

### Use Case 2: Countdown Sequences

```aria
// Natural countdown syntax
till(10, -1) {
    print(`&{$}...`);
}
```

### Use Case 3: Array Processing

```aria
int64[]:data = loadData();

till(data.length - 1, 1) {
    processElement(data[$]);
}
```

### Use Case 4: Stepped Iteration

```aria
// Process every Nth element
till(1000, 10) {
    processEveryTenth($);
}
```

---

## Best Practices

### ✅ Use till for Simple Counting

```aria
// GOOD: Clean and simple
till(100, 1) {
    doSomething($);
}
```

### ✅ Save $ if Needed in Nested Loops

```aria
// GOOD: Preserve outer iteration value
till(10, 1) {
    int64:outer = $;
    till(10, 1) {
        useValues(outer, $);
    }
}
```

### ✅ Use Negative Step for Countdown

```aria
// GOOD: Natural countdown
till(60, -1) {
    updateTimer($);
}
```

### ❌ Don't Modify $ (Compiler Error Anyway)

```aria
// WRONG: $ is read-only
till(10, 1) {
    $ = 5;  // ❌ Error
}
```

### ❌ Don't Use for Complex Iteration Logic

```aria
// BAD: Complex start/end/step logic
till(complexLimit, complexStep) {
    // Hard to understand
}

// GOOD: Use for loop for clarity
for (int64:i = start; i < end; i += customStep) {
    // Clear iteration logic
}
```

---

## Comparison with Other Languages

### Aria `till`

```aria
till(10, 1) {
    print(`&{$}`);
}
```

### Python `range`

```python
for i in range(11):  # 0 to 10
    print(i)
```

### Rust `..=`

```rust
for i in 0..=10 {
    println!("{}", i);
}
```

### Ruby `times`

```ruby
10.times do |i|
    puts i
end
```

### C/C++

```c
for (int i = 0; i <= 10; i++) {
    printf("%d\n", i);
}
```

---

## Advanced Examples

### Example 1: Fibonacci Sequence

```aria
int64:a = 0;
int64:b = 1;

till(10, 1) {
    print(`Fib(&{$}) = &{a}`);
    int64:next = a + b;
    a = b;
    b = next;
}
```

### Example 2: Factorial

```aria
int64:factorial = 1;

till(10, 1) {
    if ($ > 0) {
        factorial *= $;
    }
}
// factorial = 10! = 3628800
```

### Example 3: Prime Number Check

```aria
func:isPrime = bool(int64:n) {
    if (n < 2) { pass(false); }
    
    bool:prime = true;
    till(n / 2, 1) {
        if ($ > 1 && n % $ == 0) {
            prime = false;
            break;
        }
    }
    pass(prime);
};
```

### Example 4: Nested Grid

```aria
// 10x10 grid
till(10, 1) {
    int64:row = $;
    till(10, 1) {
        int64:col = $;
        print(`(&{row}, &{col}) `);
    }
    print("\n");
}
```

---

## Related Topics

- [loop Construct](loop.md) - Alternative automatic iteration with start/limit/step
- [for Loop](for.md) - Traditional C-style for loop
- [while Loop](while.md) - Condition-based looping
- [Dollar Variable](../operators/dollar_variable.md) - The `$` iteration variable
- [break/continue](break_continue.md) - Loop control flow

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Lines 428-441  
**Unique Feature**: Direction determined by step sign, not start vs limit

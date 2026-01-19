# The `loop` Construct (Bidirectional Iteration)

**Category**: Control Flow → Loops  
**Syntax**: `loop(start, limit, step) { body }`  
**Purpose**: Automatic iteration with direction determined by start vs limit

---

## Overview

The `loop` construct provides automatic bidirectional iteration with the special `$` variable. Direction is determined by comparing **start** to **limit**, not by the step sign.

**Philosophy**: Intuitive iteration - if start < limit, count up; if start > limit, count down.

---

## Basic Syntax

```aria
loop(start, limit, step) {
    // $ variable automatically available
    // Contains current iteration value
}
```

### Direction Rules

| Condition | Direction | $ Range | Example |
|-----------|-----------|---------|---------|
| `start < limit` | Count UP | start to limit | `loop(1, 10, 1)` → 1 to 10 |
| `start > limit` | Count DOWN | start to limit | `loop(10, 1, 1)` → 10 to 1 |
| `start == limit` | Single iteration | start only | `loop(5, 5, 1)` → 5 |

**Key**: Direction from start/limit comparison, step is always **positive magnitude**.

---

## Counting Up (start < limit)

### Example 1: Count 1 to 10

```aria
// start < limit: counts UP
loop(1, 10, 1) {
    print(`&{$}`);
    // $ goes: 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
}
```

### Example 2: Count by 2s

```aria
// Step of 2: increments by 2
loop(0, 20, 2) {
    print(`Even: &{$}`);
    // $ goes: 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20
}
```

### Example 3: Start at Offset

```aria
// Start at 50, count to 60
loop(50, 60, 1) {
    print(`&{$}`);
    // $ goes: 50, 51, 52, ..., 59, 60
}
```

---

## Counting Down (start > limit)

### Example 1: Count 10 to 1

```aria
// start > limit: counts DOWN
loop(10, 1, 1) {
    print(`Countdown: &{$}`);
    // $ goes: 10, 9, 8, 7, 6, 5, 4, 3, 2, 1
}
```

### Example 2: Count down by 3s

```aria
// Step of 3: decrements by 3
loop(30, 0, 3) {
    print(`Down: &{$}`);
    // $ goes: 30, 27, 24, 21, 18, 15, 12, 9, 6, 3, 0
}
```

### Example 3: Reverse Array Traversal

```aria
int64[]:data = [10, 20, 30, 40, 50];

loop(data.length - 1, 0, 1) {
    print(`data[&{$}] = &{data[$]}`);
}
// Output: data[4] = 50, data[3] = 40, ..., data[0] = 10
```

---

## The `$` Variable

### Automatic Declaration

The `$` variable is **automatically declared** and available in the loop body:

```aria
loop(1, 5, 1) {
    // $ is automatically available
    int64:squared = $ * $;
    print(`&{$} squared = &{squared}`);
}
```

### Read-Only

You **cannot modify** the `$` variable:

```aria
loop(1, 10, 1) {
    // WRONG: Cannot assign to $
    $ = 5;  // ❌ Compile error
    
    // CORRECT: Use $ in expressions
    int64:value = $ + 10;  // ✅
}
```

### Scope

`$` is only valid **inside** the loop body:

```aria
loop(1, 5, 1) {
    print(`&{$}`);  // ✅ Valid
}

print(`&{$}`);  // ❌ Error: $ undefined outside loop
```

---

## Step Behavior

### Always Positive Magnitude

The step is always a **positive magnitude** - direction comes from start vs limit:

```aria
// Correct: step is positive, direction from start/limit
loop(10, 1, 1) {
    // Counts DOWN: 10, 9, 8, ..., 1
}

// Wrong: negative step is invalid
loop(10, 1, -1) {  // ❌ Error: step must be positive
}
```

### Step Size Determines Increment

```aria
// Step of 5: increments/decrements by 5
loop(0, 50, 5) {
    // Counts UP: 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50
}

loop(50, 0, 5) {
    // Counts DOWN: 50, 45, 40, 35, 30, 25, 20, 15, 10, 5, 0
}
```

---

## Common Patterns

### Pattern 1: Array Iteration (Forward)

```aria
int64[]:array = [10, 20, 30, 40, 50];

loop(0, array.length - 1, 1) {
    print(`array[&{$}] = &{array[$]}`);
}
```

### Pattern 2: Array Iteration (Reverse)

```aria
int64[]:array = [10, 20, 30, 40, 50];

loop(array.length - 1, 0, 1) {
    print(`array[&{$}] = &{array[$]}`);
}
```

### Pattern 3: Range Processing

```aria
// Process values from 100 to 200
loop(100, 200, 10) {
    processValue($);
    // Processes: 100, 110, 120, ..., 200
}
```

### Pattern 4: Countdown Timer

```aria
loop(60, 0, 1) {
    print(`&{$} seconds remaining`);
    sleep(1000);  // 1 second
}
print("Time's up!");
```

---

## Comparison with `till`

Aria has two automatic iteration constructs with different direction semantics:

### `loop` - Direction from Start vs Limit

```aria
// start < limit: count UP
loop(1, 100, 1) {
    // $ = 1, 2, 3, ..., 100
}

// start > limit: count DOWN
loop(100, 1, 1) {
    // $ = 100, 99, 98, ..., 1
}
```

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

**Use `loop` when**: You need explicit start/end points  
**Use `till` when**: You want to count from 0 or count down from limit

---

## Edge Cases

### Start Equals Limit

```aria
// Executes exactly once with $ = start
loop(5, 5, 1) {
    print(`&{$}`);
}
// Output: 5
```

### Step Larger Than Range

```aria
// Step exceeds range: executes once
loop(1, 10, 20) {
    print(`&{$}`);
}
// Output: 1
```

### Step of 0

```aria
// Infinite loop (step doesn't advance)
loop(1, 10, 0) {
    print("Infinite!");  // ⚠️ Runs forever
}
```

### Negative Start/Limit

```aria
// Works with negative numbers
loop(-10, -1, 1) {
    print(`&{$}`);
}
// Output: -10, -9, -8, -7, -6, -5, -4, -3, -2, -1
```

---

## Break and Continue

### Break Early

```aria
loop(1, 100, 1) {
    if ($ > 50) {
        break;  // Exit loop when $ exceeds 50
    }
    print(`&{$}`);
}
// Prints: 1, 2, 3, ..., 50
```

### Skip Iterations

```aria
loop(1, 20, 1) {
    if ($ % 2 == 0) {
        continue;  // Skip even numbers
    }
    print(`Odd: &{$}`);
}
// Prints: 1, 3, 5, 7, ..., 19
```

---

## Nested Loops

### Shadow Behavior

```aria
// ⚠️ IMPORTANT: $ is shadowed in nested loops
loop(1, 3, 1) {
    int64:outer = $;  // Save outer $ value
    
    loop(1, 3, 1) {
        // Inner $ shadows outer $
        print(`Outer: &{outer}, Inner: &{$}`);
    }
}
```

### 2D Grid Iteration

```aria
// Iterate over 10x10 grid
loop(0, 9, 1) {
    int64:row = $;
    loop(0, 9, 1) {
        int64:col = $;
        processCell(row, col);
    }
}
```

---

## Performance

### Zero-Cost Abstraction

The `loop` construct compiles to the same machine code as a manual counter:

```aria
// This loop...
loop(1, 100, 1) {
    process($);
}

// ...compiles identically to:
for (int64:i = 1; i <= 100; i++) {
    process(i);
}
```

**No runtime overhead for automatic iteration.**

---

## Use Cases

### Use Case 1: Array Processing

```aria
int64[]:data = loadData();

loop(0, data.length - 1, 1) {
    processElement(data[$]);
}
```

### Use Case 2: Range-Based Operations

```aria
// Process values 50 to 150
loop(50, 150, 5) {
    performOperation($);
}
```

### Use Case 3: Reverse Iteration

```aria
// Process array backwards
loop(array.length - 1, 0, 1) {
    cleanupElement(array[$]);
}
```

### Use Case 4: Stepped Iteration

```aria
// Process every 10th element
loop(0, 1000, 10) {
    processEveryTenth($);
}
```

---

## Best Practices

### ✅ Use loop for Explicit Ranges

```aria
// GOOD: Clear start and end
loop(10, 100, 5) {
    processRange($);
}
```

### ✅ Save $ in Nested Loops

```aria
// GOOD: Preserve outer iteration value
loop(1, 10, 1) {
    int64:outer = $;
    loop(1, 10, 1) {
        useValues(outer, $);
    }
}
```

### ✅ Use Positive Step Always

```aria
// GOOD: Step is positive, direction from start/limit
loop(100, 1, 1) {
    // Counts down automatically
}
```

### ❌ Don't Use Negative Step

```aria
// WRONG: Step must be positive
loop(10, 1, -1);  // ❌ Error
```

### ❌ Don't Assume $ Direction

```aria
// BAD: Unclear what direction $ will go
loop(x, y, 1) {
    // Is this counting up or down?
}

// GOOD: Make direction clear
if (x < y) {
    loop(x, y, 1) { /* up */ }
} else {
    loop(x, y, 1) { /* down */ }
}
```

---

## Comparison with Other Languages

### Aria `loop`

```aria
loop(1, 10, 1) {
    print(`&{$}`);
}
```

### Python `range`

```python
for i in range(1, 11):  # 1 to 10
    print(i)
```

### Rust `..=`

```rust
for i in 1..=10 {
    println!("{}", i);
}
```

### Ruby `upto`

```ruby
1.upto(10) do |i|
    puts i
end
```

### C/C++ `for`

```c
for (int i = 1; i <= 10; i++) {
    printf("%d\n", i);
}
```

---

## Advanced Examples

### Example 1: Collatz Sequence

```aria
int64:n = 27;

while (n != 1) {
    print(`&{n}`);
    n = is (n % 2 == 0) : n / 2 : n * 3 + 1;
}

// Combined with loop:
loop(1, 100, 1) {
    int64:n = $;
    int64:steps = 0;
    
    while (n != 1) {
        n = is (n % 2 == 0) : n / 2 : n * 3 + 1;
        steps++;
    }
    
    print(`Collatz(&{$}) takes &{steps} steps`);
}
```

### Example 2: Matrix Multiplication

```aria
int64[10][10]:A;
int64[10][10]:B;
int64[10][10]:C;

loop(0, 9, 1) {
    int64:i = $;
    loop(0, 9, 1) {
        int64:j = $;
        int64:sum = 0;
        
        loop(0, 9, 1) {
            int64:k = $;
            sum += A[i][k] * B[k][j];
        }
        
        C[i][j] = sum;
    }
}
```

### Example 3: Prime Sieve

```aria
bool[1000]:isPrime;

// Initialize all as prime
loop(2, 999, 1) {
    isPrime[$] = true;
}

// Sieve
loop(2, 999, 1) {
    int64:p = $;
    if (isPrime[p]) {
        // Mark multiples as not prime
        loop(p * 2, 999, p) {
            isPrime[$] = false;
        }
    }
}

// Print primes
loop(2, 999, 1) {
    if (isPrime[$]) {
        print(`&{$}`);
    }
}
```

### Example 4: Histogram

```aria
int64[10]:histogram;  // Bins 0-9

// Count occurrences
loop(0, data.length - 1, 1) {
    int64:value = data[$];
    int64:bin = value / 10;  // Group into bins
    histogram[bin]++;
}

// Display
loop(0, 9, 1) {
    print(`Bin &{$}: &{histogram[$]}`);
}
```

---

## Related Topics

- [till Loop](till.md) - Alternative automatic iteration with step sign
- [for Loop](for.md) - Traditional C-style for loop  
- [while Loop](while.md) - Condition-based looping
- [Dollar Variable](../operators/dollar_variable.md) - The `$` iteration variable
- [break/continue](break_continue.md) - Loop control flow
- [Arrays](../types/arrays.md) - Array indexing with `$`

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Lines 442-456  
**Unique Feature**: Bidirectional based on start vs limit, step always positive

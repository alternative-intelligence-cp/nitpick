# The `$` Variable (Iteration and Borrowing)

**Category**: Operators → Special Variables  
**Syntax**: `$` (in loops), `$(expr)` (borrowing - future)  
**Purpose**: Automatic iteration variable and safe references

---

## Overview

The `$` symbol serves multiple purposes in Aria:

1. **Iteration variable** in `till` and `loop` constructs
2. **Safe reference/borrowing** operator (future feature)

**Philosophy**: Minimal syntax for common patterns, automatic and safe.

---

## Iteration Variable

### Automatic in `till` and `loop`

The `$` variable is **automatically declared** in `till` and `loop` constructs:

```aria
// till loop: $ is iteration counter
till(10, 1) {
    print(`Iteration: &{$}`);
    // $ = 0, 1, 2, 3, ..., 10
}

// loop construct: $ is iteration counter
loop(1, 10, 1) {
    print(`Count: &{$}`);
    // $ = 1, 2, 3, 4, ..., 10
}
```

**Key**: No need to declare `$` - it's automatically available in the loop body.

---

## Iteration in `till`

### Positive Step: Count Up from 0

```aria
till(100, 1) {
    print(`&{$}`);
    // $ goes: 0, 1, 2, 3, ..., 100
}
```

### Negative Step: Count Down from Limit

```aria
till(100, -1) {
    print(`&{$}`);
    // $ goes: 100, 99, 98, ..., 1, 0
}
```

### Custom Step Size

```aria
// Count by 5s
till(100, 5) {
    print(`&{$}`);
    // $ goes: 0, 5, 10, 15, ..., 100
}

// Count down by 2s
till(50, -2) {
    print(`&{$}`);
    // $ goes: 50, 48, 46, ..., 2, 0
}
```

---

## Iteration in `loop`

### Count Up (start < limit)

```aria
loop(1, 10, 1) {
    print(`&{$}`);
    // $ goes: 1, 2, 3, ..., 10
}
```

### Count Down (start > limit)

```aria
loop(10, 1, 1) {
    print(`&{$}`);
    // $ goes: 10, 9, 8, ..., 1
}
```

### Custom Start and Step

```aria
// Start at 50, count to 100 by 10s
loop(50, 100, 10) {
    print(`&{$}`);
    // $ goes: 50, 60, 70, 80, 90, 100
}
```

---

## Properties of `$`

### Read-Only

You **cannot modify** the `$` variable:

```aria
till(10, 1) {
    // WRONG: Cannot assign to $
    $ = 5;  // ❌ Compile error
    
    // CORRECT: Use $ in expressions
    int64:doubled = $ * 2;  // ✅
}
```

### Scope Limited to Loop Body

`$` only exists **inside** the loop:

```aria
till(5, 1) {
    print(`&{$}`);  // ✅ Valid
}

print(`&{$}`);  // ❌ Error: $ undefined outside loop
```

### Shadow Behavior in Nested Loops

Inner loop's `$` **shadows** outer loop's `$`:

```aria
till(3, 1) {
    int64:outer = $;  // Save outer $ value
    
    till(3, 1) {
        // Inner $ shadows outer $
        print(`Outer: &{outer}, Inner: &{$}`);
    }
}
```

---

## Common Patterns with `$`

### Pattern 1: Array Indexing

```aria
int64[100]:array;

till(array.length - 1, 1) {
    array[$] = $ * 2;  // Fill with even numbers
}

// array = [0, 2, 4, 6, 8, ..., 198]
```

### Pattern 2: Range Sum

```aria
int64:sum = 0;

till(100, 1) {
    sum += $;
}

// sum = 0 + 1 + 2 + ... + 100 = 5050
```

### Pattern 3: Conditional Processing

```aria
till(20, 1) {
    if ($ % 2 == 0) {
        print(`&{$} is even`);
    }
}
```

### Pattern 4: Matrix Access

```aria
int64[10][10]:matrix;

loop(0, 9, 1) {
    int64:row = $;
    loop(0, 9, 1) {
        int64:col = $;
        matrix[row][col] = row * 10 + col;
    }
}
```

---

## Comparison: `$` vs Manual Counter

### Without `$` (Manual Counter)

```aria
int64:i = 0;
while (i <= 100) {
    print(`&{i}`);
    i++;
}
```

### With `$` (Automatic)

```aria
till(100, 1) {
    print(`&{$}`);
}
```

**Benefits**:
- Less boilerplate
- No manual increment
- Cannot forget to update counter
- Read-only prevents accidental modification

---

## Advanced Examples

### Example 1: Fibonacci with `$`

```aria
int64:a = 0, b = 1;

till(10, 1) {
    print(`Fib(&{$}) = &{a}`);
    int64:next = a + b;
    a = b;
    b = next;
}
```

### Example 2: Prime Number Sieve

```aria
bool[1000]:isPrime;

// Initialize all as prime
loop(2, 999, 1) {
    isPrime[$] = true;
}

// Sieve of Eratosthenes
loop(2, 999, 1) {
    if (isPrime[$]) {
        // Mark multiples as not prime
        int64:multiple = $ * 2;
        while (multiple < 1000) {
            isPrime[multiple] = false;
            multiple += $;
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

### Example 3: Pascal's Triangle

```aria
int64[15][15]:pascal;

loop(0, 14, 1) {
    int64:row = $;
    loop(0, row, 1) {
        int64:col = $;
        
        if (col == 0 || col == row) {
            pascal[row][col] = 1;
        } else {
            pascal[row][col] = pascal[row - 1][col - 1] + pascal[row - 1][col];
        }
    }
}
```

### Example 4: Nested Loop Product

```aria
// Multiplication table
loop(1, 12, 1) {
    int64:row = $;
    loop(1, 12, 1) {
        int64:col = $;
        print(`&{row} x &{col} = &{row * col}\t`);
    }
    print("\n");
}
```

---

## Safe Borrowing (Future Feature)

**Status**: Under design, not yet implemented.

### Tentative Syntax: `$(expr)`

```aria
// Borrow reference without ownership transfer
int64:value = 100;
processValue($(value));  // Borrow, don't move

// value still accessible here
print(`&{value}`);  // 100
```

### Potential Use Cases

```aria
// Borrow for read-only access
func:readValue = int64($(int64:x)) {
    pass(x);  // Can read, cannot modify ownership
};

// Borrow for temporary access
func:processArray = void($(int64[]:arr)) {
    till(arr.length - 1, 1) {
        print(`&{arr[$]}`);
    }
    // arr not consumed, caller still owns it
};
```

**Note**: This feature is still being designed and may change.

---

## Best Practices

### ✅ Use `$` for Simple Iteration

```aria
// GOOD: Clean and simple
till(100, 1) {
    process($);
}
```

### ✅ Save `$` When Needed in Nested Loops

```aria
// GOOD: Preserve outer iteration value
loop(1, 10, 1) {
    int64:outer = $;
    loop(1, 10, 1) {
        useValues(outer, $);
    }
}
```

### ✅ Use `$` for Array Indexing

```aria
// GOOD: Direct indexing
till(array.length - 1, 1) {
    processElement(array[$]);
}
```

### ❌ Don't Try to Modify `$`

```aria
// WRONG: $ is read-only
till(10, 1) {
    $ = 5;  // ❌ Compile error
}
```

### ❌ Don't Use `$` Outside Loops

```aria
till(5, 1) {
    int64:x = $;
}

print(`&{$}`);  // ❌ Error: $ out of scope
```

---

## Comparison with Other Languages

### Aria `$`

```aria
till(10, 1) {
    print(`&{$}`);
}
```

### Scala `_`

```scala
(0 to 10).foreach { i =>
    println(i)
}
```

### Rust `..=`

```rust
for i in 0..=10 {
    println!("{}", i);
}
```

### Ruby Block Variable

```ruby
(0..10).each do |i|
    puts i
end
```

---

## Edge Cases

### Single Iteration

```aria
// loop with start == limit
loop(5, 5, 1) {
    print(`&{$}`);  // Prints: 5 (executes once)
}
```

### Zero Step (Infinite Loop)

```aria
// Infinite loop: $ never advances
till(100, 0) {
    print(`&{$}`);  // ⚠️ Runs forever, $ = 0 always
}
```

### Negative Ranges

```aria
// Works with negative numbers
loop(-10, -1, 1) {
    print(`&{$}`);
    // $ goes: -10, -9, -8, ..., -1
}
```

---

## Performance

### Zero-Cost Abstraction

The `$` variable compiles to the same machine code as a manual counter:

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

**No runtime overhead for automatic iteration.**

---

## Related Topics

- [till Loop](../control_flow/till.md) - Count from 0 or limit
- [loop Construct](../control_flow/loop.md) - Bidirectional iteration
- [Arrays](../types/arrays.md) - Indexing with `$`
- [for Loop](../control_flow/for.md) - Traditional C-style loop
- [Borrowing](../memory_model/borrowing.md) - Safe reference semantics (future)

---

**Status**: Comprehensive (borrowing feature TBD)  
**Specification**: aria_specs.txt Lines 179 (operator list)  
**Unique Feature**: Automatic, read-only iteration variable in loops

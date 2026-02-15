# Range Operator (..)

**Category**: Operators → Range  
**Operator**: `..`  
**Purpose**: Create ranges for iteration

---

## Syntax

```aria
<start>..<end>          // Exclusive end
<start>..=<end>         // Inclusive end
..<end>                 // From 0
<start>..               // To infinity
```

---

## Description

The range operator `..` creates ranges for **array/string slicing** and **range expressions**. 

**IMPORTANT**: Aria does NOT use `for i in range` loops - those are from Rust/Python. Aria uses `till` for iteration.

---

## Slicing (Exclusive Range)

```aria
int32[]:arr = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];

// Extract elements 0-9 (excludes index 10)
int32[]:subset = arr[0..10];  // Array slicing
// Result: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
```

---

## Slicing (Inclusive Range)

```aria
int32[]:arr = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

// Extract elements 0-10 (includes index 10)
int32[]:subset = arr[0..=10];  // Array slicing
// Result: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
```

---

## Array Slicing

```aria
int32[]:arr = [0, 1, 2, 3, 4, 5];

// Slice [1, 2, 3]
int32[]:slice = arr[1..4];

// From start
int32[]:first_three = arr[..3];

// To end
int32[]:last_three = arr[3..];
```

---

## Best Practices

### ✅ DO: Use till for Iteration

```aria
// Aria loop syntax (NOT for-in)
till(arr.length - 1, 1) {
    process(arr[$]);  // $ = 0, 1, 2, ..., arr.length-1
}
```

### ✅ DO: Use for Slicing

```aria
string:substring = text[0..5];  // First 5 characters
```

### ❌ DON'T: Use Rust for-in Syntax

```aria
// ❌ WRONG: This is Rust/Python, not Aria!
// for i in 0..10 { ... }   // Doesn't exist in Aria
// for i in 0..=10 { ... }  // Doesn't exist in Aria

// ✅ CORRECT: Aria uses till for loops
till(9, 1) { ... }   // Exclusive ($ = 0 to 9)
till(10, 1) { ... }  // Inclusive ($ = 1 to 10)
```

---

## Related

- [Range Inclusive (..=)](range_inclusive.md)
- [Range Exclusive (..)](range_exclusive.md)
- [for loops](../control_flow/for.md)

---

**Remember**: `..` is **exclusive**, `..=` is **inclusive**!

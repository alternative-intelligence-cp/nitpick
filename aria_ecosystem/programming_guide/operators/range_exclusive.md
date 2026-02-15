# Range Exclusive (..)

**Category**: Operators → Range  
**Operator**: `..`  
**Purpose**: Exclusive range

---

## Syntax

```aria
<start>..<end>
```

---

## Description

The exclusive range operator `..` creates a range that **excludes** the end value.

---

## Basic Usage

**Note**: The `..` operator is for **slicing/ranges in expressions**, NOT for loops. Aria uses `till` for loops.

```aria
// ❌ WRONG: Aria doesn't use for-in loops
// for i in 0..10 { ... }  // This is Rust syntax, not Aria!

// ✅ CORRECT: Use till for loops
till(9, 1) {  // 0 to 9 ($ = 0, 1, 2, ..., 9)
    stdout << $;
}
// Prints: 0 1 2 3 4 5 6 7 8 9
```

---

## Range for Slicing

```aria
int32[]:arr = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

// Exclusive (..) - stops before index 10
int32[]:slice1 = arr[0..10];  // [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

// Inclusive (..=) - includes index 10  
int32[]:slice2 = arr[0..=10]; // [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
```

---

## Array Slicing

```aria
int32[]:arr = [0, 1, 2, 3, 4];

// Use till for iteration (NOT for-in)
till(arr.length - 1, 1) {
    process(arr[$]);  // Valid indices: $ = 0, 1, 2, 3, 4
}

// Use .. for slicing
int32[]:slice = arr[1..3];  // [1, 2] (excludes index 3)
```

---

## Best Practices

### ✅ DO: Use for Slicing

```aria
// Extract sub-array
int32[]:subset = array[0..10];  // First 10 elements (indices 0-9)
```

### ✅ DO: Use till for Iteration

```aria
// Aria loop syntax (NOT for-in)
till(9, 1) { ... }  // 10 iterations ($ = 0 to 9)
```

### ❌ DON'T: Use Rust for-in Syntax

```aria
// ❌ WRONG: This is Rust, not Aria!
// for i in 0..10 { ... }

// ✅ CORRECT: Aria uses till
till(9, 1) { ... }  // Excludes 10 (stops at 9)
```

---

## Related

- [Range Inclusive (..=)](range_inclusive.md)
- [Range](range.md)
- [for loops](../control_flow/for.md)

---

**Remember**: `..` **excludes** the end value!

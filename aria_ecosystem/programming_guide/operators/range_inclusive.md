# Range Inclusive (..=)

**Category**: Operators → Range  
**Operator**: `..=`  
**Purpose**: Inclusive range

---

## Syntax

```aria
<start>..=<end>
```

---

## Description

The inclusive range operator `..=` creates a range that **includes** both start and end values.

---

## Basic Usage

**Note**: The `..=` operator is for **slicing/ranges in expressions**, NOT for loops. Aria uses `till` for loops.

```aria
// ❌ WRONG: Aria doesn't use for-in loops
// for i in 1..=10 { ... }  // This is Rust syntax, not Aria!

// ✅ CORRECT: Use till for loops
till(10, 1) {  // 1 to 10 ($ = 1, 2, 3, ..., 10)
    stdout << $;
}
// Prints: 1 2 3 4 5 6 7 8 9 10
```

---

## Range for Slicing

```aria
int32[]:arr = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

// Exclusive (..) - stops before index 10
int32[]:slice1 = arr[1..10];  // [1, 2, 3, 4, 5, 6, 7, 8, 9]

// Inclusive (..=) - includes index 10
int32[]:slice2 = arr[1..=10]; // [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
```

---

## Array Slicing

```aria
int32[]:arr = [0, 1, 2, 3, 4, 5];

// Include index 4 (..= is inclusive)
int32[]:slice = arr[1..=4];  // [1, 2, 3, 4]

// Compare to exclusive (.. stops before end)
int32[]:slice2 = arr[1..4];  // [1, 2, 3]
```

---

## Best Practices

### ✅ DO: Use for Inclusive Iteration with till

```aria
// 0 to 100 inclusive (Aria loop syntax)
till(100, 1) {  // $ = 1, 2, 3, ..., 100
    update_progress($);
}
```

### ❌ DON'T: Use Rust for-in Syntax

```aria
// ❌ WRONG: This is Rust, not Aria!
// for i in 0..=10 { ... }  // 11 iterations (0-10)

// ✅ CORRECT: Aria uses till
till(10, 1) { ... }  // 10 iterations ($ = 1 to 10)
```

---

## Related

- [Range Exclusive (..)](range.md)
- [for loops](../control_flow/for.md)

---

**Remember**: `..=` **includes** the end value!

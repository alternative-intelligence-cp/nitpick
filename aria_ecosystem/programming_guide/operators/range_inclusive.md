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

```aria
// 1 to 10 (includes 10)
for i in 1..=10 {
    stdout << i;
}
// Prints: 1 2 3 4 5 6 7 8 9 10
```

---

## Difference from Exclusive

```aria
// Exclusive (..) - stops before 10
for i in 1..10 {
    stdout << i;  // 1 to 9
}

// Inclusive (..=) - includes 10
for i in 1..=10 {
    stdout << i;  // 1 to 10
}
```

---

## Array Slicing

```aria
arr: []i32 = [0, 1, 2, 3, 4, 5];

// Include index 4
slice: []i32 = arr[1..=4];  // [1, 2, 3, 4]

// Compare to exclusive
slice2: []i32 = arr[1..4];  // [1, 2, 3]
```

---

## Best Practices

### ✅ DO: Use for Inclusive Ranges

```aria
// 0 to 100 inclusive
for percent in 0..=100 {
    update_progress(percent);
}
```

### ❌ DON'T: Confuse with Exclusive

```aria
// Wrong mental model
for i in 0..=10 { ... }  // 11 iterations (0-10)

// Not
for i in 0..10 { ... }   // 10 iterations (0-9)
```

---

## Related

- [Range Exclusive (..)](range.md)
- [for loops](../control_flow/for.md)

---

**Remember**: `..=` **includes** the end value!

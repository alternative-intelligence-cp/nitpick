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

```aria
// 0 to 9 (excludes 10)
for i in 0..10 {
    stdout << i;
}
// Prints: 0 1 2 3 4 5 6 7 8 9
```

---

## Difference from Inclusive

```aria
// Exclusive (..) - stops before 10
for i in 0..10 {
    stdout << i;  // 0 to 9
}

// Inclusive (..=) - includes 10
for i in 0..=10 {
    stdout << i;  // 0 to 10
}
```

---

## Array Indices

```aria
arr: []i32 = [0, 1, 2, 3, 4];

// Works well with length
for i in 0..arr.length() {
    process(arr[i]);  // Valid indices
}

// Slice [1, 2]
slice: []i32 = arr[1..3];
```

---

## Best Practices

### ✅ DO: Use with Array Length

```aria
// Perfect for array iteration
for i in 0..array.length() {
    array[i] = i;
}
```

### ✅ DO: Use for Standard Ranges

```aria
// Most intuitive for 0-based
for i in 0..10 { ... }  // 10 iterations
```

### ❌ DON'T: Mix Up with Inclusive

```aria
// Be explicit about intent
for i in 0..10 { ... }   // Excludes 10
for i in 0..=9 { ... }   // Same thing
```

---

## Related

- [Range Inclusive (..=)](range_inclusive.md)
- [Range](range.md)
- [for loops](../control_flow/for.md)

---

**Remember**: `..` **excludes** the end value!

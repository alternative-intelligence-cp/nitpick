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

The range operator `..` creates a range of values for iteration.

---

## Exclusive Range

```aria
// 0 to 9 (excludes 10)
for i in 0..10 {
    stdout << i;
}
// Prints: 0 1 2 3 4 5 6 7 8 9
```

---

## Inclusive Range

```aria
// 0 to 10 (includes 10)
for i in 0..=10 {
    stdout << i;
}
// Prints: 0 1 2 3 4 5 6 7 8 9 10
```

---

## Array Slicing

```aria
arr: []i32 = [0, 1, 2, 3, 4, 5];

// Slice [1, 2, 3]
slice: []i32 = arr[1..4];

// From start
first_three: []i32 = arr[..3];

// To end
last_three: []i32 = arr[3..];
```

---

## Best Practices

### ✅ DO: Use for Iteration

```aria
for i in 0..arr.length() {
    process(arr[i]);
}
```

### ✅ DO: Use for Slicing

```aria
substring: string = text[0..5];
```

### ❌ DON'T: Confuse Inclusive/Exclusive

```aria
// Exclusive - stops before 10
for i in 0..10 { ... }  // 0-9

// Inclusive - includes 10
for i in 0..=10 { ... }  // 0-10
```

---

## Related

- [Range Inclusive (..=)](range_inclusive.md)
- [Range Exclusive (..)](range_exclusive.md)
- [for loops](../control_flow/for.md)

---

**Remember**: `..` is **exclusive**, `..=` is **inclusive**!

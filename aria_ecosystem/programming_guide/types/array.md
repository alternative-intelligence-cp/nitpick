# Arrays (`[]T`)

**Category**: Types → Collections  
**Syntax**: `[]<type>`  
**Purpose**: Fixed or dynamic sequences of values

---

## Overview

Arrays in Aria are **homogeneous sequences** of values. They can be fixed-size or dynamic.

---

## Declaration

### Fixed-Size Arrays

```aria
// Fixed size, stack-allocated
numbers: [5]i32 = [1, 2, 3, 4, 5];

// Type inference
colors := ["red", "green", "blue"];  // [3]string
```

### Dynamic Arrays (Slices)

```aria
// Dynamic size, heap-allocated
numbers: []i32 = aria_alloc([]i32, 10);

// From literal
values: []i32 = [1, 2, 3, 4, 5];
```

---

## Indexing

```aria
arr: []i32 = [10, 20, 30, 40, 50];

// Access elements (0-indexed)
first: i32 = arr[0];   // 10
last: i32 = arr[4];    // 50

// Modify
arr[2] = 99;  // [10, 20, 99, 40, 50]
```

---

## Slicing

```aria
arr: []i32 = [1, 2, 3, 4, 5];

// Slice [start..end) - excludes end
sub: []i32 = arr[1..4];  // [2, 3, 4]

// Inclusive slice
sub: []i32 = arr[1..=3];  // [2, 3, 4]

// From start
sub: []i32 = arr[..3];  // [1, 2, 3]

// To end
sub: []i32 = arr[2..];  // [3, 4, 5]
```

---

## Common Operations

### Length

```aria
arr: []i32 = [1, 2, 3, 4, 5];
len: i32 = arr.length();  // 5
```

### Iteration

```aria
// For-each
for value in arr {
    stdout << value;
}

// With index
for i: i32 in 0..arr.length() {
    stdout << arr[i];
}
```

### Append (Dynamic Arrays)

```aria
arr: []i32 = [];
arr.append(10);
arr.append(20);
arr.append(30);
// arr = [10, 20, 30]
```

---

## Multi-Dimensional Arrays

```aria
// 2D array
matrix: [][]i32 = [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]
];

// Access
value: i32 = matrix[1][2];  // 6

// 3D array
cube: [][][]i32 = aria_alloc([][][]i32, 10, 10, 10);
```

---

## Best Practices

### ✅ DO: Use Slices for Flexibility

```aria
// Dynamic arrays are flexible
data: []i32 = aria_alloc([]i32, 100);
```

### ✅ DO: Check Bounds

```aria
when index >= 0 and index < arr.length() then
    value: i32 = arr[index];
end
```

### ❌ DON'T: Access Out of Bounds

```aria
arr: []i32 = [1, 2, 3];
value: i32 = arr[5];  // ❌ Runtime error!
```

---

## Common Patterns

### Initialize with Values

```aria
// Fill with zeros
zeros: []i32 = aria_alloc([]i32, 100);
for i in 0..100 {
    zeros[i] = 0;
}

// Or use fill
zeros.fill(0);
```

### Filter

```aria
numbers: []i32 = [1, 2, 3, 4, 5, 6];
evens: []i32 = numbers.filter(fn(x: i32) -> bool {
    return x % 2 == 0;
});
// [2, 4, 6]
```

### Map

```aria
numbers: []i32 = [1, 2, 3, 4, 5];
doubled: []i32 = numbers.map(fn(x: i32) -> i32 {
    return x * 2;
});
// [2, 4, 6, 8, 10]
```

---

## Memory Management

```aria
// Stack-allocated (fixed size)
small: [10]i32;  // On stack

// Heap-allocated (dynamic)
large: []i32 = aria_alloc([]i32, 1000000);  // On heap

// GC will clean up heap arrays automatically
```

---

## Related

- [Array Operations](array_operations.md)
- [Array Declaration](array_declaration.md)
- [Slicing (..)](../operators/range.md)

---

**Remember**: `[]T` for **dynamic**, `[N]T` for **fixed size**!

# Array Declaration

**Category**: Types → Arrays  
**Purpose**: Declaring arrays

---

## Fixed-Size Arrays

```aria
// Stack-allocated
numbers: [5]i32 = [1, 2, 3, 4, 5];

// With type, no values (initialized to zero)
zeros: [10]i32;

// Inferred size
colors := ["red", "green", "blue"];  // [3]string
```

---

## Dynamic Arrays (Slices)

```aria
// Allocate on heap
numbers: []i32 = aria_alloc([]i32, 100);

// From literal
values: []i32 = [1, 2, 3, 4, 5];

// Empty
empty: []string = [];
```

---

## Multi-Dimensional

```aria
// 2D array
matrix: [][]i32 = [
    [1, 2, 3],
    [4, 5, 6]
];

// Allocate 2D
grid: [][]i32 = aria_alloc([][]i32, 10, 10);
```

---

## Type Syntax

```aria
// []T - dynamic array of T
dynamic: []i32;

// [N]T - fixed array of N elements of T
fixed: [5]i32;

// [][]T - 2D dynamic array
matrix: [][]f32;

// [N][M]T - 2D fixed array
fixed_matrix: [3][4]i32;
```

---

## Related

- [Arrays](array.md)
- [Array Operations](array_operations.md)

---

**Remember**: `[]T` = dynamic, `[N]T` = fixed size!

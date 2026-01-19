# The `matrix` Type

**Category**: Types → Mathematical  
**Syntax**: `matrix<T, M, N>` or `mat<M>x<N><T>`  
**Purpose**: Fixed-size 2D matrix for linear algebra

---

## Overview

`matrix` represents a **2D array** optimized for mathematical operations.

---

## Declaration

```aria
// 3x3 matrix of floats
mat: matrix<flt64, 3, 3> = [
    [1.0, 0.0, 0.0],
    [0.0, 1.0, 0.0],
    [0.0, 0.0, 1.0]
];

// Alternative syntax
mat: mat3x3<flt64> = identity_matrix();
```

---

## Common Sizes

```aria
// 2x2 matrix
mat2: mat2x2<flt32> = [
    [1.0, 2.0],
    [3.0, 4.0]
];

// 4x4 matrix (common in graphics)
mat4: mat4x4<flt32> = [
    [1, 0, 0, 0],
    [0, 1, 0, 0],
    [0, 0, 1, 0],
    [0, 0, 0, 1]
];
```

---

## Element Access

```aria
mat: mat3x3<flt64> = create_matrix();

// Access element
value: flt64 = mat[1][2];  // Row 1, Col 2

// Set element
mat[0][0] = 5.0;
```

---

## Matrix Operations

### Addition

```aria
a: mat2x2<flt64> = [[1, 2], [3, 4]];
b: mat2x2<flt64> = [[5, 6], [7, 8]];

c: mat2x2<flt64> = a + b;  // [[6, 8], [10, 12]]
```

### Multiplication

```aria
a: mat2x2<flt64> = [[1, 2], [3, 4]];
b: mat2x2<flt64> = [[5, 6], [7, 8]];

c: mat2x2<flt64> = a * b;  // Matrix multiplication
```

### Scalar Operations

```aria
mat: mat2x2<flt64> = [[1, 2], [3, 4]];

// Scale
scaled: mat2x2<flt64> = mat * 2.0;  // [[2, 4], [6, 8]]
```

---

## Methods

```aria
mat: mat3x3<flt64> = create_matrix();

// Transpose
transposed: mat3x3<flt64> = mat.transpose();

// Determinant
det: flt64 = mat.determinant();

// Inverse (if exists)
inv: ?mat3x3<flt64> = mat.inverse();
```

---

## Use Cases

### Graphics Transformations

```aria
// Rotation matrix
rotation: mat4x4<flt32> = create_rotation_matrix(angle);

// Apply to point
point: vec3 = {x = 1.0, y = 2.0, z = 3.0};
transformed: vec3 = rotation * point;
```

### Linear Algebra

```aria
// System of equations
a: mat3x3<flt64> = coefficients;
b: vec3 = constants;

// Solve Ax = b
x: vec3 = a.inverse().unwrap() * b;
```

---

## Best Practices

### ✅ DO: Use for Math Operations

```aria
transform: mat4x4<flt32> = projection * view * model;
```

### ❌ DON'T: Use for General 2D Arrays

```aria
// Bad - use nested arrays instead
grid: matrix<i32, 10, 10> = ...;  // ❌

// Good
grid: [][]i32 = create_2d_array(10, 10);  // ✅
```

---

## Related

- [tensor](tensor.md) - N-dimensional arrays
- [vec2](vec2.md), [vec3](vec3.md) - Vectors
- [Array](array.md)

---

**Remember**: `matrix` = **2D math** - use for linear algebra!

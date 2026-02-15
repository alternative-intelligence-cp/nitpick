# The `tensor` Type

**Category**: Types → Mathematical  
**Syntax**: `tensor<T, ...dims>`  
**Purpose**: N-dimensional array for ML and scientific computing

---

## Overview

`tensor` represents **multi-dimensional arrays** optimized for numerical computation and machine learning.

---

## Declaration

```aria
// 1D tensor (vector)
vec: tensor<flt32, 5> = [1.0, 2.0, 3.0, 4.0, 5.0];

// 2D tensor (matrix)
mat: tensor<flt32, 3, 3> = [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]
];

// 3D tensor (cube)
cube: tensor<flt32, 2, 3, 4> = create_tensor([2, 3, 4]);
```

---

## Dynamic Tensors

```aria
// Dynamic shape
shape: []i32 = [batch_size, height, width, channels];
data: tensor<flt32> = create_tensor(shape);
```

---

## Element Access

```aria
mat: tensor<flt32, 3, 3> = create_matrix();

// Access element
value: flt32 = mat[1, 2];

// Multi-dimensional
cube: tensor<flt32, 2, 3, 4> = create_tensor();
value: flt32 = cube[0, 1, 2];
```

---

## Operations

### Element-wise

```aria
a: tensor<flt32, 3, 3> = create_tensor();
b: tensor<flt32, 3, 3> = create_tensor();

// Addition
c: tensor<flt32, 3, 3> = a + b;

// Multiplication (element-wise)
d: tensor<flt32, 3, 3> = a * b;
```

### Broadcasting

```aria
mat: tensor<flt32, 3, 3> = create_tensor();
scalar: flt32 = 2.0;

// Broadcasts scalar to all elements
Result: tensor<flt32, 3, 3> = mat * scalar;
```

### Matrix Multiplication

```aria
a: tensor<flt32, 3, 4> = create_tensor();
b: tensor<flt32, 4, 5> = create_tensor();

// Matrix multiply: (3,4) @ (4,5) -> (3,5)
c: tensor<flt32, 3, 5> = a @ b;
```

---

## Methods

```aria
data: tensor<flt32, 3, 4> = create_tensor();

// Shape
shape: []i32 = data.shape();  // [3, 4]

// Reshape
reshaped: tensor<flt32, 4, 3> = data.reshape([4, 3]);

// Transpose
transposed: tensor<flt32, 4, 3> = data.transpose();

// Sum
total: flt32 = data.sum();

// Mean
average: flt32 = data.mean();
```

---

## Use Cases

### Neural Networks

```aria
// Input batch: [batch_size, features]
input: tensor<flt32, 32, 784> = load_batch();

// Weights: [features, hidden]
weights: tensor<flt32, 784, 128> = load_weights();

// Forward pass
hidden: tensor<flt32, 32, 128> = input @ weights;
```

### Image Processing

```aria
// Image: [height, width, channels]
image: tensor<flt32, 224, 224, 3> = load_image();

// Convolution kernel
kernel: tensor<flt32, 3, 3, 3, 32> = create_kernel();

// Apply convolution
output: tensor<flt32> = convolve(image, kernel);
```

### Scientific Computing

```aria
// 3D simulation grid
grid: tensor<flt64, 100, 100, 100> = initialize_grid();

// Update step
grid = simulate_step(grid);
```

---

## Best Practices

### ✅ DO: Use for ML and Scientific Computing

```aria
// Perfect for neural networks
activations: tensor<flt32, batch, features> = forward(input);
```

### ✅ DO: Check Shapes

```aria
when a.shape() != b.shape() then
    stderr << "Shape mismatch";
    fail("Cannot add tensors");
end
```

### ❌ DON'T: Use for Small Fixed Arrays

```aria
// Bad - overkill for 3 elements
point: tensor<flt32, 3> = [1, 2, 3];  // ❌

// Good - use vec3
point: vec3 = {x = 1, y = 2, z = 3};  // ✅
```

---

## Performance

Tensors are **optimized** for:
- SIMD operations
- GPU acceleration (when available)
- Vectorized math

**Note**: Operations may use specialized libraries (BLAS, cuBLAS).

---

## Related

- [matrix](matrix.md) - 2D matrices
- [vec2](vec2.md), [vec3](vec3.md) - Fixed vectors
- [Array](array.md) - General arrays

---

**Remember**: `tensor` = **N-dimensional numerical arrays** for ML!

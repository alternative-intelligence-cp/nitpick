# The `flt32` Type

**Category**: Types → Floating Point  
**Syntax**: `flt32` (alias: `f32`)  
**Purpose**: 32-bit IEEE 754 single-precision floating point

---

## Overview

`flt32` is a **32-bit floating-point** type following the IEEE 754 standard. Good balance of precision and performance.

**Alias**: `f32`

---

## Specifications

| Aspect | Value |
|--------|-------|
| **Precision** | ~7 decimal digits |
| **Range** | ±3.4 × 10^38 |
| **Storage** | 32 bits (4 bytes) |
| **Special Values** | NaN, +∞, -∞, +0, -0 |

---

## Declaration

```aria
// Basic
pi: flt32 = 3.14159;

// Alias (preferred)
temp: f32 = -40.5;

// Type suffix
value := 2.5f32;
```

---

## Operations

### Arithmetic

```aria
a: f32 = 10.5;
b: f32 = 3.2;

sum: f32 = a + b;      // 13.7
prod: f32 = a * b;     // 33.6
quot: f32 = a / b;     // 3.28125
```

### Precision Limits

```aria
// ~7 digits of precision
precise: f32 = 1234567.0;
imprecise: f32 = 12345678.0;  // Loses precision

// Rounding errors
x: f32 = 0.1 + 0.2;  // Not exactly 0.3!
```

---

## Use Cases

### Graphics

```aria
// 3D coordinates
vertex_x: f32 = 1.5;
vertex_y: f32 = 2.3;
vertex_z: f32 = -0.8;

// Colors (0.0 to 1.0)
red: f32 = 0.8;
green: f32 = 0.2;
blue: f32 = 0.5;
```

### Physics Simulations

```aria
// Position and velocity
position: f32 = 0.0;
velocity: f32 = 9.8;  // m/s

// Update
dt: f32 = 0.016;  // 60 FPS
position += velocity * dt;
```

### Audio

```aria
// Audio sample (-1.0 to 1.0)
sample: f32 = 0.5;

// Frequency
frequency: f32 = 440.0;  // A4 note
```

---

## Best Practices

### ✅ DO: Use for Graphics

```aria
// Perfect for 3D rendering
matrix: [][]f32 = create_identity_matrix();
```

### ✅ DO: Compare with Epsilon

```aria
// Floating point comparison
EPSILON: f32 = 0.0001;

fn almost_equal(a: f32, b: f32) -> bool {
    return abs(a - b) < EPSILON;
}
```

### ❌ DON'T: Use for Exact Values

```aria
// Wrong - rounding errors
money: f32 = 0.1 + 0.2;  // Not exactly 0.3!

// Right
money: i32 = 10 + 20;  // Use cents
```

### ❌ DON'T: Compare with ==

```aria
// Wrong - floating point error
x: f32 = 0.1 + 0.2;
when x == 0.3 then ...  // ❌ Might fail!

// Right
when abs(x - 0.3) < 0.0001 then ...  // ✅
```

---

## Special Values

### NaN (Not a Number)

```aria
nan: f32 = 0.0 / 0.0;
is_nan: bool = nan.is_nan();

// NaN != NaN
when nan == nan then  // Never true!
    ...
end
```

### Infinity

```aria
inf: f32 = 1.0 / 0.0;      // +∞
neg_inf: f32 = -1.0 / 0.0;  // -∞

is_infinite: bool = inf.is_infinite();
```

---

## Conversions

### From Integers

```aria
i: i32 = 42;
f: f32 = i as f32;  // 42.0
```

### To Integers

```aria
f: f32 = 42.7;
i: i32 = f as i32;  // 42 (truncates)

// Rounding
rounded: i32 = (f + 0.5) as i32;  // 43
```

---

## Common Patterns

### Vector Math

```aria
struct Vec3 {
    x: f32,
    y: f32,
    z: f32
}

fn dot(a: Vec3, b: Vec3) -> f32 {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

fn length(v: Vec3) -> f32 {
    return sqrt(dot(v, v));
}
```

### Linear Interpolation

```aria
fn lerp(a: f32, b: f32, t: f32) -> f32 {
    return a + (b - a) * t;
}

// Interpolate between values
start: f32 = 0.0;
end: f32 = 10.0;
mid: f32 = lerp(start, end, 0.5);  // 5.0
```

---

## Comparison with flt64

| Feature | `flt32` | `flt64` |
|---------|---------|---------|
| Precision | ~7 digits | ~15 digits |
| Range | ±3.4 × 10^38 | ±1.7 × 10^308 |
| Storage | 4 bytes | 8 bytes |
| Speed | Faster | Slower |
| Use Case | Graphics | Science |

---

## Related

- [flt64](flt64.md) - 64-bit float
- [int32](int32.md) - 32-bit integer
- [vec3](vec3.md) - 3D vector

---

**Remember**: `f32` is fast but has **limited precision** (~7 digits)!

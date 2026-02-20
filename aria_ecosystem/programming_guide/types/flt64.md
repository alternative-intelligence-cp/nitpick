# The `flt64` Type

**Category**: Types → Floating Point  
**Syntax**: `flt64` (alias: `f64`)  
**Purpose**: 64-bit IEEE 754 double-precision floating point

---

## Overview

`flt64` is a **64-bit floating-point** type with double precision. Default for floating-point literals.

**Alias**: `f64`

---

## Specifications

| Aspect | Value |
|--------|-------|
| **Precision** | ~15-17 decimal digits |
| **Range** | ±1.7 × 10^308 |
| **Storage** | 64 bits (8 bytes) |
| **Special Values** | NaN, +∞, -∞, +0, -0 |

---

## Declaration

```aria
// Basic
pi: flt64 = 3.141592653589793;

// Alias (preferred)
value: f64 = 2.718281828;

// Default inference
x := 3.14;  // Inferred as f64
```

---

## Operations

### High Precision

```aria
// ~15 digits of precision
precise: f64 = 123456789012345.0;

// Scientific notation
small: f64 = 1.23e-10;
large: f64 = 6.02e23;  // Avogadro's number
```

---

## Use Cases

### Scientific Computing

```aria
// Physical constants
speed_of_light: f64 = 299792458.0;  // m/s
planck: f64 = 6.62607015e-34;  // J⋅s

// Calculations
energy: f64 = mass * speed_of_light * speed_of_light;
```

### Financial Calculations

```aria
// High precision for money
amount: f64 = 123456.789;
interest: f64 = 0.0425;  // 4.25%

total: f64 = amount * (1.0 + interest);
```

### Statistics

```aria
// Mean calculation
fn mean(values: []f64) -> f64 {
    sum: f64 = 0.0;
    till(values.length - 1, 1) {
        sum += values[$];
    }
    return sum / (values.length() as f64);
}
```

---

## Best Practices

### ✅ DO: Use for Science

```aria
// Perfect for scientific work
distance: f64 = 384400000.0;  // Earth-Moon distance (m)
```

### ✅ DO: Use for Precision

```aria
// Need accuracy
measurement: f64 = 1.23456789012345;
```

### ❌ DON'T: Use for Money (Cents Better)

```aria
// Risky - rounding errors
price: f64 = 19.99;  // ❌

// Better - use integers for cents
price_cents: i64 = 1999;  // ✅
```

---

## Special Values

### NaN Operations

```aria
nan: f64 = 0.0 / 0.0;

// NaN propagates
Result: f64 = nan + 5.0;  // Still NaN
Result: f64 = nan * 10.0;  // Still NaN

// Check for NaN
when result.is_nan() then
    handle_error();
end
```

### Infinity

```aria
inf: f64 = 1.0 / 0.0;

// Infinity arithmetic
Result: f64 = inf + 100.0;  // Still +∞
Result: f64 = inf * 2.0;    // Still +∞
Result: f64 = inf / inf;    // NaN!
```

---

## Conversions

### From Integers

```aria
big: i64 = 9999999999;
float: f64 = big as f64;  // Precise enough
```

### To Integers

```aria
pi: f64 = 3.14159;
rounded: i32 = (pi + 0.5) as i32;  // 3
```

---

## Common Patterns

### Safe Division

```aria
fn safe_divide(a: f64, b: f64) -> Result<f64> {
    when b == 0.0 then
        fail("Division by zero");
    end
    
    Result: f64 = a / b;
    
    when result.is_nan() or result.is_infinite() then
        fail("Invalid result");
    end
    
    pass(result);
}
```

### Numerical Integration

```aria
// Trapezoidal rule
fn integrate(f: fn(f64) -> f64, a: f64, b: f64, n: i32) -> f64 {
    h: f64 = (b - a) / (n as f64);
    sum: f64 = (f(a) + f(b)) / 2.0;
    
    till(n - 1, 1) {
        i: i32 = $ + 1;
        x: f64 = a + (i as f64) * h;
        sum += f(x);
    }
    
    return sum * h;
}
```

---

## Comparison with flt32

| Feature | `flt32` | `flt64` |
|---------|---------|---------|
| Precision | ~7 digits | ~15 digits |
| Memory | 4 bytes | 8 bytes |
| Speed | Faster | Acceptable |
| Default | ❌ No | ✅ Yes |

---

## Related

- [flt32](flt32.md) - 32-bit float
- [flt128](flt128.md) - 128-bit float
- [int64](int64.md) - 64-bit integer

---

**Remember**: `f64` is the **default** for floating-point literals!

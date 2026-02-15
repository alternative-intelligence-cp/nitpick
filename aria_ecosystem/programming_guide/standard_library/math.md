# math

**Category**: Standard Library → Math  
**Purpose**: Mathematical functions and constants

---

## Overview

The `std.math` module provides mathematical operations, constants, and utilities.

---

## Importing

```aria
import std.math;
```

---

## Constants

```aria
PI: flt64 = math.PI;         // 3.14159265358979323846
E: flt64 = math.E;           // 2.71828182845904523536
TAU: flt64 = math.TAU;       // 6.28318530717958647693 (2π)
```

---

## Trigonometric Functions

```aria
// Input in radians
sin: flt64 = math.sin(math.PI / 2);     // 1.0
cos: flt64 = math.cos(math.PI);         // -1.0
tan: flt64 = math.tan(math.PI / 4);     // 1.0

// Inverse trig
asin: flt64 = math.asin(1.0);           // π/2
acos: flt64 = math.acos(-1.0);          // π
atan: flt64 = math.atan(1.0);           // π/4
atan2: flt64 = math.atan2(y, x);        // Angle from (0,0) to (x,y)
```

---

## Exponential & Logarithmic

```aria
// Exponential
exp: flt64 = math.exp(2.0);             // e^2
pow: flt64 = math.pow(2.0, 10.0);       // 2^10 = 1024

// Logarithms
log: flt64 = math.log(math.E);          // ln(e) = 1
log10: flt64 = math.log10(100.0);       // log₁₀(100) = 2
log2: flt64 = math.log2(8.0);           // log₂(8) = 3
```

---

## Rounding

```aria
// Ceiling
ceil: i32 = math.ceil(4.2);             // 5

// Floor
floor: i32 = math.floor(4.8);           // 4

// Round (see math_round.md for details)
round: i32 = math.round(4.5);           // 5

// Truncate (toward zero)
trunc: i32 = math.trunc(4.8);           // 4
trunc_neg: i32 = math.trunc(-4.8);      // -4
```

---

## Absolute Value & Sign

```aria
// Absolute value
abs: i32 = math.abs(-42);               // 42
abs_f: flt64 = math.abs(-3.14);         // 3.14

// Sign (-1, 0, or 1)
sign: i32 = math.sign(-5);              // -1
sign_pos: i32 = math.sign(10);          // 1
sign_zero: i32 = math.sign(0);          // 0
```

---

## Min/Max

```aria
// Minimum
min: i32 = math.min(5, 10);             // 5

// Maximum
max: i32 = math.max(5, 10);             // 10

// Clamp (limit to range)
clamp: i32 = math.clamp(15, 0, 10);     // 10
clamp2: i32 = math.clamp(-5, 0, 10);    // 0
```

---

## Square Root

```aria
sqrt: flt64 = math.sqrt(16.0);          // 4.0
cbrt: flt64 = math.cbrt(27.0);          // 3.0 (cube root)
```

---

## Hyperbolic Functions

```aria
sinh: flt64 = math.sinh(x);
cosh: flt64 = math.cosh(x);
tanh: flt64 = math.tanh(x);
```

---

## Examples

### Distance Between Points

```aria
fn distance(x1: flt64, y1: flt64, x2: flt64, y2: flt64) -> flt64 {
    dx: flt64 = x2 - x1;
    dy: flt64 = y2 - y1;
    return math.sqrt(dx*dx + dy*dy);
}

dist: flt64 = distance(0, 0, 3, 4);  // 5.0
```

### Angle Between Vectors

```aria
fn angle_between(x1: flt64, y1: flt64, x2: flt64, y2: flt64) -> flt64 {
    dot: flt64 = x1*x2 + y1*y2;
    mag1: flt64 = math.sqrt(x1*x1 + y1*y1);
    mag2: flt64 = math.sqrt(x2*x2 + y2*y2);
    
    return math.acos(dot / (mag1 * mag2));
}
```

### Convert Degrees to Radians

```aria
fn to_radians(degrees: flt64) -> flt64 {
    return degrees * math.PI / 180.0;
}

fn to_degrees(radians: flt64) -> flt64 {
    return radians * 180.0 / math.PI;
}

radians: flt64 = to_radians(90.0);   // π/2
degrees: flt64 = to_degrees(math.PI); // 180.0
```

---

## Best Practices

### ✅ DO: Use Constants

```aria
circumference: flt64 = 2.0 * math.PI * radius;  // ✅

// Not
circumference: flt64 = 2.0 * 3.14159 * radius;  // ❌
```

### ✅ DO: Check for Domain Errors

```aria
flt64:x = get_value();

when x < 0 then
    fail("Cannot take sqrt of negative");
end

flt64:Result = math.sqrt(x);  // ✅ Safe
```

---

## Related

- [math_round](math_round.md) - Rounding modes

---

**Remember**: Trig functions use **radians**, not degrees!

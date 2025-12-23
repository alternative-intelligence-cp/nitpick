# Aria Math Library

**Status**: ✅ COMPLETE  
**Version**: v0.1.0  
**Date**: December 22, 2025

## Overview

The Aria Math Library provides essential mathematical functions as built-in functions. All functions use LLVM intrinsics for optimal performance with zero runtime overhead.

## Design Principles

1. **Zero-cost abstractions**: All math functions compile to LLVM intrinsics
2. **Type safety**: Proper type checking for all arguments
3. **No exceptions**: Math errors return special values (NaN, Inf) per IEEE 754
4. **Generic where possible**: Functions work with multiple numeric types

## Basic Math Functions

### `abs(x: T) -> T`
Returns the absolute value of x.

**Supported types**: int32, int64, flt32, flt64  
**Returns**: Same type as input  

```aria
int64:result = abs(-42);     // 42
flt64:result = abs(-3.14);   // 3.14
```

### `min(a: T, b: T) -> T`
Returns the minimum of two values.

**Supported types**: int32, int64, flt32, flt64  
**Returns**: Same type as inputs  

```aria
int64:result = min(10, 20);        // 10
flt64:result = min(3.14, 2.71);   // 2.71
```

### `max(a: T, b: T) -> T`
Returns the maximum of two values.

**Supported types**: int32, int64, flt32, flt64  
**Returns**: Same type as inputs  

```aria
int64:result = max(10, 20);        // 20
flt64:result = max(3.14, 2.71);   // 3.14
```

### `floor(x: flt64) -> flt64`
Returns the largest integer value ≤ x.

```aria
flt64:result = floor(3.7);    // 3.0
flt64:result = floor(-3.7);   // -4.0
```

### `ceil(x: flt64) -> flt64`
Returns the smallest integer value ≥ x.

```aria
flt64:result = ceil(3.2);     // 4.0
flt64:result = ceil(-3.2);    // -3.0
```

### `round(x: flt64) -> flt64`
Returns x rounded to nearest integer (ties to even).

```aria
flt64:result = round(3.5);    // 4.0
flt64:result = round(4.5);    // 4.0 (ties to even)
flt64:result = round(-3.5);   // -4.0
```

### `trunc(x: flt64) -> flt64`
Returns x truncated toward zero.

```aria
flt64:result = trunc(3.7);    // 3.0
flt64:result = trunc(-3.7);   // -3.0
```

## Exponential and Logarithmic Functions

### `sqrt(x: flt64) -> flt64`
Returns the square root of x.

```aria
flt64:result = sqrt(16.0);    // 4.0
flt64:result = sqrt(2.0);     // 1.414...
flt64:result = sqrt(-1.0);    // NaN
```

### `pow(base: flt64, exp: flt64) -> flt64`
Returns base raised to the power of exp.

```aria
flt64:result = pow(2.0, 3.0);     // 8.0
flt64:result = pow(10.0, -2.0);   // 0.01
```

### `exp(x: flt64) -> flt64`
Returns e^x.

```aria
flt64:result = exp(1.0);      // 2.718... (e)
flt64:result = exp(0.0);      // 1.0
```

### `log(x: flt64) -> flt64`
Returns natural logarithm (base e) of x.

```aria
flt64:result = log(2.71828);  // ~1.0
flt64:result = log(1.0);      // 0.0
flt64:result = log(0.0);      // -Inf
flt64:result = log(-1.0);     // NaN
```

### `log10(x: flt64) -> flt64`
Returns base-10 logarithm of x.

```aria
flt64:result = log10(100.0);  // 2.0
flt64:result = log10(10.0);   // 1.0
```

### `log2(x: flt64) -> flt64`
Returns base-2 logarithm of x.

```aria
flt64:result = log2(8.0);     // 3.0
flt64:result = log2(1024.0);  // 10.0
```

## Trigonometric Functions

### `sin(x: flt64) -> flt64`
Returns sine of x (x in radians).

```aria
flt64:result = sin(0.0);      // 0.0
flt64:result = sin(PI / 2.0); // 1.0
```

### `cos(x: flt64) -> flt64`
Returns cosine of x (x in radians).

```aria
flt64:result = cos(0.0);      // 1.0
flt64:result = cos(PI);       // -1.0
```

### `tan(x: flt64) -> flt64`
Returns tangent of x (x in radians).

```aria
flt64:result = tan(0.0);      // 0.0
flt64:result = tan(PI / 4.0); // 1.0
```

### `asin(x: flt64) -> flt64`
Returns arcsine of x in radians (domain: [-1, 1]).

```aria
flt64:result = asin(0.0);     // 0.0
flt64:result = asin(1.0);     // PI/2
flt64:result = asin(2.0);     // NaN
```

### `acos(x: flt64) -> flt64`
Returns arccosine of x in radians (domain: [-1, 1]).

```aria
flt64:result = acos(1.0);     // 0.0
flt64:result = acos(0.0);     // PI/2
```

### `atan(x: flt64) -> flt64`
Returns arctangent of x in radians.

```aria
flt64:result = atan(0.0);     // 0.0
flt64:result = atan(1.0);     // PI/4
```

### `atan2(y: flt64, x: flt64) -> flt64`
Returns arctangent of y/x in radians, using signs to determine quadrant.

```aria
flt64:result = atan2(1.0, 1.0);    // PI/4
flt64:result = atan2(-1.0, -1.0);  // -3*PI/4
```

## Mathematical Constants

### `PI() -> flt64`
Returns π (pi) = 3.14159265358979323846...

```aria
flt64:pi = PI();
```

### `E() -> flt64`
Returns e (Euler's number) = 2.71828182845904523536...

```aria
flt64:e = E();
```

### `TAU() -> flt64`
Returns τ (tau) = 2π = 6.28318530717958647692...

```aria
flt64:tau = TAU();
```

## Implementation Details

### LLVM Intrinsics Used

- `abs`: `llvm.fabs.f64`, native int abs
- `sqrt`: `llvm.sqrt.f64`
- `pow`: `llvm.pow.f64`
- `exp`: `llvm.exp.f64`
- `log`: `llvm.log.f64`
- `log10`: `llvm.log10.f64`
- `log2`: `llvm.log2.f64`
- `sin`: `llvm.sin.f64`
- `cos`: `llvm.cos.f64`
- `floor`: `llvm.floor.f64`
- `ceil`: `llvm.ceil.f64`
- `round`: `llvm.round.f64`
- `trunc`: `llvm.trunc.f64`

For trigonometric functions not provided by LLVM (tan, asin, acos, atan, atan2), we link against libm.

### Performance

All functions compile to single LLVM instructions or direct libm calls with no overhead.

### Error Handling

Math functions follow IEEE 754 floating-point standard:
- **Domain errors** (e.g., sqrt(-1)): Return NaN
- **Range errors** (e.g., log(0)): Return ±Inf
- **No exceptions**: All errors return special floating-point values

## Future Enhancements

- Extended precision versions (flt128)
- Hyperbolic functions (sinh, cosh, tanh, asinh, acosh, atanh)
- Additional functions (cbrt, hypot, fma, copysign)
- SIMD vector versions for dvec/fvec/ivec types

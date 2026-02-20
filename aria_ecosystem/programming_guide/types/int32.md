# The `int32` Type

**Category**: Types → Integers  
**Syntax**: `int32` (alias: `i32`)  
**Purpose**: Standard 32-bit signed integer

---

## Overview

`int32` is a standard **32-bit signed integer** using two's complement representation. This is the **default integer type** in Aria.

**Alias**: `i32` (commonly used)

---

## Range

| Aspect | Value |
|--------|-------|
| **Minimum** | `-2,147,483,648` |
| **Maximum** | `+2,147,483,647` |
| **Total Values** | ~4.3 billion |
| **Storage** | 32 bits (4 bytes) |

---

## Declaration

```aria
// Explicit type
count: int32 = 1000000;

// Using alias (preferred)
value: i32 = 42;

// Default inference
x := 100;  // Inferred as i32
```

---

## Operations

### Arithmetic

```aria
a: i32 = 1000000;
b: i32 = 2000000;

sum: i32 = a + b;        // 3,000,000
prod: i32 = a * b;       // Overflow! (2 trillion)
quot: i32 = a / b;       // 0 (integer division)
```

### Overflow Behavior

```aria
x: i32 = 2147483647;
x += 1;  // Wraps to -2147483648

y: i32 = -2147483648;
y -= 1;  // Wraps to 2147483647
```

---

## Use Cases

### General Purpose

```aria
// Default for most integers
counter: i32 = 0;
index: i32 = 42;
temperature: i32 = -40;
```

### Array Indices

```aria
// Perfect for array indexing
arr: []string = ["a", "b", "c"];
till(arr.length() - 1, 1) {
    stdout << arr[$];
}
```

### Business Logic

```aria
// Financial calculations (in cents)
balance: i32 = 100000;  // $1,000.00
price: i32 = 2599;      // $25.99
```

---

## Best Practices

### ✅ DO: Use as Default Integer

```aria
// Default choice for integers
count: i32 = 0;
size: i32 = data.length();
```

### ✅ DO: Use Alias `i32`

```aria
// Preferred shorthand
value: i32 = 42;  // ✅ Better
value: int32 = 42;  // ✅ Also fine
```

### ✅ DO: Check for Overflow in Critical Code

```aria
fn safe_multiply(a: i32, b: i32) -> Result<i32> {
    Result: i64 = (a as i64) * (b as i64);
    when result < -2147483648 or result > 2147483647 then
        fail("Overflow");
    end
    pass(result as i32);
}
```

### ❌ DON'T: Assume No Overflow

```aria
// Dangerous for large values
factorial: i32 = 1;
till(19, 1) {
    i := $ + 1;
    factorial *= i;  // Overflows quickly!
}
```

---

## Comparison with TBB32

| Feature | `int32` | `tbb32` |
|---------|---------|---------|
| Range | -2^31 to +2^31-1 | -2^31+1 to +2^31-1 |
| ERR Sentinel | ❌ No | ✅ Yes (-2^31) |
| Overflow | Wraps | Sticky ERR |
| Standard | ✅ Yes | ❌ Aria-specific |

---

## Conversions

### Widening (Safe)

```aria
small: i32 = 1000000;
big: i64 = small as i64;  // Always safe
```

### Narrowing (Unsafe)

```aria
big: i64 = 3000000000;
small: i32 = big as i64;  // Overflow! Truncates
```

### From Floating Point

```aria
value: flt32 = 42.7;
integer: i32 = value as i32;  // 42 (truncates)
```

---

## Common Patterns

### Loop Counters

```aria
// Standard loop pattern
till(99, 1) {
    process($);
}
```

### Array Sizes

```aria
// Track sizes and indices
size: i32 = array.length();
till(size - 1, 1) {
    item := array[$];
}
```

### Calculations

```aria
// General arithmetic
total: i32 = 0;
till(values.length - 1, 1) {
    total += values[$];
}
average: i32 = total / values.length();
```

---

## Integer Literals

```aria
// Decimal
x: i32 = 1000;

// Hexadecimal
flags: i32 = 0xFF00;

// Binary
mask: i32 = 0b1010_1010;

// Underscores for readability
population: i32 = 7_800_000_000;  // Won't fit! Overflows
```

---

## Related

- [int16](int16.md) - 16-bit integer
- [int64](int64.md) - 64-bit integer
- [tbb32](tbb32.md) - 32-bit with ERR sentinel
- [uint32](uint32.md) - Unsigned 32-bit

---

**Remember**: `i32` is the **default integer type** in Aria!

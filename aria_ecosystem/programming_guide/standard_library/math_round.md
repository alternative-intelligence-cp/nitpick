# math.round()

**Category**: Standard Library → Math  
**Syntax**: `round(value: flt64) -> i32`  
**Purpose**: Round floating-point to nearest integer

---

## Overview

`round()` rounds a floating-point number to the nearest integer.

---

## Syntax

```aria
import std.math;

rounded: i32 = math.round(4.7);  // 5
```

---

## Rounding Modes

### Round Half Up (Default)

```aria
math.round(4.5);   // 5
math.round(4.4);   // 4
math.round(4.6);   // 5
math.round(-4.5);  // -4 (away from zero)
```

---

## Related Functions

### ceil() - Round Up

```aria
math.ceil(4.2);   // 5
math.ceil(4.9);   // 5
math.ceil(-4.2);  // -4
```

### floor() - Round Down

```aria
math.floor(4.2);   // 4
math.floor(4.9);   // 4
math.floor(-4.2);  // -5
```

### trunc() - Round Toward Zero

```aria
math.trunc(4.9);   // 4
math.trunc(-4.9);  // -4 (toward zero)
```

---

## Examples

### Round to Decimal Places

```aria
fn round_to_places(value: flt64, places: i32) -> flt64 {
    multiplier: flt64 = math.pow(10.0, places);
    return math.round(value * multiplier) / multiplier;
}

price: flt64 = round_to_places(12.3456, 2);  // 12.35
```

### Round Currency

```aria
fn round_currency(amount: flt64) -> flt64 {
    return math.round(amount * 100.0) / 100.0;
}

total: flt64 = round_currency(19.999);  // 20.00
```

---

## Comparison

```aria
value: flt64 = 4.7;

math.round(value);  // 5  - Nearest integer
math.ceil(value);   // 5  - Always up
math.floor(value);  // 4  - Always down
math.trunc(value);  // 4  - Toward zero
```

---

## Best Practices

### ✅ DO: Round for Display

```aria
Result: flt64 = 12.3456789;
display: flt64 = round_to_places(result, 2);
stdout << "$display";  // "12.35"
```

### ❌ DON'T: Use for Precise Calculations

```aria
// Not accurate for money
total: flt64 = math.round(price * quantity) / 100.0;  // ❌

// Better - use integer cents
total_cents: i64 = (price_cents * quantity);  // ✅
```

---

## Related

- [math](math.md) - Math functions overview

---

**Remember**: Floating-point rounding can be **imprecise** - use integers for money!

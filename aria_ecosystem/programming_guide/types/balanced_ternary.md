# Balanced Ternary

**Category**: Types → Number Systems  
**Purpose**: Base-3 number system using -1, 0, +1

---

## Overview

**Balanced ternary** is a base-3 number system where digits can be **-1**, **0**, or **+1** (represented as T, 0, 1).

---

## Digit Representation

| Symbol | Value |
|--------|-------|
| **T** | -1 |
| **0** | 0 |
| **1** | +1 |

---

## Examples

```aria
// Balanced ternary numbers
// T1 = -1×3¹ + 1×3⁰ = -3 + 1 = -2
// 1T = 1×3¹ + (-1)×3⁰ = 3 - 1 = 2
// 111 = 1×9 + 1×3 + 1×1 = 13

bt_minus_two: tryte = parse_bt("T1");  // -2
bt_two: tryte = parse_bt("1T");        // +2
bt_thirteen: tryte = parse_bt("111");  // 13
```

---

## Advantages

### No Separate Negative Sign

```aria
// Negative numbers naturally represented
positive_five: tryte = parse_bt("1TT");   // 5
negative_five: tryte = parse_bt("T11");   // -5

// Negation just flips digits
fn negate_bt(value: tryte) -> tryte {
    // T→1, 0→0, 1→T
    ...
}
```

### Symmetric Range

```aria
// Equal positive and negative values
// N trits can represent -(3^N-1)/2 to +(3^N-1)/2
```

### Efficient Rounding

```aria
// Truncation rounds toward zero automatically
```

---

## Arithmetic

### Addition

```aria
// Example: 1 + 1T = 1T1
// (3 + 2 = 5 in decimal)

a: tryte = parse_bt("1");
b: tryte = parse_bt("1T");
sum: tryte = a + b;  // 1T1
```

### Comparison

```aria
// First differing trit determines order
a: tryte = parse_bt("11T");
b: tryte = parse_bt("1T1");
when a > b then ... end
```

---

## Use Cases

### Ternary Logic

```aria
// Three-valued logic
TRUE: trit = 1;
UNKNOWN: trit = 0;
FALSE: trit = -1;

// Ternary AND
fn tand(a: trit, b: trit) -> trit {
    return when (a < b) then a else b end;
}
```

### Specialized Computing

```aria
// For ternary computer architectures
// (rare, mostly research)
```

---

## Conversion

### To Decimal

```aria
fn bt_to_decimal(bt: tryte) -> i32 {
    // Convert balanced ternary to decimal
    Result: i32 = 0;
    power: i32 = 1;
    
    till(bt.trits().length - 1, 1) {
        result += (bt.trits()[$] as i32) * power;
        power *= 3;
    }
    
    return result;
}
```

### From Decimal

```aria
fn decimal_to_bt(n: i32) -> tryte {
    // Convert decimal to balanced ternary
    // Algorithm handles negative numbers naturally
    ...
}
```

---

## Related

- [trit](trit.md) - Ternary digit
- [tryte](tryte.md) - 9 trits
- [balanced_nonary](balanced_nonary.md) - Base-9 balanced

---

**Remember**: Balanced ternary uses **T** (−1), **0**, and **1**!

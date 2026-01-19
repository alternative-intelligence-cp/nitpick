# The `trit` Type (Ternary Digit)

**Category**: Types → Specialized  
**Syntax**: `trit`  
**Purpose**: Base-3 ternary digit

---

## Overview

`trit` represents a **ternary digit** in balanced ternary system.

---

## Values

| Aspect | Value |
|--------|-------|
| **Possible Values** | `-1`, `0`, `+1` |
| **Representation** | T (−1), 0 (0), 1 (+1) |
| **Storage** | ~2 bits (packed) |

---

## Balanced Ternary

```aria
// Values: -1, 0, 1
neg: trit = -1;  // T
zero: trit = 0;   // 0
pos: trit = 1;    // 1
```

---

## Use Cases

### Ternary Computing

```aria
// Ternary logic
fn ternary_add(a: trit, b: trit) -> (trit, trit) {
    // Returns (sum, carry)
    ...
}
```

### Compact Storage

```aria
// Array of trits
trits: []trit = aria_alloc([]trit, 9);  // One tryte
```

---

## Related

- [tryte](tryte.md) - 9 trits (ternary byte)
- [balanced_ternary](balanced_ternary.md) - Ternary number system
- [int2](int2.md) - 2-bit integer

---

**Remember**: `trit` is a **ternary digit**: `-1`, `0`, or `1`!

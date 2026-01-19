# The `int4` Type (4-Bit Integer)

**Category**: Types → Specialized Integers  
**Syntax**: `int4`  
**Purpose**: 4-bit signed integer (nibble)

---

## Overview

`int4` is a **4-bit signed integer** representing a nibble.

---

## Range

| Aspect | Value |
|--------|-------|
| **Minimum** | `-8` |
| **Maximum** | `7` |
| **Total Values** | 16 |
| **Storage** | 4 bits (half byte) |

---

## Declaration

```aria
// Basic
value: int4 = 7;
neg: int4 = -8;
```

---

## Use Cases

### BCD (Binary Coded Decimal)

```aria
// Store decimal digits
digit1: int4 = 3;
digit2: int4 = 7;

// Packed BCD byte
bcd: i8 = (digit1 << 4) | digit2;  // 0x37
```

### Compact Storage

```aria
// Two values per byte
nibbles: []int4 = aria_alloc([]int4, 1000000);  // 500 KB
```

---

## Related

- [int2](int2.md) - 2-bit integer
- [int8](int8.md) - 8-bit integer
- [nit](nit.md) - Nibble (4-bit alternative)

---

**Remember**: `int4` is a **nibble** - half a byte!

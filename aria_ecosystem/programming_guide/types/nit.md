# The `nit` Type (Nibble)

**Category**: Types → Specialized  
**Syntax**: `nit`  
**Purpose**: 4-bit value (nibble)

---

## Overview

`nit` represents a **4-bit nibble** (half byte). Alternative to `int4`.

---

## Range

| Aspect | Value |
|--------|-------|
| **Values** | 0 to 15 (typically) |
| **Storage** | 4 bits |

---

## Use Cases

### Hexadecimal Digits

```aria
// Each hex digit is a nit
high: nit = 0xA;  // 10
low: nit = 0x5;   // 5

byte: i8 = (high << 4) | low;  // 0xA5
```

### Compact Encoding

```aria
// Pack values tightly
data: []nit = aria_alloc([]nit, 1000000);
```

---

## Related

- [int4](int4.md) - 4-bit signed
- [nyte](nyte.md) - Byte
- [trit](trit.md) - Ternary digit

---

**Remember**: `nit` = **nibble** = **4 bits** = **half byte**!

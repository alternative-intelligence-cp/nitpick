# The `int1` Type (Single Bit Integer)

**Category**: Types → Specialized Integers  
**Syntax**: `int1`  
**Purpose**: Single-bit signed integer

---

## Overview

`int1` is a **1-bit signed integer** with only two values: `0` and `-1`.

---

## Range

| Aspect | Value |
|--------|-------|
| **Values** | `-1`, `0` |
| **Storage** | 1 bit (packed) |

---

## Declaration

```aria
// Basic
flag: int1 = 0;
neg: int1 = -1;
```

---

## Use Cases

### Compact Storage

```aria
// Array of single bits (packed)
flags: []int1 = aria_alloc([]int1, 1000000);  // ~125 KB
```

### Sign Bits

```aria
// Extract sign bit
sign: int1 = (value >> 31) as int1;  // -1 if negative, 0 if positive
```

---

## Best Practices

### ✅ DO: Use for Extreme Compactness

```aria
// Million flags in small space
bits: []int1 = aria_alloc([]int1, 1000000);
```

### ❌ DON'T: Use for Normal Logic

```aria
// Wrong - use bool!
is_ready: int1 = 0;  // ❌
is_ready: bool = false;  // ✅
```

---

## Related

- [int2](int2.md) - 2-bit integer
- [bool](bool.md) - Boolean type

---

**Remember**: `int1` is **niche** - use `bool` for flags!

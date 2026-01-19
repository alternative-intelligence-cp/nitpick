# The `int2` Type (2-Bit Integer)

**Category**: Types → Specialized Integers  
**Syntax**: `int2`  
**Purpose**: 2-bit signed integer

---

## Overview

`int2` is a **2-bit signed integer** with four values.

---

## Range

| Aspect | Value |
|--------|-------|
| **Values** | `-2`, `-1`, `0`, `1` |
| **Storage** | 2 bits (packed) |

---

## Declaration

```aria
// Possible values
a: int2 = -2;
b: int2 = -1;
c: int2 = 0;
d: int2 = 1;
```

---

## Use Cases

### Ternary Logic

```aria
// -1, 0, 1 for less/equal/greater
compare_Result: int2 = compare(a, b);
```

### Compact Arrays

```aria
// Tightly packed
data: []int2 = aria_alloc([]int2, 1000000);
```

---

## Related

- [int1](int1.md) - 1-bit integer
- [int4](int4.md) - 4-bit integer
- [balanced_ternary](balanced_ternary.md) - Base-3 number system

---

**Remember**: `int2` is for **specialized** compact storage!

# The `tryte` Type (Ternary Byte)

**Category**: Types → Specialized  
**Syntax**: `tryte`  
**Purpose**: 9 trits (ternary byte)

---

## Overview

`tryte` represents **9 ternary digits** (trits), analogous to a byte in binary.

---

## Specifications

| Aspect | Value |
|--------|-------|
| **Size** | 9 trits |
| **Range** | -9,841 to +9,841 |
| **Values** | 19,683 distinct |

---

## Structure

```aria
// One tryte = 9 trits
// Each trit can be -1, 0, or 1

tryte_val: tryte = parse_tryte("1T011T0T1");
// T represents -1
```

---

## Use Cases

### Ternary Computing

```aria
// Balanced ternary arithmetic
a: tryte = 100;  // In balanced ternary
b: tryte = 50;
sum: tryte = a + b;
```

### Specialized Hardware

```aria
// For ternary computers (rare)
data: []tryte = aria_alloc([]tryte, 1024);
```

---

## Conversion

```aria
// To/from decimal
decimal: i32 = tryte_to_i32(my_tryte);
back: tryte = i32_to_tryte(decimal);
```

---

## Related

- [trit](trit.md) - Single ternary digit
- [balanced_ternary](balanced_ternary.md) - Ternary number system
- [nyte](nyte.md) - Binary byte

---

**Remember**: `tryte` = **9 trits** (ternary byte)!

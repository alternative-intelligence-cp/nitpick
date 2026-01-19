# Balanced Nonary

**Category**: Types → Number Systems  
**Purpose**: Base-9 balanced number system

---

## Overview

**Balanced nonary** is a base-9 number system using symmetric digits around zero.

---

## Digit Range

| Aspect | Value |
|--------|-------|
| **Digits** | -4, -3, -2, -1, 0, +1, +2, +3, +4 |
| **Base** | 9 |

---

## Use Cases

### Higher Base Balanced System

```aria
// More compact than balanced ternary
// but same symmetric properties
value: i32 = parse_balanced_nonary("-2 1 3");
// -2×81 + 1×9 + 3×1 = -162 + 9 + 3 = -150
```

---

## Related

- [balanced_ternary](balanced_ternary.md) - Base-3 balanced
- [balanced_numbers](balanced_numbers.md) - Balanced number systems overview

---

**Remember**: Balanced nonary is **base-9** with **symmetric digits**!

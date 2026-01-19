# TBB Overview

**Category**: Types → TBB System  
**Purpose**: Understanding Twisted Balanced Binary types

---

## Overview

**TBB (Twisted Balanced Binary)** types are Aria's answer to robust error handling at the type level.

---

## Key Concept

TBB types **reserve the minimum value as an ERR sentinel**, creating sticky error propagation.

---

## TBB Types

| Type | Valid Range | ERR Sentinel | Storage |
|------|-------------|--------------|---------|
| `tbb8` | -127 to +127 | -128 | 8 bits |
| `tbb16` | -32,767 to +32,767 | -32,768 | 16 bits |
| `tbb32` | -2^31+1 to +2^31-1 | -2^31 | 32 bits |
| `tbb64` | -2^63+1 to +2^63-1 | -2^63 | 64 bits |

---

## Why TBB?

### Automatic Error Propagation

```aria
// Errors spread automatically
x: tbb32 = ERR;
y: tbb32 = 100;

Result: tbb32 = x + y;  // ERR (sticky!)
```

### No Exception Overhead

```aria
// No try-catch needed
value: tbb32 = risky_operation();
when value == ERR then
    handle_error();
end
```

### Symmetric Range

```aria
// Equal positive and negative values
// -127 to +127 (tbb8)
// vs -128 to +127 (int8)
```

---

## When to Use TBB

### ✅ Use TBB When:
- Error propagation is important
- Overflow should be detected
- Symmetric range is beneficial

### ❌ Use Standard Ints When:
- Performance is critical
- No error handling needed
- Maximum range required

---

## Related

- [tbb8](tbb8.md), [tbb16](tbb16.md), [tbb32](tbb32.md), [tbb64](tbb64.md)
- [ERR Sentinel](tbb_err_sentinel.md)
- [Sticky Errors](tbb_sticky_errors.md)
- [ERR Constant](ERR.md)

---

**Remember**: TBB = **error safety** at the type level!

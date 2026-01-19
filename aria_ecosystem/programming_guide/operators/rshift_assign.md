# Right Shift Assignment Operator (>>=)

**Category**: Operators → Compound Assignment  
**Operator**: `>>=`  
**Purpose**: Right shift and assign

---

## Syntax

```aria
<variable> >>= <count>
```

---

## Description

The right shift assignment operator `>>=` shifts bits right and assigns the result back.

---

## Basic Usage

```aria
x: i32 = 20;
x >>= 2;  // x = x >> 2
stdout << x;  // 5 (20 / 4)
```

---

## Equivalent Forms

```aria
// These are equivalent
x >>= 3;
x = x >> 3;
```

---

## Common Uses

### Divide by Powers of 2

```aria
value: i32 = 100;
value >>= 1;  // 50 (divide by 2)
value >>= 2;  // 12 (divide by 4)
```

### Extract Bits

```aria
data: i32 = 0xABCD;
data >>= 8;  // 0x00AB (shift right 8 bits)
```

---

## Best Practices

### ✅ DO: Use for Fast Division

```aria
size >>= 3;  // Divide by 8
```

### ❌ DON'T: Shift Negative Numbers

```aria
// Sign extension issues
negative: i32 = -16;
negative >>= 2;  // -4, not 4!
```

---

## Related

- [Right Shift (>>)](right_shift.md)
- [Left Shift Assignment (<<=)](lshift_assign.md)
- [Div Assignment (/=)](div_assign.md)

---

**Remember**: `>>=` shifts right **in-place**!

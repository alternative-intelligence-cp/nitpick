# Left Shift Assignment Operator (<<=)

**Category**: Operators → Compound Assignment  
**Operator**: `<<=`  
**Purpose**: Left shift and assign

---

## Syntax

```aria
<variable> <<= <count>
```

---

## Description

The left shift assignment operator `<<=` shifts bits left and assigns the result back.

---

## Basic Usage

```aria
x: i32 = 5;
x <<= 2;  // x = x << 2
stdout << x;  // 20 (5 * 4)
```

---

## Equivalent Forms

```aria
// These are equivalent
x <<= 3;
x = x << 3;
```

---

## Common Uses

### Multiply by Powers of 2

```aria
value: i32 = 10;
value <<= 1;  // 20 (multiply by 2)
value <<= 2;  // 80 (multiply by 4)
```

### Build Bit Pattern

```aria
result: i32 = 1;
till(3, 1) {
    result <<= 1;  // Shift left
    result |= 1;   // Set low bit
}
// result is 0b11111
```

### Scale Values

```aria
pixels: i32 = 100;
pixels <<= 2;  // Multiply by 4 (400)
```

---

## Best Practices

### ✅ DO: Use for Fast Multiply

```aria
size <<= 3;  // Multiply by 8
```

### ✅ DO: Use for Bit Building

```aria
value: i32 = 0;
value = (value << 8) | byte1;
value = (value << 8) | byte2;
```

### ❌ DON'T: Shift Too Far

```aria
// Dangerous for i32
value <<= 32;  // Undefined!
```

---

## Related

- [Left Shift (<<)](left_shift.md)
- [Right Shift Assignment (>>=)](rshift_assign.md)
- [Multiply Assignment (*=)](mul_assign.md)

---

**Remember**: `<<=` shifts left **in-place**!

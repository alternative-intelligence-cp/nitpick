# Bitwise AND Assignment Operator (&=)

**Category**: Operators → Compound Assignment  
**Operator**: `&=`  
**Purpose**: Bitwise AND and assign

---

## Syntax

```aria
<variable> &= <expression>
```

---

## Description

The bitwise AND assignment operator `&=` performs a bitwise AND and assigns the result back.

---

## Basic Usage

```aria
x: i32 = 0b1111;
x &= 0b1010;  // x = x & 0b1010
stdout << x;  // 0b1010 = 10
```

---

## Equivalent Forms

```aria
// These are equivalent
x &= mask;
x = x & mask;
```

---

## Common Uses

### Clear Bits

```aria
// Clear lower 4 bits
value: i32 = 0xFF;
value &= 0xF0;  // 0xF0
```

### Apply Mask

```aria
// Keep only specific bits
flags: i32 = 0b11111111;
mask: i32 = 0b00001111;
flags &= mask;  // 0b00001111
```

### Clear Flag

```aria
// Clear specific bit
flags: i32 = 0b1111;
flags &= ~(1 << 2);  // Clear bit 2
// flags is now 0b1011
```

---

## Best Practices

### ✅ DO: Use for Masking

```aria
value &= 0xFF;  // Keep lower byte
```

### ✅ DO: Use for Clearing Bits

```aria
permissions &= ~WRITE_FLAG;  // Remove write
```

### ❌ DON'T: Confuse with Logical AND

```aria
// Wrong: Logical AND
flag &= (x > 5);  // Type error!

// Right: Bitwise AND
value &= mask;
```

---

## Related

- [Bitwise AND (&)](bitwise_and.md)
- [Bitwise OR Assignment (|=)](or_assign.md)
- [Bitwise XOR Assignment (^=)](xor_assign.md)

---

**Remember**: `&=` masks bits **in-place**!

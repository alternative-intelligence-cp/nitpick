# Bitwise XOR Assignment Operator (^=)

**Category**: Operators → Compound Assignment  
**Operator**: `^=`  
**Purpose**: Bitwise XOR and assign

---

## Syntax

```aria
<variable> ^= <expression>
```

---

## Description

The bitwise XOR assignment operator `^=` performs a bitwise XOR and assigns the result back.

---

## Basic Usage

```aria
x: i32 = 0b1100;
x ^= 0b0110;  // x = x ^ 0b0110
stdout << x;  // 0b1010 = 10
```

---

## Equivalent Forms

```aria
// These are equivalent
x ^= mask;
x = x ^ mask;
```

---

## Common Uses

### Toggle Bits

```aria
// Toggle bit at position
value: i32 = 0b1010;
value ^= (1 << 2);  // Toggle bit 2
// value is now 0b1110

value ^= (1 << 2);  // Toggle again
// value is now 0b1010 (back to original)
```

### Toggle Flag

```aria
// Toggle state
flags: i32 = 0b0001;
flags ^= ACTIVE_FLAG;  // Turn on
flags ^= ACTIVE_FLAG;  // Turn off
```

### Invert Mask

```aria
// Flip specific bits
value: i32 = 0xFF;
value ^= 0x0F;  // Flip lower 4 bits
// value is now 0xF0
```

---

## Best Practices

### ✅ DO: Use for Toggling

```aria
state ^= PAUSED_FLAG;  // Toggle pause
```

### ✅ DO: Use for Flipping Bits

```aria
pixel ^= 0xFF;  // Invert color
```

### ❌ DON'T: Use for Setting

```aria
// Wrong: XOR doesn't reliably set
flags ^= FLAG;  // Toggles, doesn't set!

// Right: Use OR to set
flags |= FLAG;
```

---

## Related

- [Bitwise XOR (^)](bitwise_xor.md)
- [Bitwise OR Assignment (|=)](or_assign.md)
- [Bitwise AND Assignment (&=)](and_assign.md)

---

**Remember**: `^=` toggles bits **in-place**!

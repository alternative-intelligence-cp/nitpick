# Bitwise NOT Operator (~)

**Category**: Operators → Bitwise  
**Operator**: `~`  
**Purpose**: Bitwise complement (flip all bits)

---

## Syntax

```aria
~<expression>
```

---

## Description

The bitwise NOT operator `~` flips all bits in an integer value (0 becomes 1, 1 becomes 0).

---

## Truth Table

```
a  | ~a
0  |  1
1  |  0
```

---

## Basic Usage

```aria
value: i32 = 0b0011;  // 3
Result: i32 = ~value;  // 0b...1111111111111100 (two's complement)

// For byte
byte_val: byte = 0b00001111;
inverted: byte = ~byte_val;  // 0b11110000
```

---

## Common Uses

### Invert Bits

```aria
// Flip all bits in a byte
data: byte = 0xAB;  // 10101011
inverted: byte = ~data;  // 01010100
```

### Create Masks

```aria
// Create inverse mask
keep_mask: i32 = 0b00001111;
clear_mask: i32 = ~keep_mask;  // 0b11110000

value: i32 = 0b10101010;
cleared: i32 = value & clear_mask;  // Upper bits kept
```

### Clear Specific Bits

```aria
// Clear bit at position
value: i32 = 0b11111111;
position: i32 = 3;

value &= ~(1 << position);  // Clear bit 3
// value is now 0b11110111
```

---

## Two's Complement

```aria
// In two's complement: ~x == -x - 1
x: i32 = 5;
Result: i32 = ~x;  // -6

stdout << result;  // -6
```

---

## Bit Manipulation Patterns

### Toggle All Bits

```aria
value: byte = 0b10101010;
toggled: byte = ~value;  // 0b01010101
```

### Create All-Ones Mask

```aria
// All bits set
all_ones: i32 = ~0;  // 0xFFFFFFFF
```

### Extract Opposite

```aria
// Keep opposite bits
data: i32 = 0b11110000;
opposite: i32 = data ^ (~0);  // Flip all bits
```

---

## Best Practices

### ✅ DO: Use for Bit Clearing

```aria
// Clear specific bits
value &= ~mask;
```

### ✅ DO: Create Inverse Masks

```aria
MASK_LOW: i32 = 0x000F;
MASK_HIGH: i32 = ~MASK_LOW;
```

### ❌ DON'T: Confuse with Logical NOT

```aria
// Bitwise NOT
Result: i32 = ~5;  // -6

// Logical NOT
Result: bool = !true;  // false
```

---

## With Signed Integers

```aria
// Careful with signed integers
x: i32 = 0;
Result: i32 = ~x;  // -1 (all bits set in two's complement)

x: i32 = -1;
Result: i32 = ~x;  // 0 (all bits cleared)
```

---

## Byte Operations

```aria
// Safer with unsigned bytes
b: byte = 0b10101010;
inverted: byte = ~b;  // 0b01010101

// No sign extension issues
```

---

## Related

- [Bitwise AND (&)](bitwise_and.md)
- [Bitwise OR (|)](bitwise_or.md)
- [Bitwise XOR (^)](bitwise_xor.md)
- [Logical NOT (!)](logical_not.md)

---

**Remember**: `~` **flips all bits** - use with **unsigned** for clarity!

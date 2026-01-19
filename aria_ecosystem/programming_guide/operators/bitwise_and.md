# Bitwise AND Operator (&)

**Category**: Operators → Bitwise  
**Operator**: `&`  
**Purpose**: Bitwise AND operation

---

## Syntax

```aria
<expression> & <expression>
```

---

## Description

The bitwise AND operator `&` performs a bit-by-bit AND operation on integer values.

---

## Truth Table

```
a  b  | a & b
0  0  |  0
0  1  |  0
1  0  |  0
1  1  |  1
```

---

## Basic Usage

```aria
a: i32 = 0b1100;  // 12
b: i32 = 0b1010;  // 10
Result: i32 = a & b;  // 0b1000 = 8

stdout << result;  // 8
```

---

## Common Uses

### Extract Bits

```aria
// Extract lower 4 bits
value: i32 = 0x1234;
lower: i32 = value & 0x000F;  // 0x0004
```

### Check Flags

```aria
// Check if bit is set
flags: i32 = 0b1010;
bit2: i32 = 0b0010;

when (flags & bit2) != 0 then
    stdout << "Bit 2 is set";
end
```

### Mask Bits

```aria
// Keep only specific bits
data: i32 = 0b11110101;
mask: i32 = 0b00001111;
masked: i32 = data & mask;  // 0b00000101
```

---

## Power of Two Check

```aria
fn is_power_of_two(n: i32) -> bool {
    return n > 0 and (n & (n - 1)) == 0;
}

stdout << is_power_of_two(8);   // true
stdout << is_power_of_two(10);  // false
```

---

## Even/Odd Check

```aria
fn is_even(n: i32) -> bool {
    return (n & 1) == 0;
}

stdout << is_even(4);  // true
stdout << is_even(7);  // false
```

---

## Best Practices

### ✅ DO: Use for Flags

```aria
READ: i32 = 0b001;
WRITE: i32 = 0b010;
EXEC: i32 = 0b100;

permissions: i32 = READ | WRITE;

when (permissions & READ) != 0 then
    stdout << "Can read";
end
```

### ✅ DO: Use Hex for Clarity

```aria
// Clear intent
masked: i32 = value & 0xFF;  // Lower byte

// Less clear
masked: i32 = value & 255;
```

### ❌ DON'T: Confuse with Logical AND

```aria
// Bitwise AND
Result: i32 = 5 & 3;  // 1

// Logical AND
Result: bool = true and false;  // false
```

---

## Compound Assignment

```aria
flags: i32 = 0b1111;
mask: i32 = 0b0110;

flags &= mask;  // flags = flags & mask
stdout << flags;  // 0b0110
```

---

## Related

- [Bitwise OR (|)](bitwise_or.md)
- [Bitwise XOR (^)](bitwise_xor.md)
- [Bitwise NOT (~)](bitwise_not.md)

---

**Remember**: Bitwise `&` operates on **bits**, not boolean logic!

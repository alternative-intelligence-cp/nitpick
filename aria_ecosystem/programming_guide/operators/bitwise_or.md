# Bitwise OR Operator (|)

**Category**: Operators → Bitwise  
**Operator**: `|`  
**Purpose**: Bitwise OR operation

---

## Syntax

```aria
<expression> | <expression>
```

---

## Description

The bitwise OR operator `|` performs a bit-by-bit OR operation on integer values.

---

## Truth Table

```
a  b  | a | b
0  0  |  0
0  1  |  1
1  0  |  1
1  1  |  1
```

---

## Basic Usage

```aria
a: i32 = 0b1100;  // 12
b: i32 = 0b1010;  // 10
Result: i32 = a | b;  // 0b1110 = 14

stdout << result;  // 14
```

---

## Common Uses

### Set Bits

```aria
// Set bit 2
value: i32 = 0b0000;
value = value | 0b0100;  // 0b0100
```

### Combine Flags

```aria
READ: i32 = 0b001;
WRITE: i32 = 0b010;
EXEC: i32 = 0b100;

// Combine permissions
permissions: i32 = READ | WRITE | EXEC;  // 0b111
```

### Merge Values

```aria
// Combine high and low bytes
high: i32 = 0xFF00;
low: i32 = 0x00AB;
combined: i32 = high | low;  // 0xFFAB
```

---

## Flag Pattern

```aria
// Define flags
FLAG_A: i32 = 1 << 0;  // 0b0001
FLAG_B: i32 = 1 << 1;  // 0b0010
FLAG_C: i32 = 1 << 2;  // 0b0100

// Set multiple flags
state: i32 = FLAG_A | FLAG_C;  // 0b0101

// Check flag
when (state & FLAG_A) != 0 then
    stdout << "Flag A is set";
end
```

---

## Best Practices

### ✅ DO: Use for Combining Flags

```aria
options: i32 = OPTION_FAST | OPTION_SAFE | OPTION_VERBOSE;
```

### ✅ DO: Use for Setting Bits

```aria
// Set bit at position
value |= (1 << position);
```

### ❌ DON'T: Confuse with Logical OR

```aria
// Bitwise OR
Result: i32 = 5 | 3;  // 7

// Logical OR
Result: bool = true or false;  // true
```

---

## Compound Assignment

```aria
flags: i32 = 0b0011;
new_flags: i32 = 0b0100;

flags |= new_flags;  // flags = flags | new_flags
stdout << flags;  // 0b0111
```

---

## Related

- [Bitwise AND (&)](bitwise_and.md)
- [Bitwise XOR (^)](bitwise_xor.md)
- [Left Shift (<<)](left_shift.md)

---

**Remember**: `|` **combines** bits - useful for **flags**!

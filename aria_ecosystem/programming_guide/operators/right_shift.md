# Right Shift Operator (>>)

**Category**: Operators → Bitwise  
**Operator**: `>>`  
**Purpose**: Shift bits right or input from stream

---

## Syntax

```aria
<expression> >> <count>  // Bit shift
<stream> >> <variable>   // Stream input
```

---

## Description

The right shift operator `>>` has two uses:
1. **Bit shift**: Shift bits right
2. **Stream input**: Read from input stream

---

## Bit Shifting

```aria
// Shift right by 2
value: i32 = 0b1100;  // 12
shifted: i32 = value >> 2;  // 0b0011 = 3

// Each shift divides by 2
x: i32 = 20;
y: i32 = x >> 1;  // 10 (20 / 2)
z: i32 = x >> 2;  // 5 (20 / 4)
```

---

## Stream Input

```aria
// Read from stdin
value: i32;
stdin >> value;

// Read multiple
name: string;
age: i32;
stdin >> name >> age;
```

---

## Arithmetic vs Logical Shift

```aria
// Arithmetic shift (signed): Preserves sign
negative: i32 = -8;  // 0b...11111000
Result: i32 = negative >> 1;  // -4 (sign extended)

// Logical shift (unsigned): Fills with 0
unsigned: u32 = 0b11111000;
Result: u32 = unsigned >> 1;  // 0b01111100
```

---

## Divide by Powers of 2

```aria
// Efficient division
half: i32 = n >> 1;      // n / 2
quarter: i32 = n >> 2;   // n / 4
eighth: i32 = n >> 3;    // n / 8
```

---

## Extract Bits

```aria
// Get upper bits
value: i32 = 0xABCD;
upper: i32 = value >> 8;  // 0x00AB

// Get specific byte
byte2: i32 = (value >> 16) & 0xFF;
```

---

## Best Practices

### ✅ DO: Use for Fast Division

```aria
// Divide by power of 2
Result: i32 = value >> 3;  // value / 8
```

### ✅ DO: Use for Bit Extraction

```aria
// Extract bit at position
bit: i32 = (value >> position) & 1;
```

### ✅ DO: Read Input

```aria
count: i32;
stdin >> count;
```

### ❌ DON'T: Shift Negative for Unsigned Division

```aria
// Wrong: Sign extension issues
negative: i32 = -16;
Result: i32 = negative >> 2;  // -4, not 4!

// Use unsigned if needed
unsigned: u32 = -16 as u32;
Result: u32 = unsigned >> 2;  // Large positive
```

### ❌ DON'T: Shift Beyond Width

```aria
// Undefined!
Result: i32 = value >> 32;  // ❌ Dangerous
```

---

## Common Patterns

### Extract Bytes

```aria
// Extract individual bytes
value: i32 = 0x12345678;

byte0: i32 = value & 0xFF;        // 0x78
byte1: i32 = (value >> 8) & 0xFF;  // 0x56
byte2: i32 = (value >> 16) & 0xFF; // 0x34
byte3: i32 = (value >> 24) & 0xFF; // 0x12
```

### Binary Search

```aria
// Divide search space
mid: i32 = (low + high) >> 1;  // Fast divide by 2
```

### Check Parity

```aria
// Count set bits
count: i32 = 0;
temp: i32 = value;
while temp != 0 {
    count += temp & 1;
    temp >>= 1;
}
```

---

## Compound Assignment

```aria
value: i32 = 80;
value >>= 2;  // value = value >> 2
stdout << value;  // 20
```

---

## Related

- [Left Shift (<<)](left_shift.md)
- [Bitwise AND (&)](bitwise_and.md)
- [Stream Input](../io/stdin.md)

---

**Remember**: `>>` **shifts bits right** or **reads from streams**!

# Left Shift Operator (<<)

**Category**: Operators → Bitwise  
**Operator**: `<<`  
**Purpose**: Shift bits left or output to stream

---

## Syntax

```aria
<expression> << <count>  // Bit shift
<stream> << <value>      // Stream output
```

---

## Description

The left shift operator `<<` has two uses:
1. **Bit shift**: Shift bits left
2. **Stream output**: Write to output stream

---

## Bit Shifting

```aria
// Shift left by 2
value: i32 = 0b0001;  // 1
shifted: i32 = value << 2;  // 0b0100 = 4

// Each shift multiplies by 2
x: i32 = 5;
y: i32 = x << 1;  // 10 (5 * 2)
z: i32 = x << 2;  // 20 (5 * 4)
```

---

## Stream Output

```aria
// Output to stdout
stdout << "Hello";
stdout << 42;
stdout << 3.14;

// Chained output
stdout << "Value: " << value << "\n";
```

---

## Multiply by Powers of 2

```aria
// Efficient multiplication
times_2: i32 = n << 1;   // n * 2
times_4: i32 = n << 2;   // n * 4
times_8: i32 = n << 3;   // n * 8
times_16: i32 = n << 4;  // n * 16
```

---

## Create Bit Masks

```aria
// Set bit at position
bit_5: i32 = 1 << 5;  // 0b00100000 = 32
bit_10: i32 = 1 << 10;  // 0b10000000000 = 1024

// Combine flags
flags: i32 = (1 << 0) | (1 << 2) | (1 << 4);
// 0b00010101
```

---

## Overflow Behavior

```aria
// Bits shifted out are lost
value: i32 = 0b11110000;
Result: i32 = value << 1;  // 0b11100000 (top bit lost)

// Be careful with large shifts
x: i32 = 1;
overflow: i32 = x << 31;  // Sign bit affected!
```

---

## Best Practices

### ✅ DO: Use for Powers of 2

```aria
size: i32 = 1 << 10;  // 1024 bytes (1KB)
```

### ✅ DO: Use for Bit Flags

```aria
READ: i32 = 1 << 0;
WRITE: i32 = 1 << 1;
EXEC: i32 = 1 << 2;
```

### ✅ DO: Chain Stream Output

```aria
stdout << "Name: " << name << ", Age: " << age << "\n";
```

### ❌ DON'T: Shift by Negative

```aria
// Undefined!
Result: i32 = value << -1;  // ❌ Error
```

### ❌ DON'T: Shift Beyond Width

```aria
// Undefined for i32!
Result: i32 = value << 32;  // ❌ Dangerous
```

---

## Common Patterns

### Powers of Two

```aria
// Check if power of 2
fn is_power_of_two(n: i32) -> bool {
    return n > 0 and (n & (n - 1)) == 0;
}

// Generate powers
powers: []i32 = [];
till(9, 1) {
    powers.push(1 << $);  // 1, 2, 4, 8, 16, ...
}
```

### Bit Manipulation

```aria
// Set bit
value |= (1 << position);

// Clear bit
value &= ~(1 << position);

// Toggle bit
value ^= (1 << position);
```

---

## Compound Assignment

```aria
value: i32 = 5;
value <<= 2;  // value = value << 2
stdout << value;  // 20
```

---

## Related

- [Right Shift (>>)](right_shift.md)
- [Bitwise OR (|)](bitwise_or.md)
- [Stream Output](../io/stdout.md)

---

**Remember**: `<<` **shifts bits left** or **writes to streams**!

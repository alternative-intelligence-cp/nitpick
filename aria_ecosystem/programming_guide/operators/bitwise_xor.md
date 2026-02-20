# Bitwise XOR Operator (^)

**Category**: Operators → Bitwise  
**Operator**: `^`  
**Purpose**: Bitwise exclusive OR operation

---

## Syntax

```aria
<expression> ^ <expression>
```

---

## Description

The bitwise XOR (exclusive OR) operator `^` performs a bit-by-bit XOR operation on integer values.

---

## Truth Table

```
a  b  | a ^ b
0  0  |  0
0  1  |  1
1  0  |  1
1  1  |  0
```

---

## Basic Usage

```aria
a: i32 = 0b1100;  // 12
b: i32 = 0b1010;  // 10
Result: i32 = a ^ b;  // 0b0110 = 6

stdout << result;  // 6
```

---

## Common Uses

### Toggle Bits

```aria
// Toggle bit 2
value: i32 = 0b1010;
value = value ^ 0b0100;  // 0b1110

value = value ^ 0b0100;  // 0b1010 (toggled back)
```

### Swap Without Temporary

```aria
a: i32 = 5;
b: i32 = 10;

a = a ^ b;
b = a ^ b;  // b = 5
a = a ^ b;  // a = 10

// a and b are swapped!
```

### Detect Differences

```aria
// Find different bits
old: i32 = 0b1100;
new: i32 = 0b1010;
diff: i32 = old ^ new;  // 0b0110 (bits that changed)
```

---

## Encryption

```aria
// Simple XOR cipher
fn xor_encrypt(data: []byte, key: byte) -> []byte {
    Result: []byte = aria_alloc_buffer(data.length());
    
    till(data.length() - 1, 1) {
        result[$] = data[$] ^ key;
    }
    
    return result;
}

// XOR again to decrypt
encrypted := xor_encrypt(message, 0x5A);
decrypted := xor_encrypt(encrypted, 0x5A);  // Original message
```

---

## Find Unique Element

```aria
// All elements appear twice except one
fn find_unique(arr: []i32) -> i32 {
    Result: i32 = 0;
    
    till(arr.length - 1, 1) {
        result ^= arr[$];  // Pairs cancel out
    }
    
    return result;  // Only unique remains
}

nums: []i32 = [1, 2, 3, 2, 1];
unique: i32 = find_unique(nums);  // 3
```

---

## Best Practices

### ✅ DO: Use for Toggling

```aria
// Toggle flag
state ^= FLAG_ACTIVE;
```

### ✅ DO: Use for Parity

```aria
// Calculate parity bit
parity: i32 = data[0] ^ data[1] ^ data[2] ^ data[3];
```

### ❌ DON'T: Use for Swapping in Production

```aria
// XOR swap is clever but hard to read
a ^= b;
b ^= a;
a ^= b;

// Prefer clear swap
temp: i32 = a;
a = b;
b = temp;
```

---

## Properties

### Self-Cancelling

```aria
x ^ x == 0  // Any value XOR itself is 0
```

### Identity

```aria
x ^ 0 == x  // XOR with 0 is identity
```

### Commutative

```aria
a ^ b == b ^ a
```

### Associative

```aria
(a ^ b) ^ c == a ^ (b ^ c)
```

---

## Compound Assignment

```aria
flags: i32 = 0b1100;
toggle: i32 = 0b0110;

flags ^= toggle;  // flags = flags ^ toggle
stdout << flags;  // 0b1010
```

---

## Related

- [Bitwise AND (&)](bitwise_and.md)
- [Bitwise OR (|)](bitwise_or.md)
- [Bitwise NOT (~)](bitwise_not.md)

---

**Remember**: XOR **toggles** bits and is **self-cancelling**!

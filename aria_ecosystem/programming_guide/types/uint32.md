# The `uint32` Type

**Category**: Types → Unsigned Integers  
**Syntax**: `uint32` (alias: `u32`)  
**Purpose**: 32-bit unsigned integer

---

## Overview

`uint32` is a **32-bit unsigned integer** with values from 0 to ~4.3 billion.

**Alias**: `u32`

---

## Range

| Aspect | Value |
|--------|-------|
| **Minimum** | `0` |
| **Maximum** | `4,294,967,295` |
| **Total Values** | ~4.3 billion |
| **Storage** | 32 bits (4 bytes) |

---

## Declaration

```aria
// Basic
count: uint32 = 1000000;

// Alias (preferred)
value: u32 = 4294967295;

// Inference
size := 1024u32;
```

---

## Use Cases

### Array Sizes

```aria
// Large array indices
size: u32 = array.length();

for i: u32 in 0..size {
    process(array[i]);
}
```

### Hashes

```aria
// 32-bit hash values
hash: u32 = calculate_hash(data);
```

### IPv4 Addresses

```aria
// IPv4 as u32
ip: u32 = 0xC0A80001;  // 192.168.0.1

fn ip_to_string(ip: u32) -> string {
    a: u8 = ((ip >> 24) & 0xFF) as u8;
    b: u8 = ((ip >> 16) & 0xFF) as u8;
    c: u8 = ((ip >> 8) & 0xFF) as u8;
    d: u8 = (ip & 0xFF) as u8;
    
    return "${a}.${b}.${c}.${d}";
}
```

---

## Best Practices

### ✅ DO: Use for Sizes and Counts

```aria
byte_count: u32 = file.size();
pixel_count: u32 = width * height;
```

### ❌ DON'T: Use for Negative Values

```aria
// Wrong - can't hold negative
balance: u32 = -100;  // ❌ Wraps!

// Right
balance: i32 = -100;  // ✅
```

---

## Related

- [uint16](uint16.md) - 16-bit unsigned
- [uint64](uint64.md) - 64-bit unsigned
- [int32](int32.md) - Signed 32-bit

---

**Remember**: `u32` is for **large positive values**!

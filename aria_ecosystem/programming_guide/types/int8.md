# The `int8` Type

**Category**: Types → Integers  
**Syntax**: `int8`  
**Purpose**: Standard 8-bit signed integer

---

## Overview

`int8` is a standard **8-bit signed integer** using two's complement representation. It's the smallest standard integer type in Aria.

---

## Range

| Aspect | Value |
|--------|-------|
| **Minimum** | `-128` |
| **Maximum** | `+127` |
| **Total Values** | 256 |
| **Storage** | 8 bits (1 byte) |

---

## Declaration

```aria
// Basic declaration
age: int8 = 25;

// Negative values
temp: int8 = -40;

// Type inference
count := 100 as int8;
```

---

## Operations

### Arithmetic

```aria
a: int8 = 10;
b: int8 = 20;

sum: int8 = a + b;       // 30
diff: int8 = a - b;      // -10
prod: int8 = a * b;      // Overflow! (200 > 127)
```

### Overflow Behavior

```aria
x: int8 = 127;
x += 1;  // Wraps to -128

y: int8 = -128;
y -= 1;  // Wraps to 127
```

---

## Use Cases

### Byte Data

```aria
// Reading byte streams
fn read_byte(stream: &FileStream) -> int8 {
    return stream.read_int8();
}
```

### Small Counters

```aria
// Loop counters with small range
for i: int8 in 0..100 {
    process(i);
}
```

### Memory-Constrained Arrays

```aria
// Save memory with small values
ages: []int8 = aria_alloc([]int8, 1000);
```

---

## Best Practices

### ✅ DO: Use for Small Values

```aria
// Perfect for small ranges
offset: int8 = -50;
percentage: int8 = 85;
```

### ✅ DO: Check Range

```aria
fn set_level(value: i32) -> int8 {
    when value < -128 or value > 127 then
        return ERR;
    end
    return value as int8;
}
```

### ❌ DON'T: Assume No Overflow

```aria
// Dangerous - can overflow!
counter: int8 = 0;
loop {
    counter += 1;  // Wraps at 127!
}
```

### ❌ DON'T: Use When Range Unknown

```aria
// Wrong - might need larger type
user_count: int8 = get_users().length();  // ❌
user_count: i32 = get_users().length();   // ✅
```

---

## Comparison with TBB8

| Feature | `int8` | `tbb8` |
|---------|--------|--------|
| Range | -128 to +127 | -127 to +127 |
| ERR Sentinel | ❌ No | ✅ Yes (-128) |
| Overflow | Wraps | Sticky ERR |
| Standard | ✅ Yes | ❌ Aria-specific |

---

## Conversions

### Widening (Safe)

```aria
small: int8 = 100;
big: int32 = small as int32;  // Always safe
```

### Narrowing (Unsafe)

```aria
big: int32 = 200;
small: int8 = big as int8;  // Truncates! (-56)
```

### Checked Conversion

```aria
fn to_int8(value: i32) -> Result<int8> {
    when value < -128 or value > 127 then
        return Err("Value out of range");
    end
    return Ok(value as int8);
}
```

---

## Common Patterns

### Byte Processing

```aria
// Process byte data
fn process_bytes(data: []int8) {
    for byte in data {
        when byte < 0 then
            handle_negative(byte);
        end
    }
}
```

### Compact Storage

```aria
// Pixel intensity (0-127)
struct Pixel {
    red: int8,
    green: int8,
    blue: int8
}
```

---

## Related

- [int16](int16.md) - 16-bit integer
- [int32](int32.md) - 32-bit integer
- [tbb8](tbb8.md) - 8-bit with ERR sentinel
- [uint8](uint8.md) - Unsigned 8-bit

---

**Remember**: `int8` uses **two's complement** and can **overflow**!

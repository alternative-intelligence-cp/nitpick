# The `uint8` Type

**Category**: Types → Unsigned Integers  
**Syntax**: `uint8` (alias: `u8`)  
**Purpose**: 8-bit unsigned integer

---

## Overview

`uint8` is an **8-bit unsigned integer** with only non-negative values. Perfect for bytes, small counts, and pixel data.

**Alias**: `u8`

---

## Range

| Aspect | Value |
|--------|-------|
| **Minimum** | `0` |
| **Maximum** | `255` |
| **Total Values** | 256 |
| **Storage** | 8 bits (1 byte) |

---

## Declaration

```aria
// Basic declaration
age: uint8 = 25;

// Using alias (preferred)
byte: u8 = 0xFF;

// Type inference
count := 100u8;
```

---

## Operations

### Arithmetic

```aria
a: u8 = 100;
b: u8 = 50;

sum: u8 = a + b;       // 150
diff: u8 = a - b;      // 50
prod: u8 = a * b;      // Overflow! (5000 > 255)
```

### Underflow Behavior

```aria
x: u8 = 10;
x -= 20;  // Wraps to 246 (10 - 20 = -10 → 256 - 10 = 246)
```

---

## Use Cases

### Byte Data

```aria
// Perfect for raw bytes
buffer: []u8 = aria_alloc([]u8, 1024);

fn read_bytes(file: &File, count: i32) -> []u8 {
    return file.read(count);
}
```

### Pixel Colors

```aria
// RGB color components
struct Color {
    r: u8,  // 0-255
    g: u8,
    b: u8,
    a: u8   // Alpha channel
}
```

### Protocol Flags

```aria
// Network packet flags
flags: u8 = 0b1010_0001;

SYN: u8 = 0x02;
ACK: u8 = 0x10;
FIN: u8 = 0x01;
```

---

## Best Practices

### ✅ DO: Use for Bytes

```aria
// Perfect for byte arrays
data: []u8 = [0x48, 0x65, 0x6C, 0x6C, 0x6F];
```

### ✅ DO: Use for Small Positive Values

```aria
// Perfect for 0-255 range
percentage: u8 = 85;
level: u8 = 100;
```

### ❌ DON'T: Subtract Without Checking

```aria
// Dangerous - underflows!
count: u8 = 5;
count -= 10;  // Wraps to 251!

// Better
when count >= 10 then
    count -= 10;
else
    count = 0;
end
```

---

## Comparison with int8

| Feature | `uint8` | `int8` |
|---------|---------|--------|
| Range | 0 to 255 | -128 to +127 |
| Negative Values | ❌ No | ✅ Yes |
| Use Case | Bytes, colors | Small signed values |

---

## Conversions

### To Larger Types (Safe)

```aria
small: u8 = 200;
big: u16 = small as u16;  // Always safe
```

### From Signed (Check Required)

```aria
signed: i32 = -10;
unsigned: u8 = signed as u8;  // Wraps! (246)

// Better
fn to_u8(value: i32) -> Result<u8> {
    when value < 0 or value > 255 then
        return Err("Out of range");
    end
    return Ok(value as u8);
}
```

---

## Common Patterns

### Byte Buffers

```aria
// Network packet
packet: []u8 = aria_alloc([]u8, 1500);  // MTU size

// Read from socket
bytes_read: i32 = socket.read(packet);
```

### Image Data

```aria
// RGB image
width: i32 = 800;
height: i32 = 600;
pixels: []u8 = aria_alloc([]u8, width * height * 3);

// Set pixel
fn set_pixel(x: i32, y: i32, r: u8, g: u8, b: u8) {
    offset: i32 = (y * width + x) * 3;
    pixels[offset] = r;
    pixels[offset + 1] = g;
    pixels[offset + 2] = b;
}
```

### Bit Manipulation

```aria
// Set flag
flags: u8 = 0;
flags |= 0x01;  // Set bit 0

// Check flag
has_flag: bool = (flags & 0x01) != 0;

// Clear flag
flags &= ~0x01;
```

---

## Related

- [int8](int8.md) - Signed 8-bit
- [uint16](uint16.md) - Unsigned 16-bit
- [tbb8](tbb8.md) - 8-bit with ERR

---

**Remember**: `u8` is **unsigned** - cannot hold negative values!

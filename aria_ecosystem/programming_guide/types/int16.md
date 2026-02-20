# The `int16` Type

**Category**: Types → Integers  
**Syntax**: `int16`  
**Purpose**: Standard 16-bit signed integer

---

## Overview

`int16` is a standard **16-bit signed integer** using two's complement representation.

---

## Range

| Aspect | Value |
|--------|-------|
| **Minimum** | `-32,768` |
| **Maximum** | `+32,767` |
| **Total Values** | 65,536 |
| **Storage** | 16 bits (2 bytes) |

---

## Declaration

```aria
// Basic declaration
count: int16 = 10000;

// Negative values
elevation: int16 = -1200;

// Type inference
value := 5000 as int16;
```

---

## Operations

### Arithmetic

```aria
a: int16 = 1000;
b: int16 = 2000;

sum: int16 = a + b;      // 3000
prod: int16 = a * b;     // Overflow! (2,000,000 > 32,767)
```

### Overflow Behavior

```aria
x: int16 = 32767;
x += 1;  // Wraps to -32768

y: int16 = -32768;
y -= 1;  // Wraps to 32767
```

---

## Use Cases

### Moderate Range Values

```aria
// Coordinates in 2D space
x: int16 = -15000;
y: int16 = 8000;
```

### Audio Samples

```aria
// 16-bit audio sample
fn read_audio_sample(stream: &AudioStream) -> int16 {
    return stream.read_int16();
}
```

### Sensor Data

```aria
// Temperature in hundredths of degree
temp: int16 = 2550;  // 25.50°C
```

---

## Best Practices

### ✅ DO: Use for Medium Ranges

```aria
// Perfect for coordinates
screen_x: int16 = 1920;
screen_y: int16 = 1080;
```

### ✅ DO: Consider Overflow

```aria
fn safe_add(a: int16, b: int16) -> Result<int16> {
    Result: i32 = (a as i32) + (b as i32);
    when result < -32768 or result > 32767 then
        fail("Overflow");
    end
    pass(result as int16);
}
```

### ❌ DON'T: Use for Large Calculations

```aria
// Wrong - will overflow quickly
area: int16 = width * height;  // ❌
area: i32 = width * height;    // ✅
```

---

## Comparison with TBB16

| Feature | `int16` | `tbb16` |
|---------|---------|---------|
| Range | -32,768 to +32,767 | -32,767 to +32,767 |
| ERR Sentinel | ❌ No | ✅ Yes (-32,768) |
| Overflow | Wraps | Sticky ERR |

---

## Conversions

### From Smaller Types

```aria
small: int8 = 100;
medium: int16 = small as int16;  // Always safe
```

### To Larger Types

```aria
medium: int16 = 10000;
large: int32 = medium as int32;  // Always safe
```

### From Larger Types (Unsafe)

```aria
large: int32 = 50000;
medium: int16 = large as int16;  // Truncates!
```

---

## Common Patterns

### Audio Processing

```aria
// Process 16-bit audio
fn process_audio(samples: []int16) {
    till(samples.length - 1, 1) {
        // Normalize to -1.0 to 1.0
        normalized: flt32 = (samples[$] as flt32) / 32768.0;
        process_sample(normalized);
    }
}
```

### 2D Coordinates

```aria
struct Point2D {
    x: int16,
    y: int16
}

fn distance(p1: Point2D, p2: Point2D) -> flt32 {
    dx: int16 = p1.x - p2.x;
    dy: int16 = p1.y - p2.y;
    return sqrt((dx * dx + dy * dy) as flt32);
}
```

---

## Related

- [int8](int8.md) - 8-bit integer
- [int32](int32.md) - 32-bit integer
- [tbb16](tbb16.md) - 16-bit with ERR sentinel
- [uint16](uint16.md) - Unsigned 16-bit

---

**Remember**: `int16` provides **medium range** at 2 bytes!

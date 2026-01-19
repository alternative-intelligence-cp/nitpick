# The `uint16` Type

**Category**: Types → Unsigned Integers  
**Syntax**: `uint16` (alias: `u16`)  
**Purpose**: 16-bit unsigned integer

---

## Overview

`uint16` is a **16-bit unsigned integer** with values from 0 to 65,535.

**Alias**: `u16`

---

## Range

| Aspect | Value |
|--------|-------|
| **Minimum** | `0` |
| **Maximum** | `65,535` |
| **Total Values** | 65,536 |
| **Storage** | 16 bits (2 bytes) |

---

## Declaration

```aria
// Basic
port: uint16 = 8080;

// Alias (preferred)
value: u16 = 65535;

// Inference
count := 1000u16;
```

---

## Use Cases

### Network Ports

```aria
// Perfect for port numbers
server_port: u16 = 8080;
client_port: u16 = 54321;

when port > 0 and port < 65536 then
    bind_socket(port);
end
```

### Unicode Characters

```aria
// UTF-16 code units
char_code: u16 = 0x4E2D;  // 中

fn to_utf16(text: string) -> []u16 {
    return text.encode_utf16();
}
```

### Sensor Readings

```aria
// 16-bit ADC values (0-65535)
reading: u16 = adc.read();
voltage: flt32 = (reading as flt32) / 65535.0 * 3.3;
```

---

## Best Practices

### ✅ DO: Use for Ports and IDs

```aria
port: u16 = 3000;
process_id: u16 = 1234;
```

### ❌ DON'T: Subtract Without Checking

```aria
// Underflows!
count: u16 = 10;
count -= 20;  // Wraps to 65526!
```

---

## Related

- [uint8](uint8.md) - 8-bit unsigned
- [uint32](uint32.md) - 32-bit unsigned
- [int16](int16.md) - Signed 16-bit

---

**Remember**: `u16` range is `0` to `65,535`!

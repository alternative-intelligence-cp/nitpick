# The `tbb16` Type (Twisted Balanced Binary 16-bit)

**Category**: Types → TBB (Twisted Balanced Binary)  
**Syntax**: `tbb16`  
**Purpose**: Symmetric 16-bit integer with ERR sentinel

---

## Overview

`tbb16` is a **Twisted Balanced Binary 16-bit** type with symmetric range and built-in error handling.

---

## Range and ERR Sentinel

| Aspect | Value |
|--------|-------|
| **Valid Range** | `-32,767` to `+32,767` |
| **ERR Sentinel** | `-32,768` (`0x8000`) |
| **Total Values** | 65,536 (65,535 normal + 1 ERR) |
| **Storage** | 16 bits (2 bytes) |

---

## Declaration

```aria
// Basic
value: tbb16 = 100;

// ERR state
error: tbb16 = ERR;

// Check for error
when value == ERR then
    handle_error();
end
```

---

## Sticky Error Propagation

```aria
// Errors propagate automatically
x: tbb16 = ERR;
y: tbb16 = 10;

Result: tbb16 = x + y;  // ERR (sticky!)
Result: tbb16 = x * y;  // ERR
Result: tbb16 = x / y;  // ERR
```

---

## Use Cases

### Audio with Error Handling

```aria
// 16-bit audio samples
fn read_audio() -> tbb16 {
    sample: tbb16 = hardware.read();
    when hardware.has_error() then
        return ERR;
    end
    return sample;
}
```

### Sensor Readings

```aria
// Temperature sensor
fn read_temp() -> tbb16 {
    when not sensor.is_ready() then
        return ERR;
    end
    return sensor.read_tbb16();
}
```

---

## Best Practices

### ✅ DO: Check for ERR

```aria
value: tbb16 = calculate();
when value != ERR then
    process(value);
end
```

### ✅ DO: Use for Error-Prone Operations

```aria
fn safe_divide(a: tbb16, b: tbb16) -> tbb16 {
    when b == 0 then
        return ERR;
    end
    return a / b;
}
```

### ❌ DON'T: Ignore ERR

```aria
// Dangerous - ERR propagates silently!
result := calculate() + 10;  // ❌ Check first!
```

---

## Comparison with int16

| Feature | `tbb16` | `int16` |
|---------|---------|---------|
| Range | -32,767 to +32,767 | -32,768 to +32,767 |
| ERR Sentinel | ✅ Yes (-32,768) | ❌ No |
| Overflow | Becomes ERR | Wraps |
| Error Handling | Built-in | Manual |

---

## Related

- [tbb8](tbb8.md) - 8-bit TBB
- [tbb32](tbb32.md) - 32-bit TBB
- [int16](int16.md) - Standard 16-bit
- [ERR](ERR.md) - ERR constant

---

**Remember**: `tbb16` has **built-in error handling** via ERR!

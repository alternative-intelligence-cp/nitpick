# TBB ERR Sentinel

**Category**: Types → TBB System  
**Purpose**: Understanding the ERR sentinel value

---

## The ERR Sentinel

The **ERR sentinel** is the **minimum value** of each TBB type, reserved to indicate an error state.

---

## Sentinel Values

| Type | ERR Value | Hex |
|------|-----------|-----|
| `tbb8` | -128 | 0x80 |
| `tbb16` | -32,768 | 0x8000 |
| `tbb32` | -2,147,483,648 | 0x80000000 |
| `tbb64` | -9,223,372,036,854,775,808 | 0x8000000000000000 |

---

## Why the Minimum?

### Hardware Efficiency

```aria
// Minimum value has special bit pattern
// tbb8:  10000000 (0x80)
// tbb16: 1000000000000000 (0x8000)

// Easy to detect in hardware
is_err: bool = (value == ERR);  // Single comparison
```

### Symmetric Range

```aria
// Creates balanced positive/negative range
// tbb8: -127 to +127 (255 valid values + 1 ERR)
```

---

## Checking for ERR

```aria
value: tbb32 = calculate();

// Simple check
when value == ERR then
    handle_error();
end

// Guard pattern
when value != ERR then
    use_value(value);
end
```

---

## ERR Constant

```aria
// Global ERR constant
error_value: tbb32 = ERR;

// Use in functions
fn divide(a: tbb32, b: tbb32) -> tbb32 {
    when b == 0 then
        return ERR;
    end
    return a / b;
}
```

---

## Related

- [TBB Overview](tbb_overview.md)
- [Sticky Errors](tbb_sticky_errors.md)
- [ERR Constant](ERR.md)

---

**Remember**: ERR is the **minimum value** - reserved for errors!

# The `tbb64` Type (Twisted Balanced Binary 64-bit)

**Category**: Types → TBB (Twisted Balanced Binary)  
**Syntax**: `tbb64`  
**Purpose**: Symmetric 64-bit integer with ERR sentinel

---

## Overview

`tbb64` is a **Twisted Balanced Binary 64-bit** type with symmetric range and built-in error handling.

---

## Range and ERR Sentinel

| Aspect | Value |
|--------|-------|
| **Valid Range** | `-9,223,372,036,854,775,807` to `+9,223,372,036,854,775,807` |
| **ERR Sentinel** | `-9,223,372,036,854,775,808` (`0x8000000000000000`) |
| **Total Values** | ~18.4 quintillion |
| **Storage** | 64 bits (8 bytes) |

---

## Declaration

```aria
// Basic
timestamp: tbb64 = 1640000000000;

// ERR state
error: tbb64 = ERR;

// Check
when timestamp == ERR then
    handle_error();
end
```

---

## Sticky Error Propagation

```aria
// Auto-propagation
x: tbb64 = ERR;
y: tbb64 = 1000000;

Result: tbb64 = x + y;  // ERR propagates
```

---

## Use Cases

### Large Safe Calculations

```aria
// Financial calculations
balance: tbb64 = get_balance();
when balance == ERR then
    return "Error retrieving balance";
end

new_balance: tbb64 = balance + transaction_amount;
when new_balance == ERR then
    return "Transaction would overflow";
end
```

### Timestamps with Error Handling

```aria
fn get_timestamp() -> tbb64 {
    when not time_available() then
        return ERR;
    end
    return current_time_ms();
}
```

---

## Best Practices

### ✅ DO: Use for Large Safe Values

```aria
total: tbb64 = 0;
for value in large_dataset {
    total += value;  // ERR if overflow
}

when total == ERR then
    log("Calculation overflowed");
end
```

---

## Comparison with i64

| Feature | `tbb64` | `i64` |
|---------|---------|--------|
| Range | -2^63+1 to +2^63-1 | -2^63 to +2^63-1 |
| ERR Sentinel | ✅ Yes | ❌ No |
| Overflow | Becomes ERR | Wraps |

---

## Related

- [tbb32](tbb32.md) - 32-bit TBB
- [int64](int64.md) - Standard 64-bit
- [ERR](ERR.md) - ERR constant

---

**Remember**: `tbb64` combines **large range** with **error safety**!

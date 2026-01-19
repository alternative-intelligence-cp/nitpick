# The `tbb32` Type (Twisted Balanced Binary 32-bit)

**Category**: Types → TBB (Twisted Balanced Binary)  
**Syntax**: `tbb32`  
**Purpose**: Symmetric 32-bit integer with ERR sentinel

---

## Overview

`tbb32` is a **Twisted Balanced Binary 32-bit** type with symmetric range and built-in error handling.

---

## Range and ERR Sentinel

| Aspect | Value |
|--------|-------|
| **Valid Range** | `-2,147,483,647` to `+2,147,483,647` |
| **ERR Sentinel** | `-2,147,483,648` (`0x80000000`) |
| **Total Values** | ~4.3 billion |
| **Storage** | 32 bits (4 bytes) |

---

## Declaration

```aria
// Basic
value: tbb32 = 1000000;

// ERR state
error: tbb32 = ERR;

// Check
when value == ERR then
    handle_error();
end
```

---

## Sticky Error Propagation

```aria
// Errors stick
x: tbb32 = ERR;
y: tbb32 = 100;

sum: tbb32 = x + y;  // ERR
prod: tbb32 = x * y;  // ERR
```

---

## Use Cases

### Safe Calculations

```aria
fn calculate_total(values: []tbb32) -> tbb32 {
    total: tbb32 = 0;
    
    for value in values {
        when value == ERR then
            return ERR;  // Propagate error
        end
        total += value;
    }
    
    return total;
}
```

### Database Operations

```aria
fn get_user_id(name: string) -> tbb32 {
    when not db.is_connected() then
        return ERR;
    end
    
    id: tbb32 = db.query("SELECT id FROM users WHERE name = ?", name);
    return id;  // Returns ERR if not found
}
```

---

## Best Practices

### ✅ DO: Propagate Errors

```aria
fn process_chain() -> tbb32 {
    a: tbb32 = step1();
    when a == ERR then return ERR end
    
    b: tbb32 = step2(a);
    when b == ERR then return ERR end
    
    return b;
}
```

### ✅ DO: Check Before Use

```aria
Result: tbb32 = risky_operation();
when result != ERR then
    use_value(result);
else
    log_error("Operation failed");
end
```

---

## Comparison with i32

| Feature | `tbb32` | `i32` |
|---------|---------|--------|
| Range | -2^31+1 to +2^31-1 | -2^31 to +2^31-1 |
| ERR Sentinel | ✅ Yes | ❌ No |
| Overflow | Becomes ERR | Wraps |
| Error Handling | Built-in | Manual |

---

## Related

- [tbb16](tbb16.md) - 16-bit TBB
- [tbb64](tbb64.md) - 64-bit TBB
- [int32](int32.md) - Standard 32-bit
- [ERR](ERR.md) - ERR constant

---

**Remember**: `tbb32` provides **automatic error propagation**!

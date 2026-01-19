# The `int64` Type

**Category**: Types → Integers  
**Syntax**: `int64` (alias: `i64`)  
**Purpose**: Standard 64-bit signed integer

---

## Overview

`int64` is a standard **64-bit signed integer** using two's complement representation. Use when `i32` range is insufficient.

**Alias**: `i64`

---

## Range

| Aspect | Value |
|--------|-------|
| **Minimum** | `-9,223,372,036,854,775,808` |
| **Maximum** | `+9,223,372,036,854,775,807` |
| **Total Values** | ~18.4 quintillion |
| **Storage** | 64 bits (8 bytes) |

---

## Declaration

```aria
// Explicit type
big_number: int64 = 9000000000;

// Using alias (preferred)
timestamp: i64 = 1640000000000;

// Type suffix
huge := 9999999999i64;
```

---

## Operations

### Arithmetic

```aria
a: i64 = 1000000000000;
b: i64 = 2000000000000;

sum: i64 = a + b;        // 3 trillion
prod: i64 = a * b;       // Still can overflow!
```

### Large Calculations

```aria
// Factorial up to ~20
fact: i64 = 1;
for i: i64 in 1..20 {
    fact *= i;
}
// fact = 2432902008176640000
```

---

## Use Cases

### Timestamps

```aria
// Unix timestamp (milliseconds)
current_time: i64 = get_current_time_ms();

// Date arithmetic
one_day: i64 = 24 * 60 * 60 * 1000;
tomorrow: i64 = current_time + one_day;
```

### Large Counts

```aria
// Website visit counter
views: i64 = 50_000_000_000;

// Database IDs
user_id: i64 = 9223372036854;
```

### File Sizes

```aria
// File sizes in bytes
file_size: i64 = get_file_size("large.bin");
total_space: i64 = 2_000_000_000_000;  // 2 TB
```

---

## Best Practices

### ✅ DO: Use for Large Values

```aria
// Perfect for big numbers
population: i64 = 7_800_000_000;
nanoseconds: i64 = 1_000_000_000_000_000;
```

### ✅ DO: Use for Timestamps

```aria
// Standard for time values
created_at: i64 = timestamp_now();
expires_at: i64 = created_at + 3600000;
```

### ✅ DO: Use Alias `i64`

```aria
// Preferred shorthand
count: i64 = 0;  // ✅ Better
count: int64 = 0;  // ✅ Also fine
```

### ❌ DON'T: Use When i32 Suffices

```aria
// Wasteful
loop_counter: i64 = 0;  // ❌ i32 is enough
loop_counter: i32 = 0;  // ✅ Better
```

### ❌ DON'T: Assume No Overflow

```aria
// Even i64 can overflow!
huge: i64 = 9223372036854775807;
huge += 1;  // Wraps to -9223372036854775808
```

---

## Comparison with TBB64

| Feature | `int64` | `tbb64` |
|---------|---------|---------|
| Range | -2^63 to +2^63-1 | -2^63+1 to +2^63-1 |
| ERR Sentinel | ❌ No | ✅ Yes (-2^63) |
| Overflow | Wraps | Sticky ERR |

---

## Conversions

### From Smaller Types (Safe)

```aria
small: i32 = 1000000;
big: i64 = small as i64;  // Always safe
```

### To Smaller Types (Unsafe)

```aria
big: i64 = 9999999999;
small: i32 = big as i32;  // Truncates!
```

### Checked Conversion

```aria
fn to_i32(value: i64) -> Result<i32> {
    when value < -2147483648 or value > 2147483647 then
        return Err("Value out of range for i32");
    end
    return Ok(value as i32);
}
```

---

## Common Patterns

### Time Calculations

```aria
// Calculate duration in milliseconds
start: i64 = get_time_ms();
// ... do work ...
end: i64 = get_time_ms();
duration: i64 = end - start;

stdout << "Took ${duration}ms";
```

### Large Accumulation

```aria
// Sum large values
total: i64 = 0;
for value in large_values {
    total += value as i64;
}
```

### ID Generation

```aria
// Generate unique IDs
next_id: i64 = 0;

fn generate_id() -> i64 {
    next_id += 1;
    return next_id;
}
```

---

## Platform Considerations

```aria
// i64 is 64-bit on all platforms
// Unlike C's "long" which varies

ptr_size: i64 = sizeof(i64);  // Always 8 bytes
```

---

## Related

- [int32](int32.md) - 32-bit integer
- [int128](int128.md) - 128-bit integer
- [tbb64](tbb64.md) - 64-bit with ERR sentinel
- [uint64](uint64.md) - Unsigned 64-bit

---

**Remember**: `i64` is for **large values**, not everyday counters!

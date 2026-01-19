# The `uint64` Type

**Category**: Types → Unsigned Integers  
**Syntax**: `uint64` (alias: `u64`)  
**Purpose**: 64-bit unsigned integer

---

## Overview

`uint64` is a **64-bit unsigned integer** with values from 0 to ~18.4 quintillion.

**Alias**: `u64`

---

## Range

| Aspect | Value |
|--------|-------|
| **Minimum** | `0` |
| **Maximum** | `18,446,744,073,709,551,615` |
| **Total Values** | ~18.4 quintillion |
| **Storage** | 64 bits (8 bytes) |

---

## Declaration

```aria
// Basic
large: uint64 = 10000000000;

// Alias (preferred)
count: u64 = 18446744073709551615;

// Type suffix
huge := 9999999999999u64;
```

---

## Use Cases

### File Sizes

```aria
// Large files (> 4GB)
file_size: u64 = get_file_size("bigdata.bin");
total_space: u64 = 1000000000000;  // 1 TB
```

### Timestamps

```aria
// Millisecond timestamps
timestamp: u64 = current_time_ms();

// Nanosecond precision
nanos: u64 = time_nanos();
```

### Bit Fields

```aria
// 64-bit flags
permissions: u64 = 0;
permissions |= 0x0001;  // Read
permissions |= 0x0002;  // Write
```

---

## Best Practices

### ✅ DO: Use for Large Counts

```aria
total_bytes: u64 = 5000000000000;  // 5 TB
packet_count: u64 = 9999999999;
```

### ❌ DON'T: Mix with Signed Carelessly

```aria
unsigned: u64 = 10;
signed: i64 = -5;
Result: u64 = unsigned + (signed as u64);  // ❌ Wraps!
```

---

## Related

- [uint32](uint32.md) - 32-bit unsigned
- [uint128](uint128.md) - 128-bit unsigned
- [int64](int64.md) - Signed 64-bit

---

**Remember**: `u64` doubles the positive range of `i64`!

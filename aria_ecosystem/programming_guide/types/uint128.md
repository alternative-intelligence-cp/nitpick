# The `uint128` Type

**Category**: Types → Unsigned Integers  
**Syntax**: `uint128` (alias: `u128`)  
**Purpose**: 128-bit unsigned integer

---

## Overview

`uint128` is a **128-bit unsigned integer** for cryptography and very large positive values.

**Alias**: `u128`

---

## Range

| Aspect | Value |
|--------|-------|
| **Minimum** | `0` |
| **Maximum** | `340,282,366,920,938,463,463,374,607,431,768,211,455` |
| **Total Values** | ~3.4 × 10^38 |
| **Storage** | 128 bits (16 bytes) |

---

## Declaration

```aria
// Type suffix required
big: u128 = 340282366920938463463374607431768211455u128;

// Alias
value: u128 = 999u128;
```

---

## Use Cases

### UUIDs

```aria
// Store UUID as u128
uuid: u128 = parse_uuid("550e8400-e29b-41d4-a716-446655440000");
```

### Cryptography

```aria
// Large unsigned keys
key: u128 = generate_128bit_key();
```

---

## Performance

```aria
// Slower than u64 - software emulation
// Use only when necessary
```

---

## Related

- [uint64](uint64.md) - 64-bit unsigned
- [uint256](uint256.md) - 256-bit unsigned
- [int128](int128.md) - Signed 128-bit

---

**Remember**: `u128` is **slow** but has **huge range**!

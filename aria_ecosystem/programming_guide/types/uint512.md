# The `uint512` Type

**Category**: Types → Unsigned Integers  
**Syntax**: `uint512` (alias: `u512`)  
**Purpose**: 512-bit unsigned integer for advanced cryptography

---

## Overview

`uint512` is a **512-bit unsigned integer** for specialized cryptographic applications.

**Alias**: `u512`

**⚠️ WARNING**: Extremely slow!

---

## Range

| Aspect | Value |
|--------|-------|
| **Minimum** | `0` |
| **Maximum** | ~1.34 × 10^154 |
| **Storage** | 512 bits (64 bytes) |

---

## Use Cases

### Advanced Cryptography Only

```aria
// RSA-4096 operations
modulus: u512 = gen_4096bit_modulus();
```

---

## Performance

```aria
// ⚠️ EXTREMELY SLOW
// Avoid unless absolutely necessary
```

---

## Related

- [uint256](uint256.md) - 256-bit unsigned
- [int512](int512.md) - Signed 512-bit

---

**Remember**: `u512` is **EXTREMELY SLOW** - avoid!

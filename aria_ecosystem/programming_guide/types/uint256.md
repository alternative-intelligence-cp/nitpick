# The `uint256` Type

**Category**: Types → Unsigned Integers  
**Syntax**: `uint256` (alias: `u256`)  
**Purpose**: 256-bit unsigned integer for blockchain

---

## Overview

`uint256` is a **256-bit unsigned integer** primarily used in Ethereum and blockchain applications.

**Alias**: `u256`

---

## Range

| Aspect | Value |
|--------|-------|
| **Minimum** | `0` |
| **Maximum** | ~1.16 × 10^77 |
| **Storage** | 256 bits (32 bytes) |

---

## Declaration

```aria
// Type suffix required
balance: u256 = 1000000000000000000u256;  // 1 ETH in wei

// Alias
value: u256 = 999u256;
```

---

## Use Cases

### Ethereum/Blockchain

```aria
// Smart contract balances
balance: u256 = 1000000000000000000u256;
total_supply: u256 = 21000000000000000000000000u256;

// ERC-20 token amounts
fn transfer(amount: u256) -> Result<void> {
    when balance < amount then
        return Err("Insufficient balance");
    end
    balance -= amount;
    return Ok();
}
```

### Large Hashes

```aria
// SHA-256 hashes as integers
hash: u256 = sha256_to_u256(data);
```

---

## Performance

```aria
// ⚠️ VERY SLOW - software emulation only
// Only use for blockchain/crypto
```

---

## Best Practices

### ✅ DO: Use for Blockchain

```aria
wei: u256 = eth_to_wei(1.5);
```

### ❌ DON'T: Use for Normal Code

```aria
// ❌ Extremely slow
counter: u256 = 0u256;
```

---

## Related

- [uint128](uint128.md) - 128-bit unsigned
- [uint512](uint512.md) - 512-bit unsigned
- [int256](int256.md) - Signed 256-bit

---

**Remember**: `u256` is for **blockchain** - extremely slow otherwise!

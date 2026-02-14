# Large Unsigned Integers: uint128, uint256, uint512

## Overview

**Purpose**: Large unsigned integers beyond native CPU support, for non-negative values requiring extended range or security applications.

**Sizes and Ranges**:
- `uint128`: 128-bit unsigned (0 to 340,282,366,920,938,463,463,374,607,431,768,211,455)
- `uint256`: 256-bit unsigned (0 to ~1.2×10⁷⁷)
- `uint512`: 512-bit unsigned (0 to ~1.3×10¹⁵⁴)

**Key Characteristics**:
- All require software implementation (no native CPU operations)
- Double the positive range compared to signed equivalents
- **CRITICAL UNDERFLOW**: 0 - 1 wraps to MAX (astronomical values!)
- No negative values (only zero and positive)
- For sizes, counts, bit manipulation, and unsigned crypto operations

**When to Use**:
- uint128: Object counts, large capacities, unsigned UUIDs, IPv6 bitmasks
- uint256: Unsigned hash operations, token balances, bitmasks
- uint512: Unsigned post-quantum crypto, extreme capacities, wide bitmasks

**vs Signed Large Integers** (int128/256/512):
- Unsigned: Double positive range, underflow danger, bit manipulation friendly
- Signed: Negative values, symmetric ranges (sort of), overflow symmetry

---

## uint128: 128-Bit Unsigned Integer

### Range and Representation

**Range**: 0 to 340,282,366,920,938,463,463,374,607,431,768,211,455

**Approximately**: 0 to 3.4×10³⁸

**Bit Pattern**: 128 bits (16 bytes, all value bits)
- No sign bit (all 128 bits store value)
- Double the positive range of int128

```aria
uint128:min = 0;
uint128:max = 340282366920938463463374607431768211455;  // 2^128 - 1

// Comparison to int128
int128:signed_max = int128.max;
uint128:unsigned_max = uint128.max;
// unsigned_max is ~2× larger than signed_max (positive range)
```

### CRITICAL: Underflow Behavior

**0 - 1 = MAX** (catastrophic wraparound!):

```aria
uint128:count = 0;
count -= 1;  // WRAPS TO 340282366920938463463374607431768211455!

// Counting down trap
uint128:remaining = items.length;
loop till remaining == 0 {
    process(items[remaining]);
    remaining -= 1;  // DANGER: Will never hit 0, wraps to MAX on last iteration!
}
```

**Safe Countdown Pattern**:
```aria
// NEVER count down with unsigned to zero
// Instead use index counting up
loop (items.length:i) {
    process(items[items.length - 1 - i]);  // Access in reverse order safely
}

// Or check before decrement
uint128:remaining = items.length;
loop till remaining == 0 {
    remaining -= 1;  // Decrement FIRST
    process(items[remaining]);  // Then use (never wraps)
}
```

### Common Use Cases

**1. Object Counts and Capacities**

When uint64 (max ~1.8×10¹⁹) is insufficient:

```aria
// Total number of database records (beyond uint64)
uint128:total_records = 0;
loop (shards.length:i) {
    total_records += shards[i].record_count;
}

// Memory allocations (theoretical large systems)
uint128:total_bytes_allocated = 0;
uint128:allocation = allocate_memory(size);
total_bytes_allocated += size;
```

**2. UUID Bit Manipulation**

UUIDs as unsigned values (no sign bit complications):

```aria
// UUID: 550e8400-e29b-41d4-a716-446655440000
uint128:uuid = 0x550e8400e29b41d4a716446655440000;

// Extract version (bits 48-51)
uint128:version_mask = 0x0000000000000000F000000000000000;
uint8:version = ((uuid & version_mask) >> 52).to_uint8();

// Set variant bits (bits 64-65)
uint128:variant_mask = 0x00000000000000003FFFFFFFFFFFFFFF;
uuid = (uuid & variant_mask) | 0x0000000000000000C000000000000000;
```

**3. IPv6 Subnet Calculations**

IPv6 as unsigned (natural bit operations):

```aria
// IPv6 address as uint128
uint128:ipv6 = 0x20010db885a3000000008a2e03707334;

// Subnet mask (CIDR /64)
uint128:mask = 0xffffffffffffffff0000000000000000;

// Network address (AND operation)
uint128:network = ipv6 & mask;

// Broadcast address (OR with inverted mask)
uint128:broadcast = ipv6 | ~mask;

// Number of addresses in subnet
uint128:subnet_size = ~mask + 1;  // 2^64 addresses for /64
```

**4. Large Counter Arrays**

When counting events beyond uint64:

```aria
// Event counters (billions of events over years)
struct:EventCounters {
    uint128:total_events;
    uint128:successful_events;
    uint128:failed_events;
}

void:record_event(EventCounters:counters, bool:success) {
    counters.total_events += 1;
    if (success) {
        counters.successful_events += 1;
    } else {
        counters.failed_events += 1;
    }
}
```

### Bit Manipulation Excellence

**No Sign Bit** = All 128 bits available:

```aria
// Right shift fills with zeros (not sign extension)
uint128:value = 0x80000000000000000000000000000000;
uint128:shifted = value >> 1;  // 0x40000000000000000000000000000000 (clean shift)

// vs int128 (sign extends)
int128:signed_value = -0x80000000000000000000000000000000;
int128:signed_shifted = signed_value >> 1;  // Sign bit propagates (arithmetic shift)
```

**Bitmask Operations**:
```aria
// Set specific bits
uint128:flags = 0;
uint128:bit_127 = 1u128 << 127;  // Set highest bit
flags |= bit_127;

// Clear specific bits
uint128:mask = ~(1u128 << 64);  // Clear bit 64
flags &= mask;

// Toggle bits
flags ^= bit_127;  // Flip bit 127
```

---

## uint256: 256-Bit Unsigned Integer

### Range and Representation

**Range**: 0 to ~1.2×10⁷⁷ (exactly 2²⁵⁶ - 1)

**Bit Pattern**: 256 bits (32 bytes, all value bits)

**Enormous Positive Range**:
- uint128 → uint256: Multiply range by 3.4×10³⁸
- Double the positive range of int256

```aria
uint256:min = 0;
uint256:max = (2**256) - 1;

// All 256 bits available for value
uint256:all_ones = 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff;
// = 115792089237316195423570985008687907853269984665640564039457584007913129639935
```

### Underflow Amplified

**0 - 1 = 1.2×10⁷⁷** (catastrophic!):

```aria
uint256:balance = 0;
balance -= 1;  // WRAPS TO 115792089237316195423570985008687907853269984665640564039457584007913129639935

// This is always wrong - check before subtraction
if (balance >= withdrawal) {
    balance -= withdrawal;  // Safe
} else {
    log.write("ERROR: Insufficient balance\n");
}
```

### Common Use Cases

**1. Unsigned Hash Operations**

SHA-256 as unsigned (bit manipulation friendly):

```aria
// SHA-256 hash as uint256
uint256:hash = compute_sha256(data);

// Bit manipulation on hash (e.g., proof-of-work)
uint256:target = 0x0000000000000000000ffff0000000000000000000000000000000000000000000;

// Leading zeros check (mining difficulty)
uint8:leading_zeros = count_leading_zeros(hash);
if (hash < target) {
    log.write("Valid proof of work!\n");
}
```

**2. Token Balances (Blockchain)**

Ethereum and other blockchains use uint256 for balances:

```aria
// Token balance (wei, smallest unit)
uint256:balance = 1500000000000000000;  // 1.5 ether

// Transfer with underflow protection
void:transfer(uint256:from_balance, uint256:to_balance, uint256:amount) -> bool {
    if (from_balance < amount) {
        log.write("ERROR: Insufficient balance\n");
        return false;
    }
    from_balance -= amount;  // Safe after check
    to_balance += amount;
    return true;
}

// Overflow check on receiving
if (to_balance > uint256.max - amount) {
    log.write("ERROR: Balance overflow\n");
    return false;
}
```

**3. Wide Bitmasks**

256 bits of flags:

```aria
// Feature flags (256 bits = 256 features)
uint256:enabled_features = 0;

// Enable feature 137
uint256:feature_137 = 1u256 << 137;
enabled_features |= feature_137;

// Check if feature enabled
bool:is_enabled = (enabled_features & feature_137) != 0;

// Disable feature
enabled_features &= ~feature_137;
```

**4. Modular Arithmetic (Cryptography)**

Large modulus operations:

```aria
// Modular exponentiation (a^b mod m)
uint256:a = base;
uint256:b = exponent;
uint256:m = modulus;

uint256:result = mod_exp(a, b, m);

// Addition with modular reduction
uint256:mod_add(uint256:a, uint256:b, uint256:m) -> uint256 {
    // Use uint512 to prevent overflow
    uint512:sum = a.to_uint512() + b.to_uint512();
    return (sum % m.to_uint512()).to_uint256();
}
```

### Performance and Storage

**Size**: 32 bytes per uint256
- Same as int256
- Twice the size of uint128
- Operations similarly expensive (software emulation)

**When uint256 vs int256**:
```aria
// Use uint256 when:
// - Bit manipulation needed
// - Values always non-negative
// - Need full 256 bits of range

// Use int256 when:
// - Signed comparisons needed
// - Can have negative values
// - Symmetric range helpful
```

---

## uint512: 512-Bit Unsigned Integer

### Range and Representation

**Range**: 0 to ~1.3×10¹⁵⁴ (exactly 2⁵¹² - 1)

**Bit Pattern**: 512 bits (64 bytes, all value bits)

**Incomprehensibly Large**:
- Vastly exceeds any physical quantity
- More bits than atoms in observable universe
- Essentially "infinite" for practical purposes

```aria
uint512:min = 0;
uint512:max = (2**512) - 1;

// Even uint256 is tiny compared to uint512
uint512:ratio = uint512.max / uint256.max.to_uint512();
// ratio ≈ 1.3×10^77
```

### Common Use Cases

**1. Unsigned Post-Quantum Cryptography**

Some post-quantum schemes use unsigned 512-bit values:

```aria
// Lattice-based cryptography (unsigned coefficients)
uint512:lattice_point = compute_lattice_point();

// Unsigned modular operations
uint512:modulus = large_prime_512;
uint512:reduced = lattice_point % modulus;
```

**2. Concatenated Hashes**

Four SHA-256 hashes as single uint512:

```aria
// Combine four 256-bit hashes
uint256:h1 = sha256(data1);
uint256:h2 = sha256(data2);
uint256:h3 = sha256(data3);
uint256:h4 = sha256(data4);

uint512:combined = 
    (h1.to_uint512() << 256*3) |
    (h2.to_uint512() << 256*2) |
    (h3.to_uint512() << 256) |
    h4.to_uint512();

// Single comparison instead of four
if (combined == expected_combined) {
    log.write("All four hashes match!\n");
}
```

**3. Extreme Capacity Tracking**

Theoretical systems with astronomical limits:

```aria
// Total possible IPv6 addresses in universe (128 bits × many subnets)
uint512:total_ipv6_capacity = (2u512**128) * number_of_subnets;

// Bits processed over system lifetime
uint512:total_bits_processed = 0;
loop (operations.length:i) {
    total_bits_processed += operations[i].bit_count;
}
```

**4. Wide Cryptographic Nonces**

Non-repeating nonces for extreme security margins:

```aria
// 512-bit nonce (never repeats in practice)
uint512:nonce = generate_random_uint512();

// Increment for next message (will not exhaust in universe lifetime)
nonce += 1;
```

### When uint512 Is Overkill

**Almost Always** - consider uint256 or smaller:

```aria
// BAD: uint512 for reasonable counters
uint512:event_count = 0;  // Will never exceed uint64, let alone uint512

// GOOD: Right-size the type
uint64:event_count = 0;  // Sufficient for 18 quintillion events
```

---

## Common Characteristics of Large Unsigned Integers

### Software Implementation

**Same as signed equivalents**:
- Multi-precision arithmetic
- Addition: Add chunks with carry propagation
- Multiplication: Long multiplication
- Division: Long division (expensive!)

**Performance**:
```aria
// Same relative costs as signed
uint64:  1x   (baseline)
uint128: 3-5x
uint256: 8-15x  
uint512: 20-40x

// Division is still the slowest operation
uint512:quotient = dividend / divisor;  // Hundreds of cycles
```

### Underflow: The Silent Killer

**Underflow wraps to MAX** (unlike signed overflow which is "just" wrong):

```aria
uint256:balance = 100;
balance -= 200;  // UNDERFLOW! Now balance = uint256.max - 99 (astronomically large!)

// Always check before subtraction
if (balance >= withdrawal) {
    balance -= withdrawal;  // Safe
} else {
    handle_insufficient_funds();
}
```

**Underflow Detection**:
```aria
// After subtraction, check if result became huge
uint256:original = balance;
balance -= amount;

if (balance > original) {
    // UNDERFLOW OCCURRED! (smaller - larger = wrapped to huge)
    log.write("ERROR: Underflow detected\n");
    balance = 0;  // Or handle appropriately
}
```

### Overflow Behavior

**Wrapping Overflow** (like signed):

```aria
uint128:x = uint128.max;
x += 1;  // Wraps to 0

// Addition overflow detection
uint256:a = large_value_1;
uint256:b = large_value_2;
uint256:sum = a + b;

// Overflow if sum < either operand
if (sum < a || sum < b) {
    log.write("ERROR: Addition overflow\n");
}
```

**Saturating Arithmetic**:
```aria
uint256:saturating_add(uint256:a, uint256:b) -> uint256 {
    uint256:sum = a + b;
    if (sum < a) {  // Overflow occurred
        return uint256.max;  // Saturate
    }
    return sum;
}

uint256:saturating_sub(uint256:a, uint256:b) -> uint256 {
    if (a < b) {  // Would underflow
        return 0;  // Saturate
    }
    return a - b;
}
```

### Bit Manipulation Advantages

**All bits are value bits** (no sign complications):

```aria
// Right shift is always logical (zeros fill from left)
uint256:value = 0x8000000000000000000000000000000000000000000000000000000000000000;
uint256:shifted = value >> 1;  
// = 0x4000000000000000000000000000000000000000000000000000000000000000

// No sign extension issues
uint128:mask = ~0u128;  // All bits set
uint128:half_mask = mask >> 64;  // Clean 64-bit shift (zeros on left)
```

**Rotation Operations**:
```aria
// Rotate left (bits wrap around to right)
uint256:rotate_left(uint256:value, uint8:bits) -> uint256 {
    return (value << bits) | (value >> (256 - bits));
}

// Rotate right
uint256:rotate_right(uint256:value, uint8:bits) -> uint256 {
    return (value >> bits) | (value << (256 - bits));
}
```

---

## Large Unsigned vs Signed Integers

### Range Comparison

| Type | Bytes | Range |
|------|-------|-------|
| int128 | 16 | -1.7×10³⁸ to +1.7×10³⁸ |
| uint128 | 16 | 0 to 3.4×10³⁸ |
| int256 | 32 | ±5.8×10⁷⁶ |
| uint256 | 32 | 0 to 1.2×10⁷⁷ |
| int512 | 64 | ±6.7×10¹⁵³ |
| uint512 | 64 | 0 to 1.3×10¹⁵⁴ |

**Unsigned has 2× positive range** (sign bit becomes value bit)

### When to Choose Unsigned

**Use uint128/256/512 when**:
- ✅ Values are NEVER negative (counts, sizes, capacities)
- ✅ Bit manipulation needed (bitmasks, flags)
- ✅ Need maximum positive range (double that of signed)
- ✅ Unsigned arithmetic semantics (hashes, crypto)

**Use int128/256/512 when**:
- ✅ Can have negative values (deltas, offsets, signed comparisons)
- ✅ Symmetric range helpful (positive/negative balance)
- ✅ Signed overflow behavior desired

### Mixing Signed and Unsigned

**Dangerous** - explicit conversion required:

```aria
// BAD: Implicit conversion undefined
int256:signed_value = -1000;
uint256:unsigned_value = signed_value;  // COMPILE ERROR in Aria

// GOOD: Explicit with validation
uint256:to_unsigned_safe(int256:value) -> uint256 {
    if (value < 0) {
        log.write("ERROR: Cannot convert negative to unsigned\n");
        return 0;
    }
    return value.to_uint256();
}

// Signed to unsigned (when value known non-negative)
int256:positive = 5000;
uint256:unsigned = positive.to_uint256();  // Safe if positive

// Unsigned to signed (check overflow)
uint256:large = uint256.max;
if (large > int256.max.to_uint256()) {
    log.write("ERROR: Value exceeds int256 range\n");
}
```

---

## Common Patterns

### Safe Subtraction

```aria
// Always check before subtracting unsigned
uint256:safe_subtract(uint256:a, uint256:b) -> uint256 {
    if (a < b) {
        log.write("ERROR: Underflow would occur\n");
        return 0;  // Or return ERR
    }
    return a - b;
}

// Usage
uint256:balance = 100;
uint256:withdrawal = 150;
balance = safe_subtract(balance, withdrawal);  // Returns 0, doesn't underflow
```

### Checked Addition

```aria
// Detect overflow on addition
uint256:checked_add(uint256:a, uint256:b) -> uint256 {
    uint256:sum = a + b;
    if (sum < a) {  // Overflow occurred
        log.write("ERROR: Addition overflow\n");
        return uint256.max;  // Saturate
    }
    return sum;
}
```

### Bit Counting

```aria
// Count set bits (population count)
uint8:popcount(uint256:value) -> uint8 {
    uint8:count = 0;
    loop till value == 0 {
        if ((value & 1) != 0) {
            count += 1;
        }
        value >>= 1;
    }
    return count;
}

// Count leading zeros
uint16:count_leading_zeros(uint256:value) -> uint16 {
    if (value == 0) return 256;
    
    uint16:count = 0;
    uint256:mask = 1u256 << 255;  // Highest bit
    loop (256 times) {
        if ((value & mask) != 0) return count;
        count += 1;
        mask >>= 1;
    }
    return count;
}
```

### Range-Checked Narrowing

```aria
// uint256 to uint128 with overflow check
uint128:to_uint128_safe(uint256:value) -> uint128 {
    if (value > uint128.max.to_uint256()) {
        log.write("ERROR: Value exceeds uint128 range\n");
        return uint128.max;  // Saturate
    }
    return value.to_uint128();
}
```

---

## Anti-Patterns

### Counting Down to Zero

```aria
// BAD: Unsigned countdown to zero (INFINITE LOOP!)
uint256:i = 10;
loop till i == 0 {
    process(i);
    i -= 1;  // i becomes uint256.max when i was 0!
}

// GOOD: Count up instead
loop (10 times:i) {
    process(10 - i);
}

// ALSO GOOD: Decrement first
uint256:i = 10;
loop till i == 0 {
    i -= 1;  // Decrement first
    process(i);  // Then use (never wraps)
}
```

### Ignoring Underflow

```aria
// BAD: Subtract without checking
uint256:balance = 100;
balance -= 200;  // UNDERFLOW! balance is now huge

// GOOD: Check first
if (balance >= 200) {
    balance -= 200;
} else {
    log.write("ERROR: Insufficient balance\n");
}
```

### Converting Negative to Unsigned

```aria
// BAD: Convert negative signed to unsigned
int256:delta = -500;
uint256:unsigned_delta = delta.to_uint256();  // WRONG! Becomes huge positive

// GOOD: Handle negative values explicitly
uint256:absolute_value(int256:value) -> uint256 {
    if (value < 0) {
        return (-value).to_uint256();  // Negate first
    }
    return value.to_uint256();
}
```

### Using Large Unsigned for Signed Ranges

```aria
// BAD: Using unsigned when negatives needed
uint256:temperature_offset = initial - current;  // UNDERFLOW if current > initial!

// GOOD: Use signed when values can be negative
int256:temperature_offset = initial - current;  // Safe for negative deltas
```

---

## Performance Guidelines

### When to Use Large Unsigned Integers

**uint128**:
- ✅ Large counts, capacities (beyond uint64)
- ✅ UUID bit manipulation
- ✅ IPv6 subnet math
- ❌ Values that can be negative (use int128)
- ❌ When uint64 sufficient (check max first)

**uint256**:
- ✅ Unsigned hash operations
- ✅ Blockchain token balances
- ✅ 256-bit bitmasks
- ✅ Modular arithmetic (crypto)
- ❌ Signed comparisons needed (use int256)
- ❌ When uint128 sufficient

**uint512**:
- ✅ Unsigned post-quantum crypto
- ✅ Multiple hash concatenation
- ✅ Extreme capacity tracking
- ❌ Almost everything else (overkill)

### Optimization Tips

1. **Always check underflow** before subtraction:
   ```aria
   if (a >= b) {
       result = a - b;
   }
   ```

2. **Use bit operations** (unsigned is ideal):
   ```aria
   uint256:flag = 1u256 << bit_position;
   flags |= flag;  // Set bit
   ```

3. **Saturate instead of wrap**:
   ```aria
   result = saturating_add(a, b);  // Clamp to MAX on overflow
   ```

4. **Right-size types**:
   ```aria
   // Check max value before choosing type
   if (max_expected < uint64.max) {
       // Use faster uint64
   }
   ```

---

## Summary

**uint128/256/512 are large unsigned precision types**:
- Double positive range of signed equivalents
- No negative values (zero and positive only)
- **CRITICAL UNDERFLOW**: 0 - 1 = MAX (catastrophic)
- Excellent for bit manipulation (all value bits)
- Software emulation (performance cost)

**Use the right tool**:
- uint128: Large counts, UUIDs, IPv6 math
- uint256: Unsigned hashes, blockchain, bitmasks, modular arithmetic
- uint512: Post-quantum crypto, concatenated hashes, extreme capacities

**Key Principle**: **Always check before subtracting unsigned** - underflow wraps to astronomical values!

**vs Signed**: Use unsigned when values are NEVER negative and you need maximum positive range or bit manipulation. Use signed when negatives possible.

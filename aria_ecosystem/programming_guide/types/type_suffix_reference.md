# Type Suffix Quick Reference

**Quick reference for Aria's explicit type literal syntax.**

## Unsigned Integers

| Type | Range | Suffix | Example |
|------|-------|--------|---------|
| `uint8` | 0 to 255 | `u8` | `255u8` |
| `uint16` | 0 to 65,535 | `u16` | `8080u16` |
| `uint32` | 0 to 4,294,967,295 | `u32` | `1000u32` |
| `uint64` | 0 to 2^64-1 | `u64` | `1024u64` |
| `uint128` | 0 to 2^128-1 | `u128` | `999u128` |
| `uint256` | 0 to 2^256-1 | `u256` | `123u256` |
| `uint512` | 0 to 2^512-1 | `u512` | `456u512` |
| `uint1024` | 0 to 2^1024-1 | `u1024` | `789u1024` |

## Signed Integers

| Type | Range | Suffix | Example |
|------|-------|--------|---------|
| `int8` | -128 to 127 | `i8` | `-40i8` |
| `int16` | -32,768 to 32,767 | `i16` | `-1000i16` |
| `int32` | -2^31 to 2^31-1 | `i32` | `0i32` |
| `int64` | -2^63 to 2^63-1 | `i64` | `-5000i64` |
| `int128` | -2^127 to 2^127-1 | `i128` | `999i128` |
| `int256` | -2^255 to 2^255-1 | `i256` | `-123i256` |
| `int512` | -2^511 to 2^511-1 | `i512` | `456i512` |
| `int1024` | -2^1023 to 2^1023-1 | `i1024` | `789i1024` |

## Twisted Balanced Binary (TBB)

| Type | Range | ERR Value | Suffix | Example |
|------|-------|-----------|--------|---------|
| `tbb8` | -127 to +127 | -128 | `tbb8` | `-127tbb8` |
| `tbb16` | -32,767 to +32,767 | -32,768 | `tbb16` | `0tbb16` |
| `tbb32` | -2^31+1 to 2^31-1 | -2^31 | `tbb32` | `100tbb32` |
| `tbb64` | -2^63+1 to 2^63-1 | -2^63 | `tbb64` | `-50tbb64` |

**Note:** TBB types have symmetric ranges with ERR sentinels at minimum value.

## Floating Point

| Type | Precision | Suffix | Example |
|------|-----------|--------|---------|
| `flt32` | 32-bit float | `f32` | `3.14f32` |
| `flt64` | 64-bit float | `f64` | `2.718f64` |
| `flt128` | 128-bit float | `f128` | `1.234f128` |

## Fixed Point

| Type | Format | Precision | Suffix | Example |
|------|--------|-----------|--------|---------|
| `fix256` | Q128.128 | 2^-128 | `fix256` | `10.5fix256` |

**Note:** `fix256` has 128 integer bits and 128 fractional bits for deterministic arithmetic.

## Common Values Reference

### Zero Values

```aria
uint8:zero_u8 = 0u8;
uint32:zero_u32 = 0u32;
int32:zero_i32 = 0i32;
int64:zero_i64 = 0i64;
flt32:zero_f32 = 0.0f32;
tbb8:zero_tbb8 = 0tbb8;
```

### One Values

```aria
uint32:one_u32 = 1u32;
int32:one_i32 = 1i32;
flt32:one_f32 = 1.0f32;
fix256:one_fix = 1.0fix256;
```

### Common Constants

```aria
// Math constants
flt64:pi = 3.141592653589793f64;
flt64:e = 2.718281828459045f64;
flt32:golden_ratio = 1.618033f32;

// Bitmasks
uint8:all_bits_u8 = 0xFFu8;
uint32:all_bits_u32 = 0xFFFFFFFFu32;
uint64:high_bit = 0x8000000000000000u64;

// Array sizes
int32:buffer_size = 1024i32;
uint32:max_users = 1000u32;

// Sentinel values
tbb8:err_tbb8 = -128tbb8;       // ERR sentinel
tbb16:err_tbb16 = -32768tbb16;  // ERR sentinel
```

## In Expressions

### Arithmetic

```aria
// Addition
uint32:sum = (10u32 + 5u32);

// Subtraction
int32:diff = (100i32 - 42i32);

// Multiplication
flt32:product = (3.14f32 * 2.0f32);

// Division
uint64:quotient = (1000u64 / 10u64);
```

### Comparisons

```aria
// Equality
if (count == 0u32) { reset(); }

// Less than
if (temp < -40tbb8) { freeze_warning(); }

// Range check
if (value > 0i32 && value < 100i32) { valid(); }
```

### Bitwise Operations

```aria
// OR
uint32:flags = (flag1 | 0x01u32);

// AND
uint32:masked = (value & 0xFFu32);

// XOR
uint32:flipped = (bits ^ 0xFFFFFFFFu32);

// Shift
uint32:doubled = (value << 1u32);
```

### Loops

```aria
// For loop
for (int32:i = 0i32; i < 10i32; i++) {
    process(i);
}

// While loop
uint32:count = 0u32;
while (count < 100u32) {
    do_work();
    count = (count + 1u32);
}
```

## Type Conversion

When types don't match, use explicit conversion:

```aria
// Convert between signed and unsigned
int32:signed_val = -5i32;
uint32:unsigned_val = convert<uint32>(signed_val);  // Explicit

// Widen TBB safely (preserves ERR sentinel)
tbb8:small = ERR;
tbb16:wide = tbb_widen<tbb16>(small);  // Maps ERR correctly

// Floating to integer
flt32:float_val = 3.7f32;
int32:truncated = convert<int32>(float_val);
```

## Common Mistakes

### ❌ Incorrect: Missing suffix

```aria
uint32:mask = 1;       // ERROR: No suffix
int32:count = 0;       // ERROR: No suffix
flt32:ratio = 2.5;     // ERROR: No suffix
```

### ✅ Correct: With suffix

```aria
uint32:mask = 1u32;    // OK
int32:count = 0i32;    // OK
flt32:ratio = 2.5f32;  // OK
```

### ❌ Incorrect: Type mismatch

```aria
uint32:a = 10u32;
uint32:b = (a + 5i32);  // ERROR: Mixing uint32 and int32
```

### ✅ Correct: Matching types

```aria
uint32:a = 10u32;
uint32:b = (a + 5u32);  // OK: Both uint32
```

## See Also

- [Zero Implicit Conversion Policy](zero_implicit_conversion.md) - Full explanation
- [Type System Overview](tbb_overview.md) - All Aria types
- [TBB ERR Sentinels](tbb_err_sentinel.md) - Error handling
- [Integer Types](int32.md) - Signed integers
- [Unsigned Types](uint32.md) - Unsigned integers

---

**Quick Rule**: Every numeric literal MUST have a type suffix. No exceptions.

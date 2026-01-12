# Aria Type System Guide

**Version**: 0.0.7-dev  
**Last Updated**: January 5, 2026  
**Audience**: Developers, Language Designers, Safety Engineers

## Table of Contents

1. [Introduction](#introduction)
2. [Explicit Typing Philosophy](#explicit-typing-philosophy)
3. [TBB (Twisted Balanced Binary)](#tbb-twisted-balanced-binary)
4. [LBIM (Large Binary Integer Mathematics)](#lbim-large-binary-integer-mathematics)
5. [Fixed-Point Types (fix256-fix4096)](#fixed-point-types)
6. [Standard Types](#standard-types)
7. [Type Conversions](#type-conversions)
8. [Alignment Requirements](#alignment-requirements)
9. [Memory Layout](#memory-layout)
10. [Best Practices](#best-practices)

---

## Introduction

Aria's type system is designed with three core principles:

1. **Explicitness**: No hidden conversions, no type inference magic
2. **Safety**: Error states are part of the type system, not exceptions
3. **Performance**: Zero-cost abstractions with predictable memory layout

This guide covers Aria's unique type system features that distinguish it from traditional systems languages.

---

## Explicit Typing Philosophy

### The `type:name = value` Syntax

Aria requires **explicit type annotations** for all variable declarations:

```aria
int32:x = 42;           // Required
flt64:pi = 3.14159;     // Required
tbb32:result = compute(); // Required
```

**This is intentional:**
- No type inference (`auto`, `var`, `let`)
- No implicit conversions
- What you see is exactly what you get

### Why No Type Inference?

Type inference can hide critical information:

```c++
// C++: What type is x?
auto x = getValue();  // int? long? size_t? Who knows without looking up getValue()
```

```aria
// Aria: Type is always visible
int32:x = getValue();  // x is int32, period
```

**Benefits**:
1. **Readability**: Type is immediately visible at declaration site
2. **Maintainability**: Changing function return type causes explicit errors, not silent bugs
3. **Performance**: No compiler guessing, no template bloat

### The Cost of Explicitness

Yes, you type more characters. But you also:
- Catch bugs at compile time
- Make code self-documenting
- Avoid subtle type-related performance issues

---

## TBB (Twisted Balanced Binary)

### The Two's Complement Problem

Standard signed integers have an asymmetry:

```c
// Standard int8: range [-128, +127]
int8_t x = -128;
int8_t y = -x;        // UNDEFINED BEHAVIOR! -(-128) = 128 overflows
int8_t z = abs(-128); // UNDEFINED BEHAVIOR!
```

This asymmetry has caused countless bugs in production systems.

### TBB Solution

**TBB types reserve the minimum value as an error sentinel**, creating **perfect symmetry**:

```aria
// tbb8: range [-127, +127]
tbb8:x = -127;
tbb8:y = -x;        // y = 127 (works perfectly!)
tbb8:z = abs(-127); // z = 127 (no overflow!)
```

### TBB Type Family

| Type   | Storage | Valid Range                    | ERR Sentinel                   | Hex ERR     |
|--------|---------|--------------------------------|--------------------------------|-------------|
| tbb8   | 8-bit   | [-127, +127]                   | -128                           | 0x80        |
| tbb16  | 16-bit  | [-32,767, +32,767]             | -32,768                        | 0x8000      |
| tbb32  | 32-bit  | [-2,147,483,647, +2,147,483,647] | -2,147,483,648               | 0x80000000  |
| tbb64  | 64-bit  | [-9,223,372,036,854,775,807, +9,223,372,036,854,775,807] | -9,223,372,036,854,775,808 | 0x8000000000000000 |

### Sticky Error Propagation

TBB's killer feature is **sticky errors** - errors propagate automatically without explicit checks:

```aria
tbb32:a = 1000000;
tbb32:b = 5000;
tbb32:c = a * b;      // Overflow! c = ERR32

// ERR is "sticky" - propagates through all subsequent operations
tbb32:d = c + 100;    // ERR + 100 = ERR
tbb32:e = d * 2;      // ERR * 2 = ERR
tbb32:f = e - 50;     // ERR - 50 = ERR

// Check error once at the end
if (c == ERR32) {
    print_newline();  // Error occurred somewhere in calculation chain
} else {
    // Result is valid
}
```

**This is revolutionary** - you can build long arithmetic pipelines without checking at every step.

### TBB Constants

```aria
// ERR sentinels
tbb8:err8 = ERR8;      // -128
tbb16:err16 = ERR16;   // -32,768
tbb32:err32 = ERR32;   // -2,147,483,648
tbb64:err64 = ERR64;   // INT64_MIN

// Maximum values
tbb8:max8 = MAX8;      // +127
tbb16:max16 = MAX16;   // +32,767
tbb32:max32 = MAX32;   // +2,147,483,647
tbb64:max64 = MAX64;   // +9,223,372,036,854,775,807

// Minimum valid values (not ERR!)
tbb8:min8 = MIN8;      // -127
tbb16:min16 = MIN16;   // -32,767
tbb32:min32 = MIN32;   // -2,147,483,647
tbb64:min64 = MIN64;   // -9,223,372,036,854,775,807
```

### When ERR is Generated

ERR is automatically produced by:

1. **Overflow**: Result exceeds valid range
   ```aria
   tbb8:x = 100;
   tbb8:y = 50;
   tbb8:z = x + y;  // 150 > 127, so z = ERR8
   ```

2. **Underflow**: Result below valid range
   ```aria
   tbb8:a = -100;
   tbb8:b = -50;
   tbb8:c = a + b;  // -150 < -127, so c = ERR8
   ```

3. **Division by Zero**:
   ```aria
   tbb32:x = 100;
   tbb32:y = 0;
   tbb32:z = x / y;  // z = ERR32
   ```

4. **Propagation**: Any operation with ERR input
   ```aria
   tbb32:err = ERR32;
   tbb32:x = err + 1;  // x = ERR32
   tbb32:y = err * 5;  // y = ERR32
   ```

### Detecting ERR

Use intrinsics to check for ERR:

```aria
tbb32:result = compute();

if (@is_err_tbb32(result)) {
    // Handle error
} else {
    // Use result safely
}

// Or check for any sentinel
if (@is_sentinel_tbb32(result)) {
    // ERR, NULL, or INF
} else {
    // Normal numeric value
}
```

### TBB vs Standard Integers

| Feature | Standard int32 | tbb32 |
|---------|---------------|-------|
| Range | [-2,147,483,648, +2,147,483,647] | [-2,147,483,647, +2,147,483,647] |
| Symmetry | ❌ Asymmetric | ✅ Symmetric |
| Overflow behavior | Wraps (UB in C) | Produces ERR |
| Division by zero | UB/Exception | Produces ERR |
| Error propagation | Manual checks | Automatic (sticky) |
| Performance | Fast | Slightly slower (extra checks) |
| Safety | ❌ Prone to bugs | ✅ Safe by default |

### When to Use TBB

**Use TBB when:**
- Overflow is a **bug**, not a feature (e.g., financial calculations)
- You want automatic error propagation
- You need symmetric ranges for mathematical correctness
- Safety matters more than raw speed

**Use standard int when:**
- Wrapping behavior is intentional (e.g., hash functions)
- You need every last bit of performance
- You're working with hardware registers or bit manipulation

---

## LBIM (Large Binary Integer Mathematics)

### Fixed-Width Large Integers

Aria provides **fixed-width large integers** for applications requiring arbitrary precision:

| Type | Bits | Bytes | Alignment | Range |
|------|------|-------|-----------|-------|
| int128 | 128 | 16 | 16 | ±1.7×10³⁸ |
| int256 | 256 | 32 | 32 | ±5.8×10⁷⁶ |
| int512 | 512 | 64 | 64 | ±6.7×10¹⁵³ |
| int1024 | 1024 | 128 | 64 | ±8.9×10³⁰⁷ |
| int2048 | 2048 | 256 | 64 | ±1.6×10⁶¹⁶ |
| int4096 | 4096 | 512 | 64 | ±1.0×10¹²³³ |

### Example: Cryptographic Integer Arithmetic

```aria
// RSA-2048 key generation requires 2048-bit integers
int2048:p = generate_large_prime();
int2048:q = generate_large_prime();
int2048:n = p * q;  // Modulus for RSA

// Large integer operations work naturally
int2048:e = 65537;  // Public exponent
int2048:d = modular_inverse(e, (p - 1) * (q - 1));  // Private exponent
```

### Memory Layout

Large integers are stored as **little-endian arrays of 64-bit words**:

```aria
int256:x = ...;  // Stored as 4 × uint64 words in memory

// Memory layout (little-endian):
// [word0: bits 0-63]
// [word1: bits 64-127]
// [word2: bits 128-191]
// [word3: bits 192-255]
```

### Alignment is Critical

**IMPORTANT**: Large integers have strict alignment requirements:

```aria
// CORRECT: Properly aligned
int256:x = aria_alloc_buffer(32, 32, 1);  // 32-byte aligned

// WRONG: Unaligned allocation (CRASH on SIMD operations!)
int256:y = aria_alloc(32);  // Only 8-byte aligned on most platforms
```

### Operations

All standard arithmetic works:

```aria
int512:a = ...;
int512:b = ...;

int512:sum = a + b;
int512:diff = a - b;
int512:product = a * b;
int512:quotient = a / b;
int512:remainder = a % b;

// Comparisons
if (a > b) {
    // a is larger
} else {}

// Bitwise operations
int512:result = a & b;
int512:shifted = a << 10;
```

---

## Fixed-Point Types

### The Floating-Point Problem

IEEE 754 floating-point has issues:
- Rounding errors accumulate
- Equality comparisons are unreliable
- Not suitable for financial calculations
- Non-deterministic across platforms

### Fixed-Point Solution

Aria provides **true decimal fixed-point types**:

| Type | Bits | Bytes | Alignment | Integer Part | Fractional Part | Precision |
|------|------|-------|-----------|--------------|-----------------|-----------|
| fix256 | 256 | 32 | 32 | int128 | int128 | ~38 decimal digits |
| fix512 | 512 | 64 | 64 | int256 | int256 | ~77 decimal digits |
| fix1024 | 1024 | 128 | 64 | int512 | int512 | ~154 decimal digits |
| fix2048 | 2048 | 256 | 64 | int1024 | int1024 | ~308 decimal digits |
| fix4096 | 4096 | 512 | 64 | int2048 | int2048 | ~617 decimal digits |

### Memory Layout

Fixed-point types use **signed-magnitude representation**:

```
fix256 (32 bytes):
┌─────────────────────┬─────────────────────┐
│  Sign + Integer     │    Fractional       │
│     (16 bytes)      │    (16 bytes)       │
└─────────────────────┴─────────────────────┘
  Bit 255: Sign bit
  Bits 254-128: Integer part
  Bits 127-0: Fractional part
```

### Example: Financial Calculations

```aria
// Represent $1,234,567.89 exactly
fix256:amount = 1234567.89;

// Apply 5.25% interest rate
fix256:rate = 0.0525;
fix256:interest = amount * rate;  // Exact multiplication

// Total after interest
fix256:total = amount + interest;

// No rounding errors!
if (total == expected) {
    // This comparison is reliable
} else {}
```

### Conversion to/from Decimal

```aria
// From decimal string
fix256:price = parse_fix256("123.456789");

// To decimal string (with precision)
int8*:buf = aria_alloc_string(50);
format_fix256(buf, price, 6);  // Format with 6 decimal places
```

### Fixed-Point vs Floating-Point

| Feature | flt64 (IEEE 754) | fix256 |
|---------|------------------|--------|
| Precision | ~15 decimal digits | ~38 decimal digits |
| Rounding errors | ❌ Yes, accumulate | ✅ None (exact) |
| Equality comparison | ❌ Unreliable | ✅ Reliable |
| Financial calculations | ❌ Not recommended | ✅ Perfect |
| Scientific notation | ✅ Supports | ❌ Fixed range |
| Performance | Fast (HW support) | Slower (software) |

---

## Standard Types

### Integer Types

| Type | Size | Signed | Range |
|------|------|--------|-------|
| int8 | 1 byte | Yes | -128 to 127 |
| int16 | 2 bytes | Yes | -32,768 to 32,767 |
| int32 | 4 bytes | Yes | -2,147,483,648 to 2,147,483,647 |
| int64 | 8 bytes | Yes | -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 |

### Unsigned Integers

| Type | Size | Range |
|------|------|-------|
| uint8 | 1 byte | 0 to 255 |
| uint16 | 2 bytes | 0 to 65,535 |
| uint32 | 4 bytes | 0 to 4,294,967,295 |
| uint64 | 8 bytes | 0 to 18,446,744,073,709,551,615 |

### Floating-Point Types

| Type | Size | Precision | Range |
|------|------|-----------|-------|
| flt32 | 4 bytes | ~7 decimal digits | ±3.4×10³⁸ |
| flt64 | 8 bytes | ~15 decimal digits | ±1.7×10³⁰⁸ |

### Boolean Type

```aria
int8:flag = 1;  // True
int8:done = 0;  // False
```

**Note**: Aria uses `int8` for booleans (0 = false, non-zero = true).

---

## Type Conversions

### Explicit Conversions Only

Aria **does not perform implicit conversions**:

```aria
int32:x = 42;
int64:y = x;  // ERROR: No implicit widening!

// Must use explicit conversion:
int64:y = @i64(x);  // OK
```

### Widening Intrinsics

```aria
// Integer widening
int8:small = 127;
int16:medium = @i16(small);
int32:large = @i32(small);
int64:huge = @i64(small);

// Sign extension is automatic for signed types
int8:negative = -42;
int64:extended = @i64(negative);  // -42 (not 214!)
```

### TBB Widening

TBB types can be widened with sentinel propagation:

```aria
tbb8:small = ERR8;
tbb32:wide = @widen(small);  // wide = ERR32 (sentinel propagates!)

tbb16:val = 100;
tbb64:big = @widen(val);  // big = 100
```

**Supported TBB widenings:**
- tbb8 → tbb16, tbb32, tbb64
- tbb16 → tbb32, tbb64
- tbb32 → tbb64

### Narrowing Conversions

Narrowing requires explicit intrinsics and can truncate:

```aria
int64:big = 1000000;
int32:small = @i32(big);  // small = 1000000 (fits)

int64:huge = 9999999999;
int32:overflow = @i32(huge);  // Truncates high bits!
```

**Warning**: Narrowing conversions do not check for overflow. Use TBB types if you need overflow detection.

### Floating-Point Conversions

```aria
flt64:pi = 3.14159;
int32:rounded = @i32(pi);  // rounded = 3 (truncates toward zero)

int32:x = 42;
flt64:float_x = @f64(x);  // float_x = 42.0
```

---

## Alignment Requirements

### Why Alignment Matters

Modern CPUs require data to be **aligned** to specific byte boundaries for optimal (or even correct) access:

```
// Aligned int32 (4-byte boundary):
Address:  0x1000  0x1001  0x1002  0x1003
Data:    [  int32 value (4 bytes)      ]
         ✅ Single memory access

// Misaligned int32:
Address:  0x1001  0x1002  0x1003  0x1004
Data:     [  int32 value (4 bytes)   ]
          ❌ Requires TWO memory accesses (slow!)
          ❌ May cause hardware exception on some platforms!
```

### Alignment Rules

| Type | Required Alignment | Reason |
|------|-------------------|--------|
| int8, uint8 | 1 byte | No alignment needed |
| int16, uint16 | 2 bytes | Half-word access |
| int32, uint32, flt32 | 4 bytes | Word access |
| int64, uint64, flt64 | 8 bytes | Double-word access |
| int128 | 16 bytes | SIMD register |
| int256, fix256 | 32 bytes | AVX-256 register |
| int512, fix512 | 64 bytes | AVX-512 ZMM register |
| int1024, int2048, int4096 | 64 bytes | Cache line |

### Allocating Aligned Memory

```aria
// For fix256 (requires 32-byte alignment):
void*:ptr = aria_alloc_buffer(32, 32, 1);
fix256*:value = ptr;

// For int512 (requires 64-byte alignment):
void*:ptr = aria_alloc_buffer(64, 64, 0);
int512*:number = ptr;
```

**Critical**: Using `aria_alloc` for large types will likely cause **misalignment crashes**.

---

## Memory Layout

### Structure Packing

Aria structures are **not automatically packed** - alignment rules apply:

```aria
struct Person {
    int8:age;        // 1 byte
    // 7 bytes padding here!
    int64:id;        // 8 bytes (requires 8-byte alignment)
    int32:score;     // 4 bytes
    // 4 bytes padding here!
}
// Total: 24 bytes (not 13!)
```

### Array Layout

Arrays are **contiguous and densely packed**:

```aria
int32*:arr = aria_alloc_array(4, 10);  // 10 × 4 bytes = 40 bytes

// Memory layout:
// [arr[0]][arr[1]][arr[2]]...[arr[9]]
// No padding between elements
```

### Pointer Sizes

All pointers are **8 bytes** on 64-bit platforms:

```aria
int8*:ptr1 = ...;   // 8 bytes
int256*:ptr2 = ...; // 8 bytes (pointer itself, not the data!)
void*:ptr3 = ...;   // 8 bytes
```

---

## Best Practices

### 1. Choose the Right Type for the Job

```aria
// Financial calculation? Use fix256
fix256:price = 99.99;

// Loop counter? Use int32
int32:i = 0;

// Need overflow detection? Use tbb32
tbb32:balance = account.balance;

// Cryptographic integers? Use int2048
int2048:prime = generate_prime();
```

### 2. Always Check TBB for Errors

```aria
tbb32:result = long_calculation();

if (@is_err_tbb32(result)) {
    // Handle error appropriately
    print_newline();
} else {
    // Use result
}
```

### 3. Use Aligned Allocation for Large Types

```aria
// WRONG:
fix256*:x = aria_alloc(32);  // Misaligned!

// RIGHT:
fix256*:x = aria_alloc_buffer(32, 32, 1);  // Properly aligned
```

### 4. Prefer Explicit Over Implicit

```aria
// WRONG (if type inference existed):
auto x = getValue();  // What type is x?

// RIGHT:
int32:x = getValue();  // x is clearly int32
```

### 5. Document Large Integer Usage

```aria
// RSA-2048 modulus (product of two 1024-bit primes)
int2048:n = p * q;

// Not just "int2048:n = p * q;" - explain WHY you need 2048 bits!
```

### 6. Use sizeof and alignof

```aria
int64:size = @sizeof(fix256);    // 32 bytes
int64:align = @alignof(fix256);  // 32 bytes

// Allocate with correct size and alignment
void*:ptr = aria_alloc_buffer(@sizeof(fix512), @alignof(fix512), 1);
```

---

## Summary Table

| Type Category | Types | Use Case | Key Feature |
|---------------|-------|----------|-------------|
| **TBB** | tbb8, tbb16, tbb32, tbb64 | Safe arithmetic | Sticky error propagation |
| **LBIM** | int128-int4096 | Cryptography, big integers | Arbitrary precision |
| **Fixed-Point** | fix256-fix4096 | Financial, exact decimals | No rounding errors |
| **Standard Int** | int8-int64 | General purpose | Fast, hardware support |
| **Unsigned** | uint8-uint64 | Bit manipulation, addresses | No negatives |
| **Floating** | flt32, flt64 | Scientific computing | Wide range, imprecise |

---

## See Also

- [Intrinsics Reference](../stdlib/INTRINSICS.md) - TBB intrinsics, widening operations
- [Stdlib API](../stdlib/API.md) - High-level wrappers for type operations
- [FFI Guide](FFI.md) - Passing types to C libraries
- [Safety Guidelines](SAFETY.md) - Production best practices

---

**Maintained by**: Aria Compiler Team  
**License**: MIT  
**Contact**: https://ai-liberation-platform.org

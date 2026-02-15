# Standard Small Signed Integers (int8, int16)

**Category**: Types → Integers → Standard Signed  
**Sizes**: 8-bit, 16-bit  
**Ranges**: int8 (-128 to +127), int16 (-32,768 to +32,767)  
**Purpose**: Traditional two's complement signed integers with wrapping overflow  
**Status**: ✅ FULLY IMPLEMENTED

---

## Table of Contents

1. [Overview](#overview)
2. [int8: 8-bit Signed Integer](#int8-8-bit-signed-integer)
3. [int16: 16-bit Signed Integer](#int16-16-bit-signed-integer)
4. [vs TBB Types (Critical Differences)](#vs-tbb-types-critical-differences)
5. [When to Use Standard Ints vs TBB](#when-to-use-standard-ints-vs-tbb)
6. [Overflow Wrapping Behavior](#overflow-wrapping-behavior)
7. [abs() Undefined Behavior with Minimum Values](#abs-undefined-behavior-with-minimum-values)
8. [Two's Complement Representation](#twos-complement-representation)
9. [Arithmetic Operations](#arithmetic-operations)
10. [Bit Manipulation](#bit-manipulation)
11. [C Interoperability](#c-interoperability)
12. [Performance Characteristics](#performance-characteristics)
13. [Common Patterns](#common-patterns)
14. [Anti-Patterns](#anti-patterns)
15. [Migration from C/C++](#migration-from-cc)
16. [Related Concepts](#related-concepts)

---

## Overview

Aria's **standard signed integers** (`int8`, `int16`, `int32`, `int64`) are traditional two's complement integers with **asymmetric ranges** and **wrapping overflow behavior**.

**Key Properties**:
- **Asymmetric ranges**: Minimum is -(2^(n-1)), maximum is 2^(n-1) - 1
- **Wrapping overflow**: Overflow/underflow wraps around (defined behavior in Aria, UB in C!)
- **Two's complement**: Industry-standard representation (compatible with all modern CPUs)
- **No error propagation**: No ERR sentinel like TBB types
- **Fast**: Zero-overhead native machine integers

**⚠️ CRITICAL**: Standard ints wrap on overflow (127 + 1 = -128). Use TBB types if you need overflow detection!

---

## int8: 8-bit Signed Integer

### Range

**Minimum**: -128 (0x80, -2^7)  
**Maximum**: +127 (0x7F, 2^7 - 1)  
**Total values**: 256 (2^8)

**Asymmetric**: One more negative value than positive (abs(-128) has no representation!)

### Declaration

```aria
int8:small = 42;
int8:negative = -100;
int8:min_value = -128;
int8:max_value = 127;
```

### Literal Suffix

```aria
int8:byte_val = 50i8;  // Explicit int8 literal
```

**⚠️ CRITICAL**: Aria requires **explicit type suffixes** for all numeric literals (Zero Implicit Conversion Policy).

### Common Use Cases

1. **Byte-oriented data**: Protocol headers, binary file formats
2. **Small counters**: Loop indices for small ranges
3. **Flags/enums**: When range -128 to +127 sufficient
4. **Memory-constrained**: Embedded systems, large arrays needing tight packing
5. **C interop**: Matching C's `int8_t`, `signed char` types

### Example: Protocol Header

```aria
struct:PacketHeader = {
    int8:version;        // Protocol version (-128 to +127, but typically 0-10)
    int8:packet_type;    // Packet type ID
    int8:flags;          // Bit flags
    int8:reserved;       // Reserved for future use
};

PacketHeader:header = {
    version: 2i8,
    packet_type: 5i8,
    flags: 0x0Fi8,       // 4 bits set
    reserved: 0i8
};
```

---

## int16: 16-bit Signed Integer

### Range

**Minimum**: -32,768 (0x8000, -2^15)  
**Maximum**: +32,767 (0x7FFF, 2^15 - 1)  
**Total values**: 65,536 (2^16)

**Asymmetric**: One more negative value than positive (abs(-32768) has no representation!)

### Declaration

```aria
int16:sample = 1234;
int16:audio_pcm = -15000;  // 16-bit PCM audio sample
int16:min_value = -32768;
int16:max_value = 32767;
```

### Literal Suffix

```aria
int16:value = 10000i16;  // Explicit int16 literal
```

### Common Use Cases

1. **Audio processing**: 16-bit PCM standard (CD quality: -32768 to +32767)
2. **Sensor data**: Temperature sensors, accelerometers (0.01° precision)
3. **Small database keys**: Row IDs for small tables (up to 32,767 records)
4. **Graphics coordinates**: Screen positions for resolutions up to 32K
5. **Unicode BMP**: Basic Multilingual Plane character codes (0 to 65535)
6. **C interop**: Matching C's `int16_t`, `short` types

### Example: Audio Processing

```aria
// 16-bit PCM audio buffer (CD quality)
int16[]:audio_buffer = aria.alloc<int16>(44100);  // 1 second at 44.1kHz

// Read audio samples
till 44100 loop:i
    int16:sample = read_audio_sample();
    
    // Apply gain (watch for clipping!)
    int32:amplified = int32(sample) * 2;  // Widen to avoid overflow
    
    if (amplified > 32767) {
        audio_buffer[i] = 32767i16;  // Clip to max
    } else if (amplified < -32768) {
        audio_buffer[i] = -32768i16;  // Clip to min
    } else {
        audio_buffer[i] = int16(amplified);
    }
end
```

---

## vs TBB Types (Critical Differences)

| Feature | Standard int8/int16 | TBB tbb8/tbb16 |
|---------|---------------------|----------------|
| **Range** | Asymmetric (int8: -128 to +127) | Symmetric (tbb8: -127 to +127) |
| **Overflow** | Wraps around (127 + 1 = -128) | Returns ERR sentinel |
| **Underflow** | Wraps around (-128 - 1 = 127) | Returns ERR sentinel |
| **Division by zero** | Undefined behavior (panic) | Returns ERR sentinel |
| **abs(MIN)** | Undefined behavior (panic) | Returns ERR sentinel |
| **Error checking** | Manual (check before operation) | Automatic (sticky ERR) |
| **Performance** | Zero overhead | ~0-20% overhead |
| **C interop** | Direct (same representation) | Requires conversion |
| **Use case** | Performance, bit ops, C FFI | Safety-critical, error propagation |

### Example: Overflow Difference

```aria
// Standard int8 - WRAPS
int8:std = 127i8;
std += 1i8;
// Result: -128 (wrapped around!)

// TBB tbb8 - RETURNS ERR
tbb8:safe = 127;
safe += 1;
// Result: ERR (-128 sentinel value)

if (safe == ERR) {
    stderr.write("Overflow detected!\n");
}
```

**Key insight**: Standard ints wrap silently. TBB types propagate errors. Choose based on your domain!

---

## When to Use Standard Ints vs TBB

### Use Standard Ints (int8/int16) When:

1. **Performance critical**: Zero overhead, native machine instructions
2. **C interoperability**: Matching C types exactly (FFI, binary protocols)
3. **Bit manipulation**: Wrapping is desired behavior (cryptography, hashing)
4. **Modular arithmetic**: Intentional wraparound (ring buffers, counters)
5. **Large arrays**: Memory efficiency (millions of elements, every byte counts)
6. **Overflow impossible**: Range constraints guarantee no overflow
7. **Manual checking**: You handle overflow explicitly

### Use TBB Types (tbb8/tbb16) When:

1. **Safety-critical**: Healthcare, finance, control systems (overflow = disaster)
2. **Overflow detection**: Need to know when arithmetic fails
3. **Error propagation**: Sticky errors through computation chains
4. **Symmetric ranges**: abs(-N) must always work
5. **Therapy data**: Nikola consciousness, emotional state quantization
6. **Sensor fusion**: Multiple sensors, any can fail (ERR = malfunction)
7. **Accumulation**: Long sums where overflow risk exists

### Quick Decision Matrix

```aria
// Financial calculation - USE TBB (overflow = catastrophe)
tbb16:account_balance = 1000;
account_balance += transaction_amount;
if (account_balance == ERR) {
    log_error("Transaction overflow!");
    rollback_transaction();
}

// Ring buffer index - USE STANDARD (wrapping intentional)
int16:ring_index = 0i16;
ring_index = (ring_index + 1i16) % 1024i16;  // Modular arithmetic

// Audio sample - USE STANDARD (clipping, not overflow detection)
int16:audio = read_pcm16();
audio = clip(audio, -32768i16, 32767i16);

// Therapy engagement score - USE TBB (patient safety, detect errors)
tbb8:engagement = compute_engagement_score();
if (engagement == ERR) {
    alert_clinician("Score computation failed!");
}
```

---

## Overflow Wrapping Behavior

### Defined in Aria (NOT Undefined Behavior!)

Unlike C/C++ where signed overflow is undefined behavior, Aria **defines** overflow as two's complement wrapping.

```aria
// Addition overflow
int8:a = 127i8;
int8:b = 1i8;
int8:c = a + b;  // c = -128 (wraps to minimum)

// Subtraction underflow
int8:d = -128i8;
int8:e = 1i8;
int8:f = d - e;  // f = 127 (wraps to maximum)

// Multiplication overflow
int8:g = 16i8;
int8:h = 8i8;
int8:i = g * h;  // i = -128 (128 wraps to -128)
```

**Why defined?**: Enables bit manipulation, modular arithmetic, cryptography without compiler optimization breaking code.

### Wrapping Examples

```aria
// Incrementing past maximum
int8:counter = 127i8;
counter += 1i8;  // counter = -128
counter += 1i8;  // counter = -127
counter += 1i8;  // counter = -126

// Decrementing past minimum
int8:countdown = -128i8;
countdown -= 1i8;  // countdown = 127
countdown -= 1i8;  // countdown = 126

// Wrapping arithmetic (intentional)
int16:angle = 359i16;  // Degrees
angle = (angle + 5i16) % 360i16;  // Wraps to 4° (modular)
```

**⚠️ WARNING**: Wrapping is **silent**! If overflow is an error, use TBB types instead.

---

## abs() Undefined Behavior with Minimum Values

### The abs(INT_MIN) Problem

Standard signed integers have **one more negative value than positive**. Taking the absolute value of the minimum value **has no representation**!

```aria
int8:min = -128i8;
int8:result = abs(min);  // ⚠️ UNDEFINED BEHAVIOR! Panics in Aria

// Why? abs(-128) = 128, but int8 max is 127!
```

**Aria behavior**: Calling `abs()` on INT_MIN **panics** (program termination with error message).

**C behavior**: Undefined behavior (might return -128, might wrap, might crash, compiler can do ANYTHING).

### Safe Alternatives

**Option 1**: Use TBB types (abs works correctly)

```aria
tbb8:value = -127;  // tbb8 min is -127, not -128
tbb8:result = abs(value);  // result = 127 (works!)

tbb8:min = -127;
tbb8:abs_min = abs(min);  // abs_min = 127 (no problem)
```

**Option 2**: Check before calling abs()

```aria
int8:value = -128i8;

if (value == -128i8) {
    // Handle special case
    stderr.write("Cannot take abs of INT8_MIN\n");
} else {
    int8:result = abs(value);  // Safe (not minimum)
}
```

**Option 3**: Widen to larger type

```aria
int8:small = -128i8;
int16:wide = int16(small);  // -128 fits in int16
int16:result = abs(wide);   // result = 128 (fits in int16)
```

### Why TBB Fixes This

TBB types have **symmetric ranges**, so abs() always works:

| Type | Range | abs(MIN) |
|------|-------|----------|
| int8 | -128 to +127 | **UNDEFINED** (128 doesn't fit) |
| tbb8 | -127 to +127 | ✅ 127 (fits!) |
| int16 | -32768 to +32767 | **UNDEFINED** (32768 doesn't fit) |
| tbb16 | -32767 to +32767 | ✅ 32767 (fits!) |

---

## Two's Complement Representation

### Bit Representation

**int8** (8 bits):
```
+127 = 0x7F = 0b01111111
+1   = 0x01 = 0b00000001
 0   = 0x00 = 0b00000000
-1   = 0xFF = 0b11111111
-128 = 0x80 = 0b10000000
```

**int16** (16 bits):
```
+32767 = 0x7FFF = 0b0111111111111111
+1     = 0x0001 = 0b0000000000000001
 0     = 0x0000 = 0b0000000000000000
-1     = 0xFFFF = 0b1111111111111111
-32768 = 0x8000 = 0b1000000000000000
```

### Sign Bit

**Most significant bit (MSB)** determines sign:
- `0` = positive or zero
- `1` = negative

```aria
int8:positive = 42i8;    // 0b00101010 (MSB = 0)
int8:negative = -42i8;   // 0b11010110 (MSB = 1)
```

### Computing Negative Values

To negate a value in two's complement:
1. Flip all bits (bitwise NOT)
2. Add 1

```aria
int8:value = 5i8;       // 0b00000101

// Negate manually:
int8:inverted = ~value; // 0b11111010 (bitwise NOT)
int8:negated = inverted + 1i8;  // 0b11111011 = -5

// Or just:
int8:neg_value = -value;  // -5 (compiler does the above)
```

### Why One More Negative?

```
Positive int8: 0 to 127 (128 values: 0b00000000 to 0b01111111)
Negative int8: -1 to -128 (128 values: 0b11111111 to 0b10000000)
```

Zero uses one of the "positive" bit patterns, leaving 127 positive values but 128 negative patterns!

---

## Arithmetic Operations

### Addition/Subtraction with Wrapping

```aria
int8:a = 100i8;
int8:b = 50i8;
int8:sum = a + b;  // 150... but wraps to -106!

// Why? 150 = 0b10010110 (MSB=1, interpreted as negative)
```

**Overflow detection** (manual):

```aria
func:safe_add_i8 = (a: int8, b: int8) -> Result<int8, string> {
    // Overflow if: both positive and sum negative
    if (a > 0i8 && b > 0i8 && (a + b) < 0i8) {
        fail("Positive overflow");
    }
    
    // Underflow if: both negative and sum positive
    if (a < 0i8 && b < 0i8 && (a + b) > 0i8) {
        fail("Negative underflow");
    }
    
    pass(a + b);
}
```

**Or just use TBB** (automatic):

```aria
tbb8:a = 100;
tbb8:b = 50;
tbb8:sum = a + b;  // ERR (overflow detected automatically)

if (sum == ERR) {
    stderr.write("Addition overflow!\n");
}
```

### Multiplication with Wrapping

```aria
int8:a = 16i8;
int8:b = 8i8;
int8:product = a * b;  // 128... wraps to -128!

int16:a16 = 16i16;
int16:b16 = 8i16;
int16:product16 = a16 * b16;  // 128 (fits in int16, no wrap)
```

**Safe multiplication** (widen, then narrow):

```aria
int8:a = 16i8;
int8:b = 8i8;

int16:wide_a = int16(a);
int16:wide_b = int16(b);
int16:wide_product = wide_a * wide_b;  // 128 (no overflow in int16)

if (wide_product > 127i16 || wide_product < -128i16) {
    stderr.write("Product doesn't fit in int8!\n");
} else {
    int8:result = int8(wide_product);  // Safe narrow
}
```

### Division

```aria
int8:a = 100i8;
int8:b = 3i8;
int8:quotient = a / b;   // 33 (truncates toward zero)
int8:remainder = a % b;  // 1

int8:neg_a = -100i8;
int8:neg_quotient = neg_a / b;  // -33 (truncates toward zero)
int8:neg_remainder = neg_a % b; // -1 (sign matches dividend)
```

**Division by zero**: Panics (undefined behavior, program terminates).

**With TBB**: Returns ERR instead.

---

## Bit Manipulation

Standard ints excel at bit manipulation (wrapping is often desired).

### Bitwise Operators

```aria
int8:a = 0b00110011i8;
int8:b = 0b01010101i8;

int8:and_result = a & b;   // 0b00010001 (AND)
int8:or_result = a | b;    // 0b01110111 (OR)
int8:xor_result = a ^ b;   // 0b01100110 (XOR)
int8:not_result = ~a;      // 0b11001100 (NOT)
```

### Bit Shifts

```aria
int8:value = 0b00000001i8;  // 1

int8:left_shift = value << 1i8;   // 0b00000010 (2)
int8:left_shift2 = value << 2i8;  // 0b00000100 (4)
int8:left_shift7 = value << 7i8;  // 0b10000000 (-128)

// Right shift (arithmetic - sign extends)
int8:neg_value = -64i8;  // 0b11000000
int8:right_shift = neg_value >> 1i8;  // 0b11100000 (-32, sign extended)
```

**Arithmetic right shift**: Sign bit propagates (extends sign).

**Overflow via shift**: Left shift can overflow into sign bit (wraps).

### Bit Flags

```aria
// Protocol flags (8 bits)
const:int8:FLAG_ACK = 0x01i8;     // 0b00000001
const:int8:FLAG_SYN = 0x02i8;     // 0b00000010
const:int8:FLAG_FIN = 0x04i8;     // 0b00000100
const:int8:FLAG_RST = 0x08i8;     // 0b00001000

int8:flags = 0i8;

// Set flags
flags |= FLAG_ACK;  // Set ACK
flags |= FLAG_SYN;  // Set SYN

// Check flags
if ((flags & FLAG_ACK) != 0i8) {
    log.write("ACK flag set\n");
}

// Clear flags
flags &= ~FLAG_SYN;  // Clear SYN (AND with inverted mask)

// Toggle flags
flags ^= FLAG_FIN;  // Toggle FIN
```

---

## C Interoperability

### Direct Compatibility

Aria's standard ints match C's fixed-width types:

| Aria Type | C Type | Size | Range |
|-----------|--------|------|-------|
| int8 | `int8_t` | 1 byte | -128 to 127 |
| int16 | `int16_t` | 2 bytes | -32768 to 32767 |

### FFI Example

```aria
// C library function
extern "C" {
    func:c_audio_process = int16(
        samples: int16@,  // Pointer to int16 array
        count: int32      // Number of samples
    );
}

// Aria usage
int16[]:audio_buffer = aria.alloc<int16>(1024);

till 1024 loop:i
    audio_buffer[i] = read_audio_sample();
end

int16:result = c_audio_process(audio_buffer, 1024i32);

if (result < 0i16) {
    stderr.write("Audio processing failed\n");
}
```

**No conversion needed**: Aria int8/int16 are ABI-compatible with C.

---

## Performance Characteristics

### Zero Overhead

Standard ints compile to native machine instructions with **no overhead**:

```aria
int8:a = 10i8;
int8:b = 20i8;
int8:sum = a + b;
```

**Generated assembly** (x86):
```asm
mov al, 10      ; Load 10 into AL register
add al, 20      ; Add 20 to AL
; Result in AL (one instruction!)
```

**Performance**: Identical to hand-written assembly.

### Memory Efficiency

```aria
// Array of 1 million int8
int8[]:tiny = aria.alloc<int8>(1000000);
// Memory: 1 MB

// Array of 1 million int16
int16[]:small = aria.alloc<int16>(1000000);
// Memory: 2 MB

// vs int32
int32[]:standard = aria.alloc<int32>(1000000);
// Memory: 4 MB (2-4× larger!)
```

**Cache efficiency**: Smaller types = more data per cache line = better performance for large arrays.

---

## Common Patterns

### Range-Checked Narrowing

```aria
func:narrow_to_i8 = (value: int32) -> ?int8 {
    if (value < -128i32 || value > 127i32) {
        return NIL;  // Out of range
    }
    
    return int8(value);  // Safe cast
}

// Usage
int32:large = 1000i32;
?int8:small = narrow_to_i8(large);

if (small == NIL) {
    stderr.write("Value too large for int8\n");
}
```

### Saturating Arithmetic (Clipping)

```aria
func:saturating_add_i8 = (a: int8, b: int8) -> int8 {
    int16:wide_sum = int16(a) + int16(b);  // Widen to avoid wrap
    
    if (wide_sum > 127i16) {
        return 127i8;  // Saturate at max
    } else if (wide_sum < -128i16) {
        return -128i8;  // Saturate at min
    } else {
        return int8(wide_sum);
    }
}

// Audio clipping example
int8:sample = 100i8;
int8:gain = 50i8;
int8:amplified = saturating_add_i8(sample, gain);  // 127 (saturated)
```

### Modular Arithmetic (Ring Buffers)

```aria
struct:RingBuffer = {
    int16[1024]:data;
    int16:write_index;
    int16:read_index;
};

func:ring_write = (rb: RingBuffer@, value: int16) -> void {
    rb.data[rb.write_index] = value;
    rb.write_index = (rb.write_index + 1i16) % 1024i16;  // Wrap around
}

func:ring_read = (rb: RingBuffer@) -> int16 {
    int16:value = rb.data[rb.read_index];
    rb.read_index = (rb.read_index + 1i16) % 1024i16;  // Wrap around
    pass(value);
}
```

---

## Anti-Patterns

### ❌ Ignoring Overflow Risk

```aria
// WRONG: Accumulation without overflow check
int8:sum = 0i8;

till 100 loop:i
    sum += 50i8;  // ❌ Overflows silently!
end
// sum is garbage (wrapped multiple times)

// RIGHT: Use larger type or TBB
tbb16:safe_sum = 0;

till 100 loop:i
    safe_sum += 50;
    
    if (safe_sum == ERR) {
        stderr.write("Overflow detected!\n");
        break;
    }
end
```

### ❌ Using abs(INT_MIN)

```aria
// WRONG: abs() on minimum value
int8:value = -128i8;
int8:absolute = abs(value);  // ❌ PANICS! No representation for 128

// RIGHT: Use TBB or check first
tbb8:safe_value = -127;
tbb8:safe_abs = abs(safe_value);  // ✅ Works (tbb8 min is -127)

// OR check before abs()
if (value == -128i8) {
    stderr.write("Cannot take abs of INT8_MIN\n");
} else {
    int8:absolute = abs(value);  // Safe
}
```

### ❌ Mixing Signed/Unsigned Without Care

```aria
// WRONG: Comparing signed to unsigned
int8:signed_val = -1i8;
uint8:unsigned_val = 255u8;

if (signed_val < unsigned_val) {  // ⚠️ Implicit conversion!
    // -1 converts to 255 (bit pattern reinterpreted)
    // Comparison might not do what you expect!
}

// RIGHT: Explicit widening to signed type
int16:wide_signed = int16(signed_val);
int16:wide_unsigned = int16(unsigned_val);

if (wide_signed < wide_unsigned) {
    // Clear semantics
}
```

---

## Migration from C/C++

### Undefined Behavior → Defined in Aria

**C**: Signed overflow is undefined behavior
```c
int8_t a = 127;
a += 1;  // UNDEFINED BEHAVIOR (compiler can do anything!)
```

**Aria**: Signed overflow wraps (defined behavior)
```aria
int8:a = 127i8;
a += 1i8;  // a = -128 (defined as two's complement wrap)
```

**Benefit**: Predictable behavior, enables modular arithmetic and bit manipulation.

### Explicit Type Suffixes Required

**C**: Implicit integer literal types
```c
int8_t x = 42;  // Literal is int (32-bit), implicitly converted
```

**Aria**: Explicit suffix required
```aria
int8:x = 42i8;  // Must specify i8 suffix (Zero Implicit Conversion)
```

### abs() Behavior

**C**: abs(INT_MIN) is undefined behavior
```c
int8_t min = -128;
int8_t a = abs(min);  // UNDEFINED BEHAVIOR!
```

**Aria**: abs(INT_MIN) panics (caught at runtime)
```aria
int8:min = -128i8;
int8:a = abs(min);  // Panics with error message
```

**Better in Aria**: Use TBB which has symmetric range (abs always works).

---

## Related Concepts

### Other Integer Types

- **int32, int64**: Larger standard signed integers
- **uint8, uint16**: Unsigned integers (0 to 255/65535)
- **tbb8, tbb16**: Symmetric-range error-propagating integers
- **int1024, int2048, int4096**: Large cryptographic integers

### Special Values

- **NIL**: Optional types (not applicable to int8/int16)
- **ERR**: TBB error sentinel (not applicable to standard ints)
- **NULL**: Pointer sentinel (not applicable to value types)

### Related Operations

- **Bit manipulation**: AND, OR, XOR, NOT, shifts
- **Saturating arithmetic**: Clipping instead of wrapping
- **Widening conversions**: int8 → int16 → int32 (safe)
- **Narrowing conversions**: int32 → int16 → int8 (check range!)

---

## Summary

**int8** and **int16** are Aria's small standard signed integers:

✅ **Asymmetric ranges**: int8 (-128 to +127), int16 (-32,768 to +32,767)  
✅ **Wrapping overflow**: Defined behavior (not UB like C!)  
✅ **Zero overhead**: Native machine instructions  
✅ **C compatible**: Direct FFI, same ABI  
✅ **Bit manipulation**: Ideal for flags, masks, protocols  

⚠️ **Overflow wraps silently**: Use TBB if overflow is an error  
⚠️ **abs(INT_MIN) panics**: Use TBB for symmetric ranges  
⚠️ **Manual error checking**: No automatic ERR propagation  

**Use int8/int16 for**:
- Performance-critical code (zero overhead)
- C interoperability (FFI, binary protocols)
- Bit manipulation (flags, cryptography, hashing)
- Modular arithmetic (ring buffers, counters)
- Memory efficiency (large arrays)

**Use tbb8/tbb16 for**:
- Safety-critical systems (overflow detection)
- Error propagation (sticky errors)
- Symmetric ranges (abs always works)
- Therapy data (Nikola consciousness)
- Sensor fusion (detect malfunctions)

---

**Next**: [int32, int64 (Standard Medium/Large Signed Integers)](int32_int64.md)  
**See Also**: [tbb8](tbb8.md), [tbb16](tbb16.md), [uint8](uint8.md), [uint16](uint16.md)

---

*Aria Language Project - Traditional Integers with Defined Wrapping*  
*February 12, 2026 - Establishing timestamped prior art on standard integer semantics*

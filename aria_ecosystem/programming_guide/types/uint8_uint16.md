# Unsigned Small Integers (uint8, uint16)

**Category**: Types → Integers → Unsigned  
**Sizes**: 8-bit, 16-bit  
**Ranges**: uint8 (0 to 255), uint16 (0 to 65,535)  
**Purpose**: Traditional unsigned integers for non-negative values, bit manipulation  
**Status**: ✅ FULLY IMPLEMENTED

---

## Table of Contents

1. [Overview](#overview)
2. [uint8: 8-bit Unsigned Integer](#uint8-8-bit-unsigned-integer)
3. [uint16: 16-bit Unsigned Integer](#uint16-16-bit-unsigned-integer)
4. [vs Signed Integers (Critical Differences)](#vs-signed-integers-critical-differences)
5. [When to Use Unsigned vs Signed](#when-to-use-unsigned-vs-signed)
6. [Overflow Wrapping Behavior](#overflow-wrapping-behavior)
7. [Underflow Wrapping Behavior](#underflow-wrapping-behavior)
8. [Binary Representation](#binary-representation)
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

Aria's **unsigned integers** (`uint8`, `uint16`, `uint32`, `uint64`) represent **non-negative values only** with wrapping modular arithmetic.

**Key Properties**:
- **Non-negative only**: Range starts at 0 (no negative values)
- **Wrapping overflow**: Overflow wraps to 0 (modular arithmetic)
- **Wrapping underflow**: Underflow wraps to MAX (0 - 1 = MAX)
- **Double the positive range**: uint8 goes to 255 vs int8's 127
- **Binary alignment**: Natural for bit manipulation, memory addresses, sizes
- **Fast**: Zero-overhead native machine instructions

**⚠️ CRITICAL**: Underflow wraps (0 - 1 = 255 for uint8). This catches many beginners!

---

## uint8: 8-bit Unsigned Integer

### Range

**Minimum**: 0 (0x00)  
**Maximum**: 255 (0xFF, 2^8 - 1)  
**Total values**: 256 (2^8)

**Exactly double** the positive range of int8 (which goes to +127).

### Declaration

```aria
uint8:byte = 42u8;
uint8:max_value = 255u8;
uint8:min_value = 0u8;
```

### Literal Suffix

```aria
uint8:value = 200u8;  // Explicit uint8 literal (u8 suffix)
```

**⚠️ CRITICAL**: Aria requires **explicit type suffixes** for all numeric literals.

### Common Use Cases

1. **Byte data**: Raw bytes, binary protocols, file I/O
2. **RGB colors**: Red, green, blue channels (0-255 each)
3. **ASCII characters**: Character codes (0-127), extended ASCII (128-255)
4. **Percentages**: 0-100% scaled to 0-255
5. **Pixel values**: Grayscale images (0=black, 255=white)
6. **Network packets**: Protocol headers, checksums
7. **Memory-constrained**: Embedded systems, large arrays

### Example: RGB Color

```aria
struct:Color = {
    uint8:red;
    uint8:green;
    uint8:blue;
};

Color:hot_pink = {
    red: 255u8,    // Full red
    green: 105u8,  // Some green
    blue: 180u8    // Lots of blue
};

// Brighten color (saturating addition)
func:brighten = (color: Color@, amount: uint8) -> void {
    // Saturate at 255 (prevent wrapping)
    uint16:new_red = uint16(color.red) + uint16(amount);
    color.red = new_red > 255u16 ? 255u8 : uint8(new_red);
    
    uint16:new_green = uint16(color.green) + uint16(amount);
    color.green = new_green > 255u16 ? 255u8 : uint8(new_green);
    
    uint16:new_blue = uint16(color.blue) + uint16(amount);
    color.blue = new_blue > 255u16 ? 255u8 : uint8(new_blue);
}
```

---

## uint16: 16-bit Unsigned Integer

### Range

**Minimum**: 0 (0x0000)  
**Maximum**: 65,535 (0xFFFF, 2^16 - 1)  
**Total values**: 65,536 (2^16)

**Exactly double** the positive range of int16 (which goes to +32,767).

### Declaration

```aria
uint16:port = 8080u16;
uint16:pixel_count = 65535u16;
uint16:max_value = 65535u16;
uint16:min_value = 0u16;
```

### Literal Suffix

```aria
uint16:value = 50000u16;  // Explicit uint16 literal (u16 suffix)
```

### Common Use Cases

1. **Network ports**: TCP/UDP ports (0-65535)
2. **Unicode BMP**: Basic Multilingual Plane character codes
3. **Image dimensions**: Width/height up to 65,535 pixels
4. **Array sizes**: Small arrays (up to 65K elements)
5. **Checksums**: CRC-16, Fletcher-16
6. **Sensor IDs**: Device identifiers (up to 65K devices)
7. **Industry standard**: MIDI note numbers, DNS record types

### Example: Network Socket

```aria
struct:SocketAddress = {
    uint8[4]:ip_address;  // IPv4: 192.168.1.1
    uint16:port;          // Port: 0-65535
};

SocketAddress:web_server = {
    ip_address: [192u8, 168u8, 1u8, 1u8],
    port: 443u16  // HTTPS
};

// Validate port range (though all uint16 values valid for ports)
func:is_privileged_port = (port: uint16) -> bool {
    return port < 1024u16;  // Ports 0-1023 are privileged
}

if (is_privileged_port(web_server.port)) {
    log.write("Privileged port, requires root\n");
}
```

---

## vs Signed Integers (Critical Differences)

| Feature | Unsigned uint8/uint16 | Signed int8/int16 |
|---------|----------------------|-------------------|
| **Range** | uint8: 0 to 255 | int8: -128 to +127 |
| **Negative values** | NONE (0 is minimum) | Yes (negative numbers) |
| **Overflow** | Wraps to 0 | Wraps to negative MIN |
| **Underflow** | Wraps to MAX (0 - 1 = 255) | Wraps to positive MAX |
| **Bit pattern** | All 256 patterns non-negative | MSB is sign bit |
| **Comparison** | Simple (all positive) | Must handle negatives |
| **Use case** | Counts, sizes, bytes, colors | Offsets, differences, signed


 data |

### Example: Underflow Difference

```aria
// Unsigned uint8 - WRAPS TO MAX
uint8:u = 0u8;
u -= 1u8;
// u = 255 (wrapped to maximum!) ⚠️ DANGER!

// Signed int8 - WRAPS TO MAX (positive)
int8:s = -128i8;
s -= 1i8;
// s = 127 (wrapped from MIN to MAX)
```

**Key insight**: Unsigned underflow is **especially dangerous** because 0 - 1 = MAX (not negative!).

---

## When to Use Unsigned vs Signed

### Use Unsigned (uint8/uint16) When:

1. **Never negative**: Values cannot be negative by definition (counts, sizes, ages)
2. **Bit manipulation**: Flags, masks, bitfields (no sign bit to worry about)
3. **Binary data**: Raw bytes, protocols, file formats
4. **Memory addresses**: Offsets, indices (never negative)
5. **Full positive range**: Need 0-255 instead of -128 to +127
6. **C interop**: Matching `uint8_t`, `unsigned char`, `uint16_t`, `unsigned short`
7. **Modular arithmetic**: Intentional wrapping (hash functions, ring buffers)

### Use Signed (int8/int16) When:

1. **Can be negative**: Temperatures, offsets, deltas, differences
2. **Arithmetic**: Addition/subtraction where result might be negative
3. **Comparisons**: Need to represent "less than zero"
4. **Default choice**: When unsure, signed is safer (underflow doesn't wrap to large positive!)
5. **Symmetric ranges**: When TBB needed (error propagation)

### Quick Decision Matrix

```aria
// RGB color channel - USE UNSIGNED (0-255, never negative)
uint8:red = 128u8;

// Temperature offset - USE SIGNED (can be negative)
int8:temp_delta = -5i8;  // -5°C change

// Array index - USE UNSIGNED (never negative)
uint16:index = 1000u16;

// Coordinate difference - USE SIGNED (can be negative)
int16:x_delta = -50i16;  // Moved left

// Byte count - USE UNSIGNED (never negative)
uint32:file_size = 1024u32;

// Loop counter - EITHER (but unsigned for large counts)
uint16:i = 0u16;
till 10000u16 loop { i += 1u16; }
```

---

## Overflow Wrapping Behavior

### Modular Arithmetic

Unsigned overflow wraps according to **modular arithmetic** (mod 256 for uint8, mod 65536 for uint16).

```aria
// uint8 overflow
uint8:max = 255u8;
uint8:wrapped = max + 1u8;
// wrapped = 0 (255 + 1 = 256 % 256 = 0)

uint8:value = 250u8;
value += 10u8;
// value = 4 (250 + 10 = 260 % 256 = 4)

// uint16 overflow
uint16:max16 = 65535u16;
uint16:wrapped16 = max16 + 1u16;
// wrapped16 = 0 (65535 + 1 = 65536 % 65536 = 0)
```

**Defined behavior**: Wrapping is well-defined (not UB like C signed overflow).

### Example: Ring Buffer Index

```aria
// Ring buffer using uint8 wrapping
struct:RingBuffer8 = {
    uint8[256]:data;
    uint8:write_index;
    uint8:read_index;
};

func:ring_write = (rb: RingBuffer8@, value: uint8) -> void {
    rb.data[rb.write_index] = value;
    rb.write_index += 1u8;  // Wraps to 0 after 255 (automatic!)
}

func:ring_read = (rb: RingBuffer8@) -> uint8 {
    uint8:value = rb.data[rb.read_index];
    rb.read_index += 1u8;  // Wraps to 0 after 255
    pass(value);
}
```

---

## Underflow Wrapping Behavior

### The Underflow Trap

**Most dangerous property of unsigned integers**: Subtracting below zero wraps to MAX!

```aria
// DANGER: Underflow wraps to maximum!
uint8:count = 0u8;
count -= 1u8;
// count = 255 (NOT -1!) ⚠️

// Common bug: Loop counting down
uint8:i = 10u8;

loop {
    log.write("i = "); i.write_to(log); log.write("\n");
    
    i -= 1u8;
    
    if (i == 0u8) break;  // ⚠️ BUG! Skips i=0, wraps to 255!
end
```

**Why dangerous**: Code expecting "less than zero" gets **large positive value** instead!

### Safe Countdown Pattern

```aria
// SAFE: Use signed integer for countdown
int16:i = 10i16;

loop {
    log.write("i = "); i.write_to(log); log.write("\n");
    
    i -= 1i16;
    
    if (i < 0i16) break;  // ✅ SAFE (can go negative)
end

// OR: Count up instead
uint8:i = 0u8;

till 11u8 loop {
    log.write("i = "); i.write_to(log); log.write("\n");
    i += 1u8;
}
```

### Underflow Detection

```aria
func:safe_subtract_u8 = (a: uint8, b: uint8) -> ?uint8 {
    if (b > a) {
        return NIL;  // Would underflow
    }
    
    return a - b;  // Safe
}

// Usage
uint8:count = 5u8;
?uint8:result = safe_subtract_u8(count, 10u8);

if (result == NIL) {
    stderr.write("Underflow prevented!\n");
}
```

---

## Binary Representation

### Bit Representation (uint8)

```
255 = 0xFF = 0b11111111 (all bits set)
128 = 0x80 = 0b10000000 (MSB set, but NOT negative!)
1   = 0x01 = 0b00000001
0   = 0x00 = 0b00000000
```

**No sign bit**: MSB (bit 7) is just another data bit, not a sign!

### Bit Representation (uint16)

```
65535 = 0xFFFF = 0b1111111111111111 (all bits set)
32768 = 0x8000 = 0b1000000000000000 (MSB set, but NOT negative!)
1     = 0x0001 = 0b0000000000000001
0     = 0x0000 = 0b0000000000000000
```

### Conversion from Signed

```aria
// Signed to unsigned (reinterpret bits)
int8:neg = -1i8;  // 0b11111111 (all bits set)
uint8:u = uint8(neg);  // 0b11111111 = 255

// Why? Bit pattern unchanged, interpretation changed
// -1 (signed) = 0xFF = 255 (unsigned)

int8:neg128 = -128i8;  // 0b10000000
uint8:u128 = uint8(neg128);  // 0b10000000 = 128
```

---

## Arithmetic Operations

### Addition with Overflow

```aria
uint8:a = 200u8;
uint8:b = 100u8;
uint8:sum = a + b;  // 300... wraps to 44 (300 % 256 = 44)

// Safe addition: widen, check, narrow
uint16:wide_a = uint16(a);
uint16:wide_b = uint16(b);
uint16:wide_sum = wide_a + wide_b;  // 300 (fits in uint16)

if (wide_sum > 255u16) {
    stderr.write("Sum doesn't fit in uint8!\n");
} else {
    uint8:result = uint8(wide_sum);  // Safe
}
```

### Subtraction with Underflow

```aria
uint8:a = 50u8;
uint8:b = 100u8;
uint8:diff = a - b;  // -50... underflows to 206 (256 - 50 = 206)

// Safe subtraction: check first
if (a < b) {
    stderr.write("Underflow would occur!\n");
} else {
    uint8:result = a - b;  // Safe
}
```

### Multiplication with Overflow

```aria
uint8:a = 20u8;
uint8:b = 20u8;
uint8:product = a * b;  // 400... wraps to 144 (400 % 256 = 144)

// Safe multiplication: widen
uint16:wide_product = uint16(a) * uint16(b);  // 400 (fits!)
```

### Division

```aria
uint8:a = 100u8;
uint8:b = 3u8;
uint8:quotient = a / b;   // 33 (truncates)
uint8:remainder = a % b;  // 1

// Division by zero: PANIC (undefined behavior)
uint8:zero = 0u8;
uint8:crash = a / zero;  // ⚠️ PANICS!
```

---

## Bit Manipulation

Unsigned integers are **ideal** for bit manipulation (no sign bit complications).

### Bitwise Operators

```aria
uint8:a = 0b10101010u8;
uint8:b = 0b11001100u8;

uint8:and_result = a & b;   // 0b10001000 (AND)
uint8:or_result = a | b;    // 0b11101110 (OR)
uint8:xor_result = a ^ b;   // 0b01100110 (XOR)
uint8:not_result = ~a;      // 0b01010101 (NOT/flip)
```

### Bit Shifts

```aria
uint8:value = 0b00000001u8;  // 1

uint8:left = value << 1u8;   // 0b00000010 (2)
uint8:left2 = value << 2u8;  // 0b00000100 (4)
uint8:left7 = value << 7u8;  // 0b10000000 (128, NOT negative!)

// Right shift (logical - zero-fill, NOT sign-extend)
uint8:high = 0b11000000u8;  // 192
uint8:right = high >> 1u8;  // 0b01100000 (96, zero-filled!)

// Compare to signed (arithmetic shift, sign-extends)
int8:signed_high = -64i8;   // 0b11000000
int8:signed_right = signed_high >> 1i8;  // 0b11100000 (-32, sign-extended)
```

**Key difference**: Unsigned right-shift **zero-fills** (logical), signed right-shift **sign-extends** (arithmetic).

### Bit Manipulation Patterns

```aria
// Set bit
uint8:flags = 0u8;
flags |= (1u8 << 3u8);  // Set bit 3: 0b00001000

// Clear bit
flags &= ~(1u8 << 3u8);  // Clear bit 3

// Toggle bit
flags ^= (1u8 << 3u8);  // Toggle bit 3

// Test bit
if ((flags & (1u8 << 3u8)) != 0u8) {
    log.write("Bit 3 is set\n");
}

// Extract bit field (bits 3-5)
uint8:mask = 0b00111000u8;  // Bits 3, 4, 5
uint8:field = (flags & mask) >> 3u8;
```

---

## C Interoperability

### Direct ABI Compatibility

| Aria Type | C Type | Size | Range |
|-----------|--------|------|-------|
| uint8 | `uint8_t`, `unsigned char` | 1 byte | 0 to 255 |
| uint16 | `uint16_t`, `unsigned short` | 2 bytes | 0 to 65,535 |

### FFI Example

```aria
// C library functions
extern "C" {
    func:c_read_byte = uint8(fd: int32);
    func:c_write_byte = int32(fd: int32, byte: uint8);
    func:c_checksum = uint16(data: uint8@, length: uint32);
}

// Aria usage
uint8:byte = c_read_byte(file_descriptor);

log.write("Read byte: 0x");
byte.write_hex_to(log);
log.write("\n");

int32:result = c_write_byte(file_descriptor, 0xFFu8);

if (result < 0i32) {
    stderr.write("Write failed\n");
}
```

**No conversion needed**: Aria uint8/uint16 are ABI-compatible with C.

---

## Performance Characteristics

### Zero Overhead

```aria
uint8:a = 100u8;
uint8:b = 50u8;
uint8:sum = a + b;
```

**Generated assembly** (x86):
```asm
mov al, 100     ; Load 100 into AL
add al, 50      ; Add 50
; Result in AL (one instruction!)
```

### Memory Efficiency

```aria
// 1 million uint8
uint8[]:bytes = aria.alloc<uint8>(1000000);
// Memory: 1 MB

// vs int16
int16[]:shorts = aria.alloc<int16>(1000000);
// Memory: 2 MB (2× larger)

// vs uint32
uint32[]:ints = aria.alloc<uint32>(1000000);
// Memory: 4 MB (4× larger)
```

**Cache efficiency**: Smaller types = more data per cache line = better performance for large arrays.

---

## Common Patterns

### Saturating Arithmetic (No Wrapping)

```aria
func:saturating_add_u8 = (a: uint8, b: uint8) -> uint8 {
    uint16:wide_sum = uint16(a) + uint16(b);
    
    if (wide_sum > 255u16) {
        return 255u8;  // Saturate at max
    } else {
        return uint8(wide_sum);
    }
}

func:saturating_sub_u8 = (a: uint8, b: uint8) -> uint8 {
    if (b > a) {
        return 0u8;  // Saturate at min (prevent underflow wrap!)
    } else {
        return a - b;
    }
}

// Image brightness adjustment
uint8:pixel = 200u8;
uint8:brighter = saturating_add_u8(pixel, 100u8);  // 255 (saturated, not wrapped to 44)
```

### Range-Checked Narrowing

```aria
func:narrow_to_u8 = (value: uint32) -> ?uint8 {
    if (value > 255u32) {
        return NIL;  // Out of range
    }
    
    return uint8(value);  // Safe cast
}

// Usage
uint32:large = 1000u32;
?uint8:small = narrow_to_u8(large);

if (small == NIL) {
    stderr.write("Value too large for uint8\n");
}
```

### Byte Packing/Unpacking

```aria
// Pack two uint8 into uint16
func:pack_u16 = (high: uint8, low: uint8) -> uint16 {
    return (uint16(high) << 8u16) | uint16(low);
}

// Unpack uint16 into two uint8
func:unpack_u16 = (value: uint16) -> (uint8, uint8) {
    uint8:high = uint8((value >> 8u16) & 0xFFu16);
    uint8:low = uint8(value & 0xFFu16);
    return (high, low);
}

// Network byte order (big-endian)
uint16:port = pack_u16(0x1Fu8, 0x90u8);  // 0x1F90 = 8080
```

---

## Anti-Patterns

### ❌ Counting Down with Unsigned

```aria
// WRONG: Underflow trap!
uint8:i = 10u8;

loop {
    i -= 1u8;
    
    if (i == 0u8) break;  // ❌ BUG! Skips 0, wraps to 255, infinite loop!
end

// RIGHT: Use signed for countdown
int16:i = 10i16;

loop {
    i -= 1i16;
    
    if (i < 0i16) break;  // ✅ SAFE
end
```

### ❌ Subtracting Without Underflow Check

```aria
// WRONG: Subtraction can underflow
uint8:balance = 50u8;
uint8:withdrawal = 100u8;

balance -= withdrawal;  // ❌ Underflows to 206, balance appears large!

// RIGHT: Check before subtracting
if (withdrawal > balance) {
    stderr.write("Insufficient balance!\n");
} else {
    balance -= withdrawal;  // Safe
}
```

### ❌ Mixing Signed and Unsigned

```aria
// WRONG: Comparing signed to unsigned
int8:signed_val = -1i8;
uint8:unsigned_val = 1u8;

if (signed_val < unsigned_val) {  // ⚠️ Implicit conversion!
    // -1 converts to 255 unsigned (bit pattern reinterpreted)
    // Comparison: 255 < 1 is FALSE (unexpected!)
}

// RIGHT: Explicit widening to signed
int16:wide_signed = int16(signed_val);  // -1
int16:wide_unsigned = int16(unsigned_val);  // 1

if (wide_signed < wide_unsigned) {  // TRUE (as expected)
    // Correct comparison
}
```

---

## Migration from C/C++

### Underflow Behavior

**C**: Unsigned underflow wraps (defined)
```c
uint8_t a = 0;
a -= 1;  // a = 255 (wraps, defined behavior)
```

**Aria**: Same (wraps, defined)
```aria
uint8:a = 0u8;
a -= 1u8;  // a = 255 (wraps, defined)
```

**Both are dangerous!** Use signed for values that can go negative.

### Explicit Type Suffixes

**C**: Implicit conversions
```c
uint8_t x = 200;  // Literal is int, implicitly converted
```

**Aria**: Explicit suffix required
```aria
uint8:x = 200u8;  // Must specify u8 suffix
```

### Comparison Warnings

**C**: Signed/unsigned comparison warnings
```c
int a = -1;
unsigned int b = 1;
if (a < b) { }  // Warning: comparison between signed and unsigned
```

**Aria**: Same warning, but stricter
```aria
int32:a = -1i32;
uint32:b = 1u32;
if (a < b) { }  // ⚠️ WARNING: Implicit conversion in comparison
```

---

## Related Concepts

### Other Integer Types

- **int8, int16**: Signed small integers
- **uint32, uint64**: Larger unsigned integers
- **int32, int64**: Larger signed integers
- **tbb8, tbb16**: Symmetric-range error-propagating integers

### Special Values

- **NIL**: Optional types (not applicable to uint8/uint16)
- **ERR**: TBB error sentinel (not applicable to unsigned ints)
- **NULL**: Pointer sentinel (not applicable to value types)

### Related Operations

- **Bit manipulation**: AND, OR, XOR, NOT, shifts
- **Saturating arithmetic**: Clamping instead of wrapping
- **Modular arithmetic**: Intentional wrapping (hash functions, ring buffers)
- **Widening**: uint8 → uint16 → uint32 (always safe)
- **Narrowing**: uint32 → uint16 → uint8 (check range!)

---

## Summary

**uint8** and **uint16** are Aria's small unsigned integers:

✅ **Non-negative only**: uint8 (0-255), uint16 (0-65,535)  
✅ **Double positive range**: vs int8/int16 (twice the positive values)  
✅ **Wrapping overflow**: Defined modular arithmetic  
✅ **Zero overhead**: Native machine instructions  
✅ **Bit manipulation**: Ideal (no sign bit complications)  
✅ **C compatible**: Direct FFI, same ABI  

⚠️ **Underflow wraps to MAX**: 0 - 1 = 255 (DANGER!)  
⚠️ **Cannot be negative**: Wrong choice if values can go below zero  
⚠️ **Signed/unsigned mixing**: Implicit conversions can surprise  

**Use uint8/uint16 for**:
- Never-negative values (counts, sizes, ages)
- Bit manipulation (flags, masks, bitfields)
- Binary data (bytes, protocols, file formats)
- RGB colors (0-255 per channel)
- Network ports (0-65535)
- Memory efficiency (large arrays)

**Use int8/int16 for**:
- Can-be-negative values (temperatures, deltas, offsets)
- Default choice (safer for arithmetic)
- Counters that might go negative

**Use TBB types for**:
- Safety-critical (overflow detection)
- Error propagation (sticky errors)
- Symmetric ranges (abs works)

---

**Next**: [uint32, uint64 (Unsigned Medium/Large Integers)](uint32_uint64.md)  
**See Also**: [int8/int16](int8_int16.md), [uint32/uint64](uint32_uint64.md), [Bit Manipulation](../operators/bitwise.md)

---

*Aria Language Project - Unsigned Integers with Underflow Awareness*  
*February 12, 2026 - Establishing timestamped prior art on unsigned integer semantics*

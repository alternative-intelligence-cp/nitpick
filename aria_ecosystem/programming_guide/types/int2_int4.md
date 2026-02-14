# Small Integers (int2, int4)

**Category**: Types → Integers → Signed  
**Sizes**: 2-bit, 4-bit  
**Ranges**: int2 (-2 to +1), int4 (-8 to +7)  
**Purpose**: Ultra-small signed integers, nibble operations, educational  
**Status**: ✅ FULLY IMPLEMENTED

---

## Table of Contents

1. [Overview](#overview)
2. [int2: 2-Bit Signed Integer](#int2-2-bit-signed-integer)
3. [int4: 4-Bit Signed Integer](#int4-4-bit-signed-integer)
4. [Two's Complement at Small Scale](#twos-complement-at-small-scale)
5. [Arithmetic Operations](#arithmetic-operations)
6. [Nibble Operations (int4)](#nibble-operations-int4)
7. [Bit Representation](#bit-representation)
8. [Use Cases](#use-cases)
9. [Storage and Performance](#storage-and-performance)
10. [Common Patterns](#common-patterns)
11. [Educational Value](#educational-value)
12. [Anti-Patterns](#anti-patterns)
13. [Related Concepts](#related-concepts)

---

## Overview

Aria's **int2** and **int4** are **ultra-small signed integers** using 2 and 4 bits respectively.

**Key Properties**:
- **int2**: 2 bits, range -2 to +1 (4 values)
- **int4**: 4 bits, range -8 to +7 (16 values)
- **Asymmetric ranges**: One more negative than positive
- **Two's complement**: Standard signed representation
- **Nibble operations**: int4 fits in half a byte
- **Educational**: Understanding small-scale integer behavior

**⚠️ UNCOMMON**: Most languages start at 8-bit (byte).

---

## int2: 2-Bit Signed Integer

### Range

**Minimum**: -2 (0b10)  
**Maximum**: +1 (0b01)  
**Total values**: 4 (2²)

**All values**: -2, -1, 0, +1

### Declaration

```aria
int2:negative_two = -2i2;    // Minimum
int2:negative_one = -1i2;
int2:zero = 0i2;
int2:positive_one = 1i2;     // Maximum
```

### Literal Suffix

```aria
int2:value = -1i2;           // Explicit int2 literal
```

### Bit Patterns

| Value | Binary | Hex |
|-------|--------|-----|
| +1 | 01 | 0x1 |
| 0 | 00 | 0x0 |
| -1 | 11 | 0x3 |
| -2 | 10 | 0x2 |

**Sign bit**: Bit 1 (leftmost)
- 0: Positive (0, +1)
- 1: Negative (-2, -1)

### Arithmetic Examples

```aria
int2:a = 1i2;
int2:b = 1i2;
int2:sum = a + b;            // 1 + 1 = 2, overflows to -2

int2:c = -2i2;
int2:d = -1i2;
int2:sub = c - d;            // -2 - (-1) = -1
```

### Common Use Cases

1. **Educational**: 4-value state machine
2. **Embedded**: Ultra-compact sensor readings
3. **Quaternary logic**: 4 distinct states
4. **Bit manipulation**: Learning two's complement

### Example: State Machine

```aria
const:int2:STATE_INIT = -2i2;
const:int2:STATE_READY = -1i2;
const:int2:STATE_RUNNING = 0i2;
const:int2:STATE_DONE = 1i2;

int2:current_state = STATE_INIT;

func:transition_state = (state: int2) -> int2 {
    if (state == STATE_INIT) return STATE_READY;
    if (state == STATE_READY) return STATE_RUNNING;
    if (state == STATE_RUNNING) return STATE_DONE;
    return STATE_INIT;  // Loop
}

current_state = transition_state(current_state);  // -2 → -1
```

---

## int4: 4-Bit Signed Integer

### Range

**Minimum**: -8 (0b1000)  
**Maximum**: +7 (0b0111)  
**Total values**: 16 (2⁴)

**All values**: -8, -7, -6, -5, -4, -3, -2, -1, 0, +1, +2, +3, +4, +5, +6, +7

### Declaration

```aria
int4:minimum = -8i4;         // Minimum value
int4:zero = 0i4;
int4:maximum = 7i4;          // Maximum value
```

### Literal Suffix

```aria
int4:value = 5i4;            // Explicit int4 literal
int4:negative = -3i4;
```

### Nibble

**int4 = one nibble** (half a byte):
- 1 byte = 2 nibbles = 2 × int4
- Common in BCD (Binary Coded Decimal)
- Hex digit represents one nibble perfectly

### Bit Patterns (Partial)

| Value | Binary | Hex |
|-------|--------|-----|
| +7 | 0111 | 0x7 |
| +6 | 0110 | 0x6 |
| +1 | 0001 | 0x1 |
| 0 | 0000 | 0x0 |
| -1 | 1111 | 0xF |
| -2 | 1110 | 0xE |
| -7 | 1001 | 0x9 |
| -8 | 1000 | 0x8 |

**Sign bit**: Bit 3 (leftmost)
- 0: Positive (0 to +7)
- 1: Negative (-8 to -1)

### Arithmetic Examples

```aria
int4:a = 5i4;
int4:b = 3i4;
int4:sum = a + b;            // 5 + 3 = 8, overflows to -8

int4:c = 7i4;
int4:d = 1i4;
int4:overflow = c + d;       // 7 + 1 = 8, wraps to -8

int4:e = -8i4;
int4:f = e * -1i4;           // -8 × -1 = 8, overflows to -8 (abs(MIN) problem!)
```

### Common Use Cases

1. **BCD (Binary Coded Decimal)**: Each nibble = decimal digit (0-9)
2. **Embedded systems**: Compact sensor values
3. **Educational**: Perfect size for teaching overflow
4. **Nibble packing**: Two int4 per byte
5. **Color depths**: 16-level grayscale (0-15, but signed -8 to +7)

### Example: Nibble Packing

```aria
// Pack two int4 into one byte
func:pack_nibbles = (high: int4, low: int4) -> uint8 {
    uint8:h = uint8(high) << 4u8;
    uint8:l = uint8(low) & 0x0Fu8;
    return h | l;
}

// Unpack byte into two int4
func:unpack_nibbles = (packed: uint8) -> (int4, int4) {
    int4:high = int4((packed >> 4u8) & 0x0Fu8);
    int4:low = int4(packed & 0x0Fu8);
    pass((high, low));
}

int4:value1 = 5i4;
int4:value2 = -3i4;

uint8:packed = pack_nibbles(value1, value2);
(int4:unpacked1, int4:unpacked2) = unpack_nibbles(packed);

// unpacked1 = 5, unpacked2 = -3 ✓
```

---

## Two's Complement at Small Scale

### Asymmetric Range

**All signed integers** have one more negative value than positive:

| Type | Min | Max | Asymmetry |
|------|-----|-----|-----------|
| int1 | -1 | 0 | 1 more negative |
| int2 | -2 | +1 | 1 more negative |
| int4 | -8 | +7 | 1 more negative |
| int8 | -128 | +127 | 1 more negative |

**Formula**:
- Min = -2^(N-1)
- Max = 2^(N-1) - 1

**For int2** (N=2):
- Min = -2^1 = -2
- Max = 2^1 - 1 = +1

**For int4** (N=4):
- Min = -2^3 = -8
- Max = 2^3 - 1 = +7

### Demonstrating abs(MIN) Problem

```aria
int4:minimum = -8i4;
int4:negated = -minimum;     // -(-8) = 8, but max is 7!
// Overflows to -8 (wraps around)

log.write("Negated: ");
negated.write_to(log);
log.write("\n");
// Output: "Negated: -8"
```

**This is why TBB exists** - symmetric ranges avoid this issue.

---

## Arithmetic Operations

### Addition (int2)

```aria
int2:a = 1i2;
int2:b = 1i2;
int2:sum = a + b;            // 1 + 1 = 2, wraps to -2

int2:c = -2i2;
int2:d = 1i2;
int2:add = c + d;            // -2 + 1 = -1
```

### Addition (int4)

```aria
int4:a = 7i4;
int4:b = 1i4;
int4:overflow = a + b;       // 7 + 1 = 8, wraps to -8

int4:c = -8i4;
int4:d = -1i4;
int4:underflow = c + d;      // -8 + (-1) = -9, wraps to 7
```

### Multiplication (int4)

```aria
int4:a = 4i4;
int4:b = 2i4;
int4:product = a * b;        // 4 × 2 = 8, wraps to -8

int4:c = -4i4;
int4:d = 2i4;
int4:neg = c * d;            // -4 × 2 = -8 (fits!)
```

### Division (int4)

```aria
int4:a = -8i4;
int4:b = -1i4;
int4:quotient = a / b;       // -8 / -1 = 8, wraps to -8

int4:c = 7i4;
int4:d = 2i4;
int4:truncate = c / d;       // 7 / 2 = 3 (truncate toward zero)
```

---

## Nibble Operations (int4)

### Nibble Definition

**Nibble** = 4 bits = half a byte

**Why useful**:
- Hex digit = 1 nibble exactly (0x0 to 0xF)
- BCD uses nibbles for decimal digits
- Compact packing (2 nibbles per byte)

### Extracting Nibbles from Byte

```aria
func:extract_high_nibble = (byte: uint8) -> int4 {
    return int4((byte >> 4u8) & 0x0Fu8);
}

func:extract_low_nibble = (byte: uint8) -> int4 {
    return int4(byte & 0x0Fu8);
}

uint8:value = 0xA7u8;  // Binary: 1010 0111

int4:high = extract_high_nibble(value);  // 0xA = 10... wraps to -6
int4:low = extract_low_nibble(value);    // 0x7 = 7 ✓
```

### BCD (Binary Coded Decimal) with int4

**BCD**: Each nibble represents decimal digit 0-9

```aria
// Encode decimal 42 as BCD (two nibbles)
func:decimal_to_bcd = (value: uint8) -> uint8 {
    uint8:tens = value / 10u8;
    uint8:ones = value % 10u8;
    return (tens << 4u8) | ones;
}

// Decode BCD to decimal
func:bcd_to_decimal = (bcd: uint8) -> uint8 {
    uint8:tens = (bcd >> 4u8) & 0x0Fu8;
    uint8:ones = bcd & 0x0Fu8;
    return (tens * 10u8) + ones;
}

uint8:decimal = 42u8;
uint8:bcd = decimal_to_bcd(decimal);      // 0x42
uint8:back = bcd_to_decimal(bcd);         // 42 ✓
```

---

## Bit Representation

### int2 Bit Patterns

```
Value | S M | Decimal
------|-----|--------
 +1   | 0 1 |   1
  0   | 0 0 |   0
 -1   | 1 1 |  -1
 -2   | 1 0 |  -2
```

**S** = Sign bit  
**M** = Magnitude bit

### int4 Bit Patterns

```
Value | S M M M | Decimal
------|---------|--------
 +7   | 0 1 1 1 |   7
 +1   | 0 0 0 1 |   1
  0   | 0 0 0 0 |   0
 -1   | 1 1 1 1 |  -1
 -8   | 1 0 0 0 |  -8
```

**S** = Sign bit  
**MMM** = Magnitude bits

---

## Use Cases

### 1. Educational: Overflow Demonstration

```aria
// Teach overflow at manageable scale
func:demonstrate_overflow = () -> void {
    int4:max = 7i4;
    
    log.write("Maximum: ");
    max.write_to(log);
    log.write("\n");
    
    int4:overflow = max + 1i4;
    
    log.write("Maximum + 1: ");
    overflow.write_to(log);
    log.write("\n");
    // Output: "Maximum + 1: -8"
}
```

### 2. Embedded Systems: Sensor Values

```aria
// Temperature sensor with limited precision (-8°C to +7°C)
extern "C" {
    func:read_temperature_sensor = int4();
}

int4:temp = read_temperature_sensor();

if (temp < 0i4) {
    log.write("Freezing: ");
    temp.write_to(log);
    log.write("°C\n");
}
```

### 3. Compact Storage: Array Packing

```aria
// Store 16 int4 values in 8 bytes
struct:PackedInt4Array = {
    uint8[8]:data;
    
    func:get = (self, index: uint8) -> int4 {
        uint8:byte_index = index / 2u8;
        bool:is_high = (index % 2u8) == 0u8;
        
        uint8:byte = self.data[byte_index];
        
        if (is_high) {
            return int4((byte >> 4u8) & 0x0Fu8);
        } else {
            return int4(byte & 0x0Fu8);
        }
    }
    
    func:set = (self, index: uint8, value: int4) -> void {
        uint8:byte_index = index / 2u8;
        bool:is_high = (index % 2u8) == 0u8;
        
        uint8:byte = self.data[byte_index];
        uint8:nibble = uint8(value) & 0x0Fu8;
        
        if (is_high) {
            self.data[byte_index] = (nibble << 4u8) | (byte & 0x0Fu8);
        } else {
            self.data[byte_index] = (byte & 0xF0u8) | nibble;
        }
    }
};

PackedInt4Array:array = {data: [0u8, 0u8, 0u8, 0u8, 0u8, 0u8, 0u8, 0u8]};

array.set(0u8, 5i4);   // Store 5 at index 0
array.set(1u8, -3i4);  // Store -3 at index 1

int4:value = array.get(0u8);  // Retrieve 5
```

### 4. State Machines (int2)

```aria
// Simple 4-state machine
const:int2:IDLE = 0i2;
const:int2:PROCESSING = 1i2;
const:int2:ERROR = -1i2;
const:int2:SHUTDOWN = -2i2;

int2:state = IDLE;

func:handle_event = (current: int2, event: uint8) -> int2 {
    if (current == IDLE && event == 1u8) return PROCESSING;
    if (current == PROCESSING && event == 2u8) return ERROR;
    if (current == ERROR && event == 3u8) return SHUTDOWN;
    return current;  // No transition
}
```

---

## Storage and Performance

### Memory Footprint

**Theoretical**:
- int2: 2 bits
- int4: 4 bits (1 nibble)

**Practical** (without packing):
- int2: 1 byte (wasteful)
- int4: 1 byte (wastes half)

**With packing**:
- int2 array: 4 values per byte
- int4 array: 2 values per byte

### Performance

**Operations**: Same speed as int8/int16 (CPU operates on bytes minimum)

**Packing overhead**: Bit manipulation adds cost

**Use cases prioritize**:
- Memory savings (packed arrays)
- Educational clarity
- NOT performance

---

## Common Patterns

### Range Checking Before Narrowing

```aria
func:narrow_to_int4 = (value: int8) -> ?int4 {
    if (value < -8i8 || value > 7i8) {
        return NIL;  // Out of range
    }
    
    return int4(value);  // Safe
}

int8:large = 100i8;
?int4:result = narrow_to_int4(large);

if (result == NIL) {
    stderr.write("Value out of int4 range!\n");
}
```

### Saturating Arithmetic (int4)

```aria
func:saturating_add_int4 = (a: int4, b: int4) -> int4 {
    int8:sum = int8(a) + int8(b);
    
    if (sum > 7i8) return 7i4;
    if (sum < -8i8) return -8i4;
    
    return int4(sum);
}

int4:a = 7i4;
int4:b = 5i4;
int4:saturated = saturating_add_int4(a, b);  // 7 (not overflow to -8)
```

---

## Educational Value

### Teaching Integer Properties

**int2 and int4 are perfect teaching tools**:

1. **Small enough to enumerate all values**:
   - int2: 4 values (can list completely)
   - int4: 16 values (manageable)

2. **Demonstrate overflow quickly**:
   - int4: 7 + 1 = -8 (immediately visible)

3. **Show two's complement pattern**:
   - Binary patterns clear at 2/4 bits

4. **Explain asymmetry**:
   - Why one more negative? Clear at small scale.

### Progression of Understanding

```
int1 → int2 → int4 → int8 → int16...

Each step doubles values, but principles stay same:
- Two's complement
- Asymmetric range
- Wrapping overflow
- abs(MIN) problem
```

---

## Anti-Patterns

### ❌ Using int2/int4 for General Arithmetic

```aria
// WRONG: Frequent overflow, confusing
int4:a = 10i4;  // Wraps to -6!
int4:b = 5i4;
int4:sum = a + b;  // Overflow mess

// RIGHT: Use appropriately sized type
int8:a = 10i8;
int8:b = 5i8;
int8:sum = a + b;  // 15 (correct)
```

### ❌ Ignoring Packing Overhead

```aria
// WRONG: No memory savings, bit manipulation overhead
int4:value1 = 3i4;  // Stored in 1 byte (wastes 4 bits)
int4:value2 = 5i4;  // Stored in 1 byte (wastes 4 bits)

// RIGHT: Pack if memory constrained
uint8:packed = pack_nibbles(value1, value2);  // Both in 1 byte
```

### ❌ Mixing with Larger Types Without Care

```aria
// WRONG: Implicit overflow
int4:small = 7i4;
int8:large = 100i8;
int4:result = small + int4(large);  // 107 wraps to... confusion!

// RIGHT: Widen before arithmetic
int8:small_wide = int8(small);
int8:result = small_wide + large;  // 107 (correct)
```

---

## Related Concepts

### Other Small Integers

- **int1**: 1-bit signed (-1, 0)
- **uint2, uint4**: Unsigned variants (not particularly useful)

### Larger Integers

- **int8, int16**: Standard small integers
- **int32, int64**: Production integers

### Symmetric Alternatives

- **TBB family**: Symmetric ranges (avoid abs(MIN) problem)

---

## Summary

**int2** and **int4** are Aria's ultra-small signed integers:

✅ **int2**: 2 bits, -2 to +1 (4 values)  
✅ **int4**: 4 bits, -8 to +7 (16 values, 1 nibble)  
✅ **Educational excellence**: Perfect for teaching overflow, two's complement  
✅ **Compact storage**: Pack 2 int4 per byte, 4 int2 per byte  
✅ **Nibble operations**: int4 fits hex digits, BCD encoding  

⚠️ **Frequent overflow**: Very limited range  
⚠️ **Uncommon**: Most languages start at 8-bit  
⚠️ **Packing overhead**: Bit manipulation adds cost  

**Use int2/int4 for**:
- Educational demonstrations (overflow, two's complement)
- Embedded systems (ultra-compact sensors)
- Nibble operations (BCD, hex digits)
- State machines (few states)

**Avoid for**:
- General arithmetic (use int8+)
- Performance-critical code (overhead not worth savings)
- When overflow is likely (range too small)

---

**Next**: [Medium Integers (int128, int256, int512)](int128_int256_int512.md)  
**See Also**: [int1](int1.md), [int8/int16](int8_int16.md), [Nibble Packing](nibble_operations.md)

---

*Aria Language Project - Complete Integer Spectrum Continues*  
*February 12, 2026 - Ultra-small integers for education and embedded systems*

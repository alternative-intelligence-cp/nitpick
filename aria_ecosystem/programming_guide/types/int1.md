# Single Bit Integer (int1)

**Category**: Types → Integers → Signed  
**Size**: 1 bit  
**Range**: -1, 0  
**Purpose**: Minimal signed integer, educational, theoretical completeness  
**Status**: ✅ FULLY IMPLEMENTED

---

## Table of Contents

1. [Overview](#overview)
2. [The Two Values](#the-two-values)
3. [Why int1 Exists](#why-int1-exists)
4. [Declaration and Literals](#declaration-and-literals)
5. [Arithmetic Operations](#arithmetic-operations)
6. [Boolean Semantics](#boolean-semantics)
7. [vs bool Type](#vs-bool-type)
8. [Bit Representation](#bit-representation)
9. [Use Cases](#use-cases)
10. [Storage and Performance](#storage-and-performance)
11. [Common Patterns](#common-patterns)
12. [Educational Value](#educational-value)
13. [Anti-Patterns](#anti-patterns)
14. [Related Concepts](#related-concepts)

---

## Overview

Aria's **int1** is a **1-bit signed integer** representing exactly two values: **-1** and **0**.

**Key Properties**:
- **Smallest possible signed integer**: 1 bit = 2 states
- **Asymmetric range**: -1, 0 (like all signed integers, one more negative)
- **Two's complement**: Single bit treated as sign bit
- **Boolean-like**: Two states, but numeric semantics
- **Theoretical completeness**: Aria has ALL integer sizes

**⚠️ UNUSUAL**: Most languages don't have 1-bit signed integers.

---

## The Two Values

### Range

**Minimum**: -1 (bit = 1)  
**Maximum**: 0 (bit = 0)  
**Total values**: 2 (2¹)

### Why -1 and 0 (Not 0 and 1)?

**Two's complement with 1 bit**:
- Bit 0 (0b0) = 0 (positive zero)
- Bit 1 (0b1) = -1 (negative one)

**The sign bit IS the value**:
- Bit 0: Sign positive, value 0
- Bit 1: Sign negative, magnitude 1

**Consistency**: All Aria signed integers follow two's complement, including int1.

---

## Why int1 Exists

### 1. Theoretical Completeness

Aria has **ALL** integer sizes:
- int1 (1 bit)
- int2 (2 bits)
- int4 (4 bits)
- int8, int16, int32, int64...
- int1024, int2048, int4096...

**No gaps**. Complete spectrum.

### 2. Educational Value

**Shows how two's complement works at smallest scale**:
- Single bit = sign and magnitude combined
- int1 demonstrates asymmetry (one more negative)
- Foundation for understanding larger integers

### 3. Embedded Systems

**Ultra-compact storage**:
- Sensors with binary states (on/off)
- Hardware interfaces (single-bit signals)
- Extreme memory constraints

### 4. Consistency

**Why not just use bool?**
- bool: true/false (logical semantics)
- int1: -1/0 (numeric semantics)
- Different purposes, similar storage

---

## Declaration and Literals

### Declaration

```aria
int1:value = 0i1;        // Zero
int1:negative = -1i1;    // Negative one
```

### Literal Suffix

```aria
int1:zero = 0i1;         // Explicit int1 zero
int1:neg_one = -1i1;     // Explicit int1 negative one
```

### Default Value

```aria
int1:uninitialized;      // Compiler error (must initialize)
int1:default = 0i1;      // Explicit zero initialization
```

---

## Arithmetic Operations

### Addition

```aria
int1:a = 0i1;
int1:b = -1i1;
int1:sum = a + b;        // 0 + (-1) = -1 (wraps: actually -1)

int1:c = -1i1;
int1:d = -1i1;
int1:overflow = c + d;   // -1 + (-1) = -2, wraps to 0
```

**Addition table**:
```
  0 + 0 = 0
  0 + -1 = -1
 -1 + 0 = -1
 -1 + -1 = -2 → wraps to 0 (overflow)
```

### Subtraction

```aria
int1:a = 0i1;
int1:b = -1i1;
int1:diff = a - b;       // 0 - (-1) = 1, wraps to -1

int1:c = -1i1;
int1:d = 0i1;
int1:sub = c - d;        // -1 - 0 = -1
```

**Subtraction table**:
```
  0 - 0 = 0
  0 - -1 = 1 → wraps to -1
 -1 - 0 = -1
 -1 - -1 = 0
```

### Multiplication

```aria
int1:a = -1i1;
int1:b = -1i1;
int1:product = a * b;    // -1 × -1 = 1, wraps to -1

int1:c = 0i1;
int1:d = -1i1;
int1:zero = c * d;       // 0 × -1 = 0
```

**Multiplication table**:
```
  0 × 0 = 0
  0 × -1 = 0
 -1 × 0 = 0
 -1 × -1 = 1 → wraps to -1
```

### Division

```aria
int1:a = -1i1;
int1:b = -1i1;
int1:quotient = a / b;   // -1 / -1 = 1, wraps to -1

int1:c = 0i1;
int1:d = -1i1;
int1:div_zero = c / d;   // 0 / -1 = 0
```

**Division by zero**: PANIC (like all Aria integers)

### Negation

```aria
int1:value = -1i1;
int1:negated = -value;   // -(-1) = 1, wraps to -1

int1:zero = 0i1;
int1:neg_zero = -zero;   // -0 = 0
```

**Negation table**:
```
 -0 = 0
 -(-1) = 1 → wraps to -1
```

---

## Boolean Semantics

### Truthy/Falsy Interpretation

**In conditional contexts**, int1 acts like boolean:

```aria
int1:flag = -1i1;

if (flag) {
    // Executes: -1 is "truthy" (non-zero)
    log.write("Flag is set\n");
}

int1:zero = 0i1;

if (zero) {
    // Does NOT execute: 0 is "falsy"
} else {
    log.write("Flag is clear\n");
}
```

**Convention**:
- **-1 = true** (non-zero)
- **0 = false** (zero)

### Logical Operations (Interpreted as Boolean)

```aria
func:logical_and = (a: int1, b: int1) -> int1 {
    return (a != 0i1 && b != 0i1) ? -1i1 : 0i1;
}

func:logical_or = (a: int1, b: int1) -> int1 {
    return (a != 0i1 || b != 0i1) ? -1i1 : 0i1;
}

func:logical_not = (a: int1) -> int1 {
    return (a == 0i1) ? -1i1 : 0i1;
}

int1:a = -1i1;
int1:b = 0i1;

int1:and_result = logical_and(a, b);  // 0 (false)
int1:or_result = logical_or(a, b);    // -1 (true)
int1:not_a = logical_not(a);          // 0 (false)
```

---

## vs bool Type

### Similarities

- **Two states**: int1 (-1, 0) / bool (true, false)
- **Storage**: Both 1 bit conceptually
- **Conditional use**: Both work in if/while conditions

### Differences

| Feature | int1 | bool |
|---------|------|------|
| **Type** | Signed integer | Boolean |
| **Values** | -1, 0 | true, false |
| **Semantics** | Numeric | Logical |
| **Arithmetic** | Yes (wrapping) | No (not numeric) |
| **True value** | -1 (non-zero) | true |
| **False value** | 0 | false |
| **Overflow** | Wraps | N/A |
| **Purpose** | Compact signed integer | Logic operations |

### When to Use Each

**Use int1**:
- Need numeric operations (even if trivial)
- Theoretical completeness demonstration
- Embedded systems with signed bit values
- Educational purposes (two's complement)

**Use bool**:
- Logic conditions (if/while)
- Flags with true/false semantics
- Clarity (intent is logical, not numeric)
- Standard practice

**Most cases: Use bool for clarity.**

---

## Bit Representation

### Two's Complement (1 bit)

```
Value | Binary
------|-------
  0   |  0
 -1   |  1
```

**The bit IS the sign**:
- 0: Positive (value 0)
- 1: Negative (value -1)

### No Positive Value Besides Zero

**Can't represent +1**:
- Would need 2 bits minimum (int2: -2, -1, 0, 1)
- int1 has no space for positive non-zero

---

## Use Cases

### 1. Educational (Two's Complement Demonstration)

```aria
// Show asymmetry at smallest scale
int1:min = -1i1;   // Minimum value
int1:max = 0i1;    // Maximum value

log.write("int1 range: ");
min.write_to(log);
log.write(" to ");
max.write_to(log);
log.write("\n");
// Output: "int1 range: -1 to 0"
```

### 2. Embedded Systems (Single-Bit Signed Sensor)

```aria
// Rare: Hardware sensor with signed single-bit output
extern "C" {
    func:read_sensor_bit = int1();
}

int1:sensor_value = read_sensor_bit();

if (sensor_value == -1i1) {
    log.write("Sensor detected negative state\n");
} else {
    log.write("Sensor neutral\n");
}
```

### 3. Theoretical Completeness

```aria
// Aria has ALL integer sizes
func:demonstrate_completeness = () -> void {
    int1:i1 = -1i1;
    int2:i2 = -2i2;
    int4:i4 = -8i4;
    int8:i8 = -128i8;
    // ... all the way to int4096
    
    log.write("Complete integer spectrum implemented\n");
}
```

### 4. Compact Storage (Array of Single Bits)

```aria
// Extremely compact signed bit array
int1[64]:bit_flags;  // 64 bits total (8 bytes)

till 64 loop:i
    bit_flags[i] = (i % 2u32 == 0u32) ? 0i1 : -1i1;
end

// Packed storage: 64 signed bits in 8 bytes
```

---

## Storage and Performance

### Memory Footprint

**Theoretical**: 1 bit  
**Practical**: Usually 1 byte (8 bits) due to hardware alignment

**Array packing**:
- `int1[8]` can pack into 1 byte (8 bits)
- Requires bit manipulation
- May have performance cost

### Performance

**Operations**: Same as int8/int16 (CPU works on bytes/words minimum)

**Not faster than bool** - both compile to similar code.

### Alignment

**Single int1**: Typically 1 byte aligned  
**Arrays**: Can be bit-packed for compactness

---

## Common Patterns

### Sign Indicator

```aria
func:sign_of = (value: int32) -> int1 {
    if (value < 0i32) return -1i1;
    return 0i1;  // Zero or positive
}

int32:number = -42i32;
int1:sign = sign_of(number);  // -1 (negative)
```

### Boolean-Like Flag

```aria
int1:enabled = -1i1;  // True (enabled)

if (enabled) {
    perform_action();
}

enabled = 0i1;  // False (disabled)
```

### Toggling

```aria
int1:state = 0i1;

// Toggle: 0 → -1, -1 → 0
func:toggle = (s: int1) -> int1 {
    return (s == 0i1) ? -1i1 : 0i1;
}

state = toggle(state);  // 0 → -1
state = toggle(state);  // -1 → 0
```

---

## Educational Value

### Understanding Two's Complement

**int1 demonstrates**:
- Sign bit = entire value
- Asymmetric range (one more negative)
- Overflow wrapping at smallest scale

**Progression**:
```
int1:  -1, 0           (2 values)
int2:  -2, -1, 0, 1    (4 values)
int4:  -8..+7          (16 values)
int8:  -128..+127      (256 values)
```

**Pattern**: Always one more negative than positive.

### Why Asymmetry Exists

**Two's complement with N bits**:
- Maximum: 2^(N-1) - 1
- Minimum: -2^(N-1)

**For int1** (N=1):
- Maximum: 2^0 - 1 = 0
- Minimum: -2^0 = -1

**Consistent across all sizes**: int1 shows why asymmetry is fundamental.

---

## Anti-Patterns

### ❌ Using int1 Instead of bool

```aria
// WRONG: Unclear intent
int1:is_valid = -1i1;  // Why not bool?

// RIGHT: Clear intent
bool:is_valid = true;
```

**Use bool for logical flags** - int1 is numeric.

### ❌ Expecting Positive Values

```aria
// WRONG: Can never be 1
int1:value = 1i1;  // ERROR: 1 wraps to -1!

// RIGHT: Only -1 or 0
int1:value = -1i1;  // Valid
int1:zero = 0i1;    // Valid
```

### ❌ Complex Arithmetic

```aria
// WRONG: Meaningless overflow
int1:a = -1i1;
int1:b = -1i1;
int1:sum = a + b;  // -2 wraps to 0 (confusing!)

// RIGHT: Use larger type if arithmetic needed
int8:a = -1i8;
int8:b = -1i8;
int8:sum = a + b;  // -2 (correct)
```

---

## Related Concepts

### Other Small Integers

- **int2**: 2-bit signed (-2 to +1)
- **int4**: 4-bit signed (-8 to +7)
- **int8**: 8-bit signed (-128 to +127)

### Boolean Type

- **bool**: Logical true/false (not numeric)

### Other Integer Families

- **uint1**: (Not useful - only 0, 1; use bool instead)
- **tbb family**: Symmetric ranges (tbb8: -127 to +127)

---

## Summary

**int1** is Aria's 1-bit signed integer:

✅ **Smallest signed integer**: 1 bit = -1, 0  
✅ **Two's complement**: Demonstrates asymmetry at minimal scale  
✅ **Theoretical completeness**: Aria has ALL integer sizes  
✅ **Educational**: Shows how two's complement works  
✅ **Compact storage**: Arrays can bit-pack  

⚠️ **Unusual**: Most languages don't have this  
⚠️ **Use bool instead**: For logical flags (clearer intent)  
⚠️ **Limited arithmetic**: Trivial operations, frequent overflow  

**Use int1 for**:
- Educational purposes (teaching two's complement)
- Theoretical completeness demonstrations
- Embedded systems (signed single-bit hardware)
- Extreme memory constraints (bit-packed arrays)

**Avoid int1 for**:
- Logical flags (use bool)
- Arithmetic operations (use int8+)
- General programming (too limited)

---

**Next**: [Small Integers (int2, int4)](int2_int4.md) - 2-bit and 4-bit signed  
**See Also**: [Boolean Type](bool.md), [int8/int16](int8_int16.md)

---

*Aria Language Project - Complete Integer Spectrum*  
*February 12, 2026 - Theoretical completeness and educational foundation*

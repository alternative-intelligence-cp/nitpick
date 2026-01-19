# Balanced Ternary and Nonary Types

**Category**: Types → Balanced Number Systems  
**Types**: `trit`, `tryte`, `nit`, `nyte`  
**Purpose**: Non-binary balanced number systems for specialized applications

---

## Overview

Aria includes built-in support for **balanced ternary** (base-3) and **balanced nonary** (base-9) number systems. These use symmetric digit sets centered around zero, enabling elegant representations of negative numbers without two's complement.

**⚠️ CRITICAL: These types and their representations are NOT NEGOTIABLE.**

---

## Quick Reference

| Type | Base | Digits | Range | Storage | Values |
|------|------|--------|-------|---------|--------|
| **trit** | 3 | {-1, 0, 1} | Single digit | 2 bits | 3 |
| **tryte** | 3^10 | 10 trits | -29524 to +29524 | uint16 | 59,049 |
| **nit** | 9 | {-4, -3, -2, -1, 0, 1, 2, 3, 4} | Single digit | 4 bits | 9 |
| **nyte** | 9^5 | 5 nits | -29524 to +29524 | uint16 | 59,049 |

---

## Balanced Ternary System

### The `trit` Type

A **trit** (ternary digit) is a single balanced ternary digit with three possible values:

```
-1  (negative one)
 0  (zero)
+1  (positive one)
```

#### Declaration and Usage

```aria
// Single trit values
trit:negative = -1;
trit:zero = 0;
trit:positive = 1;

// Arithmetic
trit:sum = negative + positive;  // 0
trit:product = negative * negative;  // 1 ((-1) * (-1) = 1)
```

#### Why Balanced Ternary?

Unlike binary (0, 1) or standard ternary (0, 1, 2), balanced ternary uses {-1, 0, 1}:

- **Symmetric**: Negative and positive are mirror images
- **No two's complement**: Negation simply flips signs of all trits
- **Natural zero**: Zero is represented by all trits = 0
- **Efficient**: Some algorithms run faster in balanced ternary

### The `tryte` Type

A **tryte** is 10 trits packed together, representing numbers from `-29524` to `+29524`:

```
tryte = 10 trits = 3^10 values = 59,049 total values
Storage: uint16 (16 bits, using 14.87 bits effective)
```

#### Declaration and Usage

```aria
// Tryte declarations
tryte:small = 100;
tryte:negative = -5000;
tryte:max = 29524;    // Maximum value
tryte:min = -29524;   // Minimum value

// Arithmetic
tryte:a = 1000;
tryte:b = 2000;
tryte:sum = a + b;    // 3000

// Overflow wraps or produces error (TBD)
tryte:overflow = 20000 + 15000;  // Exceeds +29524
```

#### Internal Representation

```
Decimal 100 in balanced ternary:
= 10201 (base 3, balanced)
= [1, 0, 2, 0, 1] trits
= [+1, 0, -1, 0, +1] (where 2 = -1 in balanced form)

Stored in uint16 with custom encoding
```

---

## Balanced Nonary System

### The `nit` Type

A **nit** (nonary digit) is a single balanced nonary digit with nine possible values:

```
-4, -3, -2, -1, 0, +1, +2, +3, +4
```

#### Declaration and Usage

```aria
// Single nit values
nit:neg_four = -4;
nit:zero = 0;
nit:pos_four = 4;

// Arithmetic
nit:sum = -2 + 3;  // 1
nit:diff = 4 - 2;  // 2
```

#### Why Balanced Nonary?

Balanced nonary (base-9 with symmetric digits) offers:

- **Higher density**: 9 values per digit vs 3 for ternary
- **Still symmetric**: Centered around zero like balanced ternary
- **Base-9 efficiency**: Some calculations benefit from base-9
- **Compatibility**: 9 = 3^2, so relates to ternary naturally

### The `nyte` Type

A **nyte** is 5 nits packed together, representing numbers from `-29524` to `+29524`:

```
nyte = 5 nits = 9^5 values = 59,049 total values
Storage: uint16 (16 bits, using 15.88 bits effective)
```

**Interesting Property**: `tryte` and `nyte` have the **same range** (59,049 values) but different digit representations!

#### Declaration and Usage

```aria
// Nyte declarations
nyte:value = 1000;
nyte:negative = -10000;
nyte:max = 29524;   // Same range as tryte
nyte:min = -29524;

// Arithmetic
nyte:a = 5000;
nyte:b = 3000;
nyte:product = a + b;  // 8000
```

#### Internal Representation

```
Decimal 100 in balanced nonary:
= 5 nits representing 100
= Stored in uint16 with custom encoding
= Each nit is 4 bits (to hold -4 to +4)
```

---

## Comparison: tryte vs nyte

| Feature | tryte (10 trits) | nyte (5 nits) |
|---------|-----------------|---------------|
| **Base** | 3 | 9 |
| **Digits per value** | 10 | 5 |
| **Digit range** | {-1, 0, 1} | {-4, -3, -2, -1, 0, 1, 2, 3, 4} |
| **Total range** | -29524 to +29524 | -29524 to +29524 |
| **Storage** | uint16 | uint16 |
| **Total values** | 59,049 | 59,049 |
| **Bits per digit** | ~1.59 | ~3.17 |

**Key Insight**: Both represent the same numeric range but with different internal digit structures!

---

## Use Cases

### Balanced Ternary (trit/tryte)

#### 1. Quantum-Inspired Logic

```aria
// Representing qutrit states (3-level quantum)
trit:superposition = -1;  // |->
trit:ground = 0;          // |0>
trit:excited = 1;         // |+>

func:applyGate = trit(trit:state) {
    // Quantum-like operations
    pass(-state);  // NOT gate (flip sign)
};
```

#### 2. Error Correction

```aria
// Symmetric error codes
tryte:data = 12345;
tryte:checksum = computeChecksum(data);

// Error detection naturally handles +/- errors
```

#### 3. Numeric Algorithms

```aria
// Some numeric methods benefit from balanced ternary
tryte:value = balancedTernaryMultiply(a, b);
```

### Balanced Nonary (nit/nyte)

#### 1. Compact Encoding

```aria
// More compact than ternary for same range
nyte:compact = encodeData(largeValue);  // 5 digits instead of 10
```

#### 2. Base-9 Arithmetic

```aria
// Algorithms optimized for base-9
nyte:result = base9Transform(input);
```

#### 3. Compatibility with Ternary

```aria
// Since 9 = 3^2, can convert efficiently
nyte:nonary = 1000;
tryte:ternary = nonaryToTernary(nonary);
```

---

## Conversion Functions

### Between Balanced Types

```aria
// trit ↔ nit (single digit)
nit:from_trit = tritToNit(my_trit);
trit:from_nit = nitToTrit(my_nit);  // May lose precision

// tryte ↔ nyte (multi-digit, same range)
nyte:from_tryte = tryteToNyte(my_tryte);
tryte:from_nyte = nyteToTryte(my_nyte);
```

### To/From Standard Integers

```aria
// From int64
int64:standard = 12345;
tryte:as_tryte = int64ToTryte(standard);  // May overflow
nyte:as_nyte = int64ToNyte(standard);    // May overflow

// To int64
tryte:balanced = 10000;
int64:converted = tryteToInt64(balanced);  // Always safe
```

---

## Arithmetic Operations

### Supported Operations

| Operation | tryte | nyte | Behavior |
|-----------|-------|------|----------|
| Addition (`+`) | ✅ | ✅ | Wraps or errors on overflow |
| Subtraction (`-`) | ✅ | ✅ | Wraps or errors on underflow |
| Multiplication (`*`) | ✅ | ✅ | Wraps or errors on overflow |
| Division (`/`) | ✅ | ✅ | Balanced division |
| Modulo (`%`) | ✅ | ✅ | Balanced modulo |
| Negation (`-x`) | ✅ | ✅ | Simply flip all digit signs |
| Comparison | ✅ | ✅ | Standard ordering |

### Negation Efficiency

Balanced types make negation trivial:

```aria
tryte:positive = 12345;
tryte:negative = -positive;  // Just flip signs of internal trits

// Compare to two's complement:
// - Requires invert all bits + add 1
// - Balanced: just flip signs!
```

---

## Common Patterns

### Pattern 1: Quantum State Simulation

```aria
// Represent qutrit (3-level quantum) states
trit:qutrit_state[100];  // Array of 100 qutrits

func:applyHadamard = trit(trit:state) {
    // Balanced ternary makes quantum gates elegant
    if (state == 0) {
        pass(1);
    } else if (state == 1) {
        pass(-1);
    } else {
        pass(0);
    }
};
```

### Pattern 2: Efficient Negation

```aria
func:invertSign = tryte(tryte:value) {
    pass(-value);  // O(1) negation!
};

// vs two's complement:
// func invertSign(int16 value) {
//     return ~value + 1;  // Two operations
// }
```

### Pattern 3: Compact Data Storage

```aria
struct:CompactData = {
    nyte:field1;  // 16 bits, range -29524 to +29524
    nyte:field2;
    nyte:field3;
};

// 3 nytes = 3 * 16 = 48 bits for 3 balanced nonary values
// vs 3 int16 = same storage but different semantics
```

---

## Gotchas and Limitations

### ❌ Limited Range

```aria
// WRONG: Assuming int64 range
tryte:large = 100000;  // ❌ OVERFLOW! Max is 29524

// CORRECT: Check range
int64:value = 100000;
if (value > 29524 || value < -29524) {
    stderr.write("Value out of tryte range");
    fail(1);
}
tryte:safe = value;
```

### ❌ Not All Values Use All Digits

```aria
// tryte has 10 trits but not all combinations are valid for every value
// Some values have leading zeros (just like decimal)

tryte:small = 5;
// Internal: 0000000011 (balanced ternary)
//           ^^^^^^^^^^ leading zeros
```

### ✅ Negation is Free

```aria
// Take advantage of efficient negation
tryte:value = 10000;
tryte:negative = -value;  // Trivial operation

// This is much faster than two's complement negation
```

---

## Implementation Details

### Storage Format (IMPORTANT)

Both `tryte` and `nyte` use `uint16` for storage:

```aria
// Internal representation
tryte: uint16:storage;  // 16 bits
       // Encodes 10 trits (3^10 = 59049 values)
       
nyte: uint16:storage;   // 16 bits
      // Encodes 5 nits (9^5 = 59049 values)
```

### Encoding Scheme

The exact encoding scheme (how trits/nits map to bits) is implementation-defined but must:

1. Support full range: -29524 to +29524
2. Enable efficient arithmetic operations
3. Make negation O(1) operation
4. Preserve balanced digit semantics

---

## Related Topics

- [tbb8 Type](tbb8.md) - Twisted Balanced Binary with ERR sentinels
- [int Types](int64.md) - Standard binary integers
- [Type Conversion](../advanced_features/type_conversion.md) - Converting between types
- [Arithmetic Operators](../operators/arithmetic.md) - Math operations

---

## References

- **Balanced Ternary**: https://en.wikipedia.org/wiki/Balanced_ternary
- **Ternary Computing**: Historical use in Soviet Setun computer
- **Nonary System**: https://en.wikipedia.org/wiki/Nonary

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Lines 37-40  
**Non-Negotiable**: Digit sets and storage format are fixed language features


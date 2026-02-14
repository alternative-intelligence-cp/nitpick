# Balanced Ternary (trit, tryte)

**Category**: Types → Integers → Balanced Numbers  
**Base**: Ternary (base 3)  
**Digits**: -1, 0, +1 (T, 0, 1)  
**Purpose**: Three-state logic, cognitive primitives, alternative thinking  
**Status**: 🚧 EXPERIMENTAL (Q3/Q9 cognitive substrate)

---

## Table of Contents

1. [Overview](#overview)
2. [What is Balanced Ternary?](#what-is-balanced-ternary)
3. [trit: Single Ternary Digit](#trit-single-ternary-digit)
4. [tryte: 6-trit Word](#tryte-6-trit-word)
5. [Why Balanced Ternary is Elegant](#why-balanced-ternary-is-elegant)
6. [Arithmetic Operations](#arithmetic-operations)
7. [Comparison to Binary](#comparison-to-binary)
8. [Connection to Q3 Thinking](#connection-to-q3-thinking)
9. [Notation and Representation](#notation-and-representation)
10. [Use Cases](#use-cases)
11. [Implementation Considerations](#implementation-considerations)
12. [Common Patterns](#common-patterns)
13. [Educational Value](#educational-value)
14. [Related Concepts](#related-concepts)

---

## Overview

Aria's **trit** and **tryte** types implement **balanced ternary** - a number system using three digits: **-1, 0, +1** instead of binary's 0, 1.

**Key Properties**:
- **Three-state logic**: Negative, zero, positive (not just on/off)
- **Symmetric around zero**: -1 and +1 equally distant from 0
- **No separate sign bit**: Sign is inherent in digits
- **Elegant negation**: Flip all digits (T→1, 0→0, 1→T)
- **Foundation for Q3**: Three-state cognitive primitive (no/maybe/yes)

**⚠️ EXPERIMENTAL**: Balanced ternary is rare in modern computing but elegant for cognitive modeling.

---

## What is Balanced Ternary?

### Traditional Ternary (Unbalanced)

**Digits**: 0, 1, 2  
**Example**: 12₃ = 1×3¹ + 2×3⁰ = 3 + 2 = 5₁₀

### Balanced Ternary

**Digits**: -1, 0, +1 (often written as **T**, **0**, **1**)  
**Example**: 1T₃ = 1×3¹ + (-1)×3⁰ = 3 - 1 = 2₁₀

**Why "balanced"**: Symmetric around zero (negative and positive digits)

### Digit Notation

| Symbol | Value | Name |
|--------|-------|------|
| **T** | -1 | Negative trit (sometimes **-** or **N**) |
| **0** | 0 | Zero trit |
| **1** | +1 | Positive trit (sometimes **+** or **P**) |

**Aria uses**: `T` for -1, `0` for 0, `1` for +1

---

## trit: Single Ternary Digit

### Definition

A **trit** (ternary digit) is the fundamental unit of balanced ternary, holding one of three values:

```aria
trit:negative = T;  // -1
trit:zero = 0;      // 0
trit:positive = 1;  // +1
```

### Range

**Values**: -1, 0, +1  
**Total states**: 3

### Declaration

```aria
trit:value = 1;      // Positive
trit:neutral = 0;    // Zero
trit:negated = T;    // Negative
```

### Use Cases

1. **Three-state logic**: Yes/No/Maybe, True/False/Unknown
2. **Q3 foundation**: Cognitive primitive for gradient thinking
3. **Sign representation**: Negative/Zero/Positive (no extra bit)
4. **Educational**: Teaching alternative number bases

### Example: Three-State Logic

```aria
func:compare_values = (a: int32, b: int32) -> trit {
    if (a < b) return T;      // -1: a is less
    if (a == b) return 0;     // 0: equal
    return 1;                  // +1: a is greater
}

int32:x = 10i32;
int32:y = 20i32;

trit:result = compare_values(x, y);
// result = T (-1, x < y)
```

---

## tryte: 6-trit Word

### Definition

A **tryte** is a **6-trit word** (analogous to an 8-bit byte). It's the standard unit of balanced ternary computation.

**Why 6 trits**:
- 3⁶ = 729 values (comparable to byte's 256)
- Range: -364 to +364
- Symmetric around zero
- Efficient for ternary arithmetic

### Range

**Minimum**: -364 (TTTTTT₃)  
**Maximum**: +364 (111111₃)  
**Total values**: 729 (3⁶)

**Calculation**:
```
Maximum = 1×3⁵ + 1×3⁴ + 1×3³ + 1×3² + 1×3¹ + 1×3⁰
        = 243 + 81 + 27 + 9 + 3 + 1
        = 364

Minimum = -364 (all trits negative)
```

### Declaration

```aria
tryte:value = 0T1T01;     // Explicit tryte literal
tryte:positive = 111111;  // +364 (maximum)
tryte:negative = TTTTTT;  // -364 (minimum)
tryte:zero = 000000;      // 0
```

### Conversion to Decimal

```aria
// Convert tryte to int32
func:tryte_to_int = (t: tryte) -> int32 {
    int32:result = 0i32;
    int32:power = 1i32;
    
    // Process each trit from right to left
    till 6 loop:i
        trit:digit = t.get_trit(i);
        
        if (digit == T) {
            result -= power;
        } else if (digit == 1) {
            result += power;
        }
        // digit == 0: add nothing
        
        power *= 3i32;
    end
    
    pass(result);
}

tryte:example = 1T0;  // 1×9 + (-1)×3 + 0×1 = 9 - 3 = 6
int32:decimal = tryte_to_int(example);  // 6
```

---

## Why Balanced Ternary is Elegant

### 1. No Separate Sign Bit

**Binary**: Needs extra bit for sign (two's complement, sign-magnitude)  
**Balanced ternary**: Sign is inherent in digits

```aria
// Binary int8: -5 = 11111011 (two's complement, sign bit needed)
// Balanced ternary: -5 = 1TT (1×9 - 1×3 - 1×1 = 9 - 3 - 1 = 5, negated)
```

### 2. Trivial Negation

**Binary**: Complex (invert bits, add 1)  
**Balanced ternary**: Flip each trit (T↔1, 0→0)

```aria
tryte:positive = 10T1;   // Some value
tryte:negative = T01T;   // Negation: flip all trits

// 10T1 = 1×81 + 0×27 + (-1)×9 + 1×3 = 81 - 9 + 3 = 75
// T01T = (-1)×81 + 0×27 + 1×9 + (-1)×3 = -81 + 9 - 3 = -75  ✓
```

### 3. Symmetric Around Zero

**Binary**: Asymmetric (int8: -128 to +127)  
**Balanced ternary**: Perfectly symmetric (tryte: -364 to +364)

**No abs(MIN) problem**: Negation always works!

### 4. Natural Rounding

**Truncation rounds toward zero** (not down like binary):

```aria
// Binary: 7 / 2 = 3 (truncate toward zero, but -7 / 2 = -3, not symmetric!)
// Balanced ternary: Always rounds toward zero symmetrically

tryte:a = 10;   // 10 in decimal
tryte:result = a / 2;  // 5 (rounds toward zero)

tryte:b = T0;   // -10 in decimal (-1×27 + 0×9 + 1×3 = -27+3 = ... wait, let me recalculate)
// Actually: T0 = (-1)×3 + 0×1 = -3
```

### 5. Three-State Logic Native

**Binary**: Only on/off (must simulate "unknown" separately)  
**Balanced ternary**: Three states built-in (negative/zero/positive)

Perfect for:
- Less than / Equal / Greater than
- No / Maybe / Yes
- Disagree / Neutral / Agree

---

## Arithmetic Operations

### Addition

**Rule**: Add trits digit-by-digit with carry

**Carry table**:
```
 T + T = 1T (value -2: carry T, digit 1)
 T + 0 = T
 T + 1 = 0
 0 + 0 = 0
 0 + 1 = 1
 1 + 1 = 1T (value 2: carry 1, digit T)
```

**Example**: 1T + 10 (6 + 3 = 9)
```
   1 T
+  1 0
------
  1 0 0  (1×9 + 0×3 + 0×1 = 9) ✓
```

### Subtraction

**Subtract = Add negation**:

```aria
tryte:a = 10T;
tryte:b = 1T;
tryte:result = a - b;  // Same as: a + (-b) = a + negate(b)
```

### Multiplication

**Binary-style shift-and-add**, but with three cases:
- Multiply by T: Shift and subtract
- Multiply by 0: Add nothing
- Multiply by 1: Shift and add

### Division

**Similar to binary long division**, but with ternary digits.

---

## Comparison to Binary

| Feature | Binary (byte) | Balanced Ternary (tryte) |
|---------|---------------|--------------------------|
| **Digits** | 0, 1 | T, 0, 1 |
| **Digits per unit** | 8 bits | 6 trits |
| **Range (unsigned)** | 0 to 255 | N/A (always signed) |
| **Range (signed)** | -128 to +127 (asymmetric) | -364 to +364 (symmetric) |
| **Total values** | 256 (2⁸) | 729 (3⁶) |
| **Sign representation** | Extra bit (two's complement) | Inherent in digits |
| **Negation** | Invert + add 1 | Flip each trit |
| **Rounding** | Truncate toward zero (but asym.) | Truncate toward zero (symmetric) |
| **Hardware** | Ubiquitous | Rare (experimental) |

**Efficiency**:
- Binary: log₂(N) bits for N values
- Balanced ternary: log₃(N) trits for N values
- **Ternary is ~1.58× more efficient per digit** (but harder in hardware)

**Example**: Represent 100₁₀
- Binary: 1100100 (7 bits)
- Balanced ternary: 10T01 (5 trits, but harder to implement)

---

## Connection to Q3 Thinking

### Q3 Cognitive Primitive

**Q3<T>** uses three-state logic:
- **Hypothesis A** (negative direction)
- **Unknown/Neutral** (zero)
- **Hypothesis B** (positive direction)

**Trit is the foundation**:
```aria
trit:confidence = 0;  // Unknown

// Evidence for hypothesis A
confidence = T;  // Leaning toward A

// Evidence for hypothesis B
confidence = 1;  // Leaning toward B
```

### Three-State Decision Making

**Binary thinking**: Yes or No (forced choice)  
**Ternary thinking**: Yes, No, or Maybe (reality in between)

```aria
func:should_proceed = (risk: int32, reward: int32) -> trit {
    if (reward > risk * 2i32) return 1;    // Yes, proceed
    if (risk > reward * 2i32) return T;    // No, too risky
    return 0;                               // Maybe, need more data
}
```

### Educational: Alternative Thinking

Teaching balanced ternary shows:
- **Not everything is binary** (world has gradients)
- **Multiple valid number systems** exist
- **Elegant mathematical properties** (symmetric, simple negation)
- **Foundation for three-state logic** (human reasoning often ternary)

---

## Notation and Representation

### Trit Literals

```aria
trit:neg = T;   // -1
trit:zero = 0;  // 0
trit:pos = 1;   // +1
```

### Tryte Literals

```aria
tryte:value1 = 10T101;     // Right-aligned (6 trits)
tryte:value2 = 001T00;     // Leading zeros optional
tryte:max = 111111;        // +364
tryte:min = TTTTTT;        // -364
```

### String Representation

```aria
tryte:value = 1T01;
string:representation = value.to_string();  // "1T0100" (6 trits with leading zeros)

log.write(representation);
log.write("\n");
```

### Decimal Conversion

```aria
tryte:ternary = 10T;
int32:decimal = int32(ternary);  // Convert to decimal

log.write("Ternary: ");
ternary.write_to(log);
log.write(" = Decimal: ");
decimal.write_to(log);
log.write("\n");
```

---

## Use Cases

### 1. Three-State Logic (Comparison)

```aria
func:three_way_compare = (a: int32, b: int32) -> trit {
    if (a < b) return T;  // Less than
    if (a > b) return 1;  // Greater than
    return 0;              // Equal
}

trit:result = three_way_compare(10i32, 20i32);  // T (less than)
```

### 2. Sentiment Analysis

```aria
func:analyze_sentiment = (text: string) -> trit {
    int32:positive_words = count_positive(text);
    int32:negative_words = count_negative(text);
    
    if (negative_words > positive_words * 2i32) return T;  // Negative
    if (positive_words > negative_words * 2i32) return 1;  // Positive
    return 0;  // Neutral
}

string:review = "The product is okay, nothing special.";
trit:sentiment = analyze_sentiment(review);  // 0 (neutral)
```

### 3. Q3 Foundation (Evidence Accumulation)

```aria
// Q3 simulation with tryte as confidence tracker
tryte:confidence = 000000;  // Start neutral

// Accumulate evidence
func:add_evidence = (current: tryte, evidence: trit) -> tryte {
    return current + evidence;  // Add trit to tryte
}

confidence = add_evidence(confidence, 1);   // Evidence for B
confidence = add_evidence(confidence, T);   // Evidence for A
confidence = add_evidence(confidence, 1);   // Evidence for B

// confidence now leans toward B
```

### 4. Teaching Alternative Number Systems

```aria
// Show kids balanced ternary elegance
func:demonstrate_negation = () -> void {
    tryte:number = 10T;
    
    log.write("Original: ");
    number.write_to(log);
    log.write(" = ");
    int32(number).write_to(log);
    log.write("\n");
    
    // Negate by flipping trits
    tryte:negated = -number;  // Flip: 1→T, 0→0, T→1
    
    log.write("Negated: ");
    negated.write_to(log);
    log.write(" = ");
    int32(negated).write_to(log);
    log.write("\n");
}
```

---

## Implementation Considerations

### Storage

**Efficient storage**: 3 states per trit, 6 trits per tryte = 729 values

**Option 1**: Use 2 bits per trit (4 states, waste 1)
- tryte = 12 bits (6 trits × 2 bits)

**Option 2**: Pack into byte
- 8 bits can hold ~5.04 trits (2⁸ = 256, 3⁵ = 243)
- Requires bit manipulation

**Option 3**: Use byte per tryte (wasteful but simple)
- Map -364 to +364 onto 0-728, store in uint16

### Hardware Challenges

**Binary hardware dominance**: Modern CPUs use binary logic

**Ternary emulation**: Must simulate with binary (overhead)

**Future potential**: Ternary computing research exists (quantum, memristors)

### Performance

**Emulated ternary is slower** than native binary

**Use cases**:
- Educational (learning alternative systems)
- Cognitive modeling (three-state logic)
- Q3/Q9 foundation (gradient thinking)
- NOT for performance-critical code (unless native ternary hardware)

---

## Common Patterns

### Three-Way Comparison

```aria
func:compare = (a: int32, b: int32) -> trit {
    return (a > b) ? 1 : ((a < b) ? T : 0);
}
```

### Accumulating Evidence (Q3 Simulation)

```aria
tryte:evidence = 0;

func:process_test = (test_result: bool, hypothesis: trit) -> void {
    if (test_result) {
        evidence += hypothesis;  // Add evidence
    } else {
        evidence -= hypothesis;  // Subtract (opposite evidence)
    }
}

// Use
process_test(true, 1);   // Evidence for B
process_test(false, T);  // Evidence for B (test failed for A)
```

### Balanced Rounding

```aria
func:balanced_round = (value: int32, divisor: int32) -> int32 {
    int32:quotient = value / divisor;
    int32:remainder = value % divisor;
    
    // Round based on magnitude of remainder
    if (remainder > divisor / 2i32) {
        quotient += 1i32;
    } else if (remainder < -divisor / 2i32) {
        quotient -= 1i32;
    }
    
    pass(quotient);
}
```

---

## Educational Value

### Teaching Alternative Number Systems

**Beyond binary**: Shows kids multiple valid approaches

**Elegant properties**:
- Symmetric negation (flip digits)
- No sign bit needed
- Natural three-state logic

### Cognitive Development

**Three-state thinking**:
- Yes/No/Maybe (not forced binary)
- Positive/Negative/Neutral (sentiment)
- More/Equal/Less (comparison)

**Real-world mapping**: Reality often has gradients, not absolutes

### Foundation for Q3

Understanding balanced ternary prepares for:
- Q3<T> quantum types
- Gradient reasoning
- Evidence-based confidence
- Alternative Intelligence thinking

---

## Related Concepts

### Other Balanced Number Types

- **nit, nyte**: Balanced nonary (base 9, foundation for Q9)
- **Q3<T>**: Three-state quantum type (no/maybe/yes)
- **Q9<T>**: Nine-state quantum type (multi-level confidence)

### Binary Types

- **int8, int16, int32, int64**: Standard binary integers
- **uint8, uint16, uint32, uint64**: Unsigned binary integers
- **tbb8, tbb16, tbb32, tbb64**: Twisted Balanced Binary (symmetric ranges)

### Special Values

- **ERR**: Error sentinel (TBB propagation)
- **NIL**: Optional type sentinel
- **NULL**: Pointer sentinel

---

## Summary

**trit** and **tryte** implement balanced ternary in Aria:

✅ **Three-state logic**: -1, 0, +1 (not just binary on/off)  
✅ **Symmetric around zero**: No asymmetric range problems  
✅ **Elegant negation**: Flip digits (no complex two's complement)  
✅ **Q3 foundation**: Cognitive primitive for gradient thinking  
✅ **Educational**: Teaching alternative number systems  

⚠️ **Experimental**: Rare in modern computing (binary hardware dominance)  
⚠️ **Performance cost**: Emulated on binary hardware (slower)  
⚠️ **Novel concept**: Requires learning new mental model  

**Use trit/tryte for**:
- Three-state logic (yes/no/maybe)
- Q3 cognitive primitives
- Teaching alternative number systems
- Sentiment analysis (positive/neutral/negative)
- Comparison operations (less/equal/greater)

**Avoid for**:
- Performance-critical code (use binary)
- Standard arithmetic (use int32/int64)
- Hardware interfacing (binary dominant)

---

**Next**: [Balanced Nonary (nit, nyte)](nit_nyte.md) - Foundation for Q9  
**See Also**: [Q3 Quantum Types](q3_quantum.md), [Three-State Logic](three_state_logic.md)

---

*Aria Language Project - Balanced Ternary Alternative Thinking*  
*February 12, 2026 - Cognitive primitives for Alternative Intelligence*

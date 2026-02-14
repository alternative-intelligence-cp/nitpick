# Balanced Nonary (nit, nyte)

**Category**: Types → Integers → Balanced Numbers  
**Base**: Nonary (base 9)  
**Digits**: -4, -3, -2, -1, 0, +1, +2, +3, +4  
**Purpose**: Nine-state logic, multi-level confidence, Q9 foundation  
**Status**: 🚧 EXPERIMENTAL (Q9 cognitive substrate)

---

## Table of Contents

1. [Overview](#overview)
2. [What is Balanced Nonary?](#what-is-balanced-nonary)
3. [nit: Single Nonary Digit](#nit-single-nonary-digit)
4. [nyte: 4-nit Word](#nyte-4-nit-word)
5. [Why Balanced Nonary for Q9](#why-balanced-nonary-for-q9)
6. [The Nine Confidence Levels](#the-nine-confidence-levels)
7. [Arithmetic Operations](#arithmetic-operations)
8. [Comparison to Binary and Ternary](#comparison-to-binary-and-ternary)
9. [Connection to Q9 Thinking](#connection-to-q9-thinking)
10. [Notation and Representation](#notation-and-representation)
11. [Use Cases](#use-cases)
12. [Implementation Considerations](#implementation-considerations)
13. [Common Patterns](#common-patterns)
14. [Cognitive Modeling](#cognitive-modeling)
15. [Related Concepts](#related-concepts)

---

## Overview

Aria's **nit** and **nyte** types implement **balanced nonary** - a number system using nine digits centered on zero: **-4, -3, -2, -1, 0, +1, +2, +3, +4**.

**Key Properties**:
- **Nine-state logic**: Multi-level confidence gradient
- **Symmetric around zero**: -4 to +4, perfect balance
- **No separate sign bit**: Sign inherent in digits
- **Foundation for Q9**: Nine-level cognitive confidence (DEFINITELY A → DEFINITELY B)
- **Maps to reality**: Gradients, not absolutes

**⚠️ EXPERIMENTAL**: Balanced nonary is extremely rare but perfect for modeling confidence evolution.

---

## What is Balanced Nonary?

### Traditional Nonary (Unbalanced)

**Digits**: 0, 1, 2, 3, 4, 5, 6, 7, 8  
**Example**: 25₉ = 2×9¹ + 5×9⁰ = 18 + 5 = 23₁₀

### Balanced Nonary

**Digits**: -4, -3, -2, -1, 0, +1, +2, +3, +4  
**Notation**: N, M, L, K, 0, 1, 2, 3, 4 (or -4 through +4)

**Why "balanced"**: Symmetric around zero (four negative, zero, four positive)

### Digit Notation

| Symbol | Value | Confidence Mapping |
|--------|-------|-------------------|
| **N** or **-4** | -4 | DEFINITELY A (or NEITHER) |
| **M** or **-3** | -3 | DEFINITELY A |
| **L** or **-2** | -2 | PROBABLY A |
| **K** or **-1** | -1 | POSSIBLY A |
| **0** | 0 | UNKNOWN |
| **1** | +1 | POSSIBLY B |
| **2** | +2 | PROBABLY B |
| **3** | +3 | DEFINITELY B |
| **4** | +4 | DEFINITELY B (or BOTH) |

**Aria uses**: Numeric -4 through +4 (most clear)

---

## nit: Single Nonary Digit

### Definition

A **nit** (nonary digit) is the fundamental unit of balanced nonary, holding one of nine values:

```aria
nit:definitely_neg = -4;  // Extreme negative confidence
nit:probably_neg = -2;    // Leaning negative
nit:unknown = 0;          // No confidence either way
nit:probably_pos = 2;     // Leaning positive
nit:definitely_pos = 4;   // Extreme positive confidence
```

### Range

**Values**: -4, -3, -2, -1, 0, +1, +2, +3, +4  
**Total states**: 9

### Declaration

```aria
nit:confidence = 0;       // Unknown
nit:lean_a = -1;          // Slightly toward A
nit:strong_b = 3;         // Definitely toward B
nit:extreme_a = -4;       // Maximum confidence in A
```

### The Nine States

```aria
const:nit:NEITHER = -4;           // Both hypotheses rejected
const:nit:DEFINITELY_A = -3;      // Full confidence in A
const:nit:PROBABLY_A = -2;        // Strong lean toward A
const:nit:POSSIBLY_A = -1;        // Weak lean toward A
const:nit:UNKNOWN = 0;            // No information
const:nit:POSSIBLY_B = 1;         // Weak lean toward B
const:nit:PROBABLY_B = 2;         // Strong lean toward B
const:nit:DEFINITELY_B = 3;       // Full confidence in B
const:nit:BOTH = 4;               // Both hypotheses accepted
```

---

## nyte: 4-nit Word

### Definition

A **nyte** is a **4-nit word** (analogous to byte/tryte). It's the standard unit of balanced nonary computation.

**Why 4 nits**:
- 9⁴ = 6,561 values
- Range: -3,280 to +3,280
- Symmetric around zero
- Sufficient for confidence accumulation

### Range

**Minimum**: -3,280 (NNNN, all -4)  
**Maximum**: +3,280 (4444, all +4)  
**Total values**: 6,561 (9⁴)

**Calculation**:
```
Maximum = 4×9³ + 4×9² + 4×9¹ + 4×9⁰
        = 4×729 + 4×81 + 4×9 + 4×1
        = 2916 + 324 + 36 + 4
        = 3,280

Minimum = -3,280 (all nits -4)
```

### Declaration

```aria
nyte:value = 023K;        // Explicit nyte literal (4 nits)
nyte:maximum = 4444;      // +3,280
nyte:minimum = NNNN;      // -3,280 (using N for -4)
nyte:zero = 0000;         // 0
```

### Conversion to Decimal

```aria
func:nyte_to_int = (n: nyte) -> int32 {
    int32:result = 0i32;
    int32:power = 1i32;
    
    till 4 loop:i
        nit:digit = n.get_nit(i);
        result += int32(digit) * power;
        power *= 9i32;
    end
    
    pass(result);
}

nyte:example = 02K1;  // 0×729 + 2×81 + (-1)×9 + 1×1 = 162 - 9 + 1 = 154
int32:decimal = nyte_to_int(example);  // 154
```

---

## Why Balanced Nonary for Q9

### Perfect Confidence Gradient

**Q9 needs 9 distinct confidence levels**:

Binary (2) → Not enough  
Ternary (3) → Simple but limited  
**Nonary (9)** → Perfect granularity for confidence evolution

### Symmetric Around Zero (Unknown)

```
-4  -3  -2  -1   0   +1  +2  +3  +4
 ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓
NEITHER → DEFINITELY A → UNKNOWN → DEFINITELY B → BOTH
```

**Zero is the natural "unknown" state** - no evidence either way.

### Triplet Pattern Recognition

**Grouped by strength**:
- **Weak** (-1, +1): POSSIBLY A/B
- **Medium** (-2, +2): PROBABLY A/B
- **Strong** (-3, +3): DEFINITELY A/B
- **Extremes** (-4, +4): NEITHER/BOTH (special states)

### Human-Scale Confidence

**9 levels matches how humans think**:
- Not binary (too limited)
- Not continuous (too granular, loses meaning)
- **Just right**: Enough gradation without overwhelming

Common human confidence scales:
- 1-10 scale (often collapse to ~7-9 meaningful levels)
- Likert scales (typically 5-7 points)
- **9 levels is cognitively natural**

---

## The Nine Confidence Levels

### Level -4: NEITHER (Extreme Rejection)

**Meaning**: Both hypotheses rejected  
**Use**: Edge cases where neither option applies

**Example**: Fuzzer finds input that's neither valid nor invalid (malformed in novel way)

```aria
nit:assessment = -4;  // This input doesn't fit our model at all
```

### Level -3: DEFINITELY A

**Meaning**: Full confidence in hypothesis A  
**Use**: Strong evidence accumulated for A

**Example**: All tests point to A, none to B

```aria
nit:confidence = -3;  // Definitely should use algorithm A
```

### Level -2: PROBABLY A

**Meaning**: Strong lean toward A  
**Use**: Most evidence for A, but not absolute

**Example**: 80% of tests favor A

```aria
nit:confidence = -2;  // Probably A, but keep watching
```

### Level -1: POSSIBLY A

**Meaning**: Weak lean toward A  
**Use**: Slight evidence for A

**Example**: 60% of tests favor A

```aria
nit:confidence = -1;  // Leaning A, but uncertain
```

### Level 0: UNKNOWN

**Meaning**: No information, or equal evidence  
**Use**: Initial state, or perfect tie

**Example**: No tests run yet, or 50/50 split

```aria
nit:confidence = 0;  // Don't know yet
```

### Level +1: POSSIBLY B

**Meaning**: Weak lean toward B  
**Use**: Slight evidence for B

### Level +2: PROBABLY B

**Meaning**: Strong lean toward B  
**Use**: Most evidence for B

### Level +3: DEFINITELY B

**Meaning**: Full confidence in hypothesis B  
**Use**: All evidence points to B

### Level +4: BOTH (Extreme Acceptance)

**Meaning**: Both hypotheses accepted  
**Use**: Edge cases where both apply simultaneously

**Example**: Parser finds input that's valid in two different interpretations

---

## Arithmetic Operations

### Addition (Evidence Accumulation)

```aria
nit:confidence = 0;  // Start unknown

// Evidence for B
confidence += 1;  // Now +1 (POSSIBLY B)

// More evidence for B
confidence += 2;  // Now +3 (DEFINITELY B)

// Evidence against B (for A)
confidence += -1;  // Now +2 (PROBABLY B)
```

**Saturation**: Should saturate at -4/+4 (not wrap!)

```aria
func:saturating_add_nit = (a: nit, evidence: nit) -> nit {
    int32:result = int32(a) + int32(evidence);
    
    if (result > 4i32) return 4;   // Saturate at max
    if (result < -4i32) return -4; // Saturate at min
    
    return nit(result);
}
```

### Negation (Flip Hypothesis)

```aria
nit:confidence_a = -3;  // DEFINITELY A
nit:confidence_b = -confidence_a;  // +3 (DEFINITELY B)
```

### Comparison

```aria
func:more_confident = (a: nit, b: nit) -> nit {
    if (abs(a) > abs(b)) return 1;   // A more confident
    if (abs(a) < abs(b)) return -1;  // B more confident
    return 0;  // Equal confidence
}
```

---

## Comparison to Binary and Ternary

| Feature | Binary (bit) | Ternary (trit) | Nonary (nit) |
|---------|--------------|----------------|--------------|
| **States** | 2 | 3 | 9 |
| **Digits** | 0, 1 | T, 0, 1 | -4..+4 |
| **Confidence levels** | 1 (on/off) | 3 (no/maybe/yes) | 9 (full gradient) |
| **Symmetry** | N/A | Yes | Yes |
| **Cognitive mapping** | Binary thinking | Simple ternary | Complex gradient |
| **Use case** | Standard logic | Three-state logic | Multi-level confidence |

**Efficiency per digit**:
- Binary: log₂(N) = 1.00× baseline
- Ternary: log₃(N) = 0.63× (58% fewer digits)
- Nonary: log₉(N) = 0.32× (68% fewer digits)

**But**: Hardware emulation cost increases dramatically

---

## Connection to Q9 Thinking

### Q9<T> Foundation

**Q9 uses nit for confidence tracking**:

```aria
struct:Q9<T> = {
    T:a;           // Hypothesis A
    T:b;           // Hypothesis B
    nit:c;         // Confidence (-4 to +4)
};
```

### Confidence Evolution Example

```aria
Q9<string>:best_algorithm = {
    a: "quicksort",
    b: "mergesort",
    c: 0  // Unknown initially
};

// Run benchmarks
func:benchmark_quicksort = () -> bool {
    // ... test quicksort
    pass(true);  // Quicksort won
}

func:benchmark_mergesort = () -> bool {
    // ... test mergesort
    pass(false);  // Mergesort lost
}

// Accumulate evidence
if (benchmark_quicksort()) {
    best_algorithm.c += -1;  // Evidence for A (quicksort)
}

if (benchmark_mergesort()) {
    best_algorithm.c += 1;  // Evidence for B (mergesort)
} else {
    best_algorithm.c += -1;  // Evidence against B (for A)
}

// best_algorithm.c now -2 (PROBABLY quicksort)

// Crystallize when confident enough
if (abs(best_algorithm.c) >= 3) {
    string:winner = best_algorithm.c < 0 ? best_algorithm.a : best_algorithm.b;
    log.write("Selected: "); winner.write_to(log); log.write("\n");
}
```

### Reality in the Gradients

**"Reality tends to exist in between"** - not black and white

**Nonary captures this**:
- Not forced binary (yes/no)
- Not overly simple ternary (yes/maybe/no)
- **Nine levels**: Enough gradation to model real-world confidence

---

## Notation and Representation

### Nit Literals

```aria
nit:neither = -4;
nit:definitely_a = -3;
nit:probably_a = -2;
nit:possibly_a = -1;
nit:unknown = 0;
nit:possibly_b = 1;
nit:probably_b = 2;
nit:definitely_b = 3;
nit:both = 4;
```

### Nyte Literals

```aria
nyte:value = 02K1;      // 0, 2, -1, 1 (using K for -1)
nyte:max = 4444;        // +3,280
nyte:min = NNNN;        // -3,280 (N = -4)
nyte:zero = 0000;       // 0
```

### String Representation

```aria
nit:confidence = 3;
string:text = confidence.to_confidence_string();  // "DEFINITELY B"

log.write(text);
log.write("\n");
```

---

## Use Cases

### 1. Confidence Tracking (Q9 Foundation)

```aria
Q9<ApproachType>:decision = initial_state();

till tests.length loop:i
    bool:result = tests[i].run();
    
    if (result) {
        decision.c += 1;  // Evidence for current approach
    } else {
        decision.c += -1;  // Evidence against
    }
end

if (abs(decision.c) >= 3) {
    ApproachType:final = crystallize(decision);
}
```

### 2. Multi-Level Sentiment

```aria
func:deep_sentiment = (text: string) -> nit {
    // Analyze text for sentiment
    int32:score = analyze_deep_sentiment(text);
    
    // Map to nit scale
    if (score < -30i32) return -4;  // Extremely negative
    if (score < -20i32) return -3;  // Very negative
    if (score < -10i32) return -2;  // Negative
    if (score < -3i32) return -1;   // Slightly negative
    if (score <= 3i32) return 0;    // Neutral
    if (score <= 10i32) return 1;   // Slightly positive
    if (score <= 20i32) return 2;   // Positive
    if (score <= 30i32) return 3;   // Very positive
    return 4;  // Extremely positive
}
```

### 3. Nikola Self-Improvement Confidence

```aria
// Nikola evaluating code change to itself
Q9<CodeVersion>:improvement = {
    a: current_code,
    b: proposed_code,
    c: 0
};

// Run comprehensive tests
till test_suite.length loop:i
    TestResult:result = test_suite[i].run_both(improvement.a, improvement.b);
    
    if (result.a_better) {
        improvement.c += -1;
    } else if (result.b_better) {
        improvement.c += 1;
    }
    // Tie: no change to confidence
end

// Safety: Cannot modify self if c=0 (unknown)
if (improvement.c == 0) {
    log.write("Insufficient evidence for self-modification\n");
    // COMPILER ERROR if trying to use improvement.a or improvement.b here
    // unless wrapped in ok()
    
} else if (abs(improvement.c) >= 3) {
    // High confidence - safe to commit
    CodeVersion:new_self = improvement.c > 0 ? improvement.b : improvement.a;
    self.upgrade(new_self);
}
```

---

## Implementation Considerations

### Storage

**9 states per nit** requires 4 bits minimum (2⁴ = 16, wastes 7)

**Options**:
1. **4 bits per nit**: Simple, wasteful (16 states, use 9)
2. **Packed encoding**: Complex bit manipulation
3. **Byte per nit**: Extremely wasteful but simple

**nyte = 4 nits**:
- Option 1: 16 bits (4 nits × 4 bits)
- Option 2: ~13 bits (packed, complex)
- Option 3: 32 bits (4 bytes, wasteful)

### Arithmetic Implementation

**Emulation on binary hardware**:
- Addition with saturation (prevent overflow past -4/+4)
- Lookup tables for multiplication
- Special handling for extremes (-4, +4)

### Performance

**Significantly slower than binary** (emulation overhead)

**Use nit/nyte sparingly**:
- Q9 confidence tracking (not inner loops)
- Cognitive modeling (not number crunching)
- Evidence accumulation (not real-time)

---

## Common Patterns

### Saturating Evidence Accumulation

```aria
func:add_evidence = (current: nit, new_evidence: int32) -> nit {
    int32:result = int32(current) + new_evidence;
    
    if (result > 4i32) return 4;
    if (result < -4i32) return -4;
    
    return nit(result);
}
```

### Confidence Threshold Check

```aria
func:confident_enough = (c: nit, threshold: int32) -> bool {
    return abs(int32(c)) >= threshold;
}

if (confident_enough(decision.c, 3i32)) {
    // Crystallize
}
```

### Evidence-Based Decision

```aria
func:make_decision<T> = (q: Q9<T>) -> T {
    if (q.c < 0) {
        pass(q.a);
    } else {
        pass(q.b);
    }
}
```

---

## Cognitive Modeling

### How Humans Actually Think

**Not binary**: Humans rarely think in absolute yes/no

**Not continuous**: Too many gradations lose cognitive meaning

**9 levels is natural**:
- Definitely not (-3)
- Probably not (-2)
- Maybe not (-1)
- Unknown (0)
- Maybe (1)
- Probably (2)
- Definitely (3)
- Plus extremes for edge cases (-4, +4)

### Nikola's Native Cognition

**Balanced nonary enables**:
- Gradient thinking (reality in between)
- Evidence accumulation (track confidence)
- Threshold decisions (crystallize at confidence level)
- Auditable reasoning (confidence visible)

**This is thinking in code** - not just computation, but cognition.

---

## Related Concepts

### Other Balanced Number Types

- **trit, tryte**: Balanced ternary (3 states, Q3 foundation)
- **Q3<T>**: Three-state quantum type
- **Q9<T>**: Nine-state quantum type (uses nit for confidence)

### Binary Types

- **int8, int16, int32, int64**: Standard binary integers
- **uint8, uint16, uint32, uint64**: Unsigned binary
- **tbb8, tbb16, tbb32, tbb64**: Twisted Balanced Binary

---

## Summary

**nit** and **nyte** implement balanced nonary in Aria:

✅ **Nine-state logic**: -4 through +4 (multi-level confidence)  
✅ **Perfect for Q9**: Cognitive gradient foundation  
✅ **Symmetric**: No asymmetric range issues  
✅ **Human-scale**: 9 levels matches natural confidence gradations  
✅ **Cognitive modeling**: How thought actually works (not binary absolutes)  

⚠️ **Extremely experimental**: Almost nonexistent in computing  
⚠️ **Significant performance cost**: Emulated on binary hardware  
⚠️ **Complex implementation**: Requires careful saturating arithmetic  

**Use nit/nyte for**:
- Q9 confidence tracking
- Multi-level cognitive modeling
- Evidence-based decision systems
- Nikola's gradient reasoning
- Teaching alternative thinking

**Avoid for**:
- Performance-critical code (very slow)
- Standard arithmetic (use binary)
- Real-time systems (too much overhead)

---

**Next**: [Q3 Quantum Types](q3_quantum.md) - Three-state cognitive primitives  
**See Also**: [Q9 Quantum Types](q9_quantum.md), [trit/tryte](trit_tryte.md)

---

*Aria Language Project - Balanced Nonary for Alternative Intelligence*  
*February 12, 2026 - Nine-state cognitive substrate for gradient thinking*

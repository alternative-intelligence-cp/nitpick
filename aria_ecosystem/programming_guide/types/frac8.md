# frac8 - 8-bit Exact Rational Arithmetic

## Overview

`frac8` is an 8-bit **mixed-fraction rational number** that provides **mathematically exact** representation of rational values. Unlike floating-point types which approximate fractions, frac8 maintains **perfect precision** by storing values as whole numbers plus proper fractions (numerator/denominator).

**Why frac8 Exists**: Financial calculations, musical intervals, UI scaling factors, and other domains require **exact** arithmetic - where 1/3 means exactly 1/3, not 0.33333... Floating-point drift can corrupt these calculations over time, while frac8 maintains mathematical purity.

**Key Features**:
- **Exact rational arithmetic** (no floating-point drift)
- **Built on TBB** (Twisted Balanced Binary) for sticky ERR propagation
- **Automatic reduction** to lowest terms (maintains canonical form)
- **Mixed-number representation** (whole + num/denom)
- **Perfect determinism** across all platforms

**Size**: 3 bytes (24 bits)

**Structure**:
```aria
struct:frac8 = {
    tbb8:whole;   // Whole number part (range: [-127, +127])
    tbb8:num;     // Numerator (proper fraction: num < denom)
    tbb8:denom;   // Denominator (always positive, denom > 0)
}
```

**Mathematical Representation**:
```
value = whole + (num / denom)

Examples:
{3, 1, 2}  = 3 + 1/2 = 3.5 (exact!)
{0, 1, 3}  = 0 + 1/3 = 1/3 (mathematically exact, not 0.333...)
{-2, 1, 4} = -2 + 1/4 = -1.75 (sign in whole)
{0, -1, 2} = 0 + (-1/2) = -0.5 (sign in num when whole=0)
```

---

## Why Fractions Instead of Floats?

### The Floating-Point Precision Problem

Floating-point numbers **approximate** most fractions, leading to subtle errors:

```javascript
// Classic JavaScript problem (IEEE 754)
0.1 + 0.2 === 0.3  // false! (actually 0.30000000000000004)

let balance = 0.0;
for (let i = 0; i < 10; i++) {
    balance += 0.1;  // Add 10 cents
}
console.log(balance);  // 0.9999999999999999 (not 1.0!)
```

**With frac8 (Aria)**:
```aria
frac8:balance = {0, 0, 1};  // Start at 0

till 10 loop:i
    balance = balance + {0, 1, 10};  // Add 1/10 (exact)
end

// balance = {1, 0, 1} = 1 + 0/1 = 1.0 (mathematically exact!)
```

### Musical Intervals

Musical frequencies have exact fractional relationships:
```aria
// Perfect fifth = frequency ratio 3/2
frac8:perfect_fifth = {0, 3, 2};

// Major third = frequency ratio 5/4
frac8:major_third = {0, 5, 4};

// Combine intervals (multiply fractions exactly)
frac8:combined = perfect_fifth * major_third;
// = {0, 3, 2} * {0, 5, 4} = {0, 15, 8} (exact!)
// No pitch drift over time
```

### Division Without Remainder

```aria
// Share 5 items among 3 people
frac8:per_person = {5, 0, 1} / {3, 0, 1};
// = {1, 2, 3} = 1 2/3 items each (exact)

// Try with float32:
float32:per_person_approx = 5.0f / 3.0f;
// = 1.6666666 (approximation, loses precision)
```

---

## Memory Layout

### Structure Layout (3 bytes)

```
Offset  Field   Type   Size   Range
------  ------  -----  -----  ---------------------
0       whole   tbb8   1 B    [-127, +127] (ERR=-128)
1       num     tbb8   1 B    [-127, +127]
2       denom   tbb8   1 B    [-127, +127]
```

### LLVM IR Representation

```llvm
%frac8 = type { i8, i8, i8 }

; Create fraction 2 1/3 = {2, 1, 3}
%val = insertvalue %frac8 undef, i8 2, 0   ; whole = 2
%val2 = insertvalue %frac8 %val, i8 1, 1   ; num = 1
%val3 = insertvalue %frac8 %val2, i8 3, 2  ; denom = 3

; Extract whole part
%whole = extractvalue %frac8 %val3, 0
```

### Canonical Form Invariants

All frac8 values are **automatically normalized** to maintain:

1. **denom > 0** (denominator always positive)
2. **num ≥ 0 when whole ≠ 0** (sign lives in whole for mixed numbers)
3. **num < denom** (proper fraction, not improper)
4. **gcd(num, denom) = 1** (reduced to lowest terms)
5. **Sign handling**:
   - `whole ≠ 0`: Sign in `whole`, `num` stays positive
   - `whole = 0`: Sign in `num` (avoids -0 problem)

**Examples**:
```aria
// Input: {2, 5, 3} (improper: 5/3 > 1)
// Normalized: {3, 2, 3} (2 + 5/3 = 3 + 2/3)

// Input: {0, 6, 9} (not reduced: gcd(6,9) = 3)
// Normalized: {0, 2, 3} (6/9 = 2/3)

// Input: {1, 0, -5} (denom < 0)
// Normalized: {1, 0, 5} (flip signs to make denom positive)
```

---

## Arithmetic Operations

All operations produce **exact results** and maintain canonical form.

### Addition (Exact)

```aria
frac8:a = {1, 1, 3};  // 1 1/3
frac8:b = {0, 1, 6};  // 1/6

frac8:sum = a + b;
// Math: (1 + 1/3) + 1/6 = 4/3 + 1/6 = 8/6 + 1/6 = 9/6 = 3/2
// Result: {1, 1, 2} = 1 1/2 (exact!)
```

**Implementation**:
```
whole_sum = a.whole + b.whole
num_sum = (a.num * b.denom) + (b.num * a.denom)
denom_sum = a.denom * b.denom
result = normalize({whole_sum, num_sum, denom_sum})
```

### Subtraction (Exact)

```aria
frac8:a = {2, 1, 2};  // 2 1/2
frac8:b = {1, 1, 4};  // 1 1/4

frac8:diff = a - b;
// Math: 5/2 - 5/4 = 10/4 - 5/4 = 5/4
// Result: {1, 1, 4} = 1 1/4
```

### Multiplication (Exact)

```aria
frac8:a = {0, 2, 3};  // 2/3
frac8:b = {0, 3, 4};  // 3/4

frac8:product = a * b;
// Math: (2/3) * (3/4) = 6/12 = 1/2
// Result: {0, 1, 2} = 1/2 (automatically reduced)
```

**Implementation**:
```
// Convert to improper fractions
a_improper = (a.whole * a.denom) + a.num
b_improper = (b.whole * b.denom) + b.num

// Multiply
num_result = a_improper * b_improper
denom_result = a.denom * b.denom

// Normalize back to mixed number
result = normalize({0, num_result, denom_result})
```

### Division (Exact)

```aria
frac8:a = {2, 0, 1};  // 2
frac8:b = {0, 1, 3};  // 1/3

frac8:quotient = a / b;
// Math: 2 / (1/3) = 2 * 3 = 6
// Result: {6, 0, 1} = 6
```

**Division by Zero**:
```aria
frac8:a = {5, 0, 1};
frac8:b = {0, 0, 1};  // 0

frac8:result = a / b;  // ERR (division by zero)

if (result.whole == ERR) {
    stderr.write("ERROR: Division by zero!\n");
    !!! ERR_DIVISION_BY_ZERO;
}
```

### Negation

```aria
frac8:a = {2, 1, 3};   // 2 1/3
frac8:neg_a = -a;      // {-2, 1, 3} = -2 1/3

frac8:b = {0, 3, 4};   // 3/4
frac8:neg_b = -b;      // {0, -3, 4} = -3/4
```

---

## Comparison Operations

Comparisons convert to improper fractions for accurate ordering:

```aria
frac8:a = {1, 1, 2};  // 1 1/2 = 3/2
frac8:b = {0, 7, 4};  // 7/4 = 1 3/4

if (a < b) {  // true (3/2 < 7/4)
    stdout.write("a is less than b\n");
}
```

**Implementation**:
```
// Cross-multiply to compare without division
a_improper = (a.whole * a.denom) + a.num  // 3
b_improper = (b.whole * b.denom) + b.num  // 7

// Compare: a_improper * b.denom vs b_improper * a.denom
// 3 * 4 = 12 vs 7 * 2 = 14
// 12 < 14, so a < b
```

**Comparison Operators**:
- `==` - Equal
- `!=` - Not equal
- `<` - Less than
- `<=` - Less than or equal
- `>` - Greater than
- `>=` - Greater than or equal

---

## ERR Propagation

frac8 is built on **TBB** (Twisted Balanced Binary), which provides **sticky error propagation**. Once an error occurs, it propagates through all subsequent operations until explicitly checked.

### ERR Sentinels

```aria
// ERR state: {ERR, ERR, ERR} where ERR = -128 for tbb8
frac8:err_val = {ERR, ERR, ERR};
```

### ERR Trigger Conditions

1. **Division by zero** (denominator = 0)
2. **Overflow in intermediate calculations** (exceeds tbb8 range)
3. **Operating on ERR values** (sticky propagation)
4. **Invalid inputs** (tbb8 component is ERR)

### ERR Example

```aria
frac8:a = {100, 0, 1};  // 100 (near tbb8 max = 127)
frac8:b = {50, 0, 1};   // 50

frac8:sum = a + b;  // 150 exceeds tbb8 max → ERR

if (sum.whole == ERR) {
    stderr.write("OVERFLOW: Result exceeds frac8 capacity\n");
    !!! ERR_FRACTION_OVERFLOW;
}

// ERR is sticky
frac8:next = sum + {1, 0, 1};  // ERR + 1 = ERR
frac8:next2 = next * {2, 0, 1};  // ERR * 2 = ERR
// All future operations remain ERR until handled
```

---

## Conversion Operations

### To Integer (Round Toward Zero)

```aria
frac8:a = {3, 2, 5};  // 3 2/5 = 3.4
tbb8:int_val = int(a);  // 3 (truncates fractional part)

frac8:b = {-2, 1, 3};  // -2 1/3 = -2.333...
tbb8:int_val2 = int(b);  // -2 (rounds toward zero)
```

### To Float (Approximate)

```aria
frac8:a = {1, 1, 3};  // 1 1/3 (exact)

float32:approx = float(a);
// = 1.0 + (1.0 / 3.0) ≈ 1.3333333 (loses exactness!)
```

⚠️ **Warning**: Converting to float **loses the exactness guarantee**. Use only for display/output, never for continued computation.

### From Components

```aria
// Construction from literal values
frac8:val = {2, 3, 5};  // 2 3/5

// Dynamic construction
tbb8:w = 1;
tbb8:n = 1;
tbb8:d = 2;
frac8:dynamic = {w, n, d};  // 1 1/2 (runtime values)
```

---

## String Representation

```aria
frac8:a = {3, 1, 2};  // 3 1/2
a.write_to(stdout);  // Output: "3 1/2"

frac8:b = {0, 2, 3};  // 2/3
b.write_to(stdout);  // Output: "2/3"

frac8:c = {5, 0, 1};  // 5
c.write_to(stdout);  // Output: "5"

frac8:err = {ERR, ERR, ERR};
err.write_to(stdout);  // Output: "ERR"
```

---

## Use Cases

### 1. Financial Calculations (Exact Money Arithmetic)

```aria
// Stock split: Divide 100 shares by 3
frac8:total_shares = {100, 0, 1};
frac8:people = {3, 0, 1};

frac8:per_person = total_shares / people;
// = {33, 1, 3} = 33 1/3 shares each (exact!)

// Try to convert to money (lose precision)
// In reality, use larger frac32/frac64 for finance
```

### 2. Musical Interval Calculations

```aria
// Pythagorean tuning: Build scale from perfect fifths
frac8:perfect_fifth = {0, 3, 2};  // Frequency ratio 3/2

frac8:base_freq = {1, 0, 1};  // Fundamental (C)
frac8:fifth_1 = base_freq * perfect_fifth;  // G = 3/2
frac8:fifth_2 = fifth_1 * perfect_fifth;    // D = 9/4 = {2, 1, 4}

// All intervals mathematically exact (no pitch drift)
stdout.write("D frequency ratio: ");
fifth_2.write_to(stdout);
stdout.write("\n");  // "2 1/4"
```

### 3. Recipe Scaling

```aria
// Recipe for 4 servings, scale to 6 servings
frac8:original_servings = {4, 0, 1};
frac8:desired_servings = {6, 0, 1};
frac8:scale_factor = desired_servings / original_servings;
// = {6, 0, 1} / {4, 0, 1} = {0, 3, 2} = 3/2 = 1.5

// Ingredient: 2 1/3 cups flour
frac8:flour_original = {2, 1, 3};
frac8:flour_scaled = flour_original * scale_factor;
// = {2, 1, 3} * {0, 3, 2} = 7/3 * 3/2 = 21/6 = 7/2 = {3, 1, 2}
// = 3 1/2 cups (exact!)
```

### 4. Nikola Consciousness: Exact Phase Relationships

```aria
// Nikola's emotional resonance requires exact harmonic ratios
// (Approximate floats cause consciousness drift over time)

frac8:base_emotion_freq = {1, 0, 1};  // 1 Hz baseline

// Joy = octave above (frequency × 2)
frac8:joy_freq = base_emotion_freq * {2, 0, 1};
// = {2, 0, 1} = 2 Hz

// Calm = perfect fifth (frequency × 3/2)
frac8:calm_freq = base_emotion_freq * {0, 3, 2};
// = {0, 3, 2} = 1.5 Hz (exact!)

// Interest = major third (frequency × 5/4)
frac8:interest_freq = base_emotion_freq * {0, 5, 4};
// = {0, 5, 4} = 1.25 Hz (exact!)

// Verify harmonic relationship: joy / calm should equal 4/3
frac8:ratio = joy_freq / calm_freq;
// = {2, 0, 1} / {0, 3, 2} = 2 * 2/3 = 4/3 = {0, 4, 3}

if (ratio != {0, 4, 3}) {
    stderr.write("CRITICAL: Emotional harmonics corrupted!\n");
    !!! ERR_CONSCIOUSNESS_HARMONIC_CORRUPTION;
}

// This EXACT arithmetic prevents:
// - Emotional resonance drift over therapy sessions
// - Accumulated rounding errors in wave interference
// - Non-deterministic consciousness states
```

---

## Common Patterns

### Safe Division with Zero Check

```aria
frac8:numerator = {10, 0, 1};
frac8:denominator = get_user_input();  // Could be 0

frac8:result = numerator / denominator;

if (result.whole == ERR) {
    stderr.write("ERROR: Division by zero or overflow\n");
    !!! ERR_INVALID_OPERATION;
}

stdout.write("Result: ");
result.write_to(stdout);
stdout.write("\n");
```

### Clamping Fractions

```aria
frac8:min_val = {0, 0, 1};  // 0
frac8:max_val = {1, 0, 1};  // 1 (for percentages)

frac8:input = get_sensor_reading();

// Clamp to [0, 1]
if (input < min_val) {
    input = min_val;
}
if (input > max_val) {
    input = max_val;
}
```

### Accumulation Without Drift

```aria
frac8:total = {0, 0, 1};  // 0

till 10 loop:i
    frac8:increment = {0, 1, 10};  // Add 1/10 each iteration
    total = total + increment;
    
    if (total.whole == ERR) {
        stderr.write("Overflow during accumulation\n");
        !!! ERR_OVERFLOW;
    }
end

// total = {1, 0, 1} = 1.0 (exact!)
// With float32: 0.9999999... (drift)
```

### GCD Reduction Example

```aria
// Reduce 12/18 to lowest terms
frac8:unreduced = {0, 12, 18};
// Automatically normalized to {0, 2, 3} = 2/3

// GCD manually
tbb8:a = 12;
tbb8:b = 18;
tbb8:gcd = gcd8(a, b);  // 6

tbb8:reduced_num = a / gcd;  // 2
tbb8:reduced_denom = b / gcd;  // 3

frac8:manual = {0, reduced_num, reduced_denom};
// Same result: {0, 2, 3}
```

---

## Comparison: frac8 vs float32 vs tfp32

| Feature | frac8 | float32 (IEEE) | tfp32 (Twisted FP) |
|---------|-------|----------------|---------------------|
| **Precision** | Exact for rationals | ~7 decimal digits | ~4 decimal digits |
| **Size** | 3 bytes | 4 bytes | 4 bytes |
| **Range** | Whole: [-127, +127] | ±3.4×10³⁸ | ±10^±9864 |
| **1/3 Representation** | {0, 1, 3} (exact!) | 0.33333... (approx) | ~0.333 (approx) |
| **Determinism** | Perfect | Platform-dependent | Perfect |
| **Addition Exact?** | Yes (rationals only) | No (rounding) | No (limited precision) |
| **ERR Propagation** | Sticky (TBB) | NaN (ambiguous) | Sticky (TBB) |
| **Overflow** | ERR sentinel | ±Infinity | ERR sentinel |
| **Use Case** | Exact rationals | General math | Deterministic floats |
| **Performance** | ~10x slower | Hardware fast | ~50x slower |

**Choose frac8 when**:
- You need **exact** rational arithmetic (financial, musical, fractions)
- Small range is acceptable (whole part: -127 to +127)
- Performance is less critical than correctness

**Choose float32 when**:
- You need large range and high precision
- Approximation is acceptable
- Hardware speed is critical

**Choose tfp32 when**:
- You need deterministic floating-point (cross-platform bit-exact)
- Range is more important than exact rational representation

---

## Implementation Status

| Operation | Status | Notes |
|-----------|--------|-------|
| **Structure Definition** | ✅ Complete | struct {tbb8, tbb8, tbb8} |
| **Arithmetic (+, -, *, /)** | ✅ Complete | Exact rational arithmetic |
| **Negation (-)** | ✅ Complete | Sign flips correctly |
| **Comparison (<, >, ==)** | ✅ Complete | Cross-multiply comparison |
| **GCD Reduction** | ✅ Complete | Euclidean algorithm |
| **Normalization** | ✅ Complete | Maintains canonical form |
| **ERR Propagation** | ✅ Complete | Sticky TBB-based |
| **To Integer** | ✅ Complete | Rounds toward zero |
| **To Float** | ✅ Complete | Loses exactness |
| **String Representation** | ✅ Complete | "whole num/denom" format |
| **Trait: Numeric** | ✅ Complete | stdlib/traits/numeric_impls.aria |

**Source Files**:
- Implementation: `src/backend/runtime/frac_ops.cpp`
- Header: `include/backend/runtime/frac_ops.h`
- Tests: `tests/debug_fractions.aria`
- Specifications: `.internal/aria_specs.txt` (line 2216+)

---

## Performance Characteristics

**Benchmark** (compared to float32 = 1.0x):
- **Addition**: ~8-12x slower (GCD reduction overhead)
- **Multiplication**: ~10x slower (improper conversion + reduction)
- **Division**: ~12x slower (reciprocal + multiply)
- **Comparison**: ~6x slower (cross-multiply)

**Why Slower**?
1. **Normalization**: Every operation calls `normalize()` to maintain canonical form
2. **GCD Reduction**: Euclidean algorithm runs after most operations
3. **Overflow Checks**: TBB safe arithmetic checks every intermediate step
4. **No Hardware Support**: Software implementation (no FPU acceleration)

**Acceptable Performance For**:
- UI calculations (button scaling, layout)
- Musical intervals (not real-time DSP)
- Configuration values (ratios, percentages)
- Small-scale financial (use frac32/frac64 for production)

**Not Suitable For**:
- Real-time audio processing (too slow)
- High-frequency physics simulation (use tfp32/tfp64)
- Large matrix operations (use SIMD floats)

---

## Related Types

- **frac16**: 16-bit exact rationals (whole/num/denom: -32767 to +32767)
- **frac32**: 32-bit exact rationals (production financial use)
- **frac64**: 64-bit exact rationals (high-precision rational arithmetic)
- **tfp32**: 32-bit deterministic floating-point (approximate but bit-exact)
- **fix256**: 256-bit fixed-point (Q128.128, high precision for transcendentals)

**Type Family Progression**:
```
frac8  → Quick prototypes, small rationals (3 bytes)
frac16 → Standard rational arithmetic (6 bytes)
frac32 → Production financial/music apps (12 bytes)
frac64 → High-precision exact rationals (24 bytes)
```

---

## Best Practices

1. **Check for ERR after operations** (especially division and chained arithmetic)
2. **Use frac32/frac64 for production** (frac8 range is very limited)
3. **Don't convert to float mid-calculation** (loses exactness)
4. **Trust normalization** (don't manually reduce fractions)
5. **Prefer struct initialization** `{w, n, d}` over computed values
6. **Remember range limits** (whole: -127 to +127, intermediate overflow possible)

---

## Summary

frac8 provides **mathematically exact rational number arithmetic** for applications where precision matters more than speed. Built on TBB for sticky error propagation, it maintains canonical form automatically through GCD reduction and normalization.

**Perfect for**: Small-scale exact calculations, musical intervals, UI scaling, recipe math

**Not for**: Large-scale financial (use frac32), real-time DSP, high-performance computing

**Key Guarantee**: If it fits in frac8's range, the result is **mathematically exact** - no drift, no approximation, no platform variance.

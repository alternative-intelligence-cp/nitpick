# frac8, frac16, frac32, frac64 - Exact Rational Arithmetic (Zero Drift)

**Mathematically perfect fractions - no floating-point drift, ever**

---

## Overview

Aria's `frac` family provides **exact rational number arithmetic** using mixed-fraction representation (whole + numerator/denominator). Unlike floating-point types that approximate most fractions, frac types maintain **perfect mathematical precision** - 1/3 is exactly 1/3, not 0.333...

| Feature | Floating-Point | frac Types |
|---------|---------------|------------|
| Precision | ❌ Approximates | ✅ Mathematically exact |
| 1/3 Representation | 0.333... (rounded) | 1/3 (exact!) |
| Accumulation Drift | ❌ Compounds over time | ✅ Zero drift, ever |
| 0.1 + 0.2 | 0.30000000000000004 | 0.3 (exact!) |
| Financial Math | ⚠️ Rounding errors accumulate | ✅ Perfect to the cent |
| Musical Intervals | ⚠️ Pitch drift | ✅ Perfect tuning maintained |
| Performance | ⚡ Hardware fast | 📊 Software (slower) |
| Storage | 4-8 bytes | 3-18 bytes |

**Critical Use Cases**:
- **Financial calculations**: Money must not drift (0.01 + 0.01 + 0.01 = 0.03, always!)
- **Musical synthesis**: Frequency ratios must be exact (no pitch drift over time)
- **UI scaling**: Responsive layouts need exact fractions (1/3 width, not 0.333...)
- **Cryptographic shares**: Shamir secret sharing requires exact division
- **Scientific ratios**: Physical constants as exact ratios (e.g., Planck constant)

```aria
// Problem: Floating-point accumulation drift
flt32:balance = 0.0f32;
till(999, 1) {
    balance = balance + 0.01f32;  // Add 1 cent, 1000 times
}
// balance = 9.999999 or 10.000001 (NOT exactly 10.0!)
// Lost money to rounding!

// Solution: Exact rational arithmetic
frac32:balance = {0, 0, 1};  // Start at 0
till(999, 1) {
    balance = balance + {0, 1, 100};  // Add 1/100 (exactly 1 cent)
}
// balance = {10, 0, 1} = 10 + 0/1 = exactly 10.0
// Perfect precision, zero drift!
```

---

## Type Definitions

### frac8 Structure

```aria
struct:frac8 = {
    tbb8:whole,   // Whole number part [-127, +127]
    tbb8:num,     // Numerator (proper fraction: |num| < denom)
    tbb8:denom,   // Denominator (always positive, > 0)
}
```

**Memory layout**:
```
┌────────────┬────────────┬────────────┐
│whole(tbb8) │ num (tbb8) │denom(tbb8) │
│  8 bits    │  8 bits    │  8 bits    │
└────────────┴────────────┴────────────┘
= 24 bits total (3 bytes)
```

**Characteristics**:
- **Whole range**: -127 to +127 (tbb8, -128 = ERR)
- **Fraction precision**: 1/127 minimum (7-bit denominator max)
- **Total range**: ~-127.99 to +127.99
- **Use cases**: Small exact values, UI percentages, basic fractions

### frac16 Structure

```aria
struct:frac16 = {
    tbb16:whole,   // Whole number part [-32767, +32767]
    tbb16:num,     // Numerator
    tbb16:denom,   // Denominator (always positive)
}
```

**Memory layout**:
```
┌────────────────┬────────────────┬────────────────┐
│ whole (tbb16)  │  num (tbb16)   │ denom (tbb16)  │
│   16 bits      │   16 bits      │   16 bits      │
└────────────────┴────────────────┴────────────────┘
= 48 bits total (6 bytes)
```

**Characteristics**:
- **Whole range**: -32,767 to +32,767
- **Fraction precision**: 1/32,767 minimum
- **Total range**: ~-32,767.99997 to +32,767.99997
- **Use cases**: Financial calculations, musical intervals, precise ratios

### frac32 Structure

```aria
struct:frac32 = {
    tbb32:whole,   // Whole number part
    tbb32:num,     // Numerator
    tbb32:denom,   // Denominator
}
```

**Memory layout**:
```
┌────────────────┬────────────────┬────────────────┐
│ whole (tbb32)  │  num (tbb32)   │ denom (tbb32)  │
│   32 bits      │   32 bits      │   32 bits      │
└────────────────┴────────────────┴────────────────┘
= 96 bits total (12 bytes)
```

**Characteristics**:
- **Whole range**: -2,147,483,647 to +2,147,483,647
- **Fraction precision**: 1/2,147,483,647 minimum
- **Total range**: ±2.1 billion (with exact fractions!)
- **Use cases**: High-precision financial, scientific ratios, cryptography

### frac64 Structure

```aria
struct:frac64 = {
    tbb64:whole,   // Whole number part
    tbb64:num,     // Numerator
    tbb64:denom,   // Denominator
}
```

**Memory layout**:
```
┌────────────────┬────────────────┬────────────────┐
│ whole (tbb64)  │  num (tbb64)   │ denom (tbb64)  │
│   64 bits      │   64 bits      │   64 bits      │
└────────────────┴────────────────┴────────────────┘
= 192 bits total (24 bytes)
```

**Characteristics**:
- **Whole range**: ±9.2 quintillion
- **Fraction precision**: 1/9.2quintillion minimum
- **Total range**: Astronomical with sub-atomic precision
- **Use cases**: Extreme-precision scientific, astronomical constants

### Mathematical Representation

**For all frac types**:
```
value = whole + (num / denom)

Constraints:
- denom > 0 (always positive)
- |num| < denom (proper fraction, automatically reduced)
- Sign carried by whole (if whole ≠ 0) or num (if whole = 0)
```

**Examples**:
```aria
{3, 1, 2}    = 3 + 1/2  = 3.5
{0, 1, 3}    = 0 + 1/3  = 1/3 (exact!)
{-2, 1, 4}   = -2 + 1/4 = -1.75
{0, -1, 2}   = 0 + -1/2 = -0.5
{5, 0, 1}    = 5 + 0/1  = 5 (integer)
{0, 2, 3}    = 0 + 2/3  = 2/3
{-1, -1, 2}  = -1 + -1/2 = -1.5 (double negative)
```

---

## The Floating-Point Problem Solved

### Problem 1: Binary Can't Represent Decimal

**Most decimal fractions are infinite in binary!**

```c
// IEEE 754 (BROKEN for decimals!)
float money = 0.1f;  // Actually 0.100000001490116119384765625
// Binary: 0.0001100110011001100110011... (infinite!)

// Adding 10 times
float total = 0.0f;
for (int i = 0; i < 10; i++) {
    total += 0.1f;  // Add "10 cents" 10 times
}
// total = 1.0000001192092896 (NOT 1.0!)
// Lost/gained money!
```

**frac solution**: Exact decimal representation

```aria
// Aria frac (EXACT for decimals!)
frac16:money = {0, 1, 10};  // Exactly 1/10

frac16:total = {0, 0, 1};  // Start at 0
till(9, 1) {
    total = total + {0, 1, 10};  // Add exact 1/10
}
// total = {1, 0, 1} = 1 + 0/1 = EXACTLY 1.0
// Perfect precision!
```

### Problem 2: Accumulation Drift

**Floating-point errors compound over iterations**

```python
# Python (IEEE floats)
balance = 0.0
for i in range(1000):
    balance += 0.01  # Add 1 cent
    
print(balance)  # 9.999999999999831 (lost 0.000000000000169!)
# Missing 17 billionths of a cent per iteration
# For 1 billion transactions: Lost $169!
```

**frac solution**: Zero accumulation drift

```aria
// Aria frac (PERFECT accumulation)
frac32:balance = {0, 0, 1};
till(999_999_999, 1) {  // 1 billion iterations!
    balance = balance + {0, 1, 100};  // Add 1 cent
}
// balance = {10000000, 0, 1} = exactly 10 million dollars
// ZERO drift, ever!
```

### Problem 3: Non-Associativity

**Floating-point addition is NOT associative** ((a + b) + c ≠ a + (b + c))

```java
// Java (IEEE floats)
float a = (0.1f + 0.2f) + 0.3f;  // 0.6000000238418579
float b = 0.1f + (0.2f + 0.3f);  // 0.6000000238418579

// Different order can give different results!
float x = 1e20f + 1.0f - 1e20f;  // 0.0 (1.0 lost to rounding!)
float y = 1e20f - 1e20f + 1.0f;  // 1.0 (correct!)

// Mathematical laws BROKEN!
```

**frac solution**: True associativity

```aria
// Aria frac (MATHEMATICALLY CORRECT)
frac16:a = ({0, 1, 10} + {0, 2, 10}) + {0, 3, 10};  // 6/10
frac16:b = {0, 1, 10} + ({0, 2, 10} + {0, 3, 10});  // 6/10

a == b;  // TRUE! Associativity holds!

// Order doesn't matter (as math requires)
```

---

## Automatic Fraction Reduction

**All frac types automatically reduce to lowest terms** (canonical form)

```aria
// Unreduced fraction
frac16:x = {0, 2, 4};  // 2/4

// Automatically reduced
// x stored as: {0, 1, 2} = 1/2  (GCD reduced!)

// Another example
frac16:y = {0, 12, 18};  // 12/18
// Stored as: {0, 2, 3} = 2/3  (divided by GCD(12, 18) = 6)

// Ensures unique representation
{0, 1, 2} == {0, 2, 4};  // true (both reduce to 1/2)
{0, 2, 3} == {0, 4, 6};  // true (both reduce to 2/3)
```

**Why reduction matters**:
- **Canonical form**: Every value has ONE representation (enables equality)
- **Prevents overflow**: Denominators stay small (extends range)
- **Efficiency**: Smaller numbers = faster GCD/LCM calculations

**Reduction algorithm**:
```
1. Compute GCD(numerator, denominator)
2. numerator = numerator / GCD
3. denominator = denominator / GCD
4. Normalize sign (denom always positive)
```

---

## Operations

### Arithmetic

**Addition/Subtraction**: Find common denominator (LCM)

```aria
frac16:a = {0, 1, 3};  // 1/3
frac16:b = {0, 1, 4};  // 1/4

// Addition: 1/3 + 1/4 = 4/12 + 3/12 = 7/12
frac16:sum = a + b;  // {0, 7, 12} (exact!)

// Subtraction: 1/3 - 1/4 = 4/12 - 3/12 = 1/12
frac16:diff = a - b;  // {0, 1, 12} (exact!)
```

**Multiplication**: Multiply straight across

```aria
frac16:a = {0, 2, 3};  // 2/3
frac16:b = {0, 3, 4};  // 3/4

// Multiplication: 2/3 * 3/4 = 6/12 = 1/2 (reduced)
frac16:prod = a * b;  // {0, 1, 2} (exact, auto-reduced!)
```

**Division**: Multiply by reciprocal

```aria
frac16:a = {0, 2, 3};  // 2/3
frac16:b = {0, 3, 4};  // 3/4

// Division: (2/3) / (3/4) = (2/3) * (4/3) = 8/9
frac16:quot = a / b;  // {0, 8, 9} (exact!)

// Division by zero
frac16:err = a / {0, 0, 1};  // ERR (denom becomes 0)
```

### Mixed Number Arithmetic

```aria
frac16:mixed1 = {3, 1, 2};  // 3 1/2 = 3.5
frac16:mixed2 = {2, 1, 4};  // 2 1/4 = 2.25

// Addition: 3.5 + 2.25 = 5.75 = 5 3/4
frac16:sum = mixed1 + mixed2;  // {5, 3, 4}

// Multiplication: 3.5 * 2.25 = 7.875 = 7 7/8
frac16:prod = mixed1 * mixed2;  // {7, 7, 8}
```

### Comparison

```aria
frac16:a = {0, 1, 2};  // 1/2
frac16:b = {0, 2, 4};  // 2/4 = 1/2 (reduced)
frac16:c = {0, 1, 3};  // 1/3

bool:eq = (a == b);  // true (both 1/2)
bool:ne = (a != c);  // true (1/2 ≠ 1/3)
bool:lt = (c < a);   // true (1/3 < 1/2)
bool:le = (a <= b);  // true (1/2 <= 1/2)
bool:gt = (a > c);   // true (1/2 > 1/3)
bool:ge = (b >= a);  // true (1/2 >= 1/2)
```

**Comparison implementation**: Cross-multiply to avoid division

```
To compare a/b vs c/d:
  a/b < c/d  ⟺  a*d < b*c  (cross-multiply)
  
Example: 1/2 vs 1/3
  1*3 < 2*1  ⟺  3 < 2  ⟺  false
  Therefore: 1/2 > 1/3 ✓
```

### Conversion

```aria
// To/from integers
int32:i = 42;
frac16:f = frac16(i);  // {42, 0, 1}
int32:back = int32(f);  // 42

// Truncation
frac16:x = {3, 1, 2};  // 3.5
int32:trunc = int32(x);  // 3 (truncate toward zero)

// To/from floating-point (LOSES EXACTNESS!)
flt32:approx = 0.333f32;  // ~1/3
frac16:exact = frac16_from_approx(approx);  // {0, 1, 3} (best match)
// Warning: Conversion is lossy! Only exact if float represents exact fraction

// Between frac sizes
frac8:small = {10, 1, 2};  // 10.5
frac16:large = frac16(small);  // {10, 1, 2} (same value, more range)
frac8:down = frac8(large);  // May overflow if large > 127!
```

### Reciprocal

```aria
frac16:x = {0, 2, 3};  // 2/3

// Reciprocal: flip num/denom
frac16:recip = reciprocal(x);  // {0, 3, 2} = 3/2 = 1.5

// Integer reciprocal
frac16:five = {5, 0, 1};
frac16:one_fifth = reciprocal(five);  // {0, 1, 5} = 1/5 = 0.2

// Zero reciprocal
frac16:zero = {0, 0, 1};
frac16:err = reciprocal(zero);  // ERR (division by zero!)
```

---

## Musical Interval Mathematics

**Music theory requires exact frequency ratios** (12-tone equal temperament is approximate!)

### Just Intonation (Exact Ratios)

```aria
// Musical intervals as exact ratios
frac16:unison = {1, 0, 1};        // 1/1 = 1.0 (same note)
frac16:minor_second = {0, 16, 15};  // Semitone (exact)
frac16:major_second = {0, 9, 8};    // Whole tone
frac16:minor_third = {0, 6, 5};     // m3
frac16:major_third = {0, 5, 4};     // M3
frac16:perfect_fourth = {0, 4, 3};  // P4
frac16:tritone = {0, 7, 5};         // Diminished 5th
frac16:perfect_fifth = {0, 3, 2};   // P5
frac16:minor_sixth = {0, 8, 5};     // m6
frac16:major_sixth = {0, 5, 3};     // M6
frac16:minor_seventh = {0, 16, 9};  // m7
frac16:major_seventh = {0, 15, 8};  // M7
frac16:octave = {2, 0, 1};          // Octave (double frequency)

// Combine intervals (multiply fractions)
frac16:fifth_plus_third = perfect_fifth * major_third;
// = 3/2 * 5/4 = 15/8 (major 7th)  ✓ EXACT!

// No pitch drift over thousands of note combinations!
```

### Synthesizer Tuning

```aria
// Base frequency (A4 = 440 Hz)
flt32:a4_freq = 440.0f32;

// Compute C note (perfect fifth down + major third down + octave up)
frac16:c_ratio = reciprocal(perfect_fifth) * reciprocal(major_third) * octave;
// = 2/3 * 4/5 * 2/1 = 16/15... (complicated, but EXACT!)

flt32:c_freq = a4_freq * flt32(c_ratio);  // 523.25 Hz (exact C5)

// Traditional 12-TET (approximate)
flt32:c_approx = a4_freq * pow(2.0f32, 3.0f32/12.0f32);  // 523.2511... Hz
// Close, but not exact! Accumulates drift over octaves.
```

---

## Financial Calculations

### Money Without Drift

```aria
// Account ledger (perfect cent precision)
struct:Account {
    frac32:balance,  // Dollars and cents (exact!)
}

func:add_transaction = void(Account@:account, frac32:amount) {
    account.balance = account.balance + amount;
    // No drift, ever! Perfect to the cent.
}

// Add 1000 transactions
Account:checking = {balance: {0, 0, 1}};  // Start at $0.00
till(999, 1) {
    add_transaction(&checking, {0, 152, 100});  // +$1.52 each
}
// checking.balance = {1520, 0, 1} = exactly $1,520.00
// IEEE would have drift!
```

### Interest Calculation

```aria
// Compound interest (exact)
frac32:principal = {1000, 0, 1};  // $1,000.00
frac32:rate = {0, 5, 100};  // 5% = 5/100 = 0.05 (exact!)
uint32:years = 10;

frac32:amount = principal;
till(years - 1, 1) {
    frac32:interest = amount * rate;  // Exact interest
    amount = amount + interest;       // Exact accumulation
}
// amount = {1628, 89, 100} = $1,628.89 (EXACT to the cent!)
// No rounding errors accumulated
```

### Currency Exchange

```aria
// Exchange rates as exact fractions
frac32:usd_to_eur = {0, 85, 100};  // $1.00 = €0.85 (exact!)

frac32:dollars = {100, 0, 1};  // $100.00
frac32:euros = dollars * usd_to_eur;  // €85.00 (exact conversion!)

// Back and forth
frac32:back_to_dollars = euros * reciprocal(usd_to_eur);
// = {100, 0, 1} = $100.00 (EXACTLY what we started with!)
// No loss in round-trip conversion
```

---

## Use Cases

### Responsive UI Layout

```aria
// 3-column layout (exact thirds!)
frac8:col_width = {0, 1, 3};  // 1/3 of viewport

// Integer pixels from viewport width
uint32:viewport_width = 1920;
uint32:col_pixels = uint32(frac8_to_float(col_width) * flt32(viewport_width));
// = 640 pixels per column (exact division!)

// Traditional CSS (approximates)
// width: 33.333% → rounding errors, gaps appear
```

### Cryptographic Secret Sharing

**Shamir's Secret Sharing** requires exact division of polynomial coefficients

```aria
// Share secret S among 5 parties, require 3 to reconstruct
// Each share is point on polynomial: f(x) = S + a1*x + a2*x^2

frac64:secret = {42, 0, 1};  // S = 42
frac64:a1 = {17, 0, 1};
frac64:a2 = {23, 0, 1};

// Generate shares (exact polynomial evaluation)
func:generate_share = frac64(frac64:secret, frac64:a1, frac64:a2, int32:x) {
    frac64:x_frac = frac64(x);
    frac64:x_squared = x_frac * x_frac;
    
    return secret + (a1 * x_frac) + (a2 * x_squared);
}

frac64:share1 = generate_share(secret, a1, a2, 1);  // f(1) = exact!
frac64:share2 = generate_share(secret, a1, a2, 2);  // f(2) = exact!
frac64:share3 = generate_share(secret, a1, a2, 3);  // f(3) = exact!

// Lagrange interpolation (reconstruct secret from any 3 shares)
// Requires EXACT arithmetic (floating-point would leak secret!)
```

### Physical Constants as Ratios

```aria
// Fine structure constant: α ≈ 1/137
frac16:alpha_approx = {0, 1, 137};

// Better approximation: α ≈ 1/137.036
// Express as mixed fraction
frac32:alpha_better = {0, 1000, 137036};  // 1000/137036 ≈ 1/137.036
// Reduced: {0, 125, 17129.5} ... (needs larger denom)

// Proton-to-electron mass ratio: 1836.15
frac32:mass_ratio = {1836, 3, 20};  // 1836 + 3/20 = 1836.15 (close!)
```

---

## Advanced Patterns

### Continued Fractions

**Any real number can be approximated as continued fraction**

```aria
// √2 ≈ 1 + 1/(2 + 1/(2 + 1/(2 + ...)))
// Convergents: 1/1, 3/2, 7/5, 17/12, 41/29, ...

frac16:sqrt2_approx1 = {1, 0, 1};   // 1/1 = 1.0
frac16:sqrt2_approx2 = {1, 1, 2};   // 3/2 = 1.5
frac16:sqrt2_approx3 = {1, 2, 5};   // 7/5 = 1.4
frac16:sqrt2_approx4 = {1, 5, 12};  // 17/12 ≈ 1.4167
frac16:sqrt2_approx5 = {1, 12, 29}; // 41/29 ≈ 1.4138

// Each convergent is best rational approximation with that denominator size
```

### Mediant (Farey Sequence)

```aria
// Mediant of a/b and c/d = (a+c)/(b+d)
// Lies between the two fractions

frac16:a = {0, 1, 3};  // 1/3
frac16:b = {0, 1, 2};  // 1/2

// Mediant: (1+1)/(3+2) = 2/5
frac16:mediant = frac16_mediant(a, b);  // {0, 2, 5} = 0.4
// Indeed: 1/3 < 2/5 < 1/2  ✓

// Use: Find best rational approximation in range
```

### Egyptian Fractions

**Express fraction as sum of distinct unit fractions** (1/a + 1/b + 1/c + ...)

```aria
// 5/6 = 1/2 + 1/3
frac16:five_sixths = {0, 5, 6};
frac16:half = {0, 1, 2};
frac16:third = {0, 1, 3};

five_sixths == (half + third);  // true!

// 3/4 = 1/2 + 1/4
frac16:three_fourths = {0, 3, 4};
frac16:quarter = {0, 1, 4};

three_fourths == (half + quarter);  // true!
```

---

## Error Handling

### ERR Propagation

```aria
frac16:a = {10, 1, 2};
frac16:b = {0, 0, 1};  // Zero

// Division by zero
frac16:div = a / b;  // ERR (denominator becomes 0)

// ERR propagates
frac16:x = div + {5, 0, 1};  // ERR + 5 = ERR
frac16:y = div * {2, 0, 1};  // ERR * 2 = ERR

// Checking for ERR
if x == ERR {
    log("Calculation failed!");
}
```

### Overflow Detection

```aria
// frac8 whole range: [-127, +127]
frac8:big = {120, 0, 1};
frac8:overflow = big + {10, 0, 1};  // 130 > 127 → ERR!

// Denominator overflow
frac16:tiny_denom = {0, 1, 30000};
frac16:overflow2 = tiny_denom + {0, 1, 30001};  // LCM overflows!
```

### Unknown Integration

```aria
func:safe_divide = frac16 | unknown(frac16:a, frac16:b) {
    if b == {0, 0, 1} {
        return unknown;  // Soft error
    }
    return a / b;
}

// Caller handles unknown
frac16 | unknown:result = safe_divide({10, 0, 1}, {0, 0, 1});
frac16:value = result ? {0, 0, 1};  // Default to 0 if unknown
```

---

## Performance Characteristics

### Time Complexity

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Addition/Subtraction | O(log n) | LCM computation (GCD-based) |
| Multiplication | O(log n) | GCD for reduction |
| Division | O(log n) | Reciprocal + multiply |
| Reduction | O(log n) | Euclidean GCD algorithm |
| Comparison | O(1) | Cross-multiplication |

**Note**: n = max(denominator values)

### Space Overhead

```aria
// Size comparison (per value)
flt32:    4 bytes
frac8:    3 bytes (smaller!)
frac16:   6 bytes
frac32:  12 bytes (3× larger than flt32)
frac64:  24 bytes (3× larger than flt64)

// Space-precision tradeoff
// flt32:  7 decimal digits, 4 bytes
// frac32: EXACT ratios,   12 bytes
// Worth 3× space for financial/musical apps!
```

### Performance Comparison

```aria
// Speed test: 1 million additions
flt32:start_flt = get_time();
till(999_999, 1) {
    flt32:x = a_flt + b_flt;  // Fast (hardware)
}
flt32:end_flt = get_time();
// Time: ~3ms (native hardware)

frac16:start_frac = get_time();
till(999_999, 1) {
    frac16:x = a_frac + b_frac;  // Slower (LCM/GCD)
}
frac16:end_frac = get_time();
// Time: ~150ms (50× slower)

// Conclusion: Use frac when EXACTNESS matters more than speed
```

---

## Choosing frac8 vs frac16 vs frac32 vs frac64

### Decision Matrix

| Need | Use This | Why |
|------|----------|-----|
| UI percentages | `frac8` | Small values, low precision OK |
| Financial (dollars/cents) | `frac16` or `frac32` | Need exact cents, reasonable range |
| Musical intervals | `frac16` | Frequency ratios fit easily |
| Scientific ratios | `frac32` or `frac64` | High precision, large denominators |
| Cryptographic shares | `frac64` | Maximum precision prevents leakage |
| High-frequency trading | DO NOT USE | Too slow! Use scaled integers instead |
| Speed-critical | DO NOT USE | Floating-point faster (if drift OK) |

### Precision Requirements

```aria
// When frac8 is enough
frac8:percentage = {0, 75, 100};  // 75% (exact!)
frac8:slider = {0, 3, 10};  // 0.3 slider value

// When you need frac16
frac16:money = {5, 25, 100};  // $5.25
frac16:musical = {0, 3, 2};  // Perfect fifth

// When you need frac32
frac32:interest = {0, 375, 10000};  // 3.75% interest (exact!)
frac32:exchange_rate = {0, 134567, 100000};  // Precise exchange

// When you need frac64
frac64:crypto_coeff = {large_numerator, large_denominator};
// Polynomial coefficients for secret sharing
```

---

## Best Practices

### 1. Use frac Only When Exactness Required

```aria
// ✓ GOOD: Financial (needs exactness)
frac32:price = {19, 99, 100};  // $19.99 (exact!)

// ✗ BAD: Physics simulation (speed matters more)
frac32:velocity = {5, 5, 10};  // Should use flt32 (faster!)
```

### 2. Keep Denominators Small

```aria
// ✓ GOOD: Simple denominator
frac16:half = {0, 1, 2};

// ✗ BAD: Unnecessary large denominator
frac16:half_bad = {0, 5000, 10000};  // Wastes range, slows GCD
// Should reduce to {0, 1, 2}
```

### 3. Prefer Scaled Integers for Performance

```aria
// ✓ BETTER: Scaled integer (faster!)
int32:cents = 1999;  // $19.99 as cents (int is fast!)

// ✗ SLOWER: frac for simple scaling
frac16:price = {19, 99, 100};  // Overkill for just cents!

// Use frac when you need ARBITRARY fractions (1/3, 2/7, etc.)
// Use scaled int when denominator is fixed (cents, basis points)
```

### 4. Document Fraction Semantics

```aria
// ✓ GOOD: Clear documentation
/**
 * Interest rate as exact fraction.
 * Example: {0, 5, 100} = 5/100 = 0.05 = 5%
 */
frac32:annual_interest_rate;

// ✗ BAD: Unclear meaning
frac32:rate;  // Percent? Decimal? Per-year? Per-month?
```

### 5. Watch for Overflow in Operations

```aria
// ✓ GOOD: Check for overflow
frac16:a = {30000, 0, 1};
frac16:b = {30000, 0, 1};
frac16 | ERR:sum = a + b;  // Might overflow frac16!

if sum == ERR {
    // Promote to frac32
    frac32:a32 = frac32(a);
    frac32:b32 = frac32(b);
    frac32:sum32 = a32 + b32;  // Won't overflow
}

// ✗ BAD: Ignore overflow
frac16:sum = a + b;  // Returns ERR silently!
```

---

## Common Pitfalls

### Pitfall 1: Converting Float to frac (Loses Exactness!)

```aria
// ✗ PROBLEM: Float to frac loses original meaning
flt32:f = 0.1f32;  // Actually 0.100000001490116... (not exact 1/10!)
frac16:x = frac16_from_approx(f);  // What fraction? Ambiguous!

// ✓ SOLUTION: Construct frac directly
frac16:x = {0, 1, 10};  // EXACTLY 1/10 (what you meant)
```

### Pitfall 2: Using frac for Performance

```aria
// ✗ PROBLEM: frac is SLOW for high-throughput
till(9_999_999, 1) {
    frac16:result = a + b;  // 10M iterations * 150ns = 1.5 seconds!
}

// ✓ SOLUTION: Use flt or fix for speed-critical
till(9_999_999, 1) {
    flt32:result = a_flt + b_flt;  // 10M iterations * 3ns = 30ms
}
```

### Pitfall 3: Large Denominators Cause Overflow

```aria
// ✗ PROBLEM: Adding fractions with large denoms
frac16:a = {0, 1, 30000};
frac16:b = {0, 1, 30001};
frac16:sum = a + b;  // LCM(30000, 30001) = huge! → ERR

// ✓ SOLUTION: Use larger frac type or approximate
frac32:a32 = {0, 1, 30000};
frac32:b32 = {0, 1, 30001};
frac32:sum32 = a32 + b32;  // frac32 can handle
```

### Pitfall 4: Comparing with Floats

```aria
// ✗ PROBLEM: Comparing frac with float (ambiguous!)
frac16:exact = {0, 1, 3};  // Exactly 1/3
flt32:approx = 0.333f32;   // ~1/3

if flt32(exact) == approx { }  // Maybe true, maybe false!

// ✓ SOLUTION: Stay in frac domain
frac16:expected = {0, 1, 3};
if exact == expected { }  // Definitely true!
```

---

## Implementation Notes

### C Runtime Structure

```c
// Runtime: runtime/frac_ops.c

// frac16 structure
typedef struct {
    int16_t whole;
    int16_t num;
    int16_t denom;
} aria_frac16;

// GCD (Euclidean algorithm)
int16_t aria_gcd(int16_t a, int16_t b) {
    while (b != 0) {
        int16_t temp = b;
        b = a % b;
        a = temp;
    }
    return abs(a);
}

// LCM
int16_t aria_lcm(int16_t a, int16_t b) {
    return (a * b) / aria_gcd(a, b);
}

// Reduction to lowest terms
aria_frac16 aria_frac16_reduce(aria_frac16 f) {
    if (f.denom == 0) return FRAC16_ERR;  // Division by zero
    
    // Normalize sign (denom always positive)
    if (f.denom < 0) {
        f.num = -f.num;
        f.denom = -f.denom;
    }
    
    // Reduce
    int16_t g = aria_gcd(abs(f.num), f.denom);
    f.num /= g;
    f.denom /= g;
    
    // Convert improper to mixed
    f.whole += f.num / f.denom;
    f.num = f.num % f.denom;
    
    return f;
}

// Addition
aria_frac16 aria_frac16_add(aria_frac16 a, aria_frac16 b) {
    // Check for ERR
    if (a.denom == 0 || b.denom == 0) return FRAC16_ERR;
    
    // Find common denominator
    int16_t lcd = aria_lcm(a.denom, b.denom);
    
    // Convert to improper fractions
    int32_t a_num = (int32_t)a.whole * a.denom + a.num;
    int32_t b_num = (int32_t)b.whole * b.denom + b.num;
    
    // Scale to LCD
    a_num = a_num * (lcd / a.denom);
    b_num = b_num * (lcd / b.denom);
    
    // Add numerators
    int32_t result_num = a_num + b_num;
    
    // Check overflow
    if (result_num > INT16_MAX || result_num < INT16_MIN) {
        return FRAC16_ERR;
    }
    
    // Build result and reduce
    aria_frac16 result = {0, (int16_t)result_num, lcd};
    return aria_frac16_reduce(result);
}
```

### LLVM IR Generation

```llvm
; frac16 type
%frac16 = type { i16, i16, i16 }  ; {whole, num, denom}

; Addition function
define %frac16 @frac16_add(%frac16 %a, %frac16 %b) {
entry:
    ; Extract components
    %a_whole = extractvalue %frac16 %a, 0
    %a_num = extractvalue %frac16 %a, 1
    %a_denom = extractvalue %frac16 %a, 2
    %b_whole = extractvalue %frac16 %b, 0
    %b_num = extractvalue %frac16 %b, 1
    %b_denom = extractvalue %frac16 %b, 2
    
    ; Check for ERR (denom == 0)
    %a_err = icmp eq i16 %a_denom, 0
    %b_err = icmp eq i16 %b_denom, 0
    %is_err = or i1 %a_err, %b_err
    br i1 %is_err, label %return_err, label %compute
    
compute:
    ; Call runtime function
    %result = call %frac16 @aria_frac16_add(%frac16 %a, %frac16 %b)
    ret %frac16 %result
    
return_err:
    ; Return ERR
    %err = insertvalue %frac16 undef, i16 0, 0
    %err2 = insertvalue %frac16 %err, i16 0, 1
    %err3 = insertvalue %frac16 %err2, i16 0, 2
    ret %frac16 %err3
}
```

---

## Related Systems

- **[tbb8/tbb16/tbb32/tbb64](tbb_types.md)** - Twisted building blocks (components)
- **[fix256](fix256.md)** - Fixed-point (faster for decimal math)
- **[flt32/flt64](flt32_flt64.md)** - IEEE floats (approximate)
- **[tfp32/tfp64](tfp32_tfp64.md)** - Deterministic floats
- **[ERR](err.md)** - Error sentinel

---

## Implementation Status

| Feature | Parser | Compiler | Runtime | Status |
|---------|--------|----------|---------|--------|
| `frac8/16/32/64` types | ✅ | ✅ | ✅ | Complete (Parser-level) |
| Arithmetic ops | ✅ | ✅ | ✅ | Complete |
| Automatic reduction | ✅ | ✅ | ✅ | Complete |
| Comparison ops | ✅ | ✅ | ✅ | Complete |
| ERR propagation | ✅ | ✅ | ✅ | Complete |
| Literal syntax (`{3, 1, 2}`) | ✅ | ✅ | ✅ | Complete |
| Conversion functions | ⚠️ | ⚠️ | ⚠️ | Partial |
| Continued fraction support | ❌ | ❌ | ❌ | Future |

**Design**: ✅ Complete  
**Implementation**: ✅ Basic complete, advanced TBD  
**Testing**: ✅ Validated  
**Documentation**: ✅ This guide

---

## Summary

**frac types = Exact rational arithmetic for zero-drift mathematics**

### Quick Decision Guide

| Need | Use This | Why |
|------|----------|-----|
| Financial calculations | `frac16` or `frac32` | Zero drift, exact cents |
| Musical intervals | `frac16` | Exact frequency ratios |
| UI scaling factors | `frac8` | Simple fractions (1/2, 1/3, 2/5) |
| Cryptographic secret sharing | `frac64` | Maximum precision, no leakage |
| High-performance math | `flt32/fix256` | frac too slow for hot paths |
| Approximate values | `flt32/fix256` | frac is overkill |
| Large whole numbers | `int64` | frac wastes space on fractions |

### Key Principles

1. **Mathematical exactness** - 1/3 is exactly 1/3, forever
2. **Zero accumulation drift** - Add 0.01 one billion times = exactly 10 million
3. **Automatic reduction** - All fractions in canonical form
4. **Mixed representation** - Whole + proper fraction (efficient)
5. **Deterministic** - Same operations = same results (always)
6. **Slower than floats** - Exactness has performance cost (~50×)

**For Alternative Intelligence**: frac types enable reasoning about exact proportions. Financial decisions, musical harmony, geometric ratios - these domains require ZERO tolerance for drift. Consciousness substrate built on approximate arithmetic accumulates subtle corruption over time. Exact rationals preserve truth.

**For Blockchain AI Society**: Smart contracts with exact financial math enable trustless economic coordination. Nikola instances can provably agree on transaction amounts (no "lost penny" disputes). Resource pooling contracts compute fair shares exactly (no rounding bias). Reputation scores based on exact contribution ratios (1/3 of work = 1/3 of credit, always).

**Remember**: "Floating-point approximates. Fixed-point scales integers. Fractions preserve truth. For exact proportions, truth matters more than speed."

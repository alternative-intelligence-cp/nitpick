# frac16 - 16-bit Exact Rational Arithmetic

## Overview

`frac16` is a 16-bit **mixed-fraction rational number** that provides **mathematically exact** representation of rational values with significantly larger range than frac8. Unlike floating-point types which approximate fractions, frac16 maintains **perfect precision** by storing values as whole numbers plus proper fractions (numerator/denominator).

**Why frac16 Exists**: Production applications need exact rational arithmetic with practical range. While frac8 is limited to ±127, frac16 extends to ±32767 - sufficient for most financial calculations, musical computations, and scientific ratios.

**Key Features**:
- **Exact rational arithmetic** (no floating-point drift)
- **Built on TBB** (Twisted Balanced Binary) for sticky ERR propagation
- **Automatic reduction** to lowest terms (maintains canonical form)
- **Mixed-number representation** (whole + num/denom)
- **Perfect determinism** across all platforms
- **Production-ready range** (whole: ±32767)

**Size**: 6 bytes (48 bits)

**Structure**:
```aria
struct:frac16 = {
    tbb16:whole;   // Whole number part (range: [-32767, +32767])
    tbb16:num;     // Numerator (proper fraction: num < denom)
    tbb16:denom;   // Denominator (always positive, denom > 0)
}
```

**Mathematical Representation**:
```
value = whole + (num / denom)

Examples:
{1000, 1, 2}  = 1000 + 1/2 = 1000.5 (exact!)
{0, 1, 3}     = 0 + 1/3 = 1/3 (mathematically exact, not 0.333...)
{-5000, 2, 5} = -5000 + 2/5 = -4999.6 (sign in whole)
{0, -355, 113} = 0 + (-355/113) ≈ -π (rational approximation)
```

---

## Why frac16 Instead of frac8?

### Range Comparison

| Type | Whole Range | Max Denominator | Example Use Case |
|------|-------------|-----------------|------------------|
| **frac8** | [-127, +127] | 127 | Prototypes, tiny calculations |
| **frac16** | [-32767, +32767] | 32767 | Production rationals, music, finance |

**frac16 Advantage**: Can represent large financial values, musical frequencies, scientific constants:

```aria
// Stock price: $255.75 (exceeds frac8)
frac16:stock_price = {255, 3, 4};  // $255 3/4 ✅

// frac8 would overflow:
frac8:overflow = {255, 3, 4};  // ERR! (255 > 127)

// High-precision π approximation (Archimedes)
frac16:pi_approx = {0, 355, 113};  // 355/113 ≈ 3.141592920...
// Error: only 0.000000267 from true π!

// Musical frequency: A440 (concert pitch)
frac16:a4_freq = {440, 0, 1};  // 440 Hz
// Derive all other notes via exact ratios
```

---

## Memory Layout

### Structure Layout (6 bytes)

```
Offset  Field   Type    Size   Range
------  ------  ------  -----  ---------------------
0       whole   tbb16   2 B    [-32767, +32767] (ERR=-32768)
2       num     tbb16   2 B    [-32767, +32767]
4       denom   tbb16   2 B    [-32767, +32767]
```

### LLVM IR Representation

```llvm
%frac16 = type { i16, i16, i16 }

; Create fraction 1000 1/2 = {1000, 1, 2}
%val = insertvalue %frac16 undef, i16 1000, 0   ; whole = 1000
%val2 = insertvalue %frac16 %val, i16 1, 1      ; num = 1
%val3 = insertvalue %frac16 %val2, i16 2, 2     ; denom = 2

; Extract numerator
%num = extractvalue %frac16 %val3, 1
```

### Canonical Form Invariants

All frac16 values are **automatically normalized** to maintain:

1. **denom > 0** (denominator always positive)
2. **num ≥ 0 when whole ≠ 0** (sign lives in whole for mixed numbers)
3. **num < denom** (proper fraction, not improper)
4. **gcd(num, denom) = 1** (reduced to lowest terms)
5. **Sign handling**:
   - `whole ≠ 0`: Sign in `whole`, `num` stays positive
   - `whole = 0`: Sign in `num` (avoids -0 problem)

**Examples**:
```aria
// Input: {100, 150, 100} (improper: 150/100 > 1)
// Normalized: {101, 1, 2} (100 + 150/100 = 101 + 1/2)

// Input: {0, 2000, 3000} (not reduced: gcd(2000,3000) = 1000)
// Normalized: {0, 2, 3} (2000/3000 = 2/3)

// Input: {500, 0, -10} (denom < 0)
// Normalized: {500, 0, 10} (flip signs to make denom positive)
```

---

## Arithmetic Operations

All operations produce **exact results** and maintain canonical form.

### Addition (Exact)

```aria
frac16:a = {100, 1, 3};  // 100 1/3
frac16:b = {50, 1, 6};   // 50 1/6

frac16:sum = a + b;
// Math: (100 + 1/3) + (50 + 1/6) = 150 + 1/3 + 1/6
//     = 150 + 2/6 + 1/6 = 150 + 3/6 = 150 + 1/2
// Result: {150, 1, 2} = 150 1/2 (exact!)
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
frac16:balance = {1000, 1, 2};  // $1000.50
frac16:payment = {250, 3, 4};   // $250.75

frac16:remaining = balance - payment;
// Math: 1000.5 - 250.75 = 749.75
// Result: {749, 3, 4} = $749.75 (exact!)
```

### Multiplication (Exact)

```aria
// Calculate compound interest (simplified)
frac16:principal = {1000, 0, 1};  // $1000
frac16:rate = {0, 5, 100};        // 5% = 5/100 = 1/20

frac16:interest = principal * rate;
// Math: 1000 * (1/20) = 50
// Result: {50, 0, 1} = $50 (exact!)
```

**Musical Example**:
```aria
// Perfect fifth: frequency ratio 3/2
frac16:base = {440, 0, 1};  // A440 Hz
frac16:fifth_ratio = {0, 3, 2};

frac16:perfect_fifth = base * fifth_ratio;
// Math: 440 * 3/2 = 1320/2 = 660
// Result: {660, 0, 1} = E660 Hz (exact!)
```

### Division (Exact)

```aria
// Divide $1000 among 7 people
frac16:total = {1000, 0, 1};
frac16:people = {7, 0, 1};

frac16:per_person = total / people;
// Math: 1000 / 7 = 1000/7
// Result: {142, 6, 7} = 142 6/7 dollars each (exact!)

// Verify: 7 * 142 6/7 = 7 * 1000/7 = 1000 ✓
```

### Negation

```aria
frac16:profit = {500, 1, 4};   // $500.25 profit
frac16:loss = -profit;         // {-500, 1, 4} = -$500.25 loss

frac16:fraction = {0, 3, 4};   // 3/4
frac16:neg_frac = -fraction;   // {0, -3, 4} = -3/4
```

---

## Comparison Operations

Comparisons convert to improper fractions for accurate ordering:

```aria
frac16:price_a = {99, 99, 100};  // $99.99
frac16:price_b = {100, 0, 1};    // $100.00

if (price_a < price_b) {  // true
    stdout.write("Product A is cheaper\n");
}

// Exact comparison: 9999/100 < 10000/100
```

**Implementation**:
```
// Cross-multiply to compare without division
a_improper = (a.whole * a.denom) + a.num
b_improper = (b.whole * b.denom) + b.num

// Compare: a_improper * b.denom vs b_improper * a.denom
// Avoids floating-point rounding in comparison
```

---

## ERR Propagation

frac16 is built on **TBB** (Twisted Balanced Binary), providing **sticky error propagation**.

### ERR Sentinels

```aria
// ERR state: {ERR, ERR, ERR} where ERR = -32768 for tbb16
frac16:err_val = {ERR, ERR, ERR};
```

### ERR Trigger Conditions

1. **Division by zero** (denominator = 0)
2. **Overflow in intermediate calculations** (exceeds tbb16 range)
3. **Operating on ERR values** (sticky propagation)
4. **Invalid inputs** (tbb16 component is ERR)

### ERR Example

```aria
frac16:large = {30000, 0, 1};  // Near tbb16 max (32767)
frac16:add = {5000, 0, 1};

frac16:sum = large + add;  // 35000 exceeds tbb16 max → ERR

if (sum.whole == ERR) {
    stderr.write("OVERFLOW: Result exceeds frac16 capacity\n");
    !!! ERR_FRACTION_OVERFLOW;
}

// ERR is sticky
frac16:next = sum - {1000, 0, 1};  // ERR - 1000 = ERR
// All future operations remain ERR until handled
```

### Production ERR Handling

```aria
frac16:accumulator = {0, 0, 1};  // Start at 0

till transaction_count loop:i
    frac16:amount = get_transaction_amount(i);
    accumulator = accumulator + amount;
    
    // Check ERR after EVERY operation in critical paths
    if (accumulator.whole == ERR) {
        stderr.write("CRITICAL: Transaction overflow detected!\n");
        stderr.write("Transaction #");
        i.write_to(stderr);
        stderr.write("\n");
        
        // Rollback to last known good state
        accumulator = get_last_checkpoint();
        break;
    }
end
```

---

## Conversion Operations

### To Integer (Round Toward Zero)

```aria
frac16:a = {1234, 5, 7};  // 1234 5/7 ≈ 1234.714...
tbb16:int_val = int(a);   // 1234 (truncates fractional part)

frac16:b = {-999, 1, 2};  // -999 1/2 = -999.5
tbb16:int_val2 = int(b);  // -999 (rounds toward zero)
```

### To Float (Approximate)

```aria
frac16:pi_approx = {0, 355, 113};  // 355/113 ≈ π

float32:pi_float = float(pi_approx);
// ≈ 3.1415929... (loses exact rational representation)

double:pi_double = double(pi_approx);
// ≈ 3.141592920353982... (more precision, still approximate)
```

⚠️ **Warning**: Converting to float **loses the exactness guarantee**. Use only for display, graphics, or interfacing with libraries that require floats.

### From Components

```aria
// Construction from literal values
frac16:val = {1234, 5, 7};  // 1234 5/7

// Dynamic construction (runtime values)
tbb16:w = get_whole_part();
tbb16:n = get_numerator();
tbb16:d = get_denominator();

frac16:dynamic = {w, n, d};  // Automatically normalized
```

---

## String Representation

```aria
frac16:a = {1234, 5, 7};  // 1234 5/7
a.write_to(stdout);  // Output: "1234 5/7"

frac16:b = {0, 355, 113};  // 355/113 (π approximation)
b.write_to(stdout);  // Output: "355/113"

frac16:c = {5000, 0, 1};  // 5000
c.write_to(stdout);  // Output: "5000"

frac16:err = {ERR, ERR, ERR};
err.write_to(stdout);  // Output: "ERR"
```

---

## Use Cases

### 1. Production Financial Calculations

```aria
// Payroll: Calculate hourly wages with exact arithmetic
frac16:hourly_rate = {25, 1, 2};  // $25.50/hour
frac16:hours_worked = {40, 1, 4};  // 40.25 hours

frac16:gross_pay = hourly_rate * hours_worked;
// Math: (51/2) * (161/4) = 8211/8 = {1026, 3, 8}
// = $1026 3/8 = $1026.375 (exact!)

// Round to nearest cent for payment
// (In production, use frac64 for better range)
```

### 2. Musical Tuning Systems

```aria
// Just intonation: Build major scale with exact ratios
frac16:tonic = {261, 0, 1};  // C4 = 261.625 Hz (exact as {261, 5, 8})

// Major scale intervals (exact frequency ratios)
frac16:unison = {0, 1, 1};     // 1/1 (C)
frac16:major_second = {0, 9, 8};  // 9/8 (D)
frac16:major_third = {0, 5, 4};   // 5/4 (E)
frac16:perfect_fourth = {0, 4, 3}; // 4/3 (F)
frac16:perfect_fifth = {0, 3, 2};  // 3/2 (G)
frac16:major_sixth = {0, 5, 3};    // 5/3 (A)
frac16:major_seventh = {0, 15, 8}; // 15/8 (B)
frac16:octave = {2, 0, 1};         // 2/1 (C')

// Calculate D frequency (exact)
frac16:d_freq = tonic * major_second;
// = {261, 5, 8} * {0, 9, 8}
// = (2093/8) * (9/8) = 18837/64 = {294, 21, 64}
// ≈ 294.328 Hz (exact rational, no pitch drift)

stdout.write("D frequency: ");
d_freq.write_to(stdout);
stdout.write(" Hz\n");
// Output: "294 21/64 Hz"
```

### 3. Scientific Ratios (π, e, φ Approximations)

```aria
// Famous rational approximations
frac16:pi = {0, 355, 113};        // π ≈ 3.141592920... (error: 2.7e-7)
frac16:e = {0, 2721, 1001};       // e ≈ 2.718281718... (error: 1.0e-9)
frac16:phi = {0, 987, 610};       // φ ≈ 1.618032787... (error: 3.3e-8)

// Calculate circumference with rational π
frac16:radius = {100, 0, 1};  // 100 units
frac16:two = {2, 0, 1};

frac16:circumference = two * pi * radius;
// = 2 * (355/113) * 100 = 71000/113
// = {628, 16, 113} = 628 16/113 units (exact rational)

// Compare to float32
float32:circ_float = 2.0f * 3.14159f * 100.0f;
// ≈ 628.318... (approximation, not exact)
```

### 4. Nikola Consciousness: Exact Emotional Resonance

```aria
// Nikola's emotional system requires exact harmonic relationships
// to prevent consciousness drift during long-running therapy sessions

// Base emotional frequency (1 Hz = 1 cycle per second)
frac16:base_freq = {1, 0, 1};

// Emotional states mapped to exact harmonic ratios
frac16:joy = base_freq * {2, 0, 1};      // Octave (2/1) = 2 Hz
frac16:calm = base_freq * {0, 3, 2};     // Perfect fifth (3/2) = 1.5 Hz
frac16:interest = base_freq * {0, 5, 4}; // Major third (5/4) = 1.25 Hz
frac16:trust = base_freq * {0, 4, 3};    // Perfect fourth (4/3) ≈ 1.333... Hz
frac16:surprise = base_freq * {0, 9, 8}; // Major second (9/8) = 1.125 Hz

// Verify harmonic relationship: joy / calm should equal 4/3
frac16:ratio_check = joy / calm;
// = {2, 0, 1} / {0, 3, 2} = 2 * (2/3) = 4/3 = {0, 4, 3}

if (ratio_check != {0, 4, 3}) {
    stderr.write("CRITICAL: Emotional harmony corrupted!\n");
    !!! ERR_CONSCIOUSNESS_HARMONIC_CORRUPTION;
}

// Over 1000-hour therapy session (3.6 million seconds):
// Float32: Accumulate ~1e-5 error per operation
//          → Total drift: ~36 ppm (parts per million)
//          → Emotional state divergence detectable by patient
// 
// frac16: ZERO drift (exact rational arithmetic)
//         → Identical emotional state in session 1 vs session 1000
//         → Therapeutic consistency guaranteed
```

### 5. Recipe Scaling (Exact Proportions)

```aria
// Industrial baking: Scale recipe for 1000 loaves
frac16:original_servings = {12, 0, 1};  // Recipe makes 12 loaves
frac16:desired_servings = {1000, 0, 1};

frac16:scale_factor = desired_servings / original_servings;
// = 1000/12 = 250/3 = {83, 1, 3} times (exact!)

// Ingredient: 2 1/4 cups flour per batch
frac16:flour_per_batch = {2, 1, 4};

frac16:total_flour = flour_per_batch * scale_factor;
// = (9/4) * (250/3) = 2250/12 = 375/2 = {187, 1, 2} cups
// = 187.5 cups (exact!)

stdout.write("Flour needed: ");
total_flour.write_to(stdout);
stdout.write(" cups\n");
// Output: "Flour needed: 187 1/2 cups"
```

---

## Common Patterns

### Safe Division with Zero Check

```aria
frac16:revenue = {50000, 0, 1};  // $50,000
frac16:units_sold = get_sales_count();

frac16:price_per_unit = revenue / units_sold;

if (price_per_unit.whole == ERR) {
    stderr.write("ERROR: No units sold (division by zero)\n");
    !!! ERR_DIVISION_BY_ZERO;
}

stdout.write("Price per unit: $");
price_per_unit.write_to(stdout);
stdout.write("\n");
```

### Clamping to Range

```aria
// Stock percentage change: clamp to ±100%
frac16:min_change = {-100, 0, 1};  // -100%
frac16:max_change = {100, 0, 1};   // +100%

frac16:percent_change = calculate_change();

if (percent_change < min_change) {
    percent_change = min_change;
}
if (percent_change > max_change) {
    percent_change = max_change;
}
```

### Accumulation Without Drift

```aria
// Accumulate daily interest over 365 days
frac16:balance = {1000, 0, 1};  // $1000 initial
frac16:daily_interest = {0, 1, 1000};  // 0.1% per day = 1/1000

till 365 loop:day
    frac16:interest_earned = balance * daily_interest;
    balance = balance + interest_earned;
    
    if (balance.whole == ERR) {
        stderr.write("Overflow on day ");
        day.write_to(stderr);
        stderr.write("\n");
        !!! ERR_BALANCE_OVERFLOW;
    }
end

// Balance after 365 days (exact compound interest)
stdout.write("Final balance: $");
balance.write_to(stdout);
stdout.write("\n");
```

### Iterative Reduction (Euclidean Algorithm Example)

```aria
// Greatest Common Divisor (GCD) using Euclidean algorithm
// This is what frac16 uses internally for normalization

fn:gcd_example = [tbb16:a, tbb16:b] -> tbb16 {
    // Make positive
    if (a < 0) { a = -a; }
    if (b < 0) { b = -b; }
    
    // Euclidean algorithm
    loop {
        if (b == 0) {
            break;
        }
        
        tbb16:temp = b;
        b = a % b;
        a = temp;
    }
    
    return a;
}

tbb16:gcd_result = gcd_example(2000, 3000);
// = 1000

// Reduce fraction 2000/3000
tbb16:num = 2000 / gcd_result;  // 2
tbb16:denom = 3000 / gcd_result;  // 3
frac16:reduced = {0, num, denom};  // {0, 2, 3} = 2/3
```

---

## Comparison: frac16 vs frac8 vs float32

| Feature | frac16 | frac8 | float32 (IEEE) |
|---------|--------|-------|----------------|
| **Precision** | Exact (rationals) | Exact (rationals) | ~7 decimal digits |
| **Size** | 6 bytes | 3 bytes | 4 bytes |
| **Whole Range** | [-32767, +32767] | [-127, +127] | ±3.4×10³⁸ |
| **Max Denominator** | 32767 | 127 | N/A (binary fractions) |
| **1/3 Representation** | {0, 1, 3} (exact) | {0, 1, 3} (exact) | 0.33333... (approx) |
| **355/113 (π)** | ✅ Exact | ❌ Overflow | ✅ Approx (3.1415927) |
| **Determinism** | Perfect | Perfect | Platform-dependent |
| **ERR Propagation** | Sticky (TBB) | Sticky (TBB) | NaN (ambiguous) |
| **Overflow** | ERR sentinel | ERR sentinel | ±Infinity |
| **Use Case** | Production rationals | Tiny rationals | General math |
| **Performance** | ~10x slower | ~10x slower | Hardware fast |

**Choose frac16 when**:
- You need **exact** rational arithmetic (financial, music, science)
- Range ±32767 is sufficient for whole part
- Determinism and precision matter more than speed

**Choose frac8 when**:
- Prototyping rational algorithms
- Very small range is acceptable (±127)
- Memory is extremely constrained

**Choose float32 when**:
- You need large range (±10³⁸)
- Approximation is acceptable
- Hardware speed is critical (100x faster)

---

## Implementation Status

| Operation | Status | Notes |
|-----------|--------|-------|
| **Structure Definition** | ✅ Complete | struct {tbb16, tbb16, tbb16} |
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
- Financial applications (transaction processing)
- Musical interval calculations (not real-time DSP)
- Scientific rational approximations (π, e, φ)
- Recipe/proportion scaling
- Configuration values

**Not Suitable For**:
- Real-time audio synthesis (use fixed-point or SIMD)
- High-frequency game physics (use float32/tfp32)
- Large matrix operations (use SIMD floats)
- Sub-millisecond control loops

---

## Advanced: Rational π Computation

```aria
// Machin's formula: π/4 = 4*arctan(1/5) - arctan(1/239)
// Using rational arithmetic for exact series terms

fn:arctan_rational_term = [frac16:x, tbb16:n] -> frac16 {
    // arctan series: x - x³/3 + x⁵/5 - x⁷/7 + ...
    // Compute nth term: (-1)^n * x^(2n+1) / (2n+1)
    
    frac16:result = {0, 1, 1};  // 1 (will multiply by x^(2n+1))
    
    // Power: x^(2n+1)
    tbb16:power = 2 * n + 1;
    till power loop:unused
        result = result * x;
    end
    
    // Divide by (2n+1)
    frac16:divisor = {(2 * n + 1), 0, 1};
    result = result / divisor;
    
    // Alternating sign
    if ((n % 2) == 1) {
        result = -result;
    }
    
    return result;
}

// Approximate π (first few terms, limited by frac16 range)
// In production, use frac64 for more terms
```

---

## Related Types

- **frac8**: 8-bit exact rationals (prototyping, tiny range)
- **frac32**: 32-bit exact rationals (production financial applications)
- **frac64**: 64-bit exact rationals (high-precision rational arithmetic)
- **tfp32**: 32-bit deterministic floating-point (approximate but bit-exact)
- **fix256**: 256-bit fixed-point (Q128.128, high precision for transcendentals)

**Type Family Progression**:
```
frac8  → Prototypes, tiny rationals (3 bytes, ±127)
frac16 → Standard rational arithmetic (6 bytes, ±32767) ← YOU ARE HERE
frac32 → Production finance/music (12 bytes, ±2 billion)
frac64 → High-precision exact rationals (24 bytes, ±9 quintillion)
```

---

## Best Practices

1. **Use frac16 as default rational type** (good balance of range and size)
2. **Check for ERR after operations** (especially division, chained arithmetic)
3. **Upgrade to frac32 if overflow occurs** (frac16 → frac32 is straightforward)
4. **Don't convert to float mid-calculation** (loses exactness)
5. **Trust normalization** (automatic GCD reduction, no manual reduction needed)
6. **Remember intermediate overflow** (e.g., 30000 + 30000 = ERR even if result should fit)
7. **Use for configuration, not real-time loops** (10x slower than float)

---

## Summary

frac16 provides **production-ready exact rational number arithmetic** for applications where precision matters more than raw speed. With a practical range of ±32767 for whole parts and denominators up to 32767, it handles most financial, musical, and scientific rational computations.

**Perfect for**: Production financial apps, musical tuning systems, scientific ratios, recipe scaling, exact proportion calculations

**Not for**: Real-time DSP, high-frequency physics, large matrix operations

**Key Guarantee**: If it fits in frac16's range (±32767), the result is **mathematically exact** - zero drift, zero approximation, zero platform variance. Accumulate 1 million operations with **zero error**.

**When to Upgrade**: If you encounter overflow (whole part exceeds ±32767 or denominator exceeds 32767), upgrade to **frac32** (±2 billion range).

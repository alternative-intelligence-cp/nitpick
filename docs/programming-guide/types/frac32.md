# frac32: 32-bit Exact Rational Numbers

**General-purpose exact rational arithmetic with guaranteed mathematical precision**

---

## Overview

`frac32` is Aria's **32-bit exact rational number type**, representing fractions as mixed numbers with mathematically exact arithmetic. Unlike floating-point types that approximate rational numbers, `frac32` stores fractions precisely using three `tbb32` components: whole number, numerator, and denominator.

**The Problem `frac32` Solves**:
```javascript
// JavaScript (floating-point approximation):
0.1 + 0.2 === 0.3  // FALSE! (equals 0.30000000000000004)
1/3 + 1/3 + 1/3 === 1  // FALSE! (equals 0.9999999999999999)
```

```aria
// Aria (exact rational arithmetic):
frac32:tenth = {0, 1, 10};       // EXACTLY 1/10
frac32:fifth = {0, 2, 10};       // EXACTLY 2/10 = 1/5
frac32:sum = tenth + fifth;      // EXACTLY {0, 3, 10} = 0.3
frac32:third = {0, 1, 3};        // EXACTLY 1/3
frac32:result = third + third + third;  // EXACTLY {1, 0, 1} = 1
```

**Structure**: `struct { tbb32:whole; tbb32:num; tbb32:denom; }`  
**Size**: 12 bytes (3 × 4 bytes), aligned to 16-byte boundary  
**Range**: Whole ±2,147,483,647, numerator/denominator 0-2,147,483,647  
**Error Sentinel**: ERR if any component is ERR or denom = 0  
**Default Choice**: General-purpose exact rationals (like i32/tbb32 for integers)

---

## Purpose and Characteristics

### What is frac32?

`frac32` represents **mixed-fraction rational numbers** in the form:

**Value = whole + (num / denom)**

Where:
- `whole`: Integer part (tbb32, can be negative)
- `num`: Numerator of fractional part (tbb32, ≥ 0 when whole ≠ 0)
- `denom`: Denominator of fractional part (tbb32, always > 0)

**Examples**:
- `{2, 1, 3}` = 2 + 1/3 = 2⅓ = 7/3
- `{0, 3, 4}` = 0 + 3/4 = ¾
- `{-5, 1, 2}` = -5 + 1/2 = -4½ = -9/2
- `{0, -1, 2}` = 0 + (-1/2) = -½ (sign in num when whole = 0)

### Key Characteristics

**Exact Arithmetic**:
- ✅ **No rounding errors**: 1/3 is EXACTLY 1/3, not 0.33333...
- ✅ **No drift**: Repeated operations maintain perfect precision
- ✅ **Deterministic**: Same inputs always produce identical results
- ✅ **Mathematically correct**: (1/3) × 3 = 1 exactly, not 0.999...

**Automatic Normalization**:
- All values maintained in **canonical form** (reduced, proper fraction)
- GCD reduction happens automatically
- Denominator always positive
- Numerator < denominator (proper fraction)

**Sticky Error Propagation**:
- Built on TBB types → inherits ERR sentinel mechanism
- Any operation involving ERR produces ERR
- Division by zero → ERR (not crash or exception)
- Overflow → ERR (not wraparound or undefined behavior)

**Safety First**:
- No undefined behavior (unlike IEEE 754 NaN propagation)
- No silent failures (errors are explicit ERR states)
- No precision loss (unless converting to float)
- No accumulation errors (exact at every step)

---

## When to Use frac32

### ✅ Ideal Use Cases

**Financial Calculations** (legal/regulatory exactness):
```aria
frac32:principal = {1000000, 0, 1};     // $1,000,000
frac32:interest_rate = {0, 5, 100};     // 5% (exactly 5/100, not 0.05)
frac32:interest = principal * interest_rate;  // EXACTLY $50,000
frac32:share = {0, 1, 3};               // 33.333...% partner share
frac32:payout = principal * share;      // EXACTLY $333,333.33...
```

**Music & Audio** (exact harmonic ratios):
```aria
frac32:octave = {2, 0, 1};              // 2:1 ratio (exactly)
frac32:fifth = {3, 0, 2};               // 3:2 perfect fifth
frac32:major_third = {5, 0, 4};         // 5:4 just intonation
frac32:frequency = {440, 0, 1};         // A440 Hz
frac32:harmonic = frequency * fifth;    // 660 Hz (exact E5)
```

**Scientific Calculations** (stoichiometry, dilutions):
```aria
frac32:molar_ratio = {2, 0, 3};         // 2:3 reactant ratio
frac32:dilution = {1, 0, 10};           // 1:10 dilution factor
frac32:concentration = {5, 0, 1} * dilution;  // Exact 0.5 M
```

**Recipe Scaling** (no accumulation errors):
```aria
frac32:cup_third = {0, 1, 3};           // ⅓ cup
frac32:scaled = cup_third * {3, 0, 1};  // EXACTLY 1 cup (not 0.999...)
frac32:half = {0, 1, 2};                // ½ teaspoon
frac32:doubled = half * {2, 0, 1};      // EXACTLY 1 teaspoon
```

**Physics Simulations** (phase relationships, wave interference):
```aria
frac32:phase_offset = {0, 1, 7};        // π/7 phase offset (exact)
frac32:wavelength = {633, 0, 1};        // 633 nm (HeNe laser)
frac32:interference = phase_offset * wavelength;  // No drift over time
```

**Legal Contracts** (equity splits, profit sharing):
```aria
frac32:founder1 = {0, 40, 100};         // Exactly 40%
frac32:founder2 = {0, 35, 100};         // Exactly 35%
frac32:investor = {0, 25, 100};         // Exactly 25%
frac32:total = founder1 + founder2 + investor;  // EXACTLY {1, 0, 1} = 100%
```

### ❌ When NOT to Use frac32

**Irrational Numbers** (π, e, √2):
- frac32 cannot represent irrational numbers exactly
- Use tfp32/tfp64 for transcendental functions
- Performance cost: Exact arithmetic slower than floating-point

**High-frequency sensor data**:
- 12-byte size (vs 4-byte f32) increases memory/bandwidth
- Consider f32 for sensor readings if exactness not critical
- Use frac32 for final calculations, not raw readings

**Graphics/3D rendering**:
- GPU hardware expects f32/f64, not rational numbers
- Conversion overhead negates exactness benefits
- Use f32 for vertex positions, colors, matrices

**Performance-critical loops** (high-volume calculations):
- Exact arithmetic requires GCD, multiplication, normalization
- ~10-100× slower than floating-point operations
- Profile first: Exactness may outweigh speed concerns

**Very large denominators** (beyond tbb32 range):
- Denominator limited to ±2,147,483,647
- Use frac64 if denominators exceed tbb32 range
- Consider if precision requirement justifies 24-byte size

---

## Syntax and Literals

### Structure Initialization (Primary Syntax)

The canonical way to create `frac32` values:

```aria
// Basic fractions (whole part is zero)
frac32:half = {0, 1, 2};                // ½
frac32:third = {0, 1, 3};               // ⅓
frac32:quarter = {0, 1, 4};             // ¼
frac32:three_quarters = {0, 3, 4};      // ¾

// Mixed numbers (whole + fraction)
frac32:two_and_half = {2, 1, 2};        // 2½
frac32:three_and_third = {3, 1, 3};     // 3⅓
frac32:neg_five_half = {-5, 1, 2};      // -5½ = -4.5

// Negative fractions (sign handling)
frac32:neg_half = {0, -1, 2};           // -½ (sign in num when whole=0)
frac32:neg_quarter = {0, -1, 4};        // -¼
frac32:neg_mixed = {-2, 1, 3};          // -2⅓ (sign in whole)

// Whole numbers (denominator = 1)
frac32:ten = {10, 0, 1};                // 10
frac32:hundred = {100, 0, 1};           // 100
frac32:zero = {0, 0, 1};                // 0
```

### Sign Representation Rules

**Critical**: Sign handling follows **strictly additive model**:

1. **Whole ≠ 0**: Sign lives in `whole`, numerator stays positive
   ```aria
   frac32:pos_mixed = {2, 1, 3};        // +2 + 1/3 = 7/3
   frac32:neg_mixed = {-2, 1, 3};       // -2 + 1/3 = -5/3
   ```

2. **Whole = 0**: Sign lives in `num` (handles -1 < value < 0)
   ```aria
   frac32:pos_frac = {0, 3, 4};         // +3/4
   frac32:neg_frac = {0, -3, 4};        // -3/4
   ```

3. **Denominator always positive** (enforced by normalization)

### Compile-time Constants (Future Syntax)

*Not yet implemented - planned for future versions:*

```aria
// Potential future syntax for readability
frac32:pi_approx = 22/7;                // Compile-time constant 22/7
frac32:mixed = 3 + 1/2;                 // Parser evaluates to {3, 1, 2}
```

Currently use explicit structure initialization.

### ERR Sentinel

`frac32` is ERR if:
- Any component (whole, num, denom) is TBB ERR
- Denominator is zero (division by zero)
- Overflow during normalization

```aria
frac32:zero_denom = {1, 1, 0};          // ERR (denom = 0)
frac32:overflow = {tbb32:ERR, 0, 1};    // ERR (whole is ERR)
frac32:calc_err = {1, 1, 0} + {2, 0, 1};  // ERR (sticky propagation)

// Checking for ERR
if (value.whole == tbb32:ERR || value.denom == 0) {
    // Handle error state
}
```

---

## Canonical Form Invariants

All `frac32` values maintain strict **canonical form** through automatic normalization.

### The Five Invariants

1. **Denominator always positive**: `denom > 0`
2. **Proper fraction**: `0 ≤ num < denom` (unless whole = 0 and value negative)
3. **Reduced to lowest terms**: `gcd(num, denom) = 1`
4. **Sign handling**:
   - If `whole ≠ 0`: Sign in `whole`, `num ≥ 0`
   - If `whole = 0`: Sign in `num`
5. **No zero denominator**: `denom ≠ 0` (or ERR state)

### Normalization Examples

**Before normalization** → **After normalization**:
```aria
{0, 2, 4}   → {0, 1, 2}       // GCD reduction (gcd(2,4) = 2)
{0, 6, 8}   → {0, 3, 4}       // GCD reduction (gcd(6,8) = 2)
{0, 5, 3}   → {1, 2, 3}       // Improper to mixed (5/3 = 1 remainder 2)
{2, -1, 3}  → {1, 2, 3}       // Sign correction (2 + (-1/3) = 1 + 2/3)
{-3, -1, 2} → {-3, 1, 2}      // Denominator sign fix
{0, -6, 4}  → {0, -3, 2}      // GCD reduction preserves sign
```

**Automatic normalization** ensures:
```aria
frac32:a = {0, 2, 4};         // Stored as {0, 1, 2} internally
frac32:b = {0, 3, 6};         // Stored as {0, 1, 2} internally
a == b;                       // TRUE! Both canonicalized to ½
```

---

## Operations

### Arithmetic Operations

**Addition**:
```aria
frac32:a = {1, 1, 3};         // 1⅓ = 4/3
frac32:b = {0, 1, 6};         // 1/6
frac32:sum = a + b;           // 1½ = {1, 1, 2}

// Algorithm: (a_whole + a_num/a_denom) + (b_whole + b_num/b_denom)
// = (a_whole + b_whole) + (a_num*b_denom + b_num*a_denom)/(a_denom*b_denom)
// Normalize to canonical form
```

**Subtraction**:
```aria
frac32:c = {5, 1, 4};         // 5¼ = 21/4
frac32:d = {2, 1, 2};         // 2½ = 5/2
frac32:diff = c - d;          // 2¾ = {2, 3, 4}

// Handles sign transitions correctly
frac32:e = {0, 1, 4};         // ¼
frac32:f = {0, 3, 4};         // ¾
frac32:neg_result = e - f;    // -½ = {0, -1, 2}
```

**Multiplication**:
```aria
frac32:g = {2, 1, 3};         // 2⅓ = 7/3
frac32:h = {0, 3, 4};         // ¾
frac32:product = g * h;       // 1¾ = {1, 3, 4}

// Algorithm: (a_whole + a_num/a_denom) × (b_whole + b_num/b_denom)
// Convert to improper fractions, multiply, normalize
// (7/3) × (3/4) = 21/12 = 7/4 = 1¾
```

**Division**:
```aria
frac32:i = {3, 0, 1};         // 3
frac32:j = {0, 2, 3};         // ⅔
frac32:quotient = i / j;      // 4½ = {4, 1, 2}

// Algorithm: (a/b) = a × (1/b) = a × (b_denom/b_num)
// 3 ÷ (2/3) = 3 × (3/2) = 9/2 = 4½

// Division by zero → ERR
frac32:zero = {0, 0, 1};      // 0
frac32:err = i / zero;        // ERR (sticky propagation)
```

**Negation**:
```aria
frac32:k = {2, 1, 3};         // 2⅓
frac32:neg_k = -k;            // -2⅓ = {-2, 1, 3}

frac32:l = {0, 3, 4};         // ¾
frac32:neg_l = -l;            // -¾ = {0, -3, 4}
```

### Comparison Operations

**Equality** (exact comparison):
```aria
frac32:m = {0, 1, 2};         // ½
frac32:n = {0, 2, 4};         // Normalized to {0, 1, 2}
m == n;                       // TRUE (both ½ after normalization)

frac32:o = {0, 1, 3};         // ⅓
m == o;                       // FALSE (½ ≠ ⅓)
```

**Ordering** (< > <= >=):
```aria
frac32:p = {1, 1, 3};         // 1⅓
frac32:q = {1, 1, 2};         // 1½
p < q;                        // TRUE (1⅓ < 1½)
q > p;                        // TRUE (1½ > 1⅓)

// Algorithm: Compare a_whole - b_whole first
// If equal, compare a_num/a_denom vs b_num/b_denom
// Use cross-multiplication to avoid division: a_num*b_denom vs b_num*a_denom
```

**Three-way comparison** (<=>):
```aria
frac32:r = {2, 0, 1};         // 2
frac32:s = {3, 0, 1};         // 3
frac32:t = {2, 0, 1};         // 2

r <=> s;                      // -1 (r < s)
s <=> r;                      // +1 (s > r)
r <=> t;                      // 0 (r == t)
```

### ERR Propagation (Sticky Errors)

All operations propagate ERR automatically:

```aria
frac32:valid = {1, 1, 2};     // 1½
frac32:error = {1, 1, 0};     // ERR (denom = 0)

frac32:result1 = valid + error;        // ERR
frac32:result2 = error * {2, 0, 1};    // ERR
frac32:result3 = {3, 0, 1} / error;    // ERR
frac32:result4 = -error;               // ERR

// Entire calculation chain halts at first error
frac32:chain = (valid + error) * {5, 0, 1} + {1, 0, 1};  // ERR
```

---

## Type Conversions

### To frac32 (Widening/Exact)

**From TBB types** (exact conversion):
```aria
tbb32:whole_val = 42tbb32;
frac32:from_tbb = {whole_val, 0, 1};   // 42 (exact)

tbb32:neg_val = -17tbb32;
frac32:from_neg = {neg_val, 0, 1};     // -17 (exact)
```

**From smaller frac types** (exact widening):
```aria
frac8:small = {2, 1, 3};      // 2⅓ (frac8)
// Convert via tbb_widen<T>() to preserve ERR sentinels
frac32:widened = {
    tbb_widen<tbb32>(small.whole),
    tbb_widen<tbb32>(small.num),
    tbb_widen<tbb32>(small.denom)
};  // 2⅓ (frac32, exact)
```

**From standard integers** (explicit construction):
```aria
i32:int_val = 100;
// Must explicitly create frac32 structure
frac32:from_int = {convert<tbb32>(int_val), 0, 1};  // 100
```

### From frac32 (Narrowing/Lossy)

**To floating-point** (loses exactness!):
```aria
frac32:exact = {0, 1, 3};     // Exactly ⅓
// Pseudo-syntax for conversion (implementation-defined)
f64:approx = frac_to_f64(exact);  // 0.3333333333333333 (approximate!)

frac32:tenth = {0, 1, 10};    // Exactly 1/10
f64:approx2 = frac_to_f64(tenth);  // 0.1 (NOT exact in f64!)
```

**To smaller frac types** (potential overflow):
```aria
frac32:large = {1000000, 1, 2};   // 1,000,000½
// Narrowing to frac8: whole exceeds tbb8 range [-127, +127]
// Result: ERR (overflow)

frac32:small_enough = {100, 1, 4};  // 100¼
// Narrowing to frac16: within tbb16 range [-32767, +32767]
// Possible, but requires explicit conversion with checks
```

**To integer types** (truncation):
```aria
frac32:mixed = {7, 2, 3};     // 7⅔
// Truncate fractional part (implementation-defined)
tbb32:truncated = mixed.whole;     // 7 (drops ⅔ fractional part)

// Rounding (would require stdlib function)
// tbb32:rounded = round(mixed);   // 8 (round to nearest)
```

---

## Common Patterns

### Exact Financial Calculations

**Percentage calculations** (no rounding errors):
```aria
frac32:amount = {1000, 0, 1};         // $1000.00
frac32:tax_rate = {0, 725, 10000};    // 7.25% (exactly 725/10000)
frac32:tax = amount * tax_rate;       // $72.50 (exact!)
frac32:total = amount + tax;          // $1072.50 (exact!)

// Compare to floating-point drift:
// f64: 1000.0 * 0.0725 = 72.50000000000001 (WRONG!)
```

**Compound interest** (exact accumulation):
```aria
frac32:principal = {10000, 0, 1};     // $10,000
frac32:rate = {0, 5, 100};            // 5% annual
frac32:years = {10, 0, 1};            // 10 years

// Simplified compound interest calculation
frac32:multiplier = {1, 0, 1} + rate; // 1.05 (exact)
// For exact exponentiation, would need loop or library function
// Interest calculation remains exact at each step (no drift)
```

**Profit splits** (exact equity):
```aria
frac32:revenue = {1000000, 0, 1};     // $1M revenue
frac32:alice_share = {0, 40, 100};    // 40% exactly
frac32:bob_share = {0, 35, 100};      // 35% exactly
frac32:charlie_share = {0, 25, 100};  // 25% exactly

frac32:alice_payout = revenue * alice_share;      // $400,000 exact
frac32:bob_payout = revenue * bob_share;          // $350,000 exact
frac32:charlie_payout = revenue * charlie_share;  // $250,000 exact

// Verify total = revenue (no rounding error accumulation)
frac32:total = alice_payout + bob_payout + charlie_payout;
total == revenue;  // TRUE! (exactly $1M)
```

### Music Interval Ratios

**Just intonation** (perfect harmonic ratios):
```aria
frac32:fundamental = {440, 0, 1};     // A440 Hz
frac32:octave = {2, 0, 1};            // 2:1 ratio
frac32:fifth = {3, 0, 2};             // 3:2 perfect fifth
frac32:major_third = {5, 0, 4};       // 5:4 major third
frac32:minor_third = {6, 0, 5};       // 6:5 minor third

frac32:a5 = fundamental * octave;     // 880 Hz exact
frac32:e5 = fundamental * fifth;      // 660 Hz exact (E5)
frac32:c_sharp = fundamental * major_third;  // 550 Hz exact (C#5)

// No frequency drift over time (critical for synthesis)
```

**Phase relationships** (wave interference):
```aria
frac32:wavelength = {633, 0, 1};      // 633 nm (HeNe laser)
frac32:phase_offset = {0, 1, 4};      // π/4 phase offset
frac32:double_slit = {0, 1, 2};       // π/2 between slits

frac32:path_diff = wavelength * phase_offset;  // Exact interference pattern
// No cumulative error over millions of wave cycles
```

### Recipe Scaling

**Exact ingredient proportions**:
```aria
frac32:flour_base = {2, 1, 2};        // 2½ cups flour
frac32:sugar_base = {0, 3, 4};        // ¾ cup sugar
frac32:butter_base = {0, 1, 2};       // ½ cup butter

frac32:scale_factor = {3, 0, 1};      // Triple the recipe

frac32:flour_scaled = flour_base * scale_factor;    // 7½ cups (exact!)
frac32:sugar_scaled = sugar_base * scale_factor;    // 2¼ cups (exact!)
frac32:butter_scaled = butter_base * scale_factor;  // 1½ cups (exact!)

// No "close enough" approximations - exact measurements
```

### Scientific Ratios

**Stoichiometry** (chemical reactions):
```aria
frac32:h2_ratio = {2, 0, 1};          // 2 H₂
frac32:o2_ratio = {1, 0, 1};          // 1 O₂
frac32:h2o_ratio = {2, 0, 1};         // 2 H₂O

frac32:h2_moles = {5, 0, 1};          // 5 moles H₂
frac32:o2_needed = h2_moles * (o2_ratio / h2_ratio);  // 2.5 moles O₂ exact
frac32:h2o_produced = h2_moles * (h2o_ratio / h2_ratio);  // 5 moles H₂O exact
```

**Dilution calculations**:
```aria
frac32:stock_conc = {10, 0, 1};       // 10 M stock solution
frac32:dilution_factor = {1, 0, 100}; // 1:100 dilution
frac32:final_conc = stock_conc * dilution_factor;  // 0.1 M exact (1/10 M)

// Series dilutions maintain exactness
frac32:second_dilution = final_conc * {1, 0, 10};  // 0.01 M exact
```

---

## Common Mistakes and Anti-patterns

### ❌ WRONG: Using f64 for Financial Calculations

```aria
// WRONG: Floating-point for money (loses cents!)
f64:price = 19.99;
f64:quantity = 3.0;
f64:total = price * quantity;  // 59.970000000000006 (WRONG!)

// CORRECT: frac32 for exact currency
frac32:price_exact = {19, 99, 100};   // $19.99 exactly
frac32:quantity_exact = {3, 0, 1};    // 3 items
frac32:total_exact = price_exact * quantity_exact;  // $59.97 exactly!
```

### ❌ WRONG: Ignoring ERR State

```aria
// WRONG: Not checking for division by zero
frac32:numerator = {10, 0, 1};
frac32:denominator = {0, 0, 1};  // Zero!
frac32:result = numerator / denominator;  // ERR!
// ... continue using result (propagates ERR silently)

// CORRECT: Check for ERR before critical operations
if (denominator.whole == 0 && denominator.num == 0) {
    // Handle zero denominator case
    failsafe(ERR_DIV_ZERO);
}
frac32:result = numerator / denominator ? {0, 0, 1};  // Default to 0 on ERR
```

### ❌ WRONG: Manually Creating Non-canonical Forms

```aria
// WRONG: Creating unreduced fractions manually
frac32:improper = {0, 10, 6};  // Should be {1, 2, 3}
frac32:unreduced = {0, 4, 8};  // Should be {0, 1, 2}

// Library normalizes automatically, but avoid manual construction
// CORRECT: Trust normalization or construct directly
frac32:proper = {1, 2, 3};     // Already canonical
frac32:reduced = {0, 1, 2};    // Already canonical
```

### ❌ WRONG: Comparing frac32 to f64 Directly

```aria
// WRONG: Comparing exact to approximate
frac32:exact_third = {0, 1, 3};       // Exactly ⅓
f64:approx_third = 0.333333333333;    // Approximation
// Cannot compare directly (type mismatch)

// CORRECT: Convert one to the other (loses exactness)
f64:converted = frac_to_f64(exact_third);
if (abs(converted - approx_third) < 1e-10) { /* close enough */ }

// BETTER: Keep everything in frac32
frac32:another_third = {0, 1, 3};
exact_third == another_third;  // TRUE (exact comparison)
```

### ❌ WRONG: Exceeding tbb32 Range

```aria
// WRONG: Denominator overflow
frac32:huge_denom = {0, 1, 2000000000};  // 1 / 2,000,000,000
frac32:overflow = huge_denom + huge_denom;  // Denom overflows tbb32!
// Result: ERR (denom exceeds ±2,147,483,647)

// CORRECT: Use frac64 for large denominators
frac64:huge_denom64 = {0, 1, 2000000000};  // Within tbb64 range
```

### ❌ WRONG: Using frac32 for Irrational Numbers

```aria
// WRONG: Trying to represent π exactly
frac32:pi = {22, 0, 7};       // Only approximation (3.142857...)
// π is irrational - cannot be represented exactly as ratio

// CORRECT: Use tfp64/f64 for transcendental functions
tfp64:pi_approx = 3.14159265358979tf;  // Deterministic approximation
// Or use frac32 for known approximations with documented error
```

---

## Integration with Aria's Type System

### Result<frac32> for Fallible Operations

```aria
// Function returning exact division result
func:divide = Result<frac32>(frac32:a, frac32:b) {
    if (b.whole == 0 && b.num == 0) {
        fail("Division by zero");
    }
    pass(a / b);
};

// Usage with Result unwrapping
frac32:a = {10, 0, 1};
frac32:b = {0, 3, 4};
frac32:result = divide(a, b) ? {0, 0, 1};  // Default to 0 on error
```

### frac32 with unknown (Soft Errors)

```aria
// Calculation that might produce undefined result
frac32:sensor_reading = read_sensor();
if (sensor_reading.whole == tbb32:ERR) {
    unknown:sensor_value = unknown;  // Soft error
} else {
    frac32:calibrated = sensor_reading * {1, 5, 100};  // Apply calibration
}
```

### frac32 Arrays and Collections

```aria
// Fixed-size array of exact rationals
frac32[10]:harmonics = [
    {1, 0, 1},    // Fundamental
    {2, 0, 1},    // Octave
    {3, 0, 2},    // Fifth
    {4, 0, 3},    // Fourth
    {5, 0, 4},    // Major third
    {6, 0, 5},    // Minor third
    {7, 0, 6},    // Subminor third
    {8, 0, 7},    // Septimal whole tone
    {9, 0, 8},    // Major second
    {10, 0, 9}    // Minor second
];

// Dynamic collection
Vec<frac32>:recipe_ingredients = Vec.new();
recipe_ingredients.push({2, 1, 2});  // 2½ cups
recipe_ingredients.push({0, 3, 4});  // ¾ cup
```

---

## Performance Characteristics

### Memory Layout

**Size**: 12 bytes (3 × `tbb32` at 4 bytes each)  
**Alignment**: 16-byte boundary (4-byte padding)  
**Cache**: Fits in single cache line (64 bytes = ~5 frac32 values)

```
┌─────────────┬─────────────┬─────────────┬─────────┐
│ whole (4B)  │ num (4B)    │ denom (4B)  │ pad(4B) │
├─────────────┴─────────────┴─────────────┴─────────┤
│     frac32 structure (16 bytes aligned)           │
└───────────────────────────────────────────────────┘
```

### Computational Complexity

**Addition/Subtraction**: O(log(max(denom)))
- Cross-multiplication
- GCD calculation
- Normalization

**Multiplication**: O(log(a) + log(b))
- Numerator × numerator, denominator × denominator
- GCD reduction

**Division**: O(log(a) + log(b))
- Reciprocal + multiplication
- GCD reduction

**Comparison**: O(log(denom))
- Cross-multiplication avoids division

### Speed Comparison

| Operation | f32 (baseline) | frac32 (exact) | Factor |
|-----------|----------------|----------------|--------|
| Addition  | 1× (~1 cycle)  | ~50-100× | GCD overhead |
| Multiplication | 1× (~3 cycles) | ~50-100× | GCD + normalize |
| Division  | 1× (~10-20 cycles) | ~50-100× | Reciprocal + GCD |
| Comparison | 1× (~1 cycle) | ~10-20× | Cross-multiply |

**Trade-off**: Exactness for speed. Worth it when correctness > performance.

### Best Practices for Performance

**1. Batch Operations** (minimize GCD calls):
```aria
// Less efficient: Multiple additions
frac32:a = {1, 1, 3};
frac32:sum = a;
till (i in 0...100) {
    sum = sum + a;  // 100 GCD calculations
}

// More efficient: Use multiplication
frac32:result = a * {100, 0, 1};  // Single multiplication + GCD
```

**2. Pre-reduce Constants**:
```aria
// Store commonly used values in canonical form
const frac32:HALF = {0, 1, 2};
const frac32:THIRD = {0, 1, 3};
const frac32:QUARTER = {0, 1, 4};
// Reuse instead of recalculating
```

**3. Delay Conversion to frac32**:
```aria
// Less efficient: Intermediate frac32 values
frac32:a = {convert<tbb32>(x), 0, 1};
frac32:b = {convert<tbb32>(y), 0, 1};
frac32:c = a + b;

// More efficient: Operate on tbb32, convert once
tbb32:sum = x + y;
frac32:result = {sum, 0, 1};
```

**4. Profile Before Optimizing**:
- Exactness often worth the overhead
- Many financial/scientific applications are not performance-bound
- Consider if rounding errors cost more than computation time

---

## Comparison with Similar Types

### frac32 vs f32 (Exact vs Approximate)

| Feature | frac32 | f32 |
|---------|--------|-----|
| **Precision** | Exact rational | ~7 decimal digits |
| **0.1 + 0.2 = 0.3** | ✅ TRUE | ❌ FALSE (0.30000..4) |
| **1/3 representation** | Exactly 1/3 | 0.333333... (approx) |
| **Arithmetic drift** | None (exact) | Accumulates over operations |
| **Size** | 12 bytes | 4 bytes |
| **Speed** | ~50-100× slower | Baseline (hardware) |
| **Use case** | Financial, exact ratios | Graphics, sensors, general |

### frac32 vs frac64 (Width Selection)

| Feature | frac32 | frac64 |
|---------|--------|--------|
| **Whole range** | ±2.1 billion | ±9.2 quintillion |
| **Max denominator** | 2,147,483,647 | 9,223,372,036,854,775,807 |
| **Size** | 12 bytes | 24 bytes |
| **Use case** | General-purpose | High precision, large values |
| **Timestamp fractions** | Seconds, milliseconds | Nanoseconds, astronomy |

**When to use frac64 instead**:
- Denominators exceed 2 billion
- Whole numbers exceed ±2.1 billion
- High-precision scientific calculations
- Astronomical time scales

### frac32 vs frac16 (Width Selection)

| Feature | frac32 | frac16 |
|---------|--------|--------|
| **Whole range** | ±2.1 billion | ±32,767 |
| **Max denominator** | 2,147,483,647 | 32,767 |
| **Size** | 12 bytes | 6 bytes |
| **Use case** | General-purpose | Small exact ratios |

**When to use frac16 instead**:
- Values guaranteed within ±32K
- Memory constrained (embedded systems)
- Audio sample fractions (timing offsets)

### frac32 vs tfp32 (Exact Rational vs Deterministic Float)

| Feature | frac32 | tfp32 |
|---------|--------|-------|
| **Representation** | Exact rational | Deterministic floating-point |
| **1/3** | Exactly 1/3 | Approximation (deterministic) |
| **Irrational numbers** | Cannot represent | Approximates (deterministically) |
| **Transcendental functions** | Not directly supported | sin, cos, sqrt available |
| **Use case** | Exact fractions | General floating-point (deterministic) |

**When to use tfp32 instead**:
- Need irrational numbers (π, e, √2)
- Transcendental functions required
- Deterministic floating-point needed (not exact)

---

## Platform and Implementation

### Bit Representation

**Structure layout** (little-endian example):
```
Offset 0-3:   whole (tbb32)
Offset 4-7:   num (tbb32)
Offset 8-11:  denom (tbb32)
Offset 12-15: padding (alignment)
```

**Example**: `{5, 1, 2}` (representing 5½)
```
Bytes 0-3:  0x00000005  (whole = 5)
Bytes 4-7:  0x00000001  (num = 1)
Bytes 8-11: 0x00000002  (denom = 2)
Bytes 12-15: 0x00000000  (padding)
```

### Endianness

**Structure endianness**: Each `tbb32` component follows system endianness
- Little-endian (x86/x64, ARM64): LSB first
- Big-endian (network, some embedded): MSB first

**Serialization**: Convert each component to network byte order:
```aria
// Pseudo-code for network transmission
func:serialize_frac32 = Result<byte[]>(frac32:value) {
    byte[]:buffer = allocate(12);
    write_be_tbb32(buffer, 0, value.whole);   // Big-endian at offset 0
    write_be_tbb32(buffer, 4, value.num);     // Big-endian at offset 4
    write_be_tbb32(buffer, 8, value.denom);   // Big-endian at offset 8
    pass(buffer);
};
```

### Alignment and Padding

**Alignment requirement**: 16-byte boundary (for SIMD efficiency)
**Padding**: 4 bytes added to reach 16-byte alignment

**Impact on arrays**:
```aria
frac32[100]:array;  // 1600 bytes (100 × 16 bytes)
// Not 1200 bytes (would be misaligned)
```

### Cross-platform Determinism

**Guaranteed**:
- ✅ Same inputs produce identical outputs (all platforms)
- ✅ Normalization is deterministic (GCD algorithm specified)
- ✅ ERR propagation is deterministic (TBB semantics)
- ✅ Comparison results identical (cross-multiplication defined)

**Because**:
- Built on TBB types (deterministic by design)
- Exact arithmetic (no floating-point ambiguity)
- Defined canonical form (unique representation)

---

## Best Practices

### ✅ DO: Use frac32 for Exact Financial Calculations

```aria
frac32:price = {19, 99, 100};         // $19.99 exactly
frac32:tax_rate = {0, 0825, 10000};   // 8.25% exactly (825/10000)
frac32:tax = price * tax_rate;        // Exact tax amount
frac32:total = price + tax;           // Exact total (no rounding)
```

### ✅ DO: Check for ERR Before Using Results

```aria
frac32:result = calculate_ratio(a, b);
if (result.whole == tbb32:ERR || result.denom == 0) {
    // Handle error condition
    failsafe(ERR_INVALID_RATIO);
}
// Safe to use result here
```

### ✅ DO: Use Canonical Form Constants

```aria
// Define exact constants (automatically normalized)
const frac32:PI_APPROX = {22, 0, 7};      // 22/7 = 3.142857...
const frac32:GOLDEN_RATIO = {1618, 0, 1000};  // φ ≈ 1.618
const frac32:SQRT2_APPROX = {1414, 0, 1000};  // √2 ≈ 1.414
```

### ✅ DO: Document Precision Requirements

```aria
// Specify when exactness is required
// EXACT: Financial calculation, must not introduce rounding errors
frac32:profit_share = revenue * {0, 33, 100};  // Exactly 33%

// APPROXIMATE: Physical measurement (sensors have error anyway)
f32:sensor_temp = adc.read_temperature();  // ±0.5°C accuracy
```

### ✅ DO: Use frac64 for High-Precision Needs

```aria
// frac32 denominator limit: 2,147,483,647
frac32:limited = {0, 1, 2000000000};  // Near limit

// If need larger denominators → frac64
frac64:precise = {0, 1, 10000000000};  // 10 billion (frac64)
```

### ❌ DON'T: Use frac32 Where Floats Suffice

```aria
// WRONG: Exact rationals for graphics (overkill)
frac32:vertex_x = {512, 5, 10};       // Screen coordinate

// CORRECT: f32 for graphics (hardware optimized)
f32:vertex_x = 512.5f32;              // Fast, sufficient precision
```

### ❌ DON'T: Mix frac32 with f64 Carelessly

```aria
// WRONG: Mixing exact and approximate (loses exactness)
frac32:exact = {0, 1, 3};             // Exactly 1/3
f64:approx = 0.333333;                // Approximate
// Converting exact → approx loses precision!

// CORRECT: Keep everything exact, or clearly separate
frac32:exact_calc = exact + {0, 2, 3};  // Still exact (1/3 + 2/3 = 1)
```

### ❌ DON'T: Ignore Overflow Potential

```aria
// WRONG: Large intermediate values may overflow
frac32:a = {1000000, 0, 1};           // 1 million
frac32:b = {2000000, 0, 1};           // 2 million
frac32:product = a * b;               // 2 trillion (may overflow tbb32!)

// CORRECT: Use frac64 for large calculations
frac64:a64 = {1000000, 0, 1};
frac64:b64 = {2000000, 0, 1};
frac64:product64 = a64 * b64;         // Safe in tbb64 range
```

---

## Summary

### Key Principles

1. **Exact Arithmetic**: frac32 represents rational numbers mathematically exactly
2. **Canonical Form**: All values automatically normalized (reduced, proper fraction)
3. **Sticky ERR**: Errors propagate through calculations, preventing silent corruption
4. **General-purpose**: Default choice for exact rationals (like tbb32 for integers)
5. **Built on TBB**: Inherits safety guarantees and error propagation
6. **No Drift**: Repeated operations maintain perfect precision

### When to Use frac32

✅ **Perfect for**:
- Financial calculations (currency, percentages, interest)
- Music theory (harmonic ratios, intervals)
- Recipe scaling (exact ingredient proportions)
- Scientific ratios (stoichiometry, dilutions)
- Legal contracts (equity splits, profit sharing)
- Any calculation where exactness is legally/ethically required

❌ **Avoid for**:
- Graphics/3D rendering (use f32)
- High-frequency sensor data (use f32/f64)
- Irrational numbers (use tfp32/tfp64)
- Performance-critical tight loops (profile first)
- GPU compute (not hardware-supported)

### Critical Differences from f32

| Problem | f32 | frac32 |
|---------|-----|--------|
| 0.1 + 0.2 = 0.3 | FALSE | TRUE |
| 1/3 representation | Approximate | Exact |
| (1/3) × 3 = 1 | FALSE | TRUE |
| Accumulation drift | Yes | Never |
| Legal compliance | Risky | Guaranteed |

### The Bottom Line

**frac32 solves one of computing's oldest problems**: representing rational numbers exactly. If your domain requires mathematical precision (finance, music, science, law), use `frac32`. If you need speed and approximate values suffice, use `f32`. Choose based on correctness requirements, not convenience.

**Remember**: "The bitterness of poor results lingers long after the sweetness of meeting the schedule has been forgotten." Exactness matters.

---

*See also*: [frac8](frac8.md), [frac16](frac16.md), [frac64](frac64.md), [tbb32](tbb32.md), [tfp32](tfp32.md), [f32](f32.md)

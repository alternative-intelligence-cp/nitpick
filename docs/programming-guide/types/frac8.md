# frac8: 8-bit Compact Exact Rational Numbers

**Memory-efficient exact rational arithmetic for small fractions and embedded systems**

---

## Overview

`frac8` is Aria's **8-bit compact exact rational number type**, providing exact fraction representation with minimum memory footprint. Using three `tbb8` components, it stores fractions with whole numbers from -127 to +127 and denominators up to 127, making it ideal for embedded systems, small ingredient fractions, and cache-friendly calculations.

**Why frac8 Exists**:
```aria
// frac32 for small values wastes memory
frac32:quarter = {0, 1, 4};          // 12 bytes for simple ¼

// frac8: Same exactness, 75% less memory
frac8:quarter = {0, 1, 4};           // 3 bytes (4-byte aligned)
// 16 frac8 values fit in single 64-byte cache line!
```

**Structure**: `struct { tbb8:whole; tbb8:num; tbb8:denom; }`  
**Size**: 3 bytes (aligned to 4-byte boundary with 1-byte padding)  
**Whole Range**: -127 to +127  
**Denominator Range**: 1 to 127  
**Error Sentinel**: ERR if any component is ERR (-128) or denom = 0  
**Use Case**: Small fractions, embedded systems, recipes, temperature ratios

---

## Purpose and Characteristics

### What is frac8?

`frac8` provides **exact rational arithmetic** in minimal space:

**Value = whole + (num / denom)**

Where all components are `tbb8` (8-bit symmetric integers):
- `whole`: Integer part (-127 to +127)
- `num`: Numerator (0 to 127 when whole ≠ 0)
- `denom`: Denominator (1 to 127, always positive)

**Compact Examples**:
```aria
// Recipe fractions (cups, teaspoons)
frac8:cup_third = {0, 1, 3};         // ⅓ cup
frac8:half_tsp = {0, 1, 2};          // ½ teaspoon
frac8:two_thirds = {0, 2, 3};        // ⅔ cup

// Temperature ratios (Celsius range -127 to +127)
frac8:temp = {25, 5, 10};            // 25.5°C
frac8:freezing = {0, 0, 1};          // 0°C
frac8:boiling = {100, 0, 1};         // 100°C

// Small percentages
frac8:five_percent = {0, 5, 100};    // 5% (5/100)
frac8:half_percent = {0, 1, 200};    // Wait.. 200 > 127! Use {0, 1, 127} ≈ 0.79%
// Limitation: Cannot represent 0.5% exactly (denom 200 exceeds range)
```

### Key Characteristics

**Extreme Compactness**:
- ✅ **3 bytes**: 75% smaller than frac32 (12 bytes), 87.5% smaller than frac64 (24 bytes)
- ✅ **Cache-friendly**: 16 frac8 values per 64-byte cache line (vs 5 frac32)
- ✅ **Embedded-ready**: Minimal RAM footprint for microcontrollers
- ✅ **Array efficient**: 1000 frac8 values = 4KB (vs 12KB for frac32)

**Same Guarantees as Larger frac Types**:
- Exact rational arithmetic (no floating-point drift)
- Automatic normalization to canonical form
- Sticky ERR propagation
- Cross-platform determinism

**Limitations**:
- ⚠️ **Small range**: Whole numbers limited to ±127
- ⚠️ **Limited denominators**: Cannot exceed 127 (no percentages finer than 1/127 ≈ 0.79%)
- ⚠️ **Overflow risk**: Easy to exceed range in calculations

---

## When to Use frac8

### ✅ Ideal Use Cases

**Recipe Scaling** (ingredient fractions):
```aria
// Standard recipe fractions (cups, tablespoons, teaspoons)
frac8:flour = {2, 1, 2};             // 2½ cups
frac8:sugar = {0, 3, 4};             // ¾ cup
frac8:butter = {0, 1, 3};            // ⅓ cup
frac8:vanilla = {0, 1, 2};           // ½ teaspoon

// Scale recipe by 2
frac8:scale = {2, 0, 1};
frac8:flour_2x = flour * scale;      // 5 cups exactly
frac8:sugar_2x = sugar * scale;      // 1½ cups exactly

// Common fractions fit perfectly
frac8:eighth = {0, 1, 8};            // ⅛
frac8:sixteenth = {0, 1, 16};        // 1/16
```

**Small Temperature Ranges** (-127 to +127°C):
```aria
// Temperature with fractional degrees
frac8:room_temp = {21, 5, 10};       // 21.5°C
frac8:body_temp = {37, 2, 10};       // 37.2°C (varies: 36.1 - 37.2°C)
frac8:fever = {39, 5, 10};           // 39.5°C

// Temperature differences
frac8:delta = fever - room_temp;     // 18°C exactly
```

**Music Intervals** (small harmonic ratios):
```aria
// Simple harmonic ratios (denominators ≤ 127)
frac8:octave = {2, 0, 1};            // 2:1
frac8:fifth = {3, 0, 2};             // 3:2 (perfect fifth)
frac8:fourth = {4, 0, 3};            // 4:3 (perfect fourth)
frac8:major_third = {5, 0, 4};       // 5:4
frac8:minor_third = {6, 0, 5};       // 6:5

// Note: Complex ratios may exceed denom=127 limit
```

**Embedded Systems** (minimal RAM):
```aria
// Sensor calibration factors (embedded controller)
frac8:calibration = {1, 5, 100};     // 1.05× gain
frac8:offset = {0, 25, 100};         // +0.25 offset

// Duty cycle fractions (PWM control)
frac8:duty_50 = {0, 1, 2};           // 50% duty cycle
frac8:duty_25 = {0, 1, 4};           // 25% duty cycle
frac8:duty_75 = {0, 3, 4};           // 75% duty cycle
```

**Small exact percentages** (up to 1/127 precision):
```aria
// Percentages representable with denom ≤ 127
frac8:one_percent = {0, 1, 100};     // 1%
frac8:five_percent = {0, 5, 100};    // 5%
frac8:ten_percent = {0, 1, 10};      // 10%
frac8:twentyfive_pct = {0, 1, 4};    // 25%

// Limitation: 0.5% needs denom=200 (too large for frac8!)
// Use frac16 for finer percentages
```

**Pixel/subpixel positioning** (small screen coordinates):
```aria
// Small display (128×128 or smaller)
frac8:pixel_x = {64, 5, 10};         // Pixel 64.5
frac8:pixel_y = {32, 1, 4};          // Pixel 32.25

// Subpixel antialiasing offsets
frac8:offset_x = {0, 1, 3};          // ⅓ pixel offset
frac8:offset_y = {0, 2, 3};          // ⅔ pixel offset
```

### ❌ When NOT to Use frac8

**Values exceed ±127 range**:
```aria
// WRONG: Year 2024 doesn't fit in tbb8 (-128 to +127)
// frac8:year = {2024, 0, 1};  // OVERFLOW! → ERR

// CORRECT: Use frac16 or larger
frac16:year = {2024, 0, 1};          // Fits in tbb16 (±32767)
```

**Denominators exceed 127**:
```aria
// WRONG: 1/1000 needs denom=1000 (exceeds tbb8 max 127)
// frac8:thousandth = {0, 1, 1000};  // OVERFLOW! → ERR

// CORRECT: Use frac16 or larger
frac16:thousandth = {0, 1, 1000};    // Fits in tbb16
```

**Fine percentages** (< 1% precision):
```aria
// WRONG: 0.5% = 1/200, denom exceeds 127
// frac8:half_percent = {0, 1, 200};  // OVERFLOW! → ERR

// CORRECT: Use frac16
frac16:half_percent = {0, 1, 200};   // 0.5% exactly
```

**Large calculations** (intermediate overflow):
```aria
// WRONG: Multiplication easily overflows
frac8:a = {100, 0, 1};               // 100
frac8:b = {2, 0, 1};                 // 2
frac8:product = a * b;               // 200 > 127 → OVERFLOW! ERR

// CORRECT: Use larger type or check range first
frac16:product16 = {100, 0, 1} * {2, 0, 1};  // 200 (fits in frac16)
```

---

## Syntax and Literals

### Structure Initialization

```aria
// Basic fractions
frac8:half = {0, 1, 2};              // ½
frac8:third = {0, 1, 3};             // ⅓
frac8:quarter = {0, 1, 4};           // ¼
frac8:two_thirds = {0, 2, 3};        // ⅔

// Mixed numbers (small whole numbers)
frac8:one_half = {1, 1, 2};          // 1½
frac8:two_thirds_val = {2, 2, 3};    // 2⅔
frac8:max_mixed = {127, 0, 1};       // 127 (tbb8 max)

// Negative fractions
frac8:neg_half = {0, -1, 2};         // -½ (sign in num when whole=0)
frac8:neg_third = {0, -1, 3};        // -⅓
frac8:neg_mixed = {-5, 1, 2};        // -4½ (sign in whole)

// Range limits
frac8:min_whole = {-127, 0, 1};      // -127 (tbb8 min valid value)
frac8:max_denom = {0, 1, 127};       // 1/127 (max denominator)
```

### ERR Sentinel

```aria
frac8:zero_denom = {1, 1, 0};        // ERR (denom = 0)
frac8:err_whole = {-128, 0, 1};      // ERR (whole = tbb8:ERR = -128)
frac8:err_num = {0, -128, 1};        // ERR (num = tbb8:ERR)
frac8:err_prop = zero_denom + {1, 0, 1};  // ERR (sticky propagation)

// Checking for ERR
if (value.whole == -128 || value.num == -128 || value.denom == 0) {
    // Handle error state
    failsafe(ERR_INVALID_FRACTION);
}
```

### Canonical Form (Same Invariants as frac32/64)

```aria
// Automatic normalization
frac8:unreduced = {0, 2, 4};         // Normalized to {0, 1, 2} (½)
frac8:improper = {0, 5, 3};          // Normalized to {1, 2, 3} (1⅔)

// After normalization, all fracs in canonical form
frac8:a = {0, 2, 4};                 // Becomes {0, 1, 2}
frac8:b = {0, 1, 2};                 // Already canonical
a == b;                              // TRUE (both ½)
```

---

## Operations

### Arithmetic Operations

**Addition** (watch for overflow!):
```aria
frac8:a = {1, 1, 3};                 // 1⅓
frac8:b = {0, 1, 6};                 // 1/6
frac8:sum = a + b;                   // 1½ = {1, 1, 2}

// Overflow example
frac8:x = {100, 0, 1};               // 100
frac8:y = {50, 0, 1};                // 50
frac8:overflow = x + y;              // 150 > 127 → ERR!
```

**Multiplication** (high overflow risk):
```aria
frac8:c = {2, 0, 1};                 // 2
frac8:d = {0, 3, 4};                 // ¾
frac8:product = c * d;               // 1½ = {1, 1, 2}

// Overflow
frac8:e = {10, 0, 1};                // 10
frac8:f = {15, 0, 1};                // 15
frac8:overflow = e * f;              // 150 > 127 → ERR!
```

**Division**:
```aria
frac8:g = {3, 0, 1};                 // 3
frac8:h = {0, 2, 3};                 // ⅔
frac8:quotient = g / h;              // 4½ = {4, 1, 2}

// Division by zero
frac8:zero = {0, 0, 1};
frac8:err = g / zero;                // ERR
```

**Negation**:
```aria
frac8:i = {2, 1, 3};                 // 2⅓
frac8:neg_i = -i;                    // -2⅓ = {-2, 1, 3}

frac8:j = {0, 3, 4};                 // ¾
frac8:neg_j = -j;                    // -¾ = {0, -3, 4}
```

### Comparison Operations

```aria
frac8:a = {0, 1, 2};                 // ½
frac8:b = {0, 2, 4};                 // Normalized to ½
frac8:c = {0, 1, 3};                 // ⅓

a == b;                              // TRUE (both ½ after normalization)
a > c;                               // TRUE (½ > ⅓)
c < a;                               // TRUE (⅓ < ½)
```

### ERR Propagation

```aria
frac8:valid = {1, 1, 2};             // 1½
frac8:error = {1, 1, 0};             // ERR (denom = 0)

frac8:r1 = valid + error;            // ERR
frac8:r2 = error * {2, 0, 1};        // ERR
frac8:r3 = {3, 0, 1} / error;        // ERR
```

---

## Type Conversions

### To frac8 (Narrowing/Risky)

**From frac16/32/64** (potential overflow):
```aria
frac16:large = {1000, 1, 2};         // 1000½
// Cannot fit in frac8 (whole exceeds ±127) → ERR

frac16:small = {50, 1, 4};           // 50¼
// Can fit, but requires explicit narrowing + checks
frac8:narrowed = {
    convert<tbb8>(small.whole),      // Check for overflow!
    convert<tbb8>(small.num),
    convert<tbb8>(small.denom)
};
```

**From tbb8** (exact):
```aria
tbb8:temp_c = 25tbb8;
frac8:temp_exact = {temp_c, 0, 1};   // 25°C exactly
```

### From frac8 (Widening/Exact)

**To frac16/32/64** (safe widening):
```aria
frac8:small = {25, 5, 10};           // 25.5

// Widen to frac16 (preserves ERR sentinels)
frac16:medium = {
    tbb_widen<tbb16>(small.whole),
    tbb_widen<tbb16>(small.num),
    tbb_widen<tbb16>(small.denom)
};  // 25.5 (frac16)

// Widen to frac32
frac32:large = {
    tbb_widen<tbb32>(small.whole),
    tbb_widen<tbb32>(small.num),
    tbb_widen<tbb32>(small.denom)
};  // 25.5 (frac32)
```

**To floating-point** (loses exactness):
```aria
frac8:exact = {0, 1, 3};             // Exactly ⅓
f32:approx = frac_to_f32(exact);     // 0.333333... (approximate)
```

**To integer** (truncation):
```aria
frac8:mixed = {7, 2, 3};             // 7⅔
tbb8:truncated = mixed.whole;        // 7 (drops ⅔)
```

---

## Common Patterns

### Recipe Fraction Arithmetic

```aria
// Base recipe (serves 2)
frac8:flour_base = {1, 1, 2};        // 1½ cups
frac8:sugar_base = {0, 1, 2};        // ½ cup
frac8:egg_base = {1, 0, 1};          // 1 egg

// Scale to serve 6 (×3)
frac8:scale = {3, 0, 1};
frac8:flour_6 = flour_base * scale;  // 4½ cups exactly
frac8:sugar_6 = sugar_base * scale;  // 1½ cups exactly
frac8:egg_6 = egg_base * scale;      // 3 eggs exactly

// Common fractions
frac8:tablespoon = {0, 1, 16};       // 1/16 cup (1 Tbsp)
frac8:teaspoon = {0, 1, 48};         // 1/48 cup (1 tsp)
```

### Temperature Offset Calibration

```aria
// Sensor readings with calibration
frac8:raw_temp = {20, 0, 1};         // 20°C (raw sensor)
frac8:calibration = {1, 5, 10};      // +1.5°C offset
frac8:actual_temp = raw_temp + calibration;  // 21.5°C (calibrated)

// Temperature difference
frac8:morning = {15, 5, 10};         // 15.5°C
frac8:afternoon = {22, 0, 1};        // 22°C
frac8:delta = afternoon - morning;   // 6.5°C
```

### Embedded PWM Duty Cycle

```aria
// PWM duty cycles (typical values 0-100%)
frac8:duty_0 = {0, 0, 1};            // 0% (off)
frac8:duty_25 = {0, 1, 4};           // 25%
frac8:duty_50 = {0, 1, 2};           // 50%
frac8:duty_75 = {0, 3, 4};           // 75%
frac8:duty_100 = {1, 0, 1};          // 100% (full on)

// Adjust duty cycle (e.g., dimming LED)
frac8:brightness = duty_75;
frac8:dim_factor = {0, 8, 10};       // 80% of current
frac8:new_brightness = brightness * dim_factor;  // 60% exactly
```

---

## Common Mistakes and Anti-patterns

### ❌ WRONG: Using frac8 for Large Values

```aria
// WRONG: Value exceeds tbb8 range
// frac8:year = {2024, 0, 1};  // OVERFLOW! 2024 > 127

// CORRECT: Use appropriate width
frac16:year = {2024, 0, 1};          // Fits in tbb16
```

### ❌ WRONG: Fine Percentages (Denominator > 127)

```aria
// WRONG: 0.1% = 1/1000, denom exceeds 127
// frac8:tenth_percent = {0, 1, 1000};  // OVERFLOW!

// CORRECT: Use frac16 or larger
frac16:tenth_percent = {0, 1, 1000}; // 0.1% exactly
```

### ❌ WRONG: Large Multiplications

```aria
// WRONG: Intermediate overflow
frac8:a = {50, 0, 1};
frac8:b = {3, 0, 1};
frac8:product = a * b;               // 150 > 127 → ERR!

// CORRECT: Widen first, then multiply
frac16:a16 = widen_to_frac16(a);
frac16:b16 = widen_to_frac16(b);
frac16:product16 = a16 * b16;        // 150 (fits in frac16)
```

### ❌ WRONG: Assuming Same Performance as Integer

```aria
// WRONG: Using frac8 in tight loop assuming i8 speed
till (i in 0...10000) {
    frac8:calc = {i, 1, 2} * scale;  // GCD overhead every iteration!
}

// BETTER: Use i8 for loop, convert once
till (i in 0...10000) {
    i8:temp = i * scale_int;
}
frac8:final = {temp, 0, 1};          // Convert once at end
```

---

## Performance Characteristics

### Memory Layout

**Size**: 3 bytes (whole, num, denom at 1 byte each)  
**Alignment**: 4-byte boundary (1-byte padding)  
**Cache**: **16 frac8 values** per 64-byte cache line!

```
┌────────┬────────┬────────┬────────┐
│whole(1)│ num(1) │denom(1)│ pad(1) │
├────────┴────────┴────────┴────────┤
│  frac8 structure (4 bytes aligned)│
└───────────────────────────────────┘
```

### Cache Efficiency Comparison

| Type | Size (aligned) | Values per cache line (64B) |
|------|----------------|------------------------------|
| **frac8** | 4 bytes | **16 values** |
| frac16 | 8 bytes | 8 values |
| frac32 | 16 bytes | 4 values |
| frac64 | 32 bytes | 2 values |

**frac8 advantage**: 4× more values per cache line than frac32!

### Speed Comparison

| Operation | i8 (baseline) | frac8 | Factor |
|-----------|---------------|-------|--------|
| Addition  | 1× | ~20-50× | GCD overhead (smaller ints = faster GCD) |
| Multiplication | 1× | ~20-50× | GCD + normalize |

**Trade-off**: Faster than frac32/64 (smaller integers), but still exact overhead.

---

## Comparison with Similar Types

### frac8 vs frac16 (Compact vs Medium)

| Feature | frac8 | frac16 |
|---------|-------|--------|
| **Whole range** | ±127 | ±32,767 |
| **Max denominator** | 127 | 32,767 |
| **Size** | 3 bytes (4 aligned) | 6 bytes (8 aligned) |
| **Cache efficiency** | 16/line | 8/line |
| **Use case** | Small fractions, embedded | Medium precision |

**When to use frac16**:
- Values exceed ±127
- Denominators exceed 127
- Percentages finer than 1/127 (≈0.79%)

### frac8 vs f32 (Exact vs Approximate)

| Feature | frac8 | f32 |
|---------|-------|-----|
| **Precision** | Exact rationals (small) | ~7 decimal digits |
| **Size** | 3 bytes | 4 bytes |
| **1/3** | Exactly 1/3 (if denom≤127) | 0.333333... |
| **Speed** | ~20-50× slower | Baseline (hardware) |

---

## Best Practices

### ✅ DO: Use frac8 for Embedded Systems

```aria
// Minimal RAM footprint for microcontrollers
frac8[100]:sensor_calibrations;      // Only 400 bytes! (vs 1200 for frac32)
```

### ✅ DO: Use for Recipes and Small Fractions

```aria
const frac8:CUP_THIRD = {0, 1, 3};   // ⅓ cup
const frac8:TSP_HALF = {0, 1, 2};    // ½ teaspoon
```

### ✅ DO: Check Range Before Operations

```aria
if (a.whole > 60 && b.whole > 1) {
    // Risk of overflow in a * b
    frac16:result16 = widen_multiply(a, b);
}
```

### ❌ DON'T: Use for General-Purpose (Use frac32)

```aria
// WRONG: frac8 too limited for general use
// CORRECT: Use frac32 unless size critical
```

### ❌ DON'T: Expect Wide Range

```aria
// frac8 is for SMALL fractions only
// Values beyond ±127 → Use larger type
```

---

## Summary

### Key Principles

1. **Minimal Size**: 3 bytes (75% smaller than frac32)
2. **Cache-Friendly**: 16 values per cache line  
3. **Embedded-Ready**: Tiny RAM footprint
4. **Limited Range**: ±127 whole, ≤127 denominator
5. **Same Exactness**: No drift, canonical form, ERR propagation

### When to Use frac8

✅ **Perfect for**:
- Recipe fractions (cups, teaspoons)
- Embedded systems (minimal RAM)
- Small temperature ranges (-127 to +127°C)
- Simple music ratios
- PWM duty cycles
- Small exact percentages (down to ~0.79%)

❌ **Avoid for**:
- Values exceeding ±127
- Denominators > 127
- General-purpose (use frac32)
- Fine percentages (< 1%, use frac16+)

### The Bottom Line

**frac8 provides exact rational arithmetic with minimum memory footprint.** Use for embedded systems, recipes, or when you need 16 exact fractions to fit in a single cache line. Otherwise, prefer frac32.

---

*See also*: [frac16](frac16.md), [frac32](frac32.md), [frac64](frac64.md), [tbb8](tbb8.md), [f32](f32.md)

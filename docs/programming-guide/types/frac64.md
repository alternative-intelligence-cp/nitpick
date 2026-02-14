# frac64: 64-bit High-Precision Exact Rational Numbers

**Maximum-precision exact rational arithmetic for scientific and financial applications**

---

## Overview

`frac64` is Aria's **64-bit high-precision exact rational number type**, providing the widest range and highest precision for exact fraction representation. Like `frac32`, it stores fractions as mixed numbers using three components, but with `tbb64` width for massive range: whole numbers up to ±9.2 quintillion and denominators up to 9.2 quintillion.

**Why frac64 Exists**:
```aria
// frac32 limitation: Denominators limited to ~2.1 billion
frac32:limited = {0, 1, 2000000000};      // Near tbb32 limit

// frac64: Supports massive denominators
frac64:precise = {0, 1, 10000000000000};  // 10 trillion (impossible in frac32!)
frac64:astronomical = {0, 1, 1000000000000000};  // 1 quadrillion denominator
```

**Structure**: `struct { tbb64:whole; tbb64:num; tbb64:denom; }`  
**Size**: 24 bytes (3 × 8 bytes), aligned to 32-byte boundary  
**Whole Range**: ±9,223,372,036,854,775,807 (±9.2 quintillion)  
**Denominator Range**: 1 to 9,223,372,036,854,775,807  
**Error Sentinel**: ERR if any component is ERR or denom = 0  
**Use Case**: High-precision science, large-scale financial, astronomical calculations

---

## Purpose and Characteristics

### What is frac64?

`frac64` extends the exact rational arithmetic philosophy to **maximum precision**:

**Value = whole + (num / denom)**

Where all components are `tbb64` (64-bit symmetric integers):
- `whole`: Integer part (±9.2 quintillion range)
- `num`: Numerator (≥ 0 when whole ≠ 0)
- `denom`: Denominator (always > 0, up to 9.2 quintillion)

**Massive Range Examples**:
```aria
// Astronomy: Precise timing fractions (nanoseconds)
frac64:ns_precision = {0, 123456789, 1000000000000};  // Sub-nanosecond

// Large-scale finance: Bitcoin satoshis as exact fractions
frac64:btc = {21000000, 0, 1};                  // 21 million BTC
frac64:satoshi = {0, 1, 100000000};             // 1 satoshi = 1/100M BTC
frac64:precise_btc = {1, 50000000, 100000000};  // 1.5 BTC exactly

// High-precision science: Planck constant (6.62607015 × 10⁻³⁴)
frac64:planck_num = {662607015, 0, 1};
frac64:planck_exp = {0, 1, 100000000000000000};  // 10^-17 scale
```

### Key Characteristics

**Maximum Precision**:
- ✅ **Denominators up to 9.2 quintillion**: Far beyond frac32's ~2.1 billion limit
- ✅ **Whole numbers up to ±9.2 quintillion**: 4 million× larger than frac32
- ✅ **No loss of precision**: Exact arithmetic at all scales
- ✅ **Cross-platform determinism**: Identical results everywhere

**Same Guarantees as frac32**:
- Exact rational arithmetic (no floating-point drift)
- Automatic normalization to canonical form
- Sticky ERR propagation
- No silent failures

**Performance Trade-offs**:
- **2× memory**: 24 bytes vs 12 bytes (frac32)
- **Slower operations**: 64-bit arithmetic + GCD overhead
- **Worth it when**: Precision requirements justify cost

---

## When to Use frac64

### ✅ Ideal Use Cases

**Astronomical Calculations** (precise time/distance):
```aria
// Light-year with nanosecond precision
frac64:light_year_m = {9460730472580800, 0, 1};  // Meters in light-year
frac64:ns_fraction = {0, 1, 1000000000000};      // Nanosecond fraction
frac64:distance = light_year_m * ns_fraction;    // Exact sub-nanosecond distance

// Orbital mechanics: Exact period ratios
frac64:earth_days = {365, 2425, 10000};          // 365.2425 days (exact!)
frac64:mars_days = {686, 98, 100};               // 686.98 days (exact!)
frac64:synodic = (earth_days * mars_days) / (mars_days - earth_days);  // Exact synodic period
```

**High-prec Financial** (large crypto, multi-trillion transactions):
```aria
// Bitcoin satoshis with sub-satoshi precision (Lightning Network)
frac64:millisat = {0, 1, 1000};                  // 1/1000 satoshi
frac64:payment = {0, 5432, 1000};                // 5.432 satoshis
frac64:channel_capacity = {100000000, 0, 1};     // 1 BTC = 100M sats

// Global currency: Trillions of dollars with cent precision
frac64:world_gdp = {100000000000000, 0, 1};      // $100 trillion
frac64:percentage = {0, 125, 10000};             // 1.25%
frac64:allocation = world_gdp * percentage;      // $1.25 trillion exact
```

**Scientific Constants** (high-precision physical constants):
```aria
// Avogadro's number component
frac64:avogadro_mantissa = {602214076, 0, 1};    // 6.02214076
frac64:avogadro_exp = {0, 1, 100000000};         // ×10^23 scale factor

// Fine structure constant (α ≈ 1/137.035999...)
frac64:alpha_inv = {137, 35999084, 100000000};   // 1/α with high precision
frac64:alpha = {1, 0, 1} / alpha_inv;            // α exact to 8 decimals
```

**Nanosecond Timestamps** (precise event timing):
```aria
// Unix timestamp with nanosecond fractions
frac64:epoch_seconds = {1700000000, 0, 1};       // ~Nov 2023
frac64:nanoseconds = {0, 123456789, 1000000000}; // 0.123456789 seconds
frac64:precise_time = epoch_seconds + nanoseconds;  // Exact timestamp

// Duration with nanosecond precision
frac64:duration_ns = {0, 1, 1000000000};         // 1 nanosecond
frac64:latency = duration_ns * {42, 0, 1};       // Exactly 42 ns
```

**Molecular Chemistry** (exact stoichiometry, massive Avogadro scale):
```aria
// Molar ratios with high precision
frac64:water_h = {2, 0, 1};                      // 2 H atoms
frac64:water_o = {1, 0, 1};                      // 1 O atom
frac64:moles = {6022140760000000000, 0, 1};      // ≈Avogadro's number scale
frac64:h_atoms = moles * water_h;                // Exact hydrogen count
```

**Music Synthesis** (exact frequency with high precision):
```aria
// Just intonation ratios with harmonic precision
frac64:fundamental = {440, 0, 1};                // A440 Hz
frac64:pythagorean_comma = {531441, 0, 524288};  // 3^12 / 2^19 (exact!)
frac64:adjusted_freq = fundamental * pythagorean_comma;  // No drift over time

// Phase accumulation with massive precision
frac64:sample_rate = {192000, 0, 1};             // 192 kHz
frac64:phase_inc = fundamental / sample_rate;    // Exact phase per sample
```

### ❌ When NOT to Use frac64

**General-purpose rationals** (frac32 sufficient):
- Most financial calculations fit in frac32 (±2.1B whole, ±2.1B denom)
- Use frac32 unless requirements clearly exceed its range
- 2× memory cost only justified for actual high-precision needs

**Small exact fractions** (frac8/frac16 more efficient):
- Temperature ratios, small ingredient proportions → frac8/frac16
- Cache efficiency: Smaller types = more values per cache line
- Use frac64 only when range needed

**Graphics/rendering** (floats faster):
- GPU expects f32/f64, not exact rationals
- Conversion overhead negates benefits
- Exact precision not required for visual output

**Real-time constraints** (GCD overhead too high):
- Audio sample processing: f32/f64 for speed
- Game physics: tfp32/f32 for determinism without exact cost
- Profile first: Exactness may not justify latency

---

## Syntax and Literals

### Structure Initialization

Same syntax as frac32, but with tbb64 range:

```aria
// Basic fractions
frac64:half = {0, 1, 2};                         // ½
frac64:third = {0, 1, 3};                        // ⅓
frac64:billionth = {0, 1, 1000000000};           // 1/billion

// Mixed numbers
frac64:large_mixed = {1000000000, 1, 2};         // 1 billion + ½
frac64:precise = {42, 123456789, 1000000000};    // 42.123456789 exactly

// Massive denominators (impossible in frac32!)
frac64:high_denom = {0, 1, 10000000000000};      // 1 / 10 trillion
frac64:ultra_precise = {1, 1, 1000000000000000}; // 1 + (1 / quadrillion)

// Large whole numbers
frac64:btc_supply = {21000000, 0, 1};            // 21M BTC
frac64:world_pop = {8000000000, 0, 1};           // 8 billion people
```

### Sign Representation (Same as frac32)

```aria
// Positive fractions
frac64:pos = {0, 7, 10};                         // +7/10

// Negative fractions (sign in num when whole = 0)
frac64:neg = {0, -7, 10};                        // -7/10

// Mixed numbers (sign in whole)
frac64:pos_mixed = {5, 1, 3};                    // 5⅓
frac64:neg_mixed = {-5, 1, 3};                   // -5⅓

// Edge case: -1 < value < 0
frac64:neg_small = {0, -1, 1000000000};          // -1 billionth
```

### ERR Sentinel

```aria
frac64:zero_denom = {1, 1, 0};                   // ERR (denom = 0)
frac64:overflow = {tbb64:ERR, 0, 1};             // ERR (whole = ERR)
frac64:err_prop = zero_denom + {1, 0, 1};        // ERR (sticky)

// Checking for ERR (same as frac32)
if (value.whole == tbb64:ERR || value.denom == 0) {
    failsafe(ERR_INVALID_FRACTION);
}
```

---

## Operations

### Arithmetic Operations (Same Semantics as frac32)

**Addition** (high-precision exact):
```aria
frac64:a = {0, 1, 3};                            // ⅓
frac64:b = {0, 1, 7};                            // 1/7
frac64:sum = a + b;                              // 10/21 exactly

// Large denominators
frac64:x = {0, 1, 1000000000};                   // 1 / billion
frac64:y = {0, 1, 2000000000};                   // 1 / 2 billion  
frac64:total = x + y;                            // 3 / 2 billion exactly
```

**Multiplication** (no intermediate overflow):
```aria
frac64:million = {1000000, 0, 1};
frac64:billion = {1000000000, 0, 1};
frac64:product = million * billion;              // 10^15 (quadrillion) exact

// High-precision scaling
frac64:planck = {0, 662607015, 10000000000000000};  // ~6.62607×10⁻¹⁷
frac64:frequency = {1000000000000000, 0, 1};     // 1 PHz (10^15 Hz)
frac64:energy = planck * frequency;              // E = hν (exact)
```

**Division** (exact quotient):
```aria
frac64:large = {10000000000000, 0, 1};           // 10 trillion
frac64:small = {0, 1, 1000};                     // 1/1000
frac64:quotient = large / small;                 // 10^16 exactly

// Reciprocal
frac64:recip = {1, 0, 1} / {0, 1, 1000000000};   // 1 billion exactly
```

### Comparison Operations

```aria
frac64:a = {0, 1, 3};                            // ⅓
frac64:b = {0, 333333333, 1000000000};           // 0.333333333

a == b;                                          // FALSE (⅓ ≠ 333333333/10^9)
// Even with 9 decimal places, not exactly ⅓!

frac64:c = {0, 1, 3};
a == c;                                          // TRUE (both exactly ⅓)
```

### ERR Propagation

```aria
frac64:valid = {1, 1, 2};                        // 1½
frac64:error = {1, 1, 0};                        // ERR

frac64:r1 = valid + error;                       // ERR
frac64:r2 = error * {1000000000, 0, 1};          // ERR
frac64:r3 = {10, 0, 1} / error;                  // ERR
```

---

## Type Conversions

### To frac64 (Widening from Smaller Types)

**From frac32** (exact widening):
```aria
frac32:small = {1000, 1, 3};                     // 1000⅓ (frac32)

// Widen using tbb_widen<T>() (preserves ERR sentinels)
frac64:large = {
    tbb_widen<tbb64>(small.whole),
    tbb_widen<tbb64>(small.num),
    tbb_widen<tbb64>(small.denom)
};  // 1000⅓ (frac64, exact)
```

**From frac16/frac8** (exact widening):
```aria
frac16:tiny = {100, 1, 2};                       // 100½ (frac16)
frac64:grown = {
    tbb_widen<tbb64>(tiny.whole),
    tbb_widen<tbb64>(tiny.num),
    tbb_widen<tbb64>(tiny.denom)
};  // 100½ (frac64)
```

**From tbb64/i64** (exact whole numbers):
```aria
tbb64:whole = 123456789012345tbb64;
frac64:exact_whole = {whole, 0, 1};              // Exact integer

i64:signed = 987654321098765i64;
frac64:from_int = {convert<tbb64>(signed), 0, 1};
```

### From frac64 (Narrowing/Lossy)

**To frac32** (potential overflow):
```aria
frac64:huge = {10000000000, 1, 2};               // 10 billion + ½
// Narrowing: whole exceeds tbb32 max (±2.1B) → ERR

frac64:fits = {1000000, 1, 2};                   // 1 million + ½
// Can narrow to frac32, but requires explicit conversion + check
```

**To floating-point** (loses exactness):
```aria
frac64:exact = {0, 1, 3};                        // Exactly ⅓
f64:approx = frac_to_f64(exact);                 // 0.3333333333333333 (NOT exact!)

// f64 mantissa: 53 bits precision
// Some frac64 values have more precision than f64 can represent!
```

**To Integer** (truncation):
```aria
frac64:mixed = {123456789, 22, 7};               // 123456789 22/7
tbb64:truncated = mixed.whole;                   // 123456789 (drops fraction)
```

---

## Common Patterns

### Astronomical Precision

**Light-travel time** (nanosecond accuracy):
```aria
frac64:light_speed_m_s = {299792458, 0, 1};      // c = 299,792,458 m/s
frac64:distance_m = {384400000, 0, 1};           // Earth-Moon: 384,400 km
frac64:time_seconds = distance_m / light_speed_m_s;  // ~1.282 seconds exactly

// Nanosecond precision
frac64:ns_per_sec = {1000000000, 0, 1};          // 10^9
frac64:time_ns = time_seconds * ns_per_sec;      // Exact nanosecond count
```

**Orbital period ratios**:
```aria
frac64:earth_year = {31556952, 0, 1};            // Seconds in tropical year
frac64:mars_year = {59355072, 0, 1};             // Seconds in Martian year
frac64:ratio = mars_year / earth_year;           // 1.881 (exact ratio)

// Synodic period (Earth-Mars conjunction)
frac64:synodic = (earth_year * mars_year) / (mars_year - earth_year);
// Exactly 779.94 days (no accumulation error over millennia)
```

### Large-scale Financial

**Global economics** (trillions):
```aria
frac64:world_gdp = {100000000000000, 0, 1};      // $100 trillion (10^14)
frac64:country_share = {0, 15, 100};             // 15%
frac64:country_gdp = world_gdp * country_share;  // $15 trillion exact

// Per-capita calculations
frac64:population = {8000000000, 0, 1};          // 8 billion
frac64:per_capita = country_gdp / population;    // Exact per-capita
```

**Bitcoin sub-satoshi** (Lightning Network precision):
```aria
frac64:satoshi = {1, 0, 1};                      // 1 satoshi
frac64:millisat = {0, 1, 1000};                  // 1 millisatoshi
frac64:payment = {0, 5432, 1000};                // 5.432 sats (Lightning)

// Channel balancing (sub-sat precision)
frac64:channel_a = {50000000, 123, 1000};        // 50M sats + 123 msats
frac64:channel_b = {50000000, 877, 1000};        // 50M sats + 877 msats
frac64:total = channel_a + channel_b;            // Exactly 100M sats
```

### Scientific Constants

**Physical constants with high precision**:
```aria
// Planck constant (h = 6.62607015 × 10⁻³⁴ J⋅s)
frac64:h_mantissa = {662607015, 0, 1};
frac64:h_exp = {0, 1, 100000000000000000};       // 10^-17
frac64:planck = h_mantissa * h_exp;              // Exact representation

// Avogadro's number (Nₐ = 6.02214076 × 10²³ mol⁻¹)
frac64:na_mantissa = {602214076, 0, 1};
frac64:na_exp = {100000000000000000, 0, 1};      // 10^17
frac64:avogadro = na_mantissa * na_exp;          // Exact (within precision)
```

### Nanosecond Timing

**High-resolution timestamps**:
```aria
// Unix epoch + nanoseconds
frac64:epoch = {1700000000, 0, 1};               // Seconds since 1970
frac64:nanos = {0, 123456789, 1000000000};       // 0.123456789 sec
frac64:timestamp = epoch + nanos;                // Exact nanosecond timestamp

// Duration measurement
frac64:start = {1700000000, 100000000, 1000000000};
frac64:end = {1700000000, 200000000, 1000000000};
frac64:duration = end - start;                   // Exactly 0.1 seconds
```

**Network latency** (nanosecond precision):
```aria
frac64:send_time = {1700000000, 0, 1};
frac64:recv_time = {1700000000, 12345, 1000000000};  // +12.345 μs
frac64:latency = recv_time - send_time;          // Exactly 12,345 ns
```

---

## Common Mistakes and Anti-patterns

### ❌ WRONG: Using frac64 Everywhere (Memory Waste)

```aria
// WRONG: frac64 for small values (wastes 12 bytes per value)
frac64:temperature = {25, 5, 10};                // 25.5°C (overkill!)

// CORRECT: frac16 sufficient (6 bytes, temperature ≤ ±32K)
frac16:temp = {25, 5, 10};                       // 25.5°C (appropriate)
```

### ❌ WRONG: Ignoring Overflow in Multiplication

```aria
// WRONG: Large intermediate values may overflow even in tbb64
frac64:huge_a = {9000000000000000000, 0, 1};     // Near tbb64 max
frac64:huge_b = {2, 0, 1};                       // Small multiplier
frac64:product = huge_a * huge_b;                // OVERFLOW! → ERR

// CORRECT: Check range before operations
if (huge_a.whole > tbb64:MAX / 2) {
    // Cannot safely multiply by 2
    failsafe(ERR_OVERFLOW_RISK);
}
```

### ❌ WRONG: Assuming frac64 Has Infinite Precision

```aria
// WRONG: frac64 cannot represent irrational numbers exactly
frac64:pi = {3141592653589793, 0, 1000000000000000};  // NOT π (approximation)
// π is irrational - no exact fraction representation!

// CORRECT: Document approximation level
frac64:pi_approx = {3141592653589793, 0, 1000000000000000};  // π to 15 decimals
// Or use tfp64 for transcendental functions
```

### ❌ WRONG: Direct Comparison to f64

```aria
// WRONG: Cannot directly compare different types
frac64:exact = {0, 1, 3};                        // ⅓
f64:approx = 0.333333333333;
// exact == approx;  // TYPE ERROR!

// CORRECT: Convert to same type (loses exactness)
f64:converted = frac_to_f64(exact);
if (abs(converted - approx) < 1e-12) { /* close enough */ }
```

### ❌ WRONG: Using frac64 for Real-time Systems Without Profiling

```aria
// WRONG: Assuming frac64 fast enough (GCD overhead!)
till (sample in audio_buffer) {
    frac64:processed = sample * gain;            // TOO SLOW for real-time!
}

// CORRECT: Profile first, use floats for real-time
till (sample in audio_buffer) {
    f32:processed = sample * gain;               // Fast enough for audio
}
// Use frac64 for offline processing where exactness matters
```

---

## Performance Characteristics

### Memory Layout

**Size**: 24 bytes (3 × `tbb64` at 8 bytes each)  
**Alignment**: 32-byte boundary (8-byte padding)  
**Cache**: 2-3 frac64 values per 64-byte cache line

```
┌─────────────┬─────────────┬─────────────┬─────────┐
│ whole (8B)  │ num (8B)    │ denom (8B)  │ pad(8B) │
├─────────────┴─────────────┴─────────────┴─────────┤
│     frac64 structure (32 bytes aligned)           │
└───────────────────────────────────────────────────┘
```

### Computational Complexity

**Same as frac32, but with 64-bit integers**:
- Addition/Subtraction: O(log(max(denom)))
- Multiplication: O(log(a) + log(b))
- Division: O(log(a) + log(b))
- GCD: O(log(min(a, b))) - Euclidean algorithm

**64-bit overhead**: ~2× slower than 32-bit operations on 64-bit CPUs

### Speed Comparison

| Operation | f64 (baseline) | frac64 (exact) | Factor |
|-----------|----------------|----------------|--------|
| Addition  | 1× (~1 cycle)  | ~100-200× | GCD + 64-bit |
| Multiplication | 1× (~3 cycles) | ~100-200× | GCD + normalize |
| Division  | 1× (~10-20 cycles) | ~100-200× | Reciprocal + GCD |

**Trade-off**: Precision for speed, but 2× slower than frac32.

---

## Comparison with Similar Types

### frac64 vs frac32 (Precision vs Memory)

| Feature | frac64 | frac32 |
|---------|--------|--------|
| **Whole range** | ±9.2 quintillion | ±2.1 billion |
| **Max denominator** | 9.2 quintillion | 2.1 billion |
| **Size** | 24 bytes | 12 bytes |
| **Speed** | Slower (64-bit ops) | Faster (32-bit ops) |
| **Use case** | High-precision science | General-purpose |

**When to use frac64**:
- Denominators exceed 2 billion
- Whole numbers exceed ±2 billion
- Nanosecond timestamp fractions
- Astronomical calculations
- Large-scale financial (trillions)

### frac64 vs tfp64 (Exact vs Deterministic)

| Feature | frac64 | tfp64 |
|---------|--------|-------|
| **Representation** | Exact rational | Deterministic float |
| **1/3** | Exactly 1/3 | Approximation |
| **Irrational numbers** | Cannot represent | Approximates (deterministically) |
| **Transcendental functions** | Not supported | sin, cos, sqrt available |

**When to use tfp64** instead:
- Need irrational numbers (π, e, √2)
- Transcendental functions required
- Deterministic floating-point (not exact fractions)

### frac64 vs f64 (Exact vs Fast)

| Feature | frac64 | f64 |
|---------|--------|-----|
| **Precision** | Exact rational | ~15-16 decimal digits |
| **0.1 + 0.2 = 0.3** | ✅ TRUE | ❌ FALSE |
| **1/3 representation** | Exactly 1/3 | 0.3333... (approx) |
| **Size** | 24 bytes | 8 bytes |
| **Speed** | ~100-200× slower | Baseline (hardware) |

---

## Platform and Implementation

### Bit Representation

**Structure layout** (little-endian):
```
Offset 0-7:   whole (tbb64)
Offset 8-15:  num (tbb64)
Offset 16-23: denom (tbb64)
Offset 24-31: padding (8 bytes, alignment)
```

### Endianness

Each `tbb64` component follows system endianness:
- Little-endian: LSB first (x86/x64, ARM64)
- Big-endian: MSB first (network, some embedded)

**Serialization**: Convert to network byte order (big-endian).

### Cross-platform Determinism

**Guaranteed**:
- ✅ Identical results across all platforms (same inputs → same outputs)
- ✅ GCD algorithm deterministic
- ✅ ERR propagation deterministic
- ✅ Normalization canonical

---

## Best Practices

### ✅ DO: Use frac64 for High-Precision Requirements

```aria
// Nanosecond timestamps (Year 2262-proof!)
frac64:timestamp = epoch + {0, ns_count, 1000000000};

// Large-scale financial (trillions)
frac64:gdp = {100000000000000, 0, 1};  // $100 trillion exact
```

### ✅ DO: Document Precision Limits

```aria
// Specify when approximation used
// APPROXIMATE: π to 15 decimal places (irrational, cannot be exact)
frac64:pi = {3141592653589793, 0, 1000000000000000};
```

### ✅ DO: Check for Overflow

```aria
if (large.whole > tbb64:MAX / 2) {
    // Cannot safely multiply
    failsafe(ERR_OVERFLOW);
}
```

### ❌ DON'T: Use frac64 for General-Purpose (Use frac32)

```aria
// WRONG: frac64 wastes memory/speed
frac64:price = {19, 99, 100};  // $19.99

// CORRECT: frac32 sufficient
frac32:price = {19, 99, 100};  // $19.99 (12 bytes vs 24)
```

### ❌ DON'T: Assume Real-time Performance

```aria
// Profile before using frac64 in tight loops!
// Consider f64/tfp64 for real-time constraints
```

---

## Summary

### Key Principles

1. **Maximum Precision**: Largest exact rational type (±9.2 quintillion)
2. **Same Guarantees as frac32**: Exact arithmetic, normalization, ERR propagation
3. **2× Memory Cost**: 24 bytes vs 12 bytes (frac32)
4. **Use When Needed**: High-precision science, large finance, nanosecond timing

### When to Use frac64

✅ **Perfect for**:
- Astronomical calculations (light-years, orbital periods)
- High-precision scientific constants
- Nanosecond timestamps (Year 2262-proof)
- Large-scale financial (trillions, Bitcoin)
- Denominators > 2 billion

❌ **Avoid for**:
- General-purpose rationals (use frac32)
- Real-time systems (unless profiled)
- Graphics (use f32/f64)
- Small values (use frac8/frac16)

### The Bottom Line

**frac64 provides maximum-precision exact rational arithmetic** when frac32 range insufficient. Use for high-precision science, large-scale finance, or nanosecond timing. Otherwise, prefer frac32 for efficiency.

---

*See also*: [frac32](frac32.md), [frac16](frac16.md), [frac8](frac8.md), [tbb64](tbb64.md), [tfp64](tfp64.md), [f64](f64.md)

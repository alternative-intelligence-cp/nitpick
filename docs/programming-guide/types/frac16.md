# frac16: 16-bit Medium-Precision Exact Rational Numbers

**Memory-efficient exact rational arithmetic for medium-range fractions**

---

## Overview

`frac16` is Aria's **16-bit medium-precision exact rational number type**, bridging the gap between compact `frac8` and general-purpose `frac32`. Using three `tbb16` components, it provides exact fractions with whole numbers from -32,767 to +32,767 and denominators up to 32,767—ideal for audio sample timing, medium financial calculations, and network protocol fractions.

**Why frac16 Exists**:
```aria
// frac8: Too limited (denom max 127

, can't represent 0.1%)
// frac8:tenth_pct = {0, 1, 1000};  // OVERFLOW! 1000 > 127

// frac16: Handles fine percentages perfectly
frac16:tenth_pct = {0, 1, 1000};     // 0.1% exactly (1/1000)
frac16:half_pct = {0, 1, 200};       // 0.5% exactly (1/200)

// frac32: Overkill for values under ±32K (wastes 6 bytes per value)
// frac16: Sweet spot (half the size, sufficient range)
```

**Structure**: `struct { tbb16:whole; tbb16:num; tbb16:denom; }`  
**Size**: 6 bytes (3 × 2 bytes), aligned to 8-byte boundary  
**Whole Range**: -32,767 to +32,767  
**Denominator Range**: 1 to 32,767  
**Error Sentinel**: ERR if any component is ERR (-32768) or denom = 0  
**Use Case**: Audio timing, fine percentages, medium finance, arrays/coordinates

---

## Purpose and Characteristics

### What is frac16?

`frac16` provides **exact rational arithmetic** with medium precision:

**Value = whole + (num / denom)**

Where all components are `tbb16` (16-bit symmetric integers):
- `whole`: Integer part (-32,767 to +32,767)
- `num`: Numerator (0 to 32,767 when whole ≠ 0)
- `denom`: Denominator (1 to 32,767, always positive)

**Medium-Range Examples**:
```aria
// Fine percentages (0.01% to 100%)
frac16:basis_point = {0, 1, 10000};  // 0.01% (1 basis point)
frac16:quarter_pct = {0, 1, 400};    // 0.25%
frac16:decimal_pct = {0, 125, 10000};  // 1.25%

// Audio sample timing (44.1 kHz - 192 kHz)
frac16:sample_rate = {44100, 0, 1};  // 44.1 kHz sample rate
frac16:sample_time = {0, 1, 44100};  // 1 sample = 1/44100 sec ≈ 22.68 μs

// Medium financial (up to ±$32K)
frac16:price = {19999, 99, 100};     // $19,999.99
frac16:tax_rate = {0, 725, 10000};   // 7.25% sales tax

// Screen coordinates (up to 32K pixels)
frac16:pixel_x = {1920, 5, 10};      // 1920.5 pixels (subpixel rendering)
frac16:pixel_y = {1080, 25, 100};    // 1080.25 pixels
```

### Key Characteristics

**Medium Precision**:
- ✅ **256× larger than frac8**: Whole ±32,767 vs ±127
- ✅ **256× more denominators**: Up to 32,767 vs 127
- ✅ **Fine percentages**: Down to 0.01% (basis points)
- ✅ **Half the size of frac32**: 6 bytes vs 12 bytes

**Cache Efficiency**:
- **8 frac16 values** per 64-byte cache line (vs 5 frac32)
- Better memory bandwidth than frac32 for medium-range values
- 50% memory savings when ±32K range sufficient

**Same Guarantees**:
- Exact rational arithmetic (no floating-point drift)
- Automatic normalization to canonical form
- Sticky ERR propagation
- Cross-platform determinism

**Limitations**:
- ⚠️ **Medium range only**: Exceeds at ±32,767
- ⚠️ **Year 2038 problem**: Cannot represent years > 32,767
- ⚠️ **Limited denominators**: Max 32,767 (finer than frac8, but not frac32)

---

## When to Use frac16

### ✅ Ideal Use Cases

**Fine Percentage Calculations** (basis points, decimal percentages):
```aria
// Financial basis points (0.01% = 1/10000)
frac16:one_bp = {0, 1, 10000};       // 1 basis point (0.01%)
frac16:ten_bp = {0, 10, 10000};      // 10 bp (0.1%)
frac16:fifty_bp = {0, 50, 10000};    // 50 bp (0.5%)

// Interest rates with basis point precision
frac16:rate = {5, 2500, 10000};      // 5.25% (525 basis points)
frac16:principal = {25000, 0, 1};    // $25,000
frac16:interest = principal * rate;  // $1,312.50 exactly
```

**Audio Sample Timing** (CD quality to 192 kHz):
```aria
// Sample rates (common audio standards)
frac16:cd_rate = {44100, 0, 1};      // 44.1 kHz (CD quality)
frac16:dvd_rate = {48000, 0, 1};     // 48 kHz (DVD audio)
frac16:hd_rate = {192000, 0, 1};     // 192 kHz (HD audio) - OVERFLOW! Use i32

// Sample timing fractions
frac16:sample_cd = {0, 1, 44100};    // 1 CD sample ≈ 22.68 μs
frac16:sample_dvd = {0, 1, 48000};   // 1 DVD sample ≈ 20.83 μs

// Phase offset for filters
frac16:phase = {0, 1, 8};            // π/8 phase offset
frac16:delay_samples = {5, 1, 2};    // 5.5 samples delay
```

**Medium Financial Calculations** (up to ±$32K):
```aria
// Retail pricing
frac16:item_price = {299, 99, 100};  // $299.99
frac16:discount = {0, 15, 100};      // 15% discount
frac16:sale_price = item_price * ({1,0,1} - discount);  // $254.99 exactly

// Tax calculations
frac16:subtotal = {1542, 0, 1};      // $1,542.00
frac16:tax_rate = {0, 825, 10000};   // 8.25% tax
frac16:tax = subtotal * tax_rate;    // $127.22 exactly
frac16:total = subtotal + tax;       // $1,669.22 exactly
```

**Screen Coordinates with Subpixel Precision**:
```aria
// HD resolution (1920×1080)
frac16:screen_width = {1920, 0, 1};  // 1920 pixels
frac16:screen_height = {1080, 0, 1}; // 1080 pixels

// Subpixel rendering (antialiasing)
frac16:text_x = {100, 333, 1000};    // 100.333 pixels
frac16:text_y = {50, 667, 1000};     // 50.667 pixels

// Smoother animation using exact fractional positions
frac16:velocity_x = {5, 25, 100};    // 5.25 pixels/frame
frac16:pos_x = pos_x + velocity_x;   // Exact accumulation (no drift!)
```

**Network Protocol Fractions** (packet timing, rate limiting):
```aria
// Packets per second (up to 32K pps)
frac16:rate_limit = {10000, 0, 1};   // 10,000 packets/sec
frac16:time_per_packet = {0, 1, 10000};  // 1/10000 sec = 100 μs

// Bandwidth allocation
frac16:total_bw = {1000, 0, 1};      // 1000 Mbps link
frac16:user_share = {0, 15, 100};    // 15% allocation
frac16:user_bw = total_bw * user_share;  // 150 Mbps exactly
```

**Geographic Coordinates** (limited precision):
```aria
// Latitude/longitude with limited precision
// Note: Full precision needs frac32, but frac16 for coarse location
frac16:lat_deg = {37, 7749, 10000};  // 37.7749°N (San Francisco)
frac16:lon_deg = {-122, 4194, 10000};  // -122.4194°W
// Precision: ~3 decimal places (≈111 meters at equator)
```

### ❌ When NOT to Use frac16

**Values exceed ±32,767**:
```aria
// WRONG: Year 2024 fits, but room for growth limited
frac16:year = {2024, 0, 1};          // Fits now, but Y32.7K problem!

// CORRECT: Use frac32 for years (future-proof)
frac32:year = {2024, 0, 1};          // Safe until ±2.1 billion
```

**Denominators exceed 32,767**:
```aria
// WRONG: 1/100000 needs denom=100000 (exceeds 32,767)
// frac16:fine_fraction = {0, 1, 100000};  // OVERFLOW!

// CORRECT: Use frac32
frac32:fine_fraction = {0, 1, 100000};  // 0.00001 exactly
```

**Very fine percentages** (< 0.01%):
```aria
// WRONG: 0.001% = 1/100000, exceeds denom limit
// frac16:tenth_bp = {0, 1, 100000};  // OVERFLOW!

// CORRECT: Use frac32
frac32:tenth_bp = {0, 1, 100000};    // 0.001% exactly
```

**Large price calculations** (> $32K):
```aria
// WRONG: $50,000 exceeds whole number limit
// frac16:expensive = {50000, 0, 1};  // OVERFLOW!

// CORRECT: Use frac32
frac32:expensive = {50000, 0, 1};    // $50,000
```

**Small fractions only** (frac8 more efficient):
```aria
// WRONG: Recipe ⅓ cup wastes memory (frac8 sufficient)
frac16:cup_third = {0, 1, 3};        // 6 bytes

// BETTER: Use frac8 for simple recipe fractions
frac8:cup_third = {0, 1, 3};         // 3 bytes (4 aligned)
```

---

## Syntax and Literals

### Structure Initialization

```aria
// Basic fractions
frac16:half = {0, 1, 2};             // ½
frac16:thousandth = {0, 1, 1000};    // 0.001 (1/1000)
frac16:basis_point = {0, 1, 10000};  // 0.01% (1 bp)

// Mixed numbers
frac16:price = {19, 99, 100};        // $19.99
frac16:large = {32767, 0, 1};        // 32,767 (tbb16 max)

// Negative fractions
frac16:neg_half = {0, -1, 2};        // -½
frac16:neg_price = {-100, 50, 100};  // -$100.50

// Audio timing
frac16:sample_rate = {44100, 0, 1};  // 44.1 kHz
frac16:sample_time = {0, 1, 44100};  // ≈ 22.68 μs

// Fine percentages
frac16:tax = {0, 725, 10000};        // 7.25% (725 basis points)
frac16:discount = {0, 15, 100};      // 15%
```

### ERR Sentinel

```aria
frac16:zero_denom = {1, 1, 0};       // ERR (denom = 0)
frac16:err_whole = {-32768, 0, 1};   // ERR (whole = tbb16:ERR)
frac16:err_num = {0, -32768, 1};     // ERR (num = ERR)
frac16:overflow = {40000, 0, 1};     // OVERFLOW! 40000 > 32767 → ERR

// Checking for ERR
if (value.whole == -32768 || value.num == -32768 || value.denom == 0) {
    failsafe(ERR_INVALID_FRACTION);
}
```

### Canonical Form

```aria
// Automatic normalization (same as frac8/32/64)
frac16:unreduced = {0, 100, 1000};   // Normalized to {0, 1, 10}
frac16:improper = {0, 5000, 1000};   // Normalized to {5, 0, 1}

frac16:a = {0, 2, 4};                // Becomes {0, 1, 2}
frac16:b = {0, 1, 2};                // Already canonical
a == b;                              // TRUE (both ½)
```

---

## Operations

### Arithmetic Operations

**Addition** (watch for overflow):
```aria
frac16:a = {1000, 1, 3};             // 1000⅓
frac16:b = {500, 1, 6};              // 500 1/6
frac16:sum = a + b;                  // 1500½ = {1500, 1, 2}

// Overflow example
frac16:x = {20000, 0, 1};            // 20,000
frac16:y = {15000, 0, 1};            // 15,000
frac16:overflow = x + y;             // 35,000 > 32,767 → ERR!
```

**Multiplication**:
```aria
frac16:price = {100, 0, 1};          // $100
frac16:tax_rate = {0, 7, 100};       // 7% tax
frac16:tax = price * tax_rate;       // $7 exactly

// Larger multiplication (potential overflow)
frac16:c = {1000, 0, 1};
frac16:d = {50, 0, 1};
frac16:product = c * d;              // 50,000 > 32,767 → ERR!
```

**Division**:
```aria
frac16:total = {1000, 0, 1};         // $1000
frac16:people = {3, 0, 1};           // 3 people
frac16:per_person = total / people;  // $333⅓ exactly

// Division by zero
frac16:zero = {0, 0, 1};
frac16:err = total / zero;           // ERR
```

### Comparison Operations

```aria
frac16:a = {0, 1, 2};                // ½
frac16:b = {0, 500, 1000};           // Normalized to {0, 1, 2}
frac16:c = {0, 1, 3};                // ⅓

a == b;                              // TRUE (both ½)
a > c;                               // TRUE (½ > ⅓)
c < a;                               // TRUE (⅓ < ½)
```

### ERR Propagation

```aria
frac16:valid = {100, 1, 2};          // 100½
frac16:error = {1, 1, 0};            // ERR

frac16:r1 = valid + error;           // ERR
frac16:r2 = error * {2, 0, 1};       // ERR
frac16:r3 = {3, 0, 1} / error;       // ERR
```

---

## Type Conversions

### To frac16 (Widening from frac8)

**From frac8** (exact widening):
```aria
frac8:small = {100, 1, 2};           // 100½ (frac8)

// Widen to frac16 (preserves ERR sentinels)
frac16:medium = {
    tbb_widen<tbb16>(small.whole),
    tbb_widen<tbb16>(small.num),
    tbb_widen<tbb16>(small.denom)
};  // 100½ (frac16, exact)
```

**From tbb16** (exact whole numbers):
```aria
tbb16:count = 1024tbb16;
frac16:exact_count = {count, 0, 1};  // 1024 exactly
```

### From frac16 (Narrowing to frac8/Widening to frac32)

**To frac8** (potential overflow):
```aria
frac16:large = {1000, 1, 2};         // 1000½
// Cannot fit in frac8 (whole exceeds ±127) → ERR

frac16:small_enough = {100, 1, 2};   // 100½
// Still exceeds frac8 range (±127) → ERR

frac16:fits = {50, 1, 2};            // 50½
// Can narrow with explicit conversion
```

**To frac32** (safe widening):
```aria
frac16:medium = {10000, 99, 100};    // $10,000.99

frac32:large = {
    tbb_widen<tbb32>(medium.whole),
    tbb_widen<tbb32>(medium.num),
    tbb_widen<tbb32>(medium.denom)
};  // $10,000.99 (frac32, exact)
```

**To floating-point** (loses exactness):
```aria
frac16:exact = {0, 1, 3};            // Exactly ⅓
f32:approx = frac_to_f32(exact);     // 0.333333... (NOT exact!)
```

---

## Common Patterns

### Basis Point Financial Calculations

```aria
// Interest rate with basis point precision
frac16:principal = {10000, 0, 1};    // $10,000
frac16:rate_bp = {525, 0, 1};        // 525 basis points
frac16:rate_pct = {0, rate_bp.whole, 10000};  // 5.25%

frac16:interest = principal * rate_pct;  // $525.00 exactly

// Yield spread
frac16:bond_yield = {0, 425, 10000}; // 4.25%
frac16:risk_free = {0, 350, 10000};  // 3.50%
frac16:spread = bond_yield - risk_free;  // 0.75% (75 bp)
```

### Audio Sample Timing (CD Quality)

```aria
// CD audio: 44.1 kHz sample rate
frac16:sample_rate = {44100, 0, 1};  // 44,100 samples/second
frac16:seconds = {1, 0, 1};
frac16:samples_per_sec = sample_rate;  // 44,100

// Time per sample
frac16:time_per_sample = seconds / sample_rate;  // 1/44100 sec

// Buffer size (common: 512 samples)
frac16:buffer_samples = {512, 0, 1};
frac16:buffer_time = buffer_samples / sample_rate;  // ≈11.6 ms exactly
```

### Subpixel Rendering (Antialiasing)

```aria
// Text rendering with subpixel precision
frac16:glyph_width = {12, 333, 1000};  // 12.333 pixels (exact)
frac16:cursor_pos = {100, 0, 1};     // Pixel 100

// Advance cursor by glyph width
frac16:new_pos = cursor_pos + glyph_width;  // 112.333 pixels exactly

// Kerning adjustment (negative space)
frac16:kerning = {0, -125, 1000};    // -0.125 pixels
frac16:adjusted_pos = new_pos + kerning;  // 112.208 pixels exactly
```

### Network Rate Limiting

```aria
// Bandwidth allocation
frac16:link_speed = {1000, 0, 1};    // 1000 Mbps (1 Gbps)
frac16:user_share = {0, 125, 1000};  // 12.5% allocation
frac16:user_bw = link_speed * user_share;  // 125 Mbps exactly

// Packets per second limit
frac16:max_pps = {10000, 0, 1};      // 10,000 pps
frac16:time_per_packet = {1, 0, 1} / max_pps;  // 0.0001 sec = 100 μs
```

---

## Common Mistakes and Anti-patterns

### ❌ WRONG: Using frac16 for Large Values

```aria
// WRONG: $100,000 exceeds tbb16 range
// frac16:expensive = {100000, 0, 1};  // OVERFLOW! > 32,767

// CORRECT: Use frac32
frac32:expensive = {100000, 0, 1};   // Fits in frac32
```

### ❌ WRONG: Very Fine Denominators

```aria
// WRONG: 1/100000 exceeds denom limit (32,767)
// frac16:too_fine = {0, 1, 100000};  // OVERFLOW!

// CORRECT: Use frac32
frac32:fine = {0, 1, 100000};        // 0.00001 exactly
```

### ❌ WRONG: Using frac16 for Simple Fractions

```aria
// WRONG: Recipe ⅓ wastes memory vs frac8
frac16:ingredient = {0, 1, 3};       // 6 bytes

// BETTER: Use frac8 when values ≤ ±127
frac8:ingredient = {0, 1, 3};        // 3 bytes (smaller, faster)
```

### ❌ WRONG: Assuming Unlimited Range

```aria
// WRONG: Large multiplication overflows
frac16:a = {10000, 0, 1};
frac16:b = {5, 0, 1};
frac16:product = a * b;              // 50,000 > 32,767 → ERR!

// CORRECT: Widen before multiplication
frac32:product32 = {10000, 0, 1} * {5, 0, 1};  // 50,000 (frac32)
```

---

## Performance Characteristics

### Memory Layout

**Size**: 6 bytes (3 × `tbb16` at 2 bytes each)  
**Alignment**: 8-byte boundary (2-byte padding)  
**Cache**: **8 frac16 values** per 64-byte cache line

```
┌─────────┬─────────┬─────────┬───────┐
│whole(2) │ num(2)  │denom(2) │pad(2) │
├─────────┴─────────┴─────────┴───────┤
│ frac16 structure (8 bytes aligned)  │
└─────────────────────────────────────┘
```

### Cache Efficiency

| Type | Size (aligned) | Values per cache line |
|------|----------------|----------------------|
| frac8 | 4 bytes | 16 values |
| **frac16** | 8 bytes | **8 values** |
| frac32 | 16 bytes | 4 values |
| frac64 | 32 bytes | 2 values |

**frac16 advantage**: 2× more values per cache line than frac32!

---

## Comparison with Similar Types

### frac16 vs frac8 (Medium vs Compact)

| Feature | frac16 | frac8 |
|---------|--------|-------|
| **Whole range** | ±32,767 | ±127 |
| **Max denominator** | 32,767 | 127 |
| **Size** | 6 bytes (8 aligned) | 3 bytes (4 aligned) |
| **Use case** | Medium precision | Small fractions, embedded |

### frac16 vs frac32 (Medium vs General)

| Feature | frac16 | frac32 |
|---------|--------|--------|
| **Whole range** | ±32,767 | ±2.1 billion |
| **Max denominator** | 32,767 | 2.1 billion |
| **Size** | 6 bytes | 12 bytes |
| **Cache efficiency** | 8/line | 4/line |
| **Use case** | Medium values | General-purpose |

**When to use frac32**:
- Values exceed ±32K
- Denominators exceed 32K
- Future-proof (years, large financial)

### frac16 vs f32 (Exact vs Approximate)

| Feature | frac16 | f32 |
|---------|--------|-----|
| **Precision** | Exact rationals (medium) | ~7 decimal digits |
| **Size** | 6 bytes | 4 bytes |
| **0.1 + 0.2 = 0.3** | TRUE | FALSE |
| **Speed** | ~50× slower | Baseline |

---

## Best Practices

### ✅ DO: Use frac16 for Medium Financial Values

```aria
// Prices up to ±$32K
frac16:item_price = {299, 99, 100};  // $299.99
frac16:subtotal = {1542, 88, 100};   // $1,542.88
```

### ✅ DO: Use for Fine Percentages (Basis Points)

```aria
// Financial rates with 0.01% precision
frac16:interest_rate = {0, 525, 10000};  // 5.25% (525 bp)
```

### ✅ DO: Check Range Before Operations

```aria
if (a.whole > 16000 && b.whole > 1) {
    // Risk of overflow in a * b
    frac32:result = widen_and_multiply(a, b);
}
```

### ❌ DON'T: Use for General-Purpose (Use frac32)

```aria
// Unless range clearly ≤ ±32K, use frac32 (future-proof)
```

### ❌ DON'T: Waste on Small Fractions (Use frac8)

```aria
// Use frac8 for recipe-scale fractions (±127 sufficient)
```

---

## Summary

### Key Principles

1. **Medium Precision**: ±32,767 whole, ≤32,767 denominator
2. **Half Size of frac32**: 6 bytes vs 12 bytes (50% savings)
3. **Cache-Friendly**: 8 values per cache line
4. **Fine Percentages**: Down to 0.01% (basis points)
5. **Same Exactness**: No drift, canonical form, ERR propagation

### When to Use frac16

✅ **Perfect for**:
- Fine percentages (0.01% = basis points)
- Audio sample timing (44.1 kHz - 192 kHz)
- Medium financial (up to ±$32K)
- Screen coordinates (HD resolution)
- Network rate limiting
- Medium-range exact fractions

❌ **Avoid for**:
- Values exceeding ±32K (use frac32)
- Denominators > 32K (use frac32)
- Small fractions only (use frac8)
- General-purpose (use frac32 unless size critical)

### The Bottom Line

**frac16 fills the gap between compact frac8 and general-purpose frac32.** Use for medium-range exact fractions where precision matters and memory efficiency is valuable. 

---

*See also*: [frac8](frac8.md), [frac32](frac32.md), [frac64](frac64.md), [tbb16](tbb16.md), [f32](f32.md)

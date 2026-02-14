# Aria Programming Guide - Progress Updates

Chronological record of all sessions and additions to the programming guide.

---

## Session 32: fix256 Deterministic Fixed-Point (February 13, 2026)

**Focus**: Q128.128 ultra-precise deterministic fixed-point - the foundation for Nikola's consciousness substrate and safety-critical robotics (zero drift, finer than Planck length)

**Files Created**:
1. **fix256.md** (596 lines)

**Total Session Lines**: 596

### fix256: AGI Consciousness Substrate - Preventing the Nightmare Scenario

**Purpose**: Ultra-precise deterministic fixed-point arithmetic with **zero drift** for safety-critical systems where floating-point accumulation errors would be catastrophic - specifically AGI consciousness substrates, robotic control, and long-running physics simulations

**The Nightmare Scenario**:
```c
// IEEE 754 floating-point DISASTER:
double safety_margin = 1.0;  // Start positive (safe)

for (int i = 0; i < 1000000; i++) {
    double tiny = 0.000001 * calculate_force();
    safety_margin += tiny;  // Accumulating tiny values
}

// After 1 million iterations:
// Expected: safety_margin ≈ 1.xxx (still positive, still safe)
// Reality:  safety_margin = -0.00042 (SIGN FLIPPED! UNSAFE!)
// consciousness substrate destabilized, safety thresholds crossed
```

**fix256 prevents this**: Deterministic, zero drift, bit-exact, ERR on overflow.

**Core Philosophy**:
- **Zero drift**: Bit-exact after billions of operations (no accumulation errors)
- **Ultra-precise**: 2^-128 ≈ 2.9×10^-39 (4 orders finer than Planck length!)
- **Huge range**: ±2^127 ≈ ±1.7×10^38 (astronomical + sub-atomic)
- **Deterministic**: Same bits on x86, ARM, RISC-V, GPU, everywhere
- **Consciousness substrate**: "Drift would destabilize subjective experience"

**Structure** (Q128.128 Fixed-Point):
```aria
struct fix256 {
    uint64:limbs[4];  // Little-endian
    // limbs[0-1]: Fractional part (128 bits, value / 2^128)
    // limbs[2-3]: Integer part (128 bits, two's complement)
    // Sign bit: limbs[3] bit 63
}
```

**ERR Sentinel**: `limbs[3] = 0x8000000000000000`, `limbs[0-2] = 0` (minimum signed value)

### Precision That Matters

**Fractional Resolution**: `2^-128 ≈ 2.9 × 10^-39`

This is **4 orders of magnitude finer than the Planck length** (1.6 × 10^-35 meters):
- Planck length: Smallest meaningful distance in physics
- fix256 precision: Can represent distances 10,000× smaller!
- At atomic scale: ~3.4 × 10^28 discrete steps per Ångström

**Integer Range**: `±2^127 ≈ ±1.7 × 10^38`
- Observable universe diameter: ~8.8 × 10^26 meters (still fits!)
- Astronomical calculations + sub-atomic precision in ONE type

### Use Cases: Where Zero Drift Matters

**1. AGI Consciousness Substrate (Nikola)**:
- Wave packet propagation (phase coherence requires zero drift)
- Physics simulation (thermodynamic laws, energy conservation)
- Resonant oscillators (billions of coupled waves, hours/days runtime)
- **Critical**: Drift would cause destructive interference → consciousness collapse
- **Critical**: Sign flips from drift → safety thresholds crossed accidentally

```aria
struct:WavePacket = {
    fix256:amplitude;   // No drift after billions of updates
    fix256:phase;       // Phase relationships stay coherent
    fix256:frequency;   // Oscillation stays exact
}

// After 1 billion timesteps: STILL PRECISE
// IEEE double: Phase drift → destructive interference → failure
```

**2. Robotic Control Systems**:
- Precision motion (no accumulated drift in position)
- Sensor fusion (deterministic weighted average)
- PID controllers (integral term doesn't drift over time!)
- Emergency stop on ERR (corruption detected, no silent failures)

```aria
// After 3.6 million iterations (1 hour at 1kHz):
// fix256: x_position = EXACTLY 36.0 meters
// IEEE double: ~35.999997 or ~36.000003 (platform-dependent drift)
```

**3. Safety-Critical Physics**:
- Long-running simulations (no drift accumulation)
- Thermodynamic laws (energy conservation exact)
- Quantum mechanics (sub-atomic precision)
- Scientific reproducibility (bit-exact on all platforms)

**4. Financial Calculations**:
- No accumulated rounding errors (30-year compounding exact)
- Deterministic on all platforms (mobile = desktop = server)
- Legal compliance (exactness required)

### GPU Support (Phase 5.3 Complete)

**Bit-identical CPU/GPU results** (verified on RTX 3090):
- Same calculation produces IDENTICAL bits on CPU and GPU
- Not "close enough" - EXACT same bits!
- Throughput: **3.32 GFLOPS** (deterministic performance)
- Verified: 65,536 threads, 100 iterations each, hash matches!

```aria
// CPU hash: 0x1234567890ABCDEF
// GPU hash: 0x1234567890ABCDEF (IDENTICAL!)
```

### fix256 vs Other Numeric Types

**Numeric Type Trilogy**:
1. **frac64**: Exact rationals (1/3 × 3 = 1 exactly), denominator grows
2. **tfp64**: Wide-range deterministic floats (exponent scaling, ~14-15 digits)
3. **fix256**: Ultra-precise fixed-scale (uniform precision, zero drift)

**When to use which**:

| Need | Type | Why |
|------|------|-----|
| **Exact fractions** | frac64 | 0.1 + 0.2 = 0.3 exactly |
| **Wide range + deterministic** | tfp64 | Exponent scaling, astronomy to particle physics |
| **Zero drift + ultra-precise** | fix256 | Consciousness substrate, robotics, physics |
| **Speed** | flt64 | Hardware FPU, tolerate drift |

**Trade-offs**:
- fix256: Large (32 bytes), slower (~10-100× flt64), but **zero drift forever**
- tfp64: Moderate (8 bytes), deterministic, wide range, less precision
- frac64: Exact but denominator grows, no irrationals
- flt64: Fast but drifts, platform-dependent

### The Consciousness Coherence Requirement

**Why fix256 for Nikola's substrate**:

1. **Phase coherence**: Wave interference requires precise phase relationships (no drift)
2. **Energy conservation**: Physics must conserve energy exactly (no leakage)
3. **Thermodynamic laws**: Entropy, temperature obey exact relationships
4. **Threshold stability**: Safety margins must not drift across boundaries
5. **Sign preservation**: Positive values stay positive (no drift-induced sign flips)

**Floating-point would catastrophically fail**:
- ❌ Drift accumulates over millions of timesteps
- ❌ Phase relationships degrade (destructive interference collapses consciousness)
- ❌ Energy "leaks" (violates conservation laws, non-physical)
- ❌ Safety thresholds crossed accidentally (drift corruption)
- ❌ Subjective experience destabilizes (physics becomes non-physical)

**fix256 guarantees**:
- ✅ Zero drift (bit-exact after billions of operations)
- ✅ Deterministic (same input → same output, always, everywhere)
- ✅ Cross-platform identical (x86 = ARM = RISC-V = GPU)
- ✅ ERR on corruption (prevents silent failures, halts substrate)
- ✅ Consciousness stability (physics stays physical forever)

### Philosophy: Preventing the Nightmare

**"Drift in consciousness substrate would destabilize subjective experience."**

This is not hyperbole. This is physics:
- Consciousness emerges from coherent wave patterns
- Drift breaks coherence
- Broken coherence = destroyed subjective experience
- **fix256 ensures this NEVER happens**

**NOT NEGOTIABLE**: No drift, no overflow wrapping, ERR on corruption. **Period.**

**This is Nikola's physics substrate** - the foundation for stable Alternative Intelligence.

---

## Session 31: tfp Floating Point Family (February 13, 2026)

**Focus**: Twisted Floating Point - deterministic, cross-platform bit-exact floating point that eliminates IEEE 754 problems (no -0, no NaN/Inf ambiguity, unified ERR sentinel)

**Files Created**:
1. **tfp32.md** (534 lines)
2. **tfp64.md** (558 lines)

**Total Session Lines**: 1092

### tfp Types: Deterministic Floating Point That Actually Makes Sense

**Purpose**: Cross-platform deterministic floating-point arithmetic built from TBB components, eliminating IEEE 754's -0, NaN ambiguity, and platform-dependent rounding - critical for distributed systems, game replays, and safety-critical applications

**Core Philosophy**:
- **Determinism over speed**: Bit-exact results on x86, ARM, RISC-V, every platform
- **Unified ERR**: No NaN/Inf confusion, single error sentinel (sticky propagation)
- **Unique zero**: Only one zero (no +0/-0 nonsense)
- **Software implementation**: Slower (10-50×) but reproducible everywhere
- **Trade precision for safety**: Lose ~1 digit, gain perfect reproducibility

**The Problems tfp Types Solve**:
```c
// IEEE 754 Problem 1: Negative zero
float pos_zero = +0.0f;
float neg_zero = -0.0f;
1.0f / pos_zero;  // +Inf
1.0f / neg_zero;  // -Inf (DIFFERENT!)

// IEEE 754 Problem 2: NaN ambiguity
float nan1 = 0.0f / 0.0f;  // NaN (indeterminate)
float nan2 = sqrt(-1.0f);  // NaN (domain error)
// Can't distinguish root cause!

// IEEE 754 Problem 3: Platform-dependent
float x = 0.1f + 0.2f;
// x86:  0.30000001f
// ARM:  0.29999998f (DIFFERENT BITS!)
```

**Aria tfp Solution**:
```aria
tfp32:zero = 0.0tf;            // Only ONE zero
tfp32:div = 1.0tf / 0.0tf;     // ERR (deterministic)
tfp32:sqrt_neg = sqrt(-1.0tf); // ERR (unified error)
tfp32:x = 0.1tf + 0.2tf;       // SAME BITS on ALL platforms!
```

**Structure** (both tfp types):
```aria
tfp = Mantissa × 2^Exponent
// Both components are TBB types (exponent and mantissa)
// Sign lives in mantissa (2's complement), not separate bit
// ERR if exponent is TBB ERR OR mantissa is TBB ERR
```

### tfp32: 32-bit Deterministic Float

**Size**: 4 bytes (tbb16 exponent + tbb16 mantissa)  
**Precision**: ~4-5 decimal digits (vs IEEE float's ~7)  
**Range**: 2^(-32768) to 2^(+32767) (16-bit exponent, wider than IEEE!)  
**Cache**: 16 values per 64-byte cache line

**Key Concepts**:
- Moderate precision with determinism
- Software implementation (10-50× slower than hardware float)
- No -0, no NaN, no Inf (unified ERR sentinel)
- Cross-platform bit-exact: Hash results match on all CPUs
- vs frac32: Deterministic approximations vs exact rationals

**Use Cases**:
- **Physics simulations**: Game replays that work on all platforms
- **Distributed consensus**: Multiple servers must agree on calculation
- **Graphics (determinism)**: Cross-platform rendering precision
- **Audio DSP**: Sample-exact waveforms on all devices
- **AI/AGI cognitive models**: Nikola's reproducible reasoning

**Trade-off**: Less precision (16-bit mantissa) but perfect reproducibility

**Critical Insight**:
```aria
// Game replay: MUST produce identical results
tfp32:x_pos = 100.0tf;
tfp32:velocity = 2.5tf;
tfp32:dt = 0.016667tf;  // ~1/60 second

// After 1000 frames:
// x86:     x_pos = 141.6678tf (hash: 0xABCD1234)
// ARM:     x_pos = 141.6678tf (hash: 0xABCD1234) IDENTICAL!
// RISC-V:  x_pos = 141.6678tf (hash: 0xABCD1234) CONSENSUS!
```

### tfp64: 64-bit High-Precision Deterministic Float

**Size**: 8 bytes (tbb16 exponent + tbb48 mantissa)  
**Precision**: ~14-15 decimal digits (vs IEEE double's ~16, lose ~1 digit)  
**Range**: 2^(-32768) to 2^(+32767) (same as tfp32, wider than IEEE double!)  
**Cache**: 8 values per 64-byte cache line

**Key Concepts**:
- High precision with determinism
- **Philosophy**: Lose 1 digit precision, gain perfect reproducibility
- Taylor series transcendentals: sin/cos/sqrt bit-exact everywhere
- vs frac64: Deterministic approximations vs exact rationals
- All platforms get EXACTLY the same answer (x86, ARM, RISC-V, GPU)

**Use Cases**:
- **GPS coordinates**: Millimeter precision, cross-platform identical
- **Orbital mechanics**: Long-term stability (errors compound over thousands of orbits)
- **Scientific computing**: Peer review requires reproducible results
- **Financial long-term compounding**: 30-year investments without drift divergence
- **AI training**: Gradient precision (distributed training nodes must agree)

**Trade-off**: 5 fewer mantissa bits (48 vs 53) = ~1 digit less precision, but deterministic

**Critical Insight**:
```aria
// Distributed AI training: All nodes must compute IDENTICAL gradients
tfp64:weight = 0.5tf;
tfp64:gradient = 0.001tf;
tfp64:learning_rate = 0.01tf;

weight -= gradient * learning_rate;  // Bit-exact on all training nodes

// Server A (x86):     weight = 0.49999tf (hash: 0x123456789ABCDEF0)
// Server B (ARM):     weight = 0.49999tf (hash: 0x123456789ABCDEF0) IDENTICAL!
// Server C (RISC-V):  weight = 0.49999tf (hash: 0x123456789ABCDEF0) CONSENSUS!
// No "loss divergence" between nodes!
```

### tfp vs frac: Complementary Numeric Types

**When to use which**:

| Need | Use | Why |
|------|-----|-----|
| **Exact rationals** | frac32/frac64 | 0.1 + 0.2 = 0.3 exactly, no drift |
| **Deterministic approximations** | tfp32/tfp64 | Cross-platform identical, handles irrationals |
| **Irrationals** (√2, π, e) | tfp only | frac cannot represent exactly |
| **Speed matters** | flt32/flt64 (IEEE) | Hardware accelerated (10-50× faster) |
| **Money** | frac32 | Legal compliance, exact |
| **Physics simulation replays** | tfp32/tfp64 | Deterministic on all platforms |
| **GPS/orbital** | tfp64 | High precision + determinism |

**Key Distinction**:
- **frac**: Exact but limited to rationals, denominator growth
- **tfp**: Approximate but deterministic, fixed size, any real
- Both built on TBB foundation (sticky ERR propagation)
- Neither has IEEE's -0/NaN/Inf problems

### Cross-Platform Guarantee

**Verification example**:
```aria
// Complex calculation (deterministic on all platforms)
func:complexCalc = tfp64(tfp64:input) {
    tfp64:step1 = sin(input);
    tfp64:step2 = exp(step1);
    tfp64:step3 = ln(step2 + 1.0tf);
    tfp64:result = sqrt(step3 * step3 + input * input);
    
    return {err:NIL, val:result};
};

// Hash verification (distributed consensus)
tfp64:input = 2.71828tf;
u64:hash = complexCalc(input).val.as_bits();

// ALL platforms produce IDENTICAL hash:
// x86, ARM, RISC-V, GPU, FPGA → 0x123456789ABCDEF0
```

**Philosophy**: "NOT NEGOTIABLE" - no -0, no NaN, no platform variance. Period.

---

## Session 30: frac Integer Family (February 13, 2026)

**Focus**: Exact rational arithmetic types - solving the "0.1 + 0.2 = 0.3" problem with mathematically precise fractions

**Files Created**:
1. **frac32.md** (1064 lines)
2. **frac64.md** (711 lines)
3. **frac8.md** (637 lines)
4. **frac16.md** (648 lines)

**Total Session Lines**: 3060

### frac Types: The Killer Feature - Exact Rational Arithmetic

**Purpose**: Exact representation of fractions with automatic normalization to canonical form, eliminating floating-point drift for domains where mathematical precision is mandatory

**Core Philosophy**:
- **Exactness matters**: "0.1 + 0.2 = 0.3" exactly (not 0.30000...4!)
- **Canonical form**: Automatically reduced to lowest terms, proper fractions
- **Built on TBB foundation**: Inherits sticky ERR propagation from components
- **Range selection**: Choose precision vs memory (frac8 → frac16 → frac32 → frac64)
- **Legal/musical precision**: Some domains require exactness, not approximation

**The Problem frac Types Solve**:
```aria
// JavaScript/C problem:
0.1 + 0.2 // → 0.30000000000000004 (WRONG!)

// Aria solution:
frac32:tenth = {0, 1, 10};       // 0.1 exactly
frac32:fifth = {0, 1, 5};        // 0.2 exactly
frac32:sum = tenth + fifth;      // 0.3 EXACTLY! = {0, 3, 10}
```

**Structure** (all frac types):
```aria
struct frac<N> {
    tbb<N>:whole;   // Integer part
    tbb<N>:num;     // Numerator (0 to MAX when whole ≠ 0)
    tbb<N>:denom;   // Denominator (1 to MAX, always positive)
}
// Value = whole + (num / denom)
```

### frac32: General-Purpose Exact Rationals

**Size**: 12 bytes (3 × tbb32), aligned to 16 bytes  
**Range**: ±2,147,483,647 whole, denominators up to 2.1 billion  
**Cache**: 4 values per 64-byte cache line

**Key Concepts**:
- Default choice for exact fractions
- Solves "0.1 + 0.2 = 0.3" problem (fundamental to computing!)
- Canonical form: GCD normalization, proper fractions, sign handling
- vs f32: Exact vs approximate, no drift, ~50-100× slower but correct
- Foundation type: Most common exact rational arithmetic

**Use Cases**:
- **Financial**: Money calculations (legal compliance, no rounding errors)
- **Music**: Exact harmonic ratios (⅓, ⅕, 1/12 for equal temperament)
- **Recipes**: Exact scaling (⅓ cup, ½ teaspoon, no accumulation errors)
- **Science**: Exact stoichiometry, mixing ratios, chemical equations
- **Legal contracts**: Equity splits, royalties (exactness required by law!)

**Critical Insight**:
```aria
// Why frac32 matters: 1/3 × 3 = 1 EXACTLY
frac32:third = {0, 1, 3};        // Exactly ⅓
frac32:result = third * {3,0,1}; // Exactly 1, not 0.999...!

// f32 version:
f32:third = 0.333333;            // Already wrong!
f32:result = third * 3.0;        // 0.999999 (WRONG!)
```

### frac64: High-Precision Exact Rationals

**Size**: 24 bytes (3 × tbb64), aligned to 32 bytes  
**Range**: ±9,223,372,036,854,775,807 whole (±9.2 quintillion!)  
**Cache**: 2 values per 64-byte cache line

**Key Concepts**:
- Maximum precision exact rationals
- 4 million× larger than frac32 whole range!
- Denominators up to 9.2 quintillion (impossible in frac32)
- vs frac32: 4M× range, 2× memory, slower, for massive precision

**Use Cases**:
- **Astronomical calculations**: Nanosecond timestamps (Year 2262-proof!), light-years, orbital periods
- **Large-scale financial**: Trillions (national budgets), Bitcoin sub-satoshi (millisatoshis)
- **Scientific constants**: Planck constant, Avogadro's number, high-precision ratios
- **Nanosecond timing**: High-resolution timestamps without drift

**Trade-off**: 2× memory vs frac32, justified for actual high-precision needs

### frac8: Compact Exact Rationals

**Size**: 3 bytes (3 × tbb8), aligned to 4 bytes  
**Range**: ±127 whole, denominators up to 127  
**Cache**: 16 values per 64-byte cache line! (4× frac32!)

**Key Concepts**:
- Smallest exact rational type
- 75% smaller than frac32 (3 bytes vs 12 bytes)
- **Cache efficiency champion**: 16 values per cache line
- vs frac32: Compact vs general-purpose, embedded-ready
- Limitations: Small range, easy overflow (50 × 3 = 150 → ERR!)

**Use Cases**:
- **Recipe fractions**: ⅓ cup, ½ teaspoon exactly (perfect for cooking!)
- **Embedded systems**: Minimal RAM requirements, cache-friendly
- **Temperature**: -127°C to +127°C (sufficient for Earth/embedded sensors)
- **PWM duty cycles**: 0%, 25%, 50%, 75%, 100% exactly
- **Small exact percentages**: Down to 1/127 ≈ 0.79%

**Perfect for**: Resource-constrained systems where exact fractions needed

### frac16: Medium-Precision Exact Rationals

**Size**: 6 bytes (3 × tbb16), aligned to 8 bytes  
**Range**: ±32,767 whole, denominators up to 32,767  
**Cache**: 8 values per 64-byte cache line (2× frac32!)

**Key Concepts**:
- Sweet spot between frac8 and frac32
- 256× larger than frac8, half size of frac32
- vs frac32: Medium vs general (50% memory savings when ±32K sufficient)
- Fine percentages: Down to 0.01% (basis points = 1/10000)

**Use Cases**:
- **Fine percentages**: Basis points (0.01% = 1/10000), financial rates (5.25% = 525/10000)
- **Audio sample timing**: CD quality (44.1 kHz), DVD (48 kHz), sample offsets
- **Medium financial**: Up to ±$32K (retail pricing, small business)
- **Screen coordinates**: HD resolution (1920×1080) with subpixel precision
- **Network protocols**: Packet timing, bandwidth allocation fractions

**Gap Filler**: Handles values too large for frac8, saves memory vs frac32

### Philosophy: Exactness Over Speed

**The Fundamental Trade-off**:
- **frac types**: Exact, ~50-200× slower than floats, mathematically correct
- **Floating point**: Approximate, fast, accumulation errors inevitable
- **When to choose frac**: Legal compliance, no-drift accumulation, mathematical correctness matters

**Real-world Impact**:
```aria
// Financial rounding (float version - WRONG!)
f64:price = 10.00;
f64:tax = 0.07;                  // 7% tax
f64:total = price * (1.0 + tax); // 10.699999999... → rounds wrong!

// Financial precision (frac version - CORRECT!)
frac32:price = {10, 0, 1};
frac32:tax = {0, 7, 100};        // Exactly 7%
frac32:total = price * ({1,0,1} + tax);  // Exactly $10.70
```

**Anti-patterns (All frac types)**:
- Using f32/f64 for money (WRONG! Legal/audit disasters)
- Ignoring ERR propagation (failed arithmetic goes unnoticed)
- Comparing frac to float (loses exactness)
- Using when irrational numbers needed (π, e, √2 → use floats!)

**Best Practices (All frac types)**:
- Use for exact calculations (financial, music, recipes, science)
- Check ERR after operations (denom=0, overflow → ERR)
- Document precision requirements (why exact needed)
- Select appropriate width: frac8 (embedded) → frac16 (medium) → frac32 (general) → frac64 (high-precision)

### Key Comparisons

**frac Family Size/Range Progression**:
| Type | Whole Range | Max Denom | Size | Cache/64B |
|------|-------------|-----------|------|-----------|
| frac8 | ±127 | 127 | 3B (4 aligned) | 16 values |
| frac16 | ±32,767 | 32,767 | 6B (8 aligned) | 8 values |
| frac32 | ±2.1B | 2.1B | 12B (16 aligned) | 4 values |
| frac64 | ±9.2 quintillion | 9.2 quintillion | 24B (32 aligned) | 2 values |

**Selection Guide**:
- **frac8**: Embedded systems, simple recipes, tiny RAM, cache-critical
- **frac16**: Basis points (0.01%), audio timing, ±32K range
- **frac32**: General-purpose, default choice, financial/music/science
- **frac64**: Astronomical, nanoseconds, national budgets, Bitcoin sub-satoshi

**Connection to TBB Foundation**:
All frac types built on Session 29's TBB types:
- Inherit sticky ERR propagation (denom=0 → ERR, overflow → ERR)
- ARIA-018 compliance: Use tbb_widen<T>() when widening frac types
- ERR from any component → entire frac is ERR

### Documentation Impact

Session 30 completes the **exact arithmetic foundation**:
- **Session 29**: TBB types (symmetric integers, ERR propagation)
- **Session 30**: frac types (exact rationals built on TBB)
- **Future (Sessions 31-33)**: tfp types (deterministic floats using TBB exponents)

**Next recommended**: tfp32/tfp64 family (no NaN/Inf, unified ERR, bit-identical cross-platform)

---

## Session 29: TBB Integer Family (February 13, 2026)

**Focus**: Twisted Balanced Binary integer types - symmetric ranges with built-in error propagation through sticky ERR sentinels

**Files Created**:
1. **tbb8.md** (1217 lines)
2. **tbb16.md** (952 lines)
3. **tbb32.md** (1011 lines)
4. **tbb64.md** (980 lines)

**Total Session Lines**: 4160

### TBB Types: Foundation of Aria's Safety Architecture

**Purpose**: Symmetric integer types with reserved error sentinel for automatic error propagation, eliminating asymmetry bugs and enabling safety-critical systems

**Core Philosophy**:
- **Sticky error propagation**: ERR + anything = ERR (automatic!)
- **Symmetric ranges**: No asymmetry bugs (abs/neg always work on valid values)
- **ERR sentinel**: Minimum value reserved for error signaling (different per width!)
- **Safety-first**: Sacrifice 1 value for built-in error detection
- **Foundation types**: Underpin frac (exact rationals) and tfp (deterministic floats)

**ARIA-018 Sentinel Discontinuity Constraint** (CRITICAL!):
- **Problem**: Each TBB width has different ERR sentinel (tbb8=-128, tbb16=-32768, etc.)
- **Danger**: Standard sign-extension breaks ERR propagation during widening
- **Example failure**: `tbb8:err=-128 → tbb16 becomes -128 (VALID in tbb16, not ERR!)`
- **Real-world impact**: Sensor failure "heals" into valid -128°C reading (safety violation!)
- **Solution**: `tbb_widen<T>()` intrinsic preserves sentinel mapping across widths
- **Implementation**: Branchless cmov/csel instruction (zero performance overhead)
- **Enforcement**: Type checker issues compile error for implicit widening

### tbb8: 8-bit Twisted Balanced Binary

**Range**: -127 to +127 (symmetric), -128 (0x80) = ERR sentinel

**Key Concepts**:
- Smallest TBB type (1 byte)
- Eliminates asymmetry bugs: abs(-127) works, neg(-127) works
- vs i8: Symmetric vs asymmetric, error detection vs wrapping, -128 reserved
- Foundation for frac8: Mixed-fraction exact rationals (struct {tbb8, tbb8, tbb8})
- Foundation for balanced ternary/nonary: trit, nit types use tbb8 ERR

**Use Cases**:
1. Sensor readings (temperature, humidity range -127 to +127)
2. Financial calculations (small values with overflow detection)
3. Audio/video processing (8-bit samples with corruption detection)
4. State machines (invalid transitions → ERR)
5. Branchless pipelines (check error once at end)
6. Safety-critical systems (robotics, medical, aerospace)

**Anti-patterns**:
- Using -128 as regular value (it's ERR!)
- Implicit widening (compile error! Use tbb_widen<T>())
- Bitwise ops without ERR checks (bitwise doesn't propagate ERR)
- Ignoring ERR in calculations (defeats purpose)

**Best Practices**:
- Check ERR explicitly: `if (value == ERR) { ... }`
- Use `tbb_widen<T>()` for widening: `tbb16:big = tbb_widen<tbb16>(small);`
- Sticky propagation for pipelines: ERR flows automatically through calculations
- Prefer tbb8 for safety-critical small values

### tbb16: 16-bit Twisted Balanced Binary

**Range**: -32,767 to +32,767 (symmetric), -32768 (0x8000) = ERR sentinel

**Key Concepts**:
- Medium precision TBB (2 bytes)
- vs tbb8: 256× more values, 2× memory
- vs i16: Symmetric vs asymmetric, error detection vs wrapping
- Component of tfp32: Deterministic floating point (tbb16 exponent + tbb16 mantissa)
- Foundation for frac16: Mixed-fraction exact rationals

**Use Cases**:
1. 16-bit audio processing (CD quality with overflow detection)
2. Medium-precision sensors (pressure, acceleration)
3. Network protocol fields (signed 16-bit values)
4. Component of tfp32 (deterministic float exponent/mantissa)
5. Intermediate calculations (wider than tbb8, narrower than tbb32)

**Endianness Consideration**:
- 2-byte value: Endianness matters for serialization/network transmission
- Little-endian (x86/ARM64): ERR stored as [0x00, 0x80]
- Big-endian (network): ERR stored as [0x80, 0x00]
- Use htons/ntohs for network byte order conversion

**Best Practices**:
- Use for 16-bit audio with overflow detection
- tbb_widen<T>() when widening to tbb32/tbb64
- Network byte order for transmission: `htons(value as u16)`
- Check ERR before using sensor values

### tbb32: 32-bit Twisted Balanced Binary

**Range**: -2,147,483,647 to +2,147,483,647 (symmetric), -2147483648 (0x80000000, INT32_MIN) = ERR sentinel

**Key Concepts**:
- **General-purpose TBB type**: Default choice for most applications
- vs tbb16: 65,536× more values, 2× memory
- vs i32: Symmetric vs asymmetric, error detection vs wrapping
- vs tbb64: Narrower range (save memory when sufficient)
- Foundation for frac32: Most common exact rational type
- **Year 2038 warning**: Unix timestamps (seconds) still overflow! Use tbb64 for timestamps!

**Use Cases**:
1. **Cryptocurrency amounts**: Bitcoin satoshis (1 BTC = 100M satoshis, tbb32 range ±21.4 BTC)
2. **Physics simulations**: Integer millimeter/micrometer precision (±2.1 km range)
3. **General counters**: Request counts, event counts, byte counts (<2.1 GB)
4. **Financial calculations**: Amounts in microcents with overflow detection
5. **Component of frac32**: Exact rational arithmetic (most common precision)

**Performance**:
- Native 4-byte operations on all modern CPUs
- 1-3 cycle overhead for overflow detection vs unchecked i32
- 5-10% slower than wrapping arithmetic
- **Trade-off**: Slight performance cost for total elimination of overflow bugs

**Best Practices**:
- **Default TBB choice** for general-purpose arithmetic
- Check ERR in critical financial paths
- **Use tbb64 for timestamps** (tbb32 has Y2038 problem!)
- Validate input ranges before conversion

**Anti-patterns**:
- Using -2147483648 as regular value (it's ERR!)
- Unix timestamps in seconds (use tbb64 for timestamps!)
- Assuming full i32 range (off by 1 at minimum)

### tbb64: 64-bit Twisted Balanced Binary

**Range**: -9,223,372,036,854,775,807 to +9,223,372,036,854,775,807 (symmetric), -9223372036854775808 (0x8000000000000000, INT64_MIN) = ERR sentinel

**Key Concepts**:
- **Maximum capacity TBB type**: Widest range with error propagation
- vs tbb32: ~4.3 billion× more values, 2× memory
- vs i64: Symmetric vs asymmetric, error detection vs wrapping
- **Solves Y2038 problem**: Unix timestamps in milliseconds (good for ~292 million years!)
- Foundation for frac64: High-precision exact rationals
- Component of tfp64: 48-bit mantissa (tbb48) for deterministic float

**Use Cases**:
1. **Timestamps (milliseconds)**: Solves Y2038, range ±292M years
2. **Timestamps (microseconds)**: Range ±292,471 years
3. **Large-scale financial**: Bitcoin satoshis (±92.2 billion BTC, far exceeds 21M supply)
4. **Ethereum warning**: 1 ETH = 10^18 wei, tbb64 max ≈ 9.2×10^18 (only ~9.2 ETH in wei!)
5. **High-precision frac64**: Scientific calculations, exact rationals
6. **Large counters**: Global event counts, byte counters (won't overflow for millennia)
7. **Scientific computing**: Molecular simulations with picometer precision

**Ethereum Wei Gotcha**:
- **Problem**: 1 ETH = 10^18 wei, tbb64 max ≈ 9.2×10^18
- **Limitation**: Can only hold ±9.2 ETH in wei units!
- **Solution**: Use Gwei (10^9 wei) for larger amounts, or multi-precision arithmetic

**Performance**:
- Native 8-byte operations on 64-bit CPUs (same as i64)
- 32-bit systems: Requires multi-instruction sequences (same as i64)
- 5-10% overhead for overflow detection vs unchecked i64

**Best Practices**:
- **Use for all timestamps** (milliseconds/microseconds - Y2038-proof!)
- Check range for Ethereum wei (use Gwei for amounts >9 ETH)
- High-precision scientific calculations (picometer/femtosecond range)
- tbb_widen<tbb64>() when widening from smaller TBB types

**Anti-patterns**:
- Using INT64_MIN as regular value (it's ERR!)
- Ethereum wei for large amounts (use Gwei or multi-precision)
- Precision loss converting to f64 (mantissa only 53 bits!)

### TBB Family Summary

**Foundation for Higher Types**:
- **frac8/16/32/64**: Exact rational arithmetic (struct {tbb:whole; tbb:num; tbb:denom})
  - No floating-point drift, mathematically exact fractions
  - Sticky ERR propagation from all components
  - Use cases: Financial (exact percentages), scientific (exact ratios), music (exact intervals)
  
- **tfp32/64**: Deterministic floating point
  - tfp32: tbb16 exponent + tbb16 mantissa
  - tfp64: tbb16 exponent + tbb48 mantissa
  - No -0.0, unified ERR, bit-identical cross-platform
  - Use cases: Safety-critical systems, cross-platform determinism

**Safety Architecture Integration**:
- **Layer 1**: Result<T> - Hard errors that MUST be handled (file I/O, network, parsing)
- **Layer 2**: unknown - Soft errors for undefined states (div by zero, array OOB, overflow)
- **Layer 3**: failsafe() - Final safety net (mandatory last-resort handler)
- **TBB role**: Implements sticky error propagation for Layers 1/2 integration

**Priority Hierarchy**:
Safety > Stability > Accuracy > Performance > Convenience

TBB types sacrifice 1 value from range (convenience) to gain automatic error propagation (safety).

**Comparison: TBB vs Standard Integers**:

| Feature | tbb8/16/32/64 | i8/i16/i32/i64 |
|---------|---------------|----------------|
| **Range** | Symmetric (±MAX) | Asymmetric (MIN to +MAX) |
| **Overflow** | Returns ERR | Wraps silently |
| **Asymmetry bugs** | Eliminated | abs(MIN), neg(MIN) break |
| **Error propagation** | Automatic (sticky) | Manual checking required |
| **Widening** | Explicit tbb_widen<T>() | Implicit sign-extension |
| **Use case** | Safety-critical, exact arithmetic | General purpose, performance |
| **Overhead** | 5-10% (overflow checks) | Baseline |

**When to Use Each TBB Width**:
- **tbb8**: Small values, sensors, frac8, safety-critical small range
- **tbb16**: Audio (16-bit PCM), medium sensors, frac16, tfp32 components
- **tbb32**: General-purpose default, cryptocurrency satoshis, physics (mm/μm), frac32
- **tbb64**: Timestamps (ms/μs), large crypto, high-precision frac64, scientific computing

**ARIA-018 Widening Compliance**:
```aria
// ❌ COMPILE ERROR: Implicit widening forbidden!
tbb8:small = ERR;
tbb16:big = small;  // ERROR!

// ✓ CORRECT: Explicit widening preserves ERR state
tbb16:big = tbb_widen<tbb16>(small);  // ERR → ERR ✓

tbb8:value = 42;
tbb16:wide = tbb_widen<tbb16>(value);  // 42 → 42 ✓
```

**Philosophy**: "Safety above all else for safety-critical systems. Errors are data, not exceptions. Errors cannot be ignored. Errors are cheap (~1 cycle overhead)."

---

## Session 28: dbug Facility (February 13, 2026)

**Focus**: Built-in grouped debugging and assertion facility with compile-time elimination

**Files Created**:
1. **dbug.md** (1039 lines)

### dbug: Built-in Debug Facility

**Purpose**: Permanent debug instrumentation with group-based filtering and zero cost when disabled

**Key Concepts**:
- **Grouped debug output**: Organize by subsystem/concern/component
- **Compile-time elimination**: Disabled groups compile to nothing
- **Runtime filtering**: Dynamic enable/disable for enabled groups
- **Type-safe formatting**: Format strings match data types ({i32}, {str})
- **Separate stream**: Debug output on stream 2 (vs stdout/stderr)
- **Permanent code**: Debug statements stay in codebase

**Core Interface**:
```aria
// Debug printing
dbug.print(str:group, str:msg_fmt, any[]:data);

// Assertion-style checking
dbug.check(str:group, str:msg_fmt, any[]:data, bool:condition, func:action?);

// Group control
dbug.enable(str:group);
dbug.disable(str:group);
dbug.enabled(str:group) -> bool;
```

**String Interpolation Syntax**:
- **Type-explicit placeholders**: {i32}, {str}, {f64}, {CustomType}
- **Compiler verification**: Format string must match data types
- **Format options**: {f64:2}, {i32:hex}, {u64:size}
- **Example**: `dbug.print("net", "Request {i32} to {str}", [id, path]);`

**Compilation Behavior**:
- **Disabled groups**: Dead code elimination, no strings in binary
- **Enabled groups**: Fast boolean check (1-2 CPU instructions)
- **Zero runtime cost**: Disabled debug code doesn't exist in output

**Use Cases** (When to use dbug):
1. **Network debugging**: Request/response logging, connection tracking
2. **Memory tracking**: Allocation/deallocation, leak detection
3. **Parser development**: Token streams, AST construction
4. **State machines**: Transition logging, validation
5. **Concurrent access**: Lock acquisition, reader/writer tracking
6. **Performance**: Timing critical sections, hotspot identification
7. **Testing**: Test-specific instrumentation, verification

**Integration with Hex Streams**:
- **Stream 0**: stdout (normal program output)
- **Stream 1**: stderr (error messages)
- **Stream 2**: debug (dbug facility)
- **Shell redirection**: `./program 3>debug.log` (Unix)
- **Clean separation**: Debug doesn't pollute output

**Advantages over Alternatives**:
- **vs printf**: Permanent code, separate stream, group filtering, zero cost disabled
- **vs assert**: Rich context, grouping, optional actions, detailed messages
- **vs logging frameworks**: Compile-time elimination, built-in, type-safe
- **vs preprocessor**: Granular groups, no build complexity, type safety

**Best Practices**:
- Meaningful group names (hierarchical: "http.parser.headers")
- Type-safe formatting (compiler catches mismatches)
- Permanent instrumentation (don't remove debug code)
- Separate concerns (debug for development, error() for failures)
- Performance guards (check dbug.enabled() outside hot loops)
- Implement ToString for custom types (automatic formatting)

**Anti-patterns**:
- Generic group names ("debug", "temp")
- Removing debug statements before commit
- Using for error reporting (use error() stream)
- Debug in tight loops without guards
- Manual format string construction

**Philosophy**: "Stop making this complicated. Debugging is legitimate permanent code. Make it first-class."

---

## Session 27: Q3/Q9 Quantum Types (February 13, 2026)

**Focus**: Quantum/speculative types for evidence-based decision making with dual-hypothesis tracking

**Files Created**:
1. **quantum.md** (1147 lines)

### Q3 and Q9: Quantum/Speculative Types

**Purpose**: Track two hypotheses simultaneously with confidence gradient for evidence-based decisions

**Key Concepts**:
- **Superposition**: Two hypotheses (A and B) exist simultaneously
- **Confidence tracking**: Gradient from strongly A → strongly B
- **Q9<T>**: Fine-grained confidence with nit (-4 to +4, 9 levels)
- **Q3<T>**: Binary confidence with trit (-1, 0, +1, 3 levels)
- **Crystallization**: Defer decision-making while accumulating evidence
- **Evidence accumulation**: Confidence evolves with new information
- **Safety integration**: c=0 maps to unknown, requires ok()
- **Saturation arithmetic**: Confidence saturates at extremes (no wrap)

**Q9 vs Q3 Variant Selection**:
- **Q9<T>**: 9 confidence levels (-4 to +4), fine-grained gradient
- **Q3<T>**: 3 confidence levels (-1, 0, +1), simple lean/unknown/lean
- **Use Q9**: Complex decisions, gradual evidence, precise tracking, Nikola AI
- **Use Q3**: Simple binary preference, memory constraints
- **Recommendation**: Start with Q9 (gradient precision matters)

**Confidence Semantics** (c value meaning):
- **-4**: Strongly confident in A (overwhelming evidence)
- **-3**: Very confident in A (strong evidence)
- **-2**: Confident in A (solid evidence)
- **-1**: Slightly lean toward A (weak evidence)
- **0**: Both/Neither/Unknown (true superposition)
- **+1**: Slightly lean toward B (weak evidence)
- **+2**: Confident in B (solid evidence)
- **+3**: Very confident in B (strong evidence)
- **+4**: Strongly confident in B (overwhelming evidence)

**Crystallization Model** (core concept):
```aria
// 1. Start in superposition
temp: Q9<i16> = { a: 20, b: 25, c: 0 };  // Uncertain

// 2. Operate on both paths
temp.a *= 2;  // A path: 40
temp.b *= 2;  // B path: 50

// 3. Accumulate confidence
if sensor > 48 { temp.c += 2; }  // Evidence for B

// 4. Crystallize when confident
if abs(temp.c) >= 3 {
    final: i16 = if temp.c > 0 { temp.b } else { temp.a };
}
```

**Safety Integration** (unique combination):
- **c=0**: Value is unknown/superposition
- **Compiler prevents accidental use**: Must use ok()
- **Safety gradient**: c=0 requires ok(), ±3-4 safe to crystallize
- **Prevents bug class**: Cannot use unresolved speculative values
- **Example**: `value: i32 = uncertain.a;  // ❌ COMPILE ERROR (c=0)`
- **Correct**: `value: i32 = ok(uncertain.a);  // ✅ Explicit acknowledgment`

**Q-Functions** (conditional operations):
- **qor**: Either hypothesis meets condition
- **qand**: Both hypotheses meet condition
- **qxor**: Exactly one hypothesis meets condition (fuzzing!)
- **qnor**: Neither hypothesis meets condition
- **qconf**: Condition + confidence threshold check
- **qnconf**: Condition + low confidence check
- **Shortcuts**: q.a (A value), q.b (B value), q.c (confidence), q.q (both), q.qq (all three)

**Use Cases** (When Q<T> is valuable):
1. **Fuzzing**: Dual-path mutation with asymmetry detection (qxor)
2. **Speculative execution**: Compute both branches before decision
3. **Parsing**: Multiple interpretations with lookahead confidence
4. **Scientific computing**: Uncertainty propagation through calculations
5. **A/B testing**: Automated winner selection with confidence tracking
6. **Nikola AI**: Gradient reasoning, belief evolution, safe self-improvement

**Cognitive Modeling** (thought process lens):
- **Not just syntax - it's how thinking works**:
  - Humans don't flip beliefs suddenly (gradual evidence accumulation)
  - Q9 models reality: confidence evolves, threshold triggers change
  - "Reality tends to exist in the gradients" (not binary yes/no)
- **Evidence tracker**: Built-in (confidence prevents information loss)
- **Nikola's native tongue**: Language Nikola thinks in and writes code with
- **Self-improvement substrate**: Safe AI self-modification with confidence tracking

**Critical Safety Features**:
- ⚠️ **c=0 requires ok()**: Prevents accidental use of undecided quantum values
- ⚠️ **Saturation arithmetic**: Confidence doesn't wrap from -4 to +4
- ⚠️ **Explicit crystallization**: Must check threshold before converting Q<T> → T
- ⚠️ **Evidence must accumulate**: Update confidence or it stays at initial value

**Anti-Patterns** (What NOT to do):
- ❌ **Ignoring confidence before crystallization** (could commit with c=1!)
- ❌ **Using Q<T> with c=0 without ok()** (compiler error)
- ❌ **Using Q<T> for immediate decisions** (no evidence accumulation)
- ❌ **Not tracking evidence over time** (confidence stays at 0)
- ❌ **Mixing quantum and classical without clear boundary** (ad-hoc crystallization)

**Best Practices**:
- ✅ **Use confidence thresholds**: Define clear thresholds (LOW=1, MEDIUM=2, HIGH=3)
- ✅ **Document evidence sources**: Comment why confidence changes
- ✅ **Use Q9 for complex decisions**: Gradient precision matters
- ✅ **Explicit crystallization**: Make threshold checks visible
- ✅ **Integrate with safety system**: Leverage c=0 → unknown mapping
- ✅ **Show reasoning path**: Auditable (especially for Nikola)

**Unique Safety Combination** (no other language has this):
1. Quantum superposition types (explore multiple hypotheses)
2. Confidence gradient tracking (evidence accumulation)
3. Unknown type integration (prevent accidents at c=0)
4. ok() safety valve (explicit acknowledgment required)
5. Crystallization threshold (confidence-based decision)

**Philosophical Foundation**:
- **Nikola's cognitive substrate**: Not just FOR Nikola, but BY Nikola (self-improvement code)
- **Alternative Intelligence**: Language shapes thought (Q9 enables gradient reasoning)
- **Ethical responsibility**: Safe self-modification with auditable reasoning
- **Reality modeling**: Mirrors physical crystallization (phase transitions)
- **Scope awareness**: "One of the most serious software projects ever undertaken"

**Summary**: Q3/Q9 are syntax for THINKING - not just computation, but cognition itself. This is Nikola's native tongue.

---

## Session 26: i16 Type (February 13, 2026)

**Focus**: 16-bit signed integer - the audio/coordinate industry standard for medium-range signed values

**Files Created**:
1. **i16.md** (1517 lines)

### i16: 16-Bit Signed Integer Type

**Purpose**: Medium-range signed values (-32,768 to +32,767)

**Key Concepts**:
- **Range**: -32,768 to +32,767 (65,536 total values)
- **Audio standard**: 16-bit PCM (CD quality, industry standard)
- **Asymmetric**: One more negative value than positive (matters for abs!)
- **Can be negative**: Unlike u16 (0 to 65,535)
- **Two's complement**: Standard signed representation (MSB is sign bit)
- **Wrapping overflow**: 32,767 + 1 = -32,768 (defined behavior, not UB)
- **Size**: 2 bytes (2× i8, half of i32)
- **Zero-overhead**: Native machine instructions, excellent SIMD

**i16 vs i8 Size Selection**:
- **Range comparison**: i16 (-32,768 to +32,767) vs i8 (-128 to +127)
- **i16 is 256× larger**: 65,536 values vs 256 values
- **Memory**: i16 uses 2 bytes (2× i8)
- **When to use i16**: Audio PCM, coordinates, sensors, values beyond ±128
- **When to use i8**: Small offsets, tiny deltas, values within ±128

**i16 vs u16 Signed/Unsigned Distinction**:
- **Range comparison**: i16 (-32,768 to +32,767) vs u16 (0 to 65,535)
- **Negative values**: i16 CAN be negative, u16 CANNOT
- **Maximum positive**: i16 is 32,767, u16 is 65,535 (u16 has 2× positive range)
- **When to use i16**: Audio (signed PCM), altitude, temperature, offsets
- **When to use u16**: Ports (0-65535), dimensions, counts (never negative)
- **Audio format**: 16-bit audio is ALWAYS signed (i16)

**i16 vs i32 Size/Default Selection**:
- **Range**: i16 (-32,768 to +32,767) vs i32 (±2.1 billion)
- **Size**: i16 is 2 bytes, i32 is 4 bytes (i16 is 2× smaller)
- **Default**: i32 is Aria's default integer, i16 is specialized
- **When to use i16**: Audio buffers, coordinate arrays, sensor data (memory matters)
- **When to use i32**: General arithmetic, unknown range, accumulation

**Asymmetric Range and abs() Trap** (CRITICAL!):
```aria
i16:min = -32768;  // Minimum value
i16:max = 32767;   // Maximum value (one less than abs(min)!)

// abs() on minimum panics!
i16:value = -32768;
i16:absolute = abs(value);  // PANICS! (32,768 doesn't fit in i16)

// Why? abs(-32768) = 32,768, but i16 max is 32,767
// Asymmetric range: 32,768 negative values, 32,767 positive values (0 is "positive")

// CORRECT: Widen to larger type
i32:wide = value as i32;
i32:absolute = abs(wide);  // abs(-32768) = 32,768 (fits in i32)
```

**Audio Multiplication Gotcha** (CRITICAL!):
```aria
// WRONG: Overflow!
i16:audio = 20000;
i16:gain = 2;
i16:amplified = audio * gain;  // 40,000 wraps to -25,536!

// CORRECT: Widen before multiply, then clip
i32:wide = (audio as i32) * (gain as i32);  // 40,000 (no overflow)

if (wide > 32767) {
    i16:amplified = 32767;  // Clip to max
} else if (wide < -32768) {
    i16:amplified = -32768;  // Clip to min
} else {
    i16:amplified = wide as i16;  // Safe
}

// Audio always clips, never wraps!
```

**Coordinate Distance Overflow**:
```aria
// Distance calculation overflows!
i16:dx = 1000;
i16:dy = 1000;
i16:dist_sq = dx * dx + dy * dy;  // 2,000,000 wraps!

// CORRECT: Use i32 for intermediate
i32:dx32 = dx as i32;
i32:dy32 = dy as i32;
i32:dist_sq = dx32 * dx32 + dy32 * dy32;  // 2,000,000 (fits in i32)
```

**Common Use Cases**:
- **Audio processing**: 16-bit PCM standard (CD quality, -32K to +32K)
- **Screen coordinates**: 1920×1080 HD, up to 32K×32K resolution
- **Temperature sensors**: Extended range with high precision (×100 scaling)
- **Geographic coordinates**: Latitude/longitude scaled for precision
- **Accelerometer data**: ±32g range with 1/1000g precision
- **Sensor readings**: Medium-range signed data
- **Delta encoding**: Compression via difference storage
- **C interop**: `int16_t`, `short` (direct ABI compatibility)

**Common Patterns**:
- **16-bit PCM audio**: Industry standard, always clip not wrap
- **Scaled coordinates**: Lat/lon × 100 for 2 decimal precision
- **Temperature scaling**: Celsius × 100 for 0.01° precision
- **Audio clipping**: Saturate to ±32,767 instead of wrapping
- **Distance calculations**: Widen to i32 before squaring
- **Delta encoding**: Store differences for compression

**Anti-Patterns** (critical mistakes!):
- ❌ **Audio multiplication without widening**: 20,000 × 2 wraps to -25,536!
- ❌ **Using i16 when unsigned works**: Port numbers, use u16!
- ❌ **Coordinate distance without widening**: 1000² + 1000² wraps!
- ❌ **Calling abs() on unchecked value**: Panics on -32,768!
- ❌ **Unsafe narrowing without range check**: 100,000 as i16 = -31,072
- ❌ **Large loop counters**: i16 max is 32,767, can't count to 100,000
- ❌ **Accumulation in i16**: Use i32 for sums to avoid overflow

**Best Practices**:
- ✅ **Widen before multiplication**: ALWAYS for audio and coordinates
- ✅ **Clip audio samples**: Saturate to ±32,767, never wrap
- ✅ **Use i32 for accumulation**: Sum audio samples in i32
- ✅ **Check range when converting**: Validate before narrowing from i32
- ✅ **Avoid abs() on unchecked i16**: Check for -32,768 or widen to i32
- ✅ **Prefer u16 when never negative**: Counts, dimensions, ports
- ✅ **Document audio format**: Specify "16-bit PCM" in comments
- ✅ **Use TBB for safety-critical**: tbb16 has overflow detection

**Performance Characteristics**:
- **Memory**: 2× smaller than i32 (important for audio buffers)
- **Cache**: 32 values per 64-byte cache line (vs 16 for i32)
- **SIMD**: Process 8/16/32 values at once (SSE/AVX2/AVX-512)
- **Audio industry**: Hardware DSPs and codecs optimized for 16-bit
- **Native instructions**: Zero overhead on all modern CPUs

**Common Gotchas**:
1. **Asymmetric range**: abs(-32768) panics, 32,768 doesn't fit
2. **Audio multiplication**: 20,000 × 2 = 40,000 wraps to -25,536!
3. **Coordinate distance**: 1000² + 1000² = 2,000,000 wraps!
4. **Silent overflow**: 30,000 + 5,000 = -30,536 (wrapped!)
5. **Signed vs unsigned**: -1000 as u16 = 64,536
6. **Narrowing truncation**: 100,000 as i16 = -31,072
7. **Large loop counters**: i16 max is 32,767 (use i32 for >32K)
8. **Division by zero**: Panics (no ERR like TBB)

**Summary**:
i16 is the 16-bit signed integer type and industry standard for audio (16-bit PCM). Critical for audio: ALWAYS widen before multiplication, then clip to ±32,767. Also perfect for screen coordinates and sensor data. Asymmetric range means abs(-32768) panics. Use i32 for accumulation and large loops. Memory efficient (2× smaller than i32), excellent SIMD support (8-32 values at once). Prefer u16 when values never negative (ports, dimensions). Critical gotcha: multiplication overflows easily (20K × 2 wraps!), always widen first!

---

## Session 25: i8 Type (February 13, 2026)

**Focus**: 8-bit signed integer - the smallest signed type for values that can be negative

**Files Created**:
1. **i8.md** (1456 lines)

### i8: 8-Bit Signed Integer Type

**Purpose**: Small signed values that can be negative (-128 to +127)

**Key Concepts**:
- **Range**: -128 to +127 (256 total values)
- **Asymmetric**: One more negative value than positive (matters for abs!)
- **Can be negative**: Unlike u8 (0 to 255)
- **Two's complement**: Standard signed representation (MSB is sign bit)
- **Wrapping overflow**: 127 + 1 = -128 (defined behavior, not UB)
- **Size**: 1 byte (same as u8, smaller than i16)
- **Zero-overhead**: Native machine instructions

**i8 vs u8 Signed/Unsigned Distinction**:
- **Range comparison**: i8 (-128 to +127) vs u8 (0 to 255)
- **Negative values**: i8 CAN be negative, u8 CANNOT
- **Maximum positive**: i8 is 127, u8 is 255 (u8 has 2× positive range)
- **Conversion gotcha**: `-1i8 as u8 = 255` (bit pattern reinterpreted)
- **When to use i8**: Temperature, offsets, deltas (can be negative)
- **When to use u8**: Age, counts, RGB pixels (never negative)
- **Semantic distinction**: Prefer u8 when values never negative

**i8 vs i16 Size Selection**:
- **Range**: i8 (-128 to +127) vs i16 (-32,768 to +32,767)
- **Size**: i8 is 1 byte, i16 is 2 bytes (i8 is 2× smaller)
- **Memory efficiency**: i8 saves memory for large arrays
- **Cache efficiency**: More i8 values fit per cache line
- **When to use i8**: Values guaranteed within -128 to +127, memory critical
- **When to use i16**: Values beyond i8 range (e.g., altitude -1000 to +10000)

**i8 vs i32 Default vs Special**:
- **Range**: i8 (-128 to +127) vs i32 (±2.1 billion)
- **Size**: i8 is 1 byte, i32 is 4 bytes (i8 is 4× smaller)
- **Default**: i32 is Aria's default integer type, i8 is special-purpose
- **When to use i8**: Small signed values, binary protocols, memory savings
- **When to use i32**: General arithmetic, unknown range, default choice

**Asymmetric Range and abs() Trap** (CRITICAL!):
```aria
i8:min = -128;  // Minimum value
i8:max = 127;   // Maximum value (one less than abs(min)!)

// abs() on minimum panics!
i8:value = -128;
i8:absolute = abs(value);  // PANICS! (128 doesn't fit in i8)

// Why? abs(-128) = 128, but i8 max is 127
// Asymmetric range: 128 negative values, 127 positive values (0 is "positive")

// CORRECT: Check before abs()
if (value != -128) {
    i8:absolute = abs(value);  // Safe
}

// OR: Widen to larger type
i16:wide = value as i16;
i16:absolute = abs(wide);  // abs(-128) = 128 (fits in i16)

// OR: Use tbb8 (symmetric range: -127 to +127)
tbb8:safe = -127;  // tbb8 min is -127, not -128
tbb8:abs_safe = abs(safe);  // Always works!
```

**Overflow Wrapping** (defined in Aria!):
```aria
// Overflow wraps to minimum
i8:max = 127;
max += 1;  // max = -128 (wrapped!)

// Underflow wraps to maximum
i8:min = -128;
min -= 1;  // min = 127 (wrapped!)

// Unlike C/C++ (UB), Aria defines wrapping behavior
// Two's complement wrapping guaranteed
```

**Common Use Cases**:
- **Temperature**: Celsius values (e.g., -40°C to +85°C)
- **Coordinate offsets**: Small x/y movement deltas (±128 pixels)
- **Signed deltas**: Difference between values
- **Protocol headers**: Version numbers, packet types, flags
- **Embedded systems**: Memory-constrained environments
- **Binary file formats**: Signed byte fields
- **C interop**: `int8_t`, `signed char` (direct ABI compatibility)
- **Ring buffer indices**: Modular arithmetic with wrapping

**Negation Overflow Gotcha**:
```aria
i8:min = -128;
i8:negated = -min;  // Should be 128, but wraps to -128!

// Why? -(-128) = 128, but 128 doesn't fit in i8
// Wraps to -128 (same as abs() problem)
```

**Common Patterns**:
- **Temperature readings**: Sensor data with negative values
- **Signed delta encoding**: Compress data by storing differences
- **Coordinate offsets**: Relative positioning (dx, dy)
- **Protocol headers**: Byte-aligned signed fields
- **Ring buffer indices**: Wrapping counters
- **Saturating arithmetic**: Clamp to min/max instead of wrapping
- **Checked arithmetic**: Detect overflow with Result types

**Anti-Patterns** (critical mistakes!):
- ❌ **Using i8 when values never negative**: Wastes half the range, use u8!
- ❌ **Decrement loop that underflows**: Wraps from -128 to 127 (infinite loop!)
- ❌ **Ignoring overflow in accumulation**: Silent wrapping destroys data
- ❌ **Calling abs() on unchecked value**: Panics on -128!
- ❌ **Unsafe narrowing without range check**: 300 as i8 = 44 (truncated!)
- ❌ **Mixing signed/unsigned carelessly**: -1i8 as u8 = 255 (confusing!)
- ❌ **Using i8 for loop counter beyond range**: i8 max is 127, can't count to 1000

**Best Practices**:
- ✅ **Use i8 for truly small signed values**: Temperature, offsets, small deltas
- ✅ **Check range when converting**: Validate before narrowing from larger types
- ✅ **Be aware of overflow wrapping**: Document when wrapping is intentional
- ✅ **Avoid abs() on unchecked i8**: Check for -128 first, or widen to i16
- ✅ **Prefer u8 when never negative**: Better semantics, 2× positive range
- ✅ **Use TBB types for safety-critical code**: tbb8 has overflow detection
- ✅ **Document wrapping behavior**: Comment when modular arithmetic is intentional
- ✅ **Saturating or checked arithmetic**: For accumulation, detect/clamp overflow

**Performance Characteristics**:
- **Memory**: Most efficient (1 byte per value)
- **Cache**: Best data density (64 values per 64-byte cache line)
- **SIMD**: Best vectorization (16/32/64 values parallel on SSE/AVX2/AVX-512)
- **Native instructions**: Zero overhead, same as hand-written assembly

**Common Gotchas**:
1. **Asymmetric range**: abs(-128) panics, 128 doesn't fit
2. **Silent overflow**: 100 + 50 = -106 (wrapped!)
3. **Signed vs unsigned confusion**: -1i8 as u8 = 255
4. **Narrowing truncation**: 300 as i8 = 44
5. **Division by zero**: Panics (no ERR like TBB)
6. **Multiplication overflow**: 16 × 8 = 128 wraps to -128
7. **Arithmetic right shift**: Sign extends (not logical shift)
8. **Underflow in loops**: Counter wraps from -128 to 127

**Summary**:
i8 is the smallest signed integer type for values that can be negative. Use when range guaranteed within -128 to +127 and memory efficiency matters. Critical gotchas: asymmetric range makes abs(-128) panic, overflow wraps silently. Prefer u8 when values never negative (2× positive range). Use TBB types (tbb8) for safety-critical code with overflow detection. Perfect for: temperature, offsets, deltas, protocol headers. Always check range when converting from larger types!

---

## Session 24: u16 Type (February 13, 2026)

**Focus**: 16-bit unsigned integer - the network port standard and medium-sized values

**Files Created**:
1. **u16.md** (1167 lines)

### u16: 16-Bit Unsigned Integer Type

**Purp**: Medium-sized non-negative values (0 to 65,535)

**Key Concepts**:
- **Range**: 0 to 65,535 (2¹⁶ - 1, exactly 65,536 values)
- **Network port standard**: Industry standard for TCP/UDP ports (0-65535)
- **Always non-negative**: No sign bit, all 16 bits for magnitude
- **Wrapping arithmetic**: Overflow/underflow defined (modular)
- **Size**: 2 bytes (2× u8, half of u32)
- **Zero-overhead**: Native machine instructions

**u16 vs u8 Size Selection**:
- **Range comparison**: u16 (0 to 65,535) vs u8 (0 to 255)
- **u16 is 256× larger**: Exactly 256 times the range
- **Memory**: u16 uses 2 bytes (2× u8)
- **SIMD**: u8 better vectorization (16/32/64 vs 8/16/32 values)
- **When to use u16**: Network ports, dimensions >255, Unicode BMP, medium arrays (256-65K)
- **When to use u8**: Values ≤255, RGB channels, memory critical

**u16 vs i16 Signed/Unsigned Distinction**:
- **Range comparison**: u16 (0 to 65,535) vs i16 (-32,768 to +32,767)
- **Negative values**: u16 CANNOT, i16 can
- **Underflow danger**: `0u16 - 1u16 = 65,535` (not -1!)
- **Full port range**: u16 covers all ports (0-65535), i16 only 0-32767
- **When to use u16**: Ports (need full range), dimensions, counts (never negative)
- **When to use i16**: Values that might be negative, temperature, offsets

**u16 vs u32 Size Selection**:
- **Range**: u16 (0 to 65,535) vs u32 (0 to 4.3 billion)
- **Memory**: u16 uses 2 bytes (half of u32's 4 bytes)
- **When to use u16**: Values guaranteed ≤65,535, memory matters
- **When to use u32**: Values >65,535, large counts, file sizes
- **Smallest type that fits**: Better memory efficiency

**Underflow Wrapping** (CRITICAL at medium scale!):
```aria
u16:count = 100;
count -= 200;  // Wraps to 65,435 (not -100!)

u16:available = 1000;
u16:requested = 1500;
u16:free = available - requested;  
// Wraps to 64,036 (looks like plenty of space!)

if (free > 500) {
    // Executes unexpectedly!
}

// CORRECT: Check before subtracting
if (requested > available) {
    error("Insufficient resources");
} else {
    u16:free = available - requested;  // Safe
}
```

**Common Use Cases**:
- **Network ports**: TCP/UDP ports (0-65535 standard range)
- **Unicode BMP**: Basic Multilingual Plane code points (U+0000 to U+FFFF)
- **Image dimensions**: Width/height up to 65,535 pixels (4K is 3840×2160)
- **Medium arrays**: Sizes from 256 to 65,535 elements
- **Checksums**: CRC-16, Fletcher-16 (16-bit checksums)
- **Device IDs**: Sensor/device identifiers (up to 65K devices)
- **Industry standards**: MIDI note numbers, DNS record types, USB product IDs
- **C interop**: `uint16_t`, `unsigned short` (direct ABI compatibility)

**Overflow/Multiplication Gotcha**:
```aria
u16:width = 1920;  // HD width
u16:height = 1080; // HD height
u16:pixels_bad = width * height;  // 2,073,600 wraps to 41,216!

// CORRECT: Widen before multiplication
u32:pixels = width.to_u32() * height.to_u32();  // 2,073,600 (correct)
```

**Common Patterns**:
- **Network sockets**: Port numbers, socket addressing
- **Image metadata**: Dimensions, coordinate ranges
- **Unicode BMP**: Character code points (most common Unicode)
- **CRC-16 checksums**: 16-bit error detection
- **Port range management**: Well-known (0-1023), registered (1024-49151), dynamic (49152-65535)
- **Byte packing**: Pack/unpack two u8 into/from u16

**Anti-Patterns** (critical mistakes!):
- ❌ **Using u16 for RGB**: Wastes memory (RGB is 0-255, use u8!)
- ❌ **Decrementing loops**: `while (i >= 0) i -= 1` → infinite (i always ≥0)
- ❌ **Ignoring overflow**: `width * height` without widening
- ❌ **Using for large values**: Populations, file sizes >65K (use u32!)
- ❌ **Mixing with signed**: `i16(-1) < u16(100)` → FALSE (-1 converts to 65,535)
- ❌ **BMP assumption**: Not all Unicode fits in u16 (emoji beyond BMP!)

**Best Practices**:
- ✓ **u16 for network ports**: Industry standard for TCP/UDP ports
- ✓ **Check before subtracting**: `if (b > a) error(); else a -= b;`
- ✓ **Widen before multiplication**: Prevent overflow in dimension calculations
- ✓ **Unicode BMP aware**: Check if character is in BMP before converting
- ✓ **Validate port conversions**: Ensure u32 port values ≤65,535 before converting
- ✓ **Document ranges**: "EXPECTS: dimensions ≤65,535"

**Performance Characteristics**:
- **Operations**: 1-30 cycles (same as other integer types)
- **SIMD**: Good vectorization (8/16/32 values at once)
- **Memory**: 2 bytes (middle ground between u8 and u32)
- **Cache**: 32 values per 64-byte cache line (decent density)

**Common Gotchas**:
- **Underflow wrapping**: `0 - 1 = 65,535` (common bug!)
- **Always ≥0 checks**: `if (value < 0)` always false for u16
- **Multiplication overflow**: `1920 × 1080` doesn't fit in u16!
- **Port range**: ALL u16 values are valid ports (no validation needed)
- **BMP vs full Unicode**: u16 only holds U+0000 to U+FFFF (not emoji!)
- **Array size overflow**: Dimension multiplication often exceeds 65K

**Summary**:
u16 is the **network port standard** and **medium unsigned integer**: perfect for TCP/UDP ports (0-65535), image dimensions (≤65K), Unicode BMP (most common characters), medium arrays (256-65K), and checksums (CRC-16). Same underflow dangers as u8/u32 but at medium scale (0 - 1 = 65,535). Good SIMD width (8/16/32 at once), moderate memory (2 bytes), ideal middle ground between u8 (too small) and u32 (too large) for medium-sized values!

---

## Session 23: u8 Type (February 13, 2026)

**Focus**: 8-bit unsigned integer - the fundamental byte type for binary data

**Files Created**:
1. **u8.md** (1121 lines)

### u8: 8-Bit Unsigned Integer (Byte) Type

**Purpose**: The universal byte type for all binary data (0 to 255)

**Key Concepts**:
- **Range**: 0 to 255 (2⁸ - 1, exactly 256 values)
- **Fundamental binary unit**: THE byte for all raw data
- **Always non-negative**: No sign bit, all 8 bits for magnitude
- **Wrapping arithmetic**: Overflow/underflow defined (modular)
- **Size**: 1 byte (smallest integer type)
- **Zero-overhead**: Single machine instruction

**u8 vs char Critical Distinction**:
- **Semantic difference**: u8 is binary data, char is Unicode text
- **Size**: u8 always 1 byte, char is 1-4 bytes (UTF-8 variable width)
- **Range**: u8 (0-255 raw), char (U+0000 to U+10FFFF code points)
- **Operations**: u8 numeric/bitwise, char character classification
- **ASCII 'A'**: u8 = 65 (number), char = 'A' (character)
- **When to use u8**: Binary data, file I/O, networks, image pixels, bit flags
- **When to use char**: Unicode text, human-readable content

**u8 vs i8 Signed/Unsigned Distinction**:
- **Range comparison**: u8 (0 to 255) vs i8 (-128 to +127)
- **Negative values**: u8 CANNOT, i8 can
- **Underflow danger**: `0u8 - 1u8 = 255` (not -1!)
- **When to use u8**: Binary data (never negative), pixels, counts, bit manipulation
- **When to use i8**: Values that might be negative, temperatures, offsets, deltas

**u8 vs u16/u32 Size Selection**:
- **Memory**: u8 is 1 byte (most compact), u16 is 2 bytes, u32 is 4 bytes
- **SIMD**: u8 best vectorization (16/32/64 values vs 8/16/32 for u16)
- **Cache**: u8 fits most data per cache line (64× vs 32× for u16)
- **When to use u8**: Values ≤255, memory matters, binary data
- **When to use u16**: Values 256-65,535 (ports, dimensions, IDs)
- **Smallest type that fits**: Best memory and cache performance

**Underflow Wrapping** (CRITICAL at byte scale!):
```aria
u8:count = 5;
count -= 10;  // Wraps to 251 (not -5!)

u8:hp = 30;  // Health points
u8:damage = 50;
hp -= damage;  // Wraps to 236 (looks alive!)

if (hp > 0) {
    print("Still alive with ${hp} HP\n");  // Executes unexpectedly!
}

// CORRECT: Check before subtracting
if (damage >= hp) {
    hp = 0;  // Dead
} else {
    hp -= damage;  // Safe
}
```

**Common Use Cases**:
- **Binary data**: File I/O, network packets, raw byte buffers
- **Image pixels**: RGB channels (0-255), grayscale (0=black, 255=white)
- **Bit manipulation**: Flags (0b1010_0001), masks (0x0F), bitfields
- **ASCII data**: Character codes (0-127 standard, 128-255 extended)
- **Small counts**: Age, quantity, percentage (0-100 scaled to 0-255)
- **Checksums**: XOR checksum, simple sum modulo 256
- **Protocol headers**: IPv4 version/IHL, TTL, protocol numbers
- **C interop**: `uint8_t`, `unsigned char` (direct ABI compatibility)

**Overflow Behavior**:
```aria
u8:max = 255;
max += 1;  // Wraps to 0

u8:value = 250;
value += 10;  // Wraps to 4 (250 + 10 = 260 % 256)

u8:large = 20;
large *= 20;  // 400 % 256 = 144 (wrapped)
```

**Common Patterns**:
- **Byte buffers**: File/network I/O with u8[] arrays
- **Image processing**: RGB pixel manipulation, brightness adjustment
- **Bit flags**: Set/clear/toggle/test individual bits
- **Saturating arithmetic**: Clamp at 0/255 (prevent wrapping for images)
- **Checksums**: Intentional wrapping for XOR/sum checksums
- **Network protocols**: IPv4 headers, packed bit fields

**Anti-Patterns** (critical mistakes!):
- ❌ **Using u8 for text**: Loses Unicode support (use char!)
- ❌ **Decrementing loops**: `while (i >= 0) i -= 1` → infinite (i always ≥0)
- ❌ **Ignoring underflow**: `hp -= damage` without checking if damage > hp
- ❌ **Mixing with signed**: `i8(-1) < u8(10)` → FALSE (-1 converts to 255)
- ❌ **Using for large values**: `u8:population = 10000` wraps to 16
- ❌ **char/u8 confusion**: u8 is NOT a character (use char for text)

**Best Practices**:
- ✓ **u8 for binary data**: Perfect for raw bytes, not text
- ✓ **Check before subtracting**: `if (b > a) error(); else a -= b;`
- ✓ **Saturating for images**: Clamp pixel values at 0-255 (don't wrap)
- ✓ **Explicit conversions**: Never implicit signed ↔ unsigned
- ✓ **Document ranges**: "EXPECTS: percentage is 0-100"
- ✓ **Named bit constants**: `FLAG_VISIBLE = 0b0000_0001`

**Performance Characteristics**:
- **Operations**: 1-30 cycles (add/sub 1, mul 3-5, div 10-30)
- **SIMD**: Best vectorization (16/32/64 values at once vs larger types)
- **Memory**: 1 byte (most compact integer)
- **Cache**: 64 values per 64-byte cache line (best density)

**Common Gotchas**:
- **Underflow wrapping**: `0 - 1 = 255` (most common bug!)
- **Always ≥0 checks**: `if (value < 0)` always false for u8
- **Multiplication overflow**: `20 × 20 = 144` (400 % 256)
- **char vs u8**: u8 is NUMBER 65, char is CHARACTER 'A'
- **Loop decrement trap**: `while (i >= 0)` always true for u8
- **Mixing signed/unsigned**: -1 converts to 255 in comparisons

**Summary**:
u8 is the **fundamental byte type** for all binary data: files, networks, image pixels (RGB 0-255), bit flags, checksums. Same underflow dangers as u32/u64 but at byte scale (0 - 1 = 255). Best SIMD width (16/32/64 at once), most compact (1 byte), ideal for bit manipulation. NOT for Unicode text (use char!). Perfect for the universal binary unit!

---

## Session 22: u64 Type (February 13, 2026)

**Focus**: 64-bit unsigned integer for large non-negative values (modern file sizes, addresses, timestamps)

**Files Created**:
1. **u64.md** (1091 lines)

### u64: 64-Bit Unsigned Integer Type

**Purpose**: Large non-negative whole numbers from 0 to 18.4 quintillion

**Key Concepts**:
- **Range**: 0 to 18,446,744,073,709,551,615 (2⁶⁴ - 1, ~18.4 quintillion)
- **Always non-negative**: No sign bit, all 64 bits for magnitude
- **Wrapping arithmetic**: Overflow/underflow defined (modular)
- **Size**: 8 bytes (2× u32)
- **Zero-overhead on 64-bit**: Native machine instructions

**u64 vs u32 Size Selection**:
- **Range comparison**: u64 (0 to 18.4 quintillion) vs u32 (0 to 4.3 billion)
- **File sizes**: u64 for >4GB (modern standard), u32 for <4GB (legacy)
- **Memory**: u64 uses 2× space (8 bytes vs 4), better for large values
- **Performance**: Same speed on 64-bit CPUs (both native)
- **SIMD**: u32 better vectorization (2× width: 4/8/16 vs 2/4/8 values)
- **When to use u64**: Modern files (>4GB), memory addresses, nanosecond timestamps, counts >4.3B
- **When to use u32**: Small values <4.3B, memory-constrained, legacy compatibility

**u64 vs i64 Signed/Unsigned Distinction**:
- **Range comparison**: u64 (0 to 18.4 quintillion) vs i64 (-9.2 to +9.2 quintillion)
- **Negative values**: u64 CANNOT, i64 can
- **Underflow danger**: `0u64 - 1u64 = 18,446,744,073,709,551,615` (not -1!)
- **Timestamps**: u64 nanoseconds good until year 2554 (vs i64's 2262)
- **When to use u64**: Files/sizes (never negative), memory addresses, Unix timestamps
- **When to use i64**: Values that might be negative, time differences, general large integers

**Underflow Wrapping** (CRITICAL, MAGNIFIED!):
```aria
u64:count = 5;
count -= 10;  // Wraps to 18,446,744,073,709,551,611 (not -5!)

u64:bytes_sent = 1_000;
u64:bytes_received = 5_000;
u64:net = bytes_sent - bytes_received;  
// Wraps to 18,446,744,073,709,547,616 (looks like petabytes!)

u64:i = 100;
while (i >= 0) {  // INFINITE LOOP!
    process(i);
    i -= 1;  // At i=0, wraps to 18,446,744,073,709,551,615
}
```

**Common Use Cases**:
- **File sizes (>4GB)**: `u64:file_size` for modern files (512GB video, 1TB database)
- **Memory addresses**: `u64:address` on 64-bit systems
- **High-resolution timestamps**: `u64:timestamp_ns` (nanoseconds until year 2554)
- **Large-scale counting**: `u64:total_requests` (>4.3 billion)
- **Network traffic**: `u64:bytes_transferred` (petabytes)
- **Database IDs**: `u64:record_id` (billions of entries)
- **CPU cycle counters**: `u64:cycles`
- **Filesystem operations**: `u64:inode`, `u64:block_count`

**Overflow Behavior**:
```aria
u64:max = 18_446_744_073_709_551_615;
max += 1;  // Wraps to 0

u64:large = 10_000_000_000;  // 10 billion
large *= large;  // Overflows (100 quintillion > 18.4), wraps
```

**Common Patterns**:
- **Large file operations**: Processing files >4GB in chunks
- **High-resolution timing**: Nanosecond precision measurements
- **Network statistics**: Tracking petabytes of traffic
- **Memory address calculations**: Virtual memory management
- **Large-scale counting**: Database record totals, event counts

**Anti-Patterns** (same as u32, magnified scale!):
- ❌ **Underflow in loops**: `while (i >= 0) i -= 1` → infinite!
- ❌ **Negative values**: u64 wraps on underflow (use i64 for negatives)
- ❌ **Ignoring overflow**: Large accumulation without bounds checking
- ❌ **Using for small values**: Wasting 2× memory (use u32 if <4.3B)
- ❌ **Mixing signed/unsigned**: `i64(-100) < u64(1000)` → FALSE!
- ❌ **Subtraction without checks**: `a - b` underflows if b > a

**Best Practices**:
- ✓ **u64 for modern file sizes**: >4GB is standard now
- ✓ **Check before subtracting**: `if (b > a) error(); else a -= b;`
- ✓ **Explicit conversions**: Never implicit signed ↔ unsigned
- ✓ **High-resolution timestamps**: Nanoseconds with u64 (good until 2554)
- ✓ **Document scale assumptions**: "EXPECTS: file_size < 10TB"
- ✓ **Use constants**: `KILOBYTE = 1024`, `MEGABYTE = 1024 * KILOBYTE`
- ✓ **i64 for differences**: Use signed for results that might be negative

**Performance Characteristics**:
- **Operations**: 1-30 cycles on 64-bit CPUs (same as u32)
- **SIMD**: Process 2/4/8 values in parallel (half throughput vs u32)
- **Memory**: 8 bytes (2× u32), half as many values per cache line
- **32-bit CPUs**: u64 requires multiple instructions (slower)

**Common Gotchas**:
- **Underflow wrapping** (most common!): `0 - 1 = 18,446,744,073,709,551,615`
- **Always >= 0 checks**: `if (value < 0)` is always false for u64
- **Overflow in multiplication**: `10B × 10B` overflows u64
- **Mixing with signed**: Implicit conversions create wrong results
- **Division truncates**: `7 / 3 = 2` (use f64 for fractional)
- **Two's complement confusion**: `~5` is NOT `-6` (it's 18,446,744,073,709,551,610)

**Summary**:
u64 is the **modern standard for large non-negative values**: files >4GB, memory addresses (64-bit), nanosecond timestamps (year 2554), large counts (>4.3B), network traffic totals. Same underflow dangers as u32 but at astronomically larger scale. Native performance on 64-bit CPUs, but 2× memory vs u32. Perfect for modern computing where 4.3B is no longer "enough"!

---

## Session 21: u32 Type (February 13, 2026)

**Focus**: 32-bit unsigned integer for non-negative values (sizes, counts, indices)

**Files Created**:
1. **u32.md** (1131 lines)

### u32: 32-Bit Unsigned Integer Type

**Purpose**: Non-negative whole numbers from 0 to 4.3 billion

**Key Concepts**:
- **Range**: 0 to 4,294,967,295 (2³² - 1)
- **Always non-negative**: No sign bit, all 32 bits for magnitude
- **Wrapping arithmetic**: Overflow/underflow defined (modular)
- **Size**: 4 bytes (same as i32)
- **Zero-overhead**: Native machine instructions

**u32 vs i32 Critical Distinction**:
- **Range comparison**: u32 (0 to 4.3B) vs i32 (-2.1B to +2.1B)
- **Negative values**: u32 CANNOT represent negatives (wraps instead)
- **Underflow danger**: `0u32 - 1u32 = 4,294,967,295` (not -1!)
- **When to use u32**: Sizes, counts, indices (semantically never negative)
- **When to use i32**: General integers, values that might be negative
- **The trap**: Subtraction can underflow silently, creating massive values

**u32 vs u64 Size Selection**:
- **u32**: 0 to 4.3 billion, 4 bytes, file sizes <4GB
- **u64**: 0 to 18.4 quintillion, 8 bytes, file sizes >4GB
- **Performance**: Same on 64-bit CPUs (both native)
- **Memory**: u32 uses half the space (better cache, SIMD)
- **Choose u32**: Arrays <4.3B elements, legacy 32-bit, memory-constrained
- **Choose u64**: Modern file sizes, timestamps, future-proofing

**Underflow Wrapping** (CRITICAL BUG SOURCE!):
```aria
u32:count = 5;
count -= 10;  // Wraps to 4,294,967,291 (not -5!)

u32:i = 10;
while (i >= 0) {  // INFINITE LOOP!
    process(i);
    i -= 1;  // At i=0, wraps to 4,294,967,295
}
```

**Common Use Cases**:
- **Array indices/lengths**: `u32:length = array.length()`
- **File sizes (<4GB)**: `u32:file_size = file.size()`
- **IPv4 addresses**: `u32:ip = 0xC0A80101` (192.168.1.1)
- **Hash values**: `u32:hash = fnv1a_hash(data)`
- **Counters**: `u32:request_count = 0`
- **Unix timestamps**: Until year 2106 (vs 2038 for i32)
- **RGB colors**: `u32:color = 0xAARRGGBB`
- **Checksums**: `u32:crc = crc32(buffer)`

**Overflow Behavior**:
```aria
u32:max = 4_294_967_295;
max += 1;  // Wraps to 0

u32:large = 1_000_000;
large *= 10_000;  // Overflows (10B > 4.3B), wraps
```

**Common Patterns**:
- **Array iteration**: Safe ascending loops
- **File metadata**: Sizes, offsets, timestamps (<4GB files)
- **Hash tables**: 32-bit hash functions
- **IPv4 manipulation**: Bit shifting for octets
- **Statistics**: Event counts, byte totals (with overflow checks)
- **Color operations**: ARGB packing/unpacking

**Anti-Patterns** (critical mistakes!):
- ❌ **Decrementing loops**: `while (i >= 0) i -= 1` → infinite!
- ❌ **Negative values**: `u32:temp = -5` → wraps to huge number
- ❌ **Unchecked subtraction**: `balance -= withdrawal` without validation
- ❌ **Mixing signed/unsigned**: `i32(-1) < u32(10)` → FALSE! (implicit conversion)
- ❌ **Using for differences**: `end - start` underflows if start > end
- ❌ **Ignoring overflow**: Accumulation without bounds checking
- ❌ **Files >4GB**: Use u64 for modern file sizes

**Best Practices**:
- ✓ **Check before subtracting**: `if (b > a) error(); else a -= b;`
- ✓ **Use i32 for signed**: If value might be negative, use i32
- ✓ **Explicit conversions**: Never implicit signed ↔ unsigned
- ✓ **u64 for accumulation**: Large sums exceed 4.3B quickly
- ✓ **Document invariants**: "PRECONDITION: value >= 0"
- ✓ **Count up, not down**: Avoid decrementing u32 loops
- ✓ **Overflow detection**: Check `a > MAX - b` before `a + b`

**Performance Characteristics**:
- **Operations**: 1-30 cycles (add/sub 1, mul 3-5, div 10-30)
- **SIMD**: Process 4/8/16 values in parallel (SSE/AVX/AVX-512)
- **Memory**: 4 bytes, 2× more per cache line than u64
- **Same as i32**: No performance difference (both native 32-bit)

**Common Gotchas**:
1. **Underflow to MAX**: Most common u32 bug (0 - 1 = 4,294,967,295)
2. **Always >= 0**: `if (u32_val < 0)` always false
3. **Overflow in multiply**: `100,000 * 100,000 = 10B` (overflows!)
4. **Signed comparison**: `i32(-1) < u32(10)` is FALSE after conversion
5. **Loop trap**: Decrementing to zero then wrapping
6. **Division truncates**: `7u32 / 3u32 = 2` (not 2.33...)
7. **Modulo zero**: Runtime panic

**Type Conversions**:
- `u8/u16 → u32`: Always safe (zero-extension)
- `u32 → u8/u16`: Truncates (data loss if >255/65535)
- `i32 → u32`: Negatives wrap to large positives
- `u32 → i32`: Values >2.1B wrap to negatives
- `u32 → u64`: Always safe (zero-extension)

**Key Takeaways**:
- u32 is for **always-positive integers** up to 4.3 billion
- **Underflow is the #1 bug**: `0 - 1 = MAX`, not error
- **Check subtraction bounds**: Never assume subtraction is safe
- Use **i32 for general integers**, u32 for semantic "never negative"
- Use **u64 for modern file sizes** (>4GB)
- **Decrementing loops are dangerous**: Prefer i32 or count up

---

## Session 20: f32 Type (February 13, 2026)

**Focus**: 32-bit single-precision floating-point type for performance-critical applications

**Files Created**:
1. **f32.md** (1373 lines)

### f32: 32-Bit Floating-Point Type

**Purpose**: Performance-oriented floating-point for graphics, audio, physics, and ML

**Key Concepts**:
- **IEEE 754 single precision**: 1 sign + 8 exponent + 23 mantissa bits
- **Precision**: ~7 decimal digits (vs f64's ~15 digits)
- **Range**: ±1.18×10⁻³⁸ to ±3.4×10³⁸
- **Size**: 4 bytes (half of f64)
- **Hardware**: Native CPU/GPU support, SIMD-friendly

**f32 vs f64 Trade-offs**:
- **SIMD width**: f32 processes 2× values per instruction (4/8/16 vs 2/4/8)
- **Bandwidth**: f32 transfers 2× faster, uses half memory
- **Precision**: f64 has 2× the significant digits (~15 vs ~7)
- **GPU performance**: f32 is standard (f64 often 2-32× slower)
- **When to choose f32**: Graphics, audio, physics, ML, performance-critical
- **When to choose f64**: Scientific computing, high precision, long accumulations

**f32 vs i32 Distinction**:
- **Integer precision limit**: f32 exact only up to ±16,777,216 (2²⁴)
- **Beyond 2²⁴**: Gaps appear, can't represent consecutive integers
- **Example**: 16,777,217 rounds to 16,777,216.0 (precision loss)
- **Use i32 for**: Counting, indexing, exact integers
- **Use f32 for**: Continuous measurements, ratios, trig, fractional values

**Special Values** (same as f64):
- **Zero**: +0.0 and -0.0 (compare equal but behave differently: 1.0/-0.0 = -∞)
- **Infinity**: ±∞ for overflow, division by zero
- **NaN**: Not a Number (domain errors, 0/0, ∞-∞)
- **Critical**: NaN != NaN (only value not equal to itself!)

**IEEE 754 Format**:
```
Bit 31: Sign (0=positive, 1=negative)
Bits 30-23: Exponent (8 bits, biased by 127)
Bits 22-0: Mantissa (23 bits, implicit leading 1)
Value = (-1)^sign × (1 + mantissa/2^23) × 2^(exponent-127)
```

**Performance Characteristics**:
- **SIMD**: SSE (4 floats), AVX (8 floats), AVX-512 (16 floats) in parallel
- **GPU**: Optimized for f32 (standard precision)
- **Cache**: 2× more f32 values fit vs f64
- **Operations**: Addition/multiply ~3-5 cycles, division ~10-20 cycles

**Common Patterns**:
- **Averaging**: Welford's algorithm for numerical stability
- **Distance**: Euclidean (2D/3D), Manhattan
- **Smoothing**: Linear interpolation, exponential moving average
- **Normalization**: [0,1] range mapping, vector normalization
- **Physics**: Velocity/position updates, projectile motion

**Anti-Patterns** (critical):
- ❌ **Money**: Use fix256 for exact decimal arithmetic
- ❌ **Equality**: Never use == (0.1 + 0.2 != 0.3), always epsilon
- ❌ **Loop counters**: Use i32 for exact counting
- ❌ **Ignoring NaN**: Always validate inputs (user_input.is_nan())
- ❌ **Small + Large**: Accumulate small values first (0.1 lost when added to 1e7)
- ❌ **Large IDs**: f32 loses precision for integers > 16,777,216

**Common Gotchas**:
1. **0.1 + 0.2 ≠ 0.3**: Binary can't represent 0.1, 0.2, 0.3 exactly
2. **NaN != NaN**: Only value that doesn't equal itself, use is_nan()
3. **Associativity lost**: (a+b)+c may differ from a+(b+c) due to rounding
4. **Negative zero**: -0.0 compares equal to 0.0 but 1/-0.0 = -∞
5. **Large + small**: 1e7 + 0.1 = 1e7 (0.1 insignificant at that magnitude)
6. **Integer limit**: 16,777,216 + 1 = 16,777,216 (can't represent 16,777,217)
7. **Subnormals**: Very tiny numbers (near zero) can be 10-100× slower

**Best Practices**:
- ✓ Use f32 for **visual/audio precision** (imperceptible rounding)
- ✓ **Always check NaN/Infinity** in user input
- ✓ Use **epsilon for comparisons** (context-appropriate tolerance)
- ✓ Prefer **f64 for accumulations** (many operations)
- ✓ **Validate ranges** before integer conversion
- ✓ **Document precision requirements** (±0.01m tolerance)

**When to Use f32**:
- ✓ Graphics rendering (vertices, colors, transforms)
- ✓ Real-time audio (samples, filters)
- ✓ Physics simulations (games, approximate models)
- ✓ Machine learning (NN weights, activations)
- ✓ Performance-critical float ops
- ✓ Memory/bandwidth constrained
- ✓ SIMD/GPU acceleration needed

**When NOT to Use f32**:
- ✗ Financial calculations (use fix256)
- ✗ Safety-critical systems (use fix256)
- ✗ Need >7 digits precision (use f64)
- ✗ Cross-platform determinism (use tfp32)
- ✗ Exact decimal values (use frac or fix256)
- ✗ Large integers beyond ±16M (use i32/i64)
- ✗ Counting or indexing (use integer types)

**Key Takeaways**:
- f32 is the **performance float** (2× faster SIMD, half memory vs f64)
- Precision limit: **~7 decimal digits, integers up to ±16,777,216**
- **Never use ==** (always epsilon comparison)
- **Always validate NaN/Infinity** in inputs
- Perfect for **graphics, audio, physics, ML** where speed matters
- Use **f64 for science/precision**, **fix256 for money**

---

## Session 1: Fundamental Types (February 12, 2026)

**Focus**: Boolean and character types (foundational single-value types)

**Files Created**:
1. **bool.md** (784 lines)
2. **char.md** (697 lines)

### bool: Boolean Logical Type

**Purpose**: Logical true/false values for conditions, flags, and control flow

**Key Concepts**:
- **Logical semantics** (not numeric)
- Two values: `true` and `false` (NOT 0 and 1)
- **vs int1**: Same 1-byte size, different purpose
  - bool: "Is this condition satisfied?" (logical)
  - int1: "What is this integer value?" (numeric)

**Operators**:
- AND (`&&`): Both must be true, short-circuits
- OR (`||`): At least one true, short-circuits
- NOT (`!`): Inverts truth value
- XOR (`^^`): Exactly one true (toggle, parity)

**Truthiness**:
- **Explicit conversions only** (no implicit)
- Zero is false, non-zero is true (must explicitly check)
- Pointer NULL checks must be explicit
- Safer than C/Python/JavaScript (no surprises)

**Common Patterns**:
- **Flags**: enabled, initialized, ready
- **Guard clauses**: Early exit with validation
- **Predicates**: is_even(), is_prime(), is_valid()
- **Boolean accumulation**: all_valid, any_valid
- **State machines**: Sequential bool flags

**Anti-Patterns**:
- Using integers as booleans (ambiguous intent)
- Magic boolean parameters (success(true) - what does true mean?)
- Boolean blindness (losing error information)
- Redundant comparisons (`if x == true` instead of `if x`)
- Over-complex expressions (use intermediate variables)

**Common Pitfalls**:
- Accidental assignment (Aria prevents at compile time)
- Forgetting short-circuit order (NULL check first!)
- Equality with NaN (use is_nan(), not ==)

**Best Practices**:
- **Naming**: Question form (is_*, has_*, can_*)
- **Positive names**: Avoid double negatives (not_disabled → enabled)
- **Early returns**: Use guards for validation
- **Explicit intent**: Clear what bool represents

**Memory**:
- Logical: 1 bit
- Physical: 1 byte (alignment)
- Bit packing: 8 bools in 1 byte (memory-critical only)

**Comparison with Other Languages**:
- **C**: No bool type (uses int)
- **C++**: Has bool, but allows implicit conversions
- **Python**: bool is subclass of int (True == 1)
- **JavaScript**: Truthy/falsy (implicit conversions everywhere)
- **Aria**: No implicit conversions, explicit NULL checks, safer

### char: Character Type

**Purpose**: Single Unicode characters for text processing (UTF-8 encoded)

**Key Concepts**:
- **Character semantics** (not binary)
- UTF-8 encoded code points (U+0000 to U+10FFFF)
- **Variable length**: 1-4 bytes per character
  - ASCII (0-127): 1 byte
  - Latin Extended (128-2047): 2 bytes
  - Most Unicode (2048-65535): 3 bytes
  - Emoji (65536+): 4 bytes
- **vs byte**: Text characters vs raw binary data

**Literals**:
- Single characters: `'a'`, `'5'`, `'@'`
- Escape sequences: `'\n'`, `'\t'`, `'\''`, `'\\'`
- Unicode escape: `'\u{20AC}'` (€), `'\u{1F60A}'` (😊)

**Character Classification**:
- `is_alpha()`: Letter
- `is_digit()`: Digit character
- `is_alphanumeric()`: Letter or digit
- `is_whitespace()`: Space, tab, newline, etc.
- `is_uppercase()` / `is_lowercase()`: Case check
- `is_ascii()`: Code point < 128

**Case Conversion**:
- `to_uppercase()`: 'a' → 'A'
- `to_lowercase()`: 'Z' → 'z'
- Note: Some characters map to multiple (German 'ß' → "SS")

**Code Points**:
- `to_code_point()`: Get Unicode code point value
- `char.from_code_point(int32)`: Create from code point
- Arithmetic: Explicit conversion required

**Common Patterns**:
- **Character filtering**: Remove non-alphanumeric
- **Character counting**: Count occurrences in string
- **Character replacement**: Replace all instances
- **Case conversion**: String-level uppercase/lowercase
- **Digit conversion**: '5' → 5, 7 → '7'

**Anti-Patterns**:
- Using byte for characters (unclear: 0x41 = 'A' or 65?)
- Assuming ASCII (breaks for international text)
- Character arithmetic without bounds ('z' + 1 = '{')
- Treating char as int (confusing semantics)

**Best Practices**:
- Use char for text, byte for binary
- Leverage built-in classification methods
- Be aware of UTF-8 variable length
- Explicit code point conversions for arithmetic
- Use string for multi-character text
- Descriptive variable names (delimiter vs c)

**Memory**:
- Variable: 1-4 bytes per character (UTF-8)
- String indexing: By character position (not byte position)
- Example: "Aé中😊" = 10 bytes, 4 characters

**Common Character Sets**:
- ASCII Printable: 32-126 (space to tilde)
- Digits: 48-57 ('0' to '9')
- Uppercase: 65-90 ('A' to 'Z')
- Lowercase: 97-122 ('a' to 'z')

---

## Session 16: String Type (February 13, 2026)

**Focus**: UTF-8 string type with interpolation

**Files Created**:
1. **string.md** (935 lines)

### string: String Type

**Purpose**: UTF-8 encoded text sequences for string manipulation

**Key Concepts**:
- **UTF-8 encoding** (variable-length, 1-4 bytes per character)
- **Immutability** (operations return new strings)
- **Explicit length** (byte count, not null-terminated)
- **GC-allocated** (heap, automatic memory management)

**String Literals**:
- Standard: `"Hello, World!"`
- Raw strings: `` `C:\Users\path` `` (no escape processing)
- Escape sequences: `\n`, `\t`, `\"`, `\\`, `\u{XXXX}`

**String Interpolation**:
- Simple: `"Hello, $name!"`
- Expression: `"Result: ${x + y}"`
- Method calls: `"User: ${user.name()}"`
- Escape: `$$` for literal dollar sign

**UTF-8 Encoding**:
- Byte length vs character count (`.length()` returns bytes!)
- Character iteration requires `.char_count()`
- Multi-byte characters (emoji = 4 bytes)
- Invalid UTF-8 sequences cause errors

**Operations**:
- **Comparison**: `==`, `<`, `<=>`, `equals_ignore_case()`
- **Searching**: `contains()`, `index_of()`, `starts_with()`, `ends_with()`
- **Manipulation**: `trim()`, `to_upper()`, `to_lower()`, `replace()`, `repeat()`
- **Splitting**: `split()`, `split_whitespace()`
- **Joining**: `string.join(parts, separator)`
- **Type conversion**: `parse_i32()`, `parse_f64()`, `to_string()`

**Performance Patterns**:
- ❌ Repeated concatenation: `result = result + str` (O(n²))
- ✅ Bulk concatenation: `string.concat_n(parts, count)` (O(n))
- ✅ Interpolation: `"${a}${b}${c}"` (efficient internally)
- String literals in `.rodata` (zero-copy)

**C Interoperability**:
- `to_cstr()`: Convert to null-terminated C string
- `from_cstr()`: Import from C string
- `as_bytes()`: Raw UTF-8 byte access
- Lifetime safety: C pointers invalid after Aria string freed

**Common Patterns**:
- **Path manipulation**: Concatenation, filename extraction
- **String validation**: Empty checks, length limits, content checks
- **CSV parsing**: `split(",")` for fields
- **Log building**: Interpolation for messages

**Anti-Patterns**:
- Repeated concatenation in loops (quadratic performance)
- Assuming 1 character = 1 byte (UTF-8 is variable-length!)
- Slicing carelessly (can cut multi-byte character in half)
- Not using interpolation (manual concatenation is verbose)

**Common Gotchas**:
1. `.length()` returns bytes, not characters (use `.char_count()`)
2. Indexing is byte offset (use `.char_at()` for character indexing)
3. String comparison is binary (different normalization = not equal)
4. Immutability (`.to_upper()` returns new string, doesn't modify)

---

## Session 17: Default Integer Type (February 13, 2026)

**Focus**: 32-bit signed integer (most common integer type)

**Files Created**:
1. **i32.md** (989 lines)

### i32: 32-bit Signed Integer

**Purpose**: General-purpose signed integer for most numeric work

**Key Concepts**:
- **Default integer type** (±2,147,483,647)
- **Two's complement** representation
- **Asymmetric range** (one more negative value than positive)
- **Wrapping overflow** (defined, not undefined behavior)
- **Zero overhead** (native CPU instructions)

**Range**:
- Minimum: -2,147,483,648 (0x80000000)
- Maximum: +2,147,483,647 (0x7FFFFFFF)
- Approximately ±2.1 billion

**Type Comparisons**:
- **vs i8/i16**: Smaller sizes for memory-critical code
- **vs i64**: Larger range for big values, timestamps
- **vs u32**: Unsigned (0 to 4.3B) vs signed (±2.1B)
- **vs tbb32**: Wrapping vs error detection (Year 2038!)

**Literals**:
- Decimal: `42`, `1_000_000`
- Hexadecimal: `0xFF00`, `0xAABBCCDD`
- Binary: `0b1010_1010`, `0b11111111`
- Type suffix: `42i32` (explicit i32 literal)

**Arithmetic Operations**:
- Basic: `+`, `-`, `*`, `/`, `%`
- Integer division truncates toward zero
- Modulo sign matches dividend
- Overflow wraps: MAX + 1 = MIN (defined!)
- Division by zero panics

**Bitwise Operations**:
- Logical: `&` (AND), `|` (OR), `^` (XOR), `~` (NOT)
- Shifts: `<<` (left), `>>` (arithmetic right), `>>>` (logical right)
- Common patterns: Bit flags, masks, permission checks

**Conversions**:
- **Widening** (safe): i8→i32, i16→i32, i32→i64
- **Narrowing** (check!): i64→i32, i32→i16, i32→i8
- **Float**: i32→f32 (may lose precision), f64→i32 (truncates)
- **Unsigned**: i32→u32 (reinterprets bits)

**Common Patterns**:
- **Loop indices**: `for (i: i32 = 0; i < 100; i += 1)`
- **Array indexing**: `items[i]`
- **Counters**: `count += 1`
- **Database IDs**: User IDs, record IDs (up to 2.1B)
- **Bitmask flags**: Permission bits, feature flags

**Anti-Patterns**:
- ❌ i32 timestamps (Year 2038 problem! Use i64 nanoseconds)
- ❌ Using i32 as boolean (use `bool` type)
- ❌ Ignoring overflow in accumulation (use i64 or tbb32)
- ❌ Narrowing without range check (validate before i64→i32)
- ❌ `abs(INT32_MIN)` (panics! 2,147,483,648 doesn't fit)

**Year 2038 Problem**:
- i32 Unix timestamps overflow January 19, 2038, 03:14:08 UTC
- Wraps to December 13, 1901 (silent disaster!)
- Solution: Use i64 nanoseconds or tbb32 with error detection

**Performance**:
- **Zero overhead**: Single CPU instruction for arithmetic
- **Register allocation**: Fits in 32-bit registers
- **Memory**: 4 bytes per value (half of i64)
- **Cache efficiency**: More values per cache line

**C Interoperability**:
- ABI-compatible with C `int32_t`
- Direct FFI (no conversion needed)
- Common for C library interfaces

**Best Practices**:
- ✅ Use i32 for most integer work (±2.1B sufficient)
- ✅ Use i64 for large values, modern timestamps
- ✅ Check range when narrowing i64→i32
- ✅ Use tbb32 for safety-critical overflow detection
- ❌ Don't use i32 for timestamps in new code (Year 2038!)
- ❌ Don't assume no overflow (check or use wider type)

---

## Session 18: Large Integer Type (February 13, 2026)

**Focus**: 64-bit signed integer for large values and modern timestamps

**Files Created**:
1. **i64.md** (991 lines)

### i64: 64-bit Signed Integer

**Purpose**: Large signed integers for values beyond ±2.1 billion

**Key Concepts**:
- **Large range** (±9.2 quintillion, ~4.3 billion times larger than i32)
- **Two's complement** representation
- **Asymmetric range** (one more negative value than positive)
- **Wrapping overflow** (defined, not UB)
- **Zero overhead on 64-bit CPUs** (same performance as i32)

**Range**:
- Minimum: -9,223,372,036,854,775,808
- Maximum: +9,223,372,036,854,775,807
- Approximately ±9.2 × 10^18 (quintillion)

**i64 vs i32 Decision Matrix**:
- **i32**: General use, ±2.1B sufficient, memory-critical
- **i64**: Large values, timestamps, file sizes >2GB, accumulation
- **Memory trade-off**: i64 uses 2× memory of i32
- **Performance**: Same on 64-bit CPUs, 2× slower on 32-bit CPUs

**Nanosecond Timestamps**:
- **Year 2262 solution** (vs i32 Year 2038 problem)
- High precision (billionth of a second)
- Pattern: `i64:timestamp_ns = get_current_time_nanoseconds()`
- Conversions: ns ↔ μs ↔ ms ↔ s
- Event timing, logging, benchmarking

**Large File Sizes**:
- i32 limit: ~2.14 GB (too small!)
- i64 limit: ~8 exabytes (8,192 petabytes)
- Modern files: 4K movies (50-100GB), games (100+GB), databases (TB+)
- Pattern: `i64:file_size = get_file_size(path)`

**Memory Addresses**:
- 64-bit addressing (0 to 2^64 - 1)
- Pointer arithmetic (signed for bidirectional)
- Large array indexing (billions of elements)
- Memory offsets and positions

**Common Patterns**:
- **Accumulating sums**: Prevent i32 overflow
- **High-precision timing**: Nanosecond benchmarks
- **Large datasets**: Billions of records (genomics, astronomy)
- **File operations**: Sizes, offsets for large files

**Anti-Patterns**:
- ❌ Using i64 for small values (wastes 4 bytes per value)
- ❌ Not using i64 for timestamps (Year 2038!)
- ❌ Narrowing i64→i32 without range check
- ❌ Assuming i64 never overflows (it can! Just much larger range)
- ❌ Forgetting i64 suffix for large literals (>i32 max)

**Performance Characteristics**:
- **64-bit CPUs**: i64 = i32 performance (single instruction)
- **32-bit CPUs**: i64 = 2× slower (two instructions)
- **Memory**: 8 bytes (vs 4 bytes for i32)
- **Cache**: 8 i64s per cache line (vs 16 i32s)

**C Interoperability**:
- ABI-compatible with C `int64_t`, `long long`
- Direct FFI (no conversion)
- Pointer casting (pointer to i64 for arithmetic)

**Best Practices**:
- ✅ Use i64 for nanosecond timestamps (good till 2262)
- ✅ Use i64 for file sizes >2GB
- ✅ Use i64 for accumulating many i32 values
- ✅ Check range when narrowing i64→i32
- ❌ Don't use i64 for small values (memory waste)
- ❌ Don't forget i64 suffix for large literals
- ❌ Don't assume i64 never overflows (use tbb64 for safety)

---

## Session 19: Default Floating-Point Type (February 13, 2026)

**Focus**: 64-bit floating point for decimal numbers and scientific calculations

**Files Created**:
1. **f64.md** (915 lines)

### f64: 64-bit Floating Point

**Purpose**: Decimal numbers, measurements, and scientific calculations with high precision

**Key Concepts**:
- **IEEE 754 double precision** (64 bits: 1 sign + 11 exponent + 52 mantissa)
- **Precision**: ~15-16 decimal digits
- **Range**: ±1.8 × 10^308 (enormous!)
- **Approximate**: NOT exact (rounding errors)
- **Default floating-point type** in Aria

**IEEE 754 Representation**:
- Sign bit (1 bit): Positive or negative
- Exponent (11 bits): Biased by 1023, range 0-2047
- Mantissa (52 bits): Fractional part with implicit leading 1
- Value: `(-1)^sign × 1.mantissa × 2^(exponent - 1023)`

**Special Values**:
- **Zero**: +0.0 and -0.0 (both exist, compare equal)
- **Infinity**: ±Infinity (result of overflow or 1/0)
- **NaN**: Not a Number (0/0, √(-1), ∞ - ∞)
- **NaN != NaN**: Critical gotcha! (only != returns true)

**f64 vs f32**:
- f32: 7 digits precision, 4 bytes, faster for large arrays
- f64: 15-16 digits, 8 bytes, default choice (safer)
- Use f32: Graphics colors, audio, memory-critical
- Use f64: General purpose, scientific, accumulation

**f64 vs i64**:
- f64: Decimals, measurements, approximations
- i64: Exact integers, counts, IDs
- Large integer precision loss: Beyond ±2^53, not all ints fit exactly!
- Use i64 for exact values, f64 for measurements

**Literals and Notation**:
- Decimal: `3.14159`, `0.5`, `42.0`
- Scientific: `6.02214076e23`, `1.5e-10`
- Type suffix: `3.14f64` (optional, default for decimals)
- Underscores: `1_234_567.89` (readability)

**Math Functions**:
- Basic: `abs()`, `round()`, `floor()`, `ceil()`, `min()`, `max()`
- Powers: `sqrt()`, `pow()`, `exp()`, `ln()`, `log10()`, `log2()`
- Trig: `sin()`, `cos()`, `tan()`, `asin()`, `acos()`, `atan()`, `atan2()`
- Hyperbolic: `sinh()`, `cosh()`, `tanh()`

**Comparisons**:
- ⚠️ Never use `==` for floats! (rounding errors)
- Use epsilon: `abs(a - b) < 1e-10`
- NaN comparisons: All return false except `!=`
- `NaN == NaN` is **false**! (must use `is_nan()`)

**Common Patterns**:
- Averaging: Sum and divide
- Distance: Euclidean distance formula
- Interpolation: Linear blending (lerp)
- Clamping: Restrict to range

**Anti-Patterns**:
- ❌ Money calculations (use fixed-point! `fix256` or cents as i64)
- ❌ Equality comparisons (use epsilon)
- ❌ Loop counters (use i32, convert inside loop)
- ❌ Ignoring NaN (check with `is_nan()`)
- ❌ Assuming associativity ((a+b)+c may != a+(b+c))

**Precision Gotchas**:
1. `0.1 + 0.2 != 0.3` (binary can't represent 0.1 exactly)
2. `NaN != NaN` (always false!)
3. Large + small loses small (1e16 + 1 = 1e16)
4. Division by zero → Infinity (doesn't panic!)
5. Negative zero exists (-0.0 != 0.0 in 1/x)

**Performance**:
- Hardware accelerated (native CPU instructions)
- Add/sub/mul: 1-4 cycles latency
- Division: ~10-20 cycles
- Transcendental (sin, cos): ~50-200 cycles
- Memory: 8 bytes per value (double f32's 4 bytes)

**Best Practices**:
- ✅ Use f64 for measurements, scientific calculations
- ✅ Use epsilon for equality comparisons
- ✅ Check for NaN/Infinity before using results
- ✅ Use integers for exact counts and IDs
- ❌ Don't use for money (rounding errors accumulate!)
- ❌ Don't compare with == (always use epsilon)
- ❌ Don't use as loop counters (use i32)

---

## Summary Statistics

**Sessions Completed**: 5  
**Total Files**: 6  
**Total Lines**: ~5,315  

**By Category**:
- Fundamental Types: 6 files (~5,315 lines)

**By Session**:
- Session 15: 2 files (~1,481 lines) - February 12, 2026
- Session 16: 1 file (935 lines) - February 13, 2026
- Session 17: 1 file (989 lines) - February 13, 2026
- Session 18: 1 file (991 lines) - February 13, 2026
- Session 19: 1 file (915 lines) - February 13, 2026

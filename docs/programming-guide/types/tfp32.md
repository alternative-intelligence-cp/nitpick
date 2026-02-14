# tfp32 - 32-bit Twisted Floating Point

**Deterministic. Cross-platform bit-exact. No -0. Unified ERR. NO NEGOTIATION.**

## The Problem tfp32 Solves

**IEEE 754 is broken** for safety-critical and deterministic systems:

### IEEE Problem 1: Negative Zero (-0)
```c
// IEEE 754 (C/C++)
float pos_zero = +0.0f;
float neg_zero = -0.0f;
float div1 = 1.0f / pos_zero;  // +Inf
float div2 = 1.0f / neg_zero;  // -Inf (DIFFERENT!)

// Phase calculations can flip sign due to rounding
float angle = some_calc();     // Might be +0.0 or -0.0
float phase = 1.0f / angle;    // +Inf or -Inf (non-deterministic!)
```

**Aria tfp32 solution**:
```aria
tfp32:zero = 0.0tf;            // Only ONE zero (0x00000000)
tfp32:div = 1.0tf / 0.0tf;     // ERR (deterministic error)
```

### IEEE Problem 2: NaN Ambiguity
```c
// IEEE 754 has MULTIPLE NaN states (qNaN, sNaN, payload bits)
float nan1 = 0.0f / 0.0f;      // NaN (indeterminate)
float nan2 = sqrt(-1.0f);      // NaN (domain error)
float result = nan1 + 5.0f;    // Still NaN (silent corruption!)

// Can't distinguish root cause - all NaNs look the same
if (isnan(result)) {
    // Was it division? sqrt? Something else? WHO KNOWS!
}
```

**Aria tfp32 solution**:
```aria
tfp32:err1 = 0.0tf / 0.0tf;    // ERR (indeterminate form)
tfp32:err2 = sqrt(-1.0tf);     // ERR (domain error)
tfp32:result = err1 + 5.0tf;   // ERR (sticky propagation)

if (result == ERR) {
    // Unified error handling - no ambiguity!
}
```

### IEEE Problem 3: Platform-Dependent Rounding
```c
// Same code, DIFFERENT RESULTS on different CPUs
float x = 0.1f + 0.2f;
// x86 FPU:    0.30000001f
// ARM NEON:   0.29999998f
// RISC-V:     ??? (who knows!)

if (x == 0.3f) {
    // Branch might be taken on x86, not taken on ARM!
}
```

**Aria tfp32 solution**:
```aria
tfp32:x = 0.1tf + 0.2tf;       // EXACT SAME BITS on ALL platforms
                               // Game replays work. Distributed consensus works.
                               // Physics simulations reproduce exactly.
```

## What tfp32 IS

**Twisted Floating Point** - deterministic real number approximations built from TBB components:

```
tfp32 = Mantissa × 2^Exponent

Structure: tbb16 exponent + tbb16 mantissa (32 bits total)
```

### tfp32 Memory Layout

```
┌──────────────────┬──────────────────┐
│  tbb16 exponent  │  tbb16 mantissa  │
│    (16 bits)     │    (16 bits)     │
└──────────────────┴──────────────────┘
         ↓                  ↓
    Range/scale      Value/precision
    (-32768 to      (-32768 to +32767)
     +32767)        2's complement
```

**No separate sign bit** - sign lives in mantissa (2's complement).

**ERR detection**: tfp32 is ERR if exponent is TBB ERR OR mantissa is TBB ERR.

## Declaration and Literals

```aria
tfp32:pi = 3.14159tf;              // Literal with 'tf' suffix
tfp32:zero = 0.0tf;                // Unique zero (no -0)
tfp32:negative = -2.5tf;           // Negative (mantissa 2's complement)
tfp32:scientific = 1.23e-10tf;     // Scientific notation
tfp32:error = ERR;                 // Error sentinel
```

## Deterministic Arithmetic

**Software implementation** - all operations in software (no hardware FPU), ensuring **bit-exact** results across **all platforms** (x86, ARM, RISC-V, etc.).

```aria
tfp32:a = 1.5tf;
tfp32:b = 2.7tf;

tfp32:sum = a + b;                 // 4.2tf (bit-exact everywhere)
tfp32:product = a * b;             // 4.05tf (bit-exact everywhere)
tfp32:quotient = a / b;            // ~0.555556tf (bit-exact everywhere)
tfp32:power = a ** 3.0tf;          // 3.375tf (bit-exact everywhere)
```

**Hash the output** - same on x86, ARM, RISC-V, every platform. **This is the point.**

## Unified ERR Propagation

**ERR is sticky** - once you have ERR, it propagates through calculations:

```aria
tfp32:bad = 1.0tf / 0.0tf;         // ERR (division by zero)
tfp32:worse = bad + 5.0tf;         // ERR (sticky)
tfp32:worst = worse * 2.0tf;       // ERR (sticky)
tfp32:terminal = sqrt(worst);      // ERR (sticky)
```

### Common Error Cases

```aria
tfp32:div_zero = 1.0tf / 0.0tf;    // ERR (not Inf)
tfp32:sqrt_neg = sqrt(-1.0tf);     // ERR (not NaN)
tfp32:overflow = 1.0e38tf * 1.0e38tf;  // ERR (exponent overflow)
tfp32:underflow = 1.0e-38tf / 1.0e38tf; // 0.0tf (gradual underflow to zero)
```

**Why ERR instead of Inf/NaN**:
- **Detectable**: Can check `if (x == ERR)` explicitly
- **Halts propagation**: Forces error handling at boundary
- **No silent corruption**: Inf/NaN pretend to be valid numbers
- **Unified handling**: One error state, not multiple (sNaN, qNaN, +Inf, -Inf)

## Precision Trade-Offs

### tfp32 vs IEEE float (32-bit)

| Feature | tfp32 | IEEE float |
|---------|-------|------------|
| Mantissa bits | 16 | 24 |
| Decimal precision | ~4-5 digits | ~7 digits |
| Exponent bits | 16 | 8 |
| Range | 2^(-32768) to 2^(+32767) | ~10^(-38) to 10^(+38) |
| Negative zero | NO (unique zero) | YES (+0 and -0) |
| NaN states | NONE (unified ERR) | MANY (qNaN, sNaN, payload) |
| Infinity | NO (overflow = ERR) | YES (+Inf, -Inf) |
| Cross-platform | 100% bit-exact | Platform-dependent |
| Speed | ~10-50x slower | Hardware accelerated |

**The trade-off**:
- **Less precision** (16-bit mantissa vs 24-bit)
- **BUT: Deterministic** (no platform variance)
- **AND: Safe** (ERR instead of silent NaN/Inf)
- **AND: Larger range** (16-bit exponent vs 8-bit)

## When to Use tfp32

**Use tfp32 when**:
1. **Determinism matters** - same bits on all platforms
2. **Reproducibility required** - game replays, distributed consensus
3. **Safety-critical** - no silent NaN/Inf corruption
4. **Moderate precision sufficient** - 4-5 decimal digits enough
5. **Error detection important** - no Inf/NaN hiding failures

**DON'T use tfp32 when**:
1. **Speed critical** - tfp32 is 10-50x slower than hardware float
2. **High precision needed** - need 7+ decimal digits (use tfp64 or frac64)
3. **IEEE compatibility required** - legacy code, external libraries
4. **-0 semantics needed** - rare, but some algorithms depend on signed zero

## Deterministic Math Functions

**All transcendentals via Taylor series** - software implementation guarantees bit-exact results:

```aria
tfp32:angle = 1.5708tf;            // π/2 (approximate)
tfp32:sine = sin(angle);           // ~1.0tf (via Taylor series)
tfp32:cosine = cos(0.0tf);         // 1.0tf (exact via Taylor)
tfp32:tangent = tan(angle);        // ERR (tan(π/2) undefined)

tfp32:root = sqrt(4.0tf);          // 2.0tf (exact)
tfp32:invalid = sqrt(-1.0tf);      // ERR (domain error)

tfp32:exp_val = exp(1.0tf);        // ~2.71828tf (e via Taylor)
tfp32:log_val = ln(2.71828tf);     // ~1.0tf (natural log)
tfp32:log_neg = ln(-1.0tf);        // ERR (domain error)
```

**Deterministic guarantee**: `sin(1.5708tf)` produces **identical bits** on x86, ARM, RISC-V, every platform.

## Cross-Platform Reproducibility

### Game Replay Example

```aria
// Player inputs recorded during gameplay
struct:ReplayFrame = {
    tfp32:x_pos;
    tfp32:y_pos;
    tfp32:velocity_x;
    tfp32:velocity_y;
    u32:frame;
}

// Physics update (deterministic on all platforms)
func:updatePhysics = tfp32(tfp32:pos, tfp32:vel, tfp32:dt) {
    tfp32:acceleration = -9.8tf;    // Gravity (bit-exact constant)
    tfp32:new_vel = vel + (acceleration * dt);
    tfp32:new_pos = pos + (new_vel * dt);
    
    return {err:NIL, val:new_pos};
};

// Replay on ANY platform produces EXACT same positions
// Mobile (ARM), Desktop (x86), Server (RISC-V) - identical replay
```

### Distributed Consensus Example

```aria
// Multiple servers must agree on calculation result
func:consensusCalc = tfp32(tfp32:input) {
    tfp32:step1 = input * 2.5tf;
    tfp32:step2 = sqrt(step1);
    tfp32:step3 = sin(step2);
    tfp32:result = step3 + 1.0tf;
    
    return {err:NIL, val:result};
};

// Server A (x86):     hash = 0xABCD1234
// Server B (ARM):     hash = 0xABCD1234  (IDENTICAL!)
// Server C (RISC-V):  hash = 0xABCD1234  (CONSENSUS!)
```

## tfp32 vs frac32

**When do you need DETERMINISTIC (known answer) vs EXACT (perfect accuracy)?**

### frac32 (Exact Rationals)
- **Perfect accuracy** - 0.1 + 0.2 = 0.3 exactly
- **No rounding drift** - repeated operations stay exact
- **Rational only** - cannot represent √2, π, e (irrational)
- **Fast** - integer operations (hardware accelerated)
- **Best for**: Money, music, recipes, unit conversion

### tfp32 (Deterministic Approximations)
- **Approximate** - 0.1 + 0.2 ≈ 0.3 (close but not perfect)
- **Rounding error exists** - but SAME rounding on all platforms
- **Any real number** - can approximate √2, π, e
- **Slow** - software implementation (no hardware)
- **Best for**: Physics, graphics, simulations, distributed systems

### Example: Financial Calculation

```aria
// WRONG: Use tfp32 for money
tfp32:price = 19.99tf;
tfp32:tax_rate = 0.08tf;
tfp32:tax = price * tax_rate;          // ~1.5992tf (rounding error!)
tfp32:total = price + tax;             // ~21.5892tf (WRONG for money!)

// RIGHT: Use frac32 for money
frac32:price = {n:1999, d:100};        // $19.99 exactly
frac32:tax_rate = {n:8, d:100};        // 8% exactly
frac32:tax = price * tax_rate;         // {n:15992, d:10000} = $1.5992 exactly
frac32:total = price + tax;            // {n:215992, d:10000} = $21.5992 exactly
```

### Example: Physics Simulation

```aria
// WRONG: Use frac32 for non-rational physics
frac32:velocity = {n:10, d:1};         // 10 m/s (exact)
frac32:gravity = {n:-98, d:10};        // -9.8 m/s² (exact)
frac32:dt = {n:1, d:60};               // 1/60 second (exact)
frac32:accel_term = gravity * dt;      // {n:-98, d:600} (exact but growing denominator!)
// After 1000 frames: denominator explodes, performance dies

// RIGHT: Use tfp32 for physics
tfp32:velocity = 10.0tf;               // 10 m/s (approximate)
tfp32:gravity = -9.8tf;                // -9.8 m/s² (approximate)
tfp32:dt = 0.016667tf;                 // ~1/60 second (approximate)
tfp32:accel_term = gravity * dt;       // Deterministic rounding
// After 1000 frames: Same denominator, bit-exact on all platforms
```

## Safety Integration with Result<T>

tfp32 works seamlessly with Aria's safety system:

```aria
func:safeSqrt = Result<tfp32>(tfp32:value) {
    if (value < 0.0tf) {
        return {err:"Cannot sqrt negative", val:unknown};
    }
    
    tfp32:result = sqrt(value);
    if (result == ERR) {
        return {err:"sqrt overflow", val:unknown};
    }
    
    return {err:NIL, val:result};
};

// Usage with safety
Result<tfp32>:r = safeSqrt(-5.0tf);
if (r.err != NIL) {
    stderr.write("Error: "); r.err.write_to(stderr); stderr.write("\n");
} else {
    tfp32:root = r.val;  // Safe to use (no ERR)
}
```

## Comparison Operations

**Works as expected** with unique zero (no -0 confusion):

```aria
tfp32:a = 5.0tf;
tfp32:b = 5.0tf;
tfp32:c = 3.0tf;

bool:eq = (a == b);                    // true
bool:ne = (a != c);                    // true
bool:lt = (c < a);                     // true
bool:le = (c <= a);                    // true
bool:gt = (a > c);                     // true
bool:ge = (a >= c);                    // true

// Unique zero means no sign confusion
tfp32:pos_zero = 0.0tf;
tfp32:calc_zero = 1.0tf - 1.0tf;
bool:zeros_equal = (pos_zero == calc_zero);  // true (only ONE zero!)
```

## tfp32 with ERR Checks

**Best practice**: Check for ERR before using values in critical paths:

```aria
func:criticalCalc = Result<tfp32>(tfp32:input) {
    tfp32:step1 = input / 2.0tf;
    if (step1 == ERR) {
        return {err:"Division failed", val:unknown};
    }
    
    tfp32:step2 = sqrt(step1);
    if (step2 == ERR) {
        return {err:"sqrt failed (negative?)", val:unknown};
    }
    
    tfp32:result = sin(step2);
    if (result == ERR) {
        return {err:"sin failed (overflow?)", val:unknown};
    }
    
    return {err:NIL, val:result};
};
```

## Typical Use Cases

### 1. Physics Simulation (Deterministic)
```aria
struct:Particle = {
    tfp32:x;
    tfp32:y;
    tfp32:vx;
    tfp32:vy;
}

func:updateParticle = void(Particle#:p, tfp32:dt) {
    tfp32:ax = p.vx * dt;
    tfp32:ay = (p.vy * dt) + (-9.8tf * dt * dt * 0.5tf);  // Gravity
    
    p.x += ax;
    p.y += ay;
    p.vy += -9.8tf * dt;
    // Bit-exact on all platforms - replays work perfectly
};
```

### 2. Graphics (Cross-platform)
```aria
func:interpolate = tfp32(tfp32:start, tfp32:end, tfp32:t) {
    return {err:NIL, val:(start * (1.0tf - t)) + (end * t)};
};

// Animation frame calculation (deterministic on mobile, desktop, web)
tfp32:alpha = interpolate(0.0tf, 1.0tf, 0.5tf).val;  // 0.5tf (bit-exact)
```

### 3. Audio DSP (Sample-exact)
```aria
func:generateSineWave = tfp32(tfp32:time, tfp32:frequency) {
    tfp32:TWO_PI = 6.28318tf;
    tfp32:phase = TWO_PI * frequency * time;
    tfp32:sample = sin(phase);
    
    return {err:NIL, val:sample};
    // Identical waveform on all platforms
};
```

### 4. AI/AGI Cognitive Models (Nikola)
```aria
// Nikola's internal state must be deterministic (no platform variance)
struct:ThoughtState = {
    tfp32:confidence;        // Q9 confidence mapped to tfp32
    tfp32:activation;        // Neural activation level
    tfp32:evidence;          // Accumulated evidence weight
}

func:updateConfidence = void(ThoughtState#:state, tfp32:new_evidence) {
    tfp32:decay = 0.95tf;                      // Confidence decay
    state.confidence *= decay;
    state.confidence += new_evidence;
    
    // CRITICAL: Must be bit-exact on all platforms for Nikola reproducibility
    // Server training (x86), mobile deployment (ARM) - SAME cognition
};
```

## Memory and Performance

### Size
- **4 bytes** per value (32 bits total)
- Cache line: **16 tfp32 values** per 64-byte cache line
- Array of 1000: **4 KB** (compact)

### Speed
- **10-50x slower than hardware float** (software implementation)
- Addition/subtraction: ~50-100 CPU cycles
- Multiplication/division: ~100-200 CPU cycles
- Transcendentals (sin, cos, sqrt): ~500-2000 CPU cycles

**Trade-off**: Slower, but **deterministic and safe**.

### When Speed Matters

**If you need speed AND don't care about determinism**, use hardware IEEE types instead:

```aria
// Fast but platform-dependent (IEEE 754)
flt32:fast = 3.14;                     // Hardware accelerated
flt32:result = fast * 2.0;             // ~1 CPU cycle (FPU)

// Slow but deterministic (Twisted Float)
tfp32:safe = 3.14tf;                   // Software implementation
tfp32:result = safe * 2.0tf;           // ~100 CPU cycles (software)
```

**Aria philosophy**: Give you the choice. Need speed? Use `flt32`. Need safety? Use `tfp32`.

## Common Patterns

### Pattern 1: Safe Division
```aria
func:safeDivide = Result<tfp32>(tfp32:numerator, tfp32:denominator) {
    if (denominator == 0.0tf) {
        return {err:"Division by zero", val:unknown};
    }
    
    tfp32:result = numerator / denominator;
    if (result == ERR) {
        return {err:"Division overflow", val:unknown};
    }
    
    return {err:NIL, val:result};
};
```

### Pattern 2: Deterministic Hash
```aria
func:floatHash = u32(tfp32:value) {
    // Reinterpret tfp32 as raw bits (deterministic)
    u32:bits = value.as_bits();        // Raw 32-bit pattern
    return {err:NIL, val:bits};
    
    // SAME hash on x86, ARM, RISC-V (bit-exact)
};
```

### Pattern 3: Range Clamping
```aria
func:clamp = tfp32(tfp32:value, tfp32:min, tfp32:max) {
    if (value < min) return {err:NIL, val:min};
    if (value > max) return {err:NIL, val:max};
    return {err:NIL, val:value};
};

tfp32:clamped = clamp(-5.0tf, 0.0tf, 10.0tf).val;  // 0.0tf
```

## Summary

**tfp32** is Aria's **32-bit deterministic floating point** type:

✅ **Deterministic** - same bits on x86, ARM, RISC-V, everywhere  
✅ **Unified ERR** - no NaN/Inf confusion, one error state  
✅ **Unique zero** - no +0/-0 nonsense  
✅ **Cross-platform** - game replays, distributed consensus work  
✅ **Safety-critical** - no silent corruption  
✅ **Moderate precision** - 4-5 decimal digits  
✅ **Large range** - 16-bit exponent (wider than IEEE float)  

❌ **Slower** - 10-50x slower than hardware float  
❌ **Less precise** - 16-bit mantissa vs IEEE's 24-bit  

**Use tfp32 when determinism and safety matter more than raw speed.**

**Complement to frac32**: Use frac32 for exact rationals (money), tfp32 for deterministic approximations (physics).

**Philosophy**: Safety and reproducibility over silent failures.

**NOT NEGOTIABLE**: No -0, no NaN, no platform variance. Period.

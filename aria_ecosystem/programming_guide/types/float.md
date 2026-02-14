# float: 32-Bit Non-Deterministic Floating Point

## Overview

**Purpose**: Standard IEEE 754 single-precision floating point for general-purpose approximate arithmetic.

**Characteristics**:
- 32 bits (4 bytes): 1 sign + 8 exponent + 23 mantissa
- Range: ~±1.18×10⁻³⁸ to ±3.4×10³⁸
- Precision: ~7 decimal digits
- **Non-deterministic**: Results may vary by platform, compiler, optimization level
- Fast hardware support (native CPU/GPU operations)

**When to Use**:
- General-purpose graphics, physics, audio
- Performance critical (native hardware)
- Approximate values acceptable
- Platform variations tolerable

**When NOT to Use**:
- Finance, legal, safety-critical (use fix256)
- Cross-platform determinism required (use tfp32)
- Exact decimal values needed (use frac)
- More than ~7 digits precision (use double, tfp64, or fix256)

---

## float vs Deterministic Alternatives

### The Determinism Spectrum

| Type | Determinism | Precision | Speed | Use Case |
|------|-------------|-----------|-------|----------|
| **float** | ❌ Varies | ~7 digits | Fastest | Graphics, audio, local physics |
| **tfp32** | ✅ Guaranteed | ~4 digits | Fast | Network, replay, cross-platform |
| **fix256** | ✅ Bit-exact | ~38 digits | Slow | Finance, safety-critical |
| **frac32** | ✅ Exact rationals | Exact | Medium | Fractions, music intervals |

**The Trade-off**: float is fastest but non-deterministic. Results can differ between:
- x86 vs ARM processors
- Different compiler optimization levels (-O0 vs -O3)
- With/without FMA (fused multiply-add) instructions
- SSE vs AVX SIMD instruction sets

### Example: Non-Determinism in Action

```aria
// Same code, different platforms
float:a = 0.1;
float:b = 0.2;
float:sum = a + b;

// Platform A: sum = 0.30000001192092896 (x86 with SSE)
// Platform B: sum = 0.29999998211860657 (ARM)
// Platform C: sum = 0.30000000000000004 (x86 with FMA)

// All are "correct" under IEEE 754!
// None equals exactly 0.3 (impossible in binary float)
```

**When This Matters**:
- Networked games (physics must match across clients) → use tfp32
- Financial calculations (exact cents required) → use fix256
- Replays/deterministic simulation → use tfp32
- Safety-critical systems → use fix256

**When This Doesn't Matter**:
- Local rendering (only one platform)
- Audio processing (humans can't hear rounding differences)
- Single-player physics (no cross-platform requirement)

---

## IEEE 754 Single Precision Format

### Bit Layout

```
Bit:    31    30-23       22-0
        Sign  Exponent    Mantissa
        1     8 bits      23 bits
```

**Sign Bit** (bit 31):
- 0 = positive
- 1 = negative

**Exponent** (bits 30-23, 8 bits):
- Biased by 127
- Stored value 0: Zero or subnormal
- Stored value 1-254: Normal numbers (actual exponent = stored - 127)
- Stored value 255: Infinity or NaN

**Mantissa** (bits 22-0, 23 bits):
- Implicit leading 1 (except subnormals)
- Represents fractional part

### Value Calculation

**Normal Numbers**:
```
value = (-1)^sign × 1.mantissa × 2^(exponent - 127)
```

**Example**: 12.5 as float
```
12.5 = 1.5625 × 2^3
Sign: 0 (positive)
Exponent: 3 + 127 = 130 = 10000010₂
Mantissa: 0.5625 = 10010000000000000000000₂

Bit pattern: 0 10000010 10010000000000000000000
Hex: 0x41480000
```

### Special Values

**Zero**:
```aria
float:positive_zero = 0.0;     // Exponent=0, Mantissa=0, Sign=0
float:negative_zero = -0.0;    // Exponent=0, Mantissa=0, Sign=1

// Zeros compare equal but have different bit patterns
positive_zero == negative_zero;  // true
1.0 / positive_zero;  // +Infinity
1.0 / negative_zero;  // -Infinity
```

**Infinity**:
```aria
float:pos_inf = float.infinity;      // Exponent=255, Mantissa=0, Sign=0
float:neg_inf = -float.infinity;     // Exponent=255, Mantissa=0, Sign=1

// Arithmetic with infinity
pos_inf + 100.0;  // +Infinity
pos_inf * -1.0;   // -Infinity
pos_inf - pos_inf;  // NaN (undefined)
```

**NaN (Not a Number)**:
```aria
float:nan = float.nan;  // Exponent=255, Mantissa≠0

// NaN generation
float:a = 0.0 / 0.0;           // NaN
float:b = float.infinity - float.infinity;  // NaN
float:c = (-1.0).sqrt();       // NaN (negative square root)

// NaN comparison (NaN != anything, even itself!)
nan == nan;  // false (!!)
nan != nan;  // true
nan < 5.0;   // false
nan >= 5.0;  // false

// NaN checking
bool:is_nan = value.is_nan();
```

---

## Precision and Rounding

### Decimal Precision

**~7 Decimal Digits** of precision:
```aria
float:precise = 1234567.0;     // Exactly representable
float:imprecise = 12345678.0;  // Rounded (8th digit lost)

// Small changes invisible
float:x = 16777216.0;  // 2^24 (largest consecutive integer)
float:y = x + 1.0;
x == y;  // true! (1.0 too small to represent at this magnitude)
```

**Relative Precision** (not absolute):
```aria
// Large numbers lose fine detail
float:large = 1000000.0;
float:large_plus = large + 0.1;  // Rounds to 1000000.0 (0.1 lost!)

// Small numbers retain detail
float:small = 0.001;
float:small_plus = small + 0.0001;  // = 0.0011 (retained)
```

### Rounding Modes

**Default: Round to Nearest, Ties to Even**
```aria
float:a = 0.5;  // Exactly representable
float:b = 1.5;  // Exactly representable

// Halfway cases round to even
float:round_a = a.round();  // 0.0 (ties to even)
float:round_b = b.round();  // 2.0 (ties to even)

// Most operations round to nearest
float:third = 1.0 / 3.0;  // 0.33333334... (rounded)
```

**Other Modes** (platform-dependent):
- Round toward zero (truncate)
- Round toward +Infinity (ceiling)
- Round toward -Infinity (floor)
- Round to nearest (even)

**Rounding Errors Accumulate**:
```aria
float:sum = 0.0;
loop (1000 times) {
    sum += 0.1;  // Each addition rounds
}
// Expected: 100.0
// Actual: ~99.99906 or ~100.00095 (varies by platform)
```

---

## Common Pitfalls

### 1. Decimal Numbers Are Approximations

```aria
// BAD: Expecting exact decimal values
float:x = 0.1;  // Actually 0.100000001490116119384765625
float:y = 0.2;  // Actually 0.200000002980232238769531250
float:z = x + y;
if (z == 0.3) {  // NEVER TRUE! 0.3 not representable exactly
    log.write("This never prints\n");
}

// GOOD: Use epsilon for comparison
float:epsilon = 0.00001;
if (abs(z - 0.3) < epsilon) {
    log.write("Approximately equal\n");
}

// BETTER: Use exact types for exact decimals
fix256:exact_x = fix256.from_decimal("0.1");
fix256:exact_y = fix256.from_decimal("0.2");
fix256:exact_z = exact_x + exact_y;  // Exactly 0.3
```

### 2. Loss of Precision in Large Sums

```aria
// BAD: Adding small to large repeatedly
float:total = 10000000.0;
loop (1000 times) {
    total += 1.0;  // Small increment lost in rounding!
}
// Expected: 10001000.0
// Actual: ~10000000.0 (increments too small to matter)

// GOOD: Accumulate small values first
float:small_sum = 0.0;
loop (1000 times) {
    small_sum += 1.0;  // = 1000.0 exactly
}
float:total = 10000000.0 + small_sum;  // = 10001000.0
```

### 3. Catastrophic Cancellation

```aria
// BAD: Subtracting nearly equal values
float:a = 1.0000001;
float:b = 1.0000000;
float:diff = a - b;  // Loses relative precision dramatically

// Most significant digits cancel out, leaving only rounding error
// Result has ~1 digit precision instead of ~7
```

### 4. Associativity Lost

```aria
// Arithmetic is NOT associative with floats
float:a = 1e20;
float:b = -1e20;
float:c = 1.0;

float:result1 = (a + b) + c;  // (0.0) + 1.0 = 1.0
float:result2 = a + (b + c);  // 1e20 + (-1e20 + 1.0) = 0.0 (likely!)

// result1 != result2 (order matters!)
```

### 5. NaN Propagation

```aria
// NaN infects all operations
float:value = get_user_input();  // Could be NaN
float:result = value * 2.0 + 100.0;  // NaN if value is NaN

// ALWAYS check for NaN in untrusted inputs
if (value.is_nan()) {
    log.write("ERROR: Invalid input\n");
    value = 0.0;  // Default
}
```

---

## Performance Advantages

### Native Hardware Support

**CPU Operations** (single-cycle on modern CPUs):
```aria
float:a = 1.5;
float:b = 2.5;
float:sum = a + b;      // ~1 cycle
float:product = a * b;  // ~3-5 cycles
float:quotient = a / b; // ~10-20 cycles (still fast)
```

**SIMD Parallelism**:
```aria
// Process 4 floats in parallel (SSE)
// Process 8 floats in parallel (AVX)
// Process 16 floats in parallel (AVX-512)

simd<float, 4>:vec_a = {1.0, 2.0, 3.0, 4.0};
simd<float, 4>:vec_b = {5.0, 6.0, 7.0, 8.0};
simd<float, 4>:vec_sum = vec_a + vec_b;
// All 4 additions in one instruction!
```

**GPU Acceleration**:
```aria
// GPUs have thousands of float ALUs
// Can process millions of floats per frame
void:process_pixels(float[]:pixels) @gpu {
    parallel loop (pixels.length:i) {
        pixels[i] = pixels[i] * brightness;  // Hardware accelerated
    }
}
```

### When float Wins

**Graphics**: Rendering millions of triangles
```aria
// Transform vertex (matrix multiply, ~20 float ops)
// Need ~60 FPS × millions of vertices
// Only float is fast enough
float:transform_vertex(float:x, float:y, float:z, float[16]:matrix) -> vec3 {
    // Hardware matrix multiply
}
```

**Audio**: Processing sample streams
```aria
// 48kHz audio = 48,000 samples/second
// Real-time processing requires float speed
float:apply_filter(float:sample, float:coefficient) -> float {
    return sample * coefficient;  // Must be fast
}
```

**Physics**: Real-time simulation
```aria
// Game physics (30-60 updates/second)
// Approximate is fine, speed critical
float:update_velocity(float:velocity, float:acceleration, float:dt) -> float {
    return velocity + acceleration * dt;
}
```

---

## Conversion and Interop

### To/From Integers

```aria
// Float to int (truncates toward zero)
float:f = 3.7;
int32:i = f.to_int32();  // 3 (decimal part discarded)

float:neg = -3.7;
int32:neg_i = neg.to_int32();  // -3 (toward zero)

// Int to float (may round for large values)
int32:large = 16777217;  // 2^24 + 1
float:f = large.to_float();  // 16777216.0 (rounded down!)
```

### To/From Deterministic Types

```aria
// float → tfp32 (loses guarantees)
float:f = 1.5;
tfp32:t = tfp32.from_float(f);  // Approximate conversion

// tfp32 → float (safe)
tfp32:deterministic = tfp32.from_string("1.5");
float:f = deterministic.to_float();  // Use for hardware acceleration

// float → fix256 (for exact storage)
float:approximate = 123.456;
fix256:exact = fix256.from_float(approximate);  // Convert to exact

// fix256 → float (for fast computation)
fix256:exact_value = fix256.from_decimal("123.456");
float:fast = exact_value.to_float();  // Use for computation
```

---

## Common Patterns

### Epsilon Comparison

```aria
// Compare floats with tolerance
bool:approximately_equal(float:a, float:b, float:epsilon = 0.00001) -> bool {
    return abs(a - b) < epsilon;
}

// Usage
float:computed = expensive_calculation();
if (approximately_equal(computed, expected, 0.001)) {
    log.write("Close enough\n");
}
```

### Safe Division

```aria
// Avoid division by zero
float:safe_divide(float:numerator, float:denominator) -> float {
    if (abs(denominator) < 0.000001) {
        log.write("WARNING: Division by near-zero\n");
        return float.infinity;  // Or 0.0, or ERR
    }
    return numerator / denominator;
}
```

### Kahan Summation (Compensated Sum)

```aria
// Reduce rounding error in large sums
float:kahan_sum(float[]:values) -> float {
    float:sum = 0.0;
    float:compensation = 0.0;  // Running error
    
    loop (values.length:i) {
        float:y = values[i] - compensation;
        float:t = sum + y;
        compensation = (t - sum) - y;  // Recover lost low-order bits
        sum = t;
    }
    
    return sum;
}
```

### Clamping

```aria
// Clamp value to range
float:clamp(float:value, float:min_val, float:max_val) -> float {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

// Usage (e.g., color channels)
float:red = clamp(computed_red, 0.0, 1.0);
```

---

## Use Cases by Domain

### Graphics and Rendering

```aria
// Vertex positions (approximate is fine)
struct:Vertex {
    float:x, y, z;      // Position
    float:nx, ny, nz;   // Normal
    float:u, v;         // Texture coords
}

// Color values (0-1 range)
struct:Color {
    float:r, g, b, a;  // Fast blending
}

// Transformation matrices
float[16]:view_matrix;
float[16]:projection_matrix;
```

### Audio Processing

```aria
// Audio samples (-1.0 to +1.0)
float[]:audio_buffer;

// Apply gain
void:apply_gain(float[]:samples, float:gain) {
    loop (samples.length:i) {
        samples[i] *= gain;
    }
}

// Mix channels
float:mix(float:sample_a, float:sample_b, float:ratio) -> float {
    return sample_a * (1.0 - ratio) + sample_b * ratio;
}
```

### Physics Simulation

```aria
// Particle position and velocity
struct:Particle {
    float:x, y, z;       // Position
    float:vx, vy, vz;    // Velocity
    float:mass;
}

// Update physics (approximate is fine)
void:update_particle(Particle:p, float:dt) {
    p.vx += p.force_x / p.mass * dt;
    p.x += p.vx * dt;
}
```

---

## Anti-Patterns

### Using float for Money

```aria
// BAD: Cents as float
float:price = 19.99;
float:tax = price * 0.08;  // 1.5992000... (rounding error)
float:total = price + tax;  // 21.5892000...

// GOOD: Use fix256 for money
fix256:exact_price = fix256.from_decimal("19.99");
fix256:exact_tax = exact_price * fix256.from_decimal("0.08");
fix256:exact_total = exact_price + exact_tax;  // Exactly 21.59
```

### Expecting Determinism Across Platforms

```aria
// BAD: Relying on exact values in networked game
// Client A (x86)
float:position = compute_physics(dt);

// Client B (ARM)  
float:position = compute_physics(dt);

// Positions diverge over time! Desync!

// GOOD: Use tfp32 for deterministic physics
tfp32:position = compute_deterministic_physics(dt);
// Identical on all platforms
```

### Loop Counter

```aria
// BAD: Float as loop counter
float:i = 0.0;
loop till i >= 100.0 {
    process(i);
    i += 0.1;  // Rounding error accumulates!
}
// May not stop at exactly 100.0

// GOOD: Integer loop counter
loop (1000 times:i) {
    float:value = i * 0.1;
    process(value);
}
```

### Equality Comparison

```aria
// BAD: Exact equality
if (computed_value == expected_value) {
    // Rarely true due to rounding
}

// GOOD: Epsilon comparison
if (abs(computed_value - expected_value) < 0.00001) {
    // True within tolerance
}
```

---

## Summary

**float is for**:
- ✅ Graphics, rendering (speed critical)
- ✅ Audio processing (real-time)
- ✅ Physics simulation (approximate OK)
- ✅ Hardware acceleration (GPU/SIMD)
- ✅ Local computations (single platform)

**float is NOT for**:
- ❌ Finance, money (use fix256)
- ❌ Cross-platform determinism (use tfp32)
- ❌ Exact decimal values (use frac or fix256)
- ❌ Loop counters (use int)
- ❌ Equality comparisons (use epsilon or exact types)

**Key Principles**:
1. **~7 decimal digits** precision
2. **Non-deterministic** (varies by platform)
3. **Approximate** (0.1 + 0.2 ≠ 0.3 exactly)
4. **Fast** (native hardware)
5. **Use epsilon** for comparisons
6. **Know when to use exact types instead**

**Remember**: float is a tool for **fast approximation**, not exact computation. Choose the right type for your needs.

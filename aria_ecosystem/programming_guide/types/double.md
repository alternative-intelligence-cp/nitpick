# double: 64-Bit Non-Deterministic Floating Point

## Overview

**Purpose**: IEEE 754 double-precision floating point for high-precision approximate arithmetic.

**Characteristics**:
- 64 bits (8 bytes): 1 sign + 11 exponent + 52 mantissa
- Range: ~±2.23×10⁻³⁰⁸ to ±1.80×10³⁰⁸
- Precision: ~15-16 decimal digits
- **Non-deterministic**: Results may vary by platform (like float)
- Fast hardware support (native CPU operations, some GPU support)

**When to Use**:
- Scientific computing requiring precision beyond float
- Intermediate calculations to reduce accumulation error
- General-purpose programming (default in many languages)
- Large value ranges needed

**When NOT to Use**:
- Finance, legal, safety-critical (use fix256)
- Cross-platform determinism required (use tfp64)
- Exact decimal values needed (use frac)
- Memory/bandwidth constrained (use float if precision allows)

---

## double vs float vs Deterministic Alternatives

### The Precision Spectrum

| Type | Bytes | Precision | Range | Speed | Determinism |
|------|-------|-----------|-------|-------|-------------|
| **float** | 4 | ~7 digits | ±3.4×10³⁸ | Fastest | ❌ |
| **double** | 8 | ~15 digits | ±1.8×10³⁰⁸ | Fast | ❌ |
| **tfp64** | 8 | ~14 digits | ±1.0×10⁷⁶ | Fast | ✅ |
| **fix256** | 32 | ~38 digits | Fixed | Slow | ✅ |

**The Trade-off**: double provides more precision than float but still non-deterministic. Results can differ between:
- x86 vs ARM processors (different rounding in extended precision)
- Compiler optimization levels
- FMA (fused multiply-add) vs separate operations
- Library implementations (sin, cos, exp, etc.)

### When double vs float

**Use double when**:
- Need more than ~7 digits precision
- Accumulating many float operations (error reduction)
- Scientific computing (measurements, simulations)
- General-purpose programming (safer default)

**Use float when**:
- 7 digits sufficient (graphics, audio)
- Memory bandwidth critical (large arrays)
- SIMD/GPU throughput matters (process 2× more floats)
- Embedded systems (limited precision hardware)

### Example: Accumulated Precision

```aria
// Float accumulation error
float:sum_f = 0.0;
loop (10000000 times) {  // 10 million iterations
    sum_f += 0.1;
}
// Expected: 1000000.0
// Actual: ~999838.0 (significant error!)

// Double accumulation error
double:sum_d = 0.0;
loop (10000000 times) {
    sum_d += 0.1;
}
// Expected: 1000000.0
// Actual: ~999999.9999998 (much closer)

// Still not exact! For exact, use fix256
fix256:sum_exact = fix256.zero();
loop (10000000 times) {
    sum_exact += fix256.from_decimal("0.1");
}
// Exactly 1000000.0
```

---

## IEEE 754 Double Precision Format

### Bit Layout

```
Bit:    63    62-52         51-0
        Sign  Exponent      Mantissa
        1     11 bits       52 bits
```

**Sign Bit** (bit 63):
- 0 = positive
- 1 = negative

**Exponent** (bits 62-52, 11 bits):
- Biased by 1023
- Stored value 0: Zero or subnormal
- Stored value 1-2046: Normal numbers (actual exponent = stored - 1023)
- Stored value 2047: Infinity or NaN

**Mantissa** (bits 51-0, 52 bits):
- Implicit leading 1 (except subnormals)
- Represents fractional part
- 52 bits ≈ 15.95 decimal digits

### Value Calculation

**Normal Numbers**:
```
value = (-1)^sign × 1.mantissa × 2^(exponent - 1023)
```

**Example**: 123.456 as double
```
123.456 = 1.92891... × 2^6
Sign: 0 (positive)
Exponent: 6 + 1023 = 1029 = 10000000101₂
Mantissa: 0.92891...

Bit pattern: 0 10000000101 1110110111010010111100011010101000...
Hex: 0x405EDD2F1A9FBE77
```

### Special Values

**Zero**:
```aria
double:positive_zero = 0.0;    // All bits 0
double:negative_zero = -0.0;   // Sign bit 1, rest 0

// Behave identically in most operations
positive_zero == negative_zero;  // true

// But differ in division
1.0 / positive_zero;  // +Infinity
1.0 / negative_zero;  // -Infinity
```

**Infinity**:
```aria
double:pos_inf = double.infinity;
double:neg_inf = -double.infinity;

// Arithmetic
pos_inf + 1000000.0;  // +Infinity
pos_inf * 2.0;        // +Infinity
pos_inf / pos_inf;    // NaN (undefined)
1.0 / 0.0;            // +Infinity

// Comparison
pos_inf > 1e308;  // true (infinity > all finite values)
```

**NaN (Not a Number)**:
```aria
double:nan = double.nan;

// NaN generation
double:a = 0.0 / 0.0;
double:b = double.infinity - double.infinity;
double:c = (-1.0).sqrt();

// NaN comparison (NEVER equal to anything)
nan == nan;     // false
nan < 5.0;      // false
nan >= 5.0;     // false
nan != nan;     // true (only reliable way to check!)

// Proper NaN checking
bool:is_nan = value.is_nan();
```

---

## Precision and Rounding

### Decimal Precision

**~15-16 Decimal Digits**:
```aria
double:precise = 123456789012345.0;   // Exactly representable
double:imprecise = 1234567890123456.0; // Rounded (16th digit lost)

// Consecutive integers exactly representable up to 2^53
double:max_safe_int = 9007199254740992.0;  // 2^53
double:next = max_safe_int + 1.0;          // Exactly 2^53 + 1
double:gap = max_safe_int + 1.5;           // Rounds (can't represent .5 at this scale)
```

**Relative Precision** (not absolute):
```aria
// Large numbers lose fine detail
double:large = 1e15;
double:large_plus = large + 0.1;  // Rounds to 1e15 (0.1 insignificant)

// Small numbers retain detail  
double:small = 1e-15;
double:small_plus = small + 1e-16;  // = 1.1e-15 (retained)
```

### Guard Digits and Extended Precision

**x86 Extended Precision** (80-bit):
- x87 FPU uses 80-bit internally (64-bit mantissa)
- Can cause differences between:
  - Register values (80-bit)
  - Memory values (64-bit)
- Optimizations affect which is used

```aria
// Example: x86 extended precision differences
double:a = 1.0 / 3.0;  // May compute in 80-bit, store as 64-bit

// If kept in register (80-bit):
double:b = a * 3.0;  // May be exactly 1.0

// If spilled to memory (64-bit):
double:c = a * 3.0;  // May be 0.9999999999999999

// b != c on x86! (platform-dependent)
```

---

## Common Pitfalls (Same as float, Different Scale)

### 1. Decimal Approximations

```aria
// Still not exact (better than float, but not perfect)
double:x = 0.1;  // Actually 0.1000000000000000055511151231257827...
double:y = 0.2;  // Actually 0.2000000000000000111022302462515654...
double:sum = x + y;

if (sum == 0.3) {  // STILL NEVER TRUE!
    log.write("This still never prints\n");
}

// Use epsilon (smaller than float's epsilon)
double:epsilon = 1e-10;
if (abs(sum - 0.3) < epsilon) {
    log.write("Approximately equal\n");
}

// Or use exact types
fix256:exact = fix256.from_decimal("0.1") + fix256.from_decimal("0.2");
// Exactly 0.3
```

### 2. Catastrophic Cancellation

```aria
// Subtracting nearly equal values destroys precision
double:a = 1.0000000000000001;
double:b = 1.0000000000000000;
double:diff = a - b;

// Lost ~15 digits of precision
// Only ~1 significant digit remains
```

**Mitigation**: Reformulate equations to avoid cancellation
```aria
// BAD: Quadratic formula (b^2 - 4ac can cancel)
double:x = (-b + sqrt(b*b - 4*a*c)) / (2*a);

// BETTER: Alternative form when b > 0
double:x = (-2*c) / (b + sqrt(b*b - 4*a*c));
```

### 3. Associativity Still Lost

```aria
// Arithmetic is NOT associative (even with double)
double:a = 1e200;
double:b = -1e200;
double:c = 1.0;

double:result1 = (a + b) + c;  // 0.0 + 1.0 = 1.0
double:result2 = a + (b + c);  // 1e200 + (-1e200 + 1.0) ≈ 0.0

// Still not associative!
```

### 4. Integer Conversion Limits

```aria
// Integers larger than 2^53 lose precision
int64:big = 9007199254740993;  // 2^53 + 1
double:d = big.to_double();    // 9007199254740992.0 (rounded!)
int64:back = d.to_int64();     // 9007199254740992 (information lost)

big != back;  // true (precision lost)
```

---

## Performance Characteristics

### CPU Performance

**Native Hardware** (modern x86/ARM):
```aria
double:a = 1.5;
double:b = 2.5;

double:sum = a + b;      // ~1 cycle (same as float)
double:product = a * b;  // ~3-5 cycles
double:quotient = a / b; // ~10-20 cycles
double:root = a.sqrt();  // ~10-15 cycles
```

**SIMD Throughput** (half of float):
```aria
// SSE: Process 2 doubles (vs 4 floats)
// AVX: Process 4 doubles (vs 8 floats)
// AVX-512: Process 8 doubles (vs 16 floats)

simd<double, 4>:vec_a = {1.0, 2.0, 3.0, 4.0};
simd<double, 4>:vec_b = {5.0, 6.0, 7.0, 8.0};
simd<double, 4>:vec_sum = vec_a + vec_b;
// Still fast, but half the throughput of float
```

### Memory and Bandwidth

**Size**: 8 bytes (2× float)
- Cache: Fit half as many doubles as floats
- Bandwidth: Transfer half the data rate
- Memory: Arrays take 2× space

```aria
// 1 million floats: 4 MB
float[1000000]:float_array;

// 1 million doubles: 8 MB
double[1000000]:double_array;

// Bandwidth impact on large data processing
```

---

## When double is the Right Choice

### Scientific Computing

```aria
// Physical constants (need precision)
double:speed_of_light = 299792458.0;  // m/s
double:gravitational_constant = 6.67430e-11;  // N⋅m²/kg²
double:planck_constant = 6.62607015e-34;  // J⋅Hz⁻¹

// Calculations with many operations
double:orbital_period(double:semi_major_axis, double:central_mass) -> double {
    double:numerator = 4.0 * pi * pi * semi_major_axis * semi_major_axis * semi_major_axis;
    double:denominator = gravitational_constant * central_mass;
    return sqrt(numerator / denominator);
}
```

### Statistics and Data Analysis

```aria
// Mean calculation (accumulation)
double:mean(double[]:values) -> double {
    double:sum = 0.0;
    loop (values.length:i) {
        sum += values[i];
    }
    return sum / values.length.to_double();
}

// Standard deviation (needs precision)
double:std_dev(double[]:values, double:mean) -> double {
    double:sum_sq_diff = 0.0;
    loop (values.length:i) {
        double:diff = values[i] - mean;
        sum_sq_diff += diff * diff;
    }
    return sqrt(sum_sq_diff / values.length.to_double());
}
```

### Numerical Integration

```aria
// Simpson's rule integration
double:integrate_simpsons(double:a, double:b, int32:n) -> double {
    double:h = (b - a) / n.to_double();
    double:sum = f(a) + f(b);
    
    loop (n-1 times:i) {
        double:x = a + (i+1).to_double() * h;
        sum += (i % 2 == 0 ? 2.0 : 4.0) * f(x);
    }
    
    return (h / 3.0) * sum;
}
```

---

## Conversion and Interop

### double ↔ float

```aria
// double → float (precision loss)
double:d = 3.141592653589793;
float:f = d.to_float();  // 3.14159274... (loses ~8 digits)

// float → double (exact conversion)
float:f = 3.14159274;
double:d = f.to_double();  // Exactly 3.14159274... (no loss)
```

### double ↔ Deterministic Types

```aria
// double → tfp64 (loses determinism guarantee)
double:d = 1.5;
tfp64:t = tfp64.from_double(d);

// tfp64 → double (safe)
tfp64:deterministic = tfp64.from_string("1.5");
double:d = deterministic.to_double();

// double → fix256 (for exact storage)
double:approximate = 123.456789012345;
fix256:exact = fix256.from_double(approximate);

// fix256 → double (may lose precision if fix256 > 15 digits)
fix256:exact_value = fix256.from_decimal("123.456789012345678901234");
double:d = exact_value.to_double();  // Rounds to ~15 digits
```

---

## Common Patterns

### Compensated Summation (Kahan Algorithm)

```aria
// Reduce rounding error in large sums
double:kahan_sum(double[]:values) -> double {
    double:sum = 0.0;
    double:c = 0.0;  // Running compensation
    
    loop (values.length:i) {
        double:y = values[i] - c;
        double:t = sum + y;
        c = (t - sum) - y;  // Algebraically zero, but captures rounding error
        sum = t;
    }
    
    return sum;
}

// Dramatically improves accuracy for large arrays
```

### Relative Epsilon Comparison

```aria
// Scale epsilon by magnitude
bool:approximately_equal_relative(double:a, double:b, double:rel_epsilon = 1e-9) -> double {
    double:diff = abs(a - b);
    double:larger = max(abs(a), abs(b));
    
    return diff <= larger * rel_epsilon;
}

// Works for both large and small values
```

### Safe Math Functions

```aria
// Avoid domain errors
double:safe_sqrt(double:x) -> double {
    if (x < 0.0) {
        log.write("ERROR: Negative square root\n");
        return double.nan;
    }
    return x.sqrt();
}

double:safe_log(double:x) -> double {
    if (x <= 0.0) {
        log.write("ERROR: Log of non-positive\n");
        return double.nan;
    }
    return x.log();
}
```

---

## Use Cases by Domain

### Scientific Simulation

```aria
// N-body gravity simulation
struct:Body {
    double:x, y, z;      // Position
    double:vx, vy, vz;   // Velocity
    double:mass;
}

void:update_gravity(Body[]:bodies, double:dt) {
    loop (bodies.length:i) {
        double:fx = 0.0, fy = 0.0, fz = 0.0;
        
        loop (bodies.length:j) {
            if (i == j) continue;
            
            double:dx = bodies[j].x - bodies[i].x;
            double:dy = bodies[j].y - bodies[i].y;
            double:dz = bodies[j].z - bodies[i].z;
            
            double:r2 = dx*dx + dy*dy + dz*dz;
            double:r = sqrt(r2);
            double:f = G * bodies[i].mass * bodies[j].mass / r2;
            
            fx += f * dx / r;
            fy += f * dy / r;
            fz += f * dz / r;
        }
        
        bodies[i].vx += fx / bodies[i].mass * dt;
        bodies[i].vy += fy / bodies[i].mass * dt;
        bodies[i].vz += fz / bodies[i].mass * dt;
    }
}
```

### Machine Learning (Pre-Training)

```aria
// Neural network layer (float for training speed, double for validation)
void:forward_pass(double[]:inputs, double[]:weights, double[]:outputs) {
    loop (outputs.length:i) {
        double:sum = 0.0;
        loop (inputs.length:j) {
            sum += inputs[j] * weights[i * inputs.length + j];
        }
        outputs[i] = activation_function(sum);
    }
}
```

### Geographic Calculations

```aria
// Latitude/longitude (need precision for cm-level accuracy)
struct:GeoPoint {
    double:latitude;   // degrees (-90 to +90)
    double:longitude;  // degrees (-180 to +180)
}

// Haversine distance (great circle distance)
double:distance_km(GeoPoint:p1, GeoPoint:p2) -> double {
    double:earth_radius = 6371.0;  // km
    
    double:lat1 = p1.latitude * pi / 180.0;
    double:lat2 = p2.latitude * pi / 180.0;
    double:dlat = lat2 - lat1;
    double:dlon = (p2.longitude - p1.longitude) * pi / 180.0;
    
    double:a = sin(dlat/2.0) * sin(dlat/2.0) +
               cos(lat1) * cos(lat2) * sin(dlon/2.0) * sin(dlon/2.0);
    double:c = 2.0 * atan2(sqrt(a), sqrt(1.0-a));
    
    return earth_radius * c;
}
```

---

## Anti-Patterns

### Using double for Everything

```aria
// BAD: Overkill for graphics
struct:Vertex {
    double:x, y, z;  // Wastes memory, slows rendering
}

// GOOD: float sufficient for graphics
struct:Vertex {
    float:x, y, z;  // Fast, plenty of precision
}
```

### Currency Calculations

```aria
// BAD: Still not exact for money
double:price = 19.99;
double:quantity = 3.0;
double:total = price * quantity;  // 59.970000000000006 (not exact!)

// GOOD: Use fix256
fix256:price = fix256.from_decimal("19.99");
fix256:quantity = fix256.from_integer(3);
fix256:total = price * quantity;  // Exactly 59.97
```

### Expecting Cross-Platform Identical Results

```aria
// BAD: Networked simulation assuming identical doubles
// Server (x86)
double:position = compute_physics(dt);
send_to_clients(position);

// Client (ARM)
double:local_position = compute_physics(dt);
// May differ slightly! Causes desync!

// GOOD: Use tfp64 for deterministic networking
tfp64:position = compute_deterministic_physics(dt);
```

---

## Summary

**double is for**:
- ✅ Scientific computing (precision beyond float)
- ✅ Statistics and data analysis
- ✅ Intermediate calculations (reduce error)
- ✅ General-purpose programming (safer default)
- ✅ Large value ranges (±10³⁰⁸)

**double is NOT for**:
- ❌ Finance, money (use fix256)
- ❌ Cross-platform determinism (use tfp64)
- ❌ Exact decimal values (use frac or fix256)
- ❌ Memory-constrained large arrays (use float if precision allows)
- ❌ Maximum SIMD throughput (use float for 2× throughput)

**Key Principles**:
1. **~15 decimal digits** precision (vs float's ~7)
2. **Non-deterministic** (varies by platform, like float)
3. **Approximate** (still can't represent 0.1 exactly)
4. **Fast** (native hardware, but half SIMD throughput vs float)
5. **Use epsilon** for comparisons (smaller epsilon than float)
6. **8 bytes** (2× memory of float)

**vs float**:
- Use double when need more precision or accumulating errors
- Use float when 7 digits sufficient and speed/memory matters

**vs Exact Types**:
- Use fix256 when need exact values (finance, safety-critical)
- Use tfp64 when need determinism (networking, replay)
- Use frac when need exact rationals (fractions)

**Remember**: double is a tool for **better approximation** than float, but still not exact. Choose the right type for your domain.

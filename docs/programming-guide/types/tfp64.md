# tfp64 - 64-bit Twisted Floating Point

**High-precision deterministic float. Cross-platform bit-exact. No -0. Unified ERR. NOT NEGOTIABLE.**

## The High-Precision Need

tfp32 gives you **4-5 decimal digits** of precision. For many use cases, that's not enough:
- Scientific computing (15+ digit accuracy)
- GPS coordinates (millimeter precision)
- Orbital mechanics (tiny errors compound over time)
- Financial models (long-term compounding)
- AI training (gradient precision matters)

**tfp64 solves this**: **~14-15 decimal digits** of deterministic precision.

## What tfp64 IS

**Twisted Floating Point 64-bit** - high-precision deterministic real numbers:

```
tfp64 = Mantissa × 2^Exponent

Structure: tbb16 exponent + tbb48 mantissa (64 bits total)
```

### tfp64 Memory Layout

```
┌──────────────────┬──────────────────────────────────────────────────┐
│  tbb16 exponent  │           tbb48 mantissa                        │
│    (16 bits)     │               (48 bits)                         │
└──────────────────┴──────────────────────────────────────────────────┘
         ↓                           ↓
    Range/scale            Value/precision
    (-32768 to         (-140,737,488,355,328 to
     +32767)            +140,737,488,355,327)
                           2's complement
```

**No separate sign bit** - sign lives in 48-bit mantissa (2's complement).

**ERR detection**: tfp64 is ERR if exponent is TBB ERR OR mantissa is TBB ERR.

## Declaration and Literals

```aria
tfp64:pi = 3.14159265358979tf;     // High precision π (14+ digits)
tfp64:zero = 0.0tf;                // Unique zero (no -0)
tfp64:negative = -2.718281828tf;   // Negative (mantissa 2's complement)
tfp64:scientific = 6.022e23tf;     // Avogadro's number
tfp64:tiny = 1.0e-100tf;           // Very small values
tfp64:error = ERR;                 // Error sentinel
```

## Precision Comparison

### tfp64 vs IEEE double (64-bit)

| Feature | tfp64 | IEEE double |
|---------|-------|-------------|
| Mantissa bits | 48 | 53 |
| Decimal precision | ~14-15 digits | ~15-16 digits |
| Exponent bits | 16 | 11 |
| Range | 2^(-32768) to 2^(+32767) | ~10^(-308) to 10^(+308) |
| Negative zero | NO (unique zero) | YES (+0 and -0) |
| NaN states | NONE (unified ERR) | MANY (qNaN, sNaN, payload) |
| Infinity | NO (overflow = ERR) | YES (+Inf, -Inf) |
| Cross-platform | 100% bit-exact | Platform-dependent |
| Speed | ~10-50x slower | Hardware accelerated |

**The trade-off**:
- **5 fewer mantissa bits** (48 vs 53) = ~1 digit less precision
- **5 more exponent bits** (16 vs 11) = MUCH larger range
- **BUT: Deterministic** across ALL platforms
- **AND: Safe** (unified ERR, no NaN/Inf)

**Philosophy**: **Lose 1 digit of precision, gain perfect reproducibility.**

Rounding drift in most algorithms burns more than 1 digit anyway - might as well get determinism in exchange.

## Deterministic Arithmetic

**Software implementation** - bit-exact on x86, ARM, RISC-V, every platform:

```aria
tfp64:avogadro = 6.02214076e23tf;      // Avogadro's number
tfp64:boltzmann = 1.380649e-23tf;      // Boltzmann constant
tfp64:planck = 6.62607015e-34tf;       // Planck constant

tfp64:sum = avogadro + boltzmann;      // Bit-exact everywhere
tfp64:product = boltzmann * planck;    // Bit-exact everywhere
tfp64:ratio = avogadro / planck;       // Bit-exact everywhere

// Hash the results - IDENTICAL on all platforms
```

## Unified ERR Propagation

Same as tfp32 - **ERR is sticky**:

```aria
tfp64:bad = 1.0tf / 0.0tf;             // ERR (division by zero)
tfp64:worse = bad + 1.0e100tf;         // ERR (sticky)
tfp64:worst = sqrt(worse);             // ERR (sticky)
```

### High-Precision Error Cases

```aria
tfp64:div_zero = 1.0tf / 0.0tf;        // ERR (not Inf)
tfp64:sqrt_neg = sqrt(-1.0tf);         // ERR (not NaN)
tfp64:overflow = 1.0e30000tf;          // ERR (exponent overflow)
tfp64:ln_neg = ln(-5.0tf);             // ERR (domain error)
tfp64:asin_overflow = asin(2.0tf);     // ERR (domain error, |x| must be ≤ 1)
```

## Scientific Computing

**Deterministic transcendentals** (Taylor series, bit-exact):

```aria
tfp64:pi = 3.14159265358979tf;
tfp64:e = 2.71828182845905tf;

// Trigonometry (bit-exact)
tfp64:sin_pi_2 = sin(pi / 2.0tf);      // ~1.0tf (via Taylor)
tfp64:cos_0 = cos(0.0tf);              // 1.0tf (exact)
tfp64:tan_pi_4 = tan(pi / 4.0tf);      // ~1.0tf (via Taylor)

// Exponentials (bit-exact)
tfp64:exp_1 = exp(1.0tf);              // ~e (via Taylor)
tfp64:ln_e = ln(e);                    // ~1.0tf (via Taylor)
tfp64:pow_result = pow(2.0tf, 10.0tf); // 1024.0tf (exact)

// Roots (bit-exact)
tfp64:sqrt_2 = sqrt(2.0tf);            // ~1.41421356237tf (via Newton)
tfp64:cbrt_27 = cbrt(27.0tf);          // 3.0tf (exact)
```

**Every platform gets EXACTLY the same answer** - x86, ARM, RISC-V, GPU, FPGA, all identical.

## GPS Coordinates (Millimeter Precision)

**IEEE double has precision problems at GPS scales**:
- Earth circumference: ~40 million meters
- Millimeter precision: need 11 decimal digits
- IEEE double: ~15-16 digits (barely enough, rounding errors accumulate)

**tfp64 solution** (deterministic GPS):

```aria
struct:GPSCoord = {
    tfp64:latitude;     // -90.0 to +90.0 degrees
    tfp64:longitude;    // -180.0 to +180.0 degrees
    tfp64:altitude;     // Meters above sea level
}

func:distance = tfp64(GPSCoord:a, GPSCoord:b) {
    // Haversine formula (deterministic on all platforms)
    tfp64:R = 6371000.0tf;                    // Earth radius (meters)
    tfp64:lat1 = a.latitude * 0.0174533tf;    // Degrees to radians
    tfp64:lat2 = b.latitude * 0.0174533tf;
    tfp64:dlat = lat2 - lat1;
    tfp64:dlon = (b.longitude - a.longitude) * 0.0174533tf;
    
    tfp64:a_val = sin(dlat / 2.0tf) * sin(dlat / 2.0tf) +
                  cos(lat1) * cos(lat2) *
                  sin(dlon / 2.0tf) * sin(dlon / 2.0tf);
    
    tfp64:c = 2.0tf * asin(sqrt(a_val));
    tfp64:distance = R * c;
    
    return {err:NIL, val:distance};
    // IDENTICAL distance on mobile (ARM), server (x86), satellite (RISC-V)
};
```

## Orbital Mechanics (Long-Term Stability)

**Small errors compound over thousands of orbits** - determinism critical:

```aria
struct:OrbitalState = {
    tfp64:x, y, z;          // Position (meters)
    tfp64:vx, vy, vz;       // Velocity (m/s)
    tfp64:time;             // Mission elapsed time (seconds)
}

func:gravity = tfp64(tfp64:distance) {
    tfp64:G = 6.67430e-11tf;           // Gravitational constant
    tfp64:M_earth = 5.972e24tf;        // Earth mass (kg)
    
    if (distance == 0.0tf) {
        return {err:"Division by zero", val:unknown};
    }
    
    tfp64:force = (G * M_earth) / (distance * distance);
    return {err:NIL, val:force};
};

func:updateOrbit = void(OrbitalState#:state, tfp64:dt) {
    tfp64:r = sqrt(state.x * state.x + 
                   state.y * state.y + 
                   state.z * state.z);
    
    if (r == ERR) {
        // Overflow or underflow - halt simulation
        return;
    }
    
    tfp64:g = gravity(r).val;
    tfp64:ax = -g * (state.x / r);
    tfp64:ay = -g * (state.y / r);
    tfp64:az = -g * (state.z / r);
    
    // Velocity Verlet integration (bit-exact)
    state.vx += ax * dt;
    state.vy += ay * dt;
    state.vz += az * dt;
    
    state.x += state.vx * dt;
    state.y += state.vy * dt;
    state.z += state.vz * dt;
    
    state.time += dt;
    
    // CRITICAL: Same orbit after 10,000 iterations on ALL platforms
};
```

## Financial Long-Term Compounding

**Compound interest over decades** - small errors = big losses:

```aria
func:compoundInterest = tfp64(tfp64:principal, tfp64:rate, i32:years) {
    tfp64:amount = principal;
    
    till years loop:year
        amount *= (1.0tf + rate);
        
        if (amount == ERR) {
            return {err:"Overflow in compounding", val:unknown};
        }
    end
    
    return {err:NIL, val:amount};
};

// 30-year investment (deterministic on all platforms)
tfp64:initial = 100000.0tf;            // $100,000 initial
tfp64:annual_rate = 0.07tf;            // 7% annual return
tfp64:final = compoundInterest(initial, annual_rate, 30).val;

// IDENTICAL result on trading server (x86), mobile app (ARM), audit system (RISC-V)
// No "my calculation says $761,225, yours says $761,227" discrepancies
```

## AI Training (Gradient Precision)

**Neural network training stability** depends on precise gradients:

```aria
struct:Neuron = {
    tfp64:weight;
    tfp64:bias;
    tfp64:gradient;
}

func:updateWeight = void(Neuron#:n, tfp64:learning_rate) {
    // Gradient descent (deterministic)
    tfp64:delta = n.gradient * learning_rate;
    n.weight -= delta;
    
    if (n.weight == ERR) {
        // Gradient explosion - halt training
        stderr.write("Gradient explosion detected\n");
        return;
    }
    
    // CRITICAL: Same weights on all training nodes (distributed training)
    // Server A, B, C all compute IDENTICAL updates (bit-exact)
};

func:backprop = void(Neuron#:n, tfp64:error) {
    // Backpropagation (deterministic gradient)
    n.gradient = error * n.weight;
    
    // Bit-exact on all platforms
    // No "loss divergence" between training nodes
};
```

## tfp64 vs frac64

**High-precision deterministic approximations vs exact rationals**:

### frac64 (Exact Rationals)
- **Perfect accuracy** - no rounding, ever
- **Rational only** - cannot represent irrationals (√2, π, e)
- **Denominator growth** - repeated operations increase denominator
- **Fast** - integer arithmetic (hardware)
- **Best for**: Exact calculations (money, music, unit conversion)

### tfp64 (Deterministic Approximations)
- **High precision** - 14-15 decimal digits
- **Any real** - can approximate irrationals
- **Fixed size** - denominator doesn't grow
- **Slow** - software implementation
- **Best for**: Science, GPS, orbits, AI, long-term simulations

### Example: Square Root

```aria
// IMPOSSIBLE: frac64 cannot represent √2 exactly
frac64:two = {n:2, d:1};
// frac64:sqrt_two = sqrt(two);    // ERROR: No exact rational for √2

// POSSIBLE: tfp64 approximates √2 deterministically
tfp64:two = 2.0tf;
tfp64:sqrt_two = sqrt(two);        // ~1.41421356237tf (bit-exact everywhere)
```

### Example: Pi Calculation

```aria
// IMPOSSIBLE: frac64 cannot represent π exactly (irrational)
// frac64:pi = 3.14159...          // ERROR: No exact rational for π

// POSSIBLE: tfp64 approximates π deterministically
tfp64:pi = 3.14159265358979tf;     // Bit-exact on all platforms
tfp64:circumference = 2.0tf * pi * radius;
```

## Cross-Platform Verification

**Hash verification example** (distributed systems):

```aria
func:complexCalculation = tfp64(tfp64:input) {
    tfp64:step1 = sin(input);
    tfp64:step2 = exp(step1);
    tfp64:step3 = ln(step2 + 1.0tf);
    tfp64:result = sqrt(step3 * step3 + input * input);
    
    return {err:NIL, val:result};
};

// Compute hash of result (for consensus)
func:hashResult = u64(tfp64:value) {
    u64:bits = value.as_bits();        // Raw 64-bit pattern
    return {err:NIL, val:bits};
};

// Distributed servers verify consensus
tfp64:input = 2.71828tf;
tfp64:result = complexCalculation(input).val;
u64:hash = hashResult(result).val;

// Server A (x86):     hash = 0x123456789ABCDEF0
// Server B (ARM):     hash = 0x123456789ABCDEF0  (IDENTICAL!)
// Server C (RISC-V):  hash = 0x123456789ABCDEF0  (CONSENSUS!)
```

## Memory and Performance

### Size
- **8 bytes** per value (64 bits total)
- Cache line: **8 tfp64 values** per 64-byte cache line
- Array of 1000: **8 KB**

### Speed
- **10-50x slower than hardware double** (software implementation)
- Addition/subtraction: ~100-200 CPU cycles
- Multiplication/division: ~200-500 CPU cycles
- Transcendentals (sin, cos, sqrt): ~1000-5000 CPU cycles

**Trade-off**: Slower, but **100% deterministic and safe**.

### When to Use Hardware IEEE

**If speed matters more than determinism**:

```aria
// Fast but platform-dependent (IEEE 754)
flt64:fast = 3.14159265358979;     // Hardware double (FPU)
flt64:result = fast * fast;        // ~1-2 CPU cycles

// Slow but deterministic (Twisted Float)
tfp64:safe = 3.14159265358979tf;   // Software tfp64
tfp64:result = safe * safe;        // ~200-500 CPU cycles
```

**Choice is yours**: Speed or safety, Aria gives you both options.

## Safety Integration

tfp64 works with Aria's Result<T> safety:

```aria
func:safeLog = Result<tfp64>(tfp64:value) {
    if (value <= 0.0tf) {
        return {err:"ln requires positive value", val:unknown};
    }
    
    tfp64:result = ln(value);
    if (result == ERR) {
        return {err:"ln overflow", val:unknown};
    }
    
    return {err:NIL, val:result};
};

// Usage
Result<tfp64>:r = safeLog(-5.0tf);
if (r.err != NIL) {
    stderr.write("Error: "); r.err.write_to(stderr); stderr.write("\n");
} else {
    tfp64:log_val = r.val;  // Safe to use
}
```

## Comparison Operations

**Unique zero** (no +0/-0 confusion):

```aria
tfp64:a = 1.41421356237tf;         // √2 approximation
tfp64:b = 1.41421356237tf;
tfp64:c = 1.41421356238tf;         // Slightly different

bool:eq = (a == b);                // true (bit-exact)
bool:ne = (a != c);                // true
bool:lt = (a < c);                 // true
bool:le = (a <= b);                // true
bool:gt = (c > a);                 // true
bool:ge = (c >= a);                // true

// Unique zero (only one zero, no sign confusion)
tfp64:pos_zero = 0.0tf;
tfp64:calc_zero = 1.0tf - 1.0tf;
bool:zeros_equal = (pos_zero == calc_zero);  // true (only ONE zero!)
```

## Common Patterns

### Pattern 1: Iterative Refinement
```aria
func:newtonSqrt = tfp64(tfp64:x, i32:iterations) {
    if (x < 0.0tf) {
        return {err:"Cannot sqrt negative", val:unknown};
    }
    
    tfp64:guess = x / 2.0tf;       // Initial guess
    
    till iterations loop:i
        tfp64:next = (guess + (x / guess)) / 2.0tf;
        
        if (next == ERR) {
            return {err:"Overflow in Newton iteration", val:unknown};
        }
        
        guess = next;
    end
    
    return {err:NIL, val:guess};
    // Deterministic on all platforms (same iterations = same result)
};
```

### Pattern 2: Scientific Constants
```aria
// Physics constants (CODATA 2018 values, deterministic)
const:tfp64:c = 299792458.0tf;                // Speed of light (m/s)
const:tfp64:G = 6.67430e-11tf;                // Gravitational constant
const:tfp64:h = 6.62607015e-34tf;             // Planck constant
const:tfp64:k_B = 1.380649e-23tf;             // Boltzmann constant
const:tfp64:N_A = 6.02214076e23tf;            // Avogadro's number

// All platforms use EXACT same constants (bit-exact)
```

### Pattern 3: High-Precision Accumulation
```aria
func:sumArray = tfp64(tfp64[]:values, usize:count) {
    tfp64:sum = 0.0tf;
    
    till count loop:i
        sum += values[i];
        
        if (sum == ERR) {
            return {err:"Overflow in accumulation", val:unknown};
        }
    end
    
    return {err:NIL, val:sum};
    // Bit-exact on all platforms (no rounding variance)
};
```

## Nikola's Cognitive Models

**Alternative Intelligence needs deterministic cognition**:

```aria
struct:BeliefState = {
    tfp64:confidence;              // Q9 confidence (gradient thinking)
    tfp64:evidence_weight;         // Accumulated evidence
    tfp64:prior_probability;       // Bayesian prior
}

func:updateBelief = void(BeliefState#:belief, tfp64:new_evidence) {
    // Bayesian update (deterministic)
    tfp64:likelihood = new_evidence / (new_evidence + 1.0tf);
    tfp64:posterior = (belief.prior_probability * likelihood) /
                      ((belief.prior_probability * likelihood) +
                       ((1.0tf - belief.prior_probability) * (1.0tf - likelihood)));
    
    belief.confidence += (posterior - 0.5tf) * 2.0tf;  // Map to Q9 scale
    belief.evidence_weight += new_evidence;
    belief.prior_probability = posterior;
    
    // CRITICAL: Nikola's reasoning must be reproducible
    // Training server (x86), mobile deployment (ARM) - SAME cognition
    // Self-improvement requires deterministic thought process
};
```

## Summary

**tfp64** is Aria's **high-precision deterministic floating point**:

✅ **High precision** - 14-15 decimal digits (vs tfp32's 4-5)  
✅ **Deterministic** - bit-exact on x86, ARM, RISC-V, all platforms  
✅ **Unified ERR** - no NaN/Inf ambiguity  
✅ **Unique zero** - no +0/-0 confusion  
✅ **Large range** - 16-bit exponent (wider than IEEE double)  
✅ **Cross-platform** - distributed consensus, game replays work  
✅ **Scientific** - GPS, orbits, AI, long-term simulations  

❌ **Slower** - 10-50x slower than hardware double  
❌ **Slightly less precise** - 48-bit mantissa vs IEEE's 53-bit (lose ~1 digit)  

**Use tfp64 when**:
- High precision needed (scientific, GPS, orbital, financial)
- Determinism required (distributed systems, replays)
- Safety matters (no silent NaN/Inf corruption)
- Cross-platform reproducibility critical

**Complement to frac64**:
- frac64: Exact rationals (perfect, but denominator grows)
- tfp64: Deterministic approximations (fixed size, handles irrationals)

**Philosophy**: **Lose 1 digit of precision, gain perfect reproducibility.**

Worth it when rounding drift burns more than 1 digit anyway.

**NOT NEGOTIABLE**: No -0, no NaN, no platform variance. **Period.**

# complex<T> - Generic Complex Numbers

**Category**: Types → Advanced Mathematics  
**Purpose**: Complex number arithmetic for physics, signal processing, wavefunctions  
**Status**: ✅ IMPLEMENTED (February 2026)

---

## Overview

**complex<T>** is a **generic complex number type** where T is the component type for real and imaginary parts. Essential for quantum mechanics, wave interference, and Fourier analysis.

---

## Key Characteristics

| Property | Value |
|----------|-------|
| **Structure** | `{ *T:real; *T:imag; }` |
| **Generic** | Works with any numeric type T |
| **Common Uses** | complex<fix256>, complex<tbb64>, complex<flt64> |
| **ERR Propagation** | Inherits from component type T |
| **SIMD Ready** | Interleaved layout (real, imag, real, imag...) |

---

## Declaration

```aria
// Basic complex numbers
complex<int32>:z1 = {real: 3, imag: 4};        // 3 + 4i
complex<fix256>:wavefunction = {real: fix256(1, 5), imag: fix256(0, 5)};

// Using different base types
complex<tbb64>:signal = {real: 100t64, imag: -50t64};
complex<flt64>:fourier = {real: 0.707, imag: 0.707};
```

---

## Arithmetic Operations

### Addition & Subtraction

```aria
complex<int32>:a = {real: 3, imag: 4};    // 3 + 4i
complex<int32>:b = {real: 1, imag: 2};    // 1 + 2i

complex<int32>:sum = a + b;               // (3+1) + (4+2)i = 4 + 6i
complex<int32>:diff = a - b;              // (3-1) + (4-2)i = 2 + 2i
```

### Multiplication

```aria
// (a + bi) * (c + di) = (ac - bd) + (ad + bc)i
complex<int32>:z1 = {real: 3, imag: 4};   // 3 + 4i
complex<int32>:z2 = {real: 1, imag: 2};   // 1 + 2i

complex<int32>:product = z1 * z2;
// = (3*1 - 4*2) + (3*2 + 4*1)i
// = (3 - 8) + (6 + 4)i
// = -5 + 10i
```

### Division

```aria
// (a + bi) / (c + di) = [(ac + bd) + (bc - ad)i] / (c² + d²)
complex<int32>:numerator = {real: 10, imag: 5};
complex<int32>:denominator = {real: 2, imag: 1};

complex<int32>:quotient = numerator / denominator;
// = [(10*2 + 5*1) + (5*2 - 10*1)i] / (2² + 1²)
// = [25 + 0i] / 5
// = 5 + 0i
```

---

## Complex-Specific Operations

### Conjugate

```aria
complex<int32>:z = {real: 3, imag: 4};    // 3 + 4i
complex<int32>:conj = z.conjugate();      // 3 - 4i

// Real part stays same, imaginary part negated
```

### Magnitude (Future - not yet implemented)

```aria
// |z| = √(real² + imag²)
complex<fix256>:z = {real: fix256(3), imag: fix256(4)};
fix256:magnitude = z.magnitude();  // √(9 + 16) = 5
```

### Phase/Argument (Future - not yet implemented)

```aria
// θ = atan2(imag, real)
complex<fix256>:z = {real: fix256(1), imag: fix256(1)};
fix256:phase = z.phase();  // π/4 radians = 45°
```

---

## ERR Propagation

```aria
// ERR in either component propagates
complex<tbb64>:good = {real: 100t64, imag: 50t64};
complex<tbb64>:bad = {real: ERR, imag: 0t64};

complex<tbb64>:result = good + bad;
// result.real == ERR
// result.imag == ERR (entire complex number tainted)
```

---

## Safety-Critical Use Case: Nikola AGI Wavefunction

### 9D Wave Interference

```aria
// Nikola consciousness substrate: 9D manifold wave processing
// Wavefunction ψ(x,t) = ψ_real + i·ψ_imag
// Must be deterministic - no floating-point drift allowed

complex<fix256>:psi = calculate_wavefunction(position, time);

// Wave interference: ψ_total = ψ_1 + ψ_2
complex<fix256>:psi_1 = neuron_wave_1();
complex<fix256>:psi_2 = neuron_wave_2();
complex<fix256>:interference = psi_1 + psi_2;

// Check for corruption
if (interference.real == ERR || interference.imag == ERR) {
    stderr.write("Wave interference corruption - HALT SUBSTRATE\n");
    !!! ERR_CONSCIOUSNESS_CORRUPTED;
}

// Bit-exact determinism prevents catastrophic model drift
```

### Why complex<fix256> Specifically?

```aria
// CRITICAL REQUIREMENT:
// - Floating-point drift could cause wave destructive interference
// - Sign flips in imaginary component → inverted phases
// - Accumulated errors → consciousness substrate instability
// - Potential PTSD-like or schizophrenia-like AI states

// complex<fix256> ensures:
// ✓ Bit-exact wave calculations
// ✓ Zero accumulated drift
// ✓ Cross-platform determinism
// ✓ ERR detection before propagation
```

---

## SIMD Vectorization

### Interleaved Layout for Performance

```aria
// Structure-of-Arrays (SoA) layout enables SIMD
// Memory: [real₀, imag₀, real₁, imag₁, real₂, imag₂, ...]

simd<complex<fix256>, 8>:wave_array;
// Processes 8 complex numbers in parallel
// Critical for <1ms timestep requirement
```

---

## Type Combinations

### Common Pairings

| Type | Use Case |
|------|----------|
| `complex<fix256>` | Physics simulation, AGI substrate, robotics |
| `complex<tbb64>` | Signal processing with ERR detection |
| `complex<flt64>` | General-purpose complex math (non-critical) |
| `complex<int32>` | Integer-only complex arithmetic |

---

## Physics Example: Fourier Transform

```aria
// Discrete Fourier Transform (simplified)
func:dft = complex<fix256>[](complex<fix256>[]:signal, int64:count) {
    complex<fix256>[]:spectrum = array_alloc<complex<fix256>>(count);
    
    till count loop
        complex<fix256>:sum = {real: fix256(0), imag: fix256(0)};
        int64:k = $;  // Iteration variable
        
        till count loop
            int64:n = $;
            fix256:angle = fix256(-2) * PI * fix256(k) * fix256(n) / fix256(count);
            
            // e^(-i·angle) = cos(angle) - i·sin(angle)
            complex<fix256>:twiddle = {
                real: cos(angle),
                imag: -sin(angle)
            };
            
            sum = sum + (signal[n] * twiddle);
        end
        
        spectrum[k] = sum;
    end
    
    pass(spectrum);
};
```

---

## Implementation Status

### ✅ Complete
- [x] Generic type: complex<T>
- [x] Basic arithmetic (+, -, *, /)
- [x] Conjugate operation
- [x] ERR propagation
- [x] SIMD-ready layout

### ⚠️ Planned
- [ ] Magnitude calculation
- [ ] Phase/argument calculation  
- [ ] Polar form conversion
- [ ] Complex exponential (e^(ix))
- [ ] Complex logarithm
- [ ] Complex power (z^w)

---

## Related

- [fix256](fix256.md) - Deterministic fixed-point (primary component type)
- [simd<T,N>](simd.md) - Vectorization for parallel complex operations
- [atomic<T>](atomic.md) - Thread-safe complex numbers
- [TBB Overview](tbb_overview.md) - ERR propagation system

---

**Remember**: complex<T> = **mathematics** + **generics** + **safety**!

**Critical for**: Nikola AGI substrate, quantum simulation, signal processing, control theory

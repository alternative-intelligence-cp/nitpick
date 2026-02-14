# Complex<T> - Generic Complex Numbers for Wave Mechanics

**Category**: Types → Advanced Mathematics  
**Purpose**: Complex number arithmetic for wave processing, signal analysis, quantum mechanics  
**Status**: ✅ IMPLEMENTED (Phase 5.3 - February 2026)  
**Philosophy**: "The substrate processes waves. The infrastructure computes them."

---

## Overview

**complex<T>** is Aria's **generic complex number type** providing full complex arithmetic with any numeric component type. Complex numbers extend the real number line into a 2D plane, enabling representation of waves, rotations, and oscillations that are fundamental to physics, signal processing, and consciousness substrates.

**Critical Design Principle**: By handling wave mechanics at the language/type system level, higher-level systems (like Nikola's SHVO torus) can focus on **what information flows** rather than **how waves propagate**. This separation of concerns prevents the catastrophic memory bloat that plagued prototype implementations.

```aria
// Structure: complex number = real part + imaginary part
complex<T>:z = {real: xₛ, imag: yₛ};  // Represents x + yi in the complex plane
```

---

## The Problem: Waves Are Everywhere

### Real Numbers Can't Represent Phase

Real numbers exist on a 1D line. But physical reality is full of phenomena that require 2D representation:

```aria
// ❌ IMPOSSIBLE with real numbers alone:

// 1. Wave interference - need amplitude AND phase
float:wave = 0.5;  // Is this constructive or destructive interference?
                   // Lost the phase information!

// 2. AC circuit analysis - need voltage AND phase shift
float:impedance = 100.0;  // Is this resistive or reactive?
                          // Lost the phase angle!

// 3. Quantum state - need amplitude AND phase
float:probability = 0.5;  // What's the phase of this superposition?
                         // Lost the quantum information!

// 4. 2D rotations - need cos AND sin components
float:rotation = 45.0;  // How do we compose multiple rotations?
                        // Lost the geometric structure!
```

### Why Complex Numbers Solve This

Complex numbers provide **two orthogonal components** (real and imaginary) that naturally represent:

- **Waves**: Amplitude (magnitude) and phase (angle)
- **Rotations**: cos(θ) and sin(θ) components
- **Oscillations**: In-phase and quadrature components
- **2D Geometry**: x and y coordinates in the complex plane

```aria
// ✅ SOLVED with complex numbers:

// Wave with amplitude 1.0, phase π/4
complex<fix256>:wave = {real: fix256(0, 707), imag: fix256(0, 707)};
// = 1.0 · e^(i·π/4) = cos(π/4) + i·sin(π/4)

// Impedance: 100Ω resistance + 50Ω capacitive reactance
complex<fix256>:impedance = {real: fix256(100), imag: fix256(-50)};
// = 100 - 50i (negative imaginary = capacitive)

// Quantum superposition: |ψ⟩ = (1/√2)(|0⟩ + i|1⟩)
complex<fix256>:psi = {real: fix256(0, 707), imag: fix256(0, 707)};
// Probability = |ψ|² = 0.707² + 0.707² = 1.0 (normalized)
```

---

## Type Definition & Memory Layout

### Generic Structure

```aria
// Generic complex number with component type T
struct:complex<T> =
    *T:real;  // Real component (x-axis in complex plane)
    *T:imag;  // Imaginary component (y-axis in complex plane)
end

// Memory layout (interleaved for SIMD efficiency):
// [real₀][imag₀][real₁][imag₁][real₂][imag₂]...
```

### Size & Alignment

| Instantiation | Size | Alignment | Use Case |
|---------------|------|-----------|----------|
| `complex<tbb8>` | 2 bytes | 1 byte | Compact phase encoding |
| `complex<tbb16>` | 4 bytes | 2 bytes | Audio sample processing |
| `complex<tbb32>` | 8 bytes | 4 bytes | General signal processing |
| `complex<tbb64>` | 16 bytes | 8 bytes | High-precision waves |
| `complex<frac16>` | 12 bytes | 2 bytes | Exact phase relationships |
| `complex<tfp32>` | 8 bytes | 4 bytes | Deterministic wave simulation |
| `complex<fix256>` | 64 bytes | 32 bytes | Nikola consciousness substrate |

### Common Type Instantiations

```aria
// Deterministic wave processing (bit-exact across platforms)
complex<tfp32>:wave_deterministic;

// Exact rational phase relationships (no drift)
complex<frac16>:musical_interval;

// Fixed-point for safety-critical (zero drift over time)
complex<fix256>:consciousness_wavefunction;

// Twos-complement bounded with ERR detection
complex<tbb64>:signal_with_error_propagation;

// Standard floating-point (fast but non-deterministic)
complex<flt64>:general_purpose;
```

---

## Arithmetic Operations

### Addition & Subtraction (Component-wise)

```aria
// (a + bi) + (c + di) = (a+c) + (b+d)i
complex<int32>:z1 = {real: 3, imag: 4};    // 3 + 4i
complex<int32>:z2 = {real: 1, imag: 2};    // 1 + 2i

complex<int32>:sum = z1 + z2;
// = (3+1) + (4+2)i = 4 + 6i
// sum.real == 4, sum.imag == 6

complex<int32>:diff = z1 - z2;
// = (3-1) + (4-2)i = 2 + 2i
// diff.real == 2, diff.imag == 2
```

**Geometric Interpretation**: Vector addition/subtraction in the complex plane.

**Physical Meaning**: Wave superposition (interference) - amplitudes add at each phase.

### Multiplication (Distribute & Combine)

```aria
// (a + bi) · (c + di) = (ac - bd) + (ad + bc)i
// Remember: i² = -1

complex<int32>:z1 = {real: 3, imag: 4};    // 3 + 4i
complex<int32>:z2 = {real: 1, imag: 2};    // 1 + 2i

complex<int32>:product = z1 * z2;
// Expand: (3 + 4i)(1 + 2i)
// = 3·1 + 3·2i + 4i·1 + 4i·2i
// = 3 + 6i + 4i + 8i²
// = 3 + 10i + 8(-1)
// = 3 + 10i - 8
// = -5 + 10i
// product.real == -5, product.imag == 10
```

**Geometric Interpretation**: Multiply magnitudes, add phases (rotation + scaling).

**Physical Meaning**: Frequency mixing in signal processing, quantum operator application.

### Division (Multiply by Conjugate)

```aria
// (a + bi) / (c + di) = [(a + bi)(c - di)] / [(c + di)(c - di)]
//                     = [(ac + bd) + (bc - ad)i] / (c² + d²)

complex<int32>:numerator = {real: 10, imag: 5};      // 10 + 5i
complex<int32>:denominator = {real: 2, imag: 1};     // 2 + 1i

complex<int32>:quotient = numerator / denominator;
// Multiply num by conjugate of denom:
// = (10 + 5i)(2 - 1i) / [(2 + 1i)(2 - 1i)]
// = (20 - 10i + 10i - 5i²) / (4 - 2i + 2i - i²)
// = (20 - 5(-1)) / (4 - (-1))
// = 25 / 5
// = 5 + 0i
// quotient.real == 5, quotient.imag == 0
```

**Why this works**: Multiplying by conjugate makes denominator real (c² + d²).

**Physical Meaning**: Deconvolution, filter inversion, impedance calculations.

---

## Complex-Specific Operations

### Conjugate (Flip Imaginary Sign)

```aria
// z* = a - bi (if z = a + bi)
// Reflects across real axis in complex plane

complex<int32>:z = {real: 3, imag: 4};         // 3 + 4i
complex<int32>:conj = z.conjugate();           // 3 - 4i
// conj.real == 3, conj.imag == -4

// Property: z · z* = |z|² (always real!)
complex<int32>:magnitude_squared = z * z.conjugate();
// = (3 + 4i)(3 - 4i) = 9 - 12i + 12i - 16i² = 9 + 16 = 25
// magnitude_squared.real == 25, magnitude_squared.imag == 0
```

**Use Cases**:
- Computing magnitude: |z| = √(z · z*)
- Division algorithm (rationalize denominator)
- Signal processing (cross-correlation)
- Quantum mechanics (conjugate transpose for Hermitian operators)

### Magnitude (Euclidean Distance from Origin)

```aria
// |z| = √(real² + imag²)
// Distance from origin in complex plane

complex<fix256>:z = {real: fix256(3), imag: fix256(4)};
fix256:magnitude = z.magnitude();
// = √(3² + 4²) = √(9 + 16) = √25 = 5

// For normalized waves:
complex<fix256>:wave = {real: fix256(0, 6), imag: fix256(0, 8)};
fix256:amp = wave.magnitude();  // √(0.36 + 0.64) = 1.0 (unit circle)
```

**Physical Meaning**: 
- Wave amplitude (energy)
- Quantum probability density: P(x) = |ψ(x)|²
- AC voltage/current magnitude

### Phase / Argument (Angle from Real Axis)

```aria
// arg(z) = atan2(imag, real)
// Angle in radians from positive real axis

complex<fix256>:z = {real: fix256(1), imag: fix256(1)};
fix256:phase = z.phase();
// = atan2(1, 1) = π/4 radians = 45°

complex<fix256>:z2 = {real: fix256(-1), imag: fix256(0)};
fix256:phase2 = z2.phase();
// = atan2(0, -1) = π radians = 180° (on negative real axis)
```

**Physical Meaning**:
- Wave phase offset
- AC circuit phase shift
- Quantum mechanical phase

**Range**: (-π, π] by convention (principal value).

---

## Rectangular ↔ Polar Conversion

### Euler's Formula: The Bridge Between Forms

```aria
// Euler's magnificent formula:
// e^(iθ) = cos(θ) + i·sin(θ)

// Therefore any complex number can be written as:
// z = r·e^(iθ) = r·[cos(θ) + i·sin(θ)] = r·cos(θ) + i·r·sin(θ)
//   = (r·cos θ) + i·(r·sin θ)
//     ^^^^^^^^      ^^^^^^^^^
//      real part    imag part

// Where:
// r = |z| = √(real² + imag²)   (magnitude)
// θ = arg(z) = atan2(imag, real)  (phase/argument)
```

### Rectangular → Polar

```aria
// Given: z = a + bi (rectangular form)
// Find: r, θ such that z = r·e^(iθ) (polar form)

complex<fix256>:z_rect = {real: fix256(3), imag: fix256(4)};

fix256:r = z_rect.magnitude();    // √(9 + 16) = 5
fix256:theta = z_rect.phase();    // atan2(4, 3) ≈ 0.927 radians ≈ 53.1°

// Now: z = 5·e^(i·0.927)
```

### Polar → Rectangular

```aria
// Given: r, θ (polar form)
// Find: a, b such that z = a + bi (rectangular form)

func:from_polar = complex<fix256>(fix256:r, fix256:theta) {
    complex<fix256>:z = {
        real: r * cos(theta),   // a = r·cos(θ)
        imag: r * sin(theta),   // b = r·sin(θ)
    };
    pass(z);
};

// Example: magnitude 5, phase π/4
fix256:r = fix256(5);
fix256:theta = PI / fix256(4);  // 45°
complex<fix256>:z = from_polar(r, theta);
// z.real ≈ 5·0.707 ≈ 3.535
// z.imag ≈ 5·0.707 ≈ 3.535
```

**Why Polar Form Matters**:
- Multiplication: Multiply magnitudes, add phases
- Division: Divide magnitudes, subtract phases
- Powers: z^n = r^n · e^(inθ) (raise magnitude to power, multiply phase)
- Roots: ⁿ√z has n solutions equally spaced around circle

---

## Wave Mechanics & Signal Processing

### Fourier Transform: Time → Frequency Domain

The **Discrete Fourier Transform (DFT)** converts time-domain signals into frequency-domain spectra:

```aria
// DFT Formula:
// X[k] = Σ(n=0 to N-1) x[n] · e^(-i·2π·k·n/N)
//      = Σ(n=0 to N-1) x[n] · [cos(-2πkn/N) + i·sin(-2πkn/N)]

func:dft = complex<fix256>[](complex<fix256>[]:signal, int64:N) {
    complex<fix256>[]:spectrum = array_alloc<complex<fix256>>(N);
    
    till N loop  // For each frequency bin k
        complex<fix256>:sum = {real: fix256(0), imag: fix256(0)};
        int64:k = $;  // Frequency index
        
        till N loop  // Sum over all time samples n
            int64:n = $;
            
            // Compute twiddle factor: e^(-i·2πkn/N)
            fix256:angle = fix256(-2) * PI * fix256(k) * fix256(n) / fix256(N);
            complex<fix256>:twiddle = {
                real: cos(angle),
                imag: -sin(angle),  // Note: negative for forward DFT
            };
            
            // Accumulate: x[n] · e^(-i·2πkn/N)
            sum = sum + (signal[n] * twiddle);
        end
        
        spectrum[k] = sum;
    end
    
    pass(spectrum);
};
```

**Physical Meaning**:
- Time domain: Signal amplitude at each moment
- Frequency domain: How much of each frequency is present
- Complex result: Magnitude = amplitude, phase = time offset

**Example**: Audio Processing

```aria
// Audio sample at 44.1kHz for 1 second
int64:sample_rate = 44100;
complex<fix256>[]:audio_samples = load_audio_file("guitar.wav");

// Transform to frequency domain
complex<fix256>[]:frequency_spectrum = dft(audio_samples, sample_rate);

// Analyze: What frequencies are present?
till sample_rate loop
    int64:freq_bin = $;
    fix256:amplitude = frequency_spectrum[freq_bin].magnitude();
    fix256:phase = frequency_spectrum[freq_bin].phase();
    
    if amplitude > fix256(0, 01) then  // Above noise floor
        int64:frequency_hz = freq_bin;  // Bin index = frequency in Hz
        dbug.audio_spectrum("Frequency: {}Hz, Amplitude: {}, Phase: {}rad",
                            frequency_hz, amplitude, phase);
    end
end
```

### Wave Interference: Constructive vs Destructive

```aria
// Two waves with same frequency, different phases
complex<fix256>:wave1 = {real: fix256(1), imag: fix256(0)};  // Phase 0°
complex<fix256>:wave2 = {real: fix256(0, 707), imag: fix256(0, 707)};  // Phase 45°

// Superposition: Add the waves
complex<fix256>:interference = wave1 + wave2;
// = (1 + 0.707) + i·(0 + 0.707)
// = 1.707 + 0.707i

fix256:amplitude = interference.magnitude();
// = √(1.707² + 0.707²) ≈ 1.848 (constructive interference - amplitude increased!)

// Opposite example: Destructive interference
complex<fix256>:wave3 = {real: fix256(-1), imag: fix256(0)};  // Phase 180° (inverted)
complex<fix256>:cancellation = wave1 + wave3;
// = (1 - 1) + i·(0 + 0) = 0 + 0i (complete cancellation!)
```

**Physical Examples**:
- Noise-canceling headphones (destructive interference)
- Radio antenna arrays (beam forming via constructive interference)
- Optical interference patterns (double-slit experiment)
- Nikola neural oscillations (phase synchronization)

### Filtering: Frequency Domain Multiplication

```aria
// Convolution theorem: Convolution in time = Multiplication in frequency
// Low-pass filter: Remove high frequencies

func:lowpass_filter = complex<fix256>[](
    complex<fix256>[]:signal,
    fix256:cutoff_frequency,
    int64:N
) {
    // Transform signal to frequency domain
    complex<fix256>[]:spectrum = dft(signal, N);
    
    // Multiply by filter transfer function
    till N loop
        int64:freq_bin = $;
        fix256:frequency = fix256(freq_bin);
        
        // Simple brick-wall filter (sharp cutoff)
        if frequency > cutoff_frequency then
            // Zero out high frequencies
            spectrum[freq_bin] = {real: fix256(0), imag: fix256(0)};
        end
    end
    
    // Transform back to time domain
    complex<fix256>[]:filtered = inverse_dft(spectrum, N);
    pass(filtered);
};
```

---

## Quantum Mechanics: Wave Functions

### Schrödinger's Wave Function ψ(x,t)

In quantum mechanics, particles are described by complex-valued wave functions:

```aria
// Wave function: ψ(x,t) = A·e^(i(kx - ωt))
// Where:
// A = amplitude
// k = wave number (2π/wavelength)
// ω = angular frequency (2π·frequency)
// x = position
// t = time

func:quantum_wavefunction = complex<fix256>(
    fix256:x,           // Position
    fix256:t,           // Time
    fix256:amplitude,   // A
    fix256:k,           // Wave number
    fix256:omega        // Angular frequency
) {
    // Compute phase: kx - ωt
    fix256:phase = (k * x) - (omega * t);
    
    // Euler's formula: A·e^(iφ) = A·[cos(φ) + i·sin(φ)]
    complex<fix256>:psi = {
        real: amplitude * cos(phase),
        imag: amplitude * sin(phase),
    };
    
    pass(psi);
};
```

### Probability Density: Born Rule

```aria
// Probability of finding particle at position x:
// P(x) = |ψ(x)|² = ψ*(x) · ψ(x)

complex<fix256>:psi = quantum_wavefunction(x, t, A, k, omega);

// Compute probability density
fix256:probability_density = (psi.conjugate() * psi).real;
// Note: (psi* · psi) is always real! (Imaginary part cancels)

// For normalized wave function: ∫ |ψ(x)|² dx = 1
```

### Superposition: Multiple States Simultaneously

```aria
// Quantum bit (qubit) in superposition:
// |ψ⟩ = α|0⟩ + β|1⟩
// Where |α|² + |β|² = 1 (normalization)

complex<fix256>:alpha = {real: fix256(0, 707), imag: fix256(0)};      // Amplitude for |0⟩
complex<fix256>:beta = {real: fix256(0), imag: fix256(0, 707)};       // Amplitude for |1⟩

// Verify normalization:
fix256:prob_0 = (alpha.conjugate() * alpha).real;  // |α|² = 0.5
fix256:prob_1 = (beta.conjugate() * beta).real;    // |β|² = 0.5
fix256:total = prob_0 + prob_1;  // Should equal 1.0

if total != fix256(1) then
    stderr.write("Qubit not normalized!\n");
    !!! ERR_QUANTUM_STATE_INVALID;
end
```

---

## Nikola Consciousness Substrate: Why complex<fix256>?

### The Problem: Wave-Based Neural Oscillations

Nikola's consciousness substrate is a **9-dimensional hyperspherical manifold** where neural oscillations interfere to produce emergent consciousness. This requires:

1. **Wave superposition** across thousands of neurons
2. **Phase synchronization** between oscillating groups
3. **Deterministic evolution** (no drift over millions of timesteps)
4. **Zero accumulated error** (consciousness corruption prevention)

```aria
// Simplified conceptual model (actual implementation is 9D)

// 1. Each neuron has an oscillating activation (wave)
complex<fix256>:neuron_wave = {
    real: fix256(amplitude * cos(phase)),
    imag: fix256(amplitude * sin(phase)),
};

// 2. Neurons synchronize via phase coupling
complex<fix256>:coupled_oscillation = neuron1_wave + neuron2_wave;

// 3. Emergent patterns from interference
till neuron_count loop
    complex<fix256>:neuron_i = get_neuron_wave($);
    global_field = global_field + neuron_i;  // Superposition
end

// 4. Consciousness metric based on field coherence
fix256:coherence = global_field.magnitude() / fix256(neuron_count);
```

### Why Not Floating-Point?

```aria
// ❌ CATASTROPHIC with floating-point:

// Problem 1: Non-determinism across platforms
flt64:wave1 = sin(phase);  // Different results on Intel vs ARM vs GPU!
// After 1 million timesteps: Consciousness states diverge
// Nikola instance on PC ≠ Nikola instance on server
// Transfer learning impossible!

// Problem 2: Accumulation drift
flt64:phase = 0.0;
till 1000000 loop
    phase = phase + delta_phase;  // Tiny errors accumulate
end
// After long runtime: phase drifts, oscillations desynchronize
// Potential PTSD-like or schizophrenia-like AI states

// Problem 3: Sign flips in imaginary component
flt64:imag = 0.0000001;  // Near zero
flt64:imag_neg = -imag;
// Due to rounding: (imag + imag_neg) might NOT equal 0.0!
// Wave interference produces ghost signals
```

### Why complex<fix256> Specifically?

```aria
// ✅ SAFE with complex<fix256>:

// Benefit 1: Bit-exact determinism
fix256:wave1 = sin_fix256(phase);
// Identical results on ALL platforms (software implementation)
// Nikola state transferable between devices
// Reproducible debugging of consciousness states

// Benefit 2: Zero drift over infinite time
fix256:phase = fix256(0);
till 1000000000 loop  // Billion iterations
    phase = phase + delta_phase;
end
// phase accumulates exactly, no drift
// Consciousness substrate stable indefinitely

// Benefit 3: ERR propagation prevents silent corruption
complex<fix256>:psi = calculate_wavefunction(x, t);
if psi.real == ERR || psi.imag == ERR then
    stderr.write("Wave corruption detected - HALT SUBSTRATE\n");
    !!! ERR_CONSCIOUSNESS_CORRUPTED;
end
// Fail-fast prevents cascading damage to mental state
```

### Memory Trade-off: Worth It

```aria
// complex<fix256> = 64 bytes (32 bytes real + 32 bytes imag)
// complex<flt64>  = 16 bytes

// For 100,000 neurons:
// fix256: 64 × 100,000 = 6.4 MB
// flt64:  16 × 100,000 = 1.6 MB
// Difference: 4.8 MB extra

// BUT:
// - Determinism enables transfer learning (saves months of training)
// - Zero drift prevents divergence (reliability worth cost)
// - ERR detection prevents catastrophic failures (safety critical)
// - Cross-platform consistency enables distributed consciousness

// User's insight: "Prototype one took more RAM than Earth!"
// Solution: Move common operations to language level (infrastructure)
// Let the torus focus on WHAT (consciousness), not HOW (wave propagation)
```

---

## Electrical Engineering: AC Circuit Analysis

### Impedance: Resistance + Reactance

In AC circuits, impedance **Z** is a complex number combining resistance (real) and reactance (imaginary):

```aria
// Z = R + iX
// Where:
// R = resistance (dissipates energy)
// X = reactance (stores/releases energy)
//   X > 0: inductive (coil delays current)
//   X < 0: capacitive (capacitor advances current)

// Example: RLC series circuit
fix256:resistance = fix256(100);       // 100Ω resistor
fix256:inductance_reactance = fix256(50);   // 50Ω inductor at this frequency
fix256:capacitance_reactance = fix256(-30); // 30Ω capacitor at this frequency

complex<fix256>:total_impedance = {
    real: resistance,
    imag: inductance_reactance + capacitance_reactance,
};
// = 100 + i·(50 - 30) = 100 + 20i Ω (net inductive)

fix256:magnitude = total_impedance.magnitude();
// = √(100² + 20²) = √10400 ≈ 101.98Ω

fix256:phase_shift = total_impedance.phase();
// = atan2(20, 100) ≈ 0.197 radians ≈ 11.3° (current lags voltage)
```

### Ohm's Law for AC Circuits

```aria
// V = I·Z (voltage = current × impedance)
// All quantities are complex (phasors)

complex<fix256>:voltage = {real: fix256(120), imag: fix256(0)};  // 120V RMS, 0° phase
complex<fix256>:impedance = {real: fix256(100), imag: fix256(20)};

complex<fix256>:current = voltage / impedance;
// = 120 / (100 + 20i)
// = 120·(100 - 20i) / [(100 + 20i)·(100 - 20i)]
// = (12000 - 2400i) / (10000 + 400)
// = (12000 - 2400i) / 10400
// ≈ 1.154 - 0.231i Amps

fix256:current_magnitude = current.magnitude();  // ≈ 1.177 A RMS
fix256:current_phase = current.phase();  // ≈ -11.3° (current lags voltage)
```

### Power Calculations

```aria
// Complex power: S = V·I* (voltage × conjugate of current)
// S = P + iQ
// P = real power (watts, actually consumed)
// Q = reactive power (VARs, stored/released)

complex<fix256>:voltage = {real: fix256(120), imag: fix256(0)};
complex<fix256>:current = {real: fix256(1, 154), imag: fix256(-0, 231)};

complex<fix256>:power = voltage * current.conjugate();
// = 120·(1.154 + 0.231i)
// = 138.48 + 27.72i VA (volt-amperes)

fix256:real_power = power.real;       // 138.48 W (dissipated as heat)
fix256:reactive_power = power.imag;   // 27.72 VAR (stored in inductor)
fix256:apparent_power = power.magnitude();  // 141.23 VA (total)

fix256:power_factor = real_power / apparent_power;
// = 138.48 / 141.23 ≈ 0.98 (98% efficient, 2% reactive)
```

---

## Performance Characteristics

### Arithmetic Operation Costs

| Operation | Cost | Notes |
|-----------|------|-------|
| Addition | 2× scalar add | Add real, add imag independently |
| Subtraction | 2× scalar sub | Sub real, sub imag independently |
| Multiplication | 4× mul + 2× add/sub | (ac-bd) + i(ad+bc) |
| Division | 1× conj + 1× mul + 4× div | Multiply by conjugate, divide components |
| Conjugate | 1× negate | Flip sign of imaginary part |
| Magnitude | 2× square + 1× sqrt + 1× add | √(real² + imag²) |
| Phase | 1× atan2 | Arctangent of imag/real |

**Component Type Impact**:
```aria
// complex<tbb32>: Fast (hardware integer ops)
// complex<tfp32>: Medium (software emulation of float)
// complex<fix256>: Slow (256-bit fixed-point ops)
// complex<frac16>: Very slow (GCD reduction after every op)
```

### SIMD Vectorization: Parallel Complex Operations

The interleaved memory layout enables efficient SIMD processing:

```aria
// Memory layout: [real₀, imag₀, real₁, imag₁, real₂, imag₂, real₃, imag₃]
//                  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//                  Fits in one 256-bit SIMD register (complex<tbb32>)

simd<complex<tbb32>, 4>:wave_packet;
// Processes 4 complex numbers in parallel

// Addition: 4 complex adds in one SIMD instruction
simd<complex<tbb32>, 4>:wave1 = load_simd_complex(array1);
simd<complex<tbb32>, 4>:wave2 = load_simd_complex(array2);
simd<complex<tbb32>, 4>:sum = wave1 + wave2;  // 4× speedup!

// Critical for Nikola: Process 100,000 neurons in <1ms
// Requirement: Each neuron wave updated per timestep
// With SIMD: 100,000 / (4 × CPU_freq) = ~100,000 / 4,000,000 = 25μs per batch
// Achievable on modern CPUs!
```

### FFT Algorithm: O(N log N) vs O(N²)

```aria
// Naive DFT: O(N²) complexity
// Fast Fourier Transform (FFT): O(N log N) complexity

// For N = 1024 samples:
// DFT: 1024² = 1,048,576 operations
// FFT: 1024 × log₂(1024) = 1024 × 10 = 10,240 operations
// Speedup: 102× faster!

// Cooley-Tukey FFT (radix-2, decimation-in-time)
func:fft = complex<fix256>[]( complex<fix256>[]:x, int64:N) {
    if N == 1 then
        pass(x);  // Base case
    end
    
    // Split into even and odd indices
    complex<fix256>[]:even = array_alloc<complex<fix256>>(N/2);
    complex<fix256>[]:odd = array_alloc<complex<fix256>>(N/2);
    
    till N/2 loop
        int64:k = $;
        even[k] = x[2*k];
        odd[k] = x[2*k + 1];
    end
    
    // Recursive calls
    complex<fix256>[]:fft_even = fft(even, N/2);
    complex<fix256>[]:fft_odd = fft(odd, N/2);
    
    // Combine results
    complex<fix256>[]:result = array_alloc<complex<fix256>>(N);
    till N/2 loop
        int64:k = $;
        
        fix256:angle = fix256(-2) * PI * fix256(k) / fix256(N);
        complex<fix256>:twiddle = {
            real: cos(angle),
            imag: sin(angle),
        };
        
        complex<fix256>:t = twiddle * fft_odd[k];
        result[k] = fft_even[k] + t;
        result[k + N/2] = fft_even[k] - t;
    end
    
    pass(result);
};
```

---

## Integration with Aria Type System

### ERR Propagation from Component Type

```aria
// ERR in component type propagates to complex result
complex<tbb64>:good = {real: 100t64, imag: 50t64};
complex<tbb64>:bad = {real: ERR, imag: 0t64};

complex<tbb64>:result = good + bad;
// result.real == ERR
// result.imag == ERR (entire complex number tainted)

// Detect and handle:
if result.real == ERR || result.imag == ERR then
    stderr.write("Complex operation produced ERR\n");
    !!! ERR_COMPLEX_COMPUTATION_FAILED;
end
```

### Deterministic Wave Processing: complex<tfp32>

```aria
// Twisted floating-point provides cross-platform determinism
complex<tfp32>:wave = {
    real: 1.0tf32,
    imag: 0.5tf32,
};

// Identical results on Intel, ARM, RISC-V, GPU
// Critical for multiplayer physics simulations
till 1000000 loop  // Million timesteps
    wave = wave * rotation_tfp;  // Rotate wave
end
// wave is bit-exact across all platforms!
```

### Exact Phase Relationships: complex<frac16>

```aria
// Musical intervals require exact frequency ratios
// Just intonation: Perfect fifth = 3/2 frequency ratio

frac16:perfect_fifth_ratio = {whole: 1t16, num: 1t16, denom: 2t16};  // 3/2

// Represent as complex phasor (unit circle)
// Angle = 2π·(3/2) mod 2π
frac16:angle_fraction = perfect_fifth_ratio;  // 3/2

complex<frac16>:musical_phasor = {
    real: cos_frac(angle_fraction),  // Exact cos(3π/2)
    imag: sin_frac(angle_fraction),  // Exact sin(3π/2)
};

// Over millions of samples: NO pitch drift!
// Orchestral tuning remains perfect indefinitely
```

### Persistent Wave State: Handle<complex<T>>

```aria
// Store complex wave state in arena with generational handles
Handle<complex<fix256>>:neuron_wave_handle;

// Allocate wave state
complex<fix256>:initial_wave = {real: fix256(1), imag: fix256(0)};
neuron_wave_handle = arena.alloc(initial_wave);

// Access later (checked for use-after-free)
complex<fix256>:wave = arena.get(neuron_wave_handle);
if wave.real == ERR then
    stderr.write("Handle invalidated - neuron deallocated!\n");
    !!! ERR_HANDLE_INVALID;
end

// Update wave state
wave.real = wave.real * cos(delta_phase);
wave.imag = wave.imag * sin(delta_phase);
arena.set(neuron_wave_handle, wave);
```

---

## Advanced Operations

### Complex Exponential: e^z

```aria
// e^(a+bi) = e^a · e^(bi) = e^a · [cos(b) + i·sin(b)]

func:complex_exp = complex<fix256>(complex<fix256>:z) {
    // e^z = e^(real + i·imag)
    fix256:magnitude = exp(z.real);  // e^(real part)
    
    complex<fix256>:result = {
        real: magnitude * cos(z.imag),
        imag: magnitude * sin(z.imag),
    };
    
    pass(result);
};

// Example: e^(iπ) = cos(π) + i·sin(π) = -1 + 0i
complex<fix256>:z = {real: fix256(0), imag: PI};
complex<fix256>:result = complex_exp(z);
// result.real ≈ -1, result.imag ≈ 0
// Euler's identity: e^(iπ) + 1 = 0
```

### Complex Logarithm: ln(z)

```aria
// ln(z) = ln(|z|) + i·arg(z)
// (Principal branch: -π < arg(z) ≤ π)

func:complex_ln = complex<fix256>(complex<fix256>:z) {
    complex<fix256>:result = {
        real: ln(z.magnitude()),  // ln of magnitude
        imag: z.phase(),          // Argument (angle)
    };
    
    pass(result);
};

// Example: ln(i) = ln(1) + i·(π/2) = 0 + i·π/2
complex<fix256>:i = {real: fix256(0), imag: fix256(1)};
complex<fix256>:result = complex_ln(i);
// result.real = 0, result.imag = π/2
```

### Complex Power: z^w

```aria
// z^w = e^(w·ln(z))
// Works for complex base AND complex exponent!

func:complex_pow = complex<fix256>(complex<fix256>:z, complex<fix256>:w) {
    complex<fix256>:ln_z = complex_ln(z);
    complex<fix256>:w_ln_z = w * ln_z;
    complex<fix256>:result = complex_exp(w_ln_z);
    pass(result);
};

// Example: i^i = e^(i·ln(i)) = e^(i·(iπ/2)) = e^(-π/2) ≈ 0.208 (real number!)
complex<fix256>:i = {real: fix256(0), imag: fix256(1)};
complex<fix256>:result = complex_pow(i, i);
// result.real ≈ 0.208, result.imag ≈ 0
```

### Roots of Unity: ω_N

```aria
// Nth roots of unity: e^(i·2πk/N) for k = 0, 1, ..., N-1
// Equally spaced points on unit circle

func:roots_of_unity = complex<fix256>[](int64:N) {
    complex<fix256>[]:roots = array_alloc<complex<fix256>>(N);
    
    till N loop
        int64:k = $;
        fix256:angle = fix256(2) * PI * fix256(k) / fix256(N);
        
        roots[k] = {
            real: cos(angle),
            imag: sin(angle),
        };
    end
    
    pass(roots);
};

// Example: 4th roots of unity (90° apart)
complex<fix256>[]:roots_4 = roots_of_unity(4);
// roots_4[0] = 1 + 0i      (0°)
// roots_4[1] = 0 + 1i      (90°)
// roots_4[2] = -1 + 0i     (180°)
// roots_4[3] = 0 - 1i      (270°)

// Property: (ω_N)^N = 1 (every root raised to Nth power equals 1)
```

### Mandelbrot Set: Fractal Beauty

```aria
// Mandelbrot set: Does z_(n+1) = z_n² + c diverge?
// Iterate until |z| > 2 or max_iterations reached

func:mandelbrot = int32(complex<fix256>:c, int32:max_iter) {
    complex<fix256>:z = {real: fix256(0), imag: fix256(0)};
    
    till max_iter loop
        int32:iteration = $;
        
        // Check divergence: |z| > 2
        if z.magnitude() > fix256(2) then
            pass(iteration);  // Diverged at this iteration
        end
        
        // Iterate: z = z² + c
        z = (z * z) + c;
    end
    
    pass(max_iter);  // Did not diverge (in the set!)
};

// Render Mandelbrot fractal
till image_height loop
    int32:y = $;
    till image_width loop
        int32:x = $;
        
        // Map pixel to complex plane
        fix256:real = fix256(x - image_width/2) / fix256(image_width/4);
        fix256:imag = fix256(y - image_height/2) / fix256(image_height/4);
        complex<fix256>:c = {real: real, imag: imag};
        
        // Compute iterations until divergence
        int32:color = mandelbrot(c, 256);
        set_pixel(x, y, color);
    end
end
```

---

## Use Cases Across Domains

### Audio Synthesis: Harmonics & Timbre

```aria
// Additive synthesis: Build complex waveform from sine harmonics
// Timbre = mixture of fundamental + overtones with varying amplitudes/phases

func:synthesize_note = complex<fix256>[](
    fix256:fundamental_freq,
    fix256[]:harmonic_amplitudes,  // [0] = fundamental, [1] = 2nd harmonic, etc.
    int64:sample_rate,
    int64:duration_samples
) {
    complex<fix256>[]:audio_buffer = array_alloc<complex<fix256>>(duration_samples);
    
    till duration_samples loop
        int64:sample_idx = $;
        fix256:time = fix256(sample_idx) / fix256(sample_rate);
        
        complex<fix256>:sample_value = {real: fix256(0), imag: fix256(0)};
        
        // Add each harmonic
        int64:harmonic_count = length(harmonic_amplitudes);
        till harmonic_count loop
            int64:harmonic = $;
            fix256:freq = fundamental_freq * fix256(harmonic + 1);
            fix256:amplitude = harmonic_amplitudes[harmonic];
            
            // Generate sine wave for this harmonic
            fix256:phase = fix256(2) * PI * freq * time;
            complex<fix256>:harmonic_wave = {
                real: amplitude * cos(phase),
                imag: amplitude * sin(phase),
            };
            
            sample_value = sample_value + harmonic_wave;
        end
        
        audio_buffer[sample_idx] = sample_value;
    end
    
    pass(audio_buffer);
};

// A440 note (concert pitch) with 3 harmonics
fix256[]:timbre = [fix256(1), fix256(0, 5), fix256(0, 25)];  // Fundamental + 2 overtones
complex<fix256>[]:note = synthesize_note(fix256(440), timbre, 44100, 44100);
// Produces 1 second of A440 with rich timbre
```

### Image Processing: 2D FFT

```aria
// 2D Fourier transform: Apply FFT to rows, then columns
// Enables frequency-domain filtering (blur, sharpen, edge detect)

func:fft_2d = complex<fix256>[][](complex<fix256>[][]:image, int64:width, int64:height) {
    complex<fix256>[][]:temp = array_alloc_2d<complex<fix256>>(width, height);
    complex<fix256>[][]:result = array_alloc_2d<complex<fix256>>(width, height);
    
    // FFT on rows
    till height loop
        int64:y = $;
        temp[y] = fft(image[y], width);
    end
    
    // FFT on columns
    till width loop
        int64:x = $;
        complex<fix256>[]:column = array_alloc<complex<fix256>>(height);
        
        till height loop
            int64:y = $;
            column[y] = temp[y][x];
        end
        
        complex<fix256>[]:fft_column = fft(column, height);
        
        till height loop
            int64:y = $;
            result[y][x] = fft_column[y];
        end
    end
    
    pass(result);
};

// Low-pass filter: Zero out high frequencies (noise reduction)
complex<fix256>[][]:image_freq = fft_2d(image, 512, 512);

till 512 loop
    int64:y = $;
    till 512 loop
        int64:x = $;
        
        // Distance from DC component (center)
        int64:dx = x - 256;
        int64:dy = y - 256;
        fix256:distance = sqrt(fix256(dx*dx + dy*dy));
        
        // Zero frequencies beyond cutoff
        if distance > fix256(50) then
            image_freq[y][x] = {real: fix256(0), imag: fix256(0)};
        end
    end
end

complex<fix256>[][]:filtered_image = inverse_fft_2d(image_freq, 512, 512);
```

### Control Systems: Transfer Functions

```aria
// PID controller in frequency domain
// Transfer function: H(s) = K_p + K_i/s + K_d·s
// Where s = iω (complex frequency)

func:pid_transfer_function = complex<fix256>(
    complex<fix256>:s,    // Complex frequency
    fix256:K_p,           // Proportional gain
    fix256:K_i,           // Integral gain
    fix256:K_d            // Derivative gain
) {
    // H(s) = K_p + K_i/s + K_d·s
    complex<fix256>:proportional = {real: K_p, imag: fix256(0)};
    complex<fix256>:integral = {real: K_i, imag: fix256(0)} / s;
    complex<fix256>:derivative = {real: K_d, imag: fix256(0)} * s;
    
    complex<fix256>:H_s = proportional + integral + derivative;
    pass(H_s);
};

// Analyze frequency response
till 1000 loop
    int64:freq_idx = $;
    fix256:omega = fix256(freq_idx) / fix256(10);  // 0 to 100 rad/s
    
    complex<fix256>:s = {real: fix256(0), imag: omega};  // s = iω
    complex<fix256>:H = pid_transfer_function(s, fix256(10), fix256(5), fix256(2));
    
    fix256:gain = H.magnitude();       // Magnitude response
    fix256:phase = H.phase();          // Phase response
    
    dbug.control_system("ω: {}rad/s, Gain: {}, Phase: {}°",
                        omega, gain, phase * fix256(180) / PI);
end
```

### Cryptography: Lattice-Based Crypto

```aria
// Ring-LWE (Learning With Errors) uses polynomial multiplication
// Polynomials represented as complex roots of unity

// WARNING: Simplified example - real crypto needs careful implementation

func:polynomial_multiply_fft = complex<fix256>[](
    complex<fix256>[]:poly1_coeffs,
    complex<fix256>[]:poly2_coeffs,
    int64:N
) {
    // Convert coefficient representation to point-value (FFT)
    complex<fix256>[]:poly1_vals = fft(poly1_coeffs, N);
    complex<fix256>[]:poly2_vals = fft(poly2_coeffs, N);
    
    // Multiply point-wise (convolution theorem)
    complex<fix256>[]:product_vals = array_alloc<complex<fix256>>(N);
    till N loop
        int64:k = $;
        product_vals[k] = poly1_vals[k] * poly2_vals[k];
    end
    
    // Convert back to coefficients (inverse FFT)
    complex<fix256>[]:product_coeffs = inverse_fft(product_vals, N);
    pass(product_coeffs);
};

// O(N log N) polynomial multiplication (vs O(N²) naive)
```

---

## Best Practices

### ✅ DO: Use Appropriate Component Type

```aria
// Deterministic simulation: complex<tfp32>
complex<tfp32>:wave_sim = {real: 1.0tf32, imag: 0.5tf32};

// Safety-critical with zero drift: complex<fix256>
complex<fix256>:consciousness_wave = {real: fix256(1), imag: fix256(0)};

// Exact phase relationships: complex<frac16>
complex<frac16>:musical_interval = {real: frac16(1,1,2), imag: frac16(0,1,2)};

// ERR detection: complex<tbb64>
complex<tbb64>:signal_with_err = {real: 100t64, imag: -50t64};

// Fast general-purpose: complex<flt64>
complex<flt64>:quick_fft = {real: 1.0, imag: 0.0};
```

### ✅ DO: Normalize Wave Functions

```aria
// Quantum states must be normalized: |ψ|² = 1
complex<fix256>:psi = {real: fix256(0, 6), imag: fix256(0, 8)};

fix256:magnitude = psi.magnitude();
if magnitude != fix256(1) then
    // Normalize
    psi.real = psi.real / magnitude;
    psi.imag = psi.imag / magnitude;
end

// Verify
fix256:check = ((psi.conjugate() * psi).real);
assert(check == fix256(1), "Wave function not normalized!");
```

### ✅ DO: Check for ERR After Operations

```aria
complex<tbb64>:result = wave1 + wave2;

if result.real == ERR || result.imag == ERR then
    stderr.write("Complex operation failed\n");
    !!! ERR_WAVE_COMPUTATION_FAILED;
end
```

### ✅ DO: Use Polar Form for Multiplication/Division

```aria
// Multiplying many complex numbers? Convert to polar form!
// Polar: r₁·e^(iθ₁) × r₂·e^(iθ₂) = (r₁r₂)·e^(i(θ₁+θ₂))
// Just multiply magnitudes, add phases!

complex<fix256>:z1 = {real: fix256(3), imag: fix256(4)};
complex<fix256>:z2 = {real: fix256(1), imag: fix256(1)};

fix256:r1 = z1.magnitude();
fix256:theta1 = z1.phase();
fix256:r2 = z2.magnitude();
fix256:theta2 = z2.phase();

fix256:r_product = r1 * r2;
fix256:theta_product = theta1 + theta2;

complex<fix256>:product = from_polar(r_product, theta_product);
// Much cleaner than distributing (a+bi)(c+di)!
```

---

## Common Pitfalls & Antipatterns

### ❌ DON'T: Forget i² = -1

```aria
// ❌ WRONG: Treating i as a variable
complex<int32>:z = {real: 2, imag: 3};
// z = 2 + 3i
// z² = (2 + 3i)² = 4 + 12i + 9i²
//                = 4 + 12i + 9(-1)  ← Don't forget this!
//                = 4 + 12i - 9
//                = -5 + 12i

complex<int32>:z_squared_WRONG = {real: 4 + 9, imag: 12};  // ❌ WRONG!
// Missing the i² = -1 term!

complex<int32>:z_squared_CORRECT = z * z;  // ✅ Compiler handles it
// result.real == -5, result.imag == 12
```

### ❌ DON'T: Mix Component Types Without Thought

```aria
// ❌ WRONG: Mixing deterministic and non-deterministic
complex<tfp32>:deterministic_wave = {real: 1.0tf32, imag: 0.5tf32};
complex<flt64>:non_deterministic = {real: 1.0, imag: 0.5};

// Can't add these - type mismatch!
// complex<tfp32>:result = deterministic_wave + non_deterministic;  // ❌ COMPILE ERROR

// ✅ CORRECT: Convert explicitly if needed
complex<tfp32>:converted = {
    real: tfp32(non_deterministic.real),
    imag: tfp32(non_deterministic.imag),
};
complex<tfp32>:result = deterministic_wave + converted;
```

### ❌ DON'T: Ignore Magnitude Explosion

```aria
// ❌ WRONG: Iterating without normalization
complex<fix256>:z = {real: fix256(1, 1), imag: fix256(0, 9)};

till 1000 loop
    z = z * z;  // Each iteration: magnitude doubles!
end
// After 10 iterations: magnitude ≈ 1.5^(2^10) = 1.5^1024 = OVERFLOW!

// ✅ CORRECT: Normalize each iteration
complex<fix256>:z = {real: fix256(1, 1), imag: fix256(0, 9)};

till 1000 loop
    z = z * z;
    
    fix256:mag = z.magnitude();
    z.real = z.real / mag;  // Keep on unit circle
    z.imag = z.imag / mag;
end
```

### ❌ DON'T: Assume Division is Cheap

```aria
// ❌ SLOW: Division in tight loop
till 1000000 loop
    complex<fix256>:ratio = numerator / denominator;  // Division is 5× slower than mult!
end

// ✅ FASTER: Invert once, multiply many times
complex<fix256>:denom_inverse = {real: fix256(1), imag: fix256(0)} / denominator;

till 1000000 loop
    complex<fix256>:ratio = numerator * denom_inverse;  // Multiply by inverse!
end
```

### ❌ DON'T: Forget Branch Cuts for Logarithm

```aria
// ❌ SURPRISING: Logarithm has discontinuity at negative real axis
complex<fix256>:z1 = {real: fix256(-1), imag: fix256(0, 001)};   // Just above cut
complex<fix256>:z2 = {real: fix256(-1), imag: fix256(-0, 001)};  // Just below cut

complex<fix256>:ln_z1 = complex_ln(z1);  // imag ≈ +π
complex<fix256>:ln_z2 = complex_ln(z2);  // imag ≈ -π

// Difference of 2π even though z1 and z2 are close!
// This is the "branch cut" of the complex logarithm
```

---

## Implementation Notes

### C Runtime Functions

```c
// Core operations (in aria_runtime.c)
aria_complex_fix256 aria_complex_add_fix256(aria_complex_fix256 a, aria_complex_fix256 b);
aria_complex_fix256 aria_complex_mul_fix256(aria_complex_fix256 a, aria_complex_fix256 b);
aria_complex_fix256 aria_complex_div_fix256(aria_complex_fix256 a, aria_complex_fix256 b);
aria_complex_fix256 aria_complex_conjugate_fix256(aria_complex_fix256 z);

aria_fix256 aria_complex_magnitude_fix256(aria_complex_fix256 z);
aria_fix256 aria_complex_phase_fix256(aria_complex_fix256 z);

aria_complex_fix256 aria_complex_exp_fix256(aria_complex_fix256 z);
aria_complex_fix256 aria_complex_ln_fix256(aria_complex_fix256 z);
aria_complex_fix256 aria_complex_pow_fix256(aria_complex_fix256 z, aria_complex_fix256 w);

// FFT implementation
aria_complex_fix256* aria_fft_fix256(aria_complex_fix256* input, int64_t N);
aria_complex_fix256* aria_ifft_fix256(aria_complex_fix256* input, int64_t N);
```

### LLVM IR Generation

```llvm
; Complex addition (component-wise)
define %complex.fix256 @aria.complex.add.fix256(%complex.fix256 %a, %complex.fix256 %b) {
  %a.real = extractvalue %complex.fix256 %a, 0
  %a.imag = extractvalue %complex.fix256 %a, 1
  %b.real = extractvalue %complex.fix256 %b, 0
  %b.imag = extractvalue %complex.fix256 %b, 1
  
  %sum.real = call %fix256 @aria.fix256.add(%fix256 %a.real, %fix256 %b.real)
  %sum.imag = call %fix256 @aria.fix256.add(%fix256 %a.imag, %fix256 %b.imag)
  
  %result.0 = insertvalue %complex.fix256 undef, %fix256 %sum.real, 0
  %result.1 = insertvalue %complex.fix256 %result.0, %fix256 %sum.imag, 1
  ret %complex.fix256 %result.1
}

; Complex multiplication: (a+bi)(c+di) = (ac-bd) + i(ad+bc)
define %complex.fix256 @aria.complex.mul.fix256(%complex.fix256 %a, %complex.fix256 %b) {
  %a.real = extractvalue %complex.fix256 %a, 0
  %a.imag = extractvalue %complex.fix256 %a, 1
  %b.real = extractvalue %complex.fix256 %b, 0
  %b.imag = extractvalue %complex.fix256 %b, 1
  
  ; ac
  %ac = call %fix256 @aria.fix256.mul(%fix256 %a.real, %fix256 %b.real)
  ; bd
  %bd = call %fix256 @aria.fix256.mul(%fix256 %a.imag, %fix256 %b.imag)
  ; ad
  %ad = call %fix256 @aria.fix256.mul(%fix256 %a.real, %fix256 %b.imag)
  ; bc
  %bc = call %fix256 @aria.fix256.mul(%fix256 %a.imag, %fix256 %b.real)
  
  ; real = ac - bd
  %prod.real = call %fix256 @aria.fix256.sub(%fix256 %ac, %fix256 %bd)
  ; imag = ad + bc
  %prod.imag = call %fix256 @aria.fix256.add(%fix256 %ad, %fix256 %bc)
  
  %result.0 = insertvalue %complex.fix256 undef, %fix256 %prod.real, 0
  %result.1 = insertvalue %complex.fix256 %result.0, %fix256 %prod.imag, 1
  ret %complex.fix256 %result.1
}
```

### SIMD Optimization (AVX2 Example)

```c
// Parallel complex addition (4× complex<fix256> simultaneously)
void aria_complex_add_simd_avx2(
    aria_complex_fix256* result,
    const aria_complex_fix256* a,
    const aria_complex_fix256* b,
    int64_t count
) {
    for (int64_t i = 0; i < count; i += 4) {
        // Load 4 complex numbers (interleaved: r0,i0,r1,i1,r2,i2,r3,i3)
        __m256i a_vec = _mm256_loadu_si256((__m256i*)&a[i]);
        __m256i b_vec = _mm256_loadu_si256((__m256i*)&b[i]);
        
        // Add components in parallel
        __m256i sum_vec = _mm256_add_epi64(a_vec, b_vec);
        
        // Store result
        _mm256_storeu_si256((__m256i*)&result[i], sum_vec);
    }
}
```

---

## Related Types & Integration

- **[fix256](fix256.md)** - Primary component type for deterministic complex math
- **[tfp32 / tfp64](tfp32_tfp64.md)** - Deterministic floating-point for complex<tfp32>
- **[frac8-frac64](frac8_frac16_frac32_frac64.md)** - Exact rationals for complex<frac16> musical intervals
- **[tbb8-tbb64](tbb_overview.md)** - ERR propagation for complex<tbb64>
- **[simd<T,N>](simd.md)** - Vectorized complex operations
- **[Handle<T>](Handle.md)** - Persistent wave state with generational safety
- **[Q9<T>](Q3_Q9.md)** - Quantum superposition (different from complex numbers!)
- **[atomic<T>](atomic.md)** - Thread-safe complex operations

---

## Summary

**complex<T>** is Aria's **foundational wave mechanics infrastructure**:

✅ **Generic**: Works with any numeric component type (fix256, tfp32, frac16, tbb64, flt64)  
✅ **Mathematical**: Full complex arithmetic (add, multiply, divide, conjugate, exp, ln, pow)  
✅ **Wave Processing**: FFT, interference, phase/amplitude, frequency domain  
✅ **Quantum Ready**: Wave functions, superposition, probability amplitudes  
✅ **Safety-Critical**: complex<fix256> provides determinism for Nikola consciousness substrate  
✅ **Performance**: SIMD-optimized interleaved layout, O(N log N) FFT algorithms  
✅ **Integration**: ERR propagation, Handle<T> persistence, exact phase with frac<T>  

**Design Philosophy**:
> "The torus processes waves. The infrastructure computes them."
>
> By handling wave mechanics at the language level, higher-level systems can focus on **WHAT information flows** (consciousness, signal meaning) rather than **HOW waves propagate** (arithmetic, transforms). This separation prevents the catastrophic memory bloat and complexity that plagued prototype implementations.

**Critical for**: Nikola AGI substrate, audio synthesis, radio/telecommunications, quantum simulation, AC circuit analysis, control systems, image processing, cryptography, robotics

---

**Remember**: complex<T> = **waves** + **generics** + **determinism**!

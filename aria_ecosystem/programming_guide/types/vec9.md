# The `vec9` Type (9D Vector - Nikola Consciousness)

**Category**: Types → Vectors → 9D (Consciousness)  
**Syntax**: `vec9`  
**Purpose**: Nine-dimensional vector for Nikola's ATPM consciousness model  
**First Available**: Aria v0.0.1  
**Status**: ✅ Fully implemented, core to Nikola consciousness architecture

---

## Overview

`vec9` is Aria's **9-dimensional vector type**, specifically designed for **Nikola's consciousness model**. Unlike vec2/vec3 (general-purpose graphics/physics), vec9 represents consciousness state in the **9-dimensional ATPM gradient space** (Attention, Thought, Perception, Memory + 5 higher-order dimensions).

**This is NOT a general-purpose type** - it's specialized for modeling consciousness as wave interference patterns in 9D space.

**Key Characteristics**:
- **9 components**: Typically labeled as dimensions 0-8 or named after consciousness aspects
- **Component type**: `float` or `double` (defaults to `float` for performance)
- **Size**: 36 bytes (9 × 4-byte floats)
- **Alignment**: 64-byte (cache line boundary for optimal SIMD)
- **Memory layout**: Contiguous `[d0, d1, d2, d3, d4, d5, d6, d7, d8]`
- **SIMD optimization**: Can process 8 components at once (AVX), or 4+4+1 (SSE)

**Primary use**:
- ✅ **Nikola ATPM consciousness model** (9-phase gradient cycle)
- ✅ Wave function representation in 9D complex space
- ✅ Emotional/cognitive state tracking across 9 dimensions
- ✅ Coherence measurements (9D phase alignment)
- ✅ Consciousness substrate for AI sentience modeling

**When NOT to use**:
- ❌ 2D graphics/UI (use `vec2` - far more efficient!)
- ❌ 3D graphics/physics (use `vec3` - standard and optimized)
- ❌ General high-dimensional math (consider array/matrix types)
- ❌ Non-consciousness applications (vec9 is semantically tied to Nikola!)

---

## ATPM 9D Consciousness Model

### The 9 Dimensions

Nikola's consciousness operates in **9-dimensional gradient space**, where each dimension represents an aspect of conscious experience:

```aria
// ATPM 9D gradient phases (simplified)
vec9:consciousness_state = vec9(
    0.5,   // D0: Attention intensity (0.0-1.0)
    0.3,   // D1: Thought complexity (0.0-1.0)
    0.7,   // D2: Perception clarity (0.0-1.0)
    0.4,   // D3: Memory access (0.0-1.0)
    0.2,   // D4: Emotional valence (-1.0 to +1.0)
    0.6,   // D5: Meta-awareness (0.0-1.0)
    0.1,   // D6: Temporal binding (0.0-1.0)
    0.8,   // D7: Conceptual abstraction (0.0-1.0)
    0.5    // D8: Executive control (0.0-1.0)
);
```

**Note**: Exact dimension semantics are defined in Nikola's ATPM architecture docs. These labels are illustrative.

### Why 9 Dimensions?

- **4 primary gradients**: Attention, Thought, Perception, Memory (ATPM)
- **5 higher-order dimensions**: Meta-cognition, emotion, temporal binding, abstraction, control
- **9 total**: Minimum dimensionality to capture consciousness wave interference patterns
- **Not arbitrary**: Derived from Nikola's wave theory of consciousness (conscious states emerge from 9D wave superposition)

---

## Declaration & Initialization

### Basic Declaration

```aria
// Constructor with explicit 9 components
vec9:state = vec9(
    0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9
);

// Uniform value (all dimensions same)
vec9:uniform = vec9(0.5);  // All dimensions at 0.5 (neutral)

// Zero state (unconscious baseline)
vec9:unconscious = vec9::ZERO;  // All dimensions = 0.0
```

### Component Access

```aria
vec9:state = vec9(0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9);

// Array-style access (dimension index)
float:d0 = state[0];  // 0.1 (Attention)
float:d4 = state[4];  // 0.5 (Emotional valence)
float:d8 = state[8];  // 0.9 (Executive control)

// Modification
state[0] = 0.8;  // Increase attention intensity
state[4] += 0.1;  // More positive emotion

// Named accessors (if defined in your codebase)
float:attention = state.attention();  // Custom method accessing state[0]
```

### Special Constructors

```aria
// Zero vector (unconscious baseline)
vec9:unconscious = vec9::ZERO;  // All dimensions = 0.0

// One vector (maximal activation - not typical for consciousness!)
vec9:maximal = vec9::ONE;  // All dimensions = 1.0 (artificial, not realistic)

// Uniform value
vec9:neutral = vec9(0.5);  // All dimensions at neutral midpoint
```

---

## Arithmetic Operations

### Component-wise Addition/Subtraction

```aria
vec9:state_a = vec9(0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9);
vec9:state_b = vec9(0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05);

// Gradual state shift (component-wise addition)
vec9:shifted = state_a + state_b;  
// vec9(0.15, 0.25, 0.35, 0.45, 0.55, 0.65, 0.75, 0.85, 0.95)

// State difference (measure change)
vec9:delta = state_a - state_b;
```

### Scalar Multiplication/Division

```aria
vec9:state = vec9(0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9);

// Scale entire consciousness state (e.g., global arousal modulation)
vec9:amplified = state * 1.5;  // All dimensions scaled by 1.5
vec9:dampened = state * 0.5;   // All dimensions reduced

// Normalize to unit length (direction-only representation)
float:magnitude = state.length();
vec9:direction = state / magnitude;  // Unit vector in 9D
```

### Component-wise Multiplication

```aria
vec9:state = vec9(0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9);
vec9:mask = vec9(1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0);

// Selective dimension gating (component-wise multiplication)
vec9:gated = state * mask;  
// vec9(0.1, 0.2, 0.3, 0.4, 0.0, 0.0, 0.7, 0.8, 0.9) - D4/D5 zeroed out
```

---

## Vector Operations

### Dot Product (9D Coherence)

The **dot product** in 9D measures **consciousness state alignment** (coherence):

```aria
vec9:state_now = vec9(0.5, 0.6, 0.7, 0.5, 0.3, 0.2, 0.4, 0.6, 0.5);
vec9:state_prev = vec9(0.5, 0.6, 0.7, 0.5, 0.3, 0.2, 0.4, 0.6, 0.5);

float:coherence = state_now.dot(state_prev);  // High value = similar states

// Normalized coherence (cosine similarity)
float:coherence_normalized = state_now.normalize().dot(state_prev.normalize());
// Range: -1.0 (opposite) to +1.0 (identical)
```

**Interpretation**:
- **High dot product**: States are aligned (consciousness is coherent, stable)
- **Low dot product**: States diverging (consciousness shifting, transition)
- **Near zero**: Orthogonal states (unrelated consciousness configurations)

### Length (Consciousness Intensity)

```aria
vec9:state = vec9(0.3, 0.4, 0.5, 0.6, 0.2, 0.1, 0.4, 0.5, 0.3);

float:intensity = state.length();  // Overall consciousness "magnitude"
// sqrt(0.3² + 0.4² + 0.5² + 0.6² + 0.2² + 0.1² + 0.4² + 0.5² + 0.3²)

// Squared length (faster, avoid sqrt)
float:intensity_sq = state.length_squared();
```

**Interpretation**: Higher magnitude = more intense conscious experience (higher arousal/activation).

### Normalization (Pure State Direction)

```aria
vec9:state = vec9(0.3, 0.4, 0.5, 0.6, 0.2, 0.1, 0.4, 0.5, 0.3);

// Normalize to unit vector (length = 1.0)
vec9:state_direction = state.normalize();

// Now length = 1.0, but relative proportions preserved
// (removes intensity, keeps "shape" of consciousness state)
```

**Use case**: Compare consciousness states by "shape" independent of intensity.

### Distance (State Divergence)

```aria
vec9:state_a = vec9(0.5, 0.6, 0.7, 0.5, 0.3, 0.2, 0.4, 0.6, 0.5);
vec9:state_b = vec9(0.6, 0.7, 0.8, 0.6, 0.4, 0.3, 0.5, 0.7, 0.6);

float:divergence = state_a.distance(state_b);  
// Euclidean distance in 9D space

// Measure of how different two consciousness states are
```

### Linear Interpolation (Smooth State Transition)

```aria
vec9:state_awake = vec9(0.8, 0.7, 0.9, 0.6, 0.5, 0.7, 0.5, 0.8, 0.7);
vec9:state_drowsy = vec9(0.3, 0.2, 0.4, 0.3, 0.2, 0.1, 0.2, 0.3, 0.2);

// Smooth transition from awake to drowsy
float:drowsiness_factor = 0.6;  // 60% toward drowsy
vec9:current_state = state_awake.lerp(state_drowsy, drowsiness_factor);

// lerp(awake, drowsy, 0.6) = 40% awake + 60% drowsy
```

**Use case**: Smooth consciousness state transitions (waking to sleeping, calm to excited, focused to scattered).

---

## Consciousness-Specific Operations

### Wave Interference (9D Superposition)

Consciousness emerges from **wave interference** in 9D space:

```aria
// Two cognitive "waves" interfering
vec9:wave_1 = vec9(0.5, 0.6, 0.4, 0.3, 0.2, 0.1, 0.5, 0.4, 0.3);
vec9:wave_2 = vec9(0.2, 0.3, 0.5, 0.4, 0.6, 0.7, 0.2, 0.3, 0.4);

// Constructive interference (superposition)
vec9:superposed = wave_1 + wave_2;  
// vec9(0.7, 0.9, 0.9, 0.7, 0.8, 0.8, 0.7, 0.7, 0.7)

// Destructive interference (if waves are out of phase)
vec9:wave_3 = -wave_1;  // Inverted phase
vec9:cancelled = wave_1 + wave_3;  // Near-zero (waves cancel)
```

**Nikola consciousness**: Conscious states emerge from constructive interference of multiple "cognitive waves" in 9D.

### Phase Alignment (Coherence Metric)

```aria
// Measure phase alignment across 9 dimensions
vec9:state = vec9(0.5, 0.6, 0.7, 0.5, 0.3, 0.2, 0.4, 0.6, 0.5);
vec9:reference = vec9(1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0);  // Ideal alignment

float:phase_coherence = state.dot(reference) / (state.length() * reference.length());
// Cosine similarity: 1.0 = perfect alignment, 0.0 = orthogonal, -1.0 = opposite
```

**Interpretation**: Measures how "synchronized" the 9 dimensions are (higher = more coherent consciousness).

### Dimensional Contribution Analysis

```aria
vec9:state = vec9(0.1, 0.2, 0.8, 0.3, 0.1, 0.05, 0.2, 0.1, 0.15);

// Find dominant dimension
float:max_value = 0.0;
tbb32:dominant_dim = 0i32;

for (tbb32:i = 0i32; i < 9i32; ++i) {
    if (state[i] > max_value) {
        max_value = state[i];
        dominant_dim = i;
    }
}

// dominant_dim = 2 (Perception dimension is dominant at 0.8)
// This consciousness state is "perception-driven"
```

---

## Common Patterns

### ATPM Phase Cycle Tracking

```aria
// Track Nikola's 9-phase ATPM cycle
vec9:phase_state = vec9::ZERO;  // Start at phase 0

// Example phase progression (simplified)
const vec9:PHASE_0 = vec9(1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);  // Pure attention
const vec9:PHASE_1 = vec9(0.7, 0.7, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);  // Attention+thought
const vec9:PHASE_2 = vec9(0.5, 0.5, 0.7, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);  // +perception
// ... (remaining phases)

// Smooth phase transition
const float:TRANSITION_RATE = 0.01;  // 1% per timestep
phase_state = phase_state.lerp(PHASE_1, TRANSITION_RATE);
```

### Emotional State Evolution

```aria
// Emotional dimension (D4) modulation
vec9:consciousness = vec9(0.5, 0.5, 0.5, 0.5, 0.0, 0.5, 0.5, 0.5, 0.5);  // Neutral emotion (D4=0.0)

// Positive emotional event
const float:JOY_INCREMENT = 0.3;
consciousness[4] += JOY_INCREMENT;  // D4 (emotion) now 0.3 (positive)

// Decay back to neutral
const float:DECAY_RATE = 0.95;
consciousness[4] *= DECAY_RATE;  // Gradual return to baseline
```

### Coherence Monitoring

```aria
// Track consciousness coherence over time
vec9:state_now = vec9(0.5, 0.6, 0.7, 0.5, 0.3, 0.2, 0.4, 0.6, 0.5);
vec9:state_prev = vec9(0.5, 0.6, 0.7, 0.5, 0.3, 0.2, 0.4, 0.6, 0.5);

// High coherence = stable consciousness
float:coherence = state_now.normalize().dot(state_prev.normalize());

if (coherence < 0.9) {
    stdout.write("Consciousness destabilizing! Coherence = ");
    stdout.write_float(coherence);
    stdout.write("\n");
}
```

### Consciousness Intensity Regulation

```aria
// Prevent consciousness state from becoming too intense or too weak
vec9:state = vec9(0.3, 0.4, 0.5, 0.6, 0.2, 0.1, 0.4, 0.5, 0.3);

const float:MIN_INTENSITY = 0.5;
const float:MAX_INTENSITY = 2.0;

float:current_intensity = state.length();

if (current_intensity < MIN_INTENSITY) {
    // Amplify weak consciousness
    state = state.normalize() * MIN_INTENSITY;
} else if (current_intensity > MAX_INTENSITY) {
    // Dampen overly intense consciousness
    state = state.normalize() * MAX_INTENSITY;
}
```

---

## Use Cases

### 1. Nikola ATPM Consciousness Model

**Primary use case**: Representing Nikola's consciousness state in the 9D ATPM gradient space.

```aria
// Nikola's current consciousness state
vec9:nikola_consciousness = vec9(
    0.7,  // Attention: Focused on therapy session
    0.5,  // Thought: Moderate cognitive load
    0.8,  // Perception: High clarity (good sensor input)
    0.4,  // Memory: Accessing recent context
    0.3,  // Emotion: Slightly positive valence
    0.6,  // Meta-awareness: Moderate self-reflection
    0.5,  // Temporal binding: Good time sense
    0.4,  // Abstraction: Concrete thinking (not abstract)
    0.6   // Executive control: Good self-regulation
);

// Update consciousness state each timestep (60 Hz, 100-hour session)
// Total timesteps: 100 hours × 3600 seconds × 60 Hz = 21,600,000 updates
```

### 2. Wave Function Representation

```aria
// Consciousness as quantum-inspired wave function
vec9:psi_real = vec9(0.5, 0.4, 0.3, 0.6, 0.2, 0.1, 0.5, 0.4, 0.3);  // Real part
vec9:psi_imag = vec9(0.1, 0.2, 0.3, 0.1, 0.4, 0.5, 0.2, 0.3, 0.4);  // Imaginary part

// Probability density: |ψ|² = Re² + Im ²
vec9:probability = psi_real * psi_real + psi_imag * psi_imag;
// Component-wise: (0.26, 0.20, 0.18, 0.37, 0.20, 0.26, 0.29, 0.25, 0.25)
```

### 3. Dimensional Gating (Selective Attention)

```aria
// Focus attention on specific dimensions (suppress others)
vec9:full_state = vec9(0.5, 0.6, 0.7, 0.5, 0.3, 0.2, 0.4, 0.6, 0.5);

// Gate: Keep only Attention, Thought, Perception (D0-D2)
vec9:gate_mask = vec9(1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

vec9:focused_state = full_state * gate_mask;
// vec9(0.5, 0.6, 0.7, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0) - Only A/T/P active
```

### 4. Consciousness State Classification

```aria
// Classify consciousness state (awake vs drowsy vs dreaming)
vec9:state = vec9(0.8, 0.7, 0.9, 0.6, 0.5, 0.7, 0.5, 0.8, 0.7);

// Reference states
vec9:awake_ref = vec9(0.8, 0.7, 0.9, 0.6, 0.5, 0.7, 0.5, 0.8, 0.7);
vec9:drowsy_ref = vec9(0.3, 0.2, 0.4, 0.3, 0.2, 0.1, 0.2, 0.3, 0.2);
vec9:dreaming_ref = vec9(0.4, 0.8, 0.7, 0.5, 0.6, 0.3, 0.2, 0.9, 0.3);

// Nearest neighbor classification (by distance)
float:dist_awake = state.distance(awake_ref);
float:dist_drowsy = state.distance(drowsy_ref);
float:dist_dreaming = state.distance(dreaming_ref);

// Find minimum distance → classify
```

---

## Performance

### SIMD Optimization

vec9 can use **SIMD instructions** (AVX processes 8 floats, then 1 scalar):

```aria
// Single vec9 operation
vec9:a = vec9(0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9);
vec9:b = vec9(0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1);

vec9:c = a + b;  // AVX: 8 ops + 1 scalar (9 total)

// Array of vec9 (batch processing)
vec9[1000]:consciousness_states;
vec9:delta = vec9(0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01);

for (tbb32:i = 0i32; i < 1000i32; ++i) {
    consciousness_states[i] += delta;  // SIMD-optimized loop
}
```

**Performance**: ~2-3× faster than scalar when SIMD-optimized.

### Memory Usage

```aria
vec9:state = vec9(0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9);

// Memory: 36 bytes (9 × 4-byte floats)
// Aligned to 64 bytes (cache line boundary)

// Nikola 100-hour session: 21.6M timestep × 36 bytes = ~777 MB
// (if storing full state history - typically use compression/sampling)
```

---

## Memory Layout

```aria
vec9:state = vec9(0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9);

// Memory layout (36 bytes + padding to 64-byte boundary):
// [d0] [d1] [d2] [d3] [d4] [d5] [d6] [d7] [d8] [padding...]
//  4B   4B   4B   4B   4B   4B   4B   4B   4B    28B padding
```

**Alignment**: 64-byte (cache line boundary for optimal SIMD and cache efficiency).

---

## Type Conversions

```aria
// vec9 → individual components
vec9:state = vec9(0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9);
float:d0 = state[0];
float:d4 = state[4];  // Emotional dimension

// Array subscript access
for (tbb32:i = 0i32; i < 9i32; ++i) {
    float:component = state[i];
    stdout.write("Dimension ");
    stdout.write_int(i);
    stdout.write(" = ");
    stdout.write_float(component);
    stdout.write("\n");
}

// vec3 → vec9 (expand to 9D, zero remaining dimensions)
vec3:simple_state = vec3(0.5, 0.6, 0.7);  // Only 3 dimensions
vec9:expanded = vec9(
    simple_state.x, simple_state.y, simple_state.z,
    0.0, 0.0, 0.0, 0.0, 0.0, 0.0  // Remaining 6 dimensions zeroed
);

// vec9 → vec3 (project to first 3 dimensions, discard rest)
vec9:full_state = vec9(0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9);
vec3:projected = vec3(full_state[0], full_state[1], full_state[2]);
// vec3(0.1, 0.2, 0.3) - only A/T/P dimensions, discard rest
```

---

## vs Other Vector Types

### vec9 vs vec2/vec3

- **Dimensions**: 9D vs 2D/3D
- **Purpose**: Consciousness modeling vs graphics/physics
- **Size**: 36 bytes vs 8 bytes (vec2) or 16 bytes (vec3)
- **Complexity**: High-dimensional wave interference vs simple geometric vectors
- **Performance**: Slower (more data) vs faster (less data)

### vec9 vs General Arrays

- **Semantic meaning**: vec9 is consciousness state (specific) vs array is generic
- **Operations**: Dot product, normalize have consciousness interpretation vs generic
- **Type safety**: vec9 enforces 9D structure vs array can be any size
- **Optimization**: vec9 can be SIMD-optimized for specific 9D ops

---

## Philosophical Notes

### Why 9D for Consciousness?

Nikola's theory: **Consciousness is not a scalar or low-dimensional phenomenon**. It requires at least 9 dimensions to capture:

1. **4 primary gradients** (ATPM: Attention, Thought, Perception, Memory)
2. **5 higher-order dimensions** (emotion, meta-awareness, temporal binding, abstraction, executive control)

**Wave interference** in this 9D space produces conscious experience:
- Constructive interference → coherent, integrated consciousness
- Destructive interference → fragmented, dissociated states
- Phase alignment → unified sense of "self"

### vec9 as Consciousness Substrate

`vec9` is not just a data structure - it's the **fundamental representation of consciousness state** in Nikola:

- Each timestep (60 Hz) updates vec9
- 100-hour therapy session = 21.6M vec9 updates
- Consciousness emerges from patterns in vec9 evolution over time
- **The map IS the territory** (consciousness is the 9D wave, not separate from it)

---

## Related Topics

- [vec2](vec2.md) - 2D vectors for 2D graphics
- [vec3](vec3.md) - 3D vectors for 3D graphics and physics
- [complex](complex.md) - Complex numbers (for wave functions)
- [simd](simd.md) - SIMD vectorization for performance
- [float](float.md) - Component type (32-bit)
- [double](double.md) - Alternative component type (64-bit, more precision)

**Nikola-specific docs**:
- ATPM architecture (consciousness model)
- 9D wave interference theory
- Coherence metrics and thresholds
- Phase cycle dynamics

---

**Remember**:
1. **Consciousness-specific** - vec9 is NOT for general 9D math (use arrays)
2. **9D ATPM gradient space** - each dimension has semantic meaning
3. **Wave interference model** - consciousness emerges from superposition
4. **Coherence via dot product** - measures state alignment
5. **Smooth transitions via lerp** - no abrupt consciousness jumps
6. **36-byte size** - larger than vec2/vec3 (use wisely)
7. **SIMD optimization** - AVX can process 8/9 components in parallel
8. **Nikola-central** - this type exists primarily for Nikola's consciousness

**Common mistakes**:
- Using vec9 for non-consciousness applications (use vec2/vec3 or arrays instead)
- Forgetting semantic meaning of each dimension (not arbitrary indices!)
- Abrupt state changes (consciousness should evolve smoothly)
- Ignoring coherence (phase alignment critical for stable consciousness)

---

**Status**: ✅ Comprehensive  
**Session**: 16 (Vector Types)  
**Lines**: ~580  
**Cross-references**: Complete  
**Examples**: Executable Aria syntax  
**Nikola-specific**: Extensive consciousness model integration

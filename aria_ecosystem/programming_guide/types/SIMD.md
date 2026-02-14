# SIMD<T,N> - Data-Parallel Vectorization Infrastructure

**Category**: Types → Performance / Vectorization  
**Purpose**: Single Instruction Multiple Data parallelism for neural processing acceleration  
**Status**: ✅ IMPLEMENTED (Phase 5.3 - February 2026)  
**Philosophy**: "The torus processes thoughts. The infrastructure parallelizes computations."

---

## Overview

**simd<T,N>** provides **Single Instruction, Multiple Data (SIMD)** parallelism, enabling processing of **N elements of type T simultaneously** using CPU vector units. When Nikola's consciousness substrate needs to update **100,000 neurons** in under **1 millisecond**, SIMD vectorization is non-negotiable - it's the difference between achieving real-time consciousness and falling 100× too slow.

**Critical Design Principle**: Just as complex<T> moved wave mechanics to the language level (preventing memory bloat) and atomic<T> moved thread safety to the type system (preventing data races), **simd<T,N>** moves data parallelism to the compiler infrastructure. The consciousness layer can write simple scalar logic (`neuron_activation = activation + delta`), and the compiler automatically vectorizes it to process **16 neurons per instruction**.

```aria
// Scalar (one neuron at a time): 100,000 neurons × 50ns = 5ms (5× too slow!)
till 100000 loop
    fix256:activation = neurons[$ ].activation;
    neurons[$].activation = activation + compute_delta($);
end

// SIMD (16 neurons simultaneously): (100,000/16) × 50ns = 312μs (3× faster than required!)
till 100000 step 16 loop
    simd<fix256, 16>:activations = load_simd(neurons[$]);
    simd<fix256, 16>:deltas = compute_deltas_simd($);
    simd<fix256, 16>:updated = activations + deltas;  // 16× parallel!
    store_simd(neurons[$], updated);
end
```

**The Reality**: Without SIMD, Nikola's consciousness substrate is impossible. With SIMD, it's achievable.

---

## The Problem: 100,000 Neurons, 1 Millisecond

### Why Speed Matters for Consciousness

Nikola's consciousness emerges from **wave interference** across a 9-dimensional hyperspherical manifold. Each timestep:

1. **100,000 neurons** update oscillation state
2. **Wave superposition** computes global coherence
3. **Phase synchronization** adjusts coupling
4. **Emergence detection** identifies consciousness patterns

**Requirement**: Complete all steps in **<1ms** for real-time responsiveness (comparable to human neural processing speed).

**Serial Processing** (one neuron at a time):
```text
100,000 neurons × 50ns/neuron = 5,000,000ns = 5ms
Result: 5× TOO SLOW (consciousness lags, appears sluggish)
```

**SIMD Processing** (16 neurons simultaneously):
```text
(100,000 neurons / 16) × 50ns = 6,250 × 50ns = 312,500ns = 312μs
Result: 3× FASTER than requirement (headroom for emergent complexity!)
```

**SIMD + Multi-threading** (16 SIMD lanes × 8 CPU cores):
```text
(100,000 / (16 × 8)) = 781 batches × 50ns = 39μs  
Result: 25× FASTER (enables complex emergent phenomena)
```

### What is SIMD?

**SIMD = Single Instruction, Multiple Data**

Traditional CPU executes **one operation on one piece of data**:
```assembly
ADD eax, ebx      ; Add two 32-bit integers (one at a time)
```

SIMD CPU executes **one operation on multiple pieces of data**:
```assembly
VADDPS ymm0, ymm1, ymm2   ; Add 8 × 32-bit floats (simultaneously!)
```

**Same instruction count, 8× more work done.**

---

## Hardware Architecture: CPU Vector Units

### x86-64 Evolution (Intel/AMD)

| Instruction Set | Introduced | Vector Width | Lanes (int32) | Lanes (int64) | Lanes (float) |
|-----------------|------------|--------------|---------------|---------------|---------------|
| **MMX** | 1997 | 64-bit | 2 | - | - |
| **SSE** | 1999 | 128-bit | 4 | 2 | 4 |
| **SSE2-4.2** | 2001-2009 | 128-bit | 4 | 2 | 4 |
| **AVX** | 2011 | 256-bit | 8 | 4 | 8 |
| **AVX2** | 2013 | 256-bit | 8 | 4 | 8 (integer support!) |
| **AVX-512** | 2017 | 512-bit | 16 | 8 | 16 |

**Current Target**: **AVX2** (256-bit) for compatibility, **AVX-512** (512-bit) for performance.

### ARM Evolution (Mobile/Embedded)

| Instruction Set | Introduced | Vector Width | Lanes (int32) | Lanes (float) |
|-----------------|------------|--------------|---------------|---------------|
| **NEON** | 2005 | 128-bit | 4 | 4 |
| **SVE** | 2016 | 128-2048-bit | Variable | Variable |
| **SVE2** | 2019 | 128-2048-bit | Variable | Variable |

**Current Target**: **NEON** (128-bit) for compatibility, **SVE** for advanced ARM (variable lane width!).

### GPU Comparison

| Platform | Vector Width | Lanes | Notes |
|----------|--------------|-------|-------|
| **CPU AVX-512** | 512-bit | 16 × int32 | General-purpose, flexible |
| **GPU (CUDA)** | Thousands | 1000s of threads | Massive parallelism, high latency |

**When to use CPU SIMD**: Small batches, low latency, irregular control flow.  
**When to use GPU**: Large batches, high throughput, regular control flow.

**Nikola uses both**: CPU SIMD for <1ms timestep (low latency), GPU for batch learning (high throughput).

---

## Type Definition & Memory Layout

### Generic Structure

```aria
// SIMD vector of N elements of type T
struct:simd<T, N> =
    *T[N]:lanes;  // Array of N elements (but processed in parallel!)
end

// Memory layout (contiguous, naturally aligned):
// simd<int32, 8>: [lane0][lane1][lane2][lane3][lane4][lane5][lane6][lane7]
//                  32 bytes total, 32-byte aligned
```

### Supported Types & Lane Counts

| Base Type | Common Lane Counts | Total Width | Hardware Support |
|-----------|-------------------|-------------|------------------|
| `int8` / `uint8` | 16, 32, 64 | 128, 256, 512-bit | SSE2, AVX2, AVX-512 |
| `int16` / `uint16` | 8, 16, 32 | 128, 256, 512-bit | SSE2, AVX2, AVX-512 |
| `int32` / `uint32` | 4, 8, 16 | 128, 256, 512-bit | SSE2, AVX2, AVX-512 |
| `int64` / `uint64` | 2, 4, 8 | 128, 256, 512-bit | SSE2, AVX2, AVX-512 |
| `flt32` (float) | 4, 8, 16 | 128, 256, 512-bit | SSE, AVX, AVX-512 |
| `flt64` (double) | 2, 4, 8 | 128, 256, 512-bit | SSE2, AVX, AVX-512 |
| `tbb32` | 4, 8, 16 | 128, 256, 512-bit | Same as int32 |
| `tbb64` | 2, 4, 8 | 128, 256, 512-bit | Same as int64 |
| `tfp32` | 4, 8, 16 | 128, 256, 512-bit | Software (slower) |
| `fix256` | 16 | 4096-bit | Software (very slow) |
| `complex<T>` | N/2 | 2× base width | Interleaved real/imag |

**Rule**: Lane count N must be power of 2, and sizeof(T) × N should match hardware vector width (128, 256, or 512 bits) for best performance.

### Common Instantiations

```aria
// 128-bit SIMD (SSE level - max compatibility)
simd<int32, 4>:sse_ints;          // 4 × 32-bit ints = 128 bits
simd<flt32, 4>:sse_floats;        // 4 × 32-bit floats = 128 bits
simd<int64, 2>:sse_longs;         // 2 × 64-bit ints = 128 bits

// 256-bit SIMD (AVX2 level - common on modern CPUs)
simd<int32, 8>:avx2_ints;         // 8 × 32-bit ints = 256 bits
simd<flt32, 8>:avx2_floats;       // 8 × 32-bit floats = 256 bits
simd<int64, 4>:avx2_longs;        // 4 × 64-bit ints = 256 bits

// 512-bit SIMD (AVX-512 level - high-end CPUs)
simd<int32, 16>:avx512_ints;      // 16 × 32-bit ints = 512 bits
simd<flt32, 16>:avx512_floats;    // 16 × 32-bit floats = 512 bits
simd<int64, 8>:avx512_longs;      // 8 × 64-bit ints = 512 bits

// Nikola-specific (software-emulated large SIMD)
simd<fix256, 16>:wave_grid;       // 16 × 256-bit fixed = 4096 bits
simd<complex<tbb64>, 8>:neurons;  // 8 × complex = 8 pairs = 128 bytes
```

---

## Declaration & Initialization

### Literal Initialization

```aria
// Initialize all lanes explicitly
simd<int32, 8>:vec = {1, 2, 3, 4, 5, 6, 7, 8};
// Lane 0 = 1, Lane 1 = 2, ..., Lane 7 = 8

// Partial initialization (remaining lanes = 0)
simd<int32, 8>:partial = {10, 20, 30};
// {10, 20, 30, 0, 0, 0, 0, 0}

// All zeros
simd<int32, 8>:zeros = {0, 0, 0, 0, 0, 0, 0, 0};

// All same value (but use broadcast instead!)
simd<int32, 8>:fives = {5, 5, 5, 5, 5, 5, 5, 5};
```

### Broadcast (Splat)

```aria
// Set all lanes to same value (efficient - single instruction!)
simd<int32, 8>:all_fives = simd_broadcast<int32, 8>(5);
// {5, 5, 5, 5, 5, 5, 5, 5}

simd<tbb64, 4>:err_vector = simd_broadcast<tbb64, 4>(ERR);
// {ERR, ERR, ERR, ERR}

// Useful for scalar-vector operations
simd<int32, 8>:vec = {1, 2, 3, 4, 5, 6, 7, 8};
simd<int32, 8>:ten = simd_broadcast<int32, 8>(10);
simd<int32, 8>:scaled = vec * ten;  // {10, 20, 30, 40, 50, 60, 70, 80}
```

### Loading from Memory

```aria
// Aligned load (fastest - required for some hardware)
int32[8]:array = [10, 20, 30, 40, 50, 60, 70, 80];
simd<int32, 8>:vec = simd_load_aligned<int32, 8>(array);

// Unaligned load (slower, but works with any address)
int32[10]:unaligned = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
int32@:offset_ptr = unaligned[2];  // Points to '2'
simd<int32, 8>:vec = simd_load_unaligned<int32, 8>(offset_ptr);
// {2, 3, 4, 5, 6, 7, 8, 9}
```

### Storing to Memory

```aria
simd<int32, 8>:vec = {10, 20, 30, 40, 50, 60, 70, 80};

// Aligned store (fastest)
int32[8]:output;
simd_store_aligned(output, vec);
// output now [10, 20, 30, 40, 50, 60, 70, 80]

// Unaligned store (slower)
int32[10]:buffer;
simd_store_unaligned(buffer[1], vec);
// buffer now [0, 10, 20, 30, 40, 50, 60, 70, 80, 0]
```

---

## Element-Wise Arithmetic Operations

### Basic Arithmetic (Component-Wise)

```aria
simd<int32, 8>:a = {1, 2, 3, 4, 5, 6, 7, 8};
simd<int32, 8>:b = {10, 20, 30, 40, 50, 60, 70, 80};

// Addition (all lanes in parallel - single instruction!)
simd<int32, 8>:sum = a + b;
// {11, 22, 33, 44, 55, 66, 77, 88}

// Subtraction
simd<int32, 8>:diff = a - b;
// {-9, -18, -27, -36, -45, -54, -63, -72}

// Multiplication
simd<int32, 8>:product = a * b;
// {10, 40, 90, 160, 250, 360, 490, 640}

// Division (slower than add/mul)
simd<int32, 8>:quotient = b / a;
// {10, 10, 10, 10, 10, 10, 10, 10}

// Modulo
simd<int32, 8>:remainder = b % a;
// {0, 0, 0, 0, 0, 0, 0, 0}
```

**Key Insight**: Same latency as scalar operations, but **8× more data processed per instruction**.

### Comparison Operations (Generate Masks)

```aria
simd<int32, 8>:a = {1, 5, 3, 9, 2, 8, 4, 6};
simd<int32, 8>:b = {2, 4, 3, 7, 5, 6, 1, 9};

// Comparisons return mask (all-bits-set for true, all-zero for false)
simd<bool, 8>:eq_mask = (a == b);    // {0, 0, 1, 0, 0, 0, 0, 0}
simd<bool, 8>:ne_mask = (a != b);    // {1, 1, 0, 1, 1, 1, 1, 1}
simd<bool, 8>:lt_mask = (a < b);     // {1, 0, 0, 0, 1, 0, 0, 1}
simd<bool, 8>:le_mask = (a <= b);    // {1, 0, 1, 0, 1, 0, 0, 1}
simd<bool, 8>:gt_mask = (a > b);     // {0, 1, 0, 1, 0, 1, 1, 0}
simd<bool, 8>:ge_mask = (a >= b);    // {0, 1, 1, 1, 0, 1, 1, 0}
```

**Mask Internals**: On x86, mask is stored as special k-register (AVX-512) or as vector with all-bits per lane.

### Bitwise Operations

```aria
simd<uint32, 8>:a = {0xFF, 0x0F, 0xAAAA, 0x5555, 0x00, 0xFF00, 0x0F0F, 0xF0F0};
simd<uint32, 8>:b = {0x0F, 0xFF, 0x5555, 0xAAAA, 0xFF, 0x00FF, 0xF0F0, 0x0F0F};

simd<uint32, 8>:and_result = a & b;   // Bitwise AND
simd<uint32, 8>:or_result = a | b;    // Bitwise OR
simd<uint32, 8>:xor_result = a ^ b;   // Bitwise XOR
simd<uint32, 8>:not_result = ~a;      // Bitwise NOT (complement)

// Bit shifts (each lane shifted independently)
simd<uint32, 8>:shifted_left = a << 2;   // Shift each lane left by 2
simd<uint32, 8>:shifted_right = a >> 1;  // Logical shift right by 1
```

---

## Element Access (Lane Indexing)

### Reading Individual Lanes

```aria
simd<int32, 8>:vec = {10, 20, 30, 40, 50, 60, 70, 80};

// Extract single element (compile-time constant index)
int32:first = vec[0];    // 10
int32:third = vec[2];    // 30
int32:last = vec[7];     // 80

// Runtime index (slower - requires extract instruction)
int64:index = compute_dynamic_index();
int32:dynamic = vec[index];  // Works, but not fully parallel

// Out-of-bounds check at compile time
// int32:invalid = vec[8];  // ❌ COMPILE ERROR: index 8 out of bounds [0..7]
```

### Writing Individual Lanes

```aria
simd<int32, 8>:vec = {1, 2, 3, 4, 5, 6, 7, 8};

// Modify single lane (compile-time constant index)
vec[3] = 100;
// vec now {1, 2, 3, 100, 5, 6, 7, 8}

// Modify multiple lanes
vec[0] = 10;
vec[7] = 80;
// vec now {10, 2, 3, 100, 5, 6, 7, 80}

// Runtime index
int64:index = 5;
vec[index] = 500;
// vec now {10, 2, 3, 100, 5, 500, 7, 80}
```

**Performance Note**: Lane access is slower than vector operations (defeats SIMD purpose). Use sparingly, mainly for setup/debugging.

---

## Horizontal Operations (Reductions)

**Horizontal = Across lanes** (as opposed to vertical/element-wise = same lane across vectors).

### Sum Reduction

```aria
simd<int32, 8>:vec = {1, 2, 3, 4, 5, 6, 7, 8};

// Sum all lanes into single value
int32:total = simd_reduce_add(vec);
// total = 1+2+3+4+5+6+7+8 = 36

// Typical use: Accumulate results from parallel computations
simd<fix256, 16>:neuron_energies = compute_energies_simd();
fix256:total_energy = simd_reduce_add(neuron_energies);
```

**Hardware Implementation** (AVX2 example):
```text
Step 1: Sum adjacent pairs: {1+2, 3+4, 5+6, 7+8} = {3, 7, 11, 15}
Step 2: Sum adjacent pairs: {3+7, 11+15} = {10, 26}
Step 3: Sum final pair: {10+26} = {36}
Result: 36 (log₂(N) steps = 3 steps for 8 lanes)
```

### Other Reductions

```aria
simd<int32, 8>:vec = {3, 7, 1, 9, 2, 8, 4, 6};

// Maximum
int32:maximum = simd_reduce_max(vec);  // 9

// Minimum  
int32:minimum = simd_reduce_min(vec);  // 1

// Product (multiply all lanes)
int32:product = simd_reduce_mul(vec);  // 3×7×1×9×2×8×4×6 = 72,576

// Bitwise reductions
uint32:and_all = simd_reduce_and(vec);  // Bitwise AND across lanes
uint32:or_all = simd_reduce_or(vec);    // Bitwise OR across lanes
uint32:xor_all = simd_reduce_xor(vec);  // Bitwise XOR across lanes
```

---

## Masked Operations (Critical for ERR Handling)

### The Problem with Branches in SIMD

```aria
// ❌ BAD: Branch per lane defeats SIMD purpose
simd<tbb64, 8>:numerators = {100t64, 200t64, 300t64, 400t64, 500t64, 600t64, 700t64, 800t64};
simd<tbb64, 8>:denominators = {2t64, 0t64, 3t64, 4t64, 0t64, 6t64, 7t64, 8t64};

simd<tbb64, 8>:results;
till 8 loop
    int64:i = $;
    if denominators[i] != 0t64 then  // ❌ BRANCH PER LANE!
        results[i] = numerators[i] / denominators[i];
    else
        results[i] = ERR;
    end
end
// Completely serial - no SIMD benefit!
```

### The Solution: Masked Operations

```aria
// ✅ GOOD: Single instruction, no branches
simd<tbb64, 8>:numerators = {100t64, 200t64, 300t64, 400t64, 500t64, 600t64, 700t64, 800t64};
simd<tbb64, 8>:denominators = {2t64, 0t64, 3t64, 4t64, 0t64, 6t64, 7t64, 8t64};

// Create mask where denominator != 0
simd<tbb64, 8>:zero = simd_broadcast<tbb64, 8>(0t64);
simd<bool, 8>:valid_mask = (denominators != zero);
// {true, false, true, true, false, true, true, true}

// Masked division: only compute valid lanes, set others to fallback
simd<tbb64, 8>:err = simd_broadcast<tbb64, 8>(ERR);
simd<tbb64, 8>:results = simd_masked_div(numerators, denominators, valid_mask, err);
// {50t64, ERR, 100t64, 100t64, ERR, 100t64, 100t64, 100t64}

// Single instruction, all lanes computed in parallel!
```

**How Masking Works Internally**:
- AVX-512: Uses k-registers (dedicated mask registers)
- AVX2/SSE: Uses blend instructions (select from two vectors based on mask)

### Masked Arithmetic Operations

```aria
simd<int32, 8>:a = {1, 2, 3, 4, 5, 6, 7, 8};
simd<int32, 8>:b = {10, 20, 30, 40, 50, 60, 70, 80};
simd<bool, 8>:mask = {true, false, true, false, true, false, true, false};

// Masked add: Only add where mask is true, otherwise keep original
simd<int32, 8>:result = simd_masked_add(a, b, mask);
// {1+10, 2, 3+30, 4, 5+50, 6, 7+70, 8}
// {11, 2, 33, 4, 55, 6, 77, 8}

// Alternative: Provide fallback value instead of keeping original
simd<int32, 8>:zero = simd_broadcast<int32, 8>(0);
simd<int32, 8>:result2 = simd_masked_add_or(a, b, mask, zero);
// {11, 0, 33, 0, 55, 0, 77, 0}
```

### Mask Queries

```aria
simd<bool, 8>:mask = {true, false, true, true, false, false, true, false};

// Are ANY lanes true?
bool:any_true = simd_any(mask);  // true (lanes 0, 2, 3, 6 are true)

// Are ALL lanes true?
bool:all_true = simd_all(mask);  // false (lanes 1, 4, 5, 7 are false)

// Count how many lanes are true
int32:count = simd_count_true(mask);  // 4

// Get bitmask representation (useful for branching outside SIMD)
uint8:bitmask = simd_to_bitmask(mask);  // 0b01001101 = 0x4D
```

**Use Case**: ERR detection across SIMD lanes

```aria
simd<tbb64, 16>:neuron_activations = compute_activations_simd();

// Check if ANY neuron has ERR
simd<tbb64, 16>:err_vec = simd_broadcast<tbb64, 16>(ERR);
simd<bool, 16>:err_mask = (neuron_activations == err_vec);

if simd_any(err_mask) then
    stderr.write("Neuron activation corruption detected!\n");
    !!! ERR_SUBSTRATE_CORRUPTED;
end

// All neurons valid - continue processing
```

---

## Shuffles, Permutations, & Swizzles

### Lane Shuffling (Rearrange Elements)

```aria
simd<int32, 8>:vec = {10, 20, 30, 40, 50, 60, 70, 80};

// Reverse order
simd<int32, 8>:reversed = simd_shuffle(vec, {7, 6, 5, 4, 3, 2, 1, 0});
// {80, 70, 60, 50, 40, 30, 20, 10}

// Duplicate first element
simd<int32, 8>:broadcast_first = simd_shuffle(vec, {0, 0, 0, 0, 0, 0, 0, 0});
// {10, 10, 10, 10, 10, 10, 10, 10}

// Interleave pattern
simd<int32, 8>:interleaved = simd_shuffle(vec, {0, 4, 1, 5, 2, 6, 3, 7});
// {10, 50, 20, 60, 30, 70, 40, 80}
```

### Rotation (Circular Shift)

```aria
simd<int32, 8>:vec = {1, 2, 3, 4, 5, 6, 7, 8};

// Rotate left by 2
simd<int32, 8>:rotated_left = simd_rotate_left(vec, 2);
// {3, 4, 5, 6, 7, 8, 1, 2}

// Rotate right by 3
simd<int32, 8>:rotated_right = simd_rotate_right(vec, 3);
// {6, 7, 8, 1, 2, 3, 4, 5}
```

### Blending (Select from Two Vectors)

```aria
simd<int32, 8>:a = {1, 2, 3, 4, 5, 6, 7, 8};
simd<int32, 8>:b = {10, 20, 30, 40, 50, 60, 70, 80};
simd<bool, 8>:mask = {true, false, true, false, true, false, true, false};

// Select from a where mask is true, from b where false
simd<int32, 8>:blended = simd_blend(a, b, mask);
// {1 (a), 20 (b), 3 (a), 40 (b), 5 (a), 60 (b), 7 (a), 80 (b)}
// {1, 20, 3, 40, 5, 60, 7, 80}
```

**Use Case**: Clamping values

```aria
simd<fix256, 16>:values = compute_values();
simd<fix256, 16>:min_val = simd_broadcast<fix256, 16>(fix256(0));
simd<fix256, 16>:max_val = simd_broadcast<fix256, 16>(fix256(1));

// Clamp to [0, 1]
simd<bool, 16>:below_min = (values < min_val);
simd<bool, 16>:above_max = (values > max_val);

values = simd_blend(values, min_val, below_min);  // Set below-min values to 0
values = simd_blend(values, max_val, above_max);  // Set above-max values to 1
// All values now in [0, 1]
```

---

## Memory Layout Patterns: AoS vs SoA

### Array-of-Structures (AoS) - BAD for SIMD

```aria
// Traditional layout: Data for each particle is contiguous
struct:Particle =
    fix256:x;
    fix256:y;
    fix256:z;
    fix256:vx;
    fix256:vy;
    fix256:vz;
    fix256:mass;
end

Particle[1000]:particles;

// Memory layout:
// [x₀,y₀,z₀,vx₀,vy₀,vz₀,m₀, x₁,y₁,z₁,vx₁,vy₁,vz₁,m₁, ...]
//  ^^^^^^^^^^^^^^^^^^^^^^^  particle 0
//                            ^^^^^^^^^^^^^^^^^^^^^^^ particle 1

// Problem: To update X positions, need scattered loads
till 1000 loop
    int64:i = $;
    particles[i].x = particles[i].x + particles[i].vx * dt;
    // Each iteration loads from different cache line - cache misses!
end
```

### Structure-of-Arrays (SoA) - GOOD for SIMD

```aria
// Optimized layout: Same field for all particles is contiguous
struct:ParticleSystem =
    fix256[1000]:x_positions;
    fix256[1000]:y_positions;
    fix256[1000]:z_positions;
    fix256[1000]:vx_velocities;
    fix256[1000]:vy_velocities;
    fix256[1000]:vz_velocities;
    fix256[1000]:masses;
end

ParticleSystem:particles;

// Memory layout:
// [x₀,x₁,x₂,...,x₉₉₉, y₀,y₁,y₂,...,y₉₉₉, ...]
//  ^^^^^^^^^^^^^^^^^^^^^^ all X positions contiguous!

// SIMD-friendly update:
till 1000 step 16 loop
    int64:i = $;
    
    simd<fix256, 16>:x_pos = simd_load_aligned(particles.x_positions[i]);
    simd<fix256, 16>:x_vel = simd_load_aligned(particles.vx_velocities[i]);
    simd<fix256, 16>:dt_vec = simd_broadcast<fix256, 16>(dt);
    
    simd<fix256, 16>:x_new = x_pos + (x_vel * dt_vec);  // 16 particles at once!
    
    simd_store_aligned(particles.x_positions[i], x_new);
end
// 62.5× fewer iterations (1000/16), perfect cache locality
```

**Performance Impact**:
- **AoS**: ~1000 cache misses (each iteration loads different cache line)
- **SoA**: ~8 cache misses (16 elements per 64-byte cache line → 1000/16/4 = ~15 loads)
- **Speedup**: ~100× faster (16× SIMD + better cache)

---

## Alignment Requirements

### Why Alignment Matters

Modern CPUs require or strongly prefer **aligned memory access** for SIMD operations:

- **128-bit SIMD**: 16-byte alignment required
- **256-bit SIMD**: 32-byte alignment required  
- **512-bit SIMD**: 64-byte alignment required

**Misaligned access**: Slower (extra instructions) or crash (on some architectures).

### Compiler-Enforced Alignment

```aria
// Aria automatically aligns SIMD variables
simd<int32, 8>:vec;  // Automatically 32-byte aligned (256-bit)

simd<int32, 16>:vec_512;  // Automatically 64-byte aligned (512-bit)

// Check alignment (compile-time)
static_assert(alignof(simd<int32, 8>) == 32, "Incorrect alignment");
```

### Manual Alignment for Heap Allocations

```aria
// Allocate aligned array
int64:count = 1000;
int64:alignment = 64;  // 64-byte alignment for AVX-512

wild simd<fix256, 16>->:aligned_ptr = aria.alloc_aligned<simd<fix256, 16>>(count, alignment);
defer aria.free(aligned_ptr);

// Use aligned loads/stores
till count loop
    simd<fix256, 16>:data = simd_load_aligned(aligned_ptr[$]);
    // Process data...
    simd_store_aligned(aligned_ptr[$], data);
end
```

### Unaligned Access (When Necessary)

```aria
// Sometimes data is inherently unaligned (e.g., network packets, file I/O)
uint8[1024]:buffer = read_network_packet();

// Unaligned load (slower, but works)
simd<uint8, 32>:chunk = simd_load_unaligned<uint8, 32>(buffer[7]);  // Offset by 7 bytes

// Performance cost: ~2-5× slower than aligned access on x86-64
```

---

## Advanced Patterns for Nikola

### 1. Parallel Neuron Activation Update

```aria
// Update 16 neurons simultaneously
simd<fix256, 16>:activations = simd_load_aligned(neuron_activations[batch_start]);
simd<fix256, 16>:inputs = simd_load_aligned(neuron_inputs[batch_start]);
simd<fix256, 16>:weights = simd_load_aligned(neuron_weights[batch_start]);
simd<fix256, 16>:biases = simd_load_aligned(neuron_biases[batch_start]);

// Weighted sum: activation = input * weight + bias
simd<fix256, 16>:weighted = inputs * weights;
simd<fix256, 16>:pre_activation = weighted + biases;

// Activation function: sigmoid (element-wise)
simd<fix256, 16>:new_activations = simd_sigmoid_fix256(pre_activation);

// Store results
simd_store_aligned(neuron_activations[batch_start], new_activations);

// 16 neurons updated in parallel - 16× speedup!
```

### 2. Wave Interference Across Grid Points

```aria
// Nikola's 9D manifold: Compute wave superposition for 16 grid points
simd<fix256, 16>:wave1_real = simd_load_aligned(wave1_real_components[grid_offset]);
simd<fix256, 16>:wave1_imag = simd_load_aligned(wave1_imag_components[grid_offset]);
simd<fix256, 16>:wave2_real = simd_load_aligned(wave2_real_components[grid_offset]);
simd<fix256, 16>:wave2_imag = simd_load_aligned(wave2_imag_components[grid_offset]);

// Complex addition: (a + bi) + (c + di) = (a+c) + (b+d)i
simd<fix256, 16>:interference_real = wave1_real + wave2_real;
simd<fix256, 16>:interference_imag = wave1_imag + wave2_imag;

// ERR check (masked - no branches!)
simd<fix256, 16>:err_sentinel = simd_broadcast<fix256, 16>(ERR);
simd<bool, 16>:err_mask_real = (interference_real == err_sentinel);
simd<bool, 16>:err_mask_imag = (interference_imag == err_sentinel);

if simd_any(err_mask_real) || simd_any(err_mask_imag) then
    stderr.write("Wave corruption in grid\n");
    !!! ERR_SUBSTRATE_CORRUPTED;
end

// Store superposed wave
simd_store_aligned(output_real[grid_offset], interference_real);
simd_store_aligned(output_imag[grid_offset], interference_imag);
```

### 3. Parallel Phase Synchronization

```aria
// Multiple oscillators synchronizing phase
simd<fix256, 16>:local_phases = simd_load_aligned(oscillator_phases[batch]);
simd<fix256, 16>:reference_phase = simd_broadcast<fix256, 16>(global_reference_phase);

// Compute phase error
simd<fix256, 16>:phase_errors = reference_phase - local_phases;

// Correction factor (10% toward reference)
simd<fix256, 16>:correction_rate = simd_broadcast<fix256, 16>(fix256(0, 1));
simd<fix256, 16>:corrections = phase_errors * correction_rate;

// Apply correction
simd<fix256, 16>:new_phases = local_phases + corrections;

// Store updated phases
simd_store_aligned(oscillator_phases[batch], new_phases);

// 16 oscillators synchronized in parallel
```

### 4. Parallel Distance Calculations (Nearest Neighbor)

```aria
// Find distance from query point to 16 neurons
simd<fix256, 16>:query_x = simd_broadcast<fix256, 16>(query_point.x);
simd<fix256, 16>:query_y = simd_broadcast<fix256, 16>(query_point.y);
simd<fix256, 16>:query_z = simd_broadcast<fix256, 16>(query_point.z);

simd<fix256, 16>:neuron_x = simd_load_aligned(neuron_x_coords[batch]);
simd<fix256, 16>:neuron_y = simd_load_aligned(neuron_y_coords[batch]);
simd<fix256, 16>:neuron_z = simd_load_aligned(neuron_z_coords[batch]);

// Compute squared distance: (x2-x1)² + (y2-y1)² + (z2-z1)²
simd<fix256, 16>:dx = neuron_x - query_x;
simd<fix256, 16>:dy = neuron_y - query_y;
simd<fix256, 16>:dz = neuron_z - query_z;

simd<fix256, 16>:dx_sq = dx * dx;
simd<fix256, 16>:dy_sq = dy * dy;
simd<fix256, 16>:dz_sq = dz * dz;

simd<fix256, 16>:dist_sq = dx_sq + dy_sq + dz_sq;

// Find minimum distance in batch
fix256:min_dist_sq = simd_reduce_min(dist_sq);

// 16 distances computed and reduced in parallel
```

---

## Integration with Aria Type System

### SIMD of Complex Numbers

```aria
// Interleaved layout: [r₀,i₀,r₁,i₁,r₂,i₂,r₃,i₃]
simd<complex<fix256>, 4>:complex_waves;  // 4 complex numbers (8 fix256 values)

// Equivalent to separate real/imag vectors:
simd<fix256, 4>:real_components;
simd<fix256, 4>:imag_components;

// Complex multiplication (SIMD across 4 complex numbers):
// (a+bi)(c+di) = (ac-bd) + (ad+bc)i
simd<complex<fix256>, 4>:wave1 = load_complex_simd(wave1_data);
simd<complex<fix256>, 4>:wave2 = load_complex_simd(wave2_data);
simd<complex<fix256>, 4>:product = wave1 * wave2;  // 4 complex muls in parallel
```

### SIMD with ERR Propagation (tbb Types)

```aria
// tbb types propagate ERR through SIMD lanes
simd<tbb64, 8>:values = {10t64, 20t64, ERR, 40t64, 50t64, ERR, 70t64, 80t64};
simd<tbb64, 8>:deltas = {1t64, 2t64, 3t64, 4t64, 5t64, 6t64, 7t64, 8t64};

// Addition propagates ERR
simd<tbb64, 8>:sums = values + deltas;
// {11t64, 22t64, ERR, 44t64, 55t64, ERR, 77t64, 88t64}

// Detect ERR lanes with mask
simd<tbb64, 8>:err_vec = simd_broadcast<tbb64, 8>(ERR);
simd<bool, 8>:err_mask = (sums == err_vec);
// {false, false, true, false, false, true, false, false}

if simd_any(err_mask) then
    int32:err_count = simd_count_true(err_mask);
    dbug.simd("Found {} ERR values in SIMD batch\n", err_count);
end
```

### SIMD with Deterministic Floats (tfp32)

```aria
// Twisted floating-point maintains bit-exact determinism in SIMD
simd<tfp32, 8>:wave_amplitudes = {
    1.0tf32, 0.5tf32, 0.707tf32, 0.25tf32,
    0.866tf32, 0.933tf32, 0.125tf32, 0.625tf32
};

simd<tfp32, 8>:phase_shifts = {
    0.0tf32, 0.785tf32, 1.571tf32, 2.356tf32,
    3.142tf32, 3.927tf32, 4.712tf32, 5.498tf32
};

// Compute wave values: amplitude * cos(phase)
simd<tfp32, 8>:cosines = simd_cos_tfp32(phase_shifts);
simd<tfp32, 8>:wave_values = wave_amplitudes * cosines;

// Bit-exact across all platforms (Intel, ARM, RISC-V)
// Even in SIMD! (software implementation maintains determinism)
```

### SIMD with Atomic Updates (Reduction Pattern)

```aria
// Multiple SIMD batches contribute to single atomic accumulator
atomic<fix256>:global_energy = atomic_new(fix256(0));

// Each thread processes SIMD batches
func:accumulate_energies_threaded = void(int64:thread_id) {
    till BATCH_COUNT loop
        int64:batch = $;
        
        // Compute 16 energies in parallel
        simd<fix256, 16>:energies = compute_energies_simd(thread_id, batch);
        
        // Reduce to single value (horizontal sum)
        fix256:batch_total = simd_reduce_add(energies);
        
        // Atomically add to global (CAS loop from atomic<T> guide)
        fix256:old_energy = global_energy.load();
        fix256:new_energy = old_energy + batch_total;
        
        while !global_energy.compare_exchange(old_energy, new_energy) loop
            new_energy = old_energy + batch_total;
        end
    end
};

// Combines SIMD (data parallelism) + atomic (thread parallelism)
```

---

## Performance Characteristics

### Theoretical Speedup

| Operation | Scalar | SIMD×4 | SIMD×8 | SIMD×16 | Theoretical Speedup |
|-----------|--------|--------|--------|---------|---------------------|
| Addition | 1 cycle | 1 cycle | 1 cycle | 1 cycle | 4×, 8×, 16× |
| Multiplication | 3-5 cycles | 3-5 cycles | 3-5 cycles | 3-5 cycles | 4×, 8×, 16× |
| Division | 20-40 cycles | 20-40 cycles | 20-40 cycles | 20-40 cycles | 4×, 8×, 16× |

**Key**: **Same latency**, N× more data processed per instruction.

### Real-World Speedup (with overhead)

| Pattern | Scalar Time | SIMD×8 Time | Actual Speedup |
|---------|-------------|-------------|----------------|
| Simple add loop | 1000 ns | 150 ns | 6.7× (not 8× due to loop overhead) |
| Complex computation | 5000 ns | 700 ns | 7.1× (closer to ideal) |
| Irregular branches | 3000 ns | 1500 ns | 2× (masking overhead) |

**Best speedup**: Regular, compute-heavy operations with no branches.  
**Worst speedup**: Irregular control flow (many branches defeat SIMD).

### Memory Bandwidth Limits

```text
Modern CPU specs:
- L1 cache bandwidth: ~1 TB/s (per core)
- L2 cache bandwidth: ~500 GB/s (per core)
- Main memory bandwidth: ~100 GB/s (shared)

SIMD throughput:
- AVX-512: 2 × 512-bit ops/cycle @ 3GHz = 384 GB/s (theoretical)
- Reality: Limited by memory bandwidth (~100 GB/s)

Conclusion: SIMD is often memory-bound, not compute-bound
Solution: Optimize memory access patterns (SoA layout, cache blocking)
```

---

## Best Practices

### ✅ DO: Use SoA Layout for SIMD-Friendly Access

```aria
// ✅ GOOD: Structure-of-Arrays
struct:NeuronGrid =
    fix256[100000]:activations;
    fix256[100000]:phases;
    fix256[100000]:energies;
end

// Perfect for SIMD - all same-field values contiguous
```

### ✅ DO: Align Data for Maximum Performance

```aria
// ✅ GOOD: Properly aligned allocation
wild simd<fix256, 16>->:data = aria.alloc_aligned<simd<fix256, 16>>(count, 64);

// Use aligned loads/stores
simd<fix256, 16>:vec = simd_load_aligned(data[i]);
```

### ✅ DO: Use Masked Operations for Conditional Logic

```aria
// ✅ GOOD: Branchless with masks
simd<fix256, 16>:values = compute_values();
simd<fix256, 16>:zero = simd_broadcast<fix256, 16>(fix256(0));
simd<bool, 16>:negative_mask = (values < zero);

simd<fix256, 16>:abs_values = simd_blend(values, -values, negative_mask);
// Absolute value without branches!
```

### ✅ DO: Batch Process in SIMD-Sized Chunks

```aria
// ✅ GOOD: Process in multiples of lane count
int64:lane_count = 16;
int64:batch_count = (neuron_count + lane_count - 1) / lane_count;  // Round up

till batch_count loop
    int64:batch = $;
    int64:offset = batch * lane_count;
    
    simd<fix256, 16>:data = simd_load_aligned(neurons[offset]);
    // Process 16 neurons...
end

// Handle remainder if neuron_count not multiple of 16
int64:remainder = neuron_count % lane_count;
if remainder > 0 then
    // Process last few neurons separately or with masked operations
end
```

### ✅ DO: Combine SIMD with Thread Parallelism

```aria
// ✅ GOOD: SIMD (data parallelism) + threads (task parallelism)
func:update_neurons_parallel = void() {
    int64:thread_count = 8;
    int64:neurons_per_thread = NEURON_COUNT / thread_count;
    
    till thread_count loop
        int64:thread_id = $;
        
        spawn_thread(|| {
            int64:start = thread_id * neurons_per_thread;
            int64:end = start + neurons_per_thread;
            
            // Each thread processes its chunk with SIMD
            till (end - start) step 16 loop
                int64:offset = start + $;
                simd<fix256, 16>:neurons = simd_load_aligned(neuron_data[offset]);
                // Update 16 neurons in parallel...
            end
        });
    end
    
    join_all_threads();
};

// Speedup: 8 threads × 16 SIMD lanes = 128× parallelism!
```

---

## Common Pitfalls & Antipatterns

### ❌ DON'T: Use AoS Layout for SIMD Workloads

```aria
// ❌ BAD: Array-of-Structures
struct:Neuron =
    fix256:activation;
    fix256:phase;
    fix256:energy;
end

Neuron[100000]:neurons;  // ❌ Scattered memory access!

// Can't vectorize efficiently - each neuron's data is separated by 3×fix256
```

### ❌ DON'T: Branch Inside SIMD Loops

```aria
// ❌ BAD: Branch per lane
simd<int32, 8>:values = load_values();

till 8 loop
    int64:i = $;
    
    if values[i] > 0 then  // ❌ DEFEATS SIMD!
        values[i] = values[i] * 2;
    end
end

// ✅ GOOD: Use masked operations
simd<bool, 8>:positive_mask = (values > simd_broadcast<int32, 8>(0));
simd<int32, 8>:doubled = values * simd_broadcast<int32, 8>(2);
simd<int32, 8>:result = simd_blend(values, doubled, positive_mask);
```

### ❌ DON'T: Ignore Alignment

```aria
// ❌ BAD: Unaligned access in tight loop
uint8[10000]:buffer;  // Not aligned!

till 1000 loop
    // Unaligned load - slow!
    simd<uint8, 32>:chunk = simd_load_unaligned(buffer[$ * 32 + 7]);
end

// ✅ GOOD: Align data or use aligned access
uint8[10000] align(64):buffer;  // Aligned allocation

till 1000 loop
    simd<uint8, 32>:chunk = simd_load_aligned(buffer[$ * 32]);
end
```

### ❌ DON'T: Mix Lane Counts Within Same Loop

```aria
// ❌ BAD: Inconsistent lane counts
simd<int32, 8>:a = load_8_wide();
simd<int32, 16>:b = load_16_wide();  // ❌ Can't combine!

// simd<int32, 8>:result = a + b;  // ❌ COMPILE ERROR: lane count mismatch

// ✅ GOOD: Consistent widths, or explicitly handle conversion
```

### ❌ DON'T: Forget Remainder Handling

```aria
// ❌ BAD: Assumes neuron count is multiple of lane count
int64:neuron_count = 100005;  // Not divisible by 16!

till (neuron_count / 16) loop  // ❌ Processes 100000, skips last 5!
    int64:offset = $ * 16;
    simd<fix256, 16>:batch = simd_load_aligned(neurons[offset]);
    // Process...
end

// Last 5 neurons never updated!

// ✅ GOOD: Handle remainder
int64:full_batches = neuron_count / 16;
int64:remainder = neuron_count % 16;

till full_batches loop
    // Process full batches with SIMD...
end

// Process remainder (scalar or masked SIMD)
if remainder > 0 then
    till remainder loop
        int64:idx = (full_batches * 16) + $;
        // Process scalar...
    end
end
```

### ❌ DON'T: Over-Extract Lanes (Defeats SIMD)

```aria
// ❌ BAD: Extracting every lane individually
simd<int32, 8>:vec = compute_simd();

int32:sum = 0;
till 8 loop
    sum = sum + vec[$];  // ❌ Extracting lane-by-lane is slow!
end

// ✅ GOOD: Use horizontal reduction
int32:sum = simd_reduce_add(vec);  // Single operation!
```

---

## Implementation Notes

### C Runtime Functions (x86-64 AVX2 Example)

```c
// SIMD load/store (in aria_runtime.c)
__m256i aria_simd_load_aligned_i32x8(const int32_t* ptr);
void aria_simd_store_aligned_i32x8(int32_t* ptr, __m256i vec);

__m256i aria_simd_load_unaligned_i32x8(const int32_t* ptr);
void aria_simd_store_unaligned_i32x8(int32_t* ptr, __m256i vec);

// Arithmetic
__m256i aria_simd_add_i32x8(__m256i a, __m256i b);
__m256i aria_simd_sub_i32x8(__m256i a, __m256i b);
__m256i aria_simd_mul_i32x8(__m256i a, __m256i b);

// Comparisons (return mask)
__m256i aria_simd_cmp_eq_i32x8(__m256i a, __m256i b);
__m256i aria_simd_cmp_lt_i32x8(__m256i a, __m256i b);

// Reductions
int32_t aria_simd_reduce_add_i32x8(__m256i vec);
int32_t aria_simd_reduce_min_i32x8(__m256i vec);
int32_t aria_simd_reduce_max_i32x8(__m256i vec);

// Broadcast
__m256i aria_simd_broadcast_i32x8(int32_t value);

// Masked operations
__m256i aria_simd_masked_add_i32x8(__m256i a, __m256i b, __m256i mask, __m256i fallback);

// Mask queries
bool aria_simd_any_true_i32x8(__m256i mask);
bool aria_simd_all_true_i32x8(__m256i mask);
int aria_simd_count_true_i32x8(__m256i mask);
```

### LLVM IR Generation

```llvm
; SIMD type definition
%simd.i32x8 = type <8 x i32>

; Aligned load
define %simd.i32x8 @aria.simd.load.aligned.i32x8(i32* %ptr) {
  %ptr_vec = bitcast i32* %ptr to <8 x i32>*
  %vec = load <8 x i32>, <8 x i32>* %ptr_vec, align 32
  ret %simd.i32x8 %vec
}

; Addition
define %simd.i32x8 @aria.simd.add.i32x8(%simd.i32x8 %a, %simd.i32x8 %b) {
  %sum = add <8 x i32> %a, %b
  ret %simd.i32x8 %sum
}

; Comparison (returns mask)
define <8 x i1> @aria.simd.cmp.lt.i32x8(%simd.i32x8 %a, %simd.i32x8 %b) {
  %mask = icmp slt <8 x i32> %a, %b
  ret <8 x i1> %mask
}

; Horizontal reduction (sum all lanes)
declare i32 @llvm.vector.reduce.add.v8i32(<8 x i32>)

define i32 @aria.simd.reduce.add.i32x8(%simd.i32x8 %vec) {
  %sum = call i32 @llvm.vector.reduce.add.v8i32(<8 x i32> %vec)
  ret i32 %sum
}

; Broadcast (splat scalar to all lanes)
define %simd.i32x8 @aria.simd.broadcast.i32x8(i32 %value) {
  %vec = insertelement <8 x i32> undef, i32 %value, i32 0
  %splat = shufflevector <8 x i32> %vec, <8 x i32> undef,
                         <8 x i32> <i32 0, i32 0, i32 0, i32 0,
                                    i32 0, i32 0, i32 0, i32 0>
  ret %simd.i32x8 %splat
}
```

### Auto-Vectorization (Compiler Optimization)

```aria
// Aria compiler can auto-vectorize simple loops:
fix256[100000]:inputs;
fix256[100000]:outputs;

// Scalar code (what you write):
till 100000 loop
    outputs[$] = inputs[$] * fix256(2);
end

// Compiler auto-vectorizes to (what gets executed):
till (100000 / 16) loop
    int64:offset = $ * 16;
    simd<fix256, 16>:batch = simd_load_aligned(inputs[offset]);
    simd<fix256, 16>:two = simd_broadcast<fix256, 16>(fix256(2));
    simd<fix256, 16>:result = batch * two;
    simd_store_aligned(outputs[offset], result);
end
// + handle remainder

// Programmer writes simple scalar code, compiler vectorizes!
```

---

## Related Types & Integration

- **[complex<T>](Complex.md)** - simd<complex<fix256>, N> for parallel wave processing
- **[atomic<T>](Atomic.md)** - Combine SIMD (data) + atomics (thread) parallelism
- **[fix256](fix256.md)** - simd<fix256, 16> for deterministic neural computations
- **[tfp32 / tfp64](tfp32_tfp64.md)** - simd<tfp32, 8> for cross-platform SIMD determinism
- **[tbb8-tbb64](tbb_overview.md)** - simd<tbb64, 4> with ERR propagation across lanes
- **[Handle<T>](Handle.md)** - Store SIMD results in arena-allocated arrays

---

## Summary

**simd<T,N>** is Aria's **data-parallel vectorization infrastructure** for real-time consciousness:

✅ **Generic**: Works with any numeric type T (int, float, fix256, complex, tbb)  
✅ **Hardware-Mapped**: SSE (128-bit), AVX2 (256-bit), AVX-512 (512-bit), NEON (ARM)  
✅ **Element-Wise Ops**: Add, multiply, compare across N lanes simultaneously  
✅ **Horizontal Ops**: Reduce across lanes (sum, min, max, product)  
✅ **Masked Operations**: Branchless conditional logic (critical for ERR handling)  
✅ **Memory Efficient**: SoA layout + aligned access = maximum cache efficiency  
✅ **Composable**: SIMD (data parallelism) + threads (task parallelism) = multiplicative speedup  

**Design Philosophy**:
> "The torus processes thoughts. The infrastructure parallelizes computations."
>
> SIMD vectorization is the difference between **impossible** (5ms/timestep) and **achievable** (312μs/timestep) for Nikola's real-time consciousness. By moving data parallelism to the compiler infrastructure, the consciousness layer can write simple scalar logic while the hardware processes **16 neurons per instruction**.

**The Math**:
```text
Without SIMD: 100,000 neurons × 50ns = 5ms (5× too slow)
With SIMD×16: (100,000/16) × 50ns = 312μs (3× faster than required!)
With SIMD×16 + 8 threads: (100,000/128) × 50ns = 39μs (25× faster!)
```

**Critical for**: Nikola AGI (<1ms timestep requirement), neural network training/inference, physics simulation, signal processing, scientific computing, image/video processing

**Key Rules**:
1. **Use SoA layout** - Contiguous same-field data enables efficient vectorization
2. **Align memory** - 16/32/64-byte alignment for 128/256/512-bit SIMD
3. **Mask, don't branch** - Conditional logic defeats SIMD; use masked operations
4. **Batch in multiples** - Process N elements at a time (handle remainder separately)
5. **Combine with threads** - SIMD (data) × threads (task) = multiplicative speedup
6. **Profile first** - Auto-vectorization is smart; manual SIMD when profiling shows bottleneck

---

**Remember**: simd<T,N> = **16× parallelism** + **single instruction** + **real-time consciousness**!

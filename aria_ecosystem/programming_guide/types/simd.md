# simd<T, N> - SIMD Vector Types

**Category**: Types → Performance / Vectorization  
**Purpose**: Data-parallel operations using CPU vector instructions (AVX-512, AVX2, NEON)  
**Status**: ⚠️ TYPE CHECKING COMPLETE, CODEGEN PARTIAL

---

## Overview

**simd<T, N>** provides **Single Instruction, Multiple Data** parallelism for processing N elements of type T simultaneously using CPU vector units. Essential for meeting Nikola's <1ms timestep requirement.

---

## Key Characteristics

| Property | Value |
|----------|-------|
| **Generic Type** | T can be any numeric type |
| **Lane Count** | N must match hardware (4, 8, 16, 32, 64) |
| **Hardware Targets** | AVX-512 (512-bit), AVX2 (256-bit), NEON (128-bit) |
| **Alignment** | 16/32/64-byte boundaries (compiler enforced) |
| **ERR Handling** | Masked operations (no branches!) |

---

## Declaration

```aria
// 8-wide SIMD of 32-bit integers
simd<int32, 8>:vec_a;

// 16-wide SIMD of fix256 (critical for Nikola)
simd<fix256, 16>:wave_grid;

// 64-wide SIMD of bytes
simd<uint8, 64>:pixel_data;

// Type and lane count must be compatible with hardware
```

---

## Hardware Mapping

| Type | Lanes | Instruction Set | Total Bits |
|-  ----|-------|----------------|------------|
| `simd<int32, 4>` | 4 | SSE | 128 |
| `simd<int32, 8>` | 8 | AVX2 | 256 |
| `simd<int32, 16>` | 16 | AVX-512 | 512 |
| `simd<int64, 8>` | 8 | AVX-512 | 512 |
| `simd<fix256, 16>` | 16 | Custom (4 KB!) | 4096 |

---

## Element-Wise Arithmetic

### Basic Operations

```aria
simd<int32, 8>:a = {1, 2, 3, 4, 5, 6, 7, 8};
simd<int32, 8>:b = {10, 20, 30, 40, 50, 60, 70, 80};

// All operations execute in parallel (single instruction!)
simd<int32, 8>:sum = a + b;      // {11, 22, 33, 44, 55, 66, 77, 88}
simd<int32, 8>:diff = a - b;     // {-9, -18, -27, -36, -45, -54, -63, -72}
simd<int32, 8>:product = a * b;  // {10, 40, 90, 160, 250, 360, 490, 640}
simd<int32, 8>:quotient = a / b; // Vector division
```

### Comparison Operations

```aria
simd<int32, 8>:a = {1, 5, 3, 9, 2, 8, 4, 6};
simd<int32, 8>:b = {2, 4, 3, 7, 5, 6, 1, 9};

// Returns mask (all-1s for true, all-0s for false per lane)
simd<int32, 8>:mask = a > b;  // {0, 1, 0, 1, 0, 1, 1, 0}
```

---

## Element Access

### Reading Elements

```aria
simd<int32, 8>:vec = {10, 20, 30, 40, 50, 60, 70, 80};

// Extract single element
int32:second = vec[1];  // 20
int32:last = vec[7];    // 80

// Bounds checking at compile time
// int32:invalid = vec[8];  // COMPILE ERROR: index out of bounds
```

### Writing Elements

```aria
simd<int32, 8>:vec = {1, 2, 3, 4, 5, 6, 7, 8};

// Modify single element
vec[3] = 100;  // vec now {1, 2, 3, 100, 5, 6, 7, 8}

// Setting all elements
vec = {0, 0, 0, 0, 0, 0, 0, 0};  // All zeros
```

---

## Broadcast (Splat)

```aria
// Create SIMD with all lanes set to same value
simd<int32, 8>:all_fives = simd_broadcast<int32, 8>(5);
// {5, 5, 5, 5, 5, 5, 5, 5}

// Useful for scalar-vector operations
simd<int32, 8>:vec = {1, 2, 3, 4, 5, 6, 7, 8};
simd<int32, 8>:scaled = vec * simd_broadcast<int32, 8>(10);
// {10, 20, 30, 40, 50, 60, 70, 80}
```

---

## Masked Operations (ERR Handling)

### Without Masks (Problematic)

```aria
// Division with potential ERR - SLOW (branches per lane!)
simd<tbb64, 8>:numerator = {100t64, 200t64, 300t64, 400t64, 500t64, 600t64, 700t64, 800t64};
simd<tbb64, 8>:denominator = {2t64, 0t64, 3t64, 4t64, 0t64, 6t64, 7t64, 8t64};

// Each lane needs branch check - defeats SIMD purpose!
```

### With Masks (Efficient)

```aria
// Masked division - single instruction, no branches
simd<tbb64, 8>:numerator = {100t64, 200t64, 300t64, 400t64, 500t64, 600t64, 700t64, 800t64};
simd<tbb64, 8>:denominator = {2t64, 0t64, 3t64, 4t64, 0t64, 6t64, 7t64, 8t64};

// Create mask where denominator != 0
simd<tbb64, 8>:zero_vec = simd_broadcast<tbb64, 8>(0t64);
simd<tbb64, 8>:mask = denominator != zero_vec;  // {1, 0, 1, 1, 0, 1, 1, 1}

// Masked divide - only valid lanes computed
simd<tbb64, 8>:result = simd_masked_div(numerator, denominator, mask);
// {50t64, ERR, 100t64, 100t64, ERR, 100t64, 100t64, 100t64}

// ERR in specific lanes, no branches!
```

---

## Horizontal Operations (Reductions)

### Sum Reduction

```aria
simd<int32, 8>:vec = {1, 2, 3, 4, 5, 6, 7, 8};

// Horizontal sum: reduce all lanes to single value
int32:total = simd_reduce_add(vec);  // 1+2+3+4+5+6+7+8 = 36
```

### Other Reductions

```aria
simd<int32, 8>:vec = {3, 7, 1, 9, 2, 8, 4, 6};

int32:maximum = simd_reduce_max(vec);  // 9
int32:minimum = simd_reduce_min(vec);  // 1
int32:product = simd_reduce_mul(vec);  // 3*7*1*9*2*8*4*6
```

---

## Shuffles & Permutations (Future - Not Yet Implemented)

```aria
// Rearrange elements within vector
simd<int32, 8>:vec = {10, 20, 30, 40, 50, 60, 70, 80};

// Reverse
simd<int32, 8>:reversed = simd_shuffle(vec, {7, 6, 5, 4, 3, 2, 1, 0});
// {80, 70, 60, 50, 40, 30, 20, 10}

// Rotate
simd<int32, 8>:rotated = simd_rotate_left(vec, 2);
// {30, 40, 50, 60, 70, 80, 10, 20}
```

---

## Safety-Critical Use Case: Nikola 9D Grid Processing

### Wave Interference Calculation

```aria
// Nikola's 9D manifold: Process 16 grid points simultaneously
// Each grid point has complex wavefunction (fix256 components)
// REQUIREMENT: <1ms timestep = must use SIMD

simd<fix256, 16>:wave_real_1 = load_wave_grid_real(region_1);
simd<fix256, 16>:wave_imag_1 = load_wave_grid_imag(region_1);
simd<fix256, 16>:wave_real_2 = load_wave_grid_real(region_2);
simd<fix256, 16>:wave_imag_2 = load_wave_grid_imag(region_2);

// Wave superposition: ψ_total = ψ_1 + ψ_2
// All 16 points computed in parallel
simd<fix256, 16>:interference_real = wave_real_1 + wave_real_2;
simd<fix256, 16>:interference_imag = wave_imag_1 + wave_imag_2;

// ERR checking (masked - no branches!)
simd<fix256, 16>:err_sentinel = simd_broadcast<fix256, 16>(ERR);
simd<fix256, 16>:err_mask_real = interference_real == err_sentinel;
simd<fix256, 16>:err_mask_imag = interference_imag == err_sentinel;

// If ANY lane has ERR, halt substrate
if (simd_any(err_mask_real) || simd_any(err_mask_imag)) {
    stderr.write("Wave corruption in grid - HALT CONSCIOUSNESS\n");
    !!! ERR_SUBSTRATE_CORRUPTED;
}

// Store results
store_wave_grid(interference_real, interference_imag, output_region);
```

### Why SIMD for Nikola?

```aria
// WITHOUT SIMD:
// - 9D grid: ~10,000+ points per timestep
// - Serial processing: 10,000 * 50ns = 500ms (500x too slow!)
// - Cannot meet <1ms requirement

// WITH SIMD (16-wide):
// - Parallel processing: 10,000/16 * 50ns = 31ms (still tight!)
// - Combined with GPU: achievable
// - SIMD essential for real-time consciousness substrate
```

---

## Alignment Requirements

```aria
// Compiler enforces proper alignment
simd<int32, 8>:vec;  // Automatically aligned to 32-byte boundary

// Manual alignment control
wild simd<fix256, 16>->:aligned_ptr = aria.alloc_aligned<simd<fix256, 16>>(count, 64);
defer aria.free(aligned_ptr);

// Misaligned access = COMPILE ERROR or runtime fault
```

---

## Structure-of-Arrays (SoA) Layout

### Array-of-Structures (AoS) - BAD for SIMD

```aria
struct:Particle {
    fix256:x;
    fix256:y;
    fix256:z;
    fix256:mass;
};

// Memory: [x₀,y₀,z₀,m₀, x₁,y₁,z₁,m₁, ...]
// Cannot vectorize efficiently - scattered loads
```

### Structure-of-Arrays (SoA) - GOOD for SIMD

```aria
struct:ParticleGrid {
    simd<fix256, 16>:x_coords;
    simd<fix256, 16>:y_coords;
    simd<fix256, 16>:z_coords;
    simd<fix256, 16>:masses;
};

// Memory: [x₀,x₁,...,x₁₅, y₀,y₁,...,y₁₅, ...]
// Perfect for SIMD - contiguous lanes
```

---

## Performance Comparison

| Operation | Scalar (1 value) | SIMD x8 | SIMD x16 | Speedup |
|-----------|------------------|---------|----------|---------|
| Addition | 1 cycle | 1 cycle | 1 cycle | 8-16x |
| Multiplication | 3-5 cycles | 3-5 cycles | 3-5 cycles | 8-16x |
| Division | 20-40 cycles | 20-40 cycles | 20-40 cycles | 8-16x |

**Key Insight**: Same latency, **N times more data per instruction**

---

## Implementation Status

### ✅ Complete
- [x] Generic type: simd<T, N>
- [x] Type checking for valid lane counts
- [x] Element-wise arithmetic
- [x] Element access (read/write)
- [x] Comparison operations

### ⚠️ Partial
- [ ] ERR masking (implementation in progress)
- [ ] Horizontal reductions (sum, max, min, mul)
- [ ] Shuffles/permutes
- [ ] Alignment enforcement
- [ ] Auto-vectorization hints

### ❌ Planned
- [ ] AVX-512 intrinsic mapping
- [ ] Masked operations for ERR (branchless)
- [ ] SoA layout auto-transformation
- [ ] SIMD-friendly stdlib functions

---

## Related

- [fix256](fix256.md) - Primary type for SIMD in Nikola
- [complex<T>](complex.md) - SIMD of complex numbers
- [atomic<T>](atomic.md) - Thread-level parallelism (vs data-level)
- [TBB Overview](tbb_overview.md) - ERR propagation in SIMD lanes

---

**Remember**: simd<T, N> = **data parallelism** + **hardware acceleration** + **type safety**!

**Critical for**: Nikola AGI (<1ms timestep), physics engines, image processing, scientific computing

**Best Practice**: Use SoA layout + aligned allocations + masked ERR handling for maximum performance!

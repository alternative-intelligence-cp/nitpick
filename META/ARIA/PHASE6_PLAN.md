# Phase 6: Nikola Physics Integration - Plan

**Date**: February 12, 2026  
**Status**: 🟡 Planning  
**Prerequisites**: ✅ Phase 5.3 Complete (GPU division validated)

---

## Objectives

Integrate fix256 deterministic fixed-point arithmetic into Nikola physics engine to achieve bit-exact cross-platform simulation for networked multiplayer gameplay.

**Goal**: Replace floating-point operations in UFIE (Unified Field Integration Engine) with fix256 to eliminate non-determinism from:
- IEEE 754 implementation-defined behaviors (FMA, rounding modes)
- Platform-specific instruction sets (x87 80-bit intermediates)
- CPU/GPU computation differences

---

## Success Criteria

1. ✅ **Determinism**: Same inputs → Same outputs across:
   - CPU vs GPU
   - Linux vs Windows
   - x86-64 vs ARM64
   - Different NVIDIA GPU architectures

2. ✅ **Performance**: Physics tick completes within 1ms budget
   - Current FP32 baseline: ~0.4ms per tick
   - Target fix256: <1.0ms per tick (acceptable 2.5× slowdown)

3. ✅ **Correctness**: Simulation produces physically plausible results
   - Energy conservation (within fixed-point precision)
   - Momentum conservation
   - No catastrophic divergence over time

4. ✅ **Network Synchronization**: Distributed nodes remain in perfect sync
   - 100 frames without desync
   - Checksum validation every 10 frames

---

## Architecture Overview

### Current Nikola Physics Pipeline (FP32)
```
Particles → Forces → Acceleration → Velocity → Position
   ↓           ↓          ↓            ↓          ↓
 float32    float32   float32      float32    float32
```

### New Pipeline (Q128.128 fix256)
```
Particles → Forces → Acceleration → Velocity → Position
   ↓           ↓          ↓            ↓          ↓
 fix256     fix256     fix256       fix256     fix256
```

**Key Operations**:
- **Laplacian**: `∇²φ = (φ[i+1] + φ[i-1] - 2*φ[i]) / Δx²`
- **Gradient**: `∇φ = (φ[i+1] - φ[i-1]) / (2*Δx)`
- **Integration**: `v += a*dt`, `x += v*dt`

---

## Phase 6 Sub-Tasks

### 6.1: Minimal Integration (Proof of Concept)
**Goal**: Replace single kernel with fix256 version

**Tasks**:
1. Identify simplest kernel (likely `update_velocity_kernel`)
2. Create fix256 variant: `update_velocity_kernel_fix256`
3. Add conversion helpers: `float_to_fix256`, `fix256_to_float`
4. Run side-by-side comparison (FP32 vs fix256)
5. Verify results match within tolerance

**Files**:
- `nikola/src/physics/kernels/integration.cu` (new)
- `nikola/src/physics/ufie.cpp` (modify to call fix256 kernel)

**Validation**:
```bash
cd nikola
make test-physics-fix256
# Should show FP32 vs fix256 comparison
```

---

### 6.2: Laplacian Operator (Critical Path)
**Goal**: Implement deterministic Laplacian for field calculations

**Challenge**: Laplacian is most numerically sensitive operation
```cpp
// Current (FP32, non-deterministic)
float laplacian = (phi[i+1] + phi[i-1] - 2*phi[i]) / (dx*dx);

// New (fix256, deterministic)
fix256 laplacian = fix256_div(
    fix256_sub(
        fix256_add(phi[i+1], phi[i-1]),
        fix256_mul(fix256_from_int(2), phi[i])
    ),
    fix256_mul(dx, dx)
);
```

**Tasks**:
1. Implement `compute_laplacian_kernel_fix256`
2. Test on 1D, 2D, 3D grids
3. Compare energy distribution vs FP32
4. Verify no accumulation errors over 1000 timesteps

**Expected Issues**:
- Loss of precision in division (Q128.128 has finite precision)
- Need to tune `dx` grid spacing for optimal fixed-point range

---

### 6.3: Full UFIE Conversion
**Goal**: Convert all physics kernels to fix256

**Kernels to Convert**:
1. `update_velocity_kernel` (easiest)
2. `update_position_kernel` (easy)
3. `compute_forces_kernel` (medium - involves field sampling)
4. `compute_laplacian_kernel` (hard - numerically sensitive)
5. `apply_boundary_conditions_kernel` (medium)

**Strategy**: Convert one kernel per commit, validate after each

---

### 6.4: Cross-Platform Validation
**Goal**: Prove determinism across platforms

**Test Matrix**:
| Platform | CPU | GPU | Status |
|----------|-----|-----|--------|
| Linux x86-64 | ✅ | ✅ RTX 3090 | Primary dev |
| Linux ARM64 | ⏳ | ⏳ Jetson? | Optional |
| Windows x86-64 | ⏳ | ⏳ RTX 3090 | Critical |

**Validation Method**:
1. Run 100-frame simulation on each platform
2. Capture state checksum every 10 frames
3. Compare checksums - must match exactly
4. If mismatch, binary search to find divergence point

**Tools**:
```bash
# Generate reference checksums (Linux)
./nikola-sim --mode=fix256 --frames=100 --checksum > checksums_linux.txt

# Validate on Windows
./nikola-sim.exe --mode=fix256 --frames=100 --checksum > checksums_windows.txt
diff checksums_linux.txt checksums_windows.txt  # Should be identical
```

---

### 6.5: Network Synchronization Test
**Goal**: Demonstrate multiplayer determinism

**Scenario**: 2 players, 10 interactive objects, 300 frames

**Test Setup**:
```
Node A (Linux)    ←→    Node B (Windows)
   ↓                        ↓
fix256 physics        fix256 physics
   ↓                        ↓
Checksum: 0xABCD...     Checksum: 0xABCD...  ✅
```

**Success**: Zero desyncs over 300 frames (10 seconds @ 30 FPS)

---

### 6.6: Performance Optimization
**Goal**: Meet <1ms physics tick budget

**Baseline**: Current FP32 takes ~0.4ms per tick  
**Target**: fix256 <1.0ms per tick

**Optimization Strategies**:
1. **Parallel Operations**: For independent particles, launch more threads
2. **Memory Coalescing**: Ensure fix256 structs are aligned for GPU access
3. **Kernel Fusion**: Combine multiple passes into single kernel
4. **PTX Intrinsics**: Optimize hot paths (already done in Phase 5)

**Profiling**:
```bash
nvprof ./nikola-sim --mode=fix256 --frames=100
# Identify bottleneck kernels
```

---

## Implementation Roadmap

### Week 1: Minimal Integration (6.1)
- Create fix256 variant of simplest kernel
- Validate against FP32 baseline
- **Deliverable**: Single working kernel

### Week 2: Laplacian (6.2)
- Implement deterministic Laplacian operator
- Validate numerical stability
- **Deliverable**: Core physics operator proven

### Week 3: Full Conversion (6.3)
- Convert all remaining kernels
- Integration tests for each
- **Deliverable**: Complete fix256 physics pipeline

### Week 4: Cross-Platform (6.4-6.5)
- Test on Windows + multiple GPUs
- Network synchronization validation
- **Deliverable**: Proven determinism

### Week 5: Performance (6.6)
- Profile and optimize
- Meet <1ms budget
- **Deliverable**: Production-ready system

---

## Risk Mitigation

### Risk 1: Precision Loss in Division
**Likelihood**: High  
**Impact**: Medium  
**Mitigation**: Tune grid spacing `dx` to keep values in optimal Q128.128 range (integer part < 2^64, fractional part > 2^-64)

### Risk 2: Performance Exceeds Budget
**Likelihood**: Medium  
**Impact**: High  
**Mitigation**: 
- Hybrid approach: Use FP32 for non-critical particles, fix256 for synchronized objects
- Reduce physics quality setting (fewer particles, larger timesteps)

### Risk 3: Double-Negative Division Bug Surfaces
**Likelihood**: Low  
**Impact**: Low  
**Mitigation**: Known issue only affects <1% of operations. Monitor for edge cases during testing.

---

## Documentation Deliverables

1. `NIKOLA_FIX256_INTEGRATION.md` - Integration guide
2. `DETERMINISM_VALIDATION_REPORT.md` - Cross-platform test results
3. `PERFORMANCE_ANALYSIS.md` - Profiling data and optimizations
4. Updated Nikola engineering docs (08_implementation_guide.txt)

---

## Current Status

**Phase**: Planning  
**Next Action**: Begin Phase 6.1 (Minimal Integration)  
**Blockers**: None  
**Ready to Proceed**: ✅

---

**Questions for Review**:
1. Start with velocity update kernel or simpler test case?
2. What existing Nikola test framework should we hook into?
3. Windows test machine available, or Linux-only for now?
4. Target frame rate: 30 FPS or 60 FPS for network tests?

---

**Let's build deterministic physics!** 🎯

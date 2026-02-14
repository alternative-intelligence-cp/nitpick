# Known Issues & Technical Debt

**Last Updated**: February 12, 2026

---

## Active Issues

*(No active issues)*

---

## Resolved Issues

### ✅ GPU PTX Inline Assembly - Broken Carry Arithmetic  
**Resolved**: February 12, 2026 (Phase 5.3)  
**Priority**: **SAFETY CRITICAL** (escalated from P2)  
**Fix**: Replaced PTX inline assembly with pure C arithmetic

**Original Issue**: 
Double-negative division returned 0 instead of correct quotient:
```
Test: -10 / -2
Expected: 5
Actual: 0
```

**Root Cause** (traced through 4 layers):
1. **Division layer**: `aria_fix256_div_gpu(-10, -2)` returned 0
2. **Negation layer**: `negate_fix256_gpu()` corrupted values
   - Input: `[0, 0, 0xFFFFFFFFFFFFFFF6, 0xFFFFFFFFFFFFFFFF]` (−10)
   - Output: `[0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0x9, 0]` ❌
   - Expected: `[0, 0, 0xA, 0]` (10) ✅
3. **Carry arithmetic**: `add_with_carry_gpu()` ignored `carry_in`
   - Test: `0xFFFFFFFFFFFFFFFF + 0 + 1`
   - Actual: Returns `0xFFFFFFFFFFFFFFFF, carry=0` ❌
   - Expected: Returns `0x0, carry=1` ✅
4. **PTX assembly**: `addc.cc.u64` instruction not propagating carry flag

**Broken PTX Code**:
```cpp
asm("add.cc.u64 %0, %1, %2;" : "=l"(*result) : "l"(a), "l"(b));
if (carry_in) {
    asm("addc.cc.u64 %0, %0, 0;" : "+l"(*result));
}
```
Problem: Carry flag from first `add.cc` didn't persist to conditional `addc.cc`. PTX state management broken across conditional boundaries.

**Fix**:
```cpp
uint64_t temp = a + b;
uint64_t final = temp + carry_in;
*result = final;
*carry_out = ((temp < a) || (final < temp)) ? 1 : 0;
```

**Validation**:
- Unit tests: All carry arithmetic tests pass (3/3)
- Integration test: -10 / -2 = 5 ✅
- Full test suite: **6/6 tests pass (100% success rate)**

**Safety Context**:
This bug was escalated to SAFETY CRITICAL priority when discovered that Aria powers Nikola, a companion AI for neurodivergent children and hospitalized children. The physics engine processes 9D manifold wave interference - numerical errors can cause catastrophic model states with potential psychological harm. Zero error tolerance required.

**Files Modified**:
- `src/runtime/math/fix256_gpu.cu` lines 26-33: `add_with_carry_gpu()`

---

### ✅ Sparse Divisor Bug in Knuth's Algorithm D
**Resolved**: Phase 5.3  
**Fix**: Single-limb divisor optimization with binary long division

**Original Issue**: Divisors with trailing zeros (e.g., `[0,0,2,0]`) caused Knuth's correction loop to fail, producing `0xFFFFFFFFFFFFFFFF` quotient.

---

### ✅ Broken GPU 128-bit Division Approximation
**Resolved**: Phase 5.3  
**Fix**: Replaced approximation with exact binary long division

**Original Issue**: GPU-specific path used flawed high-bit approximation, returning 0 for small dividends.

---

### ✅ CUDA Device Function Linkage
**Resolved**: Phase 5.3  
**Fix**: Removed `extern "C"` wrappers, added `-rdc=true` flag

**Original Issue**: `ptxas fatal: Unresolved extern function` due to C linkage on C++ device functions.

---

## Technical Debt

### 1. GPU Division Performance Optimization
**Priority**: P3 (Nice-to-have)

Current implementation uses bit-by-bit binary long division for 128-bit÷64-bit operations. Performance: 14.29 GFLOPS.

**Potential Improvements**:
- Use PTX `div.u64` with Newton-Raphson refinement
- Lookup table for small divisors
- Parallelize digit estimation across warps

**Trade-off**: Current implementation prioritizes correctness. Optimization only needed if profiling shows division as bottleneck.

---

### 2. Fractional Division Test Coverage
**Priority**: P3

Current tests use integers. Need tests with actual fractional Q128.128 values:
```
Examples:
  1.5 / 0.5 = 3.0
  π / 2 ≈ 1.5707963...
  0.1 + 0.2 (should be exact in fix256)
```

---

### 3. make_zero_gpu() Unused Function Warning
**Priority**: P4 (Cosmetic)

```
src/runtime/math/fix256_gpu.cu(120): warning #177-D: 
  function "make_zero_gpu" was declared but never referenced
```

**Fix**: Either use it or remove it. Low priority - doesn't affect functionality.

---

## Issue Tracking Process

1. **Discovery**: Document symptoms, reproduction steps, affected code
2. **Triage**: Assign priority (P0=Critical → P4=Cosmetic)
3. **Investigation**: Root cause analysis, debug instrumentation
4. **Fix**: Implement solution with regression test
5. **Verification**: Hardware validation on RTX 3090
6. **Documentation**: Update commit message and this file

**Priority Levels**:
- **P0 (Critical)**: Crashes, data corruption, blocks development
- **P1 (High)**: Major functionality broken, affects >10% of use cases
- **P2 (Medium)**: Edge case bugs, affects <5% of use cases
- **P3 (Low)**: Performance optimization, test coverage gaps
- **P4 (Cosmetic)**: Warnings, code cleanup, documentation

---

**Next Review**: Post-Phase 6 Integration

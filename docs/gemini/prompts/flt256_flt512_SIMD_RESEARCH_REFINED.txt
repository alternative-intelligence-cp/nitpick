# flt256/flt512 SIMD-First Implementation Research Prompt
**Date**: December 26, 2025
**Version**: Refined Final (Security + Language Integrity + Unlimited Timeline)
**Purpose**: Production-grade high-precision floating-point for Aria 1.0 (ONE SHOT - permanent freeze)

---

## SECTION 0: Design Philosophy & Language Integrity

### The Stakes
This is **not** a research prototype. This is production code for a language that will **freeze permanently** at version 1.0 - like C, which has maintained backward compatibility from 1972 to 2025.

**Critical Context**:
- flt256/flt512 are **CORE LANGUAGE FEATURES**, not "batteries-included" libraries
- After 1.0 release, we CANNOT change bit layouts, semantics, or operation signatures
- There is NO version 2.0 to fix mistakes
- Code written in 2025 must compile unchanged in 2050
- **ONE SHOT to get it right** - this is the only chance

**Security Imperative**:
The Nikola AI system (which runs on Aria) will interact with vulnerable populations:
- Children (educational assistance, social-emotional learning)
- Elderly (cognitive assistance, companionship)
- Disabled individuals (accessibility support)
- Other at-risk groups

Floating-point bugs can bypass safety checks. NaN propagation errors can disable guardrails. Timing leaks can expose private information. **There is zero tolerance for sloppiness** - if someone is harmed because we cut corners, that's an unacceptable outcome.

### Timeline Philosophy
- **NO DEADLINE** for Aria 1.0 release
- 6 weeks, 6 months, or 6 years - **perfection is the only metric**
- Time is irrelevant; correctness is everything
- Can try 100 different approaches to find optimal
- Can rewrite from scratch if first attempt fails
- Cannot compromise quality to hit arbitrary release date

### Core vs Batteries-Included
**Core Language** (frozen at 1.0):
- Type system (flt256, flt512, int32, etc.)
- Memory model (Stack/GC/Wild ownership)
- Control flow (if, while, for, match)
- Basic operators (arithmetic, comparison, logical)
- **These CANNOT change** - must be perfect before 1.0

**Batteries-Included** (can evolve post-1.0):
- Standard library (std.math, std.io, std.net)
- Third-party libraries
- Tooling (formatters, linters, IDEs)
- Optimizations (compiler improvements, not semantic changes)
- **These CAN improve** - additive changes allowed

flt256/flt512 are CORE features. We get one shot.

---

## SECTION 1: Performance Requirements (NON-NEGOTIABLE)

### Absolute Targets (Hardware-Competitive)
These are **NOT GUIDELINES** - they are hard requirements:

```
Operation           Hardware float64    flt256 Target    flt512 Target
─────────────────────────────────────────────────────────────────────
Addition            4 cycles           ≤6 cycles         ≤8 cycles
Multiplication      4 cycles           ≤8 cycles         ≤12 cycles
Division            13-16 cycles       ≤20 cycles        ≤28 cycles
FMA (fused)         4 cycles           ≤8 cycles         ≤12 cycles
Square Root         14-16 cycles       ≤18 cycles        ≤24 cycles
Comparison          3 cycles           ≤5 cycles         ≤7 cycles
Conversion (int)    6 cycles           ≤10 cycles        ≤14 cycles
```

**Why These Are Achievable**:
1. **SIMD Parallelism**: AVX-512 can process 8× uint64 limbs simultaneously
2. **Instruction-Level Parallelism**: Modern CPUs execute 4-6 ops/cycle (latency hiding)
3. **Algorithmic Optimization**: Karatsuba multiplication, Newton-Raphson division
4. **Branchless Logic**: `vpternlogd` for arbitrary boolean functions (no branch mispredicts)
5. **Lookup Tables**: Pre-computed reciprocals, polynomial coefficients
6. **Hand-Tuned Assembly**: Critical hot paths in inline asm

The extra bits don't automatically mean 10x slowdown. With SIMD, we're processing **multiple limbs in parallel** - the latency overhead is compensated by throughput.

### Optimization Toolkit (Must Explore 2-3 Per Operation)
Do NOT settle for first-working implementation. Try multiple approaches:

1. **SIMD Parallelism**: Use AVX-512 to process all limbs simultaneously
2. **Instruction-Level Parallelism**: Interleave independent operations (hide latency)
3. **Branchless Operations**: `vpternlogd` for conditionals, avoid branch mispredicts
4. **Lookup Tables**: Reciprocal approximations, polynomial coefficients
5. **Polynomial Approximations**: Remez algorithm for special functions
6. **Prefetching**: `_mm_prefetch` for predictable memory access
7. **Loop Unrolling**: Reduce loop overhead, enable better scheduling
8. **Inline Assembly**: Hand-optimize hot paths (addition, multiplication)
9. **Algorithm Selection**: Karatsuba > schoolbook for large multiplications
10. **Special Case Fast Paths**: Zero, infinity, power-of-two optimizations

### Measurement Methodology
- Use `rdtsc` (timestamp counter) for cycle-accurate profiling
- Measure 100,000 iterations, take median (exclude cache cold starts)
- Test on Intel Skylake-X (AVX-512) and AMD Zen 4 (AVX-512)
- Compare against hardware float64 baseline on same machine
- Profile with `perf stat` (cache misses, branch mispredicts, IPC)

**Unacceptable Results**:
- "3-5x slower than hardware is acceptable" ❌ NO
- "Software emulation is sufficient" ❌ NO
- "We can optimize later" ❌ NO
- "10 cycles for addition is close enough" ❌ NO

If targets aren't met: **try different algorithms, rewrite from scratch, take more time**. This is permanent - cannot patch it in v2.0 because there is no v2.0.

---

## SECTION 2: Type Specifications

### flt256 (Quad-Precision Extended)
**Total**: 256 bits
- **Sign**: 1 bit (bit 255)
- **Exponent**: 19 bits (bits 254-236) → range ±2^(±262,143)
- **Mantissa**: 236 bits (bits 235-0) → ~71 decimal digits precision

**Memory Layout**:
```c
// Aligned to 32 bytes (AVX-2 register boundary)
typedef struct {
    __m256i value;  // Single AVX-2 register
} flt256;

// Alternative: Portable representation
typedef struct {
    uint64_t limbs[4];  // [0]=low bits, [3]=high bits (sign+exp+mantissa_high)
} flt256;
```

**Bit Layout** (little-endian):
```
limbs[3]: [S|EEEEEEEEEEEEEEEEEEE|MMMM...] (sign, 19-bit exp, 44-bit mantissa_high)
limbs[2]: [MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM...] (64-bit mantissa_mid_high)
limbs[1]: [MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM...] (64-bit mantissa_mid_low)
limbs[0]: [MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM...] (64-bit mantissa_low)
```

### flt512 (Octuple-Precision)
**Total**: 512 bits
- **Sign**: 1 bit (bit 511)
- **Exponent**: 23 bits (bits 510-488) → range ±2^(±4,194,303)
- **Mantissa**: 488 bits (bits 487-0) → ~147 decimal digits precision

**Memory Layout**:
```c
// Aligned to 64 bytes (AVX-512 register boundary)
typedef struct {
    __m512i value;  // Single AVX-512 register
} flt512;

// Alternative: Portable representation
typedef struct {
    uint64_t limbs[8];  // [0]=low bits, [7]=high bits
} flt512;
```

**Bit Layout** (little-endian):
```
limbs[7]: [S|EEEEEEEEEEEEEEEEEEEEEEE|MMMMMMMM...] (sign, 23-bit exp, 40-bit mantissa_high)
limbs[6..0]: Each contains 64-bit mantissa chunks
```

### IEEE 754 Compliance
- **NaN**: Exponent all 1s, mantissa non-zero
  - Quiet NaN: mantissa MSB = 1
  - Signaling NaN: mantissa MSB = 0
- **Infinity**: Exponent all 1s, mantissa all 0s
- **Denormals**: Exponent all 0s, mantissa non-zero (gradual underflow)
- **Zero**: Exponent and mantissa all 0s (sign bit distinguishes +0/-0)

**Rounding Modes** (required):
1. Round to nearest, ties to even (default)
2. Round toward zero (truncation)
3. Round toward +∞ (ceiling)
4. Round toward -∞ (floor)

---

## SECTION 3: SIMD Acceleration Strategy (PRIMARY PATH)

### Target Architectures
1. **Primary**: AVX-512F, AVX-512BW, AVX-512DQ (Intel Skylake-X+, AMD Zen 4+)
2. **Fallback**: AVX-2, FMA3 (Intel Haswell+, AMD Excavator+)
3. **Portable**: SSE4.2 + scalar operations (universal x86-64)

### Key SIMD Instructions
**AVX-512 (flt256 and flt512)**:
- `vpsrlq` - Parallel 64-bit right shift (exponent extraction)
- `vpsllq` - Parallel 64-bit left shift (mantissa alignment)
- `vpaddq` - Parallel 64-bit addition (mantissa arithmetic)
- `vpsubq` - Parallel 64-bit subtraction
- `vpmullq` - Parallel 64-bit multiplication (lower 64 bits)
- `vpmuludq` - Parallel 64-bit unsigned multiply (extended precision)
- `vpternlogd` - 3-input boolean operations (branchless conditionals)
- `vplzcntq` - Parallel leading zero count (normalization)
- `vpsllvq` - Parallel variable shifts (per-element shift amounts)

**AVX-2 (flt256 only)**:
- Same instructions as AVX-512, but 256-bit registers (4× uint64)
- Use `_mm256_*` intrinsics instead of `_mm512_*`

### Implementation Strategy

#### Addition/Subtraction (Example)
```c
flt256 flt256_add(flt256 a, flt256 b) {
    // 1. Extract exponents (parallel shift + mask)
    __m256i exp_a = _mm256_srli_epi64(a.value, 236) & 0x7FFFF;  // 19-bit mask
    __m256i exp_b = _mm256_srli_epi64(b.value, 236) & 0x7FFFF;
    
    // 2. Determine alignment (branchless with vpternlogd)
    __m256i exp_diff = _mm256_sub_epi64(exp_a, exp_b);
    __m256i align_mask = ...; // Use vpternlogd for exp_a > exp_b
    
    // 3. Align mantissas (parallel variable shift)
    __m256i mant_a_aligned = _mm256_sllv_epi64(mant_a, shift_amounts);
    
    // 4. Add mantissas (parallel addition with carry propagation)
    __m256i result_mant = _mm256_add_epi64(mant_a_aligned, mant_b);
    
    // 5. Normalize (parallel leading zero count)
    __m256i lz = _mm256_lzcnt_epi64(result_mant);
    __m256i normalized = _mm256_sllv_epi64(result_mant, lz);
    
    // 6. Adjust exponent
    __m256i result_exp = _mm256_sub_epi64(exp_result, lz);
    
    // 7. Round (ties to even)
    // ... rounding logic with vpternlogd for branchless selection
    
    // 8. Pack result
    return pack_flt256(sign, result_exp, normalized);
}
```

**Key Points**:
- All limbs (4× uint64 for flt256, 8× uint64 for flt512) processed in parallel
- No scalar loops - everything vectorized
- Branchless logic using `vpternlogd` (3-input LUT: 256 possible boolean functions)
- Carry propagation handled with horizontal operations (minimize dependency chains)

#### Multiplication (Karatsuba Algorithm)
Traditional schoolbook: O(n²) - 16 multiplications for flt256
Karatsuba: O(n^1.585) - 9 multiplications (parallelizable)

```c
// Karatsuba: (A*B) = z2*2^(2m) + z1*2^m + z0
// Where: z2 = A_high * B_high
//        z0 = A_low * B_low
//        z1 = (A_high + A_low)(B_high + B_low) - z2 - z0
//
// All three multiplications can run in parallel (ILP)
```

Use `vpmuludq` for extended precision (full 64x64→128 bit products).

#### Division (Newton-Raphson Iteration)
```c
// Compute 1/B using Newton-Raphson: x_{n+1} = x_n * (2 - B*x_n)
// Then multiply A * (1/B)
// 
// Lookup table for initial approximation (16-bit mantissa → 16-bit reciprocal)
// Then iterate 3-4 times for full precision
```

Alternative: **Goldschmidt division** (more parallelizable, try both and benchmark).

---

## SECTION 4: Software Fallback Path (Edge Cases Only)

**Use SIMD for 99.9% of operations**. Software emulation ONLY for:
1. **NaN propagation** (mantissa payload preservation)
2. **Denormal handling** (gradual underflow, rare in practice)
3. **Exception flags** (if IEEE 754 strict flags needed)
4. **Special functions** (sin, cos, exp) - can use polynomial approximations

**DO NOT**:
- Use MPFR for basic arithmetic (addition, multiplication, division)
- Use GMP for mantissa operations (use SIMD intrinsics)
- Default to software when hardware path exists

**Acceptable**:
- Use SoftFloat as reference for testing (NOT production)
- Use MPFR for special functions (sin, cos, exp) IF polynomial approximations don't meet precision targets
- Hybrid: SIMD for mantissa arithmetic, scalar for rare edge cases

---

## SECTION 5: Operation Specifications

### Required Operations (All Must Meet Cycle Targets)

#### Basic Arithmetic
```c
flt256 flt256_add(flt256 a, flt256 b);        // ≤6 cycles
flt256 flt256_sub(flt256 a, flt256 b);        // ≤6 cycles
flt256 flt256_mul(flt256 a, flt256 b);        // ≤8 cycles
flt256 flt256_div(flt256 a, flt256 b);        // ≤20 cycles
flt256 flt256_fma(flt256 a, flt256 b, flt256 c); // ≤8 cycles (a*b+c, single rounding)
flt256 flt256_sqrt(flt256 a);                 // ≤18 cycles
flt256 flt256_neg(flt256 a);                  // ≤2 cycles (flip sign bit)
flt256 flt256_abs(flt256 a);                  // ≤2 cycles (clear sign bit)
```

#### Comparisons (Branchless)
```c
bool flt256_eq(flt256 a, flt256 b);   // ≤5 cycles (exact equality)
bool flt256_ne(flt256 a, flt256 b);   // ≤5 cycles
bool flt256_lt(flt256 a, flt256 b);   // ≤5 cycles
bool flt256_le(flt256 a, flt256 b);   // ≤5 cycles
bool flt256_gt(flt256 a, flt256 b);   // ≤5 cycles
bool flt256_ge(flt256 a, flt256 b);   // ≤5 cycles
```

#### Conversions
```c
flt256 flt256_from_i32(int32_t x);    // ≤8 cycles
flt256 flt256_from_i64(int64_t x);    // ≤10 cycles
flt256 flt256_from_f32(float x);      // ≤6 cycles
flt256 flt256_from_f64(double x);     // ≤8 cycles
flt256 flt256_from_string(const char* s); // Best-effort (parsing is inherently slow)

int32_t flt256_to_i32(flt256 a);      // ≤10 cycles (round toward zero)
int64_t flt256_to_i64(flt256 a);      // ≤10 cycles
float flt256_to_f32(flt256 a);        // ≤8 cycles (with rounding)
double flt256_to_f64(flt256 a);       // ≤8 cycles
char* flt256_to_string(flt256 a);     // Best-effort (formatting is slow)
```

#### Special Functions (Lower Priority)
```c
flt256 flt256_exp(flt256 a);    // Exponential (e^x)
flt256 flt256_log(flt256 a);    // Natural logarithm
flt256 flt256_pow(flt256 a, flt256 b); // a^b
flt256 flt256_sin(flt256 a);    // Sine
flt256 flt256_cos(flt256 a);    // Cosine
flt256 flt256_tan(flt256 a);    // Tangent
```

**For special functions**: Polynomial approximations (Remez) or MPFR acceptable (less performance-critical).

### flt512 Operations
Same operations as flt256, scaled to flt512 bit width. Cycle targets ~1.5x higher due to doubled data size.

---

## SECTION 6: LLVM IR Integration

### Type Definitions
```llvm
; flt256 as opaque struct (32-byte aligned)
%flt256 = type { <4 x i64> }  ; Or: type { [4 x i64] } for portable

; flt512 as opaque struct (64-byte aligned)
%flt512 = type { <8 x i64> }  ; Or: type { [8 x i64] }
```

### External Function Declarations
```llvm
declare %flt256 @aria_flt256_add(%flt256, %flt256)
declare %flt256 @aria_flt256_mul(%flt256, %flt256)
declare i1 @aria_flt256_lt(%flt256, %flt256)
; ... etc for all operations
```

### Codegen Strategy
When Aria compiler encounters:
```aria
var:x = flt256(1.5);
var:y = flt256(2.5);
var:z = x + y;
```

Generate IR:
```llvm
%x = call %flt256 @aria_flt256_from_f64(double 1.5)
%y = call %flt256 @aria_flt256_from_f64(double 2.5)
%z = call %flt256 @aria_flt256_add(%flt256 %x, %flt256 %y)
```

**Runtime Library**: Implement all `aria_flt256_*` functions in C/C++ with SIMD intrinsics, compile to `.o`, link with Aria programs.

---

## SECTION 7: Precision Justification (Why flt256/flt512?)

### Why Not IEEE 754 quad (float128)?
- **Precision**: 113-bit mantissa (~34 decimal digits) - insufficient for Nikola's 10^7 timestep simulations
- **Availability**: No hardware support (software emulation only, same problem as flt256)
- **Range**: 15-bit exponent (±10^4932) - adequate but not future-proof

### Why Not fixed256 (256-bit fixed-point)?
- **Dynamic Range**: Fixed-point has limited range (e.g., Q128.128 → ±10^38)
- **Precision Loss**: Large numbers lose precision at small scales
- **Overflow**: Multiplication requires careful scaling to prevent overflow
- **Usability**: Scientists expect floating-point semantics (exponent notation)

### Nikola's Requirements
- **Frequency**: 1000Hz (1ms per cycle)
- **Timesteps**: 10^7+ (long-running simulations)
- **Accumulation**: Tiny errors compound to corruption over time
- **Budget**: ~3,000,000 CPU cycles available per millisecond
- **Operations**: 5,000-10,000 floating-point ops per cycle

**IEEE 754 double drift**: At 10^7 timesteps, accumulated rounding errors cause memory corruption in Nikola's phase-locked systems. flt256's 71 decimal digits prevent this.

---

## SECTION 8: Testing & Validation

### Correctness Testing
1. **SoftFloat Reference**: John Hauser's bit-exact IEEE 754 implementation
   - Test all operations against SoftFloat quadruple-precision
   - Verify NaN propagation, infinity handling, denormals
   - Test all rounding modes (nearest, zero, up, down)

2. **Edge Cases**:
   - Zero: +0, -0
   - Infinity: +∞, -∞
   - NaN: Quiet, signaling, payload preservation
   - Denormals: Gradual underflow
   - Overflow: Exponent overflow → ±∞
   - Underflow: Exponent underflow → denormal or ±0

3. **Randomized Testing**: Generate 1 million random inputs, compare to SoftFloat

### Performance Testing
```c
// Benchmark template (use rdtsc)
uint64_t start = __rdtsc();
for (int i = 0; i < 100000; i++) {
    result = flt256_add(a, b);
}
uint64_t end = __rdtsc();
uint64_t cycles_per_op = (end - start) / 100000;
```

Test on:
- Intel Skylake-X (AVX-512)
- AMD Zen 4 (AVX-512)
- Intel Haswell (AVX-2 fallback)
- AMD Excavator (AVX-2 fallback)

**Required**: All operations must meet cycle targets on AVX-512 hardware.

### Security Testing (CRITICAL - Vulnerable Populations)

#### 1. Fuzzing (Required)
- **Tool**: AFL (American Fuzzy Lop) or libFuzzer
- **Duration**: 72+ hours continuous fuzzing
- **Target**: All operations (add, mul, div, conversions)
- **Success Criteria**: Zero crashes, zero hangs, zero assertion failures
- **Test Cases**: Save all crash-inducing inputs for regression testing

#### 2. Undefined Behavior (UBSan)
- **Tool**: Clang/GCC `-fsanitize=undefined`
- **Checks**:
  - Integer overflow (exponent arithmetic, mantissa shifts)
  - Signed overflow (exponent addition/subtraction)
  - Shift overflow (mantissa alignment, normalization)
  - Null pointer dereference
  - Uninitialized memory reads
- **Success Criteria**: Zero UBSan errors across all test cases

#### 3. Memory Safety (ASan/MSan)
- **AddressSanitizer**: Detect buffer overflows, use-after-free
- **MemorySanitizer**: Detect uninitialized memory reads
- **Test**: All SIMD operations (ensure bounds checking)
- **Success Criteria**: Zero ASan/MSan errors

#### 4. Thread Safety (TSan)
- **ThreadSanitizer**: Detect data races (if operations are used in multi-threaded code)
- **Test**: Concurrent calls to all operations
- **Success Criteria**: Zero TSan warnings

#### 5. Constant-Time Operations (Side-Channel Resistance)
- **Why**: Comparisons and conversions must not leak information via timing
- **Tool**: `valgrind --tool=cachegrind` or custom timing analysis
- **Test**: Compare timing across different inputs (NaN, zero, normal, denormal)
- **Success Criteria**: No timing variation based on input values (branchless code)

#### 6. Static Analysis
- **Tools**: Coverity, Clang Static Analyzer, Clang-Tidy
- **Checks**:
  - Buffer overflows
  - Null pointer dereferences
  - Memory leaks
  - Logic errors (dead code, infinite loops)
- **Success Criteria**: Zero warnings at highest sensitivity level

#### 7. Formal Verification (If Feasible)
- **Tools**: Coq, Isabelle/HOL, F* (formal proof assistants)
- **Target**: Prove correctness of critical algorithms (addition, normalization)
- **Scope**: If time permits, formal proofs provide highest assurance
- **Alternative**: Exhaustive testing (cover all edge cases)

#### 8. Differential Testing
- **Method**: Compare against multiple reference implementations
  - SoftFloat (IEEE 754 reference)
  - MPFR (arbitrary precision)
  - GMP (low-level arithmetic)
- **Test**: 10 million randomized inputs, verify bit-exact agreement
- **Success Criteria**: 100% agreement on normal values, NaN payload preservation

**Why Security Matters**:
- Nikola serves children, elderly, disabled individuals
- Floating-point bugs can bypass safety checks (e.g., NaN comparison always false)
- Timing leaks can expose private information (constant-time is mandatory)
- Memory corruption can lead to arbitrary code execution
- **If someone is harmed because we were lazy, that outcome is unacceptable**

---

## SECTION 9: Deliverables Checklist

### Required Outputs
- [ ] **flt256.h / flt256.c** - Complete implementation (all operations)
- [ ] **flt512.h / flt512.c** - Complete implementation
- [ ] **Cycle Count Report** - Measured performance (rdtsc) for all operations
- [ ] **Correctness Test Suite** - SoftFloat comparison (1M+ test cases)
- [ ] **Security Test Results** - Fuzzing, ASan, UBSan, TSan, static analysis reports
- [ ] **LLVM IR Integration Guide** - How to call from Aria compiler
- [ ] **Algorithm Documentation** - Explain Karatsuba, Newton-Raphson, normalization
- [ ] **Optimization Notes** - Which techniques were tried, which worked best
- [ ] **Portability Matrix** - AVX-512 vs AVX-2 vs SSE4.2 performance
- [ ] **Edge Case Handling** - How NaN, Inf, denormals are processed
- [ ] **Rounding Mode Support** - Implementation and testing
- [ ] **Conversion Functions** - int/float/double to/from flt256/flt512
- [ ] **Special Functions** - sin/cos/exp/log (if implemented)
- [ ] **Memory Layout Diagram** - Bit-level structure of flt256/flt512
- [ ] **Benchmarking Code** - Reproducible performance tests

### Optional (But Valuable)
- [ ] **Formal Proofs** - Coq/Isabelle verification (if feasible)
- [ ] **Compiler Auto-Vectorization Analysis** - Can compiler auto-SIMD your code?
- [ ] **Power Analysis** - Energy efficiency (important for embedded Nikola)

---

## SECTION 10: Reference Implementations

### Study These (Do NOT Copy - Learn Patterns)
1. **Intel SVML** (Short Vector Math Library)
   - Proprietary, but disassembly shows techniques
   - Lookup tables for reciprocals, polynomial coefficients
   - Branchless logic with `vpternlogd`

2. **SLEEF** (SIMD Library for Evaluating Elementary Functions)
   - Open source, high-performance SIMD math
   - Remez polynomial approximations
   - Study their sin/cos/exp implementations

3. **SoftFloat** by John Hauser
   - Reference IEEE 754 implementation
   - Use for testing, NOT production (too slow)
   - Study edge case handling (NaN, Inf, denormals)

4. **GMP** (GNU Multiple Precision)
   - Low-level arbitrary precision arithmetic
   - Study limb-based algorithms
   - Their Karatsuba implementation is battle-tested

5. **Intel Decimal Floating-Point Math Library**
   - Decimal128/Decimal256 (similar challenges to binary256)
   - Study their rounding and normalization

---

## SECTION 11: Anti-Patterns (What NOT to Do)

### ❌ DO NOT:
1. **Use MPFR for Basic Arithmetic**
   - MPFR is software emulation (20-50 cycles for addition)
   - Violates performance targets
   - Only acceptable for special functions IF polynomial approximations fail

2. **Default to Software Emulation**
   - "We'll optimize later" ❌ - This IS later (permanent freeze)
   - SIMD MUST be the primary path

3. **Ignore Cycle Count Targets**
   - "10 cycles for addition is close enough" ❌
   - Targets are absolute, not guidelines

4. **Implement Once and Ship**
   - Try 2-3 algorithms per operation (Karatsuba vs schoolbook, Newton-Raphson vs Goldschmidt)
   - Benchmark all approaches

5. **Skip Edge Cases**
   - NaN, Inf, denormals must be handled correctly
   - IEEE 754 compliance is mandatory

6. **Use Branching in Hot Paths**
   - `if (exp_a > exp_b)` ❌ - Use `vpternlogd` for branchless selection
   - Branch mispredicts cost 15-20 cycles

7. **Assume Alignment**
   - Always enforce 32-byte (flt256) and 64-byte (flt512) alignment
   - Use `alignas` or `posix_memalign`

8. **Forget Rounding Modes**
   - Default to nearest-ties-even is fine for 99% of users
   - But all 4 modes must be implemented (IEEE 754 requirement)

9. **Ignore Security**
   - UBSan/ASan/fuzzing are NOT optional
   - Vulnerable populations depend on this being bulletproof

10. **Compromise on Timeline**
    - "We need to ship by Q1" ❌ - There is NO deadline
    - Perfection > arbitrary release dates

---

## SECTION 12: Success Criteria (All Must Pass)

### Quality Gates (100% Required)
1. ✅ **Performance**: All operations meet or beat cycle targets
2. ✅ **Correctness**: 100% bit-exact agreement with SoftFloat on 1M+ test cases
3. ✅ **Edge Cases**: NaN, Inf, denormals, ±0 handled per IEEE 754
4. ✅ **Rounding**: All 4 rounding modes work correctly
5. ✅ **Security**: Zero errors from fuzzing, ASan, UBSan, TSan, static analysis
6. ✅ **Portability**: Works on AVX-512, AVX-2, SSE4.2 (with graceful degradation)
7. ✅ **Documentation**: Every algorithm explained, every optimization justified
8. ✅ **Integration**: LLVM IR codegen tested, compiles with Aria programs

**Ship Only When All 8 Pass** - no exceptions, no compromises.

---

## Final Notes

This is production code for a language that will freeze at 1.0 and serve vulnerable populations. We get **one chance** to build this correctly.

Take your time. Try multiple approaches. Benchmark obsessively. Test exhaustively. Fuzz relentlessly. Prove correctness formally if possible.

**6 weeks or 6 years - perfection is the only acceptable outcome.**

The children, elderly, and disabled individuals who will interact with Nikola are depending on us to get this right. Laziness, corner-cutting, or "good enough for now" thinking is unacceptable.

Build something you can be proud of forever. Because it will last forever.

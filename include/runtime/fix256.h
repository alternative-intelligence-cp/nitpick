/**
 * ARIA-025: fix256 Deterministic Fixed-Point Arithmetic
 *
 * Q128.128 format for deterministic physics and robotic control
 * Prevents floating-point accumulation drift in safety-critical systems
 */

#ifndef ARIA_RUNTIME_FIX256_H
#define ARIA_RUNTIME_FIX256_H

#include <stdint.h>

// Support CUDA compilation
#ifdef __CUDACC__
#define ARIA_DEVICE_HOST __device__ __host__
#else
#define ARIA_DEVICE_HOST
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════
// fix256 Type Definition
// Matches compiler backend: %struct.fix256 = type { [4 x i64] }
// Layout: [fractional_low, fractional_high, integer_low, integer_high]
// ═══════════════════════════════════════════════════════════════════════

typedef struct {
    uint64_t limbs[4];  // Q128.128: limbs[0-1] fractional, limbs[2-3] integer
} npk_fix256_t;

// ═══════════════════════════════════════════════════════════════════════
// fix256 Arithmetic Operations
// ═══════════════════════════════════════════════════════════════════════

/**
 * Add two Q128.128 fixed-point numbers
 * Returns a + b with overflow detection (Phase 4 TBB safety)
 */
ARIA_DEVICE_HOST npk_fix256_t npk_fix256_add(npk_fix256_t a, npk_fix256_t b);

/**
 * Subtract two Q128.128 fixed-point numbers
 * Returns a - b with underflow detection (Phase 4 TBB safety)
 */
ARIA_DEVICE_HOST npk_fix256_t npk_fix256_sub(npk_fix256_t a, npk_fix256_t b);

/**
 * Multiply two Q128.128 fixed-point numbers
 * Returns a * b with proper decimal point realignment
 * Uses 512-bit intermediate, extracts middle 256 bits
 */
ARIA_DEVICE_HOST npk_fix256_t npk_fix256_mul(npk_fix256_t a, npk_fix256_t b);

/**
 * Divide two Q128.128 fixed-point numbers
 * Returns a / b with proper decimal point alignment
 * Returns zero if b is zero (TBB ERR in Phase 4)
 */
ARIA_DEVICE_HOST npk_fix256_t npk_fix256_div(npk_fix256_t a, npk_fix256_t b);

// ═══════════════════════════════════════════════════════════════════════
// fix256 Comparison Operations
// ═══════════════════════════════════════════════════════════════════════

/**
 * Compare two Q128.128 fixed-point numbers
 * Returns: -1 if a < b, 0 if a == b, 1 if a > b
 */
ARIA_DEVICE_HOST int npk_fix256_cmp(npk_fix256_t a, npk_fix256_t b);

/**
 * Test equality of two Q128.128 fixed-point numbers
 * Returns: true if a == b
 */
ARIA_DEVICE_HOST int npk_fix256_eq(npk_fix256_t a, npk_fix256_t b);

/**
 * Test if a < b
 */
ARIA_DEVICE_HOST int npk_fix256_lt(npk_fix256_t a, npk_fix256_t b);

/**
 * Test if a <= b
 */
ARIA_DEVICE_HOST int npk_fix256_le(npk_fix256_t a, npk_fix256_t b);

/**
 * Test if a > b
 */
ARIA_DEVICE_HOST int npk_fix256_gt(npk_fix256_t a, npk_fix256_t b);

/**
 * Test if a >= b
 */
ARIA_DEVICE_HOST int npk_fix256_ge(npk_fix256_t a, npk_fix256_t b);

// ═══════════════════════════════════════════════════════════════════════
// fix256 Conversion Functions
// ═══════════════════════════════════════════════════════════════════════

/**
 * Convert 32-bit signed integer to Q128.128 fixed-point
 */
ARIA_DEVICE_HOST npk_fix256_t npk_fix256_from_i32(int32_t val);

/**
 * Convert 64-bit signed integer to Q128.128 fixed-point
 */
ARIA_DEVICE_HOST npk_fix256_t npk_fix256_from_i64(int64_t val);

/**
 * Convert Q128.128 fixed-point to 32-bit signed integer (truncates fractional part)
 */
ARIA_DEVICE_HOST int32_t npk_fix256_to_i32(npk_fix256_t val);

/**
 * Convert Q128.128 fixed-point to 64-bit signed integer (truncates fractional part)
 */
ARIA_DEVICE_HOST int64_t npk_fix256_to_i64(npk_fix256_t val);

/**
 * Convert 32-bit float to Q128.128 fixed-point (APPROXIMATE - for I/O only!)
 * WARNING: float has ~7 decimal digits precision, fix256 has ~38 decimal digits
 */
ARIA_DEVICE_HOST npk_fix256_t npk_fix256_from_f32(float val);

/**
 * Convert 64-bit double to Q128.128 fixed-point (APPROXIMATE - for I/O only!)
 * WARNING: double has ~15 decimal digits precision, fix256 has ~38 decimal digits
 */
ARIA_DEVICE_HOST npk_fix256_t npk_fix256_from_f64(double val);

/**
 * Convert Q128.128 fixed-point to 32-bit float (APPROXIMATE - for display only!)
 * WARNING: Loss of precision - use only for human-readable output
 */
ARIA_DEVICE_HOST float npk_fix256_to_f32(npk_fix256_t val);

/**
 * Convert Q128.128 fixed-point to 64-bit double (APPROXIMATE - for display only!)
 * WARNING: Loss of precision - use only for human-readable output
 */
ARIA_DEVICE_HOST double npk_fix256_to_f64(npk_fix256_t val);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_FIX256_H

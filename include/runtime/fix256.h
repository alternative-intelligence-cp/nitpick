/**
 * ARIA-025: fix256 Deterministic Fixed-Point Arithmetic
 *
 * Q128.128 format for deterministic physics and robotic control
 * Prevents floating-point accumulation drift in safety-critical systems
 */

#ifndef ARIA_RUNTIME_FIX256_H
#define ARIA_RUNTIME_FIX256_H

#include <stdint.h>

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
} aria_fix256_t;

// ═══════════════════════════════════════════════════════════════════════
// fix256 Arithmetic Operations
// ═══════════════════════════════════════════════════════════════════════

/**
 * Add two Q128.128 fixed-point numbers
 * Returns a + b with overflow detection (Phase 4 TBB safety)
 */
aria_fix256_t aria_fix256_add(aria_fix256_t a, aria_fix256_t b);

/**
 * Subtract two Q128.128 fixed-point numbers
 * Returns a - b with underflow detection (Phase 4 TBB safety)
 */
aria_fix256_t aria_fix256_sub(aria_fix256_t a, aria_fix256_t b);

/**
 * Multiply two Q128.128 fixed-point numbers
 * Returns a * b with proper decimal point realignment
 * Uses 512-bit intermediate, extracts middle 256 bits
 */
aria_fix256_t aria_fix256_mul(aria_fix256_t a, aria_fix256_t b);

/**
 * Divide two Q128.128 fixed-point numbers
 * Returns a / b with proper decimal point alignment
 * Returns zero if b is zero (TBB ERR in Phase 4)
 */
aria_fix256_t aria_fix256_div(aria_fix256_t a, aria_fix256_t b);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_FIX256_H

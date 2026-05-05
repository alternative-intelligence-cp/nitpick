/**
 * Aria Standard Library - TBB (Twisted Balanced Binary) Types
 * 
 * TBB types provide error-propagating integers with symmetric ranges.
 * Unlike standard two's complement integers, TBB reserves the minimum 
 * value as an error sentinel, creating perfectly symmetric ranges and
 * enabling safe arithmetic with automatic overflow detection.
 * 
 * Key Properties:
 * - Symmetric signed ranges (no two's complement asymmetry)
 * - MIN_INT reserved as error sentinel (ERR)
 * - Sticky error propagation: ERR op x = ERR
 * - Overflow automatically converts to ERR
 * - Division by zero produces ERR
 * 
 * Reference: docs/specs/TBB_TYPE_SYSTEM_SPEC.md
 */

#ifndef ARIA_RUNTIME_TBB_H
#define ARIA_RUNTIME_TBB_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// TBB Constants and Sentinels
// ============================================================================

// tbb8 constants
#define TBB8_ERR    ((int8_t)(-128))     // 0x80
#define TBB8_MIN    ((int8_t)(-127))
#define TBB8_MAX    ((int8_t)(127))

// tbb16 constants
#define TBB16_ERR   ((int16_t)(-32768))  // 0x8000
#define TBB16_MIN   ((int16_t)(-32767))
#define TBB16_MAX   ((int16_t)(32767))

// tbb32 constants
#define TBB32_ERR   ((int32_t)(-2147483648LL))  // 0x80000000
#define TBB32_MIN   ((int32_t)(-2147483647))
#define TBB32_MAX   ((int32_t)(2147483647))

// tbb64 constants
#define TBB64_ERR   ((int64_t)(-9223372036854775807LL - 1))  // INT64_MIN
#define TBB64_MIN   ((int64_t)(-9223372036854775807LL))
#define TBB64_MAX   ((int64_t)(9223372036854775807LL))

// ============================================================================
// TBB8 Operations (8-bit)
// ============================================================================

/**
 * Create a tbb8 from an int64
 * If value is out of range [-127, +127], returns ERR
 */
int8_t tbb8_from_int(int64_t value);

/**
 * Check if a tbb8 value is the error sentinel
 */
bool tbb8_is_err(int8_t value);

/**
 * Convert tbb8 to int64
 * Note: ERR (-128) is converted to -128 in the wider type
 */
int64_t tbb8_to_int(int8_t value);

/**
 * Add two tbb8 values with overflow detection
 * Returns ERR if either operand is ERR or result overflows
 */
int8_t tbb8_add(int8_t a, int8_t b);

/**
 * Subtract two tbb8 values with overflow detection
 * Returns ERR if either operand is ERR or result overflows
 */
int8_t tbb8_sub(int8_t a, int8_t b);

/**
 * Multiply two tbb8 values with overflow detection
 * Returns ERR if either operand is ERR or result overflows
 */
int8_t tbb8_mul(int8_t a, int8_t b);

/**
 * Divide two tbb8 values with error handling
 * Returns ERR if either operand is ERR or divisor is zero
 */
int8_t tbb8_div(int8_t a, int8_t b);

/**
 * Negate a tbb8 value
 * Returns ERR if operand is ERR (sticky propagation)
 * Works correctly for all valid values due to symmetric range
 */
int8_t tbb8_neg(int8_t a);

/**
 * Absolute value of a tbb8
 * Returns ERR if operand is ERR
 * Works correctly for all valid values due to symmetric range
 */
int8_t tbb8_abs(int8_t a);

// ============================================================================
// TBB16 Operations (16-bit)
// ============================================================================

int16_t tbb16_from_int(int64_t value);
bool tbb16_is_err(int16_t value);
int64_t tbb16_to_int(int16_t value);
int16_t tbb16_add(int16_t a, int16_t b);
int16_t tbb16_sub(int16_t a, int16_t b);
int16_t tbb16_mul(int16_t a, int16_t b);
int16_t tbb16_div(int16_t a, int16_t b);
int16_t tbb16_neg(int16_t a);
int16_t tbb16_abs(int16_t a);

// ============================================================================
// TBB32 Operations (32-bit)
// ============================================================================

int32_t tbb32_from_int(int64_t value);
bool tbb32_is_err(int32_t value);
int64_t tbb32_to_int(int32_t value);
int32_t tbb32_add(int32_t a, int32_t b);
int32_t tbb32_sub(int32_t a, int32_t b);
int32_t tbb32_mul(int32_t a, int32_t b);
int32_t tbb32_div(int32_t a, int32_t b);
int32_t tbb32_neg(int32_t a);
int32_t tbb32_abs(int32_t a);

// ============================================================================
// TBB64 Operations (64-bit)
// ============================================================================

/**
 * Create a tbb64 from an int64
 * Special handling: if input == INT64_MIN, returns ERR
 * Otherwise checks against TBB64_MIN and TBB64_MAX
 */
int64_t tbb64_from_int(int64_t value);

bool tbb64_is_err(int64_t value);
int64_t tbb64_to_int(int64_t value);

/**
 * TBB64 arithmetic operations
 * Note: These use __int128 internally to detect overflow
 */
int64_t tbb64_add(int64_t a, int64_t b);
int64_t tbb64_sub(int64_t a, int64_t b);
int64_t tbb64_mul(int64_t a, int64_t b);
int64_t tbb64_div(int64_t a, int64_t b);
int64_t tbb64_neg(int64_t a);
int64_t tbb64_abs(int64_t a);

// ============================================================================
// TBB Widening Operations (ARIA-018: Sentinel-Preserving Conversion)
// ============================================================================
// These functions safely widen TBB types while preserving error semantics.
// Standard sign extension would convert source ERR to a valid value in the
// destination type (e.g., tbb8 ERR (-128) → -128 in tbb16, not tbb16 ERR).
// These intrinsics map source sentinel → destination sentinel.
//
// Usage: tbb16:wide = tbb_widen<tbb16>(tbb8:narrow);

/**
 * Widen tbb8 to tbb16 with sentinel preservation
 * ERR (-128) → ERR (-32768)
 */
int16_t npk_tbb_widen_8_16(int8_t val);

/**
 * Widen tbb8 to tbb32 with sentinel preservation
 * ERR (-128) → ERR (-2147483648)
 */
int32_t npk_tbb_widen_8_32(int8_t val);

/**
 * Widen tbb8 to tbb64 with sentinel preservation
 * ERR (-128) → ERR (INT64_MIN)
 */
int64_t npk_tbb_widen_8_64(int8_t val);

/**
 * Widen tbb16 to tbb32 with sentinel preservation
 * ERR (-32768) → ERR (-2147483648)
 */
int32_t npk_tbb_widen_16_32(int16_t val);

/**
 * Widen tbb16 to tbb64 with sentinel preservation
 * ERR (-32768) → ERR (INT64_MIN)
 */
int64_t npk_tbb_widen_16_64(int16_t val);

/**
 * Widen tbb32 to tbb64 with sentinel preservation
 * ERR (-2147483648) → ERR (INT64_MIN)
 */
int64_t npk_tbb_widen_32_64(int32_t val);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_TBB_H

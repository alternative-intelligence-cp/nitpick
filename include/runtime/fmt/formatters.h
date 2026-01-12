/**
 * Aria Type Formatters - Runtime String Formatting for Extended Types
 *
 * This module provides type-aware string formatting for Aria's advanced type system:
 * - TBB types (tbb8/16/32/64) with sentinel interception ("ERR" output)
 * - LBIM large integers (int128/256/512) with multi-precision conversion
 * - Exotic types (trit, tryte, nit, nyte) with balanced base conversion
 * - High-precision floats (flt256/512) with full precision output
 *
 * Reference: Extended Integer and Float Print Support specification
 */

#ifndef ARIA_RUNTIME_FMT_FORMATTERS_H
#define ARIA_RUNTIME_FMT_FORMATTERS_H

#include <stdint.h>
#include "runtime/lbim.h"
#include "runtime/lbim_extended.h"
#include "runtime/fix256.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// AriaString Structure (matches codegen layout)
// ============================================================================

/**
 * AriaString: Runtime string representation
 * This matches the struct layout used in ExprCodegen::createAriaString
 */
typedef struct {
    const char* data;
    int64_t length;
} AriaString;

// ============================================================================
// TBB Formatters (Sentinel Aware)
// These intercept ERR sentinel values and return "ERR" instead of raw integer
// ============================================================================

/**
 * Format tbb8 value with sentinel interception
 * @param val TBB8 value (-128 is ERR sentinel)
 * @return AriaString pointer, "ERR" if sentinel, otherwise decimal string
 */
AriaString* aria_format_tbb8(int8_t val);

/**
 * Format tbb16 value with sentinel interception
 * @param val TBB16 value (-32768 is ERR sentinel)
 * @return AriaString pointer, "ERR" if sentinel, otherwise decimal string
 */
AriaString* aria_format_tbb16(int16_t val);

/**
 * Format tbb32 value with sentinel interception
 * @param val TBB32 value (INT32_MIN is ERR sentinel)
 * @return AriaString pointer, "ERR" if sentinel, otherwise decimal string
 */
AriaString* aria_format_tbb32(int32_t val);

/**
 * Format tbb64 value with sentinel interception
 * @param val TBB64 value (INT64_MIN is ERR sentinel)
 * @return AriaString pointer, "ERR" if sentinel, otherwise decimal string
 */
AriaString* aria_format_tbb64(int64_t val);

// ============================================================================
// LBIM Large Integer Formatters
// Multi-precision decimal conversion for int128/256/512
// ============================================================================

/**
 * Format int128 value (signed)
 * @param val Pointer to 128-bit signed integer
 * @return AriaString pointer with decimal representation
 */
AriaString* aria_format_int128(const aria_int128_t* val);

/**
 * Format uint128 value (unsigned)
 * @param val Pointer to 128-bit unsigned integer
 * @return AriaString pointer with decimal representation
 */
AriaString* aria_format_uint128(const aria_int128_t* val);

/**
 * Format int256 value (signed)
 * @param val Pointer to 256-bit signed integer
 * @return AriaString pointer with decimal representation
 */
AriaString* aria_format_int256(const aria_int256_t* val);

/**
 * Format uint256 value (unsigned)
 * @param val Pointer to 256-bit unsigned integer
 * @return AriaString pointer with decimal representation
 */
AriaString* aria_format_uint256(const aria_int256_t* val);

/**
 * Format int512 value (signed)
 * @param val Pointer to 512-bit signed integer
 * @return AriaString pointer with decimal representation
 */
AriaString* aria_format_int512(const aria_int512_t* val);

/**
 * Format uint512 value (unsigned)
 * @param val Pointer to 512-bit unsigned integer
 * @return AriaString pointer with decimal representation
 */
AriaString* aria_format_uint512(const aria_int512_t* val);

/**
 * Format int1024 value (signed) - ARIA-025: Post-quantum cryptography
 * @param val Pointer to 1024-bit signed integer
 * @return AriaString pointer with decimal representation or "ERR"
 */
AriaString* aria_format_int1024(const aria_int1024_t* val);

/**
 * Format uint1024 value (unsigned) - ARIA-025: Post-quantum cryptography
 * @param val Pointer to 1024-bit unsigned integer
 * @return AriaString pointer with decimal representation
 */
AriaString* aria_format_uint1024(const aria_int1024_t* val);

/**
 * Format int2048 value (signed) - ARIA-025: RSA-2048 cryptography
 * @param val Pointer to 2048-bit signed integer
 * @return AriaString pointer with decimal representation
 */
AriaString* aria_format_int2048(const aria_int2048_t* val);

/**
 * Format uint2048 value (unsigned) - ARIA-025: RSA-2048 cryptography
 * @param val Pointer to 2048-bit unsigned integer
 * @return AriaString pointer with decimal representation
 */
AriaString* aria_format_uint2048(const aria_int2048_t* val);

/**
 * Format int4096 value (signed) - ARIA-025: Post-quantum RSA-4096 cryptography
 * @param val Pointer to 4096-bit signed integer
 * @return AriaString pointer with decimal representation
 */
AriaString* aria_format_int4096(const aria_int4096_t* val);

/**
 * Format uint4096 value (unsigned) - ARIA-025: Post-quantum RSA-4096 cryptography
 * @param val Pointer to 4096-bit unsigned integer
 * @return AriaString pointer with decimal representation
 */
AriaString* aria_format_uint4096(const aria_int4096_t* val);

/**
 * Format fix256 value (Q128.128 deterministic fixed-point) - ARIA-025
 * @param val Pointer to fix256 value
 * @return AriaString pointer with decimal representation or "ERR"
 */
AriaString* aria_format_fix256(const aria_fix256_t* val);

// ============================================================================
// Exotic Type Formatters (Balanced Ternary/Nonary)
// ============================================================================

/**
 * Format single trit value
 * @param val Trit value (-1, 0, or 1)
 * @return AriaString pointer: "T" for -1, "0" for 0, "1" for 1
 */
AriaString* aria_format_trit(int8_t val);

/**
 * Format tryte value (balanced ternary string)
 * @param val Tryte stored as uint16 with biased representation
 * @return AriaString pointer with balanced ternary representation (T, 0, 1)
 */
AriaString* aria_format_tryte(uint16_t val);

/**
 * Format single nit value (balanced nonary digit)
 * @param val Nit value (-4 to 4)
 * @return AriaString pointer: "D" for -4, "C" for -3, "B" for -2, "A" for -1,
 *         "0" to "4" for non-negative
 */
AriaString* aria_format_nit(int8_t val);

/**
 * Format nyte value (balanced nonary string)
 * @param val Nyte stored as uint16 with biased representation
 * @return AriaString pointer with balanced nonary representation
 */
AriaString* aria_format_nyte(uint16_t val);

// ============================================================================
// Standard Type Formatters (for consistency)
// ============================================================================

/**
 * Format standard int64 value
 * @param val 64-bit signed integer
 * @return AriaString pointer with decimal representation
 */
AriaString* aria_format_int64(int64_t val);

/**
 * Format standard double value
 * @param val 64-bit floating point
 * @return AriaString pointer with decimal representation
 */
AriaString* aria_format_float64(double val);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_FMT_FORMATTERS_H

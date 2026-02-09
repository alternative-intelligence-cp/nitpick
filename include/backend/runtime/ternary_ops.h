/**
 * @file include/backend/runtime/ternary_ops.h
 * @brief Runtime intrinsics for Balanced Ternary and Nonary arithmetic
 * 
 * Implements software emulation of ternary (base-3) and nonary (base-9)
 * arithmetic on binary hardware using lookup tables and efficient packing.
 * 
 * Type System:
 *   - trit: Single balanced ternary digit {-1, 0, 1}
 *   - tryte: 10 trits, range [-29524, 29524], stored as uint16
 *   - nit: Single balanced nonary digit {-4, -3, -2, -1, 0, 1, 2, 3, 4}
 *   - nyte: 5 nits, range [-29524, 29524], stored as uint16
 * 
 * Packing Strategy:
 *   - Tryte: Split-Byte (two 5-trit "trybbles" in uint8 each)
 *   - Nyte: Biased radix (single base-9 number with bias of 29524)
 * 
 * Error Handling:
 *   - Sticky error propagation via sentinel values
 *   - TRYTE_ERR = 0xFFFF, NYTE_ERR = 0xFFFF
 */

#ifndef ARIA_TERNARY_OPS_H
#define ARIA_TERNARY_OPS_H

#include <cstdint>

// Export C-compatible symbols for LLVM linkage
#ifdef __cplusplus
extern "C" {
#endif

// ==============================================================================
// Constants and Sentinels
// ==============================================================================

/// Error sentinel for tryte operations (outside valid range)
#define TRYTE_ERR 0xFFFF

/// Error sentinel for nyte operations (outside valid range)
#define NYTE_ERR 0xFFFF

/// Valid range for both tryte and nyte: [-29524, 29524]
/// (3^10 = 59049 states, symmetric around 0)
#define TERNARY_MIN -29524
#define TERNARY_MAX 29524

// ==============================================================================
// Atomic Trit Operations (Single Balanced Ternary Digit)
// ==============================================================================

/**
 * @brief Add two trits with carry support
 * @param a First trit (-1, 0, or 1)
 * @param b Second trit (-1, 0, or 1)
 * @param carry_in Input carry (-1, 0, or 1)
 * @return Packed result: (carry_out << 8) | (sum & 0xFF)
 * 
 * Full adder: a + b + carry_in = 3*carry_out + sum
 */
int16_t aria_trit_add_carry(int8_t a, int8_t b, int8_t carry_in);

/**
 * @brief Add two trits (without carry)
 * @return Result trit, saturated to [-1, 1]
 */
int8_t aria_trit_add(int8_t a, int8_t b);

/**
 * @brief Subtract two trits
 */
int8_t aria_trit_sub(int8_t a, int8_t b);

/**
 * @brief Multiply two trits
 */
int8_t aria_trit_mul(int8_t a, int8_t b);

/**
 * @brief Negate a trit (flip sign)
 */
int8_t aria_trit_neg(int8_t a);

/**
 * @brief Kleene logic AND for balanced ternary
 * @param a First trit (-1, 0, or 1)
 * @param b Second trit (-1, 0, or 1)
 * @return min(a, b) - takes most "false" value
 * 
 * Truth table: AND(-1,-1)=-1, AND(-1,0)=-1, AND(-1,1)=-1,
 *              AND(0,0)=0,   AND(0,1)=0,   AND(1,1)=1
 * ARIA Safety Fix (Gemini Batch 02, BUG-005): Uses min, not bitwise &
 */
int8_t aria_trit_and(int8_t a, int8_t b);

/**
 * @brief Kleene logic OR for balanced ternary
 * @param a First trit (-1, 0, or 1)
 * @param b Second trit (-1, 0, or 1)
 * @return max(a, b) - takes most "true" value
 * 
 * Truth table: OR(-1,-1)=-1, OR(-1,0)=0, OR(-1,1)=1,
 *              OR(0,0)=0,    OR(0,1)=1,  OR(1,1)=1
 * ARIA Safety Fix (Gemini Batch 02, BUG-005): Uses max, not bitwise |
 */
int8_t aria_trit_or(int8_t a, int8_t b);

/**
 * @brief Logical NOT for balanced ternary
 * @return Negated value: NOT(-1)=1, NOT(0)=0, NOT(1)=-1
 */
int8_t aria_trit_not(int8_t a);

// ==============================================================================
// Composite Tryte Operations (10 Trits, Split-Byte Packing)
// ==============================================================================

/**
 * @brief Add two trytes
 * @param a First tryte (uint16 packed representation)
 * @param b Second tryte (uint16 packed representation)
 * @return Result tryte, or TRYTE_ERR on overflow
 * 
 * Implementation uses ripple-carry addition on unpacked trybbles.
 * Returns TRYTE_ERR if either input is TRYTE_ERR (sticky error).
 */
uint16_t aria_tryte_add(uint16_t a, uint16_t b);

/**
 * @brief Subtract two trytes (a - b)
 * @return Result tryte, or TRYTE_ERR on overflow
 */
uint16_t aria_tryte_sub(uint16_t a, uint16_t b);

/**
 * @brief Multiply two trytes
 * @return Result tryte, or TRYTE_ERR on overflow
 */
uint16_t aria_tryte_mul(uint16_t a, uint16_t b);

/**
 * @brief Divide two trytes (Euclidean division)
 * @return Result tryte, or TRYTE_ERR if b == 0 or overflow
 */
uint16_t aria_tryte_div(uint16_t a, uint16_t b);

/**
 * @brief Modulo of two trytes
 * @return Result tryte, or TRYTE_ERR if b == 0
 */
uint16_t aria_tryte_mod(uint16_t a, uint16_t b);

/**
 * @brief Negate a tryte (flip all trits: 1 ↔ -1, 0 → 0)
 * @return Negated tryte, or TRYTE_ERR if input is TRYTE_ERR
 */
uint16_t aria_tryte_neg(uint16_t a);

/**
 * @brief Compare two trytes
 * @return -1 if a < b, 0 if a == b, 1 if a > b, -2 if either is ERR
 */
int32_t aria_tryte_cmp(uint16_t a, uint16_t b);

/**
 * @brief Convert binary int32 to tryte
 * @param value Integer in range [-29524, 29524]
 * @return Packed tryte, or TRYTE_ERR if out of range
 */
uint16_t aria_bin_to_tryte(int32_t value);

/**
 * @brief Convert tryte to binary int32
 * @param tryte Packed tryte representation
 * @return Signed integer, or INT32_MIN if tryte is TRYTE_ERR
 */
int32_t aria_tryte_to_bin(uint16_t tryte);

/**
 * @brief Extract a single trit from a tryte
 * @param tryte Packed tryte
 * @param index Trit position (0-9, where 0 is least significant)
 * @return Trit value (-1, 0, or 1), or -2 if index invalid or tryte is ERR
 */
int8_t aria_tryte_get_trit(uint16_t tryte, uint8_t index);

// ==============================================================================
// Atomic Nit Operations (Single Balanced Nonary Digit)
// ==============================================================================

/**
 * @brief Add two nits (base-9 digits)
 * @param a First nit (-4 to 4)
 * @param b Second nit (-4 to 4)
 * @return Result nit, saturated to [-4, 4]
 */
int8_t aria_nit_add(int8_t a, int8_t b);

/**
 * @brief Subtract two nits
 */
int8_t aria_nit_sub(int8_t a, int8_t b);

/**
 * @brief Multiply two nits
 */
int8_t aria_nit_mul(int8_t a, int8_t b);

/**
 * @brief Negate a nit
 */
int8_t aria_nit_neg(int8_t a);

/**
 * @brief Kleene logic AND for balanced nonary
 * @param a First nit [-4, 4]
 * @param b Second nit [-4, 4]
 * @return min(a, b)
 * 
 * ARIA Safety Fix (Gemini Batch 02, BUG-005): Uses min, not bitwise &
 */
int8_t aria_nit_and(int8_t a, int8_t b);

/**
 * @brief Kleene logic OR for balanced nonary
 * @param a First nit [-4, 4]
 * @param b Second nit [-4, 4]
 * @return max(a, b)
 * 
 * ARIA Safety Fix (Gemini Batch 02, BUG-005): Uses max, not bitwise |
 */
int8_t aria_nit_or(int8_t a, int8_t b);

/**
 * @brief Logical NOT for balanced nonary
 * @return Negated value
 */
int8_t aria_nit_not(int8_t a);

// ==============================================================================
// Composite Nyte Operations (5 Nits, Biased Radix Packing)
// ==============================================================================

/**
 * @brief Add two nytes
 * @param a First nyte (uint16 packed representation)
 * @param b Second nyte (uint16 packed representation)
 * @return Result nyte, or NYTE_ERR on overflow
 * 
 * Implementation uses biased binary arithmetic (faster than tryte).
 */
uint16_t aria_nyte_add(uint16_t a, uint16_t b);

/**
 * @brief Subtract two nytes (a - b)
 * @return Result nyte, or NYTE_ERR on overflow
 */
uint16_t aria_nyte_sub(uint16_t a, uint16_t b);

/**
 * @brief Multiply two nytes
 * @return Result nyte, or NYTE_ERR on overflow
 */
uint16_t aria_nyte_mul(uint16_t a, uint16_t b);

/**
 * @brief Divide two nytes
 * @return Result nyte, or NYTE_ERR if b == 0 or overflow
 */
uint16_t aria_nyte_div(uint16_t a, uint16_t b);

/**
 * @brief Modulo of two nytes
 * @return Result nyte, or NYTE_ERR if b == 0
 */
uint16_t aria_nyte_mod(uint16_t a, uint16_t b);

/**
 * @brief Negate a nyte
 * @return Negated nyte, or NYTE_ERR if input is NYTE_ERR
 */
uint16_t aria_nyte_neg(uint16_t a);

/**
 * @brief Compare two nytes
 * @return -1 if a < b, 0 if a == b, 1 if a > b, -2 if either is ERR
 */
int32_t aria_nyte_cmp(uint16_t a, uint16_t b);

/**
 * @brief Convert binary int32 to nyte
 * @param value Integer in range [-29524, 29524]
 * @return Packed nyte, or NYTE_ERR if out of range
 */
uint16_t aria_bin_to_nyte(int32_t value);

/**
 * @brief Convert nyte to binary int32
 * @param nyte Packed nyte representation
 * @return Signed integer, or INT32_MIN if nyte is NYTE_ERR
 */
int32_t aria_nyte_to_bin(uint16_t nyte);

/**
 * @brief Extract a single nit from a nyte
 * @param nyte Packed nyte
 * @param index Nit position (0-4, where 0 is least significant)
 * @return Nit value (-4 to 4), or INT8_MIN if index invalid or nyte is ERR
 */
int8_t aria_nyte_get_nit(uint16_t nyte, uint8_t index);

// ==============================================================================
// TBB (Ternary/Binary/Boolean) Type Conversions
// ==============================================================================

/**
 * @brief Convert int32 to TBB8 with overflow detection
 * @param value Integer value to convert
 * @return TBB8 value [-127, 127], or -128 (ERR) if out of range
 */
int8_t aria_tbb8_from_int(int32_t value);

/**
 * @brief Convert TBB8 to int32
 * @param value TBB8 value
 * @return int32 representation, or INT32_MIN if ERR
 */
int32_t aria_tbb8_to_int(int8_t value);

// ==============================================================================
/**
 * @brief Initialize lookup tables for ternary arithmetic
 * 
 * Must be called once before using any ternary/nonary operations.
 * Thread-safe (idempotent).
 */
void aria_ternary_init();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ARIA_TERNARY_OPS_H

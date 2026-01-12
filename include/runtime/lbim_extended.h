/**
 * @file include/runtime/lbim_extended.h
 * @brief Extension of the Limb-Based Integral Model for 2048/4096-bit arithmetic.
 * 
 * Part of the Aria Runtime Environment. 
 * Compliant with ARIA-024 (Large Integer Support).
 */

#ifndef ARIA_RUNTIME_LBIM_EXTENDED_H
#define ARIA_RUNTIME_LBIM_EXTENDED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════
// Type Definitions
// ═══════════════════════════════════════════════════════════════════════

#define ARIA_INT2048_LIMBS 32
#define ARIA_INT4096_LIMBS 64

/**
 * @brief 2048-bit unsigned integer structure.
 * Used for both int2048 and uint2048 arithmetic (two's complement).
 * 
 * Layout: Little-endian array of 32 uint64_t limbs.
 * Alignment: 8 bytes (alignof(uint64_t)).
 */
typedef struct {
    uint64_t limbs[ARIA_INT2048_LIMBS];
} aria_int2048_t;

/**
 * @brief 4096-bit unsigned integer structure.
 * Used for both int4096 and uint4096 arithmetic (two's complement).
 * 
 * Layout: Little-endian array of 64 uint64_t limbs.
 * Alignment: 8 bytes (alignof(uint64_t)).
 */
typedef struct {
    uint64_t limbs[ARIA_INT4096_LIMBS];
} aria_int4096_t;

// ═══════════════════════════════════════════════════════════════════════
// 2048-bit Arithmetic Operations
// ═══════════════════════════════════════════════════════════════════════

/**
 * @brief Adds two 2048-bit integers.
 * @param a Augend
 * @param b Addend
 * @return Sum (a + b) modulo 2^2048
 */
aria_int2048_t aria_lbim_add2048(const aria_int2048_t* a, const aria_int2048_t* b);

/**
 * @brief Subtracts two 2048-bit integers.
 * @param a Minuend
 * @param b Subtrahend
 * @return Difference (a - b) modulo 2^2048
 */
aria_int2048_t aria_lbim_sub2048(const aria_int2048_t* a, const aria_int2048_t* b);

/**
 * @brief Multiplies two 2048-bit integers.
 * @param a Multiplicand
 * @param b Multiplier
 * @return Low 2048 bits of the product (a * b)
 */
aria_int2048_t aria_lbim_mul2048(const aria_int2048_t* a, const aria_int2048_t* b);

// ═══════════════════════════════════════════════════════════════════════
// Division and Modulo
// ═══════════════════════════════════════════════════════════════════════

/**
 * @brief Unsigned division.
 * @return Quotient (a / b). Returns ERR sentinel if b == 0 (fail-hard policy).
 */
aria_int2048_t aria_lbim_udiv2048(const aria_int2048_t* dividend, const aria_int2048_t* divisor);

/**
 * @brief Unsigned modulo.
 * @return Remainder (a % b). Returns ERR sentinel if b == 0.
 */
aria_int2048_t aria_lbim_umod2048(const aria_int2048_t* dividend, const aria_int2048_t* divisor);

/**
 * @brief Signed division.
 * Handles two's complement inputs.
 */
aria_int2048_t aria_lbim_sdiv2048(const aria_int2048_t* dividend, const aria_int2048_t* divisor);

/**
 * @brief Signed modulo.
 * Result has the sign of the dividend.
 */
aria_int2048_t aria_lbim_smod2048(const aria_int2048_t* dividend, const aria_int2048_t* divisor);

// ═══════════════════════════════════════════════════════════════════════
// Bitwise and Shift Operations
// ═══════════════════════════════════════════════════════════════════════

aria_int2048_t aria_lbim_and2048(aria_int2048_t a, aria_int2048_t b);
aria_int2048_t aria_lbim_or2048(aria_int2048_t a, aria_int2048_t b);
aria_int2048_t aria_lbim_xor2048(aria_int2048_t a, aria_int2048_t b);
aria_int2048_t aria_lbim_not2048(aria_int2048_t a);

/**
 * @brief Logical Left Shift (<<).
 * @param shift Number of bits to shift.
 */
aria_int2048_t aria_lbim_shl2048(aria_int2048_t a, uint32_t shift);

/**
 * @brief Logical Right Shift (>>>).
 * Zero-fills the most significant bits.
 */
aria_int2048_t aria_lbim_lshr2048(aria_int2048_t a, uint32_t shift);

/**
 * @brief Arithmetic Right Shift (>>).
 * Sign-extends based on the most significant bit.
 */
aria_int2048_t aria_lbim_ashr2048(aria_int2048_t a, uint32_t shift);

// ═══════════════════════════════════════════════════════════════════════
// Comparisons
// ═══════════════════════════════════════════════════════════════════════

/**
 * @return -1 if a < b, 0 if a == b, 1 if a > b (Unsigned)
 */
int32_t aria_lbim_ucmp2048(const aria_int2048_t* a, const aria_int2048_t* b);

/**
 * @return -1 if a < b, 0 if a == b, 1 if a > b (Signed)
 */
int32_t aria_lbim_scmp2048(const aria_int2048_t* a, const aria_int2048_t* b);

bool aria_lbim_eq2048(const aria_int2048_t* a, const aria_int2048_t* b);

// ═══════════════════════════════════════════════════════════════════════
// 4096-bit Arithmetic Operations
// ═══════════════════════════════════════════════════════════════════════

aria_int4096_t aria_lbim_add4096(const aria_int4096_t* a, const aria_int4096_t* b);
aria_int4096_t aria_lbim_sub4096(const aria_int4096_t* a, const aria_int4096_t* b);
aria_int4096_t aria_lbim_mul4096(const aria_int4096_t* a, const aria_int4096_t* b);
aria_int4096_t aria_lbim_sdiv4096(const aria_int4096_t* a, const aria_int4096_t* b);
aria_int4096_t aria_lbim_srem4096(const aria_int4096_t* a, const aria_int4096_t* b);
aria_int4096_t aria_lbim_smod4096(const aria_int4096_t* a, const aria_int4096_t* b);
aria_int4096_t aria_lbim_udiv4096(const aria_int4096_t* a, const aria_int4096_t* b);
aria_int4096_t aria_lbim_urem4096(const aria_int4096_t* a, const aria_int4096_t* b);
aria_int4096_t aria_lbim_umod4096(const aria_int4096_t* a, const aria_int4096_t* b);

// Bitwise operations
aria_int4096_t aria_lbim_and4096(aria_int4096_t a, aria_int4096_t b);
aria_int4096_t aria_lbim_or4096(aria_int4096_t a, aria_int4096_t b);
aria_int4096_t aria_lbim_xor4096(aria_int4096_t a, aria_int4096_t b);
aria_int4096_t aria_lbim_not4096(aria_int4096_t a);

// Shift operations
aria_int4096_t aria_lbim_shl4096(aria_int4096_t a, uint32_t shift);
aria_int4096_t aria_lbim_lshr4096(aria_int4096_t a, uint32_t shift);
aria_int4096_t aria_lbim_ashr4096(aria_int4096_t a, uint32_t shift);

// Comparisons
int32_t aria_lbim_ucmp4096(const aria_int4096_t* a, const aria_int4096_t* b);
int32_t aria_lbim_scmp4096(const aria_int4096_t* a, const aria_int4096_t* b);
bool aria_lbim_eq4096(const aria_int4096_t* a, const aria_int4096_t* b);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_LBIM_EXTENDED_H

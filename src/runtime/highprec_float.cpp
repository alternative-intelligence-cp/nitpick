/**
 * @file highprec_float.cpp
 * @brief High-Precision Floating-Point Implementation
 *
 * ARIA-017: High-Precision Floating-Point Core Design
 *
 * This file implements flt256 and flt512 arithmetic operations.
 * Phase 1: Scalar reference implementation for correctness.
 * Phase 2 (future): AVX-512 SIMD optimization.
 */

#include "runtime/highprec_float.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace npk {
namespace runtime {

// ============================================================================
// flt256 Static Factory Methods
// ============================================================================

flt256 flt256::zero() {
    flt256 result;
    std::memset(result.limbs, 0, sizeof(result.limbs));
    return result;
}

flt256 flt256::one() {
    flt256 result;
    std::memset(result.limbs, 0, sizeof(result.limbs));
    // Exponent = bias (262143) for 2^0 = 1
    // Sign = 0, Exponent in bits 236-254 of limbs[3]
    // For one: mantissa = 1.0 (implicit), so mantissa bits = 0
    // Exponent field = 262143 (bias + 0)
    uint64_t exp_field = static_cast<uint64_t>(EXPONENT_BIAS) << (64 - 19 - 1);
    result.limbs[3] = exp_field;
    return result;
}

flt256 flt256::negative_one() {
    flt256 result = one();
    result.limbs[3] |= (1ULL << 63);  // Set sign bit
    return result;
}

flt256 flt256::infinity() {
    flt256 result;
    std::memset(result.limbs, 0, sizeof(result.limbs));
    // All exponent bits set, mantissa = 0
    uint64_t max_exp = ((1ULL << EXPONENT_BITS) - 1) << (64 - EXPONENT_BITS - 1);
    result.limbs[3] = max_exp;
    return result;
}

flt256 flt256::negative_infinity() {
    flt256 result = infinity();
    result.limbs[3] |= (1ULL << 63);
    return result;
}

flt256 flt256::quiet_nan() {
    flt256 result;
    std::memset(result.limbs, 0, sizeof(result.limbs));
    // All exponent bits set, mantissa != 0 (quiet NaN has MSB of mantissa set)
    uint64_t max_exp = ((1ULL << EXPONENT_BITS) - 1) << (64 - EXPONENT_BITS - 1);
    result.limbs[3] = max_exp | (1ULL << (64 - EXPONENT_BITS - 2));  // Set quiet bit
    return result;
}

flt256 flt256::quiet_nan_with_payload(uint64_t payload) {
    flt256 result = quiet_nan();
    result.limbs[0] = payload;  // Store payload in low limb
    return result;
}

// ============================================================================
// flt256 Predicates
// ============================================================================

bool flt256::is_zero() const {
    // Zero if all limbs are zero (handles +0 and -0)
    return (limbs[0] == 0) && (limbs[1] == 0) && (limbs[2] == 0) &&
           ((limbs[3] & 0x7FFFFFFFFFFFFFFFULL) == 0);
}

bool flt256::is_negative() const {
    return (limbs[3] >> 63) != 0;
}

bool flt256::is_infinity() const {
    // Infinity: max exponent, zero mantissa
    uint64_t exp_mask = ((1ULL << EXPONENT_BITS) - 1) << (64 - EXPONENT_BITS - 1);
    uint64_t mantissa_high_mask = (1ULL << (64 - EXPONENT_BITS - 1)) - 1;

    uint64_t exp = limbs[3] & exp_mask;
    uint64_t mantissa_high = limbs[3] & mantissa_high_mask;

    return (exp == exp_mask) && (mantissa_high == 0) &&
           (limbs[0] == 0) && (limbs[1] == 0) && (limbs[2] == 0);
}

bool flt256::is_nan() const {
    // NaN: max exponent, non-zero mantissa
    uint64_t exp_mask = ((1ULL << EXPONENT_BITS) - 1) << (64 - EXPONENT_BITS - 1);
    uint64_t mantissa_high_mask = (1ULL << (64 - EXPONENT_BITS - 1)) - 1;

    uint64_t exp = limbs[3] & exp_mask;
    uint64_t mantissa_high = limbs[3] & mantissa_high_mask;

    bool max_exp = (exp == exp_mask);
    bool nonzero_mantissa = (mantissa_high != 0) || (limbs[0] != 0) ||
                            (limbs[1] != 0) || (limbs[2] != 0);

    return max_exp && nonzero_mantissa;
}

bool flt256::is_normal() const {
    if (is_zero() || is_infinity() || is_nan()) return false;

    uint64_t exp_mask = ((1ULL << EXPONENT_BITS) - 1) << (64 - EXPONENT_BITS - 1);
    uint64_t exp = limbs[3] & exp_mask;

    return exp != 0;  // Normal numbers have non-zero exponent
}

bool flt256::is_subnormal() const {
    if (is_zero()) return false;

    uint64_t exp_mask = ((1ULL << EXPONENT_BITS) - 1) << (64 - EXPONENT_BITS - 1);
    uint64_t exp = limbs[3] & exp_mask;

    return exp == 0;  // Subnormal numbers have zero exponent but non-zero mantissa
}

// ============================================================================
// flt256 Sign Manipulation
// ============================================================================

flt256 flt256::negate() const {
    flt256 result = *this;
    result.limbs[3] ^= (1ULL << 63);  // Flip sign bit
    return result;
}

flt256 flt256::abs() const {
    flt256 result = *this;
    result.limbs[3] &= ~(1ULL << 63);  // Clear sign bit
    return result;
}

int flt256::get_sign() const {
    return (limbs[3] >> 63) ? -1 : 1;
}

int64_t flt256::get_exponent() const {
    uint64_t exp_mask = ((1ULL << EXPONENT_BITS) - 1) << (64 - EXPONENT_BITS - 1);
    uint64_t raw_exp = (limbs[3] & exp_mask) >> (64 - EXPONENT_BITS - 1);
    return static_cast<int64_t>(raw_exp) - EXPONENT_BIAS;
}

void flt256::get_mantissa(uint64_t out[4]) const {
    out[0] = limbs[0];
    out[1] = limbs[1];
    out[2] = limbs[2];
    // Mask out sign and exponent from high limb
    uint64_t mantissa_high_mask = (1ULL << (64 - EXPONENT_BITS - 1)) - 1;
    out[3] = limbs[3] & mantissa_high_mask;
}

// ============================================================================
// flt512 Static Factory Methods
// ============================================================================

flt512 flt512::zero() {
    flt512 result;
    std::memset(result.limbs, 0, sizeof(result.limbs));
    return result;
}

flt512 flt512::one() {
    flt512 result;
    std::memset(result.limbs, 0, sizeof(result.limbs));
    uint64_t exp_field = static_cast<uint64_t>(EXPONENT_BIAS) << (64 - 23 - 1);
    result.limbs[7] = exp_field;
    return result;
}

flt512 flt512::negative_one() {
    flt512 result = one();
    result.limbs[7] |= (1ULL << 63);
    return result;
}

flt512 flt512::infinity() {
    flt512 result;
    std::memset(result.limbs, 0, sizeof(result.limbs));
    uint64_t max_exp = ((1ULL << EXPONENT_BITS) - 1) << (64 - EXPONENT_BITS - 1);
    result.limbs[7] = max_exp;
    return result;
}

flt512 flt512::negative_infinity() {
    flt512 result = infinity();
    result.limbs[7] |= (1ULL << 63);
    return result;
}

flt512 flt512::quiet_nan() {
    flt512 result;
    std::memset(result.limbs, 0, sizeof(result.limbs));
    uint64_t max_exp = ((1ULL << EXPONENT_BITS) - 1) << (64 - EXPONENT_BITS - 1);
    result.limbs[7] = max_exp | (1ULL << (64 - EXPONENT_BITS - 2));
    return result;
}

flt512 flt512::quiet_nan_with_payload(uint64_t payload) {
    flt512 result = quiet_nan();
    result.limbs[0] = payload;
    return result;
}

// ============================================================================
// flt512 Predicates (similar to flt256)
// ============================================================================

bool flt512::is_zero() const {
    for (int i = 0; i < 7; i++) {
        if (limbs[i] != 0) return false;
    }
    return (limbs[7] & 0x7FFFFFFFFFFFFFFFULL) == 0;
}

bool flt512::is_negative() const {
    return (limbs[7] >> 63) != 0;
}

bool flt512::is_infinity() const {
    uint64_t exp_mask = ((1ULL << EXPONENT_BITS) - 1) << (64 - EXPONENT_BITS - 1);
    uint64_t mantissa_high_mask = (1ULL << (64 - EXPONENT_BITS - 1)) - 1;

    uint64_t exp = limbs[7] & exp_mask;
    uint64_t mantissa_high = limbs[7] & mantissa_high_mask;

    if (exp != exp_mask || mantissa_high != 0) return false;
    for (int i = 0; i < 7; i++) {
        if (limbs[i] != 0) return false;
    }
    return true;
}

bool flt512::is_nan() const {
    uint64_t exp_mask = ((1ULL << EXPONENT_BITS) - 1) << (64 - EXPONENT_BITS - 1);
    uint64_t mantissa_high_mask = (1ULL << (64 - EXPONENT_BITS - 1)) - 1;

    uint64_t exp = limbs[7] & exp_mask;
    if (exp != exp_mask) return false;

    uint64_t mantissa_high = limbs[7] & mantissa_high_mask;
    if (mantissa_high != 0) return true;
    for (int i = 0; i < 7; i++) {
        if (limbs[i] != 0) return true;
    }
    return false;
}

bool flt512::is_normal() const {
    if (is_zero() || is_infinity() || is_nan()) return false;
    uint64_t exp_mask = ((1ULL << EXPONENT_BITS) - 1) << (64 - EXPONENT_BITS - 1);
    return (limbs[7] & exp_mask) != 0;
}

bool flt512::is_subnormal() const {
    if (is_zero()) return false;
    uint64_t exp_mask = ((1ULL << EXPONENT_BITS) - 1) << (64 - EXPONENT_BITS - 1);
    return (limbs[7] & exp_mask) == 0;
}

flt512 flt512::negate() const {
    flt512 result = *this;
    result.limbs[7] ^= (1ULL << 63);
    return result;
}

flt512 flt512::abs() const {
    flt512 result = *this;
    result.limbs[7] &= ~(1ULL << 63);
    return result;
}

int flt512::get_sign() const {
    return (limbs[7] >> 63) ? -1 : 1;
}

int64_t flt512::get_exponent() const {
    uint64_t exp_mask = ((1ULL << EXPONENT_BITS) - 1) << (64 - EXPONENT_BITS - 1);
    uint64_t raw_exp = (limbs[7] & exp_mask) >> (64 - EXPONENT_BITS - 1);
    return static_cast<int64_t>(raw_exp) - EXPONENT_BIAS;
}

void flt512::get_mantissa(uint64_t out[8]) const {
    for (int i = 0; i < 7; i++) {
        out[i] = limbs[i];
    }
    uint64_t mantissa_high_mask = (1ULL << (64 - EXPONENT_BITS - 1)) - 1;
    out[7] = limbs[7] & mantissa_high_mask;
}

// ============================================================================
// Conversion: From Double
// ============================================================================

flt256 flt256_from_double(double d) {
    flt256 result;
    std::memset(result.limbs, 0, sizeof(result.limbs));

    if (std::isnan(d)) {
        return flt256::quiet_nan();
    }
    if (std::isinf(d)) {
        return d > 0 ? flt256::infinity() : flt256::negative_infinity();
    }
    if (d == 0.0) {
        result = flt256::zero();
        if (std::signbit(d)) {
            result.limbs[3] |= (1ULL << 63);
        }
        return result;
    }

    // Extract double components
    uint64_t bits;
    std::memcpy(&bits, &d, sizeof(bits));

    int sign = (bits >> 63) & 1;
    int64_t exp_double = ((bits >> 52) & 0x7FF) - 1023;  // Unbiased exponent
    uint64_t mantissa_double = bits & ((1ULL << 52) - 1);

    // Convert exponent to flt256 biased form
    int64_t exp_256 = exp_double + flt256::EXPONENT_BIAS;

    // Place mantissa in the high bits of the 236-bit mantissa field
    // Double has 52 explicit mantissa bits, we need to place them at the top
    // limbs[3] holds sign(1) + exp(19) + mantissa_high(44) bits
    // The 52-bit mantissa goes into the top of the 236-bit field

    uint64_t mantissa_high_bits = 64 - 1 - 19;  // 44 bits available in limbs[3]

    // Shift mantissa to fill from top of 236-bit field
    // 236 - 52 = 184 bits of zeros below the double mantissa
    result.limbs[3] = mantissa_double >> (52 - mantissa_high_bits);
    result.limbs[2] = mantissa_double << (64 - (52 - mantissa_high_bits));
    // limbs[0] and limbs[1] stay zero (lower precision bits)

    // Set exponent
    result.limbs[3] |= (static_cast<uint64_t>(exp_256) << mantissa_high_bits);

    // Set sign
    if (sign) {
        result.limbs[3] |= (1ULL << 63);
    }

    return result;
}

flt512 flt512_from_double(double d) {
    flt512 result;
    std::memset(result.limbs, 0, sizeof(result.limbs));

    if (std::isnan(d)) {
        return flt512::quiet_nan();
    }
    if (std::isinf(d)) {
        return d > 0 ? flt512::infinity() : flt512::negative_infinity();
    }
    if (d == 0.0) {
        result = flt512::zero();
        if (std::signbit(d)) {
            result.limbs[7] |= (1ULL << 63);
        }
        return result;
    }

    uint64_t bits;
    std::memcpy(&bits, &d, sizeof(bits));

    int sign = (bits >> 63) & 1;
    int64_t exp_double = ((bits >> 52) & 0x7FF) - 1023;
    uint64_t mantissa_double = bits & ((1ULL << 52) - 1);

    int64_t exp_512 = exp_double + flt512::EXPONENT_BIAS;

    uint64_t mantissa_high_bits = 64 - 1 - 23;  // 40 bits in limbs[7]

    result.limbs[7] = mantissa_double >> (52 - mantissa_high_bits);
    result.limbs[6] = mantissa_double << (64 - (52 - mantissa_high_bits));

    result.limbs[7] |= (static_cast<uint64_t>(exp_512) << mantissa_high_bits);

    if (sign) {
        result.limbs[7] |= (1ULL << 63);
    }

    return result;
}

// ============================================================================
// Conversion: To Double
// ============================================================================

double flt256_to_double(const flt256& f) {
    if (f.is_nan()) return std::nan("");
    if (f.is_infinity()) return f.is_negative() ? -INFINITY : INFINITY;
    if (f.is_zero()) return f.is_negative() ? -0.0 : 0.0;

    int sign = f.get_sign();
    int64_t exp = f.get_exponent();

    // Check for overflow/underflow in double range
    if (exp > 1023) return sign < 0 ? -INFINITY : INFINITY;
    if (exp < -1022) return sign < 0 ? -0.0 : 0.0;

    // Extract top 52 bits of mantissa
    uint64_t mantissa[4];
    f.get_mantissa(mantissa);

    // Mantissa bits are in the high part - extract top 52 bits
    uint64_t mantissa_high_bits = 64 - 1 - 19;  // 44 bits in limbs[3]
    uint64_t double_mantissa = (mantissa[3] << (52 - mantissa_high_bits)) |
                               (mantissa[2] >> (64 - (52 - mantissa_high_bits)));
    double_mantissa &= ((1ULL << 52) - 1);

    // Build double
    uint64_t exp_field = static_cast<uint64_t>(exp + 1023) & 0x7FF;
    uint64_t bits = (sign < 0 ? (1ULL << 63) : 0) |
                    (exp_field << 52) |
                    double_mantissa;

    double result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

double flt512_to_double(const flt512& f) {
    if (f.is_nan()) return std::nan("");
    if (f.is_infinity()) return f.is_negative() ? -INFINITY : INFINITY;
    if (f.is_zero()) return f.is_negative() ? -0.0 : 0.0;

    int sign = f.get_sign();
    int64_t exp = f.get_exponent();

    if (exp > 1023) return sign < 0 ? -INFINITY : INFINITY;
    if (exp < -1022) return sign < 0 ? -0.0 : 0.0;

    uint64_t mantissa[8];
    f.get_mantissa(mantissa);

    uint64_t mantissa_high_bits = 64 - 1 - 23;  // 40 bits in limbs[7]
    uint64_t double_mantissa = (mantissa[7] << (52 - mantissa_high_bits)) |
                               (mantissa[6] >> (64 - (52 - mantissa_high_bits)));
    double_mantissa &= ((1ULL << 52) - 1);

    uint64_t exp_field = static_cast<uint64_t>(exp + 1023) & 0x7FF;
    uint64_t bits = (sign < 0 ? (1ULL << 63) : 0) |
                    (exp_field << 52) |
                    double_mantissa;

    double result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

// ============================================================================
// Negation (simple operations)
// ============================================================================

flt256 flt256_neg(const flt256& a) {
    return a.negate();
}

flt512 flt512_neg(const flt512& a) {
    return a.negate();
}

// ============================================================================
// TBB Integration
// ============================================================================

flt256 flt256_from_tbb_error() {
    return flt256::quiet_nan_with_payload(TBB_ERROR_NAN_PAYLOAD);
}

flt512 flt512_from_tbb_error() {
    return flt512::quiet_nan_with_payload(TBB_ERROR_NAN_PAYLOAD);
}

bool flt256_is_tbb_error_nan(const flt256& f) {
    return f.is_nan() && (f.limbs[0] == TBB_ERROR_NAN_PAYLOAD);
}

bool flt512_is_tbb_error_nan(const flt512& f) {
    return f.is_nan() && (f.limbs[0] == TBB_ERROR_NAN_PAYLOAD);
}

// ============================================================================
// Arithmetic and Comparison Operations
// ============================================================================
// ARIA-017 Phase 2: Full-precision SIMD implementations are in highprec_simd.cpp
// Uses carry-chain addition/subtraction, schoolbook multiplication,
// Newton-Raphson division/sqrt with AVX-512/AVX2 acceleration.

// ============================================================================
// C API for FFI
// ============================================================================

extern "C" {

void aria_flt256_add(const flt256* a, const flt256* b, flt256* result) {
    *result = flt256_add(*a, *b);
}

void aria_flt256_sub(const flt256* a, const flt256* b, flt256* result) {
    *result = flt256_sub(*a, *b);
}

void aria_flt256_mul(const flt256* a, const flt256* b, flt256* result) {
    *result = flt256_mul(*a, *b);
}

void aria_flt256_div(const flt256* a, const flt256* b, flt256* result) {
    *result = flt256_div(*a, *b);
}

void aria_flt256_neg(const flt256* a, flt256* result) {
    *result = flt256_neg(*a);
}

void aria_flt512_add(const flt512* a, const flt512* b, flt512* result) {
    *result = flt512_add(*a, *b);
}

void aria_flt512_sub(const flt512* a, const flt512* b, flt512* result) {
    *result = flt512_sub(*a, *b);
}

void aria_flt512_mul(const flt512* a, const flt512* b, flt512* result) {
    *result = flt512_mul(*a, *b);
}

void aria_flt512_div(const flt512* a, const flt512* b, flt512* result) {
    *result = flt512_div(*a, *b);
}

void aria_flt512_neg(const flt512* a, flt512* result) {
    *result = flt512_neg(*a);
}

void aria_flt256_from_double(double d, flt256* result) {
    *result = flt256_from_double(d);
}

double aria_flt256_to_double(const flt256* f) {
    return flt256_to_double(*f);
}

void aria_flt512_from_double(double d, flt512* result) {
    *result = flt512_from_double(d);
}

double aria_flt512_to_double(const flt512* f) {
    return flt512_to_double(*f);
}

} // extern "C"

} // namespace runtime
} // namespace npk

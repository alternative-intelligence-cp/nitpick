#ifndef ARIA_HIGHPREC_FLOAT_H
#define ARIA_HIGHPREC_FLOAT_H

/**
 * @file highprec_float.h
 * @brief High-Precision Floating-Point Types for Aria
 *
 * ARIA-017: High-Precision Floating-Point Core Design
 *
 * Implements flt256 (octuple precision) and flt512 (sedecim precision)
 * as first-class stack-allocated types with deterministic behavior.
 *
 * Design Goals:
 * - Zero heap allocation (stack-only, predictable latency)
 * - Constant-time execution (no data-dependent branching)
 * - IEEE 754-like semantics with extended precision
 * - TBB integration (NaN <-> ERR sentinel conversion)
 *
 * Memory Layout:
 * - flt256: 4 x uint64_t limbs, 32-byte aligned (fits YMM register)
 * - flt512: 8 x uint64_t limbs, 64-byte aligned (fits ZMM register)
 */

#include <cstdint>
#include <cstddef>
#include <string>

namespace npk {
namespace runtime {

// ============================================================================
// flt256 - Octuple Precision (256-bit floating-point)
// ============================================================================

/**
 * flt256 - 256-bit floating-point number
 *
 * Component Layout:
 * - Sign:     1 bit  (bit 255)
 * - Exponent: 19 bits (bits 236-254), bias = 262143
 * - Mantissa: 236 bits (bits 0-235), implicit leading 1 = 237 effective bits
 *
 * Precision: ~71 decimal digits
 * Range: ~10^+/-78913
 *
 * Limb Layout (little-endian within struct):
 * - limbs[0]: bits 0-63   (mantissa low)
 * - limbs[1]: bits 64-127 (mantissa mid-low)
 * - limbs[2]: bits 128-191 (mantissa mid-high)
 * - limbs[3]: bits 192-255 (sign + exponent + mantissa high)
 */
struct alignas(32) flt256 {
    uint64_t limbs[4];

    // Constants
    static constexpr int TOTAL_BITS = 256;
    static constexpr int SIGN_BITS = 1;
    static constexpr int EXPONENT_BITS = 19;
    static constexpr int MANTISSA_BITS = 236;
    static constexpr int64_t EXPONENT_BIAS = 262143;  // 2^18 - 1
    static constexpr int NUM_LIMBS = 4;

    // Special value patterns
    static flt256 zero();
    static flt256 one();
    static flt256 negative_one();
    static flt256 infinity();
    static flt256 negative_infinity();
    static flt256 quiet_nan();
    static flt256 quiet_nan_with_payload(uint64_t payload);

    // Predicates
    bool is_zero() const;
    bool is_negative() const;
    bool is_infinity() const;
    bool is_nan() const;
    bool is_normal() const;
    bool is_subnormal() const;

    // Sign manipulation
    flt256 negate() const;
    flt256 abs() const;

    // Extraction
    int get_sign() const;
    int64_t get_exponent() const;  // Returns unbiased exponent
    void get_mantissa(uint64_t out[4]) const;
};

// ============================================================================
// flt512 - Sedecim Precision (512-bit floating-point)
// ============================================================================

/**
 * flt512 - 512-bit floating-point number
 *
 * Component Layout:
 * - Sign:     1 bit  (bit 511)
 * - Exponent: 23 bits (bits 488-510), bias = 4194303
 * - Mantissa: 488 bits (bits 0-487), implicit leading 1 = 489 effective bits
 *
 * Precision: ~147 decimal digits
 * Range: ~10^+/-1262611
 *
 * Limb Layout (little-endian within struct):
 * - limbs[0-6]: Full 64-bit mantissa segments
 * - limbs[7]:   Sign + exponent + high mantissa
 */
struct alignas(64) flt512 {
    uint64_t limbs[8];

    // Constants
    static constexpr int TOTAL_BITS = 512;
    static constexpr int SIGN_BITS = 1;
    static constexpr int EXPONENT_BITS = 23;
    static constexpr int MANTISSA_BITS = 488;
    static constexpr int64_t EXPONENT_BIAS = 4194303;  // 2^22 - 1
    static constexpr int NUM_LIMBS = 8;

    // Special value patterns
    static flt512 zero();
    static flt512 one();
    static flt512 negative_one();
    static flt512 infinity();
    static flt512 negative_infinity();
    static flt512 quiet_nan();
    static flt512 quiet_nan_with_payload(uint64_t payload);

    // Predicates
    bool is_zero() const;
    bool is_negative() const;
    bool is_infinity() const;
    bool is_nan() const;
    bool is_normal() const;
    bool is_subnormal() const;

    // Sign manipulation
    flt512 negate() const;
    flt512 abs() const;

    // Extraction
    int get_sign() const;
    int64_t get_exponent() const;
    void get_mantissa(uint64_t out[8]) const;
};

// ============================================================================
// Arithmetic Operations
// ============================================================================

// flt256 arithmetic
flt256 flt256_add(const flt256& a, const flt256& b);
flt256 flt256_sub(const flt256& a, const flt256& b);
flt256 flt256_mul(const flt256& a, const flt256& b);
flt256 flt256_div(const flt256& a, const flt256& b);
flt256 flt256_neg(const flt256& a);
flt256 flt256_sqrt(const flt256& a);

// flt512 arithmetic
flt512 flt512_add(const flt512& a, const flt512& b);
flt512 flt512_sub(const flt512& a, const flt512& b);
flt512 flt512_mul(const flt512& a, const flt512& b);
flt512 flt512_div(const flt512& a, const flt512& b);
flt512 flt512_neg(const flt512& a);
flt512 flt512_sqrt(const flt512& a);

// ============================================================================
// Comparison Operations
// ============================================================================

// flt256 comparison (returns -1, 0, 1 like strcmp)
int flt256_compare(const flt256& a, const flt256& b);
bool flt256_eq(const flt256& a, const flt256& b);
bool flt256_lt(const flt256& a, const flt256& b);
bool flt256_le(const flt256& a, const flt256& b);
bool flt256_gt(const flt256& a, const flt256& b);
bool flt256_ge(const flt256& a, const flt256& b);

// flt512 comparison
int flt512_compare(const flt512& a, const flt512& b);
bool flt512_eq(const flt512& a, const flt512& b);
bool flt512_lt(const flt512& a, const flt512& b);
bool flt512_le(const flt512& a, const flt512& b);
bool flt512_gt(const flt512& a, const flt512& b);
bool flt512_ge(const flt512& a, const flt512& b);

// ============================================================================
// Conversion Operations
// ============================================================================

// From standard types
flt256 flt256_from_double(double d);
flt256 flt256_from_int64(int64_t i);
flt256 flt256_from_string(const char* str, size_t len);

flt512 flt512_from_double(double d);
flt512 flt512_from_int64(int64_t i);
flt512 flt512_from_string(const char* str, size_t len);

// To standard types (may lose precision)
double flt256_to_double(const flt256& f);
int64_t flt256_to_int64(const flt256& f);
std::string flt256_to_string(const flt256& f, int precision = 71);

double flt512_to_double(const flt512& f);
int64_t flt512_to_int64(const flt512& f);
std::string flt512_to_string(const flt512& f, int precision = 147);

// Between precision levels
flt512 flt256_to_flt512(const flt256& f);
flt256 flt512_to_flt256(const flt512& f);  // May lose precision

// ============================================================================
// TBB Integration (ARIA-017 requirement)
// ============================================================================

// NaN payload for TBB error source
constexpr uint64_t TBB_ERROR_NAN_PAYLOAD = 0x5442425F455252ULL;  // "TBB_ERR" in ASCII

// Convert TBB error sentinel to NaN
flt256 flt256_from_tbb_error();
flt512 flt512_from_tbb_error();

// Check if NaN originated from TBB error
bool flt256_is_tbb_error_nan(const flt256& f);
bool flt512_is_tbb_error_nan(const flt512& f);

// ============================================================================
// C API for FFI
// ============================================================================

extern "C" {
    // flt256 C API
    void npk_flt256_add(const flt256* a, const flt256* b, flt256* result);
    void npk_flt256_sub(const flt256* a, const flt256* b, flt256* result);
    void npk_flt256_mul(const flt256* a, const flt256* b, flt256* result);
    void npk_flt256_div(const flt256* a, const flt256* b, flt256* result);
    void npk_flt256_neg(const flt256* a, flt256* result);

    // flt512 C API
    void npk_flt512_add(const flt512* a, const flt512* b, flt512* result);
    void npk_flt512_sub(const flt512* a, const flt512* b, flt512* result);
    void npk_flt512_mul(const flt512* a, const flt512* b, flt512* result);
    void npk_flt512_div(const flt512* a, const flt512* b, flt512* result);
    void npk_flt512_neg(const flt512* a, flt512* result);

    // Conversion C API
    void npk_flt256_from_double(double d, flt256* result);
    double npk_flt256_to_double(const flt256* f);
    void npk_flt512_from_double(double d, flt512* result);
    double npk_flt512_to_double(const flt512* f);
}

} // namespace runtime
} // namespace npk

#endif // ARIA_HIGHPREC_FLOAT_H

// Aria Twisted Floating Point Operations
// Software floating-point with cross-platform determinism
// No hardware FPU dependencies - bit-exact results on all architectures

#include "backend/runtime/tfp_ops.h"
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <limits>

// ============================================================================
// TBB Sentinel Values for Components
// ============================================================================

static const int16_t TBB16_ERR = -32768;
static const int16_t TBB16_MAX = 32767;
static const int16_t TBB16_MIN = -32767;

static const int64_t TBB48_ERR = -140737488355328LL;  // -2^47
static const int64_t TBB48_MAX = 140737488355327LL;   // 2^47 - 1
static const int64_t TBB48_MIN = -140737488355327LL;  // -(2^47 - 1)

// ============================================================================
// Exponent Bias
// ============================================================================
// To represent both tiny and huge numbers, exponent is biased
// tfp32: bias = 16383 (half of tbb16 range)
// tfp64: bias = 16383 (same bias for consistency)

static const int16_t EXP_BIAS = 16383;

// ============================================================================
// Special Value Constructors
// ============================================================================

extern "C" {

Tfp32 aria_tfp32_zero() {
    return {0, 0};  // exp=0, mant=0 is the unique zero
}

Tfp64 aria_tfp64_zero() {
    return {0, 0, 0};  // exp=0, mant=0 is the unique zero
}

Tfp32 aria_tfp32_err() {
    return {TBB16_ERR, TBB16_ERR};  // Both components ERR
}

Tfp64 aria_tfp64_err() {
    return {TBB16_ERR, TBB48_ERR, 0};  // Both components ERR
}

int aria_tfp32_is_err(Tfp32 val) {
    return val.exp == TBB16_ERR || val.mant == TBB16_ERR;
}

int aria_tfp64_is_err(Tfp64 val) {
    return val.exp == TBB16_ERR || val.mant == TBB48_ERR;
}

int aria_tfp32_is_zero(Tfp32 val) {
    return val.exp == 0 && val.mant == 0;
}

int aria_tfp64_is_zero(Tfp64 val) {
    return val.exp == 0 && val.mant == 0;
}

// ============================================================================
// Normalization
// ============================================================================
// Shift mantissa left until MSB is set, adjust exponent accordingly
// This ensures consistent representation

static Tfp32 normalize_tfp32(Tfp32 val) {
    if (aria_tfp32_is_err(val) || aria_tfp32_is_zero(val)) {
        return val;
    }
    
    // Handle sign separately
    int sign = (val.mant < 0) ? -1 : 1;
    int16_t abs_mant = (val.mant < 0) ? -val.mant : val.mant;
    
    // Normalize: shift left until bit 14 is set (leaving room for sign)
    while (abs_mant > 0 && abs_mant < 0x4000 && val.exp > -EXP_BIAS) {
        abs_mant <<= 1;
        val.exp--;
    }
    
    // Restore sign
    val.mant = (sign < 0) ? -abs_mant : abs_mant;
    
    // Check for underflow
    if (val.exp < -EXP_BIAS) {
        return aria_tfp32_zero();
    }
    
    return val;
}

static Tfp64 normalize_tfp64(Tfp64 val) {
    if (aria_tfp64_is_err(val) || aria_tfp64_is_zero(val)) {
        return val;
    }
    
    // Handle sign separately
    int sign = (val.mant < 0) ? -1 : 1;
    int64_t abs_mant = (val.mant < 0) ? -val.mant : val.mant;
    
    // Normalize: shift left until bit 46 is set
    while (abs_mant > 0 && abs_mant < (1LL << 46) && val.exp > -EXP_BIAS) {
        abs_mant <<= 1;
        val.exp--;
    }
    
    // Restore sign
    val.mant = (sign < 0) ? -abs_mant : abs_mant;
    
    // Check for underflow
    if (val.exp < -EXP_BIAS) {
        return aria_tfp64_zero();
    }
    
    return val;
}

// ============================================================================
// Conversion from double (for literals)
// ============================================================================
// NOTE: This is lossy and platform-dependent INPUT only
// All subsequent operations are deterministic

Tfp32 aria_tfp32_from_double(double val) {
    if (std::isnan(val) || std::isinf(val)) {
        return aria_tfp32_err();
    }
    
    if (val == 0.0) {
        return aria_tfp32_zero();
    }
    
    // Extract sign
    int sign = (val < 0) ? -1 : 1;
    val = std::fabs(val);
    
    // Extract exponent via frexp
    int exp_raw;
    double mant_double = std::frexp(val, &exp_raw);
    
    // Scale mantissa to 16-bit range (14 bits precision + sign)
    int16_t mant = (int16_t)(mant_double * 16384.0) * sign;
    int16_t exp = (int16_t)(exp_raw);
    
    // Check bounds
    if (exp > TBB16_MAX || exp < TBB16_MIN) {
        return aria_tfp32_err();
    }
    
    return normalize_tfp32({exp, mant});
}

Tfp64 aria_tfp64_from_double(double val) {
    if (std::isnan(val) || std::isinf(val)) {
        return aria_tfp64_err();
    }
    
    if (val == 0.0) {
        return aria_tfp64_zero();
    }
    
    // Extract sign
    int sign = (val < 0) ? -1 : 1;
    val = std::fabs(val);
    
    // Extract exponent via frexp
    int exp_raw;
    double mant_double = std::frexp(val, &exp_raw);
    
    // Scale mantissa to 48-bit range (46 bits precision + sign)
    int64_t mant = (int64_t)(mant_double * (1LL << 46)) * sign;
    int16_t exp = (int16_t)(exp_raw);
    
    // Check bounds
    if (exp > TBB16_MAX || exp < TBB16_MIN) {
        return aria_tfp64_err();
    }
    if (mant > TBB48_MAX || mant < TBB48_MIN) {
        return aria_tfp64_err();
    }
    
    return normalize_tfp64({exp, mant, 0});
}

// ============================================================================
// Construction from parts (exponent, mantissa)
// ============================================================================

Tfp32 aria_tfp32_from_parts(int16_t exp, int16_t mant) {
    // Direct construction from parts - no normalization needed
    // User is responsible for providing valid TBB values
    return {exp, mant};
}

Tfp64 aria_tfp64_from_parts(int16_t exp, int64_t mant) {
    // Direct construction from parts - no normalization needed
    // User is responsible for providing valid TBB values
    // mant is already a 48-bit value (enforced by struct bitfield)
    return {exp, mant, 0};
}

// ============================================================================
// Conversion to double (for debugging only - lossy)
// ============================================================================

double aria_tfp32_to_double(Tfp32 val) {
    if (aria_tfp32_is_err(val)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (aria_tfp32_is_zero(val)) {
        return 0.0;
    }
    
    return std::ldexp((double)val.mant / 16384.0, val.exp);
}

double aria_tfp64_to_double(Tfp64 val) {
    if (aria_tfp64_is_err(val)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (aria_tfp64_is_zero(val)) {
        return 0.0;
    }
    
    return std::ldexp((double)val.mant / (double)(1LL << 46), val.exp);
}

// ============================================================================
// Addition
// ============================================================================

Tfp32 aria_tfp32_add(Tfp32 a, Tfp32 b) {
    if (aria_tfp32_is_err(a) || aria_tfp32_is_err(b)) {
        return aria_tfp32_err();
    }
    
    if (aria_tfp32_is_zero(a)) return b;
    if (aria_tfp32_is_zero(b)) return a;
    
    // Align exponents
    if (a.exp < b.exp) {
        std::swap(a, b);
    }
    
    int16_t exp_diff = a.exp - b.exp;
    
    // Shift smaller mantissa right
    int32_t b_shifted = (exp_diff < 16) ? (b.mant >> exp_diff) : 0;
    
    // Add mantissas
    int32_t result_mant = (int32_t)a.mant + b_shifted;
    
    // Check for overflow
    if (result_mant > TBB16_MAX || result_mant < TBB16_MIN) {
        return aria_tfp32_err();
    }
    
    Tfp32 result = {a.exp, (int16_t)result_mant};
    return normalize_tfp32(result);
}

Tfp64 aria_tfp64_add(Tfp64 a, Tfp64 b) {
    if (aria_tfp64_is_err(a) || aria_tfp64_is_err(b)) {
        return aria_tfp64_err();
    }
    
    if (aria_tfp64_is_zero(a)) return b;
    if (aria_tfp64_is_zero(b)) return a;
    
    // Align exponents
    if (a.exp < b.exp) {
        std::swap(a, b);
    }
    
    int16_t exp_diff = a.exp - b.exp;
    
    // Shift smaller mantissa right
    int64_t b_shifted = (exp_diff < 48) ? (b.mant >> exp_diff) : 0;
    
    // Add mantissas (check for overflow)
    int64_t result_mant = a.mant + b_shifted;
    
    // Check for overflow in 48-bit space
    if (result_mant > TBB48_MAX || result_mant < TBB48_MIN) {
        return aria_tfp64_err();
    }
    
    Tfp64 result = {a.exp, result_mant, 0};
    return normalize_tfp64(result);
}

// ============================================================================
// Subtraction
// ============================================================================

Tfp32 aria_tfp32_sub(Tfp32 a, Tfp32 b) {
    return aria_tfp32_add(a, aria_tfp32_neg(b));
}

Tfp64 aria_tfp64_sub(Tfp64 a, Tfp64 b) {
    return aria_tfp64_add(a, aria_tfp64_neg(b));
}

// ============================================================================
// Negation
// ============================================================================

Tfp32 aria_tfp32_neg(Tfp32 a) {
    if (aria_tfp32_is_err(a)) return a;
    if (aria_tfp32_is_zero(a)) return aria_tfp32_zero();
    
    // Negate mantissa
    if (a.mant == TBB16_MIN) {
        return aria_tfp32_err();  // Can't negate minimum value
    }
    
    return {a.exp, (int16_t)(-a.mant)};
}

Tfp64 aria_tfp64_neg(Tfp64 a) {
    if (aria_tfp64_is_err(a)) return a;
    if (aria_tfp64_is_zero(a)) return aria_tfp64_zero();
    
    // Negate mantissa
    if (a.mant == TBB48_MIN) {
        return aria_tfp64_err();  // Can't negate minimum value
    }
    
    return {a.exp, -a.mant, 0};
}

// ============================================================================
// Multiplication
// ============================================================================

Tfp32 aria_tfp32_mul(Tfp32 a, Tfp32 b) {
    if (aria_tfp32_is_err(a) || aria_tfp32_is_err(b)) {
        return aria_tfp32_err();
    }
    
    if (aria_tfp32_is_zero(a) || aria_tfp32_is_zero(b)) {
        return aria_tfp32_zero();
    }
    
    // Multiply mantissas (need 32-bit intermediate)
    int32_t result_mant = ((int32_t)a.mant * (int32_t)b.mant) / 16384;
    
    // Add exponents
    int32_t result_exp = (int32_t)a.exp + (int32_t)b.exp;
    
    // Check bounds
    if (result_exp > TBB16_MAX || result_exp < TBB16_MIN) {
        return aria_tfp32_err();
    }
    if (result_mant > TBB16_MAX || result_mant < TBB16_MIN) {
        return aria_tfp32_err();
    }
    
    Tfp32 result = {(int16_t)result_exp, (int16_t)result_mant};
    return normalize_tfp32(result);
}

Tfp64 aria_tfp64_mul(Tfp64 a, Tfp64 b) {
    if (aria_tfp64_is_err(a) || aria_tfp64_is_err(b)) {
        return aria_tfp64_err();
    }
    
    if (aria_tfp64_is_zero(a) || aria_tfp64_is_zero(b)) {
        return aria_tfp64_zero();
    }
    
    // Multiply mantissas (need wider intermediate - use __int128 if available)
    #ifdef __SIZEOF_INT128__
    __int128 wide_mant = (__int128)a.mant * (__int128)b.mant;
    int64_t result_mant = (int64_t)(wide_mant / (1LL << 46));
    #else
    // Fallback: split into high/low parts (more complex, less precise)
    int64_t result_mant = (a.mant / 1024) * (b.mant / (1LL << 36));
    #endif
    
    // Add exponents
    int32_t result_exp = (int32_t)a.exp + (int32_t)b.exp;
    
    // Check bounds
    if (result_exp > TBB16_MAX || result_exp < TBB16_MIN) {
        return aria_tfp64_err();
    }
    if (result_mant > TBB48_MAX || result_mant < TBB48_MIN) {
        return aria_tfp64_err();
    }
    
    Tfp64 result = {(int16_t)result_exp, result_mant, 0};
    return normalize_tfp64(result);
}

// ============================================================================
// Division
// ============================================================================

Tfp32 aria_tfp32_div(Tfp32 a, Tfp32 b) {
    if (aria_tfp32_is_err(a) || aria_tfp32_is_err(b)) {
        return aria_tfp32_err();
    }
    
    if (aria_tfp32_is_zero(b)) {
        return aria_tfp32_err();  // Division by zero
    }
    
    if (aria_tfp32_is_zero(a)) {
        return aria_tfp32_zero();
    }
    
    // Divide mantissas (scale numerator up first)
    int32_t result_mant = ((int32_t)a.mant * 16384) / (int32_t)b.mant;
    
    // Subtract exponents
    int32_t result_exp = (int32_t)a.exp - (int32_t)b.exp;
    
    // Check bounds
    if (result_exp > TBB16_MAX || result_exp < TBB16_MIN) {
        return aria_tfp32_err();
    }
    if (result_mant > TBB16_MAX || result_mant < TBB16_MIN) {
        return aria_tfp32_err();
    }
    
    Tfp32 result = {(int16_t)result_exp, (int16_t)result_mant};
    return normalize_tfp32(result);
}

Tfp64 aria_tfp64_div(Tfp64 a, Tfp64 b) {
    if (aria_tfp64_is_err(a) || aria_tfp64_is_err(b)) {
        return aria_tfp64_err();
    }
    
    if (aria_tfp64_is_zero(b)) {
        return aria_tfp64_err();  // Division by zero
    }
    
    if (aria_tfp64_is_zero(a)) {
        return aria_tfp64_zero();
    }
    
    // Divide mantissas (scale numerator up first)
    #ifdef __SIZEOF_INT128__
    __int128 scaled_num = (__int128)a.mant * (1LL << 46);
    int64_t result_mant = (int64_t)(scaled_num / (__int128)b.mant);
    #else
    // Fallback for 64-bit only systems
    int64_t result_mant = (a.mant / (b.mant / (1LL << 23))) * (1LL << 23);
    #endif
    
    // Subtract exponents
    int32_t result_exp = (int32_t)a.exp - (int32_t)b.exp;
    
    // Check bounds
    if (result_exp > TBB16_MAX || result_exp < TBB16_MIN) {
        return aria_tfp64_err();
    }
    if (result_mant > TBB48_MAX || result_mant < TBB48_MIN) {
        return aria_tfp64_err();
    }
    
    Tfp64 result = {(int16_t)result_exp, result_mant, 0};
    return normalize_tfp64(result);
}

// ============================================================================
// Comparison
// ============================================================================

int32_t aria_tfp32_cmp(Tfp32 a, Tfp32 b) {
    if (aria_tfp32_is_err(a) || aria_tfp32_is_err(b)) {
        return 0;  // Unordered
    }
    
    // Convert to double for comparison (deterministic since both are already TFP)
    double a_d = aria_tfp32_to_double(a);
    double b_d = aria_tfp32_to_double(b);
    
    if (a_d < b_d) return -1;
    if (a_d > b_d) return 1;
    return 0;
}

int32_t aria_tfp64_cmp(Tfp64 a, Tfp64 b) {
    if (aria_tfp64_is_err(a) || aria_tfp64_is_err(b)) {
        return 0;  // Unordered
    }
    
    // Convert to double for comparison
    double a_d = aria_tfp64_to_double(a);
    double b_d = aria_tfp64_to_double(b);
    
    if (a_d < b_d) return -1;
    if (a_d > b_d) return 1;
    return 0;
}

// ============================================================================
// Math Helpers — Deterministic comparison and integer extraction
// ============================================================================
// All helpers use TFP integer arithmetic only. No double conversion.

// Deterministic TFP32 comparison (no FPU, no double)
static int tfp32_compare(Tfp32 a, Tfp32 b) {
    if (aria_tfp32_is_zero(a) && aria_tfp32_is_zero(b)) return 0;
    if (aria_tfp32_is_zero(a)) return (b.mant > 0) ? -1 : 1;
    if (aria_tfp32_is_zero(b)) return (a.mant > 0) ? 1 : -1;
    if (a.mant > 0 && b.mant < 0) return 1;
    if (a.mant < 0 && b.mant > 0) return -1;
    if (a.exp != b.exp) {
        bool a_larger = (a.exp > b.exp);
        return (a.mant > 0) ? (a_larger ? 1 : -1) : (a_larger ? -1 : 1);
    }
    if (a.mant == b.mant) return 0;
    return (a.mant > b.mant) ? 1 : -1;
}

static int tfp64_compare(Tfp64 a, Tfp64 b) {
    if (aria_tfp64_is_zero(a) && aria_tfp64_is_zero(b)) return 0;
    if (aria_tfp64_is_zero(a)) return (b.mant > 0) ? -1 : 1;
    if (aria_tfp64_is_zero(b)) return (a.mant > 0) ? 1 : -1;
    if (a.mant > 0 && b.mant < 0) return 1;
    if (a.mant < 0 && b.mant > 0) return -1;
    if (a.exp != b.exp) {
        bool a_larger = (a.exp > b.exp);
        return (a.mant > 0) ? (a_larger ? 1 : -1) : (a_larger ? -1 : 1);
    }
    if (a.mant == b.mant) return 0;
    return (a.mant > b.mant) ? 1 : -1;
}

// Extract integer part (truncation towards zero)
static int32_t tfp32_to_int32(Tfp32 x) {
    if (aria_tfp32_is_err(x) || aria_tfp32_is_zero(x)) return 0;
    int shift = (int)x.exp - 14;
    int32_t m = (int32_t)x.mant;
    if (shift >= 15) return (m < 0) ? -0x7FFFFFFF : 0x7FFFFFFF;
    if (shift >= 0) return m << shift;
    int ns = -shift;
    if (ns >= 16) return 0;
    return (m >= 0) ? (m >> ns) : -((-m) >> ns);
}

static int64_t tfp64_to_int64(Tfp64 x) {
    if (aria_tfp64_is_err(x) || aria_tfp64_is_zero(x)) return 0;
    int shift = (int)x.exp - 46;
    int64_t m = x.mant;
    if (shift >= 17) return (m < 0) ? (-0x7FFFFFFFFFFFFFFFLL) : 0x7FFFFFFFFFFFFFFFLL;
    if (shift >= 0) return m << shift;
    int ns = -shift;
    if (ns >= 48) return 0;
    return (m >= 0) ? (m >> ns) : -((-m) >> ns);
}

// Round to nearest integer
static int32_t tfp32_round(Tfp32 x) {
    Tfp32 half = aria_tfp32_from_double(0.5);
    if (x.mant >= 0) return tfp32_to_int32(aria_tfp32_add(x, half));
    return tfp32_to_int32(aria_tfp32_sub(x, half));
}

static int64_t tfp64_round(Tfp64 x) {
    Tfp64 half = aria_tfp64_from_double(0.5);
    if (x.mant >= 0) return tfp64_to_int64(aria_tfp64_add(x, half));
    return tfp64_to_int64(aria_tfp64_sub(x, half));
}

// Scale by power of 2 (deterministic: just adjusts exponent)
static Tfp32 tfp32_ldexp(Tfp32 x, int k) {
    if (aria_tfp32_is_err(x) || aria_tfp32_is_zero(x)) return x;
    int32_t ne = (int32_t)x.exp + k;
    if (ne > TBB16_MAX || ne < TBB16_MIN) return aria_tfp32_err();
    return {(int16_t)ne, x.mant};
}

static Tfp64 tfp64_ldexp(Tfp64 x, int k) {
    if (aria_tfp64_is_err(x) || aria_tfp64_is_zero(x)) return x;
    int32_t ne = (int32_t)x.exp + k;
    if (ne > TBB16_MAX || ne < TBB16_MIN) return aria_tfp64_err();
    return {(int16_t)ne, x.mant, 0};
}

// Angle reduction to [-π, π] using deterministic TFP subtraction
static Tfp32 tfp32_reduce_angle(Tfp32 x) {
    Tfp32 two_pi = aria_tfp32_from_double(6.283185307179586);
    Tfp32 pi = aria_tfp32_from_double(3.141592653589793);
    Tfp32 neg_pi = aria_tfp32_neg(pi);
    for (int i = 0; i < 100; i++) {
        if (tfp32_compare(x, pi) > 0) x = aria_tfp32_sub(x, two_pi);
        else if (tfp32_compare(x, neg_pi) < 0) x = aria_tfp32_add(x, two_pi);
        else break;
    }
    return x;
}

static Tfp64 tfp64_reduce_angle(Tfp64 x) {
    Tfp64 two_pi = aria_tfp64_from_double(6.283185307179586);
    Tfp64 pi = aria_tfp64_from_double(3.141592653589793);
    Tfp64 neg_pi = aria_tfp64_neg(pi);
    for (int i = 0; i < 200; i++) {
        if (tfp64_compare(x, pi) > 0) x = aria_tfp64_sub(x, two_pi);
        else if (tfp64_compare(x, neg_pi) < 0) x = aria_tfp64_add(x, two_pi);
        else break;
    }
    return x;
}

// ============================================================================
// Math Functions — Deterministic implementations using TFP arithmetic
// All intermediate computation uses integer-based TFP operations for
// cross-platform bit-exact reproducibility. No hardware FPU is used.
// ============================================================================

// Square root via Newton's method (Heron's method)
// Converges quadratically: each iteration doubles precision
Tfp32 aria_tfp32_sqrt(Tfp32 x) {
    if (aria_tfp32_is_err(x)) return aria_tfp32_err();
    if (aria_tfp32_is_zero(x)) return aria_tfp32_zero();
    if (x.mant < 0) return aria_tfp32_err();
    
    Tfp32 guess = normalize_tfp32({(int16_t)(x.exp / 2), x.mant});
    Tfp32 two = aria_tfp32_from_double(2.0);
    
    for (int i = 0; i < 10; i++) {
        Tfp32 d = aria_tfp32_div(x, guess);
        if (aria_tfp32_is_err(d)) return aria_tfp32_err();
        Tfp32 s = aria_tfp32_add(guess, d);
        if (aria_tfp32_is_err(s)) return aria_tfp32_err();
        guess = aria_tfp32_div(s, two);
        if (aria_tfp32_is_err(guess)) return aria_tfp32_err();
    }
    return guess;
}

Tfp64 aria_tfp64_sqrt(Tfp64 x) {
    if (aria_tfp64_is_err(x)) return aria_tfp64_err();
    if (aria_tfp64_is_zero(x)) return aria_tfp64_zero();
    if (x.mant < 0) return aria_tfp64_err();
    
    Tfp64 guess = normalize_tfp64({(int16_t)(x.exp / 2), x.mant, 0});
    Tfp64 two = aria_tfp64_from_double(2.0);
    
    for (int i = 0; i < 16; i++) {
        Tfp64 d = aria_tfp64_div(x, guess);
        if (aria_tfp64_is_err(d)) return aria_tfp64_err();
        Tfp64 s = aria_tfp64_add(guess, d);
        if (aria_tfp64_is_err(s)) return aria_tfp64_err();
        guess = aria_tfp64_div(s, two);
        if (aria_tfp64_is_err(guess)) return aria_tfp64_err();
    }
    return guess;
}

// Sine via Taylor series: sin(x) = x - x³/3! + x⁵/5! - x⁷/7! + ...
// Range-reduced to [-π, π] first for convergence
Tfp32 aria_tfp32_sin(Tfp32 x) {
    if (aria_tfp32_is_err(x)) return aria_tfp32_err();
    if (aria_tfp32_is_zero(x)) return aria_tfp32_zero();
    x = tfp32_reduce_angle(x);
    
    Tfp32 x_sq = aria_tfp32_mul(x, x);
    Tfp32 term = x;
    Tfp32 sum = x;
    for (int n = 1; n <= 8; n++) {
        Tfp32 denom = aria_tfp32_from_double((double)((2*n) * (2*n+1)));
        term = aria_tfp32_neg(aria_tfp32_div(aria_tfp32_mul(term, x_sq), denom));
        if (aria_tfp32_is_err(term)) break;
        sum = aria_tfp32_add(sum, term);
        if (aria_tfp32_is_err(sum)) return aria_tfp32_err();
    }
    return sum;
}

Tfp64 aria_tfp64_sin(Tfp64 x) {
    if (aria_tfp64_is_err(x)) return aria_tfp64_err();
    if (aria_tfp64_is_zero(x)) return aria_tfp64_zero();
    x = tfp64_reduce_angle(x);
    
    Tfp64 x_sq = aria_tfp64_mul(x, x);
    Tfp64 term = x;
    Tfp64 sum = x;
    for (int n = 1; n <= 14; n++) {
        Tfp64 denom = aria_tfp64_from_double((double)((2*n) * (2*n+1)));
        term = aria_tfp64_neg(aria_tfp64_div(aria_tfp64_mul(term, x_sq), denom));
        if (aria_tfp64_is_err(term)) break;
        sum = aria_tfp64_add(sum, term);
        if (aria_tfp64_is_err(sum)) return aria_tfp64_err();
    }
    return sum;
}

// Cosine via Taylor series: cos(x) = 1 - x²/2! + x⁴/4! - x⁶/6! + ...
Tfp32 aria_tfp32_cos(Tfp32 x) {
    if (aria_tfp32_is_err(x)) return aria_tfp32_err();
    if (aria_tfp32_is_zero(x)) return aria_tfp32_from_double(1.0);
    x = tfp32_reduce_angle(x);
    
    Tfp32 x_sq = aria_tfp32_mul(x, x);
    Tfp32 one = aria_tfp32_from_double(1.0);
    Tfp32 term = one;
    Tfp32 sum = one;
    for (int n = 1; n <= 8; n++) {
        Tfp32 denom = aria_tfp32_from_double((double)((2*n-1) * (2*n)));
        term = aria_tfp32_neg(aria_tfp32_div(aria_tfp32_mul(term, x_sq), denom));
        if (aria_tfp32_is_err(term)) break;
        sum = aria_tfp32_add(sum, term);
        if (aria_tfp32_is_err(sum)) return aria_tfp32_err();
    }
    return sum;
}

Tfp64 aria_tfp64_cos(Tfp64 x) {
    if (aria_tfp64_is_err(x)) return aria_tfp64_err();
    if (aria_tfp64_is_zero(x)) return aria_tfp64_from_double(1.0);
    x = tfp64_reduce_angle(x);
    
    Tfp64 x_sq = aria_tfp64_mul(x, x);
    Tfp64 one = aria_tfp64_from_double(1.0);
    Tfp64 term = one;
    Tfp64 sum = one;
    for (int n = 1; n <= 14; n++) {
        Tfp64 denom = aria_tfp64_from_double((double)((2*n-1) * (2*n)));
        term = aria_tfp64_neg(aria_tfp64_div(aria_tfp64_mul(term, x_sq), denom));
        if (aria_tfp64_is_err(term)) break;
        sum = aria_tfp64_add(sum, term);
        if (aria_tfp64_is_err(sum)) return aria_tfp64_err();
    }
    return sum;
}

// Exponential via Taylor series with range reduction
// exp(x) = 2^k * exp(r), where k = round(x/ln2) and r = x - k*ln2
// The reduced r ∈ [-ln2/2, ln2/2] for fast convergence
Tfp32 aria_tfp32_exp(Tfp32 x) {
    if (aria_tfp32_is_err(x)) return aria_tfp32_err();
    if (aria_tfp32_is_zero(x)) return aria_tfp32_from_double(1.0);
    
    Tfp32 ln2 = aria_tfp32_from_double(0.6931471805599453);
    Tfp32 q = aria_tfp32_div(x, ln2);
    int32_t k = tfp32_round(q);
    Tfp32 k_tfp = aria_tfp32_from_double((double)k);
    Tfp32 r = aria_tfp32_sub(x, aria_tfp32_mul(k_tfp, ln2));
    
    Tfp32 one = aria_tfp32_from_double(1.0);
    Tfp32 term = one;
    Tfp32 sum = one;
    for (int n = 1; n <= 12; n++) {
        Tfp32 denom = aria_tfp32_from_double((double)n);
        term = aria_tfp32_div(aria_tfp32_mul(term, r), denom);
        if (aria_tfp32_is_err(term)) break;
        sum = aria_tfp32_add(sum, term);
        if (aria_tfp32_is_err(sum)) return aria_tfp32_err();
    }
    
    return tfp32_ldexp(sum, k);
}

Tfp64 aria_tfp64_exp(Tfp64 x) {
    if (aria_tfp64_is_err(x)) return aria_tfp64_err();
    if (aria_tfp64_is_zero(x)) return aria_tfp64_from_double(1.0);
    
    Tfp64 ln2 = aria_tfp64_from_double(0.6931471805599453);
    Tfp64 q = aria_tfp64_div(x, ln2);
    int64_t k = tfp64_round(q);
    Tfp64 k_tfp = aria_tfp64_from_double((double)k);
    Tfp64 r = aria_tfp64_sub(x, aria_tfp64_mul(k_tfp, ln2));
    
    Tfp64 one = aria_tfp64_from_double(1.0);
    Tfp64 term = one;
    Tfp64 sum = one;
    for (int n = 1; n <= 22; n++) {
        Tfp64 denom = aria_tfp64_from_double((double)n);
        term = aria_tfp64_div(aria_tfp64_mul(term, r), denom);
        if (aria_tfp64_is_err(term)) break;
        sum = aria_tfp64_add(sum, term);
        if (aria_tfp64_is_err(sum)) return aria_tfp64_err();
    }
    
    return tfp64_ldexp(sum, (int)k);
}

// Natural logarithm via series expansion
// log(x) = log(m * 2^e) = log(m) + e*ln(2)
// For m ∈ [1.0, 2.0): let t = (m-1)/(m+1), then
// log(m) = 2*(t + t³/3 + t⁵/5 + t⁷/7 + ...)
// Since |t| < 1/3 for normalized mantissa, convergence is fast
Tfp32 aria_tfp32_log(Tfp32 x) {
    if (aria_tfp32_is_err(x)) return aria_tfp32_err();
    if (aria_tfp32_is_zero(x)) return aria_tfp32_err();
    if (x.mant < 0) return aria_tfp32_err();
    
    // Decompose: m = {exp=0, mant} gives mantissa fraction in [1.0, 2.0)
    Tfp32 m = {0, x.mant};
    int32_t e = x.exp;
    
    Tfp32 one = aria_tfp32_from_double(1.0);
    Tfp32 t = aria_tfp32_div(aria_tfp32_sub(m, one), aria_tfp32_add(m, one));
    
    Tfp32 t_sq = aria_tfp32_mul(t, t);
    Tfp32 power = t;
    Tfp32 sum = t;
    for (int n = 1; n <= 10; n++) {
        power = aria_tfp32_mul(power, t_sq);
        if (aria_tfp32_is_err(power)) break;
        Tfp32 denom = aria_tfp32_from_double((double)(2*n+1));
        Tfp32 term = aria_tfp32_div(power, denom);
        if (aria_tfp32_is_err(term)) break;
        sum = aria_tfp32_add(sum, term);
    }
    
    Tfp32 two = aria_tfp32_from_double(2.0);
    Tfp32 log_m = aria_tfp32_mul(two, sum);
    
    Tfp32 ln2 = aria_tfp32_from_double(0.6931471805599453);
    Tfp32 e_tfp = aria_tfp32_from_double((double)e);
    return aria_tfp32_add(log_m, aria_tfp32_mul(e_tfp, ln2));
}

Tfp64 aria_tfp64_log(Tfp64 x) {
    if (aria_tfp64_is_err(x)) return aria_tfp64_err();
    if (aria_tfp64_is_zero(x)) return aria_tfp64_err();
    if (x.mant < 0) return aria_tfp64_err();
    
    Tfp64 m = {0, x.mant, 0};
    int32_t e = x.exp;
    
    Tfp64 one = aria_tfp64_from_double(1.0);
    Tfp64 t = aria_tfp64_div(aria_tfp64_sub(m, one), aria_tfp64_add(m, one));
    
    Tfp64 t_sq = aria_tfp64_mul(t, t);
    Tfp64 power = t;
    Tfp64 sum = t;
    for (int n = 1; n <= 20; n++) {
        power = aria_tfp64_mul(power, t_sq);
        if (aria_tfp64_is_err(power)) break;
        Tfp64 denom = aria_tfp64_from_double((double)(2*n+1));
        Tfp64 term = aria_tfp64_div(power, denom);
        if (aria_tfp64_is_err(term)) break;
        sum = aria_tfp64_add(sum, term);
    }
    
    Tfp64 two = aria_tfp64_from_double(2.0);
    Tfp64 log_m = aria_tfp64_mul(two, sum);
    
    Tfp64 ln2 = aria_tfp64_from_double(0.6931471805599453);
    Tfp64 e_tfp = aria_tfp64_from_double((double)e);
    return aria_tfp64_add(log_m, aria_tfp64_mul(e_tfp, ln2));
}

// Power via exp(exp_val * log(base))
Tfp32 aria_tfp32_pow(Tfp32 base, Tfp32 exp) {
    if (aria_tfp32_is_err(base) || aria_tfp32_is_err(exp)) return aria_tfp32_err();
    if (aria_tfp32_is_zero(exp)) return aria_tfp32_from_double(1.0);
    if (aria_tfp32_is_zero(base)) return aria_tfp32_zero();
    
    Tfp32 log_base = aria_tfp32_log(base);
    if (aria_tfp32_is_err(log_base)) return aria_tfp32_err();
    Tfp32 product = aria_tfp32_mul(exp, log_base);
    if (aria_tfp32_is_err(product)) return aria_tfp32_err();
    return aria_tfp32_exp(product);
}

Tfp64 aria_tfp64_pow(Tfp64 base, Tfp64 exp) {
    if (aria_tfp64_is_err(base) || aria_tfp64_is_err(exp)) return aria_tfp64_err();
    if (aria_tfp64_is_zero(exp)) return aria_tfp64_from_double(1.0);
    if (aria_tfp64_is_zero(base)) return aria_tfp64_zero();
    
    Tfp64 log_base = aria_tfp64_log(base);
    if (aria_tfp64_is_err(log_base)) return aria_tfp64_err();
    Tfp64 product = aria_tfp64_mul(exp, log_base);
    if (aria_tfp64_is_err(product)) return aria_tfp64_err();
    return aria_tfp64_exp(product);
}

// ============================================================================
// String Conversion
// ============================================================================

int aria_tfp32_to_string(char* buffer, size_t size, Tfp32 f) {
    if (aria_tfp32_is_err(f)) {
        return snprintf(buffer, size, "ERR");
    }
    
    double val = aria_tfp32_to_double(f);
    return snprintf(buffer, size, "%.6g", val);
}

int aria_tfp64_to_string(char* buffer, size_t size, Tfp64 f) {
    if (aria_tfp64_is_err(f)) {
        return snprintf(buffer, size, "ERR");
    }
    
    double val = aria_tfp64_to_double(f);
    return snprintf(buffer, size, "%.15g", val);
}

// Bare-name aliases used by IR-generated code (no aria_ prefix)
Tfp32 tfp32_from_parts(int16_t exp, int16_t mant) { return aria_tfp32_from_parts(exp, mant); }
Tfp64 tfp64_from_parts(int16_t exp, int64_t mant) { return aria_tfp64_from_parts(exp, mant); }

} // extern "C"

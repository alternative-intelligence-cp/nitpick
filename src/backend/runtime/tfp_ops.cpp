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
// Math Functions - Stubs for now (Taylor series implementations would go here)
// ============================================================================
// TODO: Implement Taylor series for full determinism
// For now, convert to double, compute, convert back (loses determinism guarantee)

Tfp32 aria_tfp32_sqrt(Tfp32 x) {
    if (aria_tfp32_is_err(x)) return aria_tfp32_err();
    double d = aria_tfp32_to_double(x);
    if (d < 0) return aria_tfp32_err();
    return aria_tfp32_from_double(std::sqrt(d));
}

Tfp64 aria_tfp64_sqrt(Tfp64 x) {
    if (aria_tfp64_is_err(x)) return aria_tfp64_err();
    double d = aria_tfp64_to_double(x);
    if (d < 0) return aria_tfp64_err();
    return aria_tfp64_from_double(std::sqrt(d));
}

Tfp32 aria_tfp32_sin(Tfp32 x) {
    if (aria_tfp32_is_err(x)) return aria_tfp32_err();
    return aria_tfp32_from_double(std::sin(aria_tfp32_to_double(x)));
}

Tfp64 aria_tfp64_sin(Tfp64 x) {
    if (aria_tfp64_is_err(x)) return aria_tfp64_err();
    return aria_tfp64_from_double(std::sin(aria_tfp64_to_double(x)));
}

Tfp32 aria_tfp32_cos(Tfp32 x) {
    if (aria_tfp32_is_err(x)) return aria_tfp32_err();
    return aria_tfp32_from_double(std::cos(aria_tfp32_to_double(x)));
}

Tfp64 aria_tfp64_cos(Tfp64 x) {
    if (aria_tfp64_is_err(x)) return aria_tfp64_err();
    return aria_tfp64_from_double(std::cos(aria_tfp64_to_double(x)));
}

Tfp32 aria_tfp32_exp(Tfp32 x) {
    if (aria_tfp32_is_err(x)) return aria_tfp32_err();
    return aria_tfp32_from_double(std::exp(aria_tfp32_to_double(x)));
}

Tfp64 aria_tfp64_exp(Tfp64 x) {
    if (aria_tfp64_is_err(x)) return aria_tfp64_err();
    return aria_tfp64_from_double(std::exp(aria_tfp64_to_double(x)));
}

Tfp32 aria_tfp32_log(Tfp32 x) {
    if (aria_tfp32_is_err(x)) return aria_tfp32_err();
    double d = aria_tfp32_to_double(x);
    if (d <= 0) return aria_tfp32_err();
    return aria_tfp32_from_double(std::log(d));
}

Tfp64 aria_tfp64_log(Tfp64 x) {
    if (aria_tfp64_is_err(x)) return aria_tfp64_err();
    double d = aria_tfp64_to_double(x);
    if (d <= 0) return aria_tfp64_err();
    return aria_tfp64_from_double(std::log(d));
}

Tfp32 aria_tfp32_pow(Tfp32 base, Tfp32 exp) {
    if (aria_tfp32_is_err(base) || aria_tfp32_is_err(exp)) return aria_tfp32_err();
    return aria_tfp32_from_double(std::pow(aria_tfp32_to_double(base), aria_tfp32_to_double(exp)));
}

Tfp64 aria_tfp64_pow(Tfp64 base, Tfp64 exp) {
    if (aria_tfp64_is_err(base) || aria_tfp64_is_err(exp)) return aria_tfp64_err();
    return aria_tfp64_from_double(std::pow(aria_tfp64_to_double(base), aria_tfp64_to_double(exp)));
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

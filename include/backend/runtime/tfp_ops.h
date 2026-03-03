// Aria Twisted Floating Point Operations Header
// Deterministic cross-platform floating-point with TBB safety model
// No -0, no NaN ambiguity, bit-exact reproducibility

#ifndef ARIA_TFP_OPS_H
#define ARIA_TFP_OPS_H

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// TFP Structure (matches Aria language representation)
// ============================================================================

#pragma pack(push, 1)

// tfp32: 16-bit exponent + 16-bit mantissa = 32 bits
struct Tfp32 {
    int16_t exp;      // TBB16 exponent (biased)
    int16_t mant;     // TBB16 mantissa (signed, 2's complement)
};

// tfp64: 16-bit exponent + 48-bit mantissa = 64 bits
struct Tfp64 {
    int16_t exp;      // TBB16 exponent (biased)
    int64_t mant : 48;  // TBB48 mantissa (signed, 2's complement, bit-field)
    int16_t _pad;     // Padding to align to 64 bits
};

#pragma pack(pop)

// ============================================================================
// Construction from literals
// ============================================================================

Tfp32 aria_tfp32_from_double(double val);
Tfp64 aria_tfp64_from_double(double val);

// Construction from parts (exponent, mantissa)
Tfp32 aria_tfp32_from_parts(int16_t exp, int16_t mant);
Tfp64 aria_tfp64_from_parts(int16_t exp, int64_t mant);

// Bare-name aliases used by IR-generated code
Tfp32 tfp32_from_parts(int16_t exp, int16_t mant);
Tfp64 tfp64_from_parts(int16_t exp, int64_t mant);

// ============================================================================
// Conversion to double (for printing/debugging only - lossy)
// ============================================================================

double aria_tfp32_to_double(Tfp32 val);
double aria_tfp64_to_double(Tfp64 val);

// ============================================================================
// Arithmetic Operations
// ============================================================================

// Addition
Tfp32 aria_tfp32_add(Tfp32 a, Tfp32 b);
Tfp64 aria_tfp64_add(Tfp64 a, Tfp64 b);

// Subtraction
Tfp32 aria_tfp32_sub(Tfp32 a, Tfp32 b);
Tfp64 aria_tfp64_sub(Tfp64 a, Tfp64 b);

// Multiplication
Tfp32 aria_tfp32_mul(Tfp32 a, Tfp32 b);
Tfp64 aria_tfp64_mul(Tfp64 a, Tfp64 b);

// Division
Tfp32 aria_tfp32_div(Tfp32 a, Tfp32 b);
Tfp64 aria_tfp64_div(Tfp64 a, Tfp64 b);

// Negation
Tfp32 aria_tfp32_neg(Tfp32 a);
Tfp64 aria_tfp64_neg(Tfp64 a);

// ============================================================================
// Comparison Operations
// ============================================================================
// Returns: -1 (a < b), 0 (a == b), +1 (a > b)

int32_t aria_tfp32_cmp(Tfp32 a, Tfp32 b);
int32_t aria_tfp64_cmp(Tfp64 a, Tfp64 b);

// ============================================================================
// Math Functions (Taylor series implementations)
// ============================================================================

// Square root
Tfp32 aria_tfp32_sqrt(Tfp32 x);
Tfp64 aria_tfp64_sqrt(Tfp64 x);

// Trigonometric
Tfp32 aria_tfp32_sin(Tfp32 x);
Tfp64 aria_tfp64_sin(Tfp64 x);

Tfp32 aria_tfp32_cos(Tfp32 x);
Tfp64 aria_tfp64_cos(Tfp64 x);

// Exponential/Logarithm
Tfp32 aria_tfp32_exp(Tfp32 x);
Tfp64 aria_tfp64_exp(Tfp64 x);

Tfp32 aria_tfp32_log(Tfp32 x);
Tfp64 aria_tfp64_log(Tfp64 x);

// Power
Tfp32 aria_tfp32_pow(Tfp32 base, Tfp32 exp);
Tfp64 aria_tfp64_pow(Tfp64 base, Tfp64 exp);

// ============================================================================
// Special Values
// ============================================================================

Tfp32 aria_tfp32_zero();
Tfp64 aria_tfp64_zero();

Tfp32 aria_tfp32_err();
Tfp64 aria_tfp64_err();

int aria_tfp32_is_err(Tfp32 val);
int aria_tfp64_is_err(Tfp64 val);

int aria_tfp32_is_zero(Tfp32 val);
int aria_tfp64_is_zero(Tfp64 val);

// ============================================================================
// String Conversion
// ============================================================================

int aria_tfp32_to_string(char* buffer, size_t size, Tfp32 f);
int aria_tfp64_to_string(char* buffer, size_t size, Tfp64 f);

#ifdef __cplusplus
}
#endif

#endif // ARIA_TFP_OPS_H

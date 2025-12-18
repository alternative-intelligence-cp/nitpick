/**
 * Phase 6.4 Standard Library - Math Operations
 * 
 * Mathematical functions and constants for Aria runtime.
 * 
 * Design:
 * - Wraps C math library (<cmath>) functions
 * - Result types for error handling (domain errors, overflow, etc.)
 * - Support for f64 (double) precision
 * - NaN and Infinity handling
 */

#ifndef ARIA_RUNTIME_MATH_H
#define ARIA_RUNTIME_MATH_H

#include <stdint.h>
#include <stdbool.h>
#include "runtime/result.h"

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════
// Math Constants
// ═══════════════════════════════════════════════════════════════════════

/**
 * Mathematical constants.
 * Using double precision (f64) for all constants.
 */
extern const double ARIA_MATH_PI;        // π ≈ 3.14159265358979323846
extern const double ARIA_MATH_E;         // e ≈ 2.71828182845904523536
extern const double ARIA_MATH_INFINITY;  // +∞ (IEEE 754)
extern const double ARIA_MATH_NAN;       // Not a Number (IEEE 754)

// ═══════════════════════════════════════════════════════════════════════
// Basic Math Functions
// ═══════════════════════════════════════════════════════════════════════

/**
 * Absolute value.
 * 
 * @param x Input value
 * @return |x|
 */
double aria_math_abs(double x);

/**
 * Absolute value for integers.
 * 
 * @param x Input value
 * @return |x|
 */
int64_t aria_math_abs_i64(int64_t x);

/**
 * Minimum of two values.
 * 
 * @param a First value
 * @param b Second value
 * @return min(a, b)
 */
double aria_math_min(double a, double b);

/**
 * Maximum of two values.
 * 
 * @param a First value
 * @param b Second value
 * @return max(a, b)
 */
double aria_math_max(double a, double b);

/**
 * Minimum of two integers.
 * 
 * @param a First value
 * @param b Second value
 * @return min(a, b)
 */
int64_t aria_math_min_i64(int64_t a, int64_t b);

/**
 * Maximum of two integers.
 * 
 * @param a First value
 * @param b Second value
 * @return max(a, b)
 */
int64_t aria_math_max_i64(int64_t a, int64_t b);

// ═══════════════════════════════════════════════════════════════════════
// Power and Root Functions
// ═══════════════════════════════════════════════════════════════════════

/**
 * Square root.
 * Returns NaN for negative input.
 * 
 * @param x Input value (must be >= 0)
 * @return √x or NaN if x < 0
 */
double aria_math_sqrt(double x);

/**
 * Power function: x^y.
 * 
 * @param x Base
 * @param y Exponent
 * @return x^y
 */
double aria_math_pow(double x, double y);

// ═══════════════════════════════════════════════════════════════════════
// Exponential and Logarithmic Functions
// ═══════════════════════════════════════════════════════════════════════

/**
 * Natural exponential: e^x.
 * 
 * @param x Exponent
 * @return e^x
 */
double aria_math_exp(double x);

/**
 * Natural logarithm: ln(x).
 * Returns NaN for x <= 0.
 * 
 * @param x Input value (must be > 0)
 * @return ln(x) or NaN if x <= 0
 */
double aria_math_log(double x);

/**
 * Base-10 logarithm: log₁₀(x).
 * Returns NaN for x <= 0.
 * 
 * @param x Input value (must be > 0)
 * @return log₁₀(x) or NaN if x <= 0
 */
double aria_math_log10(double x);

/**
 * Base-2 logarithm: log₂(x).
 * Returns NaN for x <= 0.
 * 
 * @param x Input value (must be > 0)
 * @return log₂(x) or NaN if x <= 0
 */
double aria_math_log2(double x);

// ═══════════════════════════════════════════════════════════════════════
// Trigonometric Functions
// ═══════════════════════════════════════════════════════════════════════

/**
 * Sine function.
 * 
 * @param x Angle in radians
 * @return sin(x)
 */
double aria_math_sin(double x);

/**
 * Cosine function.
 * 
 * @param x Angle in radians
 * @return cos(x)
 */
double aria_math_cos(double x);

/**
 * Tangent function.
 * 
 * @param x Angle in radians
 * @return tan(x)
 */
double aria_math_tan(double x);

/**
 * Arc sine (inverse sine).
 * Returns NaN for |x| > 1.
 * 
 * @param x Input value (must be in [-1, 1])
 * @return arcsin(x) in radians, or NaN if |x| > 1
 */
double aria_math_asin(double x);

/**
 * Arc cosine (inverse cosine).
 * Returns NaN for |x| > 1.
 * 
 * @param x Input value (must be in [-1, 1])
 * @return arccos(x) in radians, or NaN if |x| > 1
 */
double aria_math_acos(double x);

/**
 * Arc tangent (inverse tangent).
 * 
 * @param x Input value
 * @return arctan(x) in radians
 */
double aria_math_atan(double x);

/**
 * Two-argument arc tangent: atan2(y, x).
 * Computes the angle from the positive x-axis to the point (x, y).
 * 
 * @param y Y coordinate
 * @param x X coordinate
 * @return Angle in radians in range [-π, π]
 */
double aria_math_atan2(double y, double x);

// ═══════════════════════════════════════════════════════════════════════
// Rounding and Truncation Functions
// ═══════════════════════════════════════════════════════════════════════

/**
 * Floor function: largest integer <= x.
 * 
 * @param x Input value
 * @return ⌊x⌋
 */
double aria_math_floor(double x);

/**
 * Ceiling function: smallest integer >= x.
 * 
 * @param x Input value
 * @return ⌈x⌉
 */
double aria_math_ceil(double x);

/**
 * Round to nearest integer.
 * Rounds half-way cases away from zero.
 * 
 * @param x Input value
 * @return Rounded value
 */
double aria_math_round(double x);

/**
 * Truncate toward zero.
 * 
 * @param x Input value
 * @return Integer part of x
 */
double aria_math_trunc(double x);

// ═══════════════════════════════════════════════════════════════════════
// Utility Functions
// ═══════════════════════════════════════════════════════════════════════

/**
 * Check if value is NaN.
 * 
 * @param x Value to check
 * @return true if x is NaN, false otherwise
 */
bool aria_math_is_nan(double x);

/**
 * Check if value is infinite.
 * 
 * @param x Value to check
 * @return true if x is +∞ or -∞, false otherwise
 */
bool aria_math_is_inf(double x);

/**
 * Check if value is finite (not NaN or infinite).
 * 
 * @param x Value to check
 * @return true if x is finite, false otherwise
 */
bool aria_math_is_finite(double x);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_MATH_H

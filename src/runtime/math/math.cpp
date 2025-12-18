/**
 * Phase 6.4 Standard Library - Math Operations Implementation
 * 
 * Implements advanced mathematical functions by wrapping C standard library <cmath>.
 * 
 * NOTE: Basic math functions (abs_i64, min_i64, max_i64, sqrt, pow) are in stdlib.cpp
 *       to avoid duplication. This file only provides advanced functions and constants.
 */

#include "runtime/math.h"
#include <cmath>
#include <limits>

// ═══════════════════════════════════════════════════════════════════════
// Math Constants
// ═══════════════════════════════════════════════════════════════════════

const double ARIA_MATH_PI = 3.14159265358979323846;
const double ARIA_MATH_E = 2.71828182845904523536;
const double ARIA_MATH_INFINITY = std::numeric_limits<double>::infinity();
const double ARIA_MATH_NAN = std::numeric_limits<double>::quiet_NaN();

// ═══════════════════════════════════════════════════════════════════════
// Advanced Math Functions (double versions for min/max)
// ═══════════════════════════════════════════════════════════════════════

double aria_math_abs(double x) {
    return std::fabs(x);
}

double aria_math_min(double a, double b) {
    return std::fmin(a, b);
}

double aria_math_max(double a, double b) {
    return std::fmax(a, b);
}

// ═══════════════════════════════════════════════════════════════════════
// Exponential and Logarithmic Functions
// ═══════════════════════════════════════════════════════════════════════

double aria_math_exp(double x) {
    return std::exp(x);
}

double aria_math_log(double x) {
    return std::log(x);
}

double aria_math_log10(double x) {
    return std::log10(x);
}

double aria_math_log2(double x) {
    return std::log2(x);
}

// ═══════════════════════════════════════════════════════════════════════
// Trigonometric Functions
// ═══════════════════════════════════════════════════════════════════════

double aria_math_sin(double x) {
    return std::sin(x);
}

double aria_math_cos(double x) {
    return std::cos(x);
}

double aria_math_tan(double x) {
    return std::tan(x);
}

double aria_math_asin(double x) {
    return std::asin(x);
}

double aria_math_acos(double x) {
    return std::acos(x);
}

double aria_math_atan(double x) {
    return std::atan(x);
}

double aria_math_atan2(double y, double x) {
    return std::atan2(y, x);
}

// ═══════════════════════════════════════════════════════════════════════
// Rounding and Truncation Functions
// ═══════════════════════════════════════════════════════════════════════

double aria_math_floor(double x) {
    return std::floor(x);
}

double aria_math_ceil(double x) {
    return std::ceil(x);
}

double aria_math_round(double x) {
    return std::round(x);
}

double aria_math_trunc(double x) {
    return std::trunc(x);
}

// ═══════════════════════════════════════════════════════════════════════
// Utility Functions
// ═══════════════════════════════════════════════════════════════════════

bool aria_math_is_nan(double x) {
    return std::isnan(x);
}

bool aria_math_is_inf(double x) {
    return std::isinf(x);
}

bool aria_math_is_finite(double x) {
    return std::isfinite(x);
}

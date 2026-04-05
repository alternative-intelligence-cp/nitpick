// Aria Fraction Operations - Exact Rational Arithmetic
// Mixed-number representation: value = whole + num/denom
// Built on TBB types for sticky error propagation

#include "backend/runtime/frac_ops.h"
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <limits>

// ============================================================================
// TBB Sentinel Values
// ============================================================================

template<typename T> inline T tbb_err_sentinel();
template<> inline int8_t tbb_err_sentinel<int8_t>() { return -128; }
template<> inline int16_t tbb_err_sentinel<int16_t>() { return -32768; }
template<> inline int32_t tbb_err_sentinel<int32_t>() { return INT32_MIN; }
template<> inline int64_t tbb_err_sentinel<int64_t>() { return INT64_MIN; }

template<typename T> inline T tbb_max_value();
template<> inline int8_t tbb_max_value<int8_t>() { return 127; }
template<> inline int16_t tbb_max_value<int16_t>() { return 32767; }
template<> inline int32_t tbb_max_value<int32_t>() { return 2147483647; }
template<> inline int64_t tbb_max_value<int64_t>() { return INT64_MAX; }

template<typename T> inline T tbb_min_value();
template<> inline int8_t tbb_min_value<int8_t>() { return -127; }
template<> inline int16_t tbb_min_value<int16_t>() { return -32767; }
template<> inline int32_t tbb_min_value<int32_t>() { return -2147483647; }
template<> inline int64_t tbb_min_value<int64_t>() { return -INT64_MAX; }

// ============================================================================
// TBB Safe Arithmetic
// ============================================================================

template<typename T>
static T tbb_add(T a, T b) {
    T err = tbb_err_sentinel<T>();
    if (a == err || b == err) return err;
    
    // Check overflow
    T max_val = tbb_max_value<T>();
    T min_val = tbb_min_value<T>();
    
    if (a > 0 && b > 0 && a > max_val - b) return err;
    if (a < 0 && b < 0 && a < min_val - b) return err;
    
    return a + b;
}

template<typename T>
static T tbb_sub(T a, T b) {
    T err = tbb_err_sentinel<T>();
    if (a == err || b == err) return err;
    
    T max_val = tbb_max_value<T>();
    T min_val = tbb_min_value<T>();
    
    if (b < 0 && a > max_val + b) return err;
    if (b > 0 && a < min_val + b) return err;
    
    return a - b;
}

template<typename T>
static T tbb_mul(T a, T b) {
    T err = tbb_err_sentinel<T>();
    if (a == err || b == err) return err;
    if (a == 0 || b == 0) return 0;
    
    T max_val = tbb_max_value<T>();
    T min_val = tbb_min_value<T>();
    
    // Check overflow via division
    if (a > 0) {
        if (b > 0 && a > max_val / b) return err;
        if (b < 0 && b < min_val / a) return err;
    } else {
        if (b > 0 && a < min_val / b) return err;
        if (b < 0 && a < max_val / b) return err;
    }
    
    return a * b;
}

template<typename T>
static T tbb_neg(T a) {
    T err = tbb_err_sentinel<T>();
    if (a == err) return err;
    if (a == tbb_min_value<T>()) return err; // -(-127) = 127 is ok, but -(-128) would overflow
    return -a;
}

// ============================================================================
// GCD (Greatest Common Divisor) - Euclidean Algorithm
// ============================================================================

template<typename T>
static T gcd_impl(T a, T b) {
    T err_sentinel = tbb_err_sentinel<T>();
    
    // Error propagation
    if (a == err_sentinel || b == err_sentinel) return err_sentinel;
    
    // Work with absolute values
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    
    // Euclidean algorithm
    while (b != 0) {
        T temp = b;
        b = a % b;
        a = temp;
    }
    
    return a;
}

extern "C" {
    int8_t aria_gcd8(int8_t a, int8_t b) { return gcd_impl(a, b); }
    int16_t aria_gcd16(int16_t a, int16_t b) { return gcd_impl(a, b); }
    int32_t aria_gcd32(int32_t a, int32_t b) { return gcd_impl(a, b); }
    int64_t aria_gcd64(int64_t a, int64_t b) { return gcd_impl(a, b); }
}

// ============================================================================
// Normalize - Enforce Canonical Form
// ============================================================================
// Canonical form invariants:
// 1. denom > 0 (always positive)
// 2. num >= 0 when whole != 0
// 3. num < denom (proper fraction)
// 4. gcd(num, denom) = 1 (reduced)
// 5. Sign handling: whole carries sign, num positive (or num carries sign when whole=0)

template<typename T, typename FracT>
static FracT normalize(FracT f) {
    T err_sentinel = tbb_err_sentinel<T>();
    
    // Check for ERR in components
    if (f.whole == err_sentinel || f.num == err_sentinel || f.denom == err_sentinel) {
        return {err_sentinel, err_sentinel, err_sentinel};
    }
    
    // Division by zero check
    if (f.denom == 0) {
        return {err_sentinel, err_sentinel, err_sentinel};
    }
    
    // Ensure denominator is positive
    if (f.denom < 0) {
        f.denom = -f.denom;
        f.num = -f.num;
    }
    
    // Convert improper fraction to mixed number
    if (f.num < 0) {
        T abs_num = -f.num;
        if (abs_num >= f.denom) {
            T borrow = abs_num / f.denom;
            f.whole = tbb_sub(f.whole, borrow); // Safe subtraction
            f.num = -(abs_num % f.denom);
        }
    } else if (f.num >= f.denom) {
        T carry = f.num / f.denom;
        f.whole = tbb_add(f.whole, carry); // Safe addition
        f.num = f.num % f.denom;
    }
    
    // Check for overflow in whole
    if (f.whole == err_sentinel) {
        return {err_sentinel, err_sentinel, err_sentinel};
    }
    
    // Reduce to lowest terms
    if (f.num != 0) {
        T abs_num = (f.num < 0) ? (T)(-f.num) : f.num;
        T g = gcd_impl(abs_num, f.denom);
        if (g > 1) {
            f.num /= g;
            f.denom /= g;
        }
    }
    
    // Sign handling: If whole != 0, move sign to whole
    // Borrow 1 from whole so num becomes positive: w + (-n/d) = (w-1) + (d-n)/d
    if (f.whole != 0 && f.num < 0) {
        f.whole = tbb_sub(f.whole, (T)1);
        f.num = f.denom + f.num;  // num is negative, so denom + num = denom - |num|
    }
    
    return f;
}

// ============================================================================
// Addition: (w1 + n1/d1) + (w2 + n2/d2)
// ============================================================================

template<typename T, typename FracT>
static FracT add_impl(FracT a, FracT b) {
    T err_sentinel = tbb_err_sentinel<T>();
    
    // Error propagation
    if (a.whole == err_sentinel || b.whole == err_sentinel) {
        return {err_sentinel, err_sentinel, err_sentinel};
    }
    
    // Add whole parts
    T new_whole = tbb_add(a.whole, b.whole);
    if (new_whole == err_sentinel) {
        return {err_sentinel, err_sentinel, err_sentinel};
    }
    
    // Add fractional parts: n1/d1 + n2/d2 = (n1*d2 + n2*d1) / (d1*d2)
    T new_num = tbb_add(tbb_mul(a.num, b.denom), tbb_mul(b.num, a.denom));
    T new_denom = tbb_mul(a.denom, b.denom);
    
    if (new_num == err_sentinel || new_denom == err_sentinel) {
        return {err_sentinel, err_sentinel, err_sentinel};
    }
    
    return normalize<T, FracT>({new_whole, new_num, new_denom});
}

extern "C" {
    void aria_frac8_add(Frac8* result, const Frac8* a, const Frac8* b) {
        *result = add_impl<int8_t, Frac8>(*a, *b);
    }
    
    void aria_frac16_add(Frac16* result, const Frac16* a, const Frac16* b) {
        *result = add_impl<int16_t, Frac16>(*a, *b);
    }
    
    void aria_frac32_add(Frac32* result, const Frac32* a, const Frac32* b) {
        *result = add_impl<int32_t, Frac32>(*a, *b);
    }
    
    void aria_frac64_add(Frac64* result, const Frac64* a, const Frac64* b) {
        *result = add_impl<int64_t, Frac64>(*a, *b);
    }
}

// ============================================================================
// Subtraction: a - b = a + (-b)
// ============================================================================

template<typename T, typename FracT>
static FracT sub_impl(FracT a, FracT b) {
    T err_sentinel = tbb_err_sentinel<T>();
    
    // Negate b
    T neg_whole = tbb_neg(b.whole);
    T neg_num = tbb_neg(b.num);
    
    if (neg_whole == err_sentinel || neg_num == err_sentinel) {
        return {err_sentinel, err_sentinel, err_sentinel};
    }
    
    return add_impl<T, FracT>(a, {neg_whole, neg_num, b.denom});
}

extern "C" {
    void aria_frac8_sub(Frac8* result, const Frac8* a, const Frac8* b) {
        *result = sub_impl<int8_t, Frac8>(*a, *b);
    }
    
    void aria_frac16_sub(Frac16* result, const Frac16* a, const Frac16* b) {
        *result = sub_impl<int16_t, Frac16>(*a, *b);
    }
    
    void aria_frac32_sub(Frac32* result, const Frac32* a, const Frac32* b) {
        *result = sub_impl<int32_t, Frac32>(*a, *b);
    }
    
    void aria_frac64_sub(Frac64* result, const Frac64* a, const Frac64* b) {
        *result = sub_impl<int64_t, Frac64>(*a, *b);
    }
}

// ============================================================================
// Multiplication: (w1 + n1/d1) * (w2 + n2/d2)
// ============================================================================
// Convert to improper: (w*d + n)/d, then multiply

template<typename T, typename FracT>
static FracT mul_impl(FracT a, FracT b) {
    T err_sentinel = tbb_err_sentinel<T>();
    
    // Error propagation
    if (a.whole == err_sentinel || b.whole == err_sentinel) {
        return {err_sentinel, err_sentinel, err_sentinel};
    }
    
    // Convert to improper fractions
    T a_num_total = tbb_add(tbb_mul(a.whole, a.denom), a.num);
    T b_num_total = tbb_add(tbb_mul(b.whole, b.denom), b.num);
    
    if (a_num_total == err_sentinel || b_num_total == err_sentinel) {
        return {err_sentinel, err_sentinel, err_sentinel};
    }
    
    // Multiply: (a_num_total/a_denom) * (b_num_total/b_denom)
    T new_num = tbb_mul(a_num_total, b_num_total);
    T new_denom = tbb_mul(a.denom, b.denom);
    
    if (new_num == err_sentinel || new_denom == err_sentinel) {
        return {err_sentinel, err_sentinel, err_sentinel};
    }
    
    return normalize<T, FracT>({0, new_num, new_denom});
}

extern "C" {
    void aria_frac8_mul(Frac8* result, const Frac8* a, const Frac8* b) {
        *result = mul_impl<int8_t, Frac8>(*a, *b);
    }
    
    void aria_frac16_mul(Frac16* result, const Frac16* a, const Frac16* b) {
        *result = mul_impl<int16_t, Frac16>(*a, *b);
    }
    
    void aria_frac32_mul(Frac32* result, const Frac32* a, const Frac32* b) {
        *result = mul_impl<int32_t, Frac32>(*a, *b);
    }
    
    void aria_frac64_mul(Frac64* result, const Frac64* a, const Frac64* b) {
        *result = mul_impl<int64_t, Frac64>(*a, *b);
    }
}

// ============================================================================
// Division: a / b = a * (1/b)
// ============================================================================

template<typename T, typename FracT>
static FracT div_impl(FracT a, FracT b) {
    T err_sentinel = tbb_err_sentinel<T>();
    
    // Error propagation
    if (a.whole == err_sentinel || b.whole == err_sentinel) {
        return {err_sentinel, err_sentinel, err_sentinel};
    }
    
    // Convert b to improper fraction
    T b_num_total = tbb_add(tbb_mul(b.whole, b.denom), b.num);
    
    if (b_num_total == err_sentinel) {
        return {err_sentinel, err_sentinel, err_sentinel};
    }
    
    // Division by zero check
    if (b_num_total == 0) {
        return {err_sentinel, err_sentinel, err_sentinel};
    }
    
    // Reciprocate b: (b_denom / b_num_total)
    FracT b_recip = {0, b.denom, b_num_total};
    
    return mul_impl<T, FracT>(a, normalize<T, FracT>(b_recip));
}

extern "C" {
    void aria_frac8_div(Frac8* result, const Frac8* a, const Frac8* b) {
        *result = div_impl<int8_t, Frac8>(*a, *b);
    }
    
    void aria_frac16_div(Frac16* result, const Frac16* a, const Frac16* b) {
        *result = div_impl<int16_t, Frac16>(*a, *b);
    }
    
    void aria_frac32_div(Frac32* result, const Frac32* a, const Frac32* b) {
        *result = div_impl<int32_t, Frac32>(*a, *b);
    }
    
    void aria_frac64_div(Frac64* result, const Frac64* a, const Frac64* b) {
        *result = div_impl<int64_t, Frac64>(*a, *b);
    }
}

// ============================================================================
// Negation Operation
// ============================================================================

template<typename T, typename FracT>
static FracT neg_impl(FracT a) {
    T err_sentinel = tbb_err_sentinel<T>();
    
    // Error propagation
    if (a.whole == err_sentinel) {
        return a; // Return ERR
    }
    
    // Negate whole and numerator using TBB negation
    T neg_whole = tbb_neg(a.whole);
    T neg_num = tbb_neg(a.num);
    
    // Check for overflow
    if (neg_whole == err_sentinel || neg_num == err_sentinel) {
        return {err_sentinel, err_sentinel, err_sentinel};
    }
    
    // Denominator stays the same
    return {neg_whole, neg_num, a.denom};
}

extern "C" {
    void aria_frac8_neg(Frac8* result, const Frac8* a) {
        *result = neg_impl<int8_t, Frac8>(*a);
    }
    
    void aria_frac16_neg(Frac16* result, const Frac16* a) {
        *result = neg_impl<int16_t, Frac16>(*a);
    }
    
    void aria_frac32_neg(Frac32* result, const Frac32* a) {
        *result = neg_impl<int32_t, Frac32>(*a);
    }
    
    void aria_frac64_neg(Frac64* result, const Frac64* a) {
        *result = neg_impl<int64_t, Frac64>(*a);
    }
}

// ============================================================================
// Comparison Operations
// ============================================================================

template<typename T, typename FracT>
static int32_t compare_impl(FracT a, FracT b) {
    T err_sentinel = tbb_err_sentinel<T>();
    
    // Error propagation - ERR compares as "unordered"
    if (a.whole == err_sentinel || b.whole == err_sentinel) {
        return 0; // Treat as equal (undefined)
    }
    
    // Compare whole parts first
    if (a.whole < b.whole) return -1;
    if (a.whole > b.whole) return 1;
    
    // Whole parts equal, compare fractional parts
    // n1/d1 <=> n2/d2 is equivalent to n1*d2 <=> n2*d1
    T left = tbb_mul(a.num, b.denom);
    T right = tbb_mul(b.num, a.denom);
    
    if (left == err_sentinel || right == err_sentinel) return 0;
    
    if (left < right) return -1;
    if (left > right) return 1;
    return 0;
}

extern "C" {
    int32_t aria_frac8_cmp(const Frac8* a, const Frac8* b) {
        return compare_impl<int8_t, Frac8>(*a, *b);
    }
    
    int32_t aria_frac16_cmp(const Frac16* a, const Frac16* b) {
        return compare_impl<int16_t, Frac16>(*a, *b);
    }
    
    int32_t aria_frac32_cmp(const Frac32* a, const Frac32* b) {
        return compare_impl<int32_t, Frac32>(*a, *b);
    }
    
    int32_t aria_frac64_cmp(const Frac64* a, const Frac64* b) {
        return compare_impl<int64_t, Frac64>(*a, *b);
    }

    // ========================================================================
    // Conversion Operations
    // ========================================================================

    // To integer (rounds toward zero)
    int8_t aria_frac8_to_int(const Frac8* f) {
        return f->whole + (f->num / f->denom);
    }

    int16_t aria_frac16_to_int(const Frac16* f) {
        return f->whole + (f->num / f->denom);
    }

    int32_t aria_frac32_to_int(const Frac32* f) {
        return f->whole + (f->num / f->denom);
    }

    int64_t aria_frac64_to_int(const Frac64* f) {
        return f->whole + (f->num / f->denom);
    }

    // To float
    float aria_frac8_to_float(const Frac8* f) {
        return static_cast<float>(f->whole) + 
               (static_cast<float>(f->num) / static_cast<float>(f->denom));
    }

    float aria_frac16_to_float(const Frac16* f) {
        return static_cast<float>(f->whole) + 
               (static_cast<float>(f->num) / static_cast<float>(f->denom));
    }

    float aria_frac32_to_float(const Frac32* f) {
        return static_cast<float>(f->whole) + 
               (static_cast<float>(f->num) / static_cast<float>(f->denom));
    }

    double aria_frac64_to_float(const Frac64* f) {
        return static_cast<double>(f->whole) + 
               (static_cast<double>(f->num) / static_cast<double>(f->denom));
    }
}

// ============================================================================
// String Conversion (for printing)
// ============================================================================

template<typename T, typename FracT>
static int aria_frac_to_string(char* buffer, size_t size, FracT f) {
    T err_sentinel = tbb_err_sentinel<T>();
    
    if (f.whole == err_sentinel) {
        return snprintf(buffer, size, "ERR");
    }
    
    if (f.num == 0) {
        return snprintf(buffer, size, "%lld", (long long)f.whole);
    }
    
    if (f.whole == 0) {
        return snprintf(buffer, size, "%lld/%lld", (long long)f.num, (long long)f.denom);
    }
    
    return snprintf(buffer, size, "%lld %lld/%lld", 
                    (long long)f.whole, (long long)f.num, (long long)f.denom);
}

extern "C" {
    int aria_frac8_to_string(char* buffer, size_t size, const Frac8* f) {
        return aria_frac_to_string<int8_t, Frac8>(buffer, size, *f);
    }
    
    int aria_frac16_to_string(char* buffer, size_t size, const Frac16* f) {
        return aria_frac_to_string<int16_t, Frac16>(buffer, size, *f);
    }
    
    int aria_frac32_to_string(char* buffer, size_t size, const Frac32* f) {
        return aria_frac_to_string<int32_t, Frac32>(buffer, size, *f);
    }
    
    int aria_frac64_to_string(char* buffer, size_t size, const Frac64* f) {
        return aria_frac_to_string<int64_t, Frac64>(buffer, size, *f);
    }
}

// ============================================================================
// Construction from parts (called by IR-generated code — bare names, no aria_ prefix)
// ============================================================================

extern "C" {
    Frac8  frac8_from_parts(int8_t whole, int8_t num, int8_t denom)   { Frac8  r = {whole, num, denom}; return r; }
    Frac16 frac16_from_parts(int16_t whole, int16_t num, int16_t denom){ Frac16 r = {whole, num, denom}; return r; }
    Frac32 frac32_from_parts(int32_t whole, int32_t num, int32_t denom){ Frac32 r = {whole, num, denom}; return r; }
    Frac64 frac64_from_parts(int64_t whole, int64_t num, int64_t denom){ Frac64 r = {whole, num, denom}; return r; }
}

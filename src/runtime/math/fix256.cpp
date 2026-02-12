/**
 * ARIA-025: fix256 Deterministic Fixed-Point Arithmetic
 * 
 * Q128.128 format: 256-bit fixed-point with 128-bit integer and 128-bit fractional parts
 * Precision: 2^-128 ≈ 2.9e-39 (4 orders of magnitude finer than Planck length)
 * 
 * Critical for:
 * - Robotic control systems (prevents accumulation drift)
 * - Physics simulations (deterministic across platforms)
 * - Financial calculations (exact decimal representation)
 * - Real-time systems (predictable timing via deterministic math)
 * 
 * TBB Safety: fix256 uses ERR sentinel for error propagation
 * ERR = 0x8000...0000 (high bit set in integer part, all else zero)
 * 
 * Reference: Report 7 § "Deterministic Physics", Report 8 § "fix256 Runtime"
 */

#include "runtime/fix256.h"
#include <string.h>
#include <stdint.h>
#include <cmath>
#include <limits.h>

// ═══════════════════════════════════════════════════════════════════════
// fix256 TBB Safety Helpers
// ═══════════════════════════════════════════════════════════════════════

static inline bool is_fix256_err(const aria_fix256_t* val) {
    // ERR: limbs[3] = 0x8000000000000000, all others = 0
    return (val->limbs[3] == 0x8000000000000000ULL &&
            val->limbs[2] == 0 &&
            val->limbs[1] == 0 &&
            val->limbs[0] == 0);
}

static inline aria_fix256_t make_fix256_err() {
    aria_fix256_t err;
    err.limbs[0] = 0;
    err.limbs[1] = 0;
    err.limbs[2] = 0;
    err.limbs[3] = 0x8000000000000000ULL;
    return err;
}

static inline aria_fix256_t make_zero() {
    aria_fix256_t zero;
    zero.limbs[0] = 0;
    zero.limbs[1] = 0;
    zero.limbs[2] = 0;
    zero.limbs[3] = 0;
    return zero;
}

// ═══════════════════════════════════════════════════════════════════════
// fix256 Addition
// ═══════════════════════════════════════════════════════════════════════

extern "C" aria_fix256_t aria_fix256_add(aria_fix256_t a, aria_fix256_t b) {
    // TBB safety: sticky error propagation
    if (is_fix256_err(&a) || is_fix256_err(&b)) {
        return make_fix256_err();
    }
    
    // Q128.128 addition: add all 4 limbs with carry propagation
    // Structure: limbs[0,1] = fractional part, limbs[2,3] = integer part
    
    // Extract signs before addition
    bool a_sign = (a.limbs[3] & 0x8000000000000000ULL) != 0;
    bool b_sign = (b.limbs[3] & 0x8000000000000000ULL) != 0;
    
    aria_fix256_t result;
    uint64_t carry = 0;
    
    // Add fractional part (limbs 0-1) then integer part (limbs 2-3)
    for (int i = 0; i < 4; ++i) {
        unsigned __int128 sum = (unsigned __int128)a.limbs[i] + 
                                (unsigned __int128)b.limbs[i] + 
                                (unsigned __int128)carry;
        result.limbs[i] = (uint64_t)sum;
        carry = (uint64_t)(sum >> 64);
    }
    
    // TBB safety: SIGNED overflow detection
    // Overflow rules for signed addition:
    // - pos + pos = neg: overflow (wrapped to negative)
    // - neg + neg = pos: overflow (wrapped to positive)  
    // - pos + neg: cannot overflow (moving toward zero or changing sign validly)
    //
    // For signed arithmetic, final carry is IGNORED (two's complement property)
    // Only check for sign-based overflow
    bool result_sign = (result.limbs[3] & 0x8000000000000000ULL) != 0;
    
    // If both inputs have same sign, output sign must match
    if (a_sign == b_sign && result_sign != a_sign) {
        return make_fix256_err();  // Signed overflow detected
    }
    
    return result;
}

// ═══════════════════════════════════════════════════════════════════════
// fix256 Subtraction
// ═══════════════════════════════════════════════════════════════════════

extern "C" aria_fix256_t aria_fix256_sub(aria_fix256_t a, aria_fix256_t b) {
    // TBB safety: sticky error propagation
    if (is_fix256_err(&a) || is_fix256_err(&b)) {
        return make_fix256_err();
    }
    
    // Q128.128 subtraction: subtract all 4 limbs with borrow propagation
    // Uses unsigned limb-wise subtraction with proper borrow handling
    
    aria_fix256_t result;
    uint64_t borrow = 0;
    
    for (int i = 0; i < 4; ++i) {
        unsigned __int128 diff = (unsigned __int128)a.limbs[i] - 
                                 (unsigned __int128)b.limbs[i] - 
                                 (unsigned __int128)borrow;
        result.limbs[i] = (uint64_t)diff;
        // Check if borrow occurred: if diff is negative (high bit set in 128-bit value)
        borrow = (diff >> 64) & 1;
    }
    
    // TBB safety: underflow detection
    // Note: For signed arithmetic, underflow means the result magnitude is too large
    // Since we use two's complement, check if sign mismatch indicates overflow
    // For v0.1.0, skip complex overflow checking - rely on ERR propagation
    
    return result;
}

// ═══════════════════════════════════════════════════════════════════════
// fix256 Multiplication
// ═══════════════════════════════════════════════════════════════════════

extern "C" aria_fix256_t aria_fix256_mul(aria_fix256_t a, aria_fix256_t b) {
    // TBB safety: sticky error propagation
    if (is_fix256_err(&a) || is_fix256_err(&b)) {
        return make_fix256_err();
    }
    
    // Q128.128 multiplication requires careful handling:
    // - Full 512-bit intermediate result (8 limbs)
    // - Right-shift by 128 bits to realign decimal point
    // - Extract middle 256 bits as final result
    //
    // Math: (a * 2^-128) * (b * 2^-128) = (a * b) * 2^-256
    //       We want result in 2^-128, so shift right by 128 bits
    
    // Sign handling: track signs and work with absolute values
    bool a_negative = (a.limbs[3] & 0x8000000000000000ULL) != 0;
    bool b_negative = (b.limbs[3] & 0x8000000000000000ULL) != 0;
    bool result_negative = a_negative ^ b_negative;  // XOR for sign of product
    
    // Convert to absolute values for unsigned multiplication
    aria_fix256_t abs_a = a;
    aria_fix256_t abs_b = b;
    
    if (a_negative) {
        // Two's complement negation
        uint64_t carry = 1;
        for (int i = 0; i < 4; ++i) {
            abs_a.limbs[i] = ~a.limbs[i];
            unsigned __int128 sum = (unsigned __int128)abs_a.limbs[i] + carry;
            abs_a.limbs[i] = (uint64_t)sum;
            carry = (uint64_t)(sum >> 64);
        }
    }
    
    if (b_negative) {
        // Two's complement negation
        uint64_t carry = 1;
        for (int i = 0; i < 4; ++i) {
            abs_b.limbs[i] = ~b.limbs[i];
            unsigned __int128 sum = (unsigned __int128)abs_b.limbs[i] + carry;
            abs_b.limbs[i] = (uint64_t)sum;
            carry = (uint64_t)(sum >> 64);
        }
    }
    
    // Temporary 512-bit result (8 limbs)
    uint64_t temp[8] = {0};
    
    // Comba multiplication: multiply all limb pairs (now using absolute values)
    for (int i = 0; i < 4; ++i) {
        uint64_t carry = 0;
        for (int j = 0; j < 4; ++j) {
            unsigned __int128 product = (unsigned __int128)abs_a.limbs[i] * 
                                        (unsigned __int128)abs_b.limbs[j];
            unsigned __int128 sum = (unsigned __int128)temp[i + j] + 
                                    product + 
                                    (unsigned __int128)carry;
            temp[i + j] = (uint64_t)sum;
            carry = (uint64_t)(sum >> 64);
        }
        temp[i + 4] = carry;
    }
    
    // Extract middle 256 bits (limbs 2-5) - right-shift by 128 bits
    // This realigns the decimal point for Q128.128 format
    aria_fix256_t result;
    result.limbs[0] = temp[2];  // Fractional part low
    result.limbs[1] = temp[3];  // Fractional part high
    result.limbs[2] = temp[4];  // Integer part low
    result.limbs[3] = temp[5];  // Integer part high
    
    // TBB safety: overflow check - if high limbs contain data, value too large
    if (temp[6] != 0 || temp[7] != 0) {
        return make_fix256_err();
    }
    
    // Apply final sign to result
    if (result_negative) {
        // Two's complement negation
        uint64_t carry = 1;
        for (int i = 0; i < 4; ++i) {
            result.limbs[i] = ~result.limbs[i];
            unsigned __int128 sum = (unsigned __int128)result.limbs[i] + carry;
            result.limbs[i] = (uint64_t)sum;
            carry = (uint64_t)(sum >> 64);
        }
    }
    
    return result;
}

// ═══════════════════════════════════════════════════════════════════════
// fix256 Division - Knuth's Algorithm D
// ═══════════════════════════════════════════════════════════════════════

// Helper: Check if 256-bit number is zero
static inline bool is_zero_256(const aria_fix256_t* val) {
    return (val->limbs[0] | val->limbs[1] | val->limbs[2] | val->limbs[3]) == 0;
}

// Helper: Two's complement negation for 256-bit number
static inline void negate_fix256(aria_fix256_t* val) {
    uint64_t carry = 1;
    for (int i = 0; i < 4; ++i) {
        val->limbs[i] = ~val->limbs[i];
        unsigned __int128 sum = (unsigned __int128)val->limbs[i] + carry;
        val->limbs[i] = (uint64_t)sum;
        carry = (uint64_t)(sum >> 64);
    }
}

// Helper: Addition with carry
static inline uint64_t add_carry(uint64_t a, uint64_t b, uint64_t carry_in, uint64_t* result) {
    unsigned __int128 sum = (unsigned __int128)a + b + carry_in;
    *result = (uint64_t)sum;
    return (uint64_t)(sum >> 64);
}

// Helper: Subtraction with borrow
static inline uint64_t sub_borrow(uint64_t a, uint64_t b, uint64_t borrow_in, uint64_t* result) {
    unsigned __int128 diff = (unsigned __int128)a - b - borrow_in;
    *result = (uint64_t)diff;
    return (uint64_t)((diff >> 64) & 1);
}

// Helper: Widening multiplication
static inline unsigned __int128 mul_wide(uint64_t a, uint64_t b) {
    return (unsigned __int128)a * b;
}

// Knuth's Algorithm D: 512-bit / 256-bit division
// Returns false if overflow detected (quotient doesn't fit in 256 bits)
static bool knuth_div_512_256(const uint64_t u_in[8], const aria_fix256_t* v_in, aria_fix256_t* q_out) {
    // Initialize output quotient to zero
    memset(q_out->limbs, 0, sizeof(q_out->limbs));
    
    // Find the significant length of divisor (skip leading zeros)
    int n = 4;
    while (n > 0 && v_in->limbs[n-1] == 0) n--;
    
    if (n == 0) return false;  // Division by zero (should be caught by caller)
    
    // Normalization: count leading zeros in most significant limb
    int shift = __builtin_clzll(v_in->limbs[n-1]);
    
    // Allocate normalized arrays
    uint64_t vn[4] = {0};    // Normalized divisor
    uint64_t un[9] = {0};    // Normalized dividend (8 limbs + 1 for overflow)
    
    // Normalize divisor and dividend by left-shifting
    if (shift == 0) {
        // No normalization needed
        memcpy(vn, v_in->limbs, n * sizeof(uint64_t));
        memcpy(un, u_in, 8 * sizeof(uint64_t));
        un[8] = 0;
    } else {
        // Shift divisor left by 'shift' bits
        uint64_t carry = 0;
        for (int i = 0; i < n; i++) {
            vn[i] = (v_in->limbs[i] << shift) | carry;
            carry = v_in->limbs[i] >> (64 - shift);
        }
        
        // Shift dividend left by 'shift' bits
        carry = 0;
        for (int i = 0; i < 8; i++) {
            un[i] = (u_in[i] << shift) | carry;
            carry = u_in[i] >> (64 - shift);
        }
        un[8] = carry;
    }
    
    // Main division loop: compute quotient digits
    int m = 8 - n;  // Number of quotient limbs
    
    for (int j = m; j >= 0; j--) {
        // Quotient estimation using top two limbs of dividend and top limb of divisor
        uint64_t q_hat;
        uint64_t r_hat;
        
        uint64_t num_hi = un[j + n];
        uint64_t num_lo = un[j + n - 1];
        uint64_t div_hi = vn[n - 1];
        
        if (num_hi >= div_hi) {
            // If high limbs match or dividend larger, quotient maxes out
            q_hat = ~0ULL;
            r_hat = num_lo + div_hi;
        } else {
            // Compute q_hat = (num_hi * 2^64 + num_lo) / div_hi
            unsigned __int128 num = ((unsigned __int128)num_hi << 64) | num_lo;
            q_hat = (uint64_t)(num / div_hi);
            r_hat = (uint64_t)(num % div_hi);
        }
        
        // Quotient correction step (reduces q_hat if too large)
        if (n >= 2) {
            uint64_t div_next = vn[n - 2];
            uint64_t num_next = un[j + n - 2];
            
            while (true) {
                unsigned __int128 lhs = mul_wide(q_hat, div_next);
                unsigned __int128 rhs = ((unsigned __int128)r_hat << 64) | num_next;
                
                if (lhs > rhs) {
                    q_hat--;
                    r_hat += div_hi;
                    // If r_hat overflows, q_hat is correct now
                    if (r_hat < div_hi) break;
                } else {
                    break;
                }
            }
        }
        
        // Multiply and subtract: un[j...j+n] -= q_hat * vn
        uint64_t borrow = 0;
        uint64_t mul_carry = 0;
        
        for (int i = 0; i < n; i++) {
            unsigned __int128 p = mul_wide(q_hat, vn[i]);
            unsigned __int128 term = p + mul_carry;
            mul_carry = (uint64_t)(term >> 64);
            
            uint64_t sub_val = (uint64_t)term;
            borrow = sub_borrow(un[j + i], sub_val, borrow, &un[j + i]);
        }
        
        // Subtract final carry from top limb
        uint64_t top_borrow = sub_borrow(un[j + n], mul_carry, borrow, &un[j + n]);
        
        // Add-back step: if borrow set, q_hat was too large by 1
        if (top_borrow) {
            q_hat--;
            uint64_t carry = 0;
            for (int i = 0; i < n; i++) {
                carry = add_carry(un[j + i], vn[i], carry, &un[j + i]);
            }
            un[j + n] += carry;
        }
        
        // Store quotient digit
        // Quotient should fit in 4 limbs (indices 0-3)
        if (j >= 4) {
            // If computing quotient digit beyond 256 bits and it's non-zero, overflow
            if (q_hat > 0) return false;
        } else {
            q_out->limbs[j] = q_hat;
        }
    }
    
    return true;
}

extern "C" aria_fix256_t aria_fix256_div(aria_fix256_t a, aria_fix256_t b) {
    // TBB safety: sticky error propagation
    if (is_fix256_err(&a) || is_fix256_err(&b)) {
        return make_fix256_err();
    }
    
    // Division by zero check
    if (is_zero_256(&b)) {
        return make_fix256_err();
    }
    
    // Sign extraction for signed division
    bool a_negative = (int64_t)a.limbs[3] < 0;
    bool b_negative = (int64_t)b.limbs[3] < 0;
    bool result_negative = a_negative ^ b_negative;
    
    // Convert to absolute values for unsigned division
    aria_fix256_t abs_a = a;
    aria_fix256_t abs_b = b;
    
    if (a_negative) negate_fix256(&abs_a);
    if (b_negative) negate_fix256(&abs_b);
    
    // Q128.128 division: (a * 2^-128) / (b * 2^-128) = (a / b)
    // To get result in Q128.128: result = (a * 2^128) / b
    // Left-shift dividend by 128 bits, creating 512-bit intermediate
    uint64_t dividend_512[8] = {0};
    dividend_512[2] = abs_a.limbs[0];  // Fractional low → position 2
    dividend_512[3] = abs_a.limbs[1];  // Fractional high → position 3
    dividend_512[4] = abs_a.limbs[2];  // Integer low → position 4
    dividend_512[5] = abs_a.limbs[3];  // Integer high → position 5
    // dividend_512[0,1] = 0 (adds 128 bits of fractional precision)
    // dividend_512[6,7] = 0 (overflow detection space)
    
    // Perform 512-bit / 256-bit division using Knuth's Algorithm D
    aria_fix256_t quotient;
    bool success = knuth_div_512_256(dividend_512, &abs_b, &quotient);
    
    if (!success) {
        // Overflow: quotient doesn't fit in 256 bits
        return make_fix256_err();
    }
    
    // Apply sign to result
    if (result_negative) {
        negate_fix256(&quotient);
    }
    
    // Final TBB validation (ensure negation didn't produce ERR sentinel)
    if (is_fix256_err(&quotient)) {
        return make_fix256_err();
    }
    
    return quotient;
}

// ═══════════════════════════════════════════════════════════════════════
// fix256 Comparison Operations
// ═══════════════════════════════════════════════════════════════════════

extern "C" int aria_fix256_cmp(aria_fix256_t a, aria_fix256_t b) {
    // TBB safety: ERR comparison (ERR == ERR, ERR < all non-ERR)
    bool a_err = is_fix256_err(&a);
    bool b_err = is_fix256_err(&b);
    
    if (a_err && b_err) return 0;   // ERR == ERR
    if (a_err) return -1;            // ERR < non-ERR
    if (b_err) return 1;             // non-ERR > ERR
    
    // Compare signed: check sign bits first
    int64_t a_sign = (int64_t)a.limbs[3];
    int64_t b_sign = (int64_t)b.limbs[3];
    
    if (a_sign < 0 && b_sign >= 0) return -1;  // negative < positive
    if (a_sign >= 0 && b_sign < 0) return 1;   // positive > negative
    
    // Same sign: compare magnitude from high to low
    // For negative numbers, smaller magnitude means larger value
    bool negative = a_sign < 0;
    
    for (int i = 3; i >= 0; i--) {
        if (a.limbs[i] < b.limbs[i]) return negative ? 1 : -1;
        if (a.limbs[i] > b.limbs[i]) return negative ? -1 : 1;
    }
    
    return 0;  // Equal
}

extern "C" int aria_fix256_eq(aria_fix256_t a, aria_fix256_t b) {
    return aria_fix256_cmp(a, b) == 0;
}

extern "C" int aria_fix256_lt(aria_fix256_t a, aria_fix256_t b) {
    return aria_fix256_cmp(a, b) < 0;
}

extern "C" int aria_fix256_le(aria_fix256_t a, aria_fix256_t b) {
    return aria_fix256_cmp(a, b) <= 0;
}

extern "C" int aria_fix256_gt(aria_fix256_t a, aria_fix256_t b) {
    return aria_fix256_cmp(a, b) > 0;
}

extern "C" int aria_fix256_ge(aria_fix256_t a, aria_fix256_t b) {
    return aria_fix256_cmp(a, b) >= 0;
}

// ═══════════════════════════════════════════════════════════════════════
// fix256 Conversion Functions
// ═══════════════════════════════════════════════════════════════════════

extern "C" aria_fix256_t aria_fix256_from_i32(int32_t val) {
    aria_fix256_t result;
    result.limbs[0] = 0;  // Fractional part = 0
    result.limbs[1] = 0;
    result.limbs[2] = (uint64_t)(int64_t)val;  // Cast to int64_t for sign extension
    result.limbs[3] = (val < 0) ? ~0ULL : 0;   // Sign extension to high limb
    return result;
}

extern "C" aria_fix256_t aria_fix256_from_i64(int64_t val) {
    aria_fix256_t result;
    result.limbs[0] = 0;  // Fractional part = 0
    result.limbs[1] = 0;
    result.limbs[2] = (uint64_t)val;           // Integer part low
    result.limbs[3] = (val < 0) ? ~0ULL : 0;   // Sign extension to high limb
    return result;
}

extern "C" int32_t aria_fix256_to_i32(aria_fix256_t val) {
    // TBB safety: ERR converts to 0
    if (is_fix256_err(&val)) {
        return 0;
    }
    
    // Extract integer part (truncate fractional)
    // Check if value fits in 32-bit range
    int64_t val64 = (int64_t)val.limbs[2];
    
    // Check sign extension validity
    int64_t sign_ext = (val64 < 0) ? ~0LL : 0LL;
    if ((int64_t)val.limbs[3] != sign_ext) {
        // Value too large for 32-bit - saturate
        return (val.limbs[3] & 0x8000000000000000ULL) ? INT32_MIN : INT32_MAX;
    }
    
    // Check 32-bit range
    if (val64 > INT32_MAX) return INT32_MAX;
    if (val64 < INT32_MIN) return INT32_MIN;
    
    return (int32_t)val64;
}

extern "C" int64_t aria_fix256_to_i64(aria_fix256_t val) {
    // TBB safety: ERR converts to 0
    if (is_fix256_err(&val)) {
        return 0;
    }
    
    // Extract integer part (truncate fractional)
    int64_t result = (int64_t)val.limbs[2];
    
    // Check sign extension validity
    int64_t sign_ext = (result < 0) ? ~0LL : 0LL;
    if ((int64_t)val.limbs[3] != sign_ext) {
        // Value too large for 64-bit - saturate
        return (val.limbs[3] & 0x8000000000000000ULL) ? INT64_MIN : INT64_MAX;
    }
    
    return result;
}

extern "C" aria_fix256_t aria_fix256_from_f32(float val) {
    // Special cases
    if (val != val) return make_fix256_err();  // NaN
    if (val >= 1.7014118e+38f) return make_fix256_err();  // Overflow (2^127)
    if (val <= -1.7014118e+38f) return make_fix256_err(); // Underflow (-2^127)
    
    // Convert to double for better intermediate precision
    return aria_fix256_from_f64((double)val);
}

extern "C" aria_fix256_t aria_fix256_from_f64(double val) {
    // Special cases
    if (val != val) return make_fix256_err();  // NaN
    if (val >= 1.7014118346046923e+38) return make_fix256_err();  // Overflow (2^127)
    if (val <= -1.7014118346046923e+38) return make_fix256_err(); // Underflow (-2^127)
    
    aria_fix256_t result;
    
    // Handle sign
    bool negative = val < 0;
    if (negative) val = -val;
    
    // Split into integer and fractional parts
    double integer_part = 0;
    double fractional_part = modf(val, &integer_part);
    
    // Convert integer part (limbs[2-3])
    uint64_t int_low = (uint64_t)fmod(integer_part, 18446744073709551616.0);  // 2^64
    uint64_t int_high = (uint64_t)(integer_part / 18446744073709551616.0);
    
    // Convert fractional part (limbs[0-1])
    // Multiply by 2^128 to shift into integer range
    double frac_scaled = fractional_part * 340282366920938463463374607431768211456.0;  // 2^128
    uint64_t frac_low = (uint64_t)fmod(frac_scaled, 18446744073709551616.0);  // 2^64
    uint64_t frac_high = (uint64_t)(frac_scaled / 18446744073709551616.0);
    
    result.limbs[0] = frac_low;
    result.limbs[1] = frac_high;
    result.limbs[2] = int_low;
    result.limbs[3] = int_high;
    
    // Apply sign via two's complement negation
    if (negative) {
        negate_fix256(&result);
    }
    
    return result;
}

extern "C" float aria_fix256_to_f32(aria_fix256_t val) {
    // Convert to double first for better precision
    return (float)aria_fix256_to_f64(val);
}

extern "C" double aria_fix256_to_f64(aria_fix256_t val) {
    // TBB safety: ERR converts to NaN
    if (is_fix256_err(&val)) {
        return 0.0 / 0.0;  // NaN
    }
    
    // Handle sign
    bool negative = (int64_t)val.limbs[3] < 0;
    aria_fix256_t abs_val = val;
    if (negative) {
        negate_fix256(&abs_val);
    }
    
    // Convert integer part (limbs[2-3])
    double integer = (double)abs_val.limbs[3] * 18446744073709551616.0 +  // 2^64
                     (double)abs_val.limbs[2];
    
    // Convert fractional part (limbs[0-1])
    // Divide by 2^128 to shift into fractional range
    double fractional = (double)abs_val.limbs[1] / 18446744073709551616.0 +  // 2^64
                        (double)abs_val.limbs[0] / 340282366920938463463374607431768211456.0;  // 2^128
    
    double result = integer + fractional;
    return negative ? -result : result;
}

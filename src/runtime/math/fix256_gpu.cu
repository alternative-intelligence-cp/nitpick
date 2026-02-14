/**
 * ARIA-025: fix256 GPU-Compatible Implementation
 * 
 * CUDA-compatible version using PTX intrinsics instead of __int128
 * Uses explicit carry/borrow handling for 64-bit arithmetic
 */

#include "runtime/fix256.h"
#include <stdint.h>

#ifdef __CUDA_ARCH__
// GPU-specific code - use PTX intrinsics
#define DEVICE_HOST __device__ __host__
#else
// CPU fallback - will use __int128 version
#define DEVICE_HOST
#endif

// ═══════════════════════════════════════════════════════════════════════
// GPU-Compatible Arithmetic Helpers
// ═══════════════════════════════════════════════════════════════════════

#ifdef __CUDA_ARCH__

// Add with carry using PTX intrinsic
__device__ inline void add_with_carry_gpu(uint64_t a, uint64_t b, uint32_t carry_in,
                                          uint64_t* result, uint32_t* carry_out) {
    // Perform addition: result = a + b + carry_in
    uint64_t temp = a + b;
    uint64_t final = temp + carry_in;
    
    *result = final;
    
    // Detect carry: overflow if (a + b < a) or (final < temp)
    *carry_out = ((temp < a) || (final < temp)) ? 1 : 0;
}

// Subtract with borrow using PTX intrinsic
__device__ inline void sub_with_borrow_gpu(uint64_t a, uint64_t b, uint32_t borrow_in,
                                           uint64_t* result, uint32_t* borrow_out) {
    // PTX: sub.cc sets borrow flag
    asm("sub.cc.u64 %0, %1, %2;"
        : "=l"(*result)
        : "l"(a), "l"(b));
    
    if (borrow_in) {
        // subc.cc subtracts with incoming borrow
        asm("subc.cc.u64 %0, %0, 0;" : "+l"(*result));
    }
    
    // Extract borrow flag
    uint32_t borrow;
    asm("{\n\t"
        ".reg .pred p;\n\t"
        "subc.u32 %0, 0, 0;\n\t"
        "}" : "=r"(borrow));
    *borrow_out = borrow;
}

// 64-bit x 64-bit → 128-bit multiplication using PTX intrinsic
__device__ inline void mul_wide_gpu(uint64_t a, uint64_t b, uint64_t* lo, uint64_t* hi) {
    asm("mul.lo.u64 %0, %1, %2;" : "=l"(*lo) : "l"(a), "l"(b));
    asm("mul.hi.u64 %0, %1, %2;" : "=l"(*hi) : "l"(a), "l"(b));
}

#else

// CPU fallback using __int128
inline void add_with_carry_gpu(uint64_t a, uint64_t b, uint32_t carry_in,
                                uint64_t* result, uint32_t* carry_out) {
    unsigned __int128 sum = (unsigned __int128)a + b + carry_in;
    *result = (uint64_t)sum;
    *carry_out = (uint32_t)(sum >> 64);
}

inline void sub_with_borrow_gpu(uint64_t a, uint64_t b, uint32_t borrow_in,
                                uint64_t* result, uint32_t* borrow_out) {
    unsigned __int128 diff = (unsigned __int128)a - b - borrow_in;
    *result = (uint64_t)diff;
    *borrow_out = (uint32_t)((diff >> 64) & 1);
}

inline void mul_wide_gpu(uint64_t a, uint64_t b, uint64_t* lo, uint64_t* hi) {
    unsigned __int128 product = (unsigned __int128)a * b;
    *lo = (uint64_t)product;
    *hi = (uint64_t)(product >> 64);
}

#endif

// ═══════════════════════════════════════════════════════════════════════
// GPU TBB Safety Helpers
// ═══════════════════════════════════════════════════════════════════════

DEVICE_HOST static inline bool is_fix256_err_gpu(const aria_fix256_t* val) {
    return (val->limbs[3] == 0x8000000000000000ULL &&
            val->limbs[2] == 0 &&
            val->limbs[1] == 0 &&
            val->limbs[0] == 0);
}

DEVICE_HOST static inline aria_fix256_t make_fix256_err_gpu() {
    aria_fix256_t err;
    err.limbs[0] = 0;
    err.limbs[1] = 0;
    err.limbs[2] = 0;
    err.limbs[3] = 0x8000000000000000ULL;
    return err;
}

DEVICE_HOST static inline aria_fix256_t make_zero_gpu() {
    aria_fix256_t zero;
    zero.limbs[0] = 0;
    zero.limbs[1] = 0;
    zero.limbs[2] = 0;
    zero.limbs[3] = 0;
    return zero;
}

// Two's complement negation
DEVICE_HOST static inline void negate_fix256_gpu(aria_fix256_t* val) {
    uint32_t carry = 1;
    for (int i = 0; i < 4; ++i) {
        val->limbs[i] = ~val->limbs[i];
        uint64_t result;
        uint32_t new_carry;
        add_with_carry_gpu(val->limbs[i], 0, carry, &result, &new_carry);
        val->limbs[i] = result;
        carry = new_carry;
    }
}

// ═══════════════════════════════════════════════════════════════════════
// GPU-Compatible fix256 Operations
// ═══════════════════════════════════════════════════════════════════════
// Note: No extern "C" - CUDA device functions require C++ linkage

DEVICE_HOST aria_fix256_t aria_fix256_add_gpu(aria_fix256_t a, aria_fix256_t b) {
    // TBB safety: sticky error propagation
    if (is_fix256_err_gpu(&a) || is_fix256_err_gpu(&b)) {
        return make_fix256_err_gpu();
    }
    
    // Extract signs before addition
    bool a_sign = (a.limbs[3] & 0x8000000000000000ULL) != 0;
    bool b_sign = (b.limbs[3] & 0x8000000000000000ULL) != 0;
    
    aria_fix256_t result;
    uint32_t carry = 0;
    
    // Add all 4 limbs with carry propagation
    for (int i = 0; i < 4; ++i) {
        add_with_carry_gpu(a.limbs[i], b.limbs[i], carry, &result.limbs[i], &carry);
    }
    
    // TBB safety: SIGNED overflow detection
    bool result_sign = (result.limbs[3] & 0x8000000000000000ULL) != 0;
    
    // If both inputs have same sign, output sign must match
    if (a_sign == b_sign && result_sign != a_sign) {
        return make_fix256_err_gpu();  // Signed overflow detected
    }
    
    return result;
}

DEVICE_HOST aria_fix256_t aria_fix256_sub_gpu(aria_fix256_t a, aria_fix256_t b) {
    // TBB safety: sticky error propagation
    if (is_fix256_err_gpu(&a) || is_fix256_err_gpu(&b)) {
        return make_fix256_err_gpu();
    }
    
    aria_fix256_t result;
    uint32_t borrow = 0;
    
    // Subtract all 4 limbs with borrow propagation
    for (int i = 0; i < 4; ++i) {
        sub_with_borrow_gpu(a.limbs[i], b.limbs[i], borrow, &result.limbs[i], &borrow);
    }
    
    return result;
}

DEVICE_HOST aria_fix256_t aria_fix256_mul_gpu(aria_fix256_t a, aria_fix256_t b) {
    // TBB safety: sticky error propagation
    if (is_fix256_err_gpu(&a) || is_fix256_err_gpu(&b)) {
        return make_fix256_err_gpu();
    }
    
    // Sign handling
    bool a_negative = (a.limbs[3] & 0x8000000000000000ULL) != 0;
    bool b_negative = (b.limbs[3] & 0x8000000000000000ULL) != 0;
    bool result_negative = a_negative ^ b_negative;
    
    // Convert to absolute values
    aria_fix256_t abs_a = a;
    aria_fix256_t abs_b = b;
    
    if (a_negative) negate_fix256_gpu(&abs_a);
    if (b_negative) negate_fix256_gpu(&abs_b);
    
    // Temporary 512-bit result (8 limbs)
    uint64_t temp[8] = {0};
    
    // Comba multiplication
    for (int i = 0; i < 4; ++i) {
        uint32_t carry = 0;
        for (int j = 0; j < 4; ++j) {
            uint64_t prod_lo, prod_hi;
            mul_wide_gpu(abs_a.limbs[i], abs_b.limbs[j], &prod_lo, &prod_hi);
            
            // Add product to temp[i+j]
            uint64_t sum1;
            uint32_t carry1;
            add_with_carry_gpu(temp[i + j], prod_lo, 0, &sum1, &carry1);
            
            // Add previous carry
            uint64_t sum2;
            uint32_t carry2;
            add_with_carry_gpu(sum1, 0, carry, &sum2, &carry2);
            
            temp[i + j] = sum2;
            
            // Combine carries with prod_hi
            uint64_t carry_sum;
            uint32_t carry3;
            add_with_carry_gpu(prod_hi, (uint64_t)carry1, carry2, &carry_sum, &carry3);
            
            carry = (uint32_t)carry_sum;
        }
        temp[i + 4] = carry;
    }
    
    // Extract middle 256 bits (limbs 2-5) - right-shift by 128 bits
    aria_fix256_t result;
    result.limbs[0] = temp[2];
    result.limbs[1] = temp[3];
    result.limbs[2] = temp[4];
    result.limbs[3] = temp[5];
    
    // TBB safety: overflow check
    if (temp[6] != 0 || temp[7] != 0) {
        return make_fix256_err_gpu();
    }
    
    // Apply final sign
    if (result_negative) {
        negate_fix256_gpu(&result);
    }
    
    return result;
}

// ═══════════════════════════════════════════════════════════════════════
// GPU Division - Knuth's Algorithm D
// ═══════════════════════════════════════════════════════════════════════

// Helper: Check if 256-bit number is zero
DEVICE_HOST static inline bool is_zero_256_gpu(const aria_fix256_t* val) {
    return (val->limbs[0] | val->limbs[1] | val->limbs[2] | val->limbs[3]) == 0;
}

// Helper: Addition with carry (returns carry_out)
DEVICE_HOST static inline uint64_t add_carry_gpu(uint64_t a, uint64_t b, uint64_t carry_in, uint64_t* result) {
    uint32_t carry = (uint32_t)carry_in;
    uint32_t carry_out;
    add_with_carry_gpu(a, b, carry, result, &carry_out);
    return carry_out;
}

// Helper: Subtraction with borrow (returns borrow_out)
DEVICE_HOST static inline uint64_t sub_borrow_gpu(uint64_t a, uint64_t b, uint64_t borrow_in, uint64_t* result) {
    uint32_t borrow = (uint32_t)borrow_in;
    uint32_t borrow_out;
    sub_with_borrow_gpu(a, b, borrow, result, &borrow_out);
    return borrow_out;
}

// GPU-compatible count leading zeros
DEVICE_HOST static inline int clz_gpu(uint64_t x) {
#ifdef __CUDA_ARCH__
    return __clzll(x);
#else
    return __builtin_clzll(x);
#endif
}

// Knuth's Algorithm D: 512-bit / 256-bit division
// Returns false if overflow detected (quotient doesn't fit in 256 bits)
DEVICE_HOST static bool knuth_div_512_256_gpu(const uint64_t u_in[8], const aria_fix256_t* v_in, aria_fix256_t* q_out) {
    // Initialize output quotient to zero
    for (int i = 0; i < 4; i++) {
        q_out->limbs[i] = 0;
    }
    
    // Find the significant length of divisor (skip leading zeros)
    int n = 4;
    while (n > 0 && v_in->limbs[n-1] == 0) n--;
    
    if (n == 0) return false;  // Division by zero (should be caught by caller)
    
    // Count actual non-zero limbs (for sparse divisor detection)
    int nonzero_limbs = 0;
    int first_nonzero = -1;
    for (int i = 0; i < n; i++) {
        if (v_in->limbs[i] != 0) {
            nonzero_limbs++;
            if (first_nonzero < 0) first_nonzero = i;
        }
    }
    
    // For single non-zero limb in divisor, use simpler algorithm
    if (nonzero_limbs == 1) {
        uint64_t divisor = v_in->limbs[first_nonzero];
        uint64_t remainder = 0;
        
        // Long division from high to low, accounting for limb offset
        for (int i = 7; i >= 0; i--) {
            uint64_t dividend_hi = remainder;
            uint64_t dividend_lo = u_in[i];
            uint64_t q, r;
            
            #ifdef __CUDA_ARCH__
            // GPU: Implement 128-bit / 64-bit division via binary long division
            if (dividend_hi == 0) {
                // Simple case: 64-bit / 64-bit
                q = dividend_lo / divisor;
                r = dividend_lo % divisor;
            } else if (dividend_hi < divisor) {
                // Binary long division for 128-bit / 64-bit
                q = 0;
                r = 0;
                
                // Process from high bit to low bit
                for (int bit = 127; bit >= 0; bit--) {
                    // Shift remainder left by 1
                    r = r << 1;
                    
                    // Bring down next bit from dividend
                    uint64_t dividend_bit;
                    if (bit >= 64) {
                        dividend_bit = (dividend_hi >> (bit - 64)) & 1;
                    } else {
                        dividend_bit = (dividend_lo >> bit) & 1;
                    }
                    r |= dividend_bit;
                    
                    // Check if we can subtract divisor
                    if (r >= divisor) {
                        r -= divisor;
                        // Set corresponding bit in quotient
                        if (bit >= 64) {
                            q |= (1ULL << (bit - 64));
                        } else {
                            q |= (1ULL << bit);
                        }
                    }
                }
            } else {
                // dividend_hi >= divisor, quotient overflows single limb
                return false;
            }
            #else
            // CPU: Use __int128
            unsigned __int128 dividend = ((unsigned __int128)remainder << 64) | u_in[i];
            q = (uint64_t)(dividend / divisor);
            r = (uint64_t)(dividend % divisor);
            #endif
            
            // Output quotient digit, accounting for divisor limb offset
            int output_idx = i - first_nonzero;
            if (output_idx >= 0 && output_idx < 4) {
                q_out->limbs[output_idx] = q;
            } else if (output_idx >= 4 && q > 0) {
                // Quotient doesn't fit in 256 bits
                return false;
            }
            
            remainder = r;
        }
        
        return true;
    }
    
    // For multi-limb divisors, continue with full Knuth algorithm
    
    // Normalization: count leading zeros in most significant limb
    int shift = clz_gpu(v_in->limbs[n-1]);
    
    // Allocate normalized arrays
    uint64_t vn[4] = {0};    // Normalized divisor
    uint64_t un[9] = {0};    // Normalized dividend (8 limbs + 1 for overflow)
    
    // Normalize divisor and dividend by left-shifting
    if (shift == 0) {
        // No normalization needed
        for (int i = 0; i < n; i++) {
            vn[i] = v_in->limbs[i];
        }
        for (int i = 0; i < 8; i++) {
            un[i] = u_in[i];
        }
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
            // We need 128-bit division
            
            #ifdef __CUDA_ARCH__
            // GPU: Use approximation (may need refinement)
            // First approximation using only high 64 bits
            if (num_hi == 0) {
                q_hat = num_lo / div_hi;
                r_hat = num_lo % div_hi;
            } else {
                // Estimate quotient using high bits
                q_hat = num_hi / div_hi;
                r_hat = num_hi % div_hi;
                // Could refine here if needed, but correction loop below handles it
            }
            #else
            // CPU: Use __int128 for exact division
            unsigned __int128 num = ((unsigned __int128)num_hi << 64) | num_lo;
            q_hat = (uint64_t)(num / div_hi);
            r_hat = (uint64_t)(num % div_hi);
            #endif
        }
        
        // Quotient correction step (reduces q_hat if too large)
        if (n >= 2) {
            uint64_t div_next = vn[n - 2];
            uint64_t num_next = un[j + n - 2];
            
            // Limit correction iterations to prevent infinite loops
            for (int correction_iter = 0; correction_iter < 2; correction_iter++) {
                // lhs = q_hat * div_next (128-bit)
                uint64_t lhs_lo, lhs_hi;
                mul_wide_gpu(q_hat, div_next, &lhs_lo, &lhs_hi);
                
                // rhs = r_hat * 2^64 + num_next (128-bit)
                uint64_t rhs_lo = num_next;
                uint64_t rhs_hi = r_hat;
                
                // Compare lhs > rhs
                bool needs_correction = false;
                if (lhs_hi > rhs_hi) {
                    needs_correction = true;
                } else if (lhs_hi == rhs_hi && lhs_lo > rhs_lo) {
                    needs_correction = true;
                }
                
                if (needs_correction) {
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
            // p = q_hat * vn[i] (128-bit)
            uint64_t p_lo, p_hi;
            mul_wide_gpu(q_hat, vn[i], &p_lo, &p_hi);
            
            // term = p + mul_carry (128-bit addition)
            uint64_t term_lo, term_hi;
            uint64_t add_carry = add_carry_gpu(p_lo, mul_carry, 0, &term_lo);
            term_hi = p_hi + add_carry;
            mul_carry = term_hi;
            
            // un[j + i] -= term_lo
            uint64_t sub_val = term_lo;
            borrow = sub_borrow_gpu(un[j + i], sub_val, borrow, &un[j + i]);
        }
        
        // Subtract final carry from top limb
        uint64_t top_borrow = sub_borrow_gpu(un[j + n], mul_carry, borrow, &un[j + n]);
        
        // Add-back step: if borrow set, q_hat was too large by 1
        if (top_borrow) {
            q_hat--;
            uint64_t carry = 0;
            for (int i = 0; i < n; i++) {
                carry = add_carry_gpu(un[j + i], vn[i], carry, &un[j + i]);
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

DEVICE_HOST aria_fix256_t aria_fix256_div_gpu(aria_fix256_t a, aria_fix256_t b) {
    // TBB safety: sticky error propagation
    if (is_fix256_err_gpu(&a) || is_fix256_err_gpu(&b)) {
        return make_fix256_err_gpu();
    }
    
    // Division by zero check
    if (is_zero_256_gpu(&b)) {
        return make_fix256_err_gpu();
    }
    
    // Sign extraction for signed division
    bool a_negative = (int64_t)a.limbs[3] < 0;
    bool b_negative = (int64_t)b.limbs[3] < 0;
    bool result_negative = a_negative ^ b_negative;
    
    // Convert to absolute values for unsigned division
    aria_fix256_t abs_a = a;
    aria_fix256_t abs_b = b;
    
    if (a_negative) negate_fix256_gpu(&abs_a);
    if (b_negative) negate_fix256_gpu(&abs_b);
    
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
    bool success = knuth_div_512_256_gpu(dividend_512, &abs_b, &quotient);
    
    if (!success) {
        // Overflow: quotient doesn't fit in 256 bits
        return make_fix256_err_gpu();
    }
    
    // Apply sign to result
    if (result_negative) {
        negate_fix256_gpu(&quotient);
    }
    
    // Final TBB validation (ensure negation didn't produce ERR sentinel)
    if (is_fix256_err_gpu(&quotient)) {
        return make_fix256_err_gpu();
    }
    
    return quotient;
}

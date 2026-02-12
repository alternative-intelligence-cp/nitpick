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
    // PTX: add.cc sets carry flag
    asm("add.cc.u64 %0, %1, %2;"
        : "=l"(*result)
        : "l"(a), "l"(b));
    
    if (carry_in) {
        // addc.cc adds with incoming carry
        asm("addc.cc.u64 %0, %0, 0;" : "+l"(*result));
    }
    
    // Extract carry flag
    uint32_t carry;
    asm("{\n\t"
        ".reg .pred p;\n\t"
        "addc.u32 %0, 0, 0;\n\t"
        "}" : "=r"(carry));
    *carry_out = carry;
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

// Note: Division is complex and rarely used on GPU
// For now, provide stub that returns ERR
// Full Knuth's Algorithm D can be ported if needed
DEVICE_HOST aria_fix256_t aria_fix256_div_gpu(aria_fix256_t a, aria_fix256_t b) {
    // TODO: Port Knuth's Algorithm D to GPU
    // For Phase 5, division on GPU is low priority (physics uses mostly add/mul)
    return make_fix256_err_gpu();
}

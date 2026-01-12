/**
 * ARIA-024: Limb-Based Integral Model (LBIM) Runtime Intrinsics
 *
 * Implements division and modulo for large integers using binary long division.
 * This is a straightforward shift-and-subtract algorithm suitable for fixed-width types.
 *
 * For v0.1.0, correctness is prioritized over performance.
 * Future versions may implement Knuth's Algorithm D for better performance.
 */

#include "runtime/lbim.h"
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════
// Helper Functions
// ═══════════════════════════════════════════════════════════════════════

namespace {

// Check if a limb array is zero
template<int N>
bool is_zero(const uint64_t* limbs) {
    for (int i = 0; i < N; ++i) {
        if (limbs[i] != 0) return false;
    }
    return true;
}

// Compare two limb arrays: returns -1 if a < b, 0 if a == b, 1 if a > b
template<int N>
int compare(const uint64_t* a, const uint64_t* b) {
    for (int i = N - 1; i >= 0; --i) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

// Subtract b from a (a -= b), returns borrow
template<int N>
uint64_t subtract(uint64_t* a, const uint64_t* b) {
    uint64_t borrow = 0;
    for (int i = 0; i < N; ++i) {
        uint64_t sub = b[i] + borrow;
        borrow = (sub < borrow) ? 1 : 0;  // overflow in b[i] + borrow
        borrow += (a[i] < sub) ? 1 : 0;   // underflow in a[i] - sub
        a[i] -= sub;
    }
    return borrow;
}

// Left shift by 1 bit, returns carry out
template<int N>
uint64_t shift_left_1(uint64_t* limbs) {
    uint64_t carry = 0;
    for (int i = 0; i < N; ++i) {
        uint64_t new_carry = limbs[i] >> 63;
        limbs[i] = (limbs[i] << 1) | carry;
        carry = new_carry;
    }
    return carry;
}

// Get the bit at position 'bit' (0 = LSB)
template<int N>
bool get_bit(const uint64_t* limbs, int bit) {
    int limb_idx = bit / 64;
    int bit_idx = bit % 64;
    if (limb_idx >= N) return false;
    return (limbs[limb_idx] >> bit_idx) & 1;
}

// Set bit 0 of the result
template<int N>
void set_bit_0(uint64_t* limbs) {
    limbs[0] |= 1;
}

// Count leading zeros (from MSB)
template<int N>
int count_leading_zeros(const uint64_t* limbs) {
    for (int i = N - 1; i >= 0; --i) {
        if (limbs[i] != 0) {
            int clz = __builtin_clzll(limbs[i]);
            return (N - 1 - i) * 64 + clz;
        }
    }
    return N * 64;  // All zeros
}

// Binary long division: quotient = dividend / divisor, remainder stored in dividend
template<int N>
void div_mod(uint64_t* quotient, uint64_t* remainder, const uint64_t* dividend, const uint64_t* divisor) {
    // Initialize
    memset(quotient, 0, N * sizeof(uint64_t));
    memcpy(remainder, dividend, N * sizeof(uint64_t));

    // Check for division by zero - return zero (caller handles TBB ERR)
    if (is_zero<N>(divisor)) {
        memset(remainder, 0, N * sizeof(uint64_t));
        return;
    }

    // If dividend < divisor, quotient = 0, remainder = dividend
    if (compare<N>(dividend, divisor) < 0) {
        return;
    }

    // Find the position of the highest set bit in dividend
    int dividend_bits = N * 64 - count_leading_zeros<N>(dividend);
    int divisor_bits = N * 64 - count_leading_zeros<N>(divisor);

    // Shift divisor to align with dividend
    uint64_t shifted_divisor[N];
    memcpy(shifted_divisor, divisor, N * sizeof(uint64_t));

    // Left-shift divisor to align MSBs
    int shift_amount = dividend_bits - divisor_bits;
    for (int s = 0; s < shift_amount; ++s) {
        shift_left_1<N>(shifted_divisor);
    }

    // Binary long division
    for (int i = shift_amount; i >= 0; --i) {
        // Shift quotient left by 1
        shift_left_1<N>(quotient);

        // If remainder >= shifted_divisor, subtract and set quotient bit
        if (compare<N>(remainder, shifted_divisor) >= 0) {
            subtract<N>(remainder, shifted_divisor);
            set_bit_0<N>(quotient);
        }

        // Shift divisor right by 1 (by shifting our view)
        if (i > 0) {
            // Shift shifted_divisor right by 1
            uint64_t carry = 0;
            for (int j = N - 1; j >= 0; --j) {
                uint64_t new_carry = shifted_divisor[j] & 1;
                shifted_divisor[j] = (shifted_divisor[j] >> 1) | (carry << 63);
                carry = new_carry;
            }
        }
    }
}

// Negate a limb array (two's complement)
template<int N>
void negate(uint64_t* limbs) {
    // Invert all bits
    for (int i = 0; i < N; ++i) {
        limbs[i] = ~limbs[i];
    }
    // Add 1
    uint64_t carry = 1;
    for (int i = 0; i < N; ++i) {
        uint64_t sum = limbs[i] + carry;
        carry = (sum < limbs[i]) ? 1 : 0;
        limbs[i] = sum;
    }
}

// Check if negative (high bit set)
template<int N>
bool is_negative(const uint64_t* limbs) {
    return (limbs[N-1] >> 63) & 1;
}

// Check if ERR sentinel (minimum signed value: 0x8000...0000)
template<int N>
bool is_err_sentinel(const uint64_t* limbs) {
    // ERR is: high limb = 0x8000000000000000, all other limbs = 0
    if (limbs[N-1] != 0x8000000000000000ULL) {
        return false;
    }
    for (int i = 0; i < N-1; ++i) {
        if (limbs[i] != 0) {
            return false;
        }
    }
    return true;
}

// Set ERR sentinel
template<int N>
void set_err_sentinel(uint64_t* limbs) {
    memset(limbs, 0, N * sizeof(uint64_t));
    limbs[N-1] = 0x8000000000000000ULL;
}

// Signed division using unsigned division
template<int N>
void sdiv_mod(uint64_t* quotient, uint64_t* remainder, const uint64_t* dividend, const uint64_t* divisor) {
    uint64_t abs_dividend[N];
    uint64_t abs_divisor[N];
    memcpy(abs_dividend, dividend, N * sizeof(uint64_t));
    memcpy(abs_divisor, divisor, N * sizeof(uint64_t));

    bool dividend_neg = is_negative<N>(dividend);
    bool divisor_neg = is_negative<N>(divisor);

    if (dividend_neg) negate<N>(abs_dividend);
    if (divisor_neg) negate<N>(abs_divisor);

    div_mod<N>(quotient, remainder, abs_dividend, abs_divisor);

    // Quotient is negative if signs differ
    if (dividend_neg != divisor_neg) {
        negate<N>(quotient);
    }

    // Remainder has same sign as dividend
    if (dividend_neg) {
        negate<N>(remainder);
    }
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════
// int128 Division and Modulo
// ═══════════════════════════════════════════════════════════════════════

extern "C" aria_int128_t aria_lbim_udiv128(aria_int128_t dividend, aria_int128_t divisor) {
    aria_int128_t quotient, remainder;
    div_mod<2>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return quotient;
}

extern "C" aria_int128_t aria_lbim_umod128(aria_int128_t dividend, aria_int128_t divisor) {
    aria_int128_t quotient, remainder;
    div_mod<2>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return remainder;
}

extern "C" aria_int128_t aria_lbim_sdiv128(aria_int128_t dividend, aria_int128_t divisor) {
    aria_int128_t quotient, remainder;
    sdiv_mod<2>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return quotient;
}

extern "C" aria_int128_t aria_lbim_smod128(aria_int128_t dividend, aria_int128_t divisor) {
    aria_int128_t quotient, remainder;
    sdiv_mod<2>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return remainder;
}

// ═══════════════════════════════════════════════════════════════════════
// int256 Division and Modulo
// ═══════════════════════════════════════════════════════════════════════

extern "C" aria_int256_t aria_lbim_udiv256(aria_int256_t dividend, aria_int256_t divisor) {
    aria_int256_t quotient, remainder;
    div_mod<4>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return quotient;
}

extern "C" aria_int256_t aria_lbim_umod256(aria_int256_t dividend, aria_int256_t divisor) {
    aria_int256_t quotient, remainder;
    div_mod<4>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return remainder;
}

extern "C" aria_int256_t aria_lbim_sdiv256(aria_int256_t dividend, aria_int256_t divisor) {
    aria_int256_t quotient, remainder;
    sdiv_mod<4>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return quotient;
}

extern "C" aria_int256_t aria_lbim_smod256(aria_int256_t dividend, aria_int256_t divisor) {
    aria_int256_t quotient, remainder;
    sdiv_mod<4>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return remainder;
}

// ═══════════════════════════════════════════════════════════════════════
// int512 Division and Modulo
// ═══════════════════════════════════════════════════════════════════════

extern "C" aria_int512_t aria_lbim_udiv512(aria_int512_t dividend, aria_int512_t divisor) {
    aria_int512_t quotient, remainder;
    div_mod<8>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return quotient;
}

extern "C" aria_int512_t aria_lbim_umod512(aria_int512_t dividend, aria_int512_t divisor) {
    aria_int512_t quotient, remainder;
    div_mod<8>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return remainder;
}

extern "C" aria_int512_t aria_lbim_sdiv512(aria_int512_t dividend, aria_int512_t divisor) {
    aria_int512_t quotient, remainder;
    sdiv_mod<8>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return quotient;
}

extern "C" aria_int512_t aria_lbim_smod512(aria_int512_t dividend, aria_int512_t divisor) {
    aria_int512_t quotient, remainder;
    sdiv_mod<8>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return remainder;
}

// ═══════════════════════════════════════════════════════════════════════
// int1024 Division and Modulo (ARIA-025: Post-quantum cryptography)
// RSA-4096, lattice-based crypto, and quantum-resistant key derivation
// ═══════════════════════════════════════════════════════════════════════

extern "C" aria_int1024_t aria_lbim_udiv1024(aria_int1024_t dividend, aria_int1024_t divisor) {
    aria_int1024_t quotient, remainder;
    
    // TBB safety: check for ERR in inputs
    if (is_err_sentinel<16>(dividend.limbs) || is_err_sentinel<16>(divisor.limbs)) {
        set_err_sentinel<16>(quotient.limbs);
        return quotient;
    }
    
    // Division by zero returns ERR
    if (is_zero<16>(divisor.limbs)) {
        set_err_sentinel<16>(quotient.limbs);
        return quotient;
    }
    
    div_mod<16>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return quotient;
}

extern "C" aria_int1024_t aria_lbim_umod1024(aria_int1024_t dividend, aria_int1024_t divisor) {
    aria_int1024_t quotient, remainder;
    
    // TBB safety: check for ERR in inputs
    if (is_err_sentinel<16>(dividend.limbs) || is_err_sentinel<16>(divisor.limbs)) {
        set_err_sentinel<16>(remainder.limbs);
        return remainder;
    }
    
    // Division by zero returns ERR
    if (is_zero<16>(divisor.limbs)) {
        set_err_sentinel<16>(remainder.limbs);
        return remainder;
    }
    
    div_mod<16>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return remainder;
}

extern "C" aria_int1024_t aria_lbim_sdiv1024(aria_int1024_t dividend, aria_int1024_t divisor) {
    aria_int1024_t quotient, remainder;
    
    // TBB safety: check for ERR in inputs
    if (is_err_sentinel<16>(dividend.limbs) || is_err_sentinel<16>(divisor.limbs)) {
        set_err_sentinel<16>(quotient.limbs);
        return quotient;
    }
    
    // Division by zero returns ERR
    if (is_zero<16>(divisor.limbs)) {
        set_err_sentinel<16>(quotient.limbs);
        return quotient;
    }
    
    sdiv_mod<16>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return quotient;
}

extern "C" aria_int1024_t aria_lbim_smod1024(aria_int1024_t dividend, aria_int1024_t divisor) {
    aria_int1024_t quotient, remainder;
    
    // TBB safety: check for ERR in inputs
    if (is_err_sentinel<16>(dividend.limbs) || is_err_sentinel<16>(divisor.limbs)) {
        set_err_sentinel<16>(remainder.limbs);
        return remainder;
    }
    
    // Division by zero returns ERR
    if (is_zero<16>(divisor.limbs)) {
        set_err_sentinel<16>(remainder.limbs);
        return remainder;
    }
    
    sdiv_mod<16>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return remainder;
}

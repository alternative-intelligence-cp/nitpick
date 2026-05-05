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

// Logical left shift by arbitrary amount
template<int N>
void shift_left(uint64_t* result, const uint64_t* a, uint32_t shift) {
    memset(result, 0, N * sizeof(uint64_t));
    if (shift >= (uint32_t)(N * 64)) return;
    uint32_t limb_shift = shift / 64;
    uint32_t bit_shift = shift % 64;
    if (bit_shift == 0) {
        for (int i = N - 1; i >= (int)limb_shift; --i)
            result[i] = a[i - limb_shift];
    } else {
        for (int i = N - 1; i >= (int)limb_shift; --i) {
            result[i] = a[i - limb_shift] << bit_shift;
            if (i > (int)limb_shift)
                result[i] |= a[i - limb_shift - 1] >> (64 - bit_shift);
        }
    }
}

// Logical right shift by arbitrary amount
template<int N>
void shift_right_logical(uint64_t* result, const uint64_t* a, uint32_t shift) {
    memset(result, 0, N * sizeof(uint64_t));
    if (shift >= (uint32_t)(N * 64)) return;
    uint32_t limb_shift = shift / 64;
    uint32_t bit_shift = shift % 64;
    if (bit_shift == 0) {
        for (int i = 0; i < N - (int)limb_shift; ++i)
            result[i] = a[i + limb_shift];
    } else {
        for (int i = 0; i < N - (int)limb_shift; ++i) {
            result[i] = a[i + limb_shift] >> bit_shift;
            if (i + (int)limb_shift + 1 < N)
                result[i] |= a[i + limb_shift + 1] << (64 - bit_shift);
        }
    }
}

// Arithmetic right shift by arbitrary amount (sign-extends)
template<int N>
void shift_right_arithmetic(uint64_t* result, const uint64_t* a, uint32_t shift) {
    bool neg = is_negative<N>(a);
    if (shift >= (uint32_t)(N * 64)) {
        memset(result, neg ? 0xFF : 0, N * sizeof(uint64_t));
        return;
    }
    shift_right_logical<N>(result, a, shift);
    if (neg) {
        uint32_t limb_shift = shift / 64;
        uint32_t bit_shift = shift % 64;
        if (bit_shift != 0 && (int)limb_shift < N) {
            uint64_t mask = ~((1ULL << (64 - bit_shift)) - 1);
            result[N - limb_shift - 1] |= mask;
        }
        for (int i = N - (int)limb_shift; i < N; ++i)
            result[i] = ~0ULL;
    }
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

extern "C" npk_int128_t npk_lbim_udiv128(npk_int128_t dividend, npk_int128_t divisor) {
    npk_int128_t quotient, remainder;
    div_mod<2>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return quotient;
}

extern "C" npk_int128_t npk_lbim_umod128(npk_int128_t dividend, npk_int128_t divisor) {
    npk_int128_t quotient, remainder;
    div_mod<2>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return remainder;
}

extern "C" npk_int128_t npk_lbim_sdiv128(npk_int128_t dividend, npk_int128_t divisor) {
    npk_int128_t quotient, remainder;
    sdiv_mod<2>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return quotient;
}

extern "C" npk_int128_t npk_lbim_smod128(npk_int128_t dividend, npk_int128_t divisor) {
    npk_int128_t quotient, remainder;
    sdiv_mod<2>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return remainder;
}

// ═══════════════════════════════════════════════════════════════════════
// int256 Division and Modulo
// ═══════════════════════════════════════════════════════════════════════

extern "C" npk_int256_t npk_lbim_udiv256(npk_int256_t dividend, npk_int256_t divisor) {
    npk_int256_t quotient, remainder;
    div_mod<4>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return quotient;
}

extern "C" npk_int256_t npk_lbim_umod256(npk_int256_t dividend, npk_int256_t divisor) {
    npk_int256_t quotient, remainder;
    div_mod<4>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return remainder;
}

extern "C" npk_int256_t npk_lbim_sdiv256(npk_int256_t dividend, npk_int256_t divisor) {
    npk_int256_t quotient, remainder;
    sdiv_mod<4>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return quotient;
}

extern "C" npk_int256_t npk_lbim_smod256(npk_int256_t dividend, npk_int256_t divisor) {
    npk_int256_t quotient, remainder;
    sdiv_mod<4>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return remainder;
}

// ═══════════════════════════════════════════════════════════════════════
// int512 Division and Modulo
// ═══════════════════════════════════════════════════════════════════════

extern "C" npk_int512_t npk_lbim_udiv512(npk_int512_t dividend, npk_int512_t divisor) {
    npk_int512_t quotient, remainder;
    div_mod<8>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return quotient;
}

extern "C" npk_int512_t npk_lbim_umod512(npk_int512_t dividend, npk_int512_t divisor) {
    npk_int512_t quotient, remainder;
    div_mod<8>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return remainder;
}

extern "C" npk_int512_t npk_lbim_sdiv512(npk_int512_t dividend, npk_int512_t divisor) {
    npk_int512_t quotient, remainder;
    sdiv_mod<8>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return quotient;
}

extern "C" npk_int512_t npk_lbim_smod512(npk_int512_t dividend, npk_int512_t divisor) {
    npk_int512_t quotient, remainder;
    sdiv_mod<8>(quotient.limbs, remainder.limbs, dividend.limbs, divisor.limbs);
    return remainder;
}

// ═══════════════════════════════════════════════════════════════════════
// int1024 Division and Modulo (ARIA-025: Post-quantum cryptography)
// RSA-4096, lattice-based crypto, and quantum-resistant key derivation
// ═══════════════════════════════════════════════════════════════════════

extern "C" npk_int1024_t npk_lbim_udiv1024(npk_int1024_t dividend, npk_int1024_t divisor) {
    npk_int1024_t quotient, remainder;
    
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

extern "C" npk_int1024_t npk_lbim_umod1024(npk_int1024_t dividend, npk_int1024_t divisor) {
    npk_int1024_t quotient, remainder;
    
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

extern "C" npk_int1024_t npk_lbim_sdiv1024(npk_int1024_t dividend, npk_int1024_t divisor) {
    npk_int1024_t quotient, remainder;
    
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

extern "C" npk_int1024_t npk_lbim_smod1024(npk_int1024_t dividend, npk_int1024_t divisor) {
    npk_int1024_t quotient, remainder;
    
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

// ═══════════════════════════════════════════════════════════════════════
// Exponentiation (int128/256/512/1024)
// Binary exponentiation (square-and-multiply). Exponent is unsigned int64.
// Returns ERR sentinel on overflow or if base is ERR.
// ═══════════════════════════════════════════════════════════════════════

namespace {

// Unsigned multiply with overflow detection for pow.
// Returns true if overflow occurred (result exceeds N limbs).
template<int N>
bool mul_overflow(uint64_t* result, const uint64_t* a, const uint64_t* b) {
    // Use 2N-limb product buffer to detect overflow
    uint64_t product[2 * N];
    memset(product, 0, 2 * N * sizeof(uint64_t));

    for (int i = 0; i < N; ++i) {
        if (a[i] == 0) continue;
        uint64_t carry = 0;
        for (int j = 0; j < N; ++j) {
            // product[i+j] += a[i] * b[j] + carry
            __uint128_t wide = (__uint128_t)a[i] * b[j] + product[i + j] + carry;
            product[i + j] = (uint64_t)wide;
            carry = (uint64_t)(wide >> 64);
        }
        product[i + N] += carry;
    }

    // Check if any high limbs are set (overflow)
    for (int i = N; i < 2 * N; ++i) {
        if (product[i] != 0) return true;
    }
    // Also check sign bit for signed interpretation
    if (product[N - 1] >> 63) return true;

    memcpy(result, product, N * sizeof(uint64_t));
    return false;
}

// Binary exponentiation: base^exp where exp is a uint64_t
// Sets ERR sentinel on overflow.
template<int N>
void pow_impl(uint64_t* result, const uint64_t* base, uint64_t exp) {
    // result = 1
    memset(result, 0, N * sizeof(uint64_t));
    result[0] = 1;

    // Handle trivial cases
    if (exp == 0) return;  // x^0 = 1

    if (is_zero<N>(base)) {
        memset(result, 0, N * sizeof(uint64_t));  // 0^n = 0 for n>0
        return;
    }

    // Square-and-multiply
    uint64_t temp_base[N];
    memcpy(temp_base, base, N * sizeof(uint64_t));

    while (exp > 0) {
        if (exp & 1) {
            if (mul_overflow<N>(result, result, temp_base)) {
                set_err_sentinel<N>(result);
                return;
            }
        }
        exp >>= 1;
        if (exp > 0) {
            uint64_t sq[N];
            if (mul_overflow<N>(sq, temp_base, temp_base)) {
                // If squaring overflows and we still have bits, result will too
                if (exp & 1) {
                    set_err_sentinel<N>(result);
                    return;
                }
                // Even if this intermediate overflows, it might not matter
                // if remaining exp bits are 0. But conservatively ERR.
                set_err_sentinel<N>(result);
                return;
            }
            memcpy(temp_base, sq, N * sizeof(uint64_t));
        }
    }
}

} // anonymous namespace (pow helpers)

extern "C" npk_int128_t npk_lbim_pow128(npk_int128_t base, uint64_t exp) {
    npk_int128_t result;
    if (is_err_sentinel<2>(base.limbs)) {
        set_err_sentinel<2>(result.limbs);
        return result;
    }
    // Handle negative base: result is negative if base is negative and exp is odd
    bool neg = is_negative<2>(base.limbs);
    uint64_t abs_base[2];
    memcpy(abs_base, base.limbs, sizeof(abs_base));
    if (neg) negate<2>(abs_base);
    pow_impl<2>(result.limbs, abs_base, exp);
    if (!is_err_sentinel<2>(result.limbs) && neg && (exp & 1)) {
        negate<2>(result.limbs);
    }
    return result;
}

extern "C" npk_int256_t npk_lbim_pow256(npk_int256_t base, uint64_t exp) {
    npk_int256_t result;
    if (is_err_sentinel<4>(base.limbs)) {
        set_err_sentinel<4>(result.limbs);
        return result;
    }
    bool neg = is_negative<4>(base.limbs);
    uint64_t abs_base[4];
    memcpy(abs_base, base.limbs, sizeof(abs_base));
    if (neg) negate<4>(abs_base);
    pow_impl<4>(result.limbs, abs_base, exp);
    if (!is_err_sentinel<4>(result.limbs) && neg && (exp & 1)) {
        negate<4>(result.limbs);
    }
    return result;
}

extern "C" npk_int512_t npk_lbim_pow512(npk_int512_t base, uint64_t exp) {
    npk_int512_t result;
    if (is_err_sentinel<8>(base.limbs)) {
        set_err_sentinel<8>(result.limbs);
        return result;
    }
    bool neg = is_negative<8>(base.limbs);
    uint64_t abs_base[8];
    memcpy(abs_base, base.limbs, sizeof(abs_base));
    if (neg) negate<8>(abs_base);
    pow_impl<8>(result.limbs, abs_base, exp);
    if (!is_err_sentinel<8>(result.limbs) && neg && (exp & 1)) {
        negate<8>(result.limbs);
    }
    return result;
}

extern "C" npk_int1024_t npk_lbim_pow1024(npk_int1024_t base, uint64_t exp) {
    npk_int1024_t result;
    if (is_err_sentinel<16>(base.limbs)) {
        set_err_sentinel<16>(result.limbs);
        return result;
    }
    bool neg = is_negative<16>(base.limbs);
    uint64_t abs_base[16];
    memcpy(abs_base, base.limbs, sizeof(abs_base));
    if (neg) negate<16>(abs_base);
    pow_impl<16>(result.limbs, abs_base, exp);
    if (!is_err_sentinel<16>(result.limbs) && neg && (exp & 1)) {
        negate<16>(result.limbs);
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════════════
// Bitwise Operations (int128/256/512/1024)
// These are sign-independent: the same function handles both the signed
// (int*) and unsigned (uint*) variants for each width.
// ═══════════════════════════════════════════════════════════════════════

// ----- int128 -----

extern "C" npk_int128_t npk_lbim_and128(npk_int128_t a, npk_int128_t b) {
    npk_int128_t r;
    for (int i = 0; i < 2; ++i) r.limbs[i] = a.limbs[i] & b.limbs[i];
    return r;
}

extern "C" npk_int128_t npk_lbim_or128(npk_int128_t a, npk_int128_t b) {
    npk_int128_t r;
    for (int i = 0; i < 2; ++i) r.limbs[i] = a.limbs[i] | b.limbs[i];
    return r;
}

extern "C" npk_int128_t npk_lbim_xor128(npk_int128_t a, npk_int128_t b) {
    npk_int128_t r;
    for (int i = 0; i < 2; ++i) r.limbs[i] = a.limbs[i] ^ b.limbs[i];
    return r;
}

extern "C" npk_int128_t npk_lbim_not128(npk_int128_t a) {
    npk_int128_t r;
    for (int i = 0; i < 2; ++i) r.limbs[i] = ~a.limbs[i];
    return r;
}

// ----- int256 -----

extern "C" npk_int256_t npk_lbim_and256(npk_int256_t a, npk_int256_t b) {
    npk_int256_t r;
    for (int i = 0; i < 4; ++i) r.limbs[i] = a.limbs[i] & b.limbs[i];
    return r;
}

extern "C" npk_int256_t npk_lbim_or256(npk_int256_t a, npk_int256_t b) {
    npk_int256_t r;
    for (int i = 0; i < 4; ++i) r.limbs[i] = a.limbs[i] | b.limbs[i];
    return r;
}

extern "C" npk_int256_t npk_lbim_xor256(npk_int256_t a, npk_int256_t b) {
    npk_int256_t r;
    for (int i = 0; i < 4; ++i) r.limbs[i] = a.limbs[i] ^ b.limbs[i];
    return r;
}

extern "C" npk_int256_t npk_lbim_not256(npk_int256_t a) {
    npk_int256_t r;
    for (int i = 0; i < 4; ++i) r.limbs[i] = ~a.limbs[i];
    return r;
}

// ----- int512 -----

extern "C" npk_int512_t npk_lbim_and512(npk_int512_t a, npk_int512_t b) {
    npk_int512_t r;
    for (int i = 0; i < 8; ++i) r.limbs[i] = a.limbs[i] & b.limbs[i];
    return r;
}

extern "C" npk_int512_t npk_lbim_or512(npk_int512_t a, npk_int512_t b) {
    npk_int512_t r;
    for (int i = 0; i < 8; ++i) r.limbs[i] = a.limbs[i] | b.limbs[i];
    return r;
}

extern "C" npk_int512_t npk_lbim_xor512(npk_int512_t a, npk_int512_t b) {
    npk_int512_t r;
    for (int i = 0; i < 8; ++i) r.limbs[i] = a.limbs[i] ^ b.limbs[i];
    return r;
}

extern "C" npk_int512_t npk_lbim_not512(npk_int512_t a) {
    npk_int512_t r;
    for (int i = 0; i < 8; ++i) r.limbs[i] = ~a.limbs[i];
    return r;
}

// ----- int1024 -----

extern "C" npk_int1024_t npk_lbim_and1024(npk_int1024_t a, npk_int1024_t b) {
    npk_int1024_t r;
    for (int i = 0; i < 16; ++i) r.limbs[i] = a.limbs[i] & b.limbs[i];
    return r;
}

extern "C" npk_int1024_t npk_lbim_or1024(npk_int1024_t a, npk_int1024_t b) {
    npk_int1024_t r;
    for (int i = 0; i < 16; ++i) r.limbs[i] = a.limbs[i] | b.limbs[i];
    return r;
}

extern "C" npk_int1024_t npk_lbim_xor1024(npk_int1024_t a, npk_int1024_t b) {
    npk_int1024_t r;
    for (int i = 0; i < 16; ++i) r.limbs[i] = a.limbs[i] ^ b.limbs[i];
    return r;
}

extern "C" npk_int1024_t npk_lbim_not1024(npk_int1024_t a) {
    npk_int1024_t r;
    for (int i = 0; i < 16; ++i) r.limbs[i] = ~a.limbs[i];
    return r;
}

// ═══════════════════════════════════════════════════════════════════════
// Shift Operations (int128/256/512/1024)
// Left shift (shl), logical right shift (lshr), arithmetic right shift (ashr)
// ERR sentinel is preserved through shifts.
// ═══════════════════════════════════════════════════════════════════════

#define LBIM_SHIFT_IMPL(BITS, LIMBS) \
extern "C" npk_int##BITS##_t npk_lbim_shl##BITS(npk_int##BITS##_t a, uint32_t s) { \
    if (is_err_sentinel<LIMBS>(a.limbs)) return a; \
    npk_int##BITS##_t r; shift_left<LIMBS>(r.limbs, a.limbs, s); return r; \
} \
extern "C" npk_int##BITS##_t npk_lbim_lshr##BITS(npk_int##BITS##_t a, uint32_t s) { \
    if (is_err_sentinel<LIMBS>(a.limbs)) return a; \
    npk_int##BITS##_t r; shift_right_logical<LIMBS>(r.limbs, a.limbs, s); return r; \
} \
extern "C" npk_int##BITS##_t npk_lbim_ashr##BITS(npk_int##BITS##_t a, uint32_t s) { \
    if (is_err_sentinel<LIMBS>(a.limbs)) return a; \
    npk_int##BITS##_t r; shift_right_arithmetic<LIMBS>(r.limbs, a.limbs, s); return r; \
}

LBIM_SHIFT_IMPL(128,  2)
LBIM_SHIFT_IMPL(256,  4)
LBIM_SHIFT_IMPL(512,  8)
LBIM_SHIFT_IMPL(1024, 16)

#undef LBIM_SHIFT_IMPL

#include "runtime/lbim_extended.h"
#include "runtime/fmt/formatters.h"
#include "runtime/stdlib.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <string>

// Constants used for limb iteration
static const int N = ARIA_INT2048_LIMBS;

// ----------------------------------------------------------------------
// Helper: Check for Zero
// ----------------------------------------------------------------------
static bool is_zero(const aria_int2048_t* val) {
    for (int i = 0; i < N; ++i) {
        if (val->limbs[i] != 0) return false;
    }
    return true;
}

// ----------------------------------------------------------------------
// Helper: Is Negative (Check sign bit of MSB)
// ----------------------------------------------------------------------
static bool is_negative(const aria_int2048_t* val) {
    return (val->limbs[N - 1] & (1ULL << 63)) != 0;
}

// ----------------------------------------------------------------------
// Helper: ERR Sentinel Detection and Creation
// ----------------------------------------------------------------------
// Check if ERR sentinel (minimum signed value: 0x8000...0000)
template<int Limbs>
static bool is_err_sentinel(const uint64_t* limbs) {
    // ERR is: high limb = 0x8000000000000000, all other limbs = 0
    if (limbs[Limbs-1] != 0x8000000000000000ULL) {
        return false;
    }
    for (int i = 0; i < Limbs-1; ++i) {
        if (limbs[i] != 0) {
            return false;
        }
    }
    return true;
}

// Set ERR sentinel
template<int Limbs>
static void set_err_sentinel(uint64_t* limbs) {
    memset(limbs, 0, Limbs * sizeof(uint64_t));
    limbs[Limbs-1] = 0x8000000000000000ULL;
}

// ----------------------------------------------------------------------
// Helper: Two's Complement Negation
// ----------------------------------------------------------------------
static aria_int2048_t negate(aria_int2048_t a) {
    aria_int2048_t result;
    uint64_t carry = 1;
    for (int i = 0; i < N; ++i) {
        uint64_t val = ~a.limbs[i];
        unsigned __int128 sum = (unsigned __int128)val + carry;
        result.limbs[i] = (uint64_t)sum;
        carry = (uint64_t)(sum >> 64);
    }
    return result;
}

// ----------------------------------------------------------------------
// Addition (Ripple Carry)
// ----------------------------------------------------------------------
extern "C" aria_int2048_t aria_lbim_add2048(const aria_int2048_t* a, const aria_int2048_t* b) {
    // ARIA SAFETY: Propagate ERR sentinel (fail-hard policy)
    if (is_err_sentinel<32>(a->limbs) || is_err_sentinel<32>(b->limbs)) {
        aria_int2048_t err;
        set_err_sentinel<32>(err.limbs);
        return err;
    }
    
    aria_int2048_t result;
    unsigned __int128 carry = 0;
    
    // Explicit loop for safety verification.
    // Compilers will auto-vectorize this if safe to do so.
    for (int i = 0; i < N; ++i) {
        unsigned __int128 sum = (unsigned __int128)a->limbs[i] + b->limbs[i] + carry;
        result.limbs[i] = (uint64_t)sum;
        carry = sum >> 64;
    }
    // Final carry is discarded in modular arithmetic.
    return result;
}

// ----------------------------------------------------------------------
// Subtraction (Ripple Borrow)
// ----------------------------------------------------------------------
extern "C" aria_int2048_t aria_lbim_sub2048(const aria_int2048_t* a, const aria_int2048_t* b) {
    // ARIA SAFETY: Propagate ERR sentinel (fail-hard policy)
    if (is_err_sentinel<32>(a->limbs) || is_err_sentinel<32>(b->limbs)) {
        aria_int2048_t err;
        set_err_sentinel<32>(err.limbs);
        return err;
    }
    
    aria_int2048_t result;
    uint64_t borrow = 0;
    
    for (int i = 0; i < N; ++i) {
        // Check if we need to borrow
        uint64_t temp_a = a->limbs[i];
        uint64_t temp_b = b->limbs[i] + borrow;
        
        // Detect if adding borrow caused overflow in b
        if (borrow && temp_b < b->limbs[i]) {
            // Overflow in b + borrow, will definitely need borrow
            result.limbs[i] = temp_a - temp_b;
            borrow = 1;
        } else if (temp_a < temp_b) {
            // Need to borrow
            result.limbs[i] = temp_a - temp_b;  // Wraps around
            borrow = 1;
        } else {
            // No borrow needed
            result.limbs[i] = temp_a - temp_b;
            borrow = 0;
        }
    }
    return result;
}

// ----------------------------------------------------------------------
// Multiplication (Schoolbook O(N^2))
// ----------------------------------------------------------------------
extern "C" aria_int2048_t aria_lbim_mul2048(const aria_int2048_t* a, const aria_int2048_t* b) {
    // ARIA SAFETY: Propagate ERR sentinel (fail-hard policy)
    if (is_err_sentinel<32>(a->limbs) || is_err_sentinel<32>(b->limbs)) {
        aria_int2048_t err;
        set_err_sentinel<32>(err.limbs);
        return err;
    }
    
    aria_int2048_t result;
    std::memset(&result, 0, sizeof(result));
    
    for (int i = 0; i < N; ++i) {
        uint64_t carry = 0;
        
        // Inner loop: multiply limb a[i] by entire b
        for (int j = 0; j < N - i; ++j) {
            // Calculation: result[i+j] += a[i] * b[j] + carry
            unsigned __int128 product = (unsigned __int128)a->limbs[i] * b->limbs[j];
            unsigned __int128 sum = (unsigned __int128)result.limbs[i+j] + product + carry;
            
            result.limbs[i+j] = (uint64_t)sum;
            carry = (uint64_t)(sum >> 64);
        }
        // Note: Any carry out of the top limb is discarded (modular arithmetic).
    }
    return result;
}

// ----------------------------------------------------------------------
// Comparison Logic
// ----------------------------------------------------------------------

extern "C" int32_t aria_lbim_ucmp2048(const aria_int2048_t* a, const aria_int2048_t* b) {
    for (int i = N - 1; i >= 0; --i) {
        if (a->limbs[i] > b->limbs[i]) return 1;
        if (a->limbs[i] < b->limbs[i]) return -1;
    }
    return 0;
}

extern "C" int32_t aria_lbim_scmp2048(const aria_int2048_t* a, const aria_int2048_t* b) {
    // Check signs
    bool neg_a = is_negative(a);
    bool neg_b = is_negative(b);
    
    if (neg_a != neg_b) {
        return neg_a ? -1 : 1; // Negative < Positive
    }
    
    // Same sign: compare magnitudes
    // For both positive: standard unsigned comparison
    // For both negative: unsigned comparison still works correctly
    // because more negative numbers have smaller bit patterns in two's complement
    return aria_lbim_ucmp2048(a, b);
}

extern "C" bool aria_lbim_eq2048(const aria_int2048_t* a, const aria_int2048_t* b) {
    // Use memcmp for potentially faster vectorization by the compiler
    return std::memcmp(a, b, sizeof(aria_int2048_t)) == 0;
}

// ----------------------------------------------------------------------
// Division Helpers
// ----------------------------------------------------------------------

// Helper: Count Leading Zeros across the 2048-bit struct
static int count_leading_zeros_2048(const aria_int2048_t* val) {
    for (int i = N - 1; i >= 0; --i) {
        if (val->limbs[i] != 0) {
            // __builtin_clzll is undefined for 0, so we check.
            // Returns number of leading zeros in the 64-bit limb.
            return __builtin_clzll(val->limbs[i]) + (N - 1 - i) * 64;
        }
    }
    return 2048; // All zero
}

// Internal Unsigned Division / Modulo
static void divmod2048_unsigned(aria_int2048_t dividend, aria_int2048_t divisor, 
                                aria_int2048_t* quot, aria_int2048_t* rem) {
    // 1. Handle Division by Zero
    if (is_zero(&divisor)) {
        // ARIA SAFETY: Division by zero returns ERR sentinel (fail-hard policy)
        // Physical consequence: Prevents silent zero (robot thinks "no load" → actuator relaxes → child falls)
        if (quot) set_err_sentinel<32>(quot->limbs);
        if (rem) set_err_sentinel<32>(rem->limbs);
        return;
    }

    // 2. Handle Divisor > Dividend
    if (aria_lbim_ucmp2048(&dividend, &divisor) < 0) {
        if (quot) std::memset(quot, 0, sizeof(*quot));
        if (rem) *rem = dividend;
        return;
    }

    // 3. Bit-wise Long Division
    // This is O(bits^2) worst case but very simple to verify.
    aria_int2048_t q; std::memset(&q, 0, sizeof(q));
    aria_int2048_t r; std::memset(&r, 0, sizeof(r));
    
    // Optimize: only scan relevant bits
    int dividend_bits = 2048 - count_leading_zeros_2048(&dividend);
    
    for (int i = dividend_bits - 1; i >= 0; --i) {
        // Shift r left by 1 (r = r << 1)
        uint64_t carry = 0;
        for (int k = 0; k < N; ++k) {
            uint64_t next_carry = r.limbs[k] >> 63;
            r.limbs[k] = (r.limbs[k] << 1) | carry;
            carry = next_carry;
        }
        
        // Inject the i-th bit of dividend into LSB of r
        // dividend bit: (dividend.limbs[i/64] >> (i%64)) & 1
        if ((dividend.limbs[i / 64] >> (i % 64)) & 1) {
            r.limbs[0] |= 1;
        }
        
        // If r >= divisor, subtract divisor and set bit in quotient
        if (aria_lbim_ucmp2048(&r, &divisor) >= 0) {
            r = aria_lbim_sub2048(&r, &divisor);
            // Set i-th bit of q
            q.limbs[i / 64] |= (1ULL << (i % 64));
        }
    }
    
    if (quot) *quot = q;
    if (rem) *rem = r;
}

extern "C" aria_int2048_t aria_lbim_udiv2048(const aria_int2048_t* dividend, const aria_int2048_t* divisor) {
    aria_int2048_t q;
    divmod2048_unsigned(*dividend, *divisor, &q, nullptr);
    return q;
}

extern "C" aria_int2048_t aria_lbim_umod2048(const aria_int2048_t* dividend, const aria_int2048_t* divisor) {
    aria_int2048_t r;
    divmod2048_unsigned(*dividend, *divisor, nullptr, &r);
    return r;
}

extern "C" aria_int2048_t aria_lbim_sdiv2048(const aria_int2048_t* dividend, const aria_int2048_t* divisor) {
    bool neg_d = is_negative(dividend);
    bool neg_v = is_negative(divisor);
    
    // Convert to absolute
    aria_int2048_t abs_d = neg_d ? negate(*dividend) : *dividend;
    aria_int2048_t abs_v = neg_v ? negate(*divisor) : *divisor;
    
    aria_int2048_t q = aria_lbim_udiv2048(&abs_d, &abs_v);
    
    // If signs differ, quotient is negative
    if (neg_d != neg_v) {
        q = negate(q);
    }
    return q;
}

extern "C" aria_int2048_t aria_lbim_smod2048(const aria_int2048_t* dividend, const aria_int2048_t* divisor) {
    bool neg_d = is_negative(dividend);
    bool neg_v = is_negative(divisor);
    
    aria_int2048_t abs_d = neg_d ? negate(*dividend) : *dividend;
    aria_int2048_t abs_v = neg_v ? negate(*divisor) : *divisor;
    
    aria_int2048_t r = aria_lbim_umod2048(&abs_d, &abs_v);
    
    // Remainder sign follows the dividend (standard C99 behavior)
    if (neg_d) {
        r = negate(r);
    }
    return r;
}

// ----------------------------------------------------------------------
// Bitwise Operations
// ----------------------------------------------------------------------

extern "C" aria_int2048_t aria_lbim_and2048(aria_int2048_t a, aria_int2048_t b) {
    aria_int2048_t result;
    for (int i = 0; i < N; ++i) {
        result.limbs[i] = a.limbs[i] & b.limbs[i];
    }
    return result;
}

extern "C" aria_int2048_t aria_lbim_or2048(aria_int2048_t a, aria_int2048_t b) {
    aria_int2048_t result;
    for (int i = 0; i < N; ++i) {
        result.limbs[i] = a.limbs[i] | b.limbs[i];
    }
    return result;
}

extern "C" aria_int2048_t aria_lbim_xor2048(aria_int2048_t a, aria_int2048_t b) {
    aria_int2048_t result;
    for (int i = 0; i < N; ++i) {
        result.limbs[i] = a.limbs[i] ^ b.limbs[i];
    }
    return result;
}

extern "C" aria_int2048_t aria_lbim_not2048(aria_int2048_t a) {
    aria_int2048_t result;
    for (int i = 0; i < N; ++i) {
        result.limbs[i] = ~a.limbs[i];
    }
    return result;
}

// ----------------------------------------------------------------------
// Shift Operations
// ----------------------------------------------------------------------

extern "C" aria_int2048_t aria_lbim_shl2048(aria_int2048_t a, uint32_t shift) {
    // ARIA SAFETY: Preserve ERR sentinel through shifts (fail-hard policy)
    if (is_err_sentinel<32>(a.limbs)) {
        return a;
    }
    
    aria_int2048_t result;
    std::memset(&result, 0, sizeof(result));
    
    if (shift >= 2048) {
        // Shift out everything
        return result;
    }
    
    uint32_t limb_shift = shift / 64;
    uint32_t bit_shift = shift % 64;
    
    if (bit_shift == 0) {
        // Simple limb shift
        for (int i = N - 1; i >= (int)limb_shift; --i) {
            result.limbs[i] = a.limbs[i - limb_shift];
        }
    } else {
        // Combined limb and bit shift
        for (int i = N - 1; i >= (int)limb_shift; --i) {
            result.limbs[i] = a.limbs[i - limb_shift] << bit_shift;
            if (i > (int)limb_shift) {
                result.limbs[i] |= a.limbs[i - limb_shift - 1] >> (64 - bit_shift);
            }
        }
    }
    
    return result;
}

extern "C" aria_int2048_t aria_lbim_lshr2048(aria_int2048_t a, uint32_t shift) {
    // ARIA SAFETY: Preserve ERR sentinel through shifts (fail-hard policy)
    if (is_err_sentinel<32>(a.limbs)) {
        return a;
    }
    
    aria_int2048_t result;
    std::memset(&result, 0, sizeof(result));
    
    if (shift >= 2048) {
        // Shift out everything
        return result;
    }
    
    uint32_t limb_shift = shift / 64;
    uint32_t bit_shift = shift % 64;
    
    if (bit_shift == 0) {
        // Simple limb shift
        for (int i = 0; i < N - (int)limb_shift; ++i) {
            result.limbs[i] = a.limbs[i + limb_shift];
        }
    } else {
        // Combined limb and bit shift
        for (int i = 0; i < N - (int)limb_shift; ++i) {
            result.limbs[i] = a.limbs[i + limb_shift] >> bit_shift;
            if (i + limb_shift + 1 < N) {
                result.limbs[i] |= a.limbs[i + limb_shift + 1] << (64 - bit_shift);
            }
        }
    }
    
    return result;
}

extern "C" aria_int2048_t aria_lbim_ashr2048(aria_int2048_t a, uint32_t shift) {
    // ARIA SAFETY: Preserve ERR sentinel through shifts (fail-hard policy)
    if (is_err_sentinel<32>(a.limbs)) {
        return a;
    }
    
    aria_int2048_t result;
    
    // Check if negative
    bool is_neg = is_negative(&a);
    
    if (shift >= 2048) {
        // Shift out everything, fill with sign bit
        if (is_neg) {
            std::memset(&result, 0xFF, sizeof(result));
        } else {
            std::memset(&result, 0, sizeof(result));
        }
        return result;
    }
    
    // Perform logical right shift
    result = aria_lbim_lshr2048(a, shift);
    
    // Sign extend if negative
    if (is_neg) {
        uint32_t limb_shift = shift / 64;
        uint32_t bit_shift = shift % 64;
        
        // Fill high bits with 1s
        if (bit_shift != 0 && limb_shift < N) {
            // Set high bits of the limb that was partially shifted
            uint64_t mask = ~((1ULL << (64 - bit_shift)) - 1);
            result.limbs[N - limb_shift - 1] |= mask;
        }
        
        // Fill all higher limbs with 1s
        for (int i = N - (int)limb_shift; i < N; ++i) {
            result.limbs[i] = ~0ULL;
        }
    }
    
    return result;
}
// ==============================================================================
// Formatters - Convert int2048/uint2048 to Decimal Strings
// ==============================================================================

/**
 * Allocate AriaString on heap with given C string content
 */
static AriaString* alloc_aria_string(const char* cstr) {
    if (!cstr) return nullptr;
    
    size_t len = strlen(cstr);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return nullptr;
    memcpy(buf, cstr, len);
    buf[len] = '\0';
    
    AriaString* str = (AriaString*)malloc(sizeof(AriaString));
    if (!str) {
        free(buf);
        return nullptr;
    }
    
    str->data = buf;
    str->length = (int64_t)len;
    return str;
}

/**
 * Convert uint2048 to decimal string (division by 10 repeatedly)
 */
extern "C" AriaString* aria_format_uint2048(const aria_int2048_t* val) {
    if (!val) return alloc_aria_string("(null)");
    
    // Check for zero
    bool is_zero_val = true;
    for (int i = 0; i < N; ++i) {
        if (val->limbs[i] != 0) {
            is_zero_val = false;
            break;
        }
    }
    if (is_zero_val) return alloc_aria_string("0");
    
    // Copy value for modification
    aria_int2048_t work = *val;
    
    // Build string in reverse
    std::string result;
    result.reserve(700);  // 2048 bits ≈ 617 decimal digits max
    
    aria_int2048_t ten;
    memset(&ten, 0, sizeof(ten));
    ten.limbs[0] = 10;
    
    // Repeatedly divide by 10, collecting remainders
    while (!is_zero(&work)) {
        aria_int2048_t quotient = aria_lbim_udiv2048(&work, &ten);
        aria_int2048_t remainder = aria_lbim_umod2048(&work, &ten);
        
        // Remainder is guaranteed to be < 10, so it fits in limbs[0]
        char digit = '0' + (char)(remainder.limbs[0] & 0xFF);
        result.push_back(digit);
        
        work = quotient;
    }
    
    // Reverse to get correct order
    std::reverse(result.begin(), result.end());
    
    return alloc_aria_string(result.c_str());
}

/**
 * Convert int2048 to decimal string (handle sign, then delegate to unsigned)
 */
extern "C" AriaString* aria_format_int2048(const aria_int2048_t* val) {
    if (!val) return alloc_aria_string("(null)");
    
    // Check for negative (MSB of top limb set)
    bool negative = (val->limbs[N - 1] & 0x8000000000000000ULL) != 0;
    
    if (negative) {
        // Negate and format as unsigned, then prepend '-'
        aria_int2048_t abs_val = negate(*val);
        AriaString* abs_str = aria_format_uint2048(&abs_val);
        if (!abs_str) return nullptr;
        
        // Prepend '-'
        size_t new_len = abs_str->length + 1;
        char* new_buf = (char*)malloc(new_len + 1);
        if (!new_buf) {
            free((void*)abs_str->data);  // Cast away const for free
            free(abs_str);
            return nullptr;
        }
        
        new_buf[0] = '-';
        memcpy(new_buf + 1, abs_str->data, abs_str->length);
        new_buf[new_len] = '\0';
        
        free((void*)abs_str->data);  // Cast away const for free
        abs_str->data = new_buf;
        abs_str->length = (int64_t)new_len;
        
        return abs_str;
    } else {
        // Positive - format as unsigned
        return aria_format_uint2048(val);
    }
}
// ══════════════════════════════════════════════════════════════════════════════
// 4096-BIT INTEGER ARITHMETIC (64 limbs)
// ══════════════════════════════════════════════════════════════════════════════

static const int N4096 = ARIA_INT4096_LIMBS;

// Helper: Check for Zero
static bool is_zero_4096(const aria_int4096_t* val) {
    for (int i = 0; i < N4096; ++i) {
        if (val->limbs[i] != 0) return false;
    }
    return true;
}

// Helper: Is Negative
static bool is_negative_4096(const aria_int4096_t* val) {
    return (val->limbs[N4096 - 1] & (1ULL << 63)) != 0;
}

// Helper: Two's Complement Negation
static aria_int4096_t negate_4096(aria_int4096_t a) {
    aria_int4096_t result;
    uint64_t carry = 1;
    for (int i = 0; i < N4096; ++i) {
        uint64_t val = ~a.limbs[i];
        unsigned __int128 sum = (unsigned __int128)val + carry;
        result.limbs[i] = (uint64_t)sum;
        carry = (uint64_t)(sum >> 64);
    }
    return result;
}

// Addition
extern "C" aria_int4096_t aria_lbim_add4096(const aria_int4096_t* a, const aria_int4096_t* b) {
    // ARIA SAFETY: Propagate ERR sentinel (fail-hard policy)
    if (is_err_sentinel<64>(a->limbs) || is_err_sentinel<64>(b->limbs)) {
        aria_int4096_t err;
        set_err_sentinel<64>(err.limbs);
        return err;
    }
    
    aria_int4096_t result;
    unsigned __int128 carry = 0;
    
    for (int i = 0; i < N4096; ++i) {
        unsigned __int128 sum = (unsigned __int128)a->limbs[i] + b->limbs[i] + carry;
        result.limbs[i] = (uint64_t)sum;
        carry = sum >> 64;
    }
    
    return result;
}

// Subtraction
extern "C" aria_int4096_t aria_lbim_sub4096(const aria_int4096_t* a, const aria_int4096_t* b) {
    // ARIA SAFETY: Propagate ERR sentinel (fail-hard policy)
    if (is_err_sentinel<64>(a->limbs) || is_err_sentinel<64>(b->limbs)) {
        aria_int4096_t err;
        set_err_sentinel<64>(err.limbs);
        return err;
    }
    
    aria_int4096_t result;
    unsigned __int128 borrow = 0;
    
    for (int i = 0; i < N4096; ++i) {
        unsigned __int128 diff = (unsigned __int128)a->limbs[i] - b->limbs[i] - borrow;
        result.limbs[i] = (uint64_t)diff;
        borrow = (diff >> 64) & 1;
    }
    
    return result;
}

// Multiplication
extern "C" aria_int4096_t aria_lbim_mul4096(const aria_int4096_t* a, const aria_int4096_t* b) {
    // ARIA SAFETY: Propagate ERR sentinel (fail-hard policy)
    if (is_err_sentinel<64>(a->limbs) || is_err_sentinel<64>(b->limbs)) {
        aria_int4096_t err;
        set_err_sentinel<64>(err.limbs);
        return err;
    }
    
    aria_int4096_t result;
    memset(&result, 0, sizeof(result));
    
    for (int i = 0; i < N4096; ++i) {
        unsigned __int128 carry = 0;
        for (int j = 0; j < N4096 - i; ++j) {
            unsigned __int128 prod = (unsigned __int128)a->limbs[i] * b->limbs[j];
            prod += result.limbs[i + j];
            prod += carry;
            result.limbs[i + j] = (uint64_t)prod;
            carry = prod >> 64;
        }
    }
    
    return result;
}

// Unsigned Division
extern "C" aria_int4096_t aria_lbim_udiv4096(const aria_int4096_t* dividend, const aria_int4096_t* divisor) {
    aria_int4096_t quotient;
    memset(&quotient, 0, sizeof(quotient));
    
    if (is_zero_4096(divisor)) {
        // ARIA SAFETY: Division by zero returns ERR sentinel (fail-hard policy)
        set_err_sentinel<64>(quotient.limbs);
        return quotient;
    }
    
    aria_int4096_t remainder = *dividend;
    aria_int4096_t current_divisor = *divisor;
    
    int shift = 0;
    while (!is_negative_4096(&current_divisor) && shift < 4096) {
        aria_int4096_t temp = aria_lbim_shl4096(current_divisor, 1);
        if (is_negative_4096(&temp)) break;
        current_divisor = temp;
        shift++;
    }
    
    for (int i = 0; i <= shift; ++i) {
        aria_int4096_t sub_result = aria_lbim_sub4096(&remainder, &current_divisor);
        if (!is_negative_4096(&sub_result)) {
            remainder = sub_result;
            aria_int4096_t bit_val;
            memset(&bit_val, 0, sizeof(bit_val));
            bit_val.limbs[0] = 1;
            bit_val = aria_lbim_shl4096(bit_val, shift - i);
            quotient = aria_lbim_add4096(&quotient, &bit_val);
        }
        current_divisor = aria_lbim_lshr4096(current_divisor, 1);
    }
    
    return quotient;
}

// Unsigned Remainder
extern "C" aria_int4096_t aria_lbim_urem4096(const aria_int4096_t* dividend, const aria_int4096_t* divisor) {
    if (is_zero_4096(divisor)) {
        // ARIA SAFETY: Division by zero returns ERR sentinel (fail-hard policy)
        aria_int4096_t err;
        set_err_sentinel<64>(err.limbs);
        return err;
    }
    
    aria_int4096_t quotient = aria_lbim_udiv4096(dividend, divisor);
    aria_int4096_t product = aria_lbim_mul4096(&quotient, divisor);
    return aria_lbim_sub4096(dividend, &product);
}

// Unsigned Modulo (same as remainder for unsigned)
extern "C" aria_int4096_t aria_lbim_umod4096(const aria_int4096_t* a, const aria_int4096_t* b) {
    return aria_lbim_urem4096(a, b);
}

// Signed Division
extern "C" aria_int4096_t aria_lbim_sdiv4096(const aria_int4096_t* dividend, const aria_int4096_t* divisor) {
    if (is_zero_4096(divisor)) {
        // ARIA SAFETY: Division by zero returns ERR sentinel (fail-hard policy)
        aria_int4096_t err;
        set_err_sentinel<64>(err.limbs);
        return err;
    }
    
    bool dividend_neg = is_negative_4096(dividend);
    bool divisor_neg = is_negative_4096(divisor);
    
    aria_int4096_t abs_dividend = dividend_neg ? negate_4096(*dividend) : *dividend;
    aria_int4096_t abs_divisor = divisor_neg ? negate_4096(*divisor) : *divisor;
    
    aria_int4096_t quotient = aria_lbim_udiv4096(&abs_dividend, &abs_divisor);
    
    if (dividend_neg != divisor_neg) {
        quotient = negate_4096(quotient);
    }
    
    return quotient;
}

// Signed Remainder
extern "C" aria_int4096_t aria_lbim_srem4096(const aria_int4096_t* dividend, const aria_int4096_t* divisor) {
    if (is_zero_4096(divisor)) {
        // ARIA SAFETY: Division by zero returns ERR sentinel (fail-hard policy)
        aria_int4096_t err;
        set_err_sentinel<64>(err.limbs);
        return err;
    }
    
    bool dividend_neg = is_negative_4096(dividend);
    
    aria_int4096_t abs_dividend = dividend_neg ? negate_4096(*dividend) : *dividend;
    aria_int4096_t abs_divisor = is_negative_4096(divisor) ? negate_4096(*divisor) : *divisor;
    
    aria_int4096_t remainder = aria_lbim_urem4096(&abs_dividend, &abs_divisor);
    
    if (dividend_neg) {
        remainder = negate_4096(remainder);
    }
    
    return remainder;
}

// Signed Modulo (alias for srem - C99 behavior)
extern "C" aria_int4096_t aria_lbim_smod4096(const aria_int4096_t* dividend, const aria_int4096_t* divisor) {
    return aria_lbim_srem4096(dividend, divisor);
}

// Bitwise AND
extern "C" aria_int4096_t aria_lbim_and4096(aria_int4096_t a, aria_int4096_t b) {
    aria_int4096_t result;
    for (int i = 0; i < N4096; ++i) {
        result.limbs[i] = a.limbs[i] & b.limbs[i];
    }
    return result;
}

// Bitwise OR
extern "C" aria_int4096_t aria_lbim_or4096(aria_int4096_t a, aria_int4096_t b) {
    aria_int4096_t result;
    for (int i = 0; i < N4096; ++i) {
        result.limbs[i] = a.limbs[i] | b.limbs[i];
    }
    return result;
}

// Bitwise XOR
extern "C" aria_int4096_t aria_lbim_xor4096(aria_int4096_t a, aria_int4096_t b) {
    aria_int4096_t result;
    for (int i = 0; i < N4096; ++i) {
        result.limbs[i] = a.limbs[i] ^ b.limbs[i];
    }
    return result;
}

// Bitwise NOT
extern "C" aria_int4096_t aria_lbim_not4096(aria_int4096_t a) {
    aria_int4096_t result;
    for (int i = 0; i < N4096; ++i) {
        result.limbs[i] = ~a.limbs[i];
    }
    return result;
}

// Left Shift
extern "C" aria_int4096_t aria_lbim_shl4096(aria_int4096_t a, uint32_t shift) {
    // ARIA SAFETY: Preserve ERR sentinel through shifts (fail-hard policy)
    if (is_err_sentinel<64>(a.limbs)) {
        return a;
    }
    
    aria_int4096_t result;
    memset(&result, 0, sizeof(result));
    
    if (shift >= 4096) return result;
    
    uint32_t limb_shift = shift / 64;
    uint32_t bit_shift = shift % 64;
    
    for (int i = N4096 - 1; i >= 0; --i) {
        int src_idx = i - limb_shift;
        if (src_idx >= 0) {
            result.limbs[i] = a.limbs[src_idx] << bit_shift;
            if (bit_shift > 0 && src_idx > 0) {
                result.limbs[i] |= a.limbs[src_idx - 1] >> (64 - bit_shift);
            }
        }
    }
    
    return result;
}

// Logical Right Shift
extern "C" aria_int4096_t aria_lbim_lshr4096(aria_int4096_t a, uint32_t shift) {
    // ARIA SAFETY: Preserve ERR sentinel through shifts (fail-hard policy)
    if (is_err_sentinel<64>(a.limbs)) {
        return a;
    }
    
    aria_int4096_t result;
    memset(&result, 0, sizeof(result));
    
    if (shift >= 4096) return result;
    
    uint32_t limb_shift = shift / 64;
    uint32_t bit_shift = shift % 64;
    
    for (int i = 0; i < N4096; ++i) {
        int src_idx = i + limb_shift;
        if (src_idx < N4096) {
            result.limbs[i] = a.limbs[src_idx] >> bit_shift;
            if (bit_shift > 0 && src_idx + 1 < N4096) {
                result.limbs[i] |= a.limbs[src_idx + 1] << (64 - bit_shift);
            }
        }
    }
    
    return result;
}

// Arithmetic Right Shift
extern "C" aria_int4096_t aria_lbim_ashr4096(aria_int4096_t a, uint32_t shift) {
    // ARIA SAFETY: Preserve ERR sentinel through shifts (fail-hard policy)
    if (is_err_sentinel<64>(a.limbs)) {
        return a;
    }
    
    aria_int4096_t result = aria_lbim_lshr4096(a, shift);
    
    if (is_negative_4096(&a)) {
        uint32_t limb_shift = shift / 64;
        uint32_t bit_shift = shift % 64;
        
        for (int i = N4096 - 1; i >= N4096 - limb_shift && i >= 0; --i) {
            result.limbs[i] = 0xFFFFFFFFFFFFFFFFULL;
        }
        
        if (limb_shift < N4096 && bit_shift > 0) {
            result.limbs[N4096 - 1 - limb_shift] |= (0xFFFFFFFFFFFFFFFFULL << (64 - bit_shift));
        }
    }
    
    return result;
}

// Unsigned Compare
extern "C" int32_t aria_lbim_ucmp4096(const aria_int4096_t* a, const aria_int4096_t* b) {
    for (int i = N4096 - 1; i >= 0; --i) {
        if (a->limbs[i] > b->limbs[i]) return 1;
        if (a->limbs[i] < b->limbs[i]) return -1;
    }
    return 0;
}

// Signed Compare
extern "C" int32_t aria_lbim_scmp4096(const aria_int4096_t* a, const aria_int4096_t* b) {
    bool a_neg = is_negative_4096(a);
    bool b_neg = is_negative_4096(b);
    
    if (a_neg && !b_neg) return -1;
    if (!a_neg && b_neg) return 1;
    
    return aria_lbim_ucmp4096(a, b);
}

// Equality
extern "C" bool aria_lbim_eq4096(const aria_int4096_t* a, const aria_int4096_t* b) {
    return aria_lbim_ucmp4096(a, b) == 0;
}

// ══════════════════════════════════════════════════════════════════════════════
// 4096-BIT FORMATTERS
// ══════════════════════════════════════════════════════════════════════════════

extern "C" AriaString* aria_format_uint4096(const aria_int4096_t* val) {
    if (!val) return alloc_aria_string("(null)");
    
    // Check for zero
    if (is_zero_4096(val)) {
        return alloc_aria_string("0");
    }
    
    aria_int4096_t work = *val;
    std::string result;
    result.reserve(1300);  // 4096 bits ≈ 1233 decimal digits max
    
    aria_int4096_t ten;
    memset(&ten, 0, sizeof(ten));
    ten.limbs[0] = 10;
    
    // Repeatedly divide by 10, collecting remainders
    while (!is_zero_4096(&work)) {
        aria_int4096_t quotient = aria_lbim_udiv4096(&work, &ten);
        aria_int4096_t remainder = aria_lbim_umod4096(&work, &ten);
        
        char digit = '0' + (char)(remainder.limbs[0] & 0xFF);
        result.push_back(digit);
        
        work = quotient;
    }
    
    std::reverse(result.begin(), result.end());
    return alloc_aria_string(result.c_str());
}

extern "C" AriaString* aria_format_int4096(const aria_int4096_t* val) {
    if (!val) return alloc_aria_string("(null)");
    
    bool negative = is_negative_4096(val);
    
    if (negative) {
        aria_int4096_t abs_val = negate_4096(*val);
        AriaString* abs_str = aria_format_uint4096(&abs_val);
        
        // Prepend '-' sign
        size_t new_len = abs_str->length + 1;
        char* new_buf = (char*)malloc(new_len + 1);
        new_buf[0] = '-';
        memcpy(new_buf + 1, abs_str->data, abs_str->length);
        new_buf[new_len] = '\0';
        
        free((void*)abs_str->data);
        abs_str->data = new_buf;
        abs_str->length = (int64_t)new_len;
        
        return abs_str;
    } else {
        return aria_format_uint4096(val);
    }
}

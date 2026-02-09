/**
 * @file src/backend/runtime/ternary_ops.cpp
 * @brief Implementation of balanced ternary and nonary arithmetic runtime
 * 
 * This file implements software emulation of ternary (base-3) and nonary (base-9)
 * arithmetic using lookup tables and efficient ripple-carry algorithms.
 * 
 * Key optimizations:
 *   - Lookup tables for pack/unpack operations (avoid modulo/division)
 *   - Split-byte packing for tryte (two 5-trit trybbles)
 *   - Direct biased arithmetic for nyte (faster than ternary)
 *   - Sticky error propagation (consistent with TBB model)
 */

#include "backend/runtime/ternary_ops.h"
#include <cstdint>
#include <cstdio>   // for fprintf (debug)
#include <cstdlib>
#include <limits>

// ==============================================================================
// Internal Data Structures
// ==============================================================================

namespace {

/// Unpacked representation of a 5-trit trybble
struct Trybble {
    int8_t trits[5]; ///< 5 trits, each in {-1, 0, 1}
};

/// Lookup table: uint8 → Trybble (256 entries)
/// Maps packed trybble (bias 121) to 5 trits
static Trybble LUT_UNPACK[256];

/// Powers of 3 for packing: {1, 3, 9, 27, 81, 243, 729, 2187, 6561, 19683}
/// 10 elements needed: indices 0-4 for low trybble, 5-9 for high trybble
static const int POW3[10] = {1, 3, 9, 27, 81, 243, 729, 2187, 6561, 19683};

/// Powers of 9 for nyte operations: {1, 9, 81, 729, 6561}
static const int POW9[5] = {1, 9, 81, 729, 6561};

/// Initialization flag (thread-safe via static init)
static bool g_initialized = false;

/// Trybble bias: maps [-121, 121] to [0, 242]
static constexpr int TRYBBLE_BIAS = 121;

/// Nyte/Tryte bias: maps [-29524, 29524] to [0, 59048]
static constexpr int NYTE_BIAS = 29524;

/**
 * @brief Initialize lookup tables for ternary arithmetic
 * 
 * Fills LUT_UNPACK with all 256 possible trybble values.
 * Indices 0-242 are valid (representing -121 to 121).
 * Indices 243-255 are marked as invalid (sentinel -2 in all trits).
 */
void init_lut() {
    if (g_initialized) return;
    
    for (int i = 0; i < 256; ++i) {
        // Unbias the stored value
        int value = i - TRYBBLE_BIAS;
        
        // Mark invalid range (outside [-121, 121])
        if (value < -TRYBBLE_BIAS || value > TRYBBLE_BIAS) {
            for (int k = 0; k < 5; ++k) {
                LUT_UNPACK[i].trits[k] = -2; // Sentinel
            }
            continue;
        }
        
        // Unpack to balanced ternary using repeated division
        int temp = value;
        for (int k = 0; k < 5; ++k) {
            int rem = temp % 3;
            temp /= 3;
            
            // Adjust for balanced ternary: {-1, 0, 1}
            // Standard modulo gives {0, 1, 2} for positive
            // and {0, -1, -2} for negative
            if (rem == 2) {
                LUT_UNPACK[i].trits[k] = -1;
                temp += 1; // Carry adjustment
            } else if (rem == -2) {
                LUT_UNPACK[i].trits[k] = 1;
                temp -= 1; // Borrow adjustment
            } else {
                LUT_UNPACK[i].trits[k] = static_cast<int8_t>(rem);
            }
        }
    }
    
    g_initialized = true;
}

/**
 * @brief Pack 5 trits into a uint8 trybble
 * @param trits Array of 5 trits
 * @return Packed trybble (biased to [0, 242])
 */
inline uint8_t pack_trybble(const int8_t trits[5]) {
    int value = 0;
    for (int i = 0; i < 5; ++i) {
        value += trits[i] * POW3[i];
    }
    return static_cast<uint8_t>(value + TRYBBLE_BIAS);
}

/**
 * @brief Check if trybble is valid (not sentinel)
 */
inline bool is_valid_trybble(const Trybble& tb) {
    return tb.trits[0] != -2;
}

} // anonymous namespace

// ==============================================================================
// Public Initialization
// ==============================================================================

void aria_ternary_init() {
    init_lut();
}

// ==============================================================================
// Atomic Trit Operations
// ==============================================================================

int16_t aria_trit_add_carry(int8_t a, int8_t b, int8_t carry_in) {
    // Full adder: a + b + carry_in = 3*carry_out + sum
    int raw_sum = static_cast<int>(a) + static_cast<int>(b) + static_cast<int>(carry_in);
    
    int8_t carry_out = 0;
    int8_t sum = static_cast<int8_t>(raw_sum);
    
    // Handle carry propagation for balanced ternary
    if (raw_sum >= 2) {
        carry_out = 1;
        sum = static_cast<int8_t>(raw_sum - 3);
    } else if (raw_sum <= -2) {
        carry_out = -1;
        sum = static_cast<int8_t>(raw_sum + 3);
    }
    
    // Pack result: (carry << 8) | (sum & 0xFF)
    return static_cast<int16_t>((carry_out << 8) | (sum & 0xFF));
}

int8_t aria_trit_add(int8_t a, int8_t b) {
    // ERR propagation (sticky error)
    if (a == -128 || b == -128) return -128;  // TRIT_ERR
    
    int16_t result = aria_trit_add_carry(a, b, 0);
    int8_t sum = static_cast<int8_t>(result & 0xFF);
    
    // Check if result is out of valid trit range (-1, 0, 1)
    if (sum < -1 || sum > 1) return -128;  // Overflow → ERR
    
    return sum;
}

int8_t aria_trit_sub(int8_t a, int8_t b) {
    // ERR propagation (sticky error)
    if (a == -128 || b == -128) return -128;  // TRIT_ERR
    
    return aria_trit_add(a, static_cast<int8_t>(-b));
}

int8_t aria_trit_mul(int8_t a, int8_t b) {
    // ERR propagation (sticky error)
    if (a == -128 || b == -128) return -128;  // TRIT_ERR
    
    // Simple multiplication: {-1, 0, 1} × {-1, 0, 1}
    int8_t result = static_cast<int8_t>(a * b);
    
    // Validate result is in trit range
    if (result < -1 || result > 1) return -128;  // Overflow → ERR
    
    return result;
}

int8_t aria_trit_neg(int8_t a) {
    // ERR propagation (sticky error)
    if (a == -128) return -128;  // TRIT_ERR
    
    return static_cast<int8_t>(-a);
}

// ARIA SAFETY FIX (Gemini Batch 02, BUG-005): Kleene Logic for Balanced Ternary
// Balanced ternary logic uses Kleene logic (min/max), NOT bitwise operations
// Values: True=1, False=-1, Unknown=0, ERR=-128

int8_t aria_trit_and(int8_t a, int8_t b) {
    // Kleene AND: min(a, b) - takes most "false" value
    // ERR propagates (sticky error)
    if (a == -128 || b == -128) return -128;  // TRIT_ERR
    return (a < b) ? a : b;  // min(a, b)
}

int8_t aria_trit_or(int8_t a, int8_t b) {
    // Kleene OR: max(a, b) - takes most "true" value
    // ERR propagates (sticky error)
    if (a == -128 || b == -128) return -128;  // TRIT_ERR
    return (a > b) ? a : b;  // max(a, b)
}

int8_t aria_trit_not(int8_t a) {
    // NOT: logical negation
    // True (1) -> False (-1), False (-1) -> True (1), Unknown (0) -> Unknown (0)
    // ERR propagates
    if (a == -128) return -128;  // TRIT_ERR
    return static_cast<int8_t>(-a);
}

// ==============================================================================
// Composite Tryte Operations
// ==============================================================================

uint16_t aria_tryte_add(uint16_t a, uint16_t b) {
    // Lazy initialization
    if (!g_initialized) init_lut();
    
    // 1. Sticky error check
    if (a == TRYTE_ERR || b == TRYTE_ERR) {
        return TRYTE_ERR;
    }
    
    // 2. Unpack using split-byte strategy
    uint8_t a_lo = a & 0xFF;
    uint8_t a_hi = (a >> 8) & 0xFF;
    uint8_t b_lo = b & 0xFF;
    uint8_t b_hi = (b >> 8) & 0xFF;
    
    const Trybble& ta_lo = LUT_UNPACK[a_lo];
    const Trybble& ta_hi = LUT_UNPACK[a_hi];
    const Trybble& tb_lo = LUT_UNPACK[b_lo];
    const Trybble& tb_hi = LUT_UNPACK[b_hi];
    
    // Validate unpacked values
    if (!is_valid_trybble(ta_lo) || !is_valid_trybble(ta_hi) ||
        !is_valid_trybble(tb_lo) || !is_valid_trybble(tb_hi)) {
        return TRYTE_ERR;
    }
    
    // 3. Ripple-carry addition
    int8_t res_lo[5], res_hi[5];
    int8_t carry = 0;
    
    // Process low trybble (trits 0-4)
    for (int i = 0; i < 5; ++i) {
        int16_t result = aria_trit_add_carry(ta_lo.trits[i], tb_lo.trits[i], carry);
        carry = static_cast<int8_t>(result >> 8);
        res_lo[i] = static_cast<int8_t>(result & 0xFF);
    }
    
    // Process high trybble (trits 5-9)
    for (int i = 0; i < 5; ++i) {
        int16_t result = aria_trit_add_carry(ta_hi.trits[i], tb_hi.trits[i], carry);
        carry = static_cast<int8_t>(result >> 8);
        res_hi[i] = static_cast<int8_t>(result & 0xFF);
    }
    
    // 4. Check for overflow (final carry != 0)
    if (carry != 0) {
        return TRYTE_ERR;
    }
    
    // 5. Repack result
    uint8_t packed_lo = pack_trybble(res_lo);
    uint8_t packed_hi = pack_trybble(res_hi);
    
    return static_cast<uint16_t>((packed_hi << 8) | packed_lo);
}

uint16_t aria_tryte_sub(uint16_t a, uint16_t b) {
    // Subtraction via negation: a - b = a + (-b)
    uint16_t neg_b = aria_tryte_neg(b);
    return aria_tryte_add(a, neg_b);
}

uint16_t aria_tryte_neg(uint16_t a) {
    if (!g_initialized) init_lut();
    
    // Sticky error
    if (a == TRYTE_ERR) return TRYTE_ERR;
    
    // Unpack
    uint8_t a_lo = a & 0xFF;
    uint8_t a_hi = (a >> 8) & 0xFF;
    
    const Trybble& ta_lo = LUT_UNPACK[a_lo];
    const Trybble& ta_hi = LUT_UNPACK[a_hi];
    
    if (!is_valid_trybble(ta_lo) || !is_valid_trybble(ta_hi)) {
        return TRYTE_ERR;
    }
    
    // Negate all trits (element-wise inversion)
    int8_t neg_lo[5], neg_hi[5];
    for (int i = 0; i < 5; ++i) {
        neg_lo[i] = -ta_lo.trits[i];
        neg_hi[i] = -ta_hi.trits[i];
    }
    
    // Repack
    uint8_t packed_lo = pack_trybble(neg_lo);
    uint8_t packed_hi = pack_trybble(neg_hi);
    
    return static_cast<uint16_t>((packed_hi << 8) | packed_lo);
}

uint16_t aria_tryte_mul(uint16_t a, uint16_t b) {
    // For multiplication, convert to binary, multiply, convert back
    if (a == TRYTE_ERR || b == TRYTE_ERR) return TRYTE_ERR;
    
    int32_t a_bin = aria_tryte_to_bin(a);
    int32_t b_bin = aria_tryte_to_bin(b);
    
    if (a_bin == INT32_MIN || b_bin == INT32_MIN) return TRYTE_ERR;
    
    int64_t product = static_cast<int64_t>(a_bin) * static_cast<int64_t>(b_bin);
    
    // Check range
    if (product < TERNARY_MIN || product > TERNARY_MAX) {
        return TRYTE_ERR;
    }
    
    return aria_bin_to_tryte(static_cast<int32_t>(product));
}

uint16_t aria_tryte_div(uint16_t a, uint16_t b) {
    if (a == TRYTE_ERR || b == TRYTE_ERR) return TRYTE_ERR;
    
    int32_t a_bin = aria_tryte_to_bin(a);
    int32_t b_bin = aria_tryte_to_bin(b);
    
    if (a_bin == INT32_MIN || b_bin == INT32_MIN || b_bin == 0) {
        return TRYTE_ERR;
    }
    
    int32_t quotient = a_bin / b_bin;
    return aria_bin_to_tryte(quotient);
}

uint16_t aria_tryte_mod(uint16_t a, uint16_t b) {
    // Sticky Error Propagation
    if (a == TRYTE_ERR || b == TRYTE_ERR) return TRYTE_ERR;
    
    // Unbias (convert to semantic values)
    int32_t val_a = aria_tryte_to_bin(a);
    int32_t val_b = aria_tryte_to_bin(b);
    
    // Division by Zero Check
    if (val_a == INT32_MIN || val_b == INT32_MIN || val_b == 0) {
        return TRYTE_ERR;
    }
    
    // Calculate Balanced Remainder: r = a - (b * round(a/b))
    // Use round-to-nearest (symmetric rounding for balanced arithmetic)
    double quot_d = static_cast<double>(val_a) / static_cast<double>(val_b);
    int32_t q;
    if (quot_d >= 0) {
        q = static_cast<int32_t>(quot_d + 0.5);
    } else {
        q = static_cast<int32_t>(quot_d - 0.5);
    }
    
    int32_t result_balanced = val_a - (val_b * q);
    
    // Bounds check (defensive coding)
    if (result_balanced < TERNARY_MIN || result_balanced > TERNARY_MAX) {
        return TRYTE_ERR;
    }
    
    // Re-bias and return
    return aria_bin_to_tryte(result_balanced);
}

int32_t aria_tryte_cmp(uint16_t a, uint16_t b) {
    if (a == TRYTE_ERR || b == TRYTE_ERR) return -2;
    
    int32_t a_bin = aria_tryte_to_bin(a);
    int32_t b_bin = aria_tryte_to_bin(b);
    
    if (a_bin == INT32_MIN || b_bin == INT32_MIN) return -2;
    
    if (a_bin < b_bin) return -1;
    if (a_bin > b_bin) return 1;
    return 0;
}

uint16_t aria_bin_to_tryte(int32_t value) {
    if (!g_initialized) init_lut();
    
    // Range check
    if (value < TERNARY_MIN || value > TERNARY_MAX) {
        return TRYTE_ERR;
    }
    
    // Convert to balanced ternary
    int8_t trits[10];
    int temp = value;
    
    for (int i = 0; i < 10; ++i) {
        int rem = temp % 3;
        temp /= 3;
        
        // Adjust for balanced ternary
        if (rem == 2) {
            trits[i] = -1;
            temp += 1;
        } else if (rem == -2) {
            trits[i] = 1;
            temp -= 1;
        } else {
            trits[i] = static_cast<int8_t>(rem);
        }
    }
    
    // Pack into two trybbles
    uint8_t lo = pack_trybble(&trits[0]);
    uint8_t hi = pack_trybble(&trits[5]);
    
    return static_cast<uint16_t>((hi << 8) | lo);
}

int32_t aria_tryte_to_bin(uint16_t tryte) {
    if (!g_initialized) init_lut();
    
    if (tryte == TRYTE_ERR) return INT32_MIN;
    
    // Unpack
    uint8_t lo = tryte & 0xFF;
    uint8_t hi = (tryte >> 8) & 0xFF;
    
    const Trybble& t_lo = LUT_UNPACK[lo];
    const Trybble& t_hi = LUT_UNPACK[hi];
    
    if (!is_valid_trybble(t_lo) || !is_valid_trybble(t_hi)) {
        return INT32_MIN;
    }
    
    // Convert to integer
    int32_t value = 0;
    for (int i = 0; i < 5; ++i) {
        value += t_lo.trits[i] * POW3[i];
    }
    for (int i = 0; i < 5; ++i) {
        value += t_hi.trits[i] * POW3[i + 5];
    }
    
    return value;
}

int8_t aria_tryte_get_trit(uint16_t tryte, uint8_t index) {
    if (!g_initialized) init_lut();
    
    if (tryte == TRYTE_ERR || index > 9) return -2;
    
    // Select trybble
    uint8_t packed = (index < 5) ? (tryte & 0xFF) : ((tryte >> 8) & 0xFF);
    const Trybble& tb = LUT_UNPACK[packed];
    
    if (!is_valid_trybble(tb)) return -2;
    
    return tb.trits[index % 5];
}

// ==============================================================================
// Atomic Nit Operations
// ==============================================================================

// ARIA SAFETY FIX (Gemini Batch 02, BUG-007): Error sentinels instead of saturation
// Nit valid range: [-4, +4] (5 nonary digits: -4, -3, -2, -1, 0, 1, 2, 3, 4)
// NIT_ERR: -128 (same as TRIT_ERR for consistency)

#define NIT_ERR -128

int8_t aria_nit_add(int8_t a, int8_t b) {
    // Sticky error propagation
    if (a == NIT_ERR || b == NIT_ERR) return NIT_ERR;
    
    int sum = static_cast<int>(a) + static_cast<int>(b);
    
    // FIXED: Return ERR on overflow instead of saturating
    if (sum > 4 || sum < -4) return NIT_ERR;
    
    return static_cast<int8_t>(sum);
}

int8_t aria_nit_sub(int8_t a, int8_t b) {
    // Sticky error propagation
    if (a == NIT_ERR || b == NIT_ERR) return NIT_ERR;
    
    return aria_nit_add(a, static_cast<int8_t>(-b));
}

int8_t aria_nit_mul(int8_t a, int8_t b) {
    // Sticky error propagation
    if (a == NIT_ERR || b == NIT_ERR) return NIT_ERR;
    
    int product = static_cast<int>(a) * static_cast<int>(b);
    
    // FIXED: Return ERR on overflow instead of saturating
    if (product > 4 || product < -4) return NIT_ERR;
    
    return static_cast<int8_t>(product);
}

int8_t aria_nit_neg(int8_t a) {
    // Sticky error propagation
    if (a == NIT_ERR) return NIT_ERR;
    
    return static_cast<int8_t>(-a);
}

// ARIA SAFETY FIX (Gemini Batch 02, BUG-005): Kleene Logic for Nonary
// Nonary logic uses same Kleene semantics as balanced ternary
// Values: [-4, -3, -2, -1, 0, 1, 2, 3, 4], ERR=-128

int8_t aria_nit_and(int8_t a, int8_t b) {
    // Kleene AND: min(a, b)
    if (a == NIT_ERR || b == NIT_ERR) return NIT_ERR;
    return (a < b) ? a : b;
}

int8_t aria_nit_or(int8_t a, int8_t b) {
    // Kleene OR: max(a, b)
    if (a == NIT_ERR || b == NIT_ERR) return NIT_ERR;
    return (a > b) ? a : b;
}

int8_t aria_nit_not(int8_t a) {
    // NOT: logical negation
    if (a == NIT_ERR) return NIT_ERR;
    return static_cast<int8_t>(-a);
}

// ==============================================================================
// Composite Nyte Operations (Biased Arithmetic - Faster than Tryte)
// ==============================================================================

uint16_t aria_nyte_add(uint16_t a, uint16_t b) {
    // Sticky error
    if (a == NYTE_ERR || b == NYTE_ERR) return NYTE_ERR;
    
    // Unbias to signed integers
    int32_t a_val = static_cast<int32_t>(a) - NYTE_BIAS;
    int32_t b_val = static_cast<int32_t>(b) - NYTE_BIAS;
    
    // Add
    int32_t sum = a_val + b_val;
    
    // Range check
    if (sum < TERNARY_MIN || sum > TERNARY_MAX) {
        return NYTE_ERR;
    }
    
    // Rebias and return
    return static_cast<uint16_t>(sum + NYTE_BIAS);
}

uint16_t aria_nyte_sub(uint16_t a, uint16_t b) {
    if (a == NYTE_ERR || b == NYTE_ERR) return NYTE_ERR;
    
    int32_t a_val = static_cast<int32_t>(a) - NYTE_BIAS;
    int32_t b_val = static_cast<int32_t>(b) - NYTE_BIAS;
    
    int32_t diff = a_val - b_val;
    
    if (diff < TERNARY_MIN || diff > TERNARY_MAX) {
        return NYTE_ERR;
    }
    
    return static_cast<uint16_t>(diff + NYTE_BIAS);
}

uint16_t aria_nyte_mul(uint16_t a, uint16_t b) {
    if (a == NYTE_ERR || b == NYTE_ERR) return NYTE_ERR;
    
    int32_t a_val = static_cast<int32_t>(a) - NYTE_BIAS;
    int32_t b_val = static_cast<int32_t>(b) - NYTE_BIAS;
    
    int64_t product = static_cast<int64_t>(a_val) * static_cast<int64_t>(b_val);
    
    if (product < TERNARY_MIN || product > TERNARY_MAX) {
        return NYTE_ERR;
    }
    
    return static_cast<uint16_t>(static_cast<int32_t>(product) + NYTE_BIAS);
}

uint16_t aria_nyte_div(uint16_t a, uint16_t b) {
    if (a == NYTE_ERR || b == NYTE_ERR) return NYTE_ERR;
    
    int32_t a_val = static_cast<int32_t>(a) - NYTE_BIAS;
    int32_t b_val = static_cast<int32_t>(b) - NYTE_BIAS;
    
    if (b_val == 0) return NYTE_ERR;
    
    int32_t quotient = a_val / b_val;
    return static_cast<uint16_t>(quotient + NYTE_BIAS);
}

uint16_t aria_nyte_mod(uint16_t a, uint16_t b) {
    // Sticky Error Propagation
    if (a == NYTE_ERR || b == NYTE_ERR) return NYTE_ERR;
    
    // Unbias (convert to semantic values)
    int32_t val_a = static_cast<int32_t>(a) - NYTE_BIAS;
    int32_t val_b = static_cast<int32_t>(b) - NYTE_BIAS;
    
    // Division by Zero Check
    if (val_b == 0) return NYTE_ERR;
    
    // Calculate Balanced Remainder: r = a - (b * round(a/b))
    // Same round-to-nearest logic for base-9 balanced system
    double quot_d = static_cast<double>(val_a) / static_cast<double>(val_b);
    int32_t q;
    if (quot_d >= 0) {
        q = static_cast<int32_t>(quot_d + 0.5);
    } else {
        q = static_cast<int32_t>(quot_d - 0.5);
    }
    
    int32_t result_balanced = val_a - (val_b * q);
    
    // Bounds check (defensive coding)
    if (result_balanced < TERNARY_MIN || result_balanced > TERNARY_MAX) {
        return NYTE_ERR;
    }
    
    // Re-bias and return
    return static_cast<uint16_t>(result_balanced + NYTE_BIAS);
}

uint16_t aria_nyte_neg(uint16_t a) {
    if (a == NYTE_ERR) return NYTE_ERR;
    
    int32_t a_val = static_cast<int32_t>(a) - NYTE_BIAS;
    int32_t neg_val = -a_val;
    
    return static_cast<uint16_t>(neg_val + NYTE_BIAS);
}

int32_t aria_nyte_cmp(uint16_t a, uint16_t b) {
    if (a == NYTE_ERR || b == NYTE_ERR) return -2;
    
    int32_t a_val = static_cast<int32_t>(a) - NYTE_BIAS;
    int32_t b_val = static_cast<int32_t>(b) - NYTE_BIAS;
    
    if (a_val < b_val) return -1;
    if (a_val > b_val) return 1;
    return 0;
}

uint16_t aria_bin_to_nyte(int32_t value) {
    if (value < TERNARY_MIN || value > TERNARY_MAX) {
        return NYTE_ERR;
    }
    return static_cast<uint16_t>(value + NYTE_BIAS);
}

int32_t aria_nyte_to_bin(uint16_t nyte) {
    if (nyte == NYTE_ERR) return INT32_MIN;
    return static_cast<int32_t>(nyte) - NYTE_BIAS;
}

int8_t aria_nyte_get_nit(uint16_t nyte, uint8_t index) {
    if (nyte == NYTE_ERR || index > 4) return INT8_MIN;
    
    int32_t value = aria_nyte_to_bin(nyte);
    if (value == INT32_MIN) return INT8_MIN;
    
    // Extract nit at index using division/modulo
    for (uint8_t i = 0; i < index; ++i) {
        value /= 9;
    }
    
    int rem = value % 9;
    // Adjust for balanced nonary [-4, 4]
    if (rem > 4) rem -= 9;
    else if (rem < -4) rem += 9;
    
    return static_cast<int8_t>(rem);
}
// ==============================================================================
// TBB (Ternary/Binary/Boolean) Type Conversion Functions
// ==============================================================================

/**
 * @brief Convert int32 to TBB8 (Ternary-like type with range -127 to 127)
 * @param value Integer value to convert
 * @return TBB8 value in range [-127, 127], or -128 (ERR) if out of range
 * 
 * CRITICAL SAFETY: This prevents wraparound bugs (e.g., 300 → 44)
 * Used for safety-critical systems where overflow must be detected
 */
int8_t aria_tbb8_from_int(int32_t value) {
    // Valid range for TBB8: [-127, 127]
    // Sentinel -128 represents ERR
    if (value < -127 || value > 127) {
        return -128; // ERR sentinel
    }
    return static_cast<int8_t>(value);
}

/**
 * @brief Convert TBB8 to int32
 * @param value TBB8 value
 * @return int32 representation, or INT32_MIN if input is ERR
 */
int32_t aria_tbb8_to_int(int8_t value) {
    if (value == -128) {
        return INT32_MIN; // Propagate error
    }
    return static_cast<int32_t>(value);
}
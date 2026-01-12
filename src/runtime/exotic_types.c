#include "runtime/exotic_types.h"
#include <stdlib.h>

// ============================================================================
// EXOTIC BALANCED BASE ARITHMETIC - Implementation
// ============================================================================
// Reference: docs/gemini/responses/remaining/Exotic Balanced Base Arithmetic Implementation.txt
//
// This implements arithmetic for balanced ternary (base 3) and balanced nonary
// (base 9) types using biased representation for efficient storage.
//
// Key Design Principles:
//   1. Biased representation: stored_value = balanced_value + BIAS
//   2. Sticky error propagation: ERR + anything = ERR
//   3. Overflow detection: result outside ±29,524 → ERR
//   4. Sentinel protection: 0xFFFF reserved for TRYTE_ERR/NYTE_ERR
// ============================================================================

// ============================================================================
// TRYTE Arithmetic Implementation
// ============================================================================

tryte_t tryte_add(tryte_t a, tryte_t b) {
    // Step 1: Check for ERR propagation
    if (a == TRYTE_ERR || b == TRYTE_ERR) {
        return TRYTE_ERR;
    }
    
    // Step 2: Convert to balanced values
    int32_t val_a = (int32_t)a - TRYTE_BIAS;
    int32_t val_b = (int32_t)b - TRYTE_BIAS;
    
    // Step 3: Perform addition
    int32_t result_balanced = val_a + val_b;
    
    // Step 4: Check for overflow
    if (result_balanced < TRYTE_MIN_VALUE || result_balanced > TRYTE_MAX_VALUE) {
        return TRYTE_ERR;
    }
    
    // Step 5: Convert back to biased representation
    return (tryte_t)(result_balanced + TRYTE_BIAS);
}

tryte_t tryte_sub(tryte_t a, tryte_t b) {
    // Step 1: Check for ERR propagation
    if (a == TRYTE_ERR || b == TRYTE_ERR) {
        return TRYTE_ERR;
    }
    
    // Step 2: Convert to balanced values
    int32_t val_a = (int32_t)a - TRYTE_BIAS;
    int32_t val_b = (int32_t)b - TRYTE_BIAS;
    
    // Step 3: Perform subtraction
    int32_t result_balanced = val_a - val_b;
    
    // Step 4: Check for overflow
    if (result_balanced < TRYTE_MIN_VALUE || result_balanced > TRYTE_MAX_VALUE) {
        return TRYTE_ERR;
    }
    
    // Step 5: Convert back to biased representation
    return (tryte_t)(result_balanced + TRYTE_BIAS);
}

tryte_t tryte_mul(tryte_t a, tryte_t b) {
    // Step 1: Check for ERR propagation
    if (a == TRYTE_ERR || b == TRYTE_ERR) {
        return TRYTE_ERR;
    }
    
    // Step 2: Convert to balanced values
    int32_t val_a = (int32_t)a - TRYTE_BIAS;
    int32_t val_b = (int32_t)b - TRYTE_BIAS;
    
    // Step 3: Perform multiplication
    int32_t result_balanced = val_a * val_b;
    
    // Step 4: Check for overflow
    if (result_balanced < TRYTE_MIN_VALUE || result_balanced > TRYTE_MAX_VALUE) {
        return TRYTE_ERR;
    }
    
    // Step 5: Convert back to biased representation
    return (tryte_t)(result_balanced + TRYTE_BIAS);
}

tryte_t tryte_div(tryte_t a, tryte_t b) {
    // Step 1: Check for ERR propagation
    if (a == TRYTE_ERR || b == TRYTE_ERR) {
        return TRYTE_ERR;
    }
    
    // Step 2: Convert to balanced values
    int32_t val_a = (int32_t)a - TRYTE_BIAS;
    int32_t val_b = (int32_t)b - TRYTE_BIAS;
    
    // Step 3: Check for division by zero
    if (val_b == 0) {
        return TRYTE_ERR;
    }
    
    // Step 4: Perform division (truncated)
    int32_t result_balanced = val_a / val_b;
    
    // Step 5: Check for overflow (unlikely in division, but be safe)
    if (result_balanced < TRYTE_MIN_VALUE || result_balanced > TRYTE_MAX_VALUE) {
        return TRYTE_ERR;
    }
    
    // Step 6: Convert back to biased representation
    return (tryte_t)(result_balanced + TRYTE_BIAS);
}

tryte_t tryte_mod(tryte_t a, tryte_t b) {
    // Step 1: Check for ERR propagation
    if (a == TRYTE_ERR || b == TRYTE_ERR) {
        return TRYTE_ERR;
    }
    
    // Step 2: Convert to balanced values
    int32_t val_a = (int32_t)a - TRYTE_BIAS;
    int32_t val_b = (int32_t)b - TRYTE_BIAS;
    
    // Step 3: Check for modulo by zero
    if (val_b == 0) {
        return TRYTE_ERR;
    }
    
    // Step 4: Perform modulo
    int32_t result_balanced = val_a % val_b;
    
    // Step 5: Check for overflow (modulo result should always be in range)
    if (result_balanced < TRYTE_MIN_VALUE || result_balanced > TRYTE_MAX_VALUE) {
        return TRYTE_ERR;
    }
    
    // Step 6: Convert back to biased representation
    return (tryte_t)(result_balanced + TRYTE_BIAS);
}

tryte_t tryte_neg(tryte_t a) {
    // Step 1: Check for ERR propagation
    if (a == TRYTE_ERR) {
        return TRYTE_ERR;
    }
    
    // Step 2: Negation using bias adjustment
    // Balanced: -x
    // Biased: stored(-x) = -x + BIAS = 2*BIAS - (x + BIAS) = 2*BIAS - stored(x)
    uint32_t result = (uint32_t)(2 * TRYTE_BIAS) - (uint32_t)a;
    
    // Step 3: Check sentinel collision
    if (result == TRYTE_ERR) {
        return TRYTE_ERR;
    }
    
    return (tryte_t)result;
}

tryte_t tryte_abs(tryte_t a) {
    // Step 1: Check for ERR propagation
    if (a == TRYTE_ERR) {
        return TRYTE_ERR;
    }
    
    // Step 2: Convert to balanced value
    int32_t val_a = (int32_t)a - TRYTE_BIAS;
    
    // Step 3: Take absolute value
    int32_t result_balanced = (val_a < 0) ? -val_a : val_a;
    
    // Step 4: Convert back to biased representation
    return (tryte_t)(result_balanced + TRYTE_BIAS);
}

bool tryte_is_err(tryte_t a) {
    return a == TRYTE_ERR;
}

// ============================================================================
// NYTE Arithmetic Implementation (isomorphic to tryte)
// ============================================================================
// Nyte uses the same algorithms as tryte since 9^5 = 3^10 = 59,049

nyte_t nyte_add(nyte_t a, nyte_t b) {
    return (nyte_t)tryte_add((tryte_t)a, (tryte_t)b);
}

nyte_t nyte_sub(nyte_t a, nyte_t b) {
    return (nyte_t)tryte_sub((tryte_t)a, (tryte_t)b);
}

nyte_t nyte_mul(nyte_t a, nyte_t b) {
    return (nyte_t)tryte_mul((tryte_t)a, (tryte_t)b);
}

nyte_t nyte_div(nyte_t a, nyte_t b) {
    return (nyte_t)tryte_div((tryte_t)a, (tryte_t)b);
}

nyte_t nyte_mod(nyte_t a, nyte_t b) {
    return (nyte_t)tryte_mod((tryte_t)a, (tryte_t)b);
}

nyte_t nyte_neg(nyte_t a) {
    return (nyte_t)tryte_neg((tryte_t)a);
}

nyte_t nyte_abs(nyte_t a) {
    return (nyte_t)tryte_abs((tryte_t)a);
}

bool nyte_is_err(nyte_t a) {
    return a == NYTE_ERR;
}

// ============================================================================
// NIT Arithmetic Implementation (Lookup Table Optimization)
// ============================================================================
// Nit operations use 9x9 lookup tables for optimal performance
// Tables are L1 cache-friendly (81 entries each)

// Addition lookup table: nit_add_table[a+4][b+4] (9x9 = 81 entries)
// Indexed by offset values (range -4 to +4 mapped to 0-8)
static const int8_t nit_add_table[9][9] = {
    // b=-4  -3  -2  -1   0   1   2   3   4
    {  -8,  -7,  -6,  -5,  -4,  -3,  -2,  -1,   0 }, // a = -4
    {  -7,  -6,  -5,  -4,  -3,  -2,  -1,   0,   1 }, // a = -3
    {  -6,  -5,  -4,  -3,  -2,  -1,   0,   1,   2 }, // a = -2
    {  -5,  -4,  -3,  -2,  -1,   0,   1,   2,   3 }, // a = -1
    {  -4,  -3,  -2,  -1,   0,   1,   2,   3,   4 }, // a =  0
    {  -3,  -2,  -1,   0,   1,   2,   3,   4,   5 }, // a =  1
    {  -2,  -1,   0,   1,   2,   3,   4,   5,   6 }, // a =  2
    {  -1,   0,   1,   2,   3,   4,   5,   6,   7 }, // a =  3
    {   0,   1,   2,   3,   4,   5,   6,   7,   8 }  // a =  4
};

// Multiplication lookup table: nit_mul_table[a+4][b+4]
static const int8_t nit_mul_table[9][9] = {
    // b=-4  -3  -2  -1   0   1   2   3   4
    {  16,  12,   8,   4,   0,  -4,  -8, -12, -16 }, // a = -4
    {  12,   9,   6,   3,   0,  -3,  -6,  -9, -12 }, // a = -3
    {   8,   6,   4,   2,   0,  -2,  -4,  -6,  -8 }, // a = -2
    {   4,   3,   2,   1,   0,  -1,  -2,  -3,  -4 }, // a = -1
    {   0,   0,   0,   0,   0,   0,   0,   0,   0 }, // a =  0
    {  -4,  -3,  -2,  -1,   0,   1,   2,   3,   4 }, // a =  1
    {  -8,  -6,  -4,  -2,   0,   2,   4,   6,   8 }, // a =  2
    { -12,  -9,  -6,  -3,   0,   3,   6,   9,  12 }, // a =  3
    { -16, -12,  -8,  -4,   0,   4,   8,  12,  16 }  // a =  4
};

nit_t nit_add(nit_t a, nit_t b) {
    // Check bounds
    if (a < NIT_MIN_VALUE || a > NIT_MAX_VALUE ||
        b < NIT_MIN_VALUE || b > NIT_MAX_VALUE) {
        return 0; // Out of bounds treated as 0 (neutral element)
    }
    
    // Lookup result
    int8_t result = nit_add_table[a + 4][b + 4];
    
    // Clamp to valid nit range
    if (result < NIT_MIN_VALUE) return NIT_MIN_VALUE;
    if (result > NIT_MAX_VALUE) return NIT_MAX_VALUE;
    
    return (nit_t)result;
}

nit_t nit_sub(nit_t a, nit_t b) {
    // Subtraction: a - b = a + (-b)
    return nit_add(a, nit_neg(b));
}

nit_t nit_mul(nit_t a, nit_t b) {
    // Check bounds
    if (a < NIT_MIN_VALUE || a > NIT_MAX_VALUE ||
        b < NIT_MIN_VALUE || b > NIT_MAX_VALUE) {
        return 0; // Out of bounds treated as 0
    }
    
    // Lookup result
    int8_t result = nit_mul_table[a + 4][b + 4];
    
    // Clamp to valid nit range
    if (result < NIT_MIN_VALUE) return NIT_MIN_VALUE;
    if (result > NIT_MAX_VALUE) return NIT_MAX_VALUE;
    
    return (nit_t)result;
}

nit_t nit_div(nit_t a, nit_t b) {
    // Check for division by zero
    if (b == 0) {
        return 0; // Balanced ternary convention: x / 0 = 0
    }
    
    // Integer division (truncated)
    int8_t result = a / b;
    
    // Clamp to valid nit range
    if (result < NIT_MIN_VALUE) return NIT_MIN_VALUE;
    if (result > NIT_MAX_VALUE) return NIT_MAX_VALUE;
    
    return (nit_t)result;
}

nit_t nit_neg(nit_t a) {
    return (nit_t)(-a);
}

nit_t nit_abs(nit_t a) {
    return (nit_t)((a < 0) ? -a : a);
}

// ============================================================================
// Conversion Functions
// ============================================================================

tryte_t tryte_from_balanced(int32_t balanced_value) {
    if (balanced_value < TRYTE_MIN_VALUE || balanced_value > TRYTE_MAX_VALUE) {
        return TRYTE_ERR;
    }
    return (tryte_t)(balanced_value + TRYTE_BIAS);
}

nyte_t nyte_from_balanced(int32_t balanced_value) {
    if (balanced_value < NYTE_MIN_VALUE || balanced_value > NYTE_MAX_VALUE) {
        return NYTE_ERR;
    }
    return (nyte_t)(balanced_value + NYTE_BIAS);
}

int32_t tryte_to_balanced(tryte_t stored_value) {
    if (stored_value == TRYTE_ERR) {
        return 0; // ERR maps to 0 by convention
    }
    return (int32_t)stored_value - TRYTE_BIAS;
}

int32_t nyte_to_balanced(nyte_t stored_value) {
    if (stored_value == NYTE_ERR) {
        return 0; // ERR maps to 0 by convention
    }
    return (int32_t)stored_value - NYTE_BIAS;
}

nyte_t tryte_to_nyte(tryte_t t) {
    // Tryte and nyte are isomorphic (both 59,049 values)
    return (nyte_t)t;
}

tryte_t nyte_to_tryte(nyte_t n) {
    // Tryte and nyte are isomorphic (both 59,049 values)
    return (tryte_t)n;
}

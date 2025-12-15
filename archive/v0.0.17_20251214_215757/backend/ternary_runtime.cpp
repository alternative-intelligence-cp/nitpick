/**
 * src/backend/ternary_runtime.cpp
 * 
 * C linkage wrappers for balanced ternary operations
 * Used by LLVM IR to call into runtime library
 */

#include "ternary_ops.h"

using namespace aria::backend;

// Ensure C linkage for LLVM
extern "C" {

/**
 * Initialize ternary system (build LUTs).
 * Should be called once at program startup.
 */
void aria_ternary_init() {
    TernaryOps::initialize();
}

/**
 * Add two trytes.
 * Returns TRYTE_ERR (0xFFFF) on overflow or if either input is ERR.
 */
uint16_t aria_tryte_add(uint16_t a, uint16_t b) {
    return TernaryOps::addTrytes(a, b);
}

/**
 * Subtract two trytes (a - b).
 * Returns TRYTE_ERR on overflow or if either input is ERR.
 */
uint16_t aria_tryte_sub(uint16_t a, uint16_t b) {
    return TernaryOps::subtractTrytes(a, b);
}

/**
 * Multiply two trytes.
 * Returns TRYTE_ERR on overflow or if either input is ERR.
 */
uint16_t aria_tryte_mul(uint16_t a, uint16_t b) {
    return TernaryOps::multiplyTrytes(a, b);
}

/**
 * Divide two trytes (a / b).
 * Returns TRYTE_ERR on divide-by-zero or if either input is ERR.
 */
uint16_t aria_tryte_div(uint16_t a, uint16_t b) {
    return TernaryOps::divideTrytes(a, b);
}

/**
 * Modulo operation (a % b).
 * Returns TRYTE_ERR on divide-by-zero or if either input is ERR.
 */
uint16_t aria_tryte_mod(uint16_t a, uint16_t b) {
    uint16_t quotient = aria_tryte_div(a, b);
    if (quotient == TRYTE_ERR) {
        return TRYTE_ERR;
    }
    
    uint16_t product = aria_tryte_mul(quotient, b);
    if (product == TRYTE_ERR) {
        return TRYTE_ERR;
    }
    
    return aria_tryte_sub(a, product);
}

/**
 * Negate a tryte (flip all trits).
 * NEG(ERR) = ERR.
 */
uint16_t aria_tryte_negate(uint16_t a) {
    return TernaryOps::negateTryte(a);
}

/**
 * Compare two trytes for equality.
 * Returns 1 if equal, 0 otherwise.
 */
int32_t aria_tryte_eq(uint16_t a, uint16_t b) {
    return (a == b) ? 1 : 0;
}

/**
 * Compare two trytes for inequality.
 * Returns 1 if not equal, 0 otherwise.
 */
int32_t aria_tryte_ne(uint16_t a, uint16_t b) {
    return (a != b) ? 1 : 0;
}

/**
 * Less-than comparison (a < b).
 * Returns 1 if true, 0 otherwise.
 * ERR comparisons return 0.
 */
int32_t aria_tryte_lt(uint16_t a, uint16_t b) {
    if (a == TRYTE_ERR || b == TRYTE_ERR) {
        return 0;
    }
    
    trit trits_a[10], trits_b[10];
    TernaryOps::unpackTryte(a, trits_a);
    TernaryOps::unpackTryte(b, trits_b);
    
    // Compare from MST to LST
    for (int i = 9; i >= 0; --i) {
        if (trits_a[i] != trits_b[i]) {
            return (trits_a[i] < trits_b[i]) ? 1 : 0;
        }
    }
    
    return 0;  // Equal
}

/**
 * Less-than-or-equal comparison (a <= b).
 */
int32_t aria_tryte_le(uint16_t a, uint16_t b) {
    return (aria_tryte_lt(a, b) || aria_tryte_eq(a, b)) ? 1 : 0;
}

/**
 * Greater-than comparison (a > b).
 */
int32_t aria_tryte_gt(uint16_t a, uint16_t b) {
    return aria_tryte_le(a, b) ? 0 : 1;
}

/**
 * Greater-than-or-equal comparison (a >= b).
 */
int32_t aria_tryte_ge(uint16_t a, uint16_t b) {
    return aria_tryte_lt(a, b) ? 0 : 1;
}

/**
 * Convert binary integer to tryte.
 * Returns TRYTE_ERR if out of range [-29524, +29524].
 */
uint16_t aria_int32_to_tryte(int32_t value) {
    return TernaryOps::binaryToTryte(value);
}

/**
 * Convert tryte to binary integer.
 * Returns 0 if input is TRYTE_ERR.
 */
int32_t aria_tryte_to_int32(uint16_t tryte) {
    return TernaryOps::tryteToBinary(tryte);
}

/**
 * Check if a tryte is the ERR sentinel.
 * Returns 1 if ERR, 0 otherwise.
 */
int32_t aria_tryte_is_err(uint16_t tryte) {
    return (tryte == TRYTE_ERR) ? 1 : 0;
}

} // extern "C"

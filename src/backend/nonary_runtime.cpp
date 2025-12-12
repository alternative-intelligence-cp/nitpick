/**
 * src/backend/nonary_runtime.cpp
 * 
 * C Runtime Wrappers for Nonary Operations
 * These functions provide C linkage for LLVM-generated code
 * Version: 0.0.12
 */

#include "nonary_ops.h"

using namespace aria::backend;

// C linkage for LLVM
extern "C" {

/**
 * Initialize nonary operations
 */
void _aria_nonary_initialize() {
    NonaryOps::initialize();
}

/**
 * Add two nyte values
 */
uint16_t _aria_nyte_add(uint16_t a, uint16_t b) {
    return NonaryOps::add(a, b);
}

/**
 * Subtract two nyte values
 */
uint16_t _aria_nyte_sub(uint16_t a, uint16_t b) {
    return NonaryOps::subtract(a, b);
}

/**
 * Multiply two nyte values
 */
uint16_t _aria_nyte_mul(uint16_t a, uint16_t b) {
    return NonaryOps::multiply(a, b);
}

/**
 * Divide two nyte values
 */
uint16_t _aria_nyte_div(uint16_t a, uint16_t b) {
    return NonaryOps::divide(a, b);
}

/**
 * Modulo operation
 */
uint16_t _aria_nyte_mod(uint16_t a, uint16_t b) {
    return NonaryOps::modulo(a, b);
}

/**
 * Negate a nyte value
 */
uint16_t _aria_nyte_negate(uint16_t a) {
    return NonaryOps::negate(a);
}

/**
 * Compare two nyte values
 * @return -1 if a < b, 0 if a == b, 1 if a > b
 */
int32_t _aria_nyte_compare(uint16_t a, uint16_t b) {
    return NonaryOps::compare(a, b);
}

/**
 * Convert int32 to nyte
 */
uint16_t _aria_int_to_nyte(int32_t value) {
    return NonaryOps::packValue(value);
}

/**
 * Convert nyte to int32
 */
int32_t _aria_nyte_to_int(uint16_t packed) {
    return NonaryOps::unpackValue(packed);
}

} // extern "C"

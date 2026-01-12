#ifndef ARIA_RUNTIME_EXOTIC_TYPES_H
#define ARIA_RUNTIME_EXOTIC_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// EXOTIC BALANCED BASE ARITHMETIC - Runtime Library
// ============================================================================
// Reference: docs/gemini/responses/remaining/Exotic Balanced Base Arithmetic Implementation.txt
//
// Implements balanced ternary (base 3) and balanced nonary (base 9) arithmetic
// for Aria's exotic numeric types.
//
// Type Summary:
//   trit:  Single balanced ternary digit stored as tbb8 (-1, 0, 1, ERR=-128)
//   tryte: 10 trits stored in uint16 with biased representation (±29,524)
//   nit:   Single nonary digit stored as int8 (-4 to +4)
//   nyte:  5 nits (isomorphic to tryte) stored in uint16 (±29,524)
//
// Biased Representation:
//   stored_value = balanced_value + BIAS
//   where BIAS = 29,524 for tryte/nyte
//
// Storage Efficiency: 59,049 values in 65,536 space = 90.1%
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

// Tryte/Nyte Biased Representation
#define TRYTE_BIAS 29524
#define TRYTE_ERR 0xFFFF
#define TRYTE_MIN_VALUE (-29524)
#define TRYTE_MAX_VALUE (29524)

// Nyte uses same bias (since 9^5 = 3^10 = 59,049)
#define NYTE_BIAS TRYTE_BIAS
#define NYTE_ERR TRYTE_ERR
#define NYTE_MIN_VALUE TRYTE_MIN_VALUE
#define NYTE_MAX_VALUE TRYTE_MAX_VALUE

// Trit range (-1, 0, 1) using tbb8 sentinel
#define TRIT_ERR (-128)
#define TRIT_MIN_VALUE (-1)
#define TRIT_MAX_VALUE (1)

// Nit range (-4 to +4)
#define NIT_MIN_VALUE (-4)
#define NIT_MAX_VALUE (4)

// ============================================================================
// Type Definitions
// ============================================================================

typedef int8_t trit_t;    // Balanced ternary digit (-1, 0, 1, ERR=-128)
typedef uint16_t tryte_t; // 10 trits with biased representation
typedef int8_t nit_t;     // Balanced nonary digit (-4 to +4)
typedef uint16_t nyte_t;  // 5 nits (isomorphic to tryte)

// ============================================================================
// TRYTE Arithmetic Operations
// ============================================================================

// Add two trytes with overflow detection
tryte_t tryte_add(tryte_t a, tryte_t b);

// Subtract two trytes with overflow detection
tryte_t tryte_sub(tryte_t a, tryte_t b);

// Multiply two trytes with overflow detection
tryte_t tryte_mul(tryte_t a, tryte_t b);

// Divide two trytes (returns ERR on division by zero)
tryte_t tryte_div(tryte_t a, tryte_t b);

// Modulo operation (returns ERR on modulo by zero)
tryte_t tryte_mod(tryte_t a, tryte_t b);

// Negate a tryte using bias adjustment
tryte_t tryte_neg(tryte_t a);

// Absolute value of a tryte
tryte_t tryte_abs(tryte_t a);

// Check if tryte is ERR
bool tryte_is_err(tryte_t a);

// ============================================================================
// NYTE Arithmetic Operations (isomorphic to tryte)
// ============================================================================

// Add two nytes with overflow detection
nyte_t nyte_add(nyte_t a, nyte_t b);

// Subtract two nytes with overflow detection
nyte_t nyte_sub(nyte_t a, nyte_t b);

// Multiply two nytes with overflow detection
nyte_t nyte_mul(nyte_t a, nyte_t b);

// Divide two nytes (returns ERR on division by zero)
nyte_t nyte_div(nyte_t a, nyte_t b);

// Modulo operation (returns ERR on modulo by zero)
nyte_t nyte_mod(nyte_t a, nyte_t b);

// Negate a nyte using bias adjustment
nyte_t nyte_neg(nyte_t a);

// Absolute value of a nyte
nyte_t nyte_abs(nyte_t a);

// Check if nyte is ERR
bool nyte_is_err(nyte_t a);

// ============================================================================
// TRIT Operations
// ============================================================================
// Note: Trit uses tbb8 representation, so it inherits TBB arithmetic
// These are convenience wrappers

// Trit is just tbb8 with restricted range (-1, 0, 1, ERR)
// No special arithmetic needed - use TBB operations

// ============================================================================
// NIT Arithmetic Operations
// ============================================================================
// Nit uses lookup tables for optimal performance (9x9=81 entries)

// Add two nits using lookup table
nit_t nit_add(nit_t a, nit_t b);

// Subtract two nits using lookup table
nit_t nit_sub(nit_t a, nit_t b);

// Multiply two nits using lookup table
nit_t nit_mul(nit_t a, nit_t b);

// Divide two nits (returns 0 on division by zero, per balanced ternary convention)
nit_t nit_div(nit_t a, nit_t b);

// Negate a nit
nit_t nit_neg(nit_t a);

// Absolute value of a nit
nit_t nit_abs(nit_t a);

// ============================================================================
// Conversion Functions
// ============================================================================

// Convert balanced value to biased storage (for tryte/nyte)
tryte_t tryte_from_balanced(int32_t balanced_value);
nyte_t nyte_from_balanced(int32_t balanced_value);

// Convert biased storage to balanced value (for tryte/nyte)
int32_t tryte_to_balanced(tryte_t stored_value);
int32_t nyte_to_balanced(nyte_t stored_value);

// Convert tryte to nyte and vice versa (isomorphic)
nyte_t tryte_to_nyte(tryte_t t);
tryte_t nyte_to_tryte(nyte_t n);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_EXOTIC_TYPES_H

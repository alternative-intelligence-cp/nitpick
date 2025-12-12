/**
 * src/backend/nonary_ops.h
 * 
 * Balanced Nonary Arithmetic Operations
 * Version: 0.0.12
 * 
 * Implements packing, unpacking, and arithmetic for nit/nyte types.
 * 
 * Nonary Digit Set: {-4, -3, -2, -1, 0, 1, 2, 3, 4}
 * - nit: Single balanced nonary digit (int8)
 * - nyte: 5 nits packed into uint16 (59,049 valid states)
 * 
 * Packing Strategy: Biased-Radix Representation
 * - Range: [-29,524, +29,524]
 * - Bias: 29,524 (maps to unsigned [0, 59,048])
 * - Packed value: stored = value + 29,524
 * 
 * Error Sentinel: 0xFFFF (65,535)
 * Valid range: [0x0000, 0xE6A8] = [0, 59,048]
 */

#ifndef ARIA_BACKEND_NONARY_OPS_H
#define ARIA_BACKEND_NONARY_OPS_H

#include <cstdint>
#include <array>
#include <string>

namespace aria {
namespace backend {

// Nonary digit representation
using nit = int8_t;  // -4, -3, -2, -1, 0, 1, 2, 3, 4
static constexpr nit NIT_MIN = -4;
static constexpr nit NIT_MAX = 4;

// Nyte constants
static constexpr uint16_t NYTE_ERR = 0xFFFF;        // Error sentinel
static constexpr int32_t NYTE_MAX = 29524;          // Max valid value
static constexpr int32_t NYTE_MIN = -29524;         // Min valid value
static constexpr uint16_t NYTE_BIAS = 29524;        // Bias for uint16 storage
static constexpr uint16_t NYTE_VALID_MAX = 59048;   // Max valid uint16 encoding (0xE6A8)

// Precomputed powers of 9
static constexpr int32_t POW9[5] = {
    1,      // 9^0
    9,      // 9^1
    81,     // 9^2
    729,    // 9^3
    6561    // 9^4
};

/**
 * NonaryOps: Core balanced nonary operations
 */
class NonaryOps {
public:
    /**
     * Initialize lookup tables (if needed for optimization).
     * Must be called once before using nonary operations.
     */
    static void initialize();

    // ========== Validation Operations ==========

    /**
     * Check if a nit value is valid.
     * @param value The nit value to check
     * @return true if value is in [-4, 4]
     */
    static bool isValidNit(int8_t value);

    /**
     * Check if a nyte packed value is valid.
     * @param packed The packed uint16 value
     * @return true if value is in [0, 59048] or is ERR sentinel
     */
    static bool isValidNyte(uint16_t packed);

    /**
     * Check if a nyte value is ERR.
     * @param packed The packed uint16 value
     * @return true if value is NYTE_ERR
     */
    static bool isError(uint16_t packed);

    // ========== Packing Operations ==========

    /**
     * Pack 5 nits into a uint16 (nyte).
     * Uses biased-radix: stored = value + 29,524
     * 
     * @param nits Array of 5 nits
     * @return Packed uint16 value or NYTE_ERR if invalid
     */
    static uint16_t packNyte(const nit nits[5]);

    /**
     * Pack a signed int32 value into a nyte.
     * @param value Logical value in range [-29,524, +29,524]
     * @return Packed uint16 value or NYTE_ERR if out of range
     */
    static uint16_t packValue(int32_t value);

    // ========== Unpacking Operations ==========

    /**
     * Unpack a uint16 nyte into 5 nits.
     * 
     * @param packed Packed uint16 value
     * @param nits Output array of 5 nits
     * @return true if successful, false if ERR or invalid
     */
    static bool unpackNyte(uint16_t packed, nit nits[5]);

    /**
     * Unpack a nyte to its logical int32 value.
     * @param packed Packed uint16 value
     * @return Logical value or 0 if ERR/invalid
     */
    static int32_t unpackValue(uint16_t packed);

    // ========== Arithmetic Operations ==========

    /**
     * Add two nyte values.
     * Implements sticky error propagation.
     * 
     * @param a First operand (packed)
     * @param b Second operand (packed)
     * @return Result (packed) or NYTE_ERR on overflow
     */
    static uint16_t add(uint16_t a, uint16_t b);

    /**
     * Subtract two nyte values.
     * @param a Minuend (packed)
     * @param b Subtrahend (packed)
     * @return Result (packed) or NYTE_ERR on overflow
     */
    static uint16_t subtract(uint16_t a, uint16_t b);

    /**
     * Multiply two nyte values.
     * @param a First operand (packed)
     * @param b Second operand (packed)
     * @return Result (packed) or NYTE_ERR on overflow
     */
    static uint16_t multiply(uint16_t a, uint16_t b);

    /**
     * Divide two nyte values.
     * Truncates towards zero (C/LLVM semantics).
     * 
     * @param a Dividend (packed)
     * @param b Divisor (packed)
     * @return Result (packed) or NYTE_ERR on division by zero
     */
    static uint16_t divide(uint16_t a, uint16_t b);

    /**
     * Modulo operation.
     * @param a Dividend (packed)
     * @param b Divisor (packed)
     * @return Remainder (packed) or NYTE_ERR on division by zero
     */
    static uint16_t modulo(uint16_t a, uint16_t b);

    /**
     * Negate a nyte value.
     * @param a Operand (packed)
     * @return Negated result (packed) or NYTE_ERR if input is ERR
     */
    static uint16_t negate(uint16_t a);

    // ========== Comparison Operations ==========

    /**
     * Compare two nyte values.
     * Takes advantage of biased representation (monotonic mapping).
     * 
     * @param a First operand (packed)
     * @param b Second operand (packed)
     * @return -1 if a < b, 0 if a == b, 1 if a > b
     */
    static int compare(uint16_t a, uint16_t b);

    /**
     * Check equality.
     * @return true if a == b
     */
    static bool equals(uint16_t a, uint16_t b);

    /**
     * Check less than.
     * @return true if a < b
     */
    static bool lessThan(uint16_t a, uint16_t b);

    // ========== Conversion Operations ==========

    /**
     * Convert a nyte to a string representation.
     * Format: "nyte[d4 d3 d2 d1 d0]" where di are nit digits
     * 
     * @param packed Packed nyte value
     * @return String representation or "ERR" if invalid
     */
    static std::string toString(uint16_t packed);

    /**
     * Convert a nit to string.
     * @param nit_val The nit value
     * @return String representation
     */
    static std::string nitToString(int8_t nit_val);

private:
    // Internal helper for nits to value calculation
    static int32_t nitsToValue(const nit nits[5]);
    
    // Internal helper for value to nits decomposition
    static void valueToNits(int32_t value, nit nits[5]);
};

} // namespace backend
} // namespace aria

#endif // ARIA_BACKEND_NONARY_OPS_H

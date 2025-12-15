/**
 * src/backend/ternary_ops.h
 * 
 * Balanced Ternary Arithmetic Operations
 * Version: 0.0.11
 * 
 * Implements packing, unpacking, and arithmetic for trit/tryte types.
 * 
 * Ternary Digit Set: {-1, 0, 1} represented as {T, 0, 1}
 * - trit: Single balanced ternary digit (int8)
 * - tryte: 10 trits packed into uint16 (59,049 valid states)
 * 
 * Packing Strategy: Split-Byte Encoding
 * - Low byte (bits 0-7): Trits 0-4
 * - High byte (bits 8-15): Trits 5-9
 * - Each byte stores 5 trits (trybble) with bias of 121
 * 
 * Range: [-29,524, +29,524]
 * Error Sentinel: 0xFFFF (65,535)
 */

#ifndef ARIA_BACKEND_TERNARY_OPS_H
#define ARIA_BACKEND_TERNARY_OPS_H

#include <cstdint>
#include <array>

namespace aria {
namespace backend {

// Ternary digit representation
using trit = int8_t;  // -1, 0, 1
static constexpr trit TRIT_NEG = -1;  // T
static constexpr trit TRIT_ZERO = 0;
static constexpr trit TRIT_POS = 1;

// Tryte constants
static constexpr uint16_t TRYTE_ERR = 0xFFFF;        // Error sentinel
static constexpr int32_t TRYTE_MAX = 29524;          // Max valid value
static constexpr int32_t TRYTE_MIN = -29524;         // Min valid value
static constexpr uint16_t TRYTE_VALID_MAX = 59048;   // Max valid uint16 encoding

// Trybble constants (5 trits)
static constexpr int32_t TRYBBLE_MAX = 121;          // Max 5-trit value
static constexpr int32_t TRYBBLE_MIN = -121;         // Min 5-trit value
static constexpr uint8_t TRYBBLE_BIAS = 121;         // Bias for uint8 storage

// Precomputed powers of 3
static constexpr int32_t POW3[10] = {
    1,      // 3^0
    3,      // 3^1
    9,      // 3^2
    27,     // 3^3
    81,     // 3^4
    243,    // 3^5
    729,    // 3^6
    2187,   // 3^7
    6561,   // 3^8
    19683   // 3^9
};

/**
 * TernaryOps: Core balanced ternary operations
 */
class TernaryOps {
public:
    /**
     * Initialize lookup tables for fast packing/unpacking.
     * Must be called once before using ternary operations.
     */
    static void initialize();

    // ========== Packing Operations ==========

    /**
     * Pack 5 trits into a uint8 (trybble).
     * Uses bias of 121 to map [-121, +121] to [0, 242].
     * 
     * @param trits Array of 5 trits
     * @return Packed uint8 value
     */
    static uint8_t packTrybble(const trit trits[5]);

    /**
     * Pack 10 trits into a uint16 (tryte).
     * Uses split-byte encoding: low byte = trits 0-4, high byte = trits 5-9.
     * 
     * @param trits Array of 10 trits
     * @return Packed uint16 value or TRYTE_ERR if invalid
     */
    static uint16_t packTryte(const trit trits[10]);

    // ========== Unpacking Operations ==========

    /**
     * Unpack a uint8 trybble into 5 trits.
     * Uses 256-entry lookup table for O(1) performance.
     * 
     * @param trybble Packed uint8 value
     * @param trits Output array of 5 trits
     * @return true if valid, false if invalid encoding
     */
    static bool unpackTrybble(uint8_t trybble, trit trits[5]);

    /**
     * Unpack a uint16 tryte into 10 trits.
     * Handles error sentinel detection.
     * 
     * @param tryte Packed uint16 value
     * @param trits Output array of 10 trits
     * @return true if valid, false if ERR sentinel or invalid
     */
    static bool unpackTryte(uint16_t tryte, trit trits[10]);

    // ========== Arithmetic Operations ==========

    /**
     * Add two trits with carry.
     * Implements balanced ternary addition logic.
     * 
     * @param a First trit
     * @param b Second trit
     * @param carry_in Input carry (can be -1, 0, 1)
     * @param carry_out Output carry
     * @return Result trit
     */
    static trit addTrits(trit a, trit b, trit carry_in, trit& carry_out);

    /**
     * Add two trytes.
     * Returns TRYTE_ERR on overflow or if either input is ERR.
     * 
     * @param a First tryte
     * @param b Second tryte
     * @return Sum or TRYTE_ERR
     */
    static uint16_t addTrytes(uint16_t a, uint16_t b);

    /**
     * Negate a tryte (invert all trits).
     * NEG(ERR) = ERR.
     * 
     * @param tryte Input value
     * @return Negated value or TRYTE_ERR
     */
    static uint16_t negateTryte(uint16_t tryte);

    /**
     * Subtract two trytes (a - b = a + NEG(b)).
     * 
     * @param a First tryte
     * @param b Second tryte
     * @return Difference or TRYTE_ERR
     */
    static uint16_t subtractTrytes(uint16_t a, uint16_t b);

    /**
     * Multiply two trytes.
     * Uses shift-and-add algorithm adapted for ternary.
     * 
     * @param a First tryte
     * @param b Second tryte
     * @return Product or TRYTE_ERR on overflow
     */
    static uint16_t multiplyTrytes(uint16_t a, uint16_t b);

    /**
     * Divide two trytes (a / b).
     * Returns TRYTE_ERR on divide-by-zero or if either input is ERR.
     * 
     * @param a Dividend
     * @param b Divisor
     * @return Quotient or TRYTE_ERR
     */
    static uint16_t divideTrytes(uint16_t a, uint16_t b);

    // ========== Conversion Operations ==========

    /**
     * Convert binary integer to tryte.
     * Returns TRYTE_ERR if value out of range [-29524, +29524].
     * 
     * @param value Signed integer value
     * @return Packed tryte or TRYTE_ERR
     */
    static uint16_t binaryToTryte(int32_t value);

    /**
     * Convert tryte to binary integer.
     * Returns 0 if input is TRYTE_ERR.
     * 
     * @param tryte Packed tryte value
     * @return Signed integer value
     */
    static int32_t tryteToBinary(uint16_t tryte);

    // ========== Validation ==========

    /**
     * Check if a uint16 value is a valid tryte encoding.
     * 
     * @param tryte Value to check
     * @return true if valid (not ERR and within valid range)
     */
    static bool isValidTryte(uint16_t tryte) {
        return tryte != TRYTE_ERR && tryte <= TRYTE_VALID_MAX;
    }

    /**
     * Check if a value is the ERR sentinel.
     */
    static bool isTryteError(uint16_t tryte) {
        return tryte == TRYTE_ERR;
    }

private:
    // Lookup table: uint8 -> 5 trits (256 entries)
    static bool lut_initialized;
    static std::array<std::array<trit, 5>, 256> unpack_lut;

    /**
     * Build the unpacking lookup table.
     * Called once by initialize().
     */
    static void buildUnpackLUT();

    /**
     * Helper: Compute value of 5 trits
     */
    static int32_t computeTrybbleValue(const trit trits[5]);
};

} // namespace backend
} // namespace aria

#endif // ARIA_BACKEND_TERNARY_OPS_H

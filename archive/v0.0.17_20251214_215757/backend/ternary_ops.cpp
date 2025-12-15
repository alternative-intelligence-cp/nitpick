/**
 * src/backend/ternary_ops.cpp
 * 
 * Balanced Ternary Arithmetic Implementation
 */

#include "ternary_ops.h"
#include <cmath>

namespace aria {
namespace backend {

// Static member initialization
bool TernaryOps::lut_initialized = false;
std::array<std::array<trit, 5>, 256> TernaryOps::unpack_lut;

void TernaryOps::initialize() {
    if (!lut_initialized) {
        buildUnpackLUT();
        lut_initialized = true;
    }
}

// ========== Packing Implementation ==========

int32_t TernaryOps::computeTrybbleValue(const trit trits[5]) {
    int32_t value = 0;
    for (int i = 0; i < 5; i++) {
        value += trits[i] * POW3[i];
    }
    return value;
}

uint8_t TernaryOps::packTrybble(const trit trits[5]) {
    int32_t value = computeTrybbleValue(trits);
    
    // Apply bias to map [-121, +121] to [0, 242]
    int32_t biased = value + TRYBBLE_BIAS;
    
    // Safety check (should never happen with valid trits)
    if (biased < 0 || biased > 242) {
        return 255;  // Invalid marker
    }
    
    return static_cast<uint8_t>(biased);
}

uint16_t TernaryOps::packTryte(const trit trits[10]) {
    // Pack low byte (trits 0-4)
    trit low_trits[5] = {trits[0], trits[1], trits[2], trits[3], trits[4]};
    uint8_t low_byte = packTrybble(low_trits);
    
    // Pack high byte (trits 5-9)
    trit high_trits[5] = {trits[5], trits[6], trits[7], trits[8], trits[9]};
    uint8_t high_byte = packTrybble(high_trits);
    
    // Check for invalid packing
    if (low_byte == 255 || high_byte == 255) {
        return TRYTE_ERR;
    }
    
    // Combine bytes
    return (static_cast<uint16_t>(high_byte) << 8) | low_byte;
}

// ========== Unpacking Implementation ==========

void TernaryOps::buildUnpackLUT() {
    // For each possible uint8 value
    for (int byte_val = 0; byte_val < 256; byte_val++) {
        // Remove bias
        int32_t value = byte_val - TRYBBLE_BIAS;
        
        // Convert to balanced ternary trits
        trit trits[5];
        int32_t remaining = value;
        
        for (int i = 0; i < 5; i++) {
            // Get trit at position i
            int32_t trit_val = remaining % 3;
            remaining /= 3;
            
            // Adjust for balanced ternary
            if (trit_val == 2) {
                trits[i] = TRIT_NEG;  // -1
                remaining += 1;        // Carry
            } else if (trit_val == -2) {
                trits[i] = TRIT_POS;   // +1
                remaining -= 1;        // Borrow
            } else {
                trits[i] = static_cast<trit>(trit_val);
            }
        }
        
        // Store in LUT
        unpack_lut[byte_val][0] = trits[0];
        unpack_lut[byte_val][1] = trits[1];
        unpack_lut[byte_val][2] = trits[2];
        unpack_lut[byte_val][3] = trits[3];
        unpack_lut[byte_val][4] = trits[4];
    }
}

bool TernaryOps::unpackTrybble(uint8_t trybble, trit trits[5]) {
    if (!lut_initialized) {
        initialize();
    }
    
    // Lookup in LUT
    for (int i = 0; i < 5; i++) {
        trits[i] = unpack_lut[trybble][i];
    }
    
    return true;
}

bool TernaryOps::unpackTryte(uint16_t tryte, trit trits[10]) {
    // Check for error sentinel
    if (tryte == TRYTE_ERR) {
        // Fill with zeros
        for (int i = 0; i < 10; i++) {
            trits[i] = TRIT_ZERO;
        }
        return false;
    }
    
    // Check validity
    if (tryte > TRYTE_VALID_MAX) {
        for (int i = 0; i < 10; i++) {
            trits[i] = TRIT_ZERO;
        }
        return false;
    }
    
    // Extract bytes
    uint8_t low_byte = tryte & 0xFF;
    uint8_t high_byte = (tryte >> 8) & 0xFF;
    
    // Unpack low byte (trits 0-4)
    if (!unpackTrybble(low_byte, &trits[0])) {
        return false;
    }
    
    // Unpack high byte (trits 5-9)
    if (!unpackTrybble(high_byte, &trits[5])) {
        return false;
    }
    
    return true;
}

// ========== Arithmetic Implementation ==========

trit TernaryOps::addTrits(trit a, trit b, trit carry_in, trit& carry_out) {
    // Compute raw sum
    int32_t sum_raw = a + b + carry_in;
    
    // Determine result trit and carry out
    // We want: sum_raw = 3 * carry_out + result
    // where result ∈ {-1, 0, 1}
    
    if (sum_raw >= 2) {
        carry_out = TRIT_POS;     // +1
        return static_cast<trit>(sum_raw - 3);
    } else if (sum_raw <= -2) {
        carry_out = TRIT_NEG;     // -1
        return static_cast<trit>(sum_raw + 3);
    } else {
        carry_out = TRIT_ZERO;
        return static_cast<trit>(sum_raw);
    }
}

uint16_t TernaryOps::addTrytes(uint16_t a, uint16_t b) {
    // Sticky error propagation
    if (a == TRYTE_ERR || b == TRYTE_ERR) {
        return TRYTE_ERR;
    }
    
    // Unpack
    trit trits_a[10], trits_b[10], result_trits[10];
    if (!unpackTryte(a, trits_a) || !unpackTryte(b, trits_b)) {
        return TRYTE_ERR;
    }
    
    // Add trit-by-trit with carry propagation
    trit carry = TRIT_ZERO;
    for (int i = 0; i < 10; i++) {
        result_trits[i] = addTrits(trits_a[i], trits_b[i], carry, carry);
    }
    
    // Check for overflow (carry out of most significant trit)
    if (carry != TRIT_ZERO) {
        return TRYTE_ERR;
    }
    
    // Pack result
    uint16_t result = packTryte(result_trits);
    
    // Verify result is in valid range
    int32_t value = tryteToBinary(result);
    if (value > TRYTE_MAX || value < TRYTE_MIN) {
        return TRYTE_ERR;
    }
    
    return result;
}

uint16_t TernaryOps::negateTryte(uint16_t tryte) {
    // Sticky error
    if (tryte == TRYTE_ERR) {
        return TRYTE_ERR;
    }
    
    // Unpack
    trit trits[10];
    if (!unpackTryte(tryte, trits)) {
        return TRYTE_ERR;
    }
    
    // Invert each trit: 1 → -1, -1 → 1, 0 → 0
    for (int i = 0; i < 10; i++) {
        trits[i] = -trits[i];
    }
    
    // Pack result
    return packTryte(trits);
}

uint16_t TernaryOps::subtractTrytes(uint16_t a, uint16_t b) {
    // a - b = a + (-b)
    uint16_t neg_b = negateTryte(b);
    return addTrytes(a, neg_b);
}

uint16_t TernaryOps::multiplyTrytes(uint16_t a, uint16_t b) {
    // Sticky error
    if (a == TRYTE_ERR || b == TRYTE_ERR) {
        return TRYTE_ERR;
    }
    
    // Convert to binary, multiply, convert back
    int32_t val_a = tryteToBinary(a);
    int32_t val_b = tryteToBinary(b);
    
    int64_t product = static_cast<int64_t>(val_a) * static_cast<int64_t>(val_b);
    
    // Check for overflow
    if (product > TRYTE_MAX || product < TRYTE_MIN) {
        return TRYTE_ERR;
    }
    
    return binaryToTryte(static_cast<int32_t>(product));
}

uint16_t TernaryOps::divideTrytes(uint16_t a, uint16_t b) {
    // Sticky error
    if (a == TRYTE_ERR || b == TRYTE_ERR) {
        return TRYTE_ERR;
    }
    
    // Check for divide by zero
    int32_t val_b = tryteToBinary(b);
    if (val_b == 0) {
        return TRYTE_ERR;
    }
    
    // Convert to binary, divide, convert back
    int32_t val_a = tryteToBinary(a);
    int32_t quotient = val_a / val_b;  // Truncated division
    
    return binaryToTryte(quotient);
}

// ========== Conversion Implementation ==========

uint16_t TernaryOps::binaryToTryte(int32_t value) {
    // Range check
    if (value > TRYTE_MAX || value < TRYTE_MIN) {
        return TRYTE_ERR;
    }
    
    // Convert to balanced ternary
    trit trits[10];
    int32_t remaining = value;
    
    for (int i = 0; i < 10; i++) {
        int32_t trit_val = remaining % 3;
        remaining /= 3;
        
        // Adjust for balanced ternary
        if (trit_val == 2) {
            trits[i] = TRIT_NEG;  // -1
            remaining += 1;        // Carry
        } else if (trit_val == -2) {
            trits[i] = TRIT_POS;   // +1
            remaining -= 1;        // Borrow
        } else {
            trits[i] = static_cast<trit>(trit_val);
        }
    }
    
    return packTryte(trits);
}

int32_t TernaryOps::tryteToBinary(uint16_t tryte) {
    // Check for error sentinel
    if (tryte == TRYTE_ERR) {
        return 0;
    }
    
    // Unpack
    trit trits[10];
    if (!unpackTryte(tryte, trits)) {
        return 0;
    }
    
    // Compute value
    int32_t value = 0;
    for (int i = 0; i < 10; i++) {
        value += trits[i] * POW3[i];
    }
    
    return value;
}

} // namespace backend
} // namespace aria

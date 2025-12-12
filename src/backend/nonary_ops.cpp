/**
 * src/backend/nonary_ops.cpp
 * 
 * Balanced Nonary Arithmetic Implementation
 * Version: 0.0.12
 */

#include "nonary_ops.h"
#include <sstream>
#include <cmath>

namespace aria {
namespace backend {

// ========== Initialization ==========

void NonaryOps::initialize() {
    // No lookup tables needed for biased-radix implementation
    // Could add optimizations later if profiling shows hotspots
}

// ========== Validation Operations ==========

bool NonaryOps::isValidNit(int8_t value) {
    return value >= NIT_MIN && value <= NIT_MAX;
}

bool NonaryOps::isValidNyte(uint16_t packed) {
    return packed <= NYTE_VALID_MAX || packed == NYTE_ERR;
}

bool NonaryOps::isError(uint16_t packed) {
    return packed == NYTE_ERR;
}

// ========== Packing Operations ==========

int32_t NonaryOps::nitsToValue(const nit nits[5]) {
    int32_t value = 0;
    for (int i = 4; i >= 0; i--) {
        value = value * 9 + nits[i];
    }
    return value;
}

uint16_t NonaryOps::packNyte(const nit nits[5]) {
    // Validate all nits
    for (int i = 0; i < 5; i++) {
        if (!isValidNit(nits[i])) {
            return NYTE_ERR;
        }
    }
    
    // Convert nits to value using Horner's method
    int32_t value = nitsToValue(nits);
    
    // Apply bias
    return packValue(value);
}

uint16_t NonaryOps::packValue(int32_t value) {
    // Check range
    if (value < NYTE_MIN || value > NYTE_MAX) {
        return NYTE_ERR;
    }
    
    // Apply bias: stored = value + 29,524
    uint16_t packed = static_cast<uint16_t>(value + NYTE_BIAS);
    return packed;
}

// ========== Unpacking Operations ==========

void NonaryOps::valueToNits(int32_t value, nit nits[5]) {
    // Extract digits using division/modulo
    // Balanced nonary: divide by 9, round towards nearest
    int32_t temp = value;
    
    for (int i = 0; i < 5; i++) {
        // Standard balanced digit extraction
        // Compute remainder in range [-4, 4]
        int32_t digit = temp % 9;
        
        // Adjust to balanced range
        if (digit > 4) {
            digit -= 9;
            temp = temp / 9 + 1;
        } else if (digit < -4) {
            digit += 9;
            temp = temp / 9 - 1;
        } else {
            temp = temp / 9;
        }
        
        nits[i] = static_cast<nit>(digit);
    }
}

bool NonaryOps::unpackNyte(uint16_t packed, nit nits[5]) {
    // Check for error sentinel
    if (isError(packed)) {
        return false;
    }
    
    // Check valid range
    if (packed > NYTE_VALID_MAX) {
        return false;
    }
    
    // Remove bias: value = stored - 29,524
    int32_t value = static_cast<int32_t>(packed) - NYTE_BIAS;
    
    // Convert to nits
    valueToNits(value, nits);
    
    return true;
}

int32_t NonaryOps::unpackValue(uint16_t packed) {
    if (isError(packed) || packed > NYTE_VALID_MAX) {
        return 0;  // Return zero for invalid values
    }
    
    return static_cast<int32_t>(packed) - NYTE_BIAS;
}

// ========== Arithmetic Operations ==========

uint16_t NonaryOps::add(uint16_t a, uint16_t b) {
    // Sticky error propagation
    if (isError(a) || isError(b)) {
        return NYTE_ERR;
    }
    
    // Validate inputs
    if (!isValidNyte(a) || !isValidNyte(b)) {
        return NYTE_ERR;
    }
    
    // Unpack to native integers
    int32_t val_a = unpackValue(a);
    int32_t val_b = unpackValue(b);
    
    // Perform addition
    int32_t result = val_a + val_b;
    
    // Pack result (includes range check)
    return packValue(result);
}

uint16_t NonaryOps::subtract(uint16_t a, uint16_t b) {
    // Sticky error propagation
    if (isError(a) || isError(b)) {
        return NYTE_ERR;
    }
    
    // Validate inputs
    if (!isValidNyte(a) || !isValidNyte(b)) {
        return NYTE_ERR;
    }
    
    // Unpack to native integers
    int32_t val_a = unpackValue(a);
    int32_t val_b = unpackValue(b);
    
    // Perform subtraction
    int32_t result = val_a - val_b;
    
    // Pack result (includes range check)
    return packValue(result);
}

uint16_t NonaryOps::multiply(uint16_t a, uint16_t b) {
    // Sticky error propagation
    if (isError(a) || isError(b)) {
        return NYTE_ERR;
    }
    
    // Validate inputs
    if (!isValidNyte(a) || !isValidNyte(b)) {
        return NYTE_ERR;
    }
    
    // Unpack to native integers
    int32_t val_a = unpackValue(a);
    int32_t val_b = unpackValue(b);
    
    // Perform multiplication (use int64 to prevent intermediate overflow)
    int64_t result = static_cast<int64_t>(val_a) * static_cast<int64_t>(val_b);
    
    // Check range before packing
    if (result < NYTE_MIN || result > NYTE_MAX) {
        return NYTE_ERR;
    }
    
    // Pack result
    return packValue(static_cast<int32_t>(result));
}

uint16_t NonaryOps::divide(uint16_t a, uint16_t b) {
    // Sticky error propagation
    if (isError(a) || isError(b)) {
        return NYTE_ERR;
    }
    
    // Validate inputs
    if (!isValidNyte(a) || !isValidNyte(b)) {
        return NYTE_ERR;
    }
    
    // Unpack to native integers
    int32_t val_a = unpackValue(a);
    int32_t val_b = unpackValue(b);
    
    // Check for division by zero
    if (val_b == 0) {
        return NYTE_ERR;
    }
    
    // Perform division (truncates towards zero)
    int32_t result = val_a / val_b;
    
    // Pack result (division always produces valid range)
    return packValue(result);
}

uint16_t NonaryOps::modulo(uint16_t a, uint16_t b) {
    // Sticky error propagation
    if (isError(a) || isError(b)) {
        return NYTE_ERR;
    }
    
    // Validate inputs
    if (!isValidNyte(a) || !isValidNyte(b)) {
        return NYTE_ERR;
    }
    
    // Unpack to native integers
    int32_t val_a = unpackValue(a);
    int32_t val_b = unpackValue(b);
    
    // Check for division by zero
    if (val_b == 0) {
        return NYTE_ERR;
    }
    
    // Perform modulo
    int32_t result = val_a % val_b;
    
    // Pack result
    return packValue(result);
}

uint16_t NonaryOps::negate(uint16_t a) {
    // Sticky error propagation
    if (isError(a)) {
        return NYTE_ERR;
    }
    
    // Validate input
    if (!isValidNyte(a)) {
        return NYTE_ERR;
    }
    
    // Unpack
    int32_t val = unpackValue(a);
    
    // Negate
    int32_t result = -val;
    
    // Pack result
    return packValue(result);
}

// ========== Comparison Operations ==========

int NonaryOps::compare(uint16_t a, uint16_t b) {
    // Error values compare as equal to themselves
    if (isError(a) && isError(b)) {
        return 0;
    }
    
    // Error is "greater" than valid values for sorting purposes
    if (isError(a)) return 1;
    if (isError(b)) return -1;
    
    // Invalid values
    if (!isValidNyte(a) || !isValidNyte(b)) {
        return 0;  // Undefined behavior for invalid values
    }
    
    // Direct comparison works due to monotonic biased encoding
    // If A < B logically, then (A + bias) < (B + bias)
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

bool NonaryOps::equals(uint16_t a, uint16_t b) {
    return a == b;
}

bool NonaryOps::lessThan(uint16_t a, uint16_t b) {
    // Handle error cases
    if (isError(a) || isError(b)) {
        return false;
    }
    
    // Direct comparison works due to monotonic encoding
    return a < b;
}

// ========== Conversion Operations ==========

std::string NonaryOps::nitToString(int8_t nit_val) {
    if (nit_val == 0) return "0";
    if (nit_val > 0) return std::to_string(nit_val);
    
    // Negative values: use T notation for -1
    if (nit_val == -1) return "T";
    return std::to_string(nit_val);
}

std::string NonaryOps::toString(uint16_t packed) {
    if (isError(packed)) {
        return "ERR";
    }
    
    if (!isValidNyte(packed)) {
        return "INVALID";
    }
    
    // Unpack to nits
    nit nits[5];
    if (!unpackNyte(packed, nits)) {
        return "ERR";
    }
    
    // Format: nyte[d4 d3 d2 d1 d0]
    std::ostringstream oss;
    oss << "nyte[";
    for (int i = 4; i >= 0; i--) {
        oss << nitToString(nits[i]);
        if (i > 0) oss << " ";
    }
    oss << "]";
    
    // Also show decimal value
    int32_t value = unpackValue(packed);
    oss << " = " << value;
    
    return oss.str();
}

} // namespace backend
} // namespace aria

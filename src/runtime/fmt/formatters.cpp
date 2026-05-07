/**
 * Aria Type Formatters - Runtime String Formatting Implementation
 *
 * Implements type-aware string formatting for Aria's advanced type system.
 * See include/runtime/fmt/formatters.h for API documentation.
 */

#include "runtime/fmt/formatters.h"
#include "runtime/stdlib.h"
#include "runtime/fix256.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>

// ============================================================================
// Internal Helpers
// ============================================================================

namespace {

/**
 * Allocate AriaString on heap with given C string content
 * Uses malloc for simplicity (strings are typically short-lived in formatting)
 */
AriaString* alloc_aria_string(const char* cstr) {
    if (!cstr) return nullptr;

    size_t len = strlen(cstr);

    // Allocate string data
    char* buf = (char*)malloc(len + 1);
    if (!buf) return nullptr;
    memcpy(buf, cstr, len);
    buf[len] = '\0';

    // Allocate AriaString struct
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
 * Allocate AriaString from std::string
 */
AriaString* alloc_aria_string(const std::string& s) {
    return alloc_aria_string(s.c_str());
}

// ============================================================================
// Multi-Precision Division Helper (for LBIM)
// ============================================================================

/**
 * Divide multi-limb integer by 10^19 (largest power of 10 fitting in uint64)
 * Returns the remainder and modifies limbs in-place to be the quotient.
 *
 * This uses __int128 for intermediate calculations to handle 128-bit
 * multiplication/division correctly.
 */
uint64_t div_mod_1e19(std::vector<uint64_t>& limbs) {
    const uint64_t DIVISOR = 10000000000000000000ULL; // 10^19
    uint64_t remainder = 0;

    // Standard long division, processing most significant limb first
    for (int i = static_cast<int>(limbs.size()) - 1; i >= 0; i--) {
        // Combine previous remainder with current limb
        // Using __int128 to hold 128-bit intermediate value
        unsigned __int128 current = ((unsigned __int128)remainder << 64) | limbs[i];

        limbs[i] = static_cast<uint64_t>(current / DIVISOR);
        remainder = static_cast<uint64_t>(current % DIVISOR);
    }

    return remainder;
}

/**
 * Check if all limbs are zero
 */
bool is_zero(const std::vector<uint64_t>& limbs) {
    for (uint64_t x : limbs) {
        if (x != 0) return false;
    }
    return true;
}

/**
 * Two's complement negation of limb array
 */
void negate_limbs(std::vector<uint64_t>& limbs) {
    uint64_t carry = 1;
    for (auto& limb : limbs) {
        limb = ~limb;
        unsigned __int128 res = static_cast<unsigned __int128>(limb) + carry;
        limb = static_cast<uint64_t>(res);
        carry = static_cast<uint64_t>(res >> 64);
    }
}

/**
 * Generic formatter for any width LBIM integer
 * @param limbs Array of 64-bit limbs (little-endian: limbs[0] is LSB)
 * @param is_signed Whether to treat as signed (check MSB for negative)
 */
AriaString* format_large_int(std::vector<uint64_t> limbs, bool is_signed) {
    if (is_zero(limbs)) {
        return alloc_aria_string("0");
    }

    bool negative = false;
    if (is_signed && !limbs.empty()) {
        // Check sign bit of the most significant limb
        if (limbs.back() & (1ULL << 63)) {
            negative = true;
            negate_limbs(limbs);
        }
    }

    std::string result;

    // Extract 19-digit chunks until the number is zero
    while (!is_zero(limbs)) {
        uint64_t chunk = div_mod_1e19(limbs);

        if (is_zero(limbs)) {
            // Last chunk (most significant) - no padding needed
            result = std::to_string(chunk) + result;
        } else {
            // Inner chunk - must be zero-padded to 19 digits
            std::ostringstream ss;
            ss << std::setfill('0') << std::setw(19) << chunk;
            result = ss.str() + result;
        }
    }

    if (negative) {
        result = "-" + result;
    }

    return alloc_aria_string(result);
}

} // anonymous namespace

// ============================================================================
// TBB Formatters (Sentinel Aware)
// ============================================================================

extern "C" {

AriaString* npk_format_tbb8(int8_t val) {
    // 0x80 (-128) is the ERR sentinel
    if (val == static_cast<int8_t>(0x80)) {
        return alloc_aria_string("ERR");
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", val);
    return alloc_aria_string(buf);
}

AriaString* npk_format_tbb16(int16_t val) {
    // 0x8000 (-32768) is the ERR sentinel
    if (val == static_cast<int16_t>(0x8000)) {
        return alloc_aria_string("ERR");
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", val);
    return alloc_aria_string(buf);
}

AriaString* npk_format_tbb32(int32_t val) {
    // 0x80000000 (INT32_MIN) is the ERR sentinel
    if (val == static_cast<int32_t>(0x80000000)) {
        return alloc_aria_string("ERR");
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", val);
    return alloc_aria_string(buf);
}

AriaString* npk_format_tbb64(int64_t val) {
    // 0x8000000000000000 (INT64_MIN) is the ERR sentinel
    if (val == static_cast<int64_t>(0x8000000000000000ULL)) {
        return alloc_aria_string("ERR");
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(val));
    return alloc_aria_string(buf);
}

// ============================================================================
// Plain Integer/Float to String (for template literals)
// ============================================================================

/**
 * Convert int64 to string in a deterministic, locale-independent way
 * Signature: int64_t npk_int64_to_str(int64_t value, char* buffer)
 * @param value The int64 value to convert
 * @param buffer Pre-allocated buffer (must be at least 24 bytes for "-9223372036854775808\0")
 * @return Length of the string (excluding null terminator)
 */
int64_t npk_int64_to_str(int64_t value, char* buffer) {
    if (!buffer) return 0;
    
    char* p = buffer;
    
    // Handle zero explicitly
    if (value == 0) {
        *p++ = '0';
        *p = '\0';
        return 1;
    }
    
    // Handle sign
    uint64_t uval;
    if (value < 0) {
        *p++ = '-';
        // Special case for INT64_MIN which can't be negated directly
        if (value == static_cast<int64_t>(0x8000000000000000ULL)) {
            uval = 0x8000000000000000ULL;
        } else {
            uval = static_cast<uint64_t>(-value);
        }
    } else {
        uval = static_cast<uint64_t>(value);
    }
    
    // Build digits in reverse
    char* digit_start = p;
    while (uval > 0) {
        *p++ = '0' + (uval % 10);
        uval /= 10;
    }
    
    // Reverse the digits
    char* digit_end = p - 1;
    while (digit_start < digit_end) {
        char tmp = *digit_start;
        *digit_start = *digit_end;
        *digit_end = tmp;
        digit_start++;
        digit_end--;
    }
    
    *p = '\0';
    return static_cast<int64_t>(p - buffer);
}

/**
 * Convert flt64 to string in a deterministic, locale-independent way
 * Signature: int64_t npk_flt64_to_str(double value, char* buffer)
 * @param value The double value to convert
 * @param buffer Pre-allocated buffer (must be at least 64 bytes)
 * @return Length of the string (excluding null terminator)
 */
int64_t npk_flt64_to_str(double value, char* buffer) {
    if (!buffer) return 0;
    
    // Use snprintf with fixed precision for deterministic output
    // %g uses shortest representation, removing trailing zeros
    int len = snprintf(buffer, 64, "%.16g", value);
    
    if (len < 0 || len >= 64) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }
    
    return static_cast<int64_t>(len);
}

// ============================================================================
// LBIM Large Integer Formatters
// ============================================================================

AriaString* npk_format_int128(const npk_int128_t* val) {
    if (!val) return alloc_aria_string("null");
    std::vector<uint64_t> limbs = {val->limbs[0], val->limbs[1]};
    return format_large_int(limbs, true);
}

AriaString* npk_format_uint128(const npk_int128_t* val) {
    if (!val) return alloc_aria_string("null");
    std::vector<uint64_t> limbs = {val->limbs[0], val->limbs[1]};
    return format_large_int(limbs, false);
}

AriaString* npk_format_int256(const npk_int256_t* val) {
    if (!val) return alloc_aria_string("null");
    std::vector<uint64_t> limbs = {val->limbs[0], val->limbs[1], val->limbs[2], val->limbs[3]};
    return format_large_int(limbs, true);
}

AriaString* npk_format_uint256(const npk_int256_t* val) {
    if (!val) return alloc_aria_string("null");
    std::vector<uint64_t> limbs = {val->limbs[0], val->limbs[1], val->limbs[2], val->limbs[3]};
    return format_large_int(limbs, false);
}

AriaString* npk_format_int512(const npk_int512_t* val) {
    if (!val) return alloc_aria_string("null");
    std::vector<uint64_t> limbs = {
        val->limbs[0], val->limbs[1], val->limbs[2], val->limbs[3],
        val->limbs[4], val->limbs[5], val->limbs[6], val->limbs[7]
    };
    return format_large_int(limbs, true);
}

AriaString* npk_format_uint512(const npk_int512_t* val) {
    if (!val) return alloc_aria_string("null");
    std::vector<uint64_t> limbs = {
        val->limbs[0], val->limbs[1], val->limbs[2], val->limbs[3],
        val->limbs[4], val->limbs[5], val->limbs[6], val->limbs[7]
    };
    return format_large_int(limbs, false);
}

AriaString* npk_format_int1024(const npk_int1024_t* val) {
    if (!val) return alloc_aria_string("null");
    
    // Check for ERR sentinel: high limb = 0x8000000000000000, all others = 0
    if (val->limbs[15] == 0x8000000000000000ULL) {
        bool is_err = true;
        for (int i = 0; i < 15; ++i) {
            if (val->limbs[i] != 0) {
                is_err = false;
                break;
            }
        }
        if (is_err) return alloc_aria_string("ERR");
    }
    
    std::vector<uint64_t> limbs = {
        val->limbs[0], val->limbs[1], val->limbs[2], val->limbs[3],
        val->limbs[4], val->limbs[5], val->limbs[6], val->limbs[7],
        val->limbs[8], val->limbs[9], val->limbs[10], val->limbs[11],
        val->limbs[12], val->limbs[13], val->limbs[14], val->limbs[15]
    };
    return format_large_int(limbs, true);
}

AriaString* npk_format_uint1024(const npk_int1024_t* val) {
    if (!val) return alloc_aria_string("null");
    std::vector<uint64_t> limbs = {
        val->limbs[0], val->limbs[1], val->limbs[2], val->limbs[3],
        val->limbs[4], val->limbs[5], val->limbs[6], val->limbs[7],
        val->limbs[8], val->limbs[9], val->limbs[10], val->limbs[11],
        val->limbs[12], val->limbs[13], val->limbs[14], val->limbs[15]
    };
    return format_large_int(limbs, false);
}

AriaString* npk_format_fix256(const npk_fix256_t* val) {
    if (!val) return alloc_aria_string("null");
    
    // Check for ERR sentinel: limbs[3] = 0x8000000000000000, all others = 0
    if (val->limbs[3] == 0x8000000000000000ULL &&
        val->limbs[2] == 0 && val->limbs[1] == 0 && val->limbs[0] == 0) {
        return alloc_aria_string("ERR");
    }
    
    // Q128.128 format: integer part in limbs[2-3], fractional in limbs[0-1]
    // For simplicity, format as: integer_part.fractional_part
    // This is a basic implementation - full precision would require arbitrary precision
    
    // Extract sign from high bit of limbs[3]
    bool is_negative = (val->limbs[3] & 0x8000000000000000ULL) != 0;
    
    // Work with absolute value for formatting
    npk_fix256_t abs_val = *val;
    if (is_negative) {
        // Two's complement negation: invert and add 1
        for (int i = 0; i < 4; ++i) {
            abs_val.limbs[i] = ~abs_val.limbs[i];
        }
        uint64_t carry = 1;
        for (int i = 0; i < 4; ++i) {
            unsigned __int128 sum = (unsigned __int128)abs_val.limbs[i] + carry;
            abs_val.limbs[i] = (uint64_t)sum;
            carry = (uint64_t)(sum >> 64);
        }
    }
    
    // Extract integer part (limbs 2-3)
    unsigned __int128 integer_part = 
        ((unsigned __int128)abs_val.limbs[3] << 64) | abs_val.limbs[2];
    
    // Extract fractional part (limbs 0-1)
    unsigned __int128 frac_part = 
        ((unsigned __int128)abs_val.limbs[1] << 64) | abs_val.limbs[0];
    
    // Convert integer part to string
    std::string result;
    if (is_negative) result += "-";
    
    if (integer_part == 0) {
        result += "0";
    } else {
        std::string int_str;
        unsigned __int128 temp = integer_part;
        while (temp > 0) {
            int_str += '0' + (temp % 10);
            temp /= 10;
        }
        std::reverse(int_str.begin(), int_str.end());
        result += int_str;
    }
    
    // Add decimal point
    result += ".";
    
    // Convert fractional part (simplified - shows ~6 decimal places)
    // Full precision would need arbitrary precision decimal conversion
    if (frac_part == 0) {
        result += "0";
    } else {
        // Multiply by 10^6 and divide by 2^128 to get 6 decimal digits
        unsigned __int128 scaled = frac_part;
        for (int i = 0; i < 6; ++i) {
            scaled *= 10;
            uint64_t digit = (uint64_t)(scaled >> 128);
            result += '0' + digit;
            scaled &= ((unsigned __int128)-1) >> 0;  // Keep only lower 128 bits
        }
    }
    
    return alloc_aria_string(result.c_str());
}

// ============================================================================
// Exotic Type Formatters (Balanced Ternary/Nonary)
// ============================================================================

AriaString* npk_format_trit(int8_t val) {
    // Check for ERR sentinel (trit uses tbb8 representation)
    if (val == -128) return alloc_aria_string("ERR");
    
    if (val == -1) return alloc_aria_string("T");
    if (val == 0) return alloc_aria_string("0");
    if (val == 1) return alloc_aria_string("1");
    // Invalid trit value
    return alloc_aria_string("?");
}

AriaString* npk_format_tryte(uint16_t val) {
    // Check for ERR sentinel (0xFFFF)
    if (val == 0xFFFF) return alloc_aria_string("ERR");
    
    // Convert from biased representation to balanced value
    // Bias = 29,524, so balanced_value = stored_value - 29524
    int32_t balanced = (int32_t)val - 29524;
    
    if (balanced == 0) return alloc_aria_string("0");

    std::string result;
    int32_t work = balanced;
    bool is_negative = (work < 0);
    if (is_negative) work = -work; // Work with absolute value

    // Balanced Ternary Conversion
    // Digits: T (-1), 0, 1
    while (work != 0) {
        int rem = work % 3;
        work /= 3;

        if (rem == 0) {
            result = "0" + result;
        } else if (rem == 1) {
            if (is_negative) {
                result = "T" + result;
            } else {
                result = "1" + result;
            }
        } else if (rem == 2) {
            // 2 becomes "T" with carry
            if (is_negative) {
                result = "1" + result;
            } else {
                result = "T" + result;
            }
            work++;
        }
    }

    return alloc_aria_string(result);
}

AriaString* npk_format_nit(int8_t val) {
    // Balanced nonary digit: -4 to 4
    // Positive: 0, 1, 2, 3, 4
    // Negative: A (-1), B (-2), C (-3), D (-4)
    switch (val) {
        case 0: return alloc_aria_string("0");
        case 1: return alloc_aria_string("1");
        case 2: return alloc_aria_string("2");
        case 3: return alloc_aria_string("3");
        case 4: return alloc_aria_string("4");
        case -1: return alloc_aria_string("A");
        case -2: return alloc_aria_string("B");
        case -3: return alloc_aria_string("C");
        case -4: return alloc_aria_string("D");
        default: return alloc_aria_string("?");
    }
}

AriaString* npk_format_nyte(uint16_t val) {
    // Check for ERR sentinel (0xFFFF)
    if (val == 0xFFFF) return alloc_aria_string("ERR");
    
    // Convert from biased representation to balanced value
    // Bias = 29,524, so balanced_value = stored_value - 29524
    int32_t balanced = (int32_t)val - 29524;
    
    if (balanced == 0) return alloc_aria_string("0");

    std::string result;
    int32_t work = balanced;
    bool is_negative = (work < 0);
    if (is_negative) work = -work; // Work with absolute value

    // Balanced Nonary Conversion (Base 9)
    // Digits: D (-4), C (-3), B (-2), A (-1), 0, 1, 2, 3, 4
    while (work != 0) {
        int rem = work % 9;
        work /= 9;

        // Adjust remainder to be in [-4, 4]
        if (rem > 4) {
            rem -= 9;
            work++;
        }

        // Map remainder to character
        char c;
        if (is_negative && rem != 0) {
            // Negate the digit for negative numbers
            rem = -rem;
            if (rem > 4) {
                rem -= 9;
                work++;
            }
        }
        
        if (rem >= 0) {
            c = '0' + rem;
        } else {
            c = 'A' + (-rem - 1); // -1->A, -2->B, -3->C, -4->D
        }

        result = c + result;
    }

    return alloc_aria_string(result);
}

// ============================================================================
// Standard Type Formatters
// ============================================================================

AriaString* npk_format_int64(int64_t val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(val));
    return alloc_aria_string(buf);
}

AriaString* npk_format_float64(double val) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", val);
    return alloc_aria_string(buf);
}

} // extern "C"

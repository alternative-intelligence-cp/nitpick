#include "semantic/literal_converter.h"
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Error.h>
#include <algorithm>
#include <cctype>

namespace npk {
namespace semantic {

using FloatPrecision = FloatPrecision;  // Already defined in header

// ============================================================================
// Float Literal Conversion
// ============================================================================

std::optional<llvm::APFloat> LiteralConverter::convertFloatLiteral(
    const std::string& raw_text,
    FloatPrecision precision
) {
    // Clean the literal (remove underscores, validate)
    std::string cleaned = cleanLiteral(raw_text);
    
    // Get appropriate LLVM float semantics
    const llvm::fltSemantics& semantics = getFloatSemantics(precision);
    
    // Check for special values (inf, nan)
    auto special = handleSpecialFloat(cleaned, semantics);
    if (special.has_value()) {
        return special;
    }
    
    // Convert string to APFloat
    // APFloat constructor accepts StringRef and semantics
    llvm::APFloat result(semantics);
    auto expectedStatus = result.convertFromString(
        llvm::StringRef(cleaned),
        llvm::APFloat::rmNearestTiesToEven
    );
    
    // In LLVM 20.x, convertFromString returns Expected<opStatus>
    if (!expectedStatus) {
        // Parsing failed
        return std::nullopt;
    }
    
    llvm::APFloat::opStatus status = *expectedStatus;
    
    // Check conversion status
    if (status == llvm::APFloat::opInvalidOp) {
        // Parsing failed
        return std::nullopt;
    }
    
    // Status can be:
    // - opOK: exact conversion
    // - opInexact: rounded (expected for many literals)
    // - opOverflow: value too large for precision
    // - opUnderflow: value too small (rounds to zero)
    // All of these are acceptable except opInvalidOp
    
    return result;
}

// ============================================================================
// Integer Literal Conversion
// ============================================================================

std::optional<llvm::APInt> LiteralConverter::convertIntLiteral(
    const std::string& raw_text,
    unsigned bit_width,
    bool is_signed
) {
    // Clean the literal (remove underscores)
    std::string cleaned = cleanLiteral(raw_text);
    
    // Determine radix (base)
    unsigned radix = 10;  // Default decimal
    size_t start_pos = 0;
    
    // Check for hex prefix (0x)
    if (cleaned.size() > 2 && cleaned[0] == '0' && 
        (cleaned[1] == 'x' || cleaned[1] == 'X')) {
        radix = 16;
        start_pos = 2;
    }
    // Check for binary prefix (0b)
    else if (cleaned.size() > 2 && cleaned[0] == '0' && 
             (cleaned[1] == 'b' || cleaned[1] == 'B')) {
        radix = 2;
        start_pos = 2;
    }
    // Check for octal prefix (0o or leading 0)
    else if (cleaned.size() > 2 && cleaned[0] == '0' && 
             (cleaned[1] == 'o' || cleaned[1] == 'O')) {
        radix = 8;
        start_pos = 2;
    }
    else if (cleaned.size() > 1 && cleaned[0] == '0' && std::isdigit(cleaned[1])) {
        radix = 8;
        start_pos = 0;  // Keep the leading 0 for octal
    }
    
    // Extract the numeric part
    std::string numeric_part = cleaned.substr(start_pos);
    
    // Convert using APInt constructor
    // APInt(unsigned numBits, StringRef str, uint8_t radix)
    llvm::APInt result(bit_width, llvm::StringRef(numeric_part), radix);
    
    // Check if the value fits in the specified bit width
    // APInt constructor doesn't fail, it just truncates/wraps
    // We need to verify the conversion was valid
    
    // For signed integers, check if value fits in bit_width-1 bits
    if (is_signed) {
        // Check if value requires more bits than available
        // Convert back to string and compare
        llvm::SmallString<64> str;
        result.toStringUnsigned(str, radix);
        
        // If the string representation changed significantly, overflow occurred
        // This is a simple check - more sophisticated validation possible
        if (str.str() != numeric_part && result.isNegative()) {
            return std::nullopt;  // Overflow
        }
    }
    
    return result;
}

// ============================================================================
// Validation
// ============================================================================

bool LiteralConverter::validateLiteral(const std::string& raw_text, bool is_float) {
    if (raw_text.empty()) {
        return false;
    }
    
    std::string cleaned = cleanLiteral(raw_text);
    
    if (is_float) {
        // Float validation: check for valid float format
        // Valid: digits, optional decimal point, optional exponent
        bool has_digit = false;
        bool has_decimal = false;
        bool has_exponent = false;
        
        for (size_t i = 0; i < cleaned.size(); ++i) {
            char c = cleaned[i];
            
            if (std::isdigit(c)) {
                has_digit = true;
            } else if (c == '.') {
                if (has_decimal || has_exponent) {
                    return false;  // Multiple decimals or decimal after exponent
                }
                has_decimal = true;
            } else if (c == 'e' || c == 'E') {
                if (!has_digit || has_exponent) {
                    return false;  // Exponent without digit or multiple exponents
                }
                has_exponent = true;
                has_digit = false;  // Reset to check for digits after exponent
            } else if (c == '+' || c == '-') {
                // Valid only at start or after exponent marker
                if (i != 0 && cleaned[i-1] != 'e' && cleaned[i-1] != 'E') {
                    return false;
                }
            } else {
                return false;  // Invalid character
            }
        }
        
        return has_digit;
    } else {
        // Integer validation
        // Check for valid prefix and numeric content
        size_t start = 0;
        
        // Handle sign
        if (cleaned[0] == '+' || cleaned[0] == '-') {
            start = 1;
        }
        
        // Check for prefix
        if (cleaned.size() > start + 2) {
            if (cleaned[start] == '0') {
                char prefix = cleaned[start + 1];
                if (prefix == 'x' || prefix == 'X' || 
                    prefix == 'b' || prefix == 'B' ||
                    prefix == 'o' || prefix == 'O') {
                    start += 2;
                }
            }
        }
        
        // Validate remaining characters are valid for the base
        if (start >= cleaned.size()) {
            return false;  // No digits after prefix
        }
        
        for (size_t i = start; i < cleaned.size(); ++i) {
            if (!std::isalnum(cleaned[i])) {
                return false;
            }
        }
        
        return true;
    }
}

// ============================================================================
// Float Semantics
// ============================================================================

const llvm::fltSemantics& LiteralConverter::getFloatSemantics(FloatPrecision precision) {
    switch (precision) {
        case FloatPrecision::FLT32:
            return llvm::APFloat::IEEEsingle();  // 32-bit float
        
        case FloatPrecision::FLT64:
            return llvm::APFloat::IEEEdouble();  // 64-bit double
        
        case FloatPrecision::FLT128:
            return llvm::APFloat::IEEEquad();    // 128-bit quadruple precision
        
        case FloatPrecision::FLT256:
            // FLT256: Use PPCDoubleDouble (106-bit mantissa via double-double pair).
            // True 256-bit IEEE requires MPFR; PPCDoubleDouble is the highest
            // precision available in LLVM's APFloat beyond quad.
            return llvm::APFloat::PPCDoubleDouble();
        
        case FloatPrecision::FLT512:
            // FLT512: No LLVM APFloat semantics above 128-bit IEEE.
            // Use IEEEquad as ceiling until MPFR soft-float integration (Phase 4).
            return llvm::APFloat::IEEEquad();
        
        default:
            return llvm::APFloat::IEEEdouble();
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string LiteralConverter::cleanLiteral(const std::string& raw_text) {
    std::string cleaned;
    cleaned.reserve(raw_text.size());
    
    // Remove underscores (used for readability: 1_000_000)
    for (char c : raw_text) {
        if (c != '_') {
            cleaned += c;
        }
    }
    
    return cleaned;
}

std::optional<llvm::APFloat> LiteralConverter::handleSpecialFloat(
    const std::string& raw_text,
    const llvm::fltSemantics& semantics
) {
    std::string lower = raw_text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "inf" || lower == "+inf" || lower == "infinity") {
        return llvm::APFloat::getInf(semantics, false);  // Positive infinity
    }
    
    if (lower == "-inf" || lower == "-infinity") {
        return llvm::APFloat::getInf(semantics, true);   // Negative infinity
    }
    
    if (lower == "nan") {
        return llvm::APFloat::getNaN(semantics, false);  // NaN
    }
    
    return std::nullopt;  // Not a special value
}

} // namespace semantic
} // namespace npk

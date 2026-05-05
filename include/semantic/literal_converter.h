#ifndef ARIA_LITERAL_CONVERTER_H
#define ARIA_LITERAL_CONVERTER_H

#include <string>
#include <optional>
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>

namespace npk {
namespace semantic {

// Precision enums for clarity (defined outside class for forward declaration)
enum class FloatPrecision {
    FLT32,   // IEEE 754 single (24-bit mantissa)
    FLT64,   // IEEE 754 double (53-bit mantissa)
    FLT128,  // IEEE 754 quadruple (113-bit mantissa)
    FLT256,  // Future: requires MPFR
    FLT512   // Future: requires MPFR
};

/**
 * Literal Converter - Converts raw literal strings to high-precision values
 * 
 * This module solves the "Double Bottleneck" where std::stod() truncates
 * float precision to 53 bits. By storing raw strings from the lexer and
 * deferring conversion to this module, we can use LLVM's APFloat/APInt
 * for arbitrary precision conversion.
 * 
 * Architecture: "String-as-Truth"
 * - Lexer: Stores raw string (no conversion)
 * - Parser: Transfers raw string to AST
 * - Type Checker: Calls this converter when type is known
 * - IR Generator: Uses APFloat/APInt values
 */
class LiteralConverter {
public:
    /**
     * Convert raw float literal string to APFloat
     * 
     * @param raw_text Raw literal string (e.g., "3.141592653589793238462643383279502884197")
     * @param precision Target precision (flt32, flt64, flt128, etc.)
     * @return APFloat value, or empty if parsing fails
     * 
     * Examples:
     * - "3.14" + flt32 → APFloat(IEEEsingle, "3.14")
     * - "3.141592653589793238462643383279502884197" + flt128 → APFloat(IEEEquad, "3.141...")
     */
    static std::optional<llvm::APFloat> convertFloatLiteral(
        const std::string& raw_text,
        FloatPrecision precision
    );
    
    /**
     * Convert raw integer literal string to APInt
     * 
     * @param raw_text Raw literal string (e.g., "9223372036854775808")
     * @param bit_width Target bit width (64, 128, 256, 512)
     * @param is_signed Whether to treat as signed integer
     * @return APInt value, or empty if parsing fails
     * 
     * Examples:
     * - "42" + 64 bits → APInt(64, "42")
     * - "9223372036854775808" + 128 bits → APInt(128, "9223372036854775808")
     */
    static std::optional<llvm::APInt> convertIntLiteral(
        const std::string& raw_text,
        unsigned bit_width,
        bool is_signed = true
    );
    
    /**
     * Validate that a raw string is a valid numeric literal
     * 
     * @param raw_text Raw literal string to validate
     * @param is_float true for float, false for integer
     * @return true if valid, false otherwise
     */
    static bool validateLiteral(const std::string& raw_text, bool is_float);

    /**
     * Get the appropriate LLVM float semantics for a given precision
     * 
     * @param precision Float precision (flt32/64/128/256/512)
     * @return LLVM APFloat semantics
     */
    static const llvm::fltSemantics& getFloatSemantics(FloatPrecision precision);

private:
    /**
     * Clean a raw literal string for parsing
     * - Remove underscores (e.g., "1_000_000" → "1000000")
     * - Validate format
     * 
     * @param raw_text Raw literal string
     * @return Cleaned string ready for parsing
     */
    static std::string cleanLiteral(const std::string& raw_text);
    
    /**
     * Handle special float values (inf, nan)
     * 
     * @param raw_text Raw literal string
     * @param semantics Target float semantics
     * @return APFloat for special value, or empty if not special
     */
    static std::optional<llvm::APFloat> handleSpecialFloat(
        const std::string& raw_text,
        const llvm::fltSemantics& semantics
    );
};

} // namespace semantic
} // namespace npk

#endif // ARIA_LITERAL_CONVERTER_H

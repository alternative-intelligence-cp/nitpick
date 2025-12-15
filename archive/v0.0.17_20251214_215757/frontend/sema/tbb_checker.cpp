/**
 * src/frontend/sema/tbb_checker.cpp
 *
 * Twisted Balanced Binary (TBB) Type Checker Implementation
 */

#include "tbb_checker.h"
#include <stdexcept>
#include <sstream>

namespace aria {
namespace sema {

// Static constraint definitions
TBBConstraints TBBConstraints::forType(const std::string& type_name) {
    if (type_name == "tbb8") {
        return TBBConstraints{
            .min_value = -127,
            .max_value = 127,
            .err_value = -128,  // 0x80
            .bit_width = 8
        };
    }
    else if (type_name == "tbb16") {
        return TBBConstraints{
            .min_value = -32767,
            .max_value = 32767,
            .err_value = -32768,  // 0x8000
            .bit_width = 16
        };
    }
    else if (type_name == "tbb32") {
        return TBBConstraints{
            .min_value = -2147483647,
            .max_value = 2147483647,
            .err_value = -2147483648LL,  // 0x80000000
            .bit_width = 32
        };
    }
    else if (type_name == "tbb64") {
        return TBBConstraints{
            .min_value = -9223372036854775807LL,
            .max_value = 9223372036854775807LL,
            .err_value = INT64_MIN,  // 0x8000000000000000
            .bit_width = 64
        };
    }
    
    throw std::invalid_argument("Unknown TBB type: " + type_name);
}

bool TBBTypeChecker::isTBBType(const std::string& type_name) {
    return type_name == "tbb8" || 
           type_name == "tbb16" || 
           type_name == "tbb32" || 
           type_name == "tbb64";
}

TBBConstraints TBBTypeChecker::getConstraints(const std::string& type_name) {
    if (!isTBBType(type_name)) {
        throw std::invalid_argument("Not a TBB type: " + type_name);
    }
    return TBBConstraints::forType(type_name);
}

std::string TBBTypeChecker::validateLiteral(const std::string& type_name, int64_t value) {
    if (!isTBBType(type_name)) {
        return "Not a TBB type: " + type_name;
    }
    
    TBBConstraints constraints = TBBConstraints::forType(type_name);
    
    // Check if value is the error sentinel (reserved, cannot be used directly)
    if (constraints.isErrorSentinel(value)) {
        std::ostringstream oss;
        oss << "Cannot assign error sentinel value " << value 
            << " to " << type_name << ". Use ERR keyword instead.";
        return oss.str();
    }
    
    // Check if value is within symmetric range
    if (!constraints.isValidValue(value)) {
        std::ostringstream oss;
        oss << "Value " << value << " out of range for " << type_name 
            << " (valid range: [" << constraints.min_value 
            << ", " << constraints.max_value << "])";
        return oss.str();
    }
    
    return "";  // Valid
}

bool TBBTypeChecker::isErrorSentinel(const std::string& type_name, int64_t value) {
    if (!isTBBType(type_name)) {
        return false;
    }
    
    TBBConstraints constraints = TBBConstraints::forType(type_name);
    return constraints.isErrorSentinel(value);
}

std::string TBBTypeChecker::getLLVMTypeName(const std::string& type_name) {
    if (type_name == "tbb8") return "i8";
    if (type_name == "tbb16") return "i16";
    if (type_name == "tbb32") return "i32";
    if (type_name == "tbb64") return "i64";
    
    throw std::invalid_argument("Unknown TBB type: " + type_name);
}

} // namespace sema
} // namespace aria

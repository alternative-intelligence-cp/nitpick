/**
 * src/frontend/sema/tbb_checker.h
 *
 * Twisted Balanced Binary (TBB) Type Checker
 * Version: 0.0.9
 *
 * Validates TBB type constraints:
 * - Symmetric ranges: [-127, +127] for tbb8, etc.
 * - Error sentinel (ERR) is reserved minimum value
 * - Compile-time range validation for literals
 */

#ifndef ARIA_FRONTEND_SEMA_TBB_CHECKER_H
#define ARIA_FRONTEND_SEMA_TBB_CHECKER_H

#include "types.h"
#include <string>
#include <cstdint>

namespace aria {
namespace sema {

/**
 * TBB Type Constraints
 * 
 * Each TBB type has a symmetric range with an error sentinel:
 * - tbb8:  [-127, +127],  ERR = -128 (0x80)
 * - tbb16: [-32767, +32767], ERR = -32768 (0x8000)
 * - tbb32: [-2147483647, +2147483647], ERR = -2147483648 (0x80000000)
 * - tbb64: [-9223372036854775807, +9223372036854775807], ERR = INT64_MIN
 */
struct TBBConstraints {
    int64_t min_value;  // Minimum valid value (symmetric)
    int64_t max_value;  // Maximum valid value (symmetric)
    int64_t err_value;  // Error sentinel value
    uint8_t bit_width;  // Bit width (8, 16, 32, 64)
    
    static TBBConstraints forType(const std::string& type_name);
    
    bool isValidValue(int64_t value) const {
        return value >= min_value && value <= max_value;
    }
    
    bool isErrorSentinel(int64_t value) const {
        return value == err_value;
    }
};

/**
 * TBB Type Checker
 */
class TBBTypeChecker {
public:
    /**
     * Check if a type name is a TBB type
     */
    static bool isTBBType(const std::string& type_name);
    
    /**
     * Get constraints for a TBB type
     * Throws if type is not a valid TBB type
     */
    static TBBConstraints getConstraints(const std::string& type_name);
    
    /**
     * Validate a literal value for a TBB type
     * Returns error message if invalid, empty string if valid
     */
    static std::string validateLiteral(const std::string& type_name, int64_t value);
    
    /**
     * Check if a value would be the error sentinel for a type
     */
    static bool isErrorSentinel(const std::string& type_name, int64_t value);
    
    /**
     * Get the LLVM type name for a TBB type (for codegen)
     */
    static std::string getLLVMTypeName(const std::string& type_name);
};

} // namespace sema
} // namespace aria

#endif // ARIA_FRONTEND_SEMA_TBB_CHECKER_H

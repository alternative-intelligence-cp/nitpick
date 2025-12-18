/**
 * Aria Standard Library - Result Type Support
 * 
 * This header defines the C++ runtime support for Aria's result<T> type,
 * which is used for explicit error handling without exceptions.
 * 
 * Reference: research_031_essential_stdlib.txt Section 7
 */

#ifndef ARIA_RUNTIME_RESULT_H
#define ARIA_RUNTIME_RESULT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Result Type Structures
// ============================================================================

/**
 * Generic result structure for pointer types
 * Used for operations that return pointers (strings, objects, etc.)
 */
typedef struct {
    void* value;       // Success value (NULL if error)
    void* error;       // Error value (NULL if success)
    bool is_error;     // True if this is an error result
} AriaResultPtr;

/**
 * Result structure for int64 values
 * Used for operations that return integers
 */
typedef struct {
    int64_t value;     // Success value (0 if error by convention)
    void* error;       // Error value (NULL if success)
    bool is_error;     // True if this is an error result
} AriaResultI64;

/**
 * Result structure for flt64 values
 * Used for operations that return floating-point values
 */
typedef struct {
    double value;      // Success value (0.0 if error by convention)
    void* error;       // Error value (NULL if success)
    bool is_error;     // True if this is an error result
} AriaResultF64;

/**
 * Result structure for bool values
 */
typedef struct {
    bool value;        // Success value (false if error by convention)
    void* error;       // Error value (NULL if success)
    bool is_error;     // True if this is an error result
} AriaResultBool;

/**
 * Result structure for void operations (success/failure only)
 */
typedef struct {
    void* error;       // Error value (NULL if success)
    bool is_error;     // True if this is an error result
} AriaResultVoid;

// ============================================================================
// Error Object
// ============================================================================

/**
 * Error object structure
 * Contains error code, message, and optional context
 */
typedef struct {
    int32_t code;              // Error code
    const char* message;       // Error message (null-terminated UTF-8)
    const char* file;          // Source file where error occurred
    int32_t line;              // Line number where error occurred
} AriaError;

// ============================================================================
// Result Construction Functions
// ============================================================================

/**
 * Create a success result with pointer value
 */
AriaResultPtr aria_result_ok_ptr(void* value);

/**
 * Create an error result with pointer type
 */
AriaResultPtr aria_result_err_ptr(AriaError* error);

/**
 * Create a success result with int64 value
 */
AriaResultI64 aria_result_ok_i64(int64_t value);

/**
 * Create an error result with int64 type
 */
AriaResultI64 aria_result_err_i64(AriaError* error);

/**
 * Create a success result with flt64 value
 */
AriaResultF64 aria_result_ok_f64(double value);

/**
 * Create an error result with flt64 type
 */
AriaResultF64 aria_result_err_f64(AriaError* error);

/**
 * Create a success result with bool value
 */
AriaResultBool aria_result_ok_bool(bool value);

/**
 * Create an error result with bool type
 */
AriaResultBool aria_result_err_bool(AriaError* error);

/**
 * Create a success result for void operation
 */
AriaResultVoid aria_result_ok_void(void);

/**
 * Create an error result for void operation
 */
AriaResultVoid aria_result_err_void(AriaError* error);

// ============================================================================
// Error Construction Functions
// ============================================================================

/**
 * Create a new error object
 * @param code Error code
 * @param message Error message (copied to GC heap)
 * @param file Source file (optional, can be NULL)
 * @param line Line number (0 if not applicable)
 * @return Pointer to error object (allocated on GC heap)
 */
AriaError* aria_error_new(int32_t code, const char* message, const char* file, int32_t line);

/**
 * Create a simple error with just a message
 */
AriaError* aria_error_msg(const char* message);

// ============================================================================
// Common Error Codes
// ============================================================================

#define ARIA_ERR_UNKNOWN        -1
#define ARIA_ERR_INVALID_ARG    -2
#define ARIA_ERR_OUT_OF_MEMORY  -3
#define ARIA_ERR_NOT_FOUND      -4
#define ARIA_ERR_PERMISSION     -5
#define ARIA_ERR_IO             -6
#define ARIA_ERR_TIMEOUT        -7
#define ARIA_ERR_OVERFLOW       -8
#define ARIA_ERR_UNDERFLOW      -9
#define ARIA_ERR_DIV_BY_ZERO    -10
#define ARIA_ERR_NULL_PTR       -11
#define ARIA_ERR_INDEX_OUT_OF_BOUNDS -12
#define ARIA_ERR_OUT_OF_BOUNDS  -12  // Alias for INDEX_OUT_OF_BOUNDS

// ============================================================================
// Result Query Functions
// ============================================================================

/**
 * Check if result is Ok (success)
 */
bool aria_result_is_ok_ptr(AriaResultPtr result);
bool aria_result_is_ok_i64(AriaResultI64 result);
bool aria_result_is_ok_f64(AriaResultF64 result);
bool aria_result_is_ok_bool(AriaResultBool result);
bool aria_result_is_ok_void(AriaResultVoid result);

/**
 * Check if result is Err (error)
 */
bool aria_result_is_err_ptr(AriaResultPtr result);
bool aria_result_is_err_i64(AriaResultI64 result);
bool aria_result_is_err_f64(AriaResultF64 result);
bool aria_result_is_err_bool(AriaResultBool result);
bool aria_result_is_err_void(AriaResultVoid result);

/**
 * Get error from result (returns NULL if result is Ok)
 */
AriaError* aria_result_get_error_ptr(AriaResultPtr result);
AriaError* aria_result_get_error_i64(AriaResultI64 result);
AriaError* aria_result_get_error_f64(AriaResultF64 result);
AriaError* aria_result_get_error_bool(AriaResultBool result);
AriaError* aria_result_get_error_void(AriaResultVoid result);

/**
 * Unwrap result (returns value, panics if error)
 * NOTE: These should only be used when you're certain the result is Ok
 */
void* aria_result_unwrap_ptr(AriaResultPtr result);
int64_t aria_result_unwrap_i64(AriaResultI64 result);
double aria_result_unwrap_f64(AriaResultF64 result);
bool aria_result_unwrap_bool(AriaResultBool result);

/**
 * Unwrap or return default value
 */
void* aria_result_unwrap_or_ptr(AriaResultPtr result, void* default_value);
int64_t aria_result_unwrap_or_i64(AriaResultI64 result, int64_t default_value);
double aria_result_unwrap_or_f64(AriaResultF64 result, double default_value);
bool aria_result_unwrap_or_bool(AriaResultBool result, bool default_value);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_RESULT_H

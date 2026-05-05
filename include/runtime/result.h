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
AriaResultPtr npk_result_ok_ptr(void* value);

/**
 * Create an error result with pointer type
 */
AriaResultPtr npk_result_err_ptr(AriaError* error);

/**
 * Create a success result with int64 value
 */
AriaResultI64 npk_result_ok_i64(int64_t value);

/**
 * Create an error result with int64 type
 */
AriaResultI64 npk_result_err_i64(AriaError* error);

/**
 * Create a success result with flt64 value
 */
AriaResultF64 npk_result_ok_f64(double value);

/**
 * Create an error result with flt64 type
 */
AriaResultF64 npk_result_err_f64(AriaError* error);

/**
 * Create a success result with bool value
 */
AriaResultBool npk_result_ok_bool(bool value);

/**
 * Create an error result with bool type
 */
AriaResultBool npk_result_err_bool(AriaError* error);

/**
 * Create a success result for void operation
 */
AriaResultVoid npk_result_ok_void(void);

/**
 * Create an error result for void operation
 */
AriaResultVoid npk_result_err_void(AriaError* error);

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
AriaError* npk_error_new(int32_t code, const char* message, const char* file, int32_t line);

/**
 * Create a simple error with just a message
 */
AriaError* npk_error_msg(const char* message);

// ============================================================================
// Allocation Result Types (Phase 4.2)
// ============================================================================

/**
 * Allocation error codes
 * Maps to Aria's alloc_error enum
 */
typedef enum {
    ARIA_ALLOC_OK = 0,                    // Success (not an error)
    ARIA_ALLOC_ERR_OUT_OF_MEMORY = 1,     // System allocator returned NULL
    ARIA_ALLOC_ERR_INVALID_SIZE = 2,      // Size is 0 or exceeds limits
    ARIA_ALLOC_ERR_INVALID_ALIGNMENT = 3, // Alignment not power of 2
    ARIA_ALLOC_ERR_SIZE_OVERFLOW = 4,     // size * count overflow
    ARIA_ALLOC_ERR_UNSUPPORTED = 5        // Operation not supported
} AriaAllocError;

/**
 * Allocation result structure
 * Used for result-based allocation functions that provide rich error context
 * 
 * Layout matches Aria's result<wild T@, alloc_error>:
 * - value: Allocated pointer (valid when error_code == ARIA_ALLOC_OK)
 * - error_code: Error variant (0 = success)
 * - requested_size: Size requested by caller (for diagnostics)
 * - requested_align: Alignment requested (0 = default)
 */
typedef struct {
    void* value;               // Allocated pointer (NULL if error)
    AriaAllocError error_code; // Error code (ARIA_ALLOC_OK = success)
    size_t requested_size;     // Size parameter from caller
    size_t requested_align;    // Alignment parameter (0 = default)
} AriaAllocResult;

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
bool npk_result_is_ok_ptr(AriaResultPtr result);
bool npk_result_is_ok_i64(AriaResultI64 result);
bool npk_result_is_ok_f64(AriaResultF64 result);
bool npk_result_is_ok_bool(AriaResultBool result);
bool npk_result_is_ok_void(AriaResultVoid result);

/**
 * Check if result is Err (error)
 */
bool npk_result_is_err_ptr(AriaResultPtr result);
bool npk_result_is_err_i64(AriaResultI64 result);
bool npk_result_is_err_f64(AriaResultF64 result);
bool npk_result_is_err_bool(AriaResultBool result);
bool npk_result_is_err_void(AriaResultVoid result);

/**
 * Get error from result (returns NULL if result is Ok)
 */
AriaError* npk_result_get_error_ptr(AriaResultPtr result);
AriaError* npk_result_get_error_i64(AriaResultI64 result);
AriaError* npk_result_get_error_f64(AriaResultF64 result);
AriaError* npk_result_get_error_bool(AriaResultBool result);
AriaError* npk_result_get_error_void(AriaResultVoid result);

/**
 * Unwrap result (returns value, panics if error)
 * NOTE: These should only be used when you're certain the result is Ok
 */
void* npk_result_unwrap_ptr(AriaResultPtr result);
int64_t npk_result_unwrap_i64(AriaResultI64 result);
double npk_result_unwrap_f64(AriaResultF64 result);
bool npk_result_unwrap_bool(AriaResultBool result);

/**
 * Unwrap or return default value
 */
void* npk_result_unwrap_or_ptr(AriaResultPtr result, void* default_value);
int64_t npk_result_unwrap_or_i64(AriaResultI64 result, int64_t default_value);
double npk_result_unwrap_or_f64(AriaResultF64 result, double default_value);
bool npk_result_unwrap_or_bool(AriaResultBool result, bool default_value);

// ============================================================================
// Allocation Result Functions (Phase 4.2)
// ============================================================================

/**
 * Create successful allocation result
 * @param ptr Allocated pointer
 * @param size Size that was allocated
 * @param align Alignment that was used (0 = default)
 */
AriaAllocResult npk_alloc_result_ok(void* ptr, size_t size, size_t align);

/**
 * Create error allocation result
 * @param error Error code variant
 * @param size Size that was requested
 * @param align Alignment that was requested
 */
AriaAllocResult npk_alloc_result_err(AriaAllocError error, size_t size, size_t align);

/**
 * Check if allocation result is Ok
 */
bool npk_alloc_result_is_ok(AriaAllocResult result);

/**
 * Check if allocation result is Err
 */
bool npk_alloc_result_is_err(AriaAllocResult result);

/**
 * Get error message for allocation error
 * @return Static string describing the error
 */
const char* npk_alloc_error_message(AriaAllocError error);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_RESULT_H

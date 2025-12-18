/**
 * Aria Standard Library Runtime Support
 * 
 * This header defines the C++ runtime functions that support the Aria standard
 * library. These functions are called from compiled Aria code via extern "C"
 * linkage.
 * 
 * Reference: research_031_essential_stdlib.txt
 */

#ifndef ARIA_RUNTIME_STDLIB_H
#define ARIA_RUNTIME_STDLIB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// I/O Functions (std.io)
// ============================================================================

/**
 * Print a string to stdout (no newline)
 * @param str Pointer to UTF-8 string
 * @param len Length of string in bytes
 */
void aria_print(const char* str, int64_t len);

/**
 * Print a string to stdout with newline
 * @param str Pointer to UTF-8 string
 * @param len Length of string in bytes
 */
void aria_println(const char* str, int64_t len);

/**
 * Print a string to stderr (no newline)
 * @param str Pointer to UTF-8 string
 * @param len Length of string in bytes
 */
void aria_eprint(const char* str, int64_t len);

/**
 * Print a string to stderr with newline
 * @param str Pointer to UTF-8 string
 * @param len Length of string in bytes
 */
void aria_eprintln(const char* str, int64_t len);

/**
 * Print a string to debug channel (stddbg - fd 3)
 * @param str Pointer to UTF-8 string
 * @param len Length of string in bytes
 */
void aria_debug(const char* str, int64_t len);

/**
 * Print a string to debug channel with newline
 * @param str Pointer to UTF-8 string
 * @param len Length of string in bytes
 */
void aria_debugln(const char* str, int64_t len);

// ============================================================================
// Memory Functions (std.mem) - Wrappers for existing allocators
// ============================================================================

/**
 * Allocate memory on GC heap (wrapper for aria_gc_alloc)
 * @param size Size in bytes
 * @return Pointer to allocated memory or NULL on failure
 */
void* aria_std_gc_alloc(int64_t size);

/**
 * Allocate memory on wild heap (wrapper for aria_alloc)
 * @param size Size in bytes
 * @return Pointer to allocated memory or NULL on failure
 */
void* aria_std_alloc(int64_t size);

/**
 * Free memory on wild heap (wrapper for aria_free)
 * @param ptr Pointer to memory to free
 */
void aria_std_free(void* ptr);

/**
 * Allocate executable memory (wrapper for aria_alloc_exec)
 * @param size Size in bytes
 * @return Pointer to allocated memory or NULL on failure
 */
void* aria_std_alloc_exec(int64_t size);

/**
 * Free executable memory
 * @param ptr Pointer to memory to free
 */
void aria_std_free_exec(void* ptr);

// ============================================================================
// String Functions (std.string)
// ============================================================================

/**
 * Get length of a null-terminated string
 * @param str Pointer to string
 * @return Length in bytes (excluding null terminator)
 */
int64_t aria_cstr_length(const char* str);

/**
 * Compare two strings
 * @param s1 First string
 * @param s2 Second string
 * @return 0 if equal, <0 if s1 < s2, >0 if s1 > s2
 */
int32_t aria_string_compare(const char* s1, const char* s2);

/**
 * Concatenate two strings (allocates new string on GC heap)
 * @param s1 First string
 * @param s2 Second string
 * @return Pointer to new concatenated string or NULL on failure
 */
char* aria_cstr_concat(const char* s1, const char* s2);

// ============================================================================
// Math Functions (std.math)
// ============================================================================

/**
 * Absolute value for int64
 * @param x Input value
 * @return Absolute value
 */
int64_t aria_math_abs_i64(int64_t x);

/**
 * Absolute value for flt64
 * @param x Input value
 * @return Absolute value
 */
double aria_math_abs_f64(double x);

/**
 * Square root
 * @param x Input value
 * @return Square root of x
 */
double aria_math_sqrt(double x);

/**
 * Power function
 * @param x Base
 * @param y Exponent
 * @return x raised to power y
 */
double aria_math_pow(double x, double y);

/**
 * Minimum of two int64 values
 * @param a First value
 * @param b Second value
 * @return Minimum value
 */
int64_t aria_math_min_i64(int64_t a, int64_t b);

/**
 * Maximum of two int64 values
 * @param a First value
 * @param b Second value
 * @return Maximum value
 */
int64_t aria_math_max_i64(int64_t a, int64_t b);

// ============================================================================
// Math Constants
// ============================================================================

/**
 * Get PI constant
 * @return Value of PI
 */
double aria_math_pi(void);

/**
 * Get E constant
 * @return Value of E
 */
double aria_math_e(void);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_STDLIB_H

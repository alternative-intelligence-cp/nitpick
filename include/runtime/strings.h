/**
 * Phase 6.3 Standard Library - String Operations
 * 
 * High-level string manipulation functions for Aria runtime.
 * 
 * Design:
 * - UTF-8 string support (basic, full Unicode handling future)
 * - Result types for error handling
 * - GC-integrated string allocation
 * - Common string operations (length, substring, split, etc.)
 * 
 * Note: Aria strings are UTF-8 byte arrays. For now, operations work on
 * byte boundaries. Full Unicode grapheme cluster support is future work.
 */

#ifndef ARIA_RUNTIME_STRINGS_H
#define ARIA_RUNTIME_STRINGS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "runtime/result.h"
#include "runtime/collections.h"

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════
// String Structure
// ═══════════════════════════════════════════════════════════════════════

/**
 * Aria string structure.
 * 
 * Strings are immutable UTF-8 byte sequences with explicit length.
 * No null termination required (but may be present for C interop).
 */
typedef struct {
    const char* data;   // UTF-8 byte data (may or may not be null-terminated)
    int64_t length;     // Length in bytes (NOT characters)
} AriaString;

// ═══════════════════════════════════════════════════════════════════════
// String Creation
// ═══════════════════════════════════════════════════════════════════════

/**
 * Create a new string from C string (null-terminated).
 * Makes a GC-allocated copy of the data.
 * 
 * @param cstr Null-terminated C string
 * @return Result containing AriaString* or error
 */
AriaResultPtr aria_string_from_cstr(const char* cstr);

/**
 * Create a new string from byte array with explicit length.
 * Makes a GC-allocated copy of the data.
 * 
 * @param data Byte array (UTF-8)
 * @param length Length in bytes
 * @return Result containing AriaString* or error
 */
AriaResultPtr aria_string_from_bytes(const char* data, int64_t length);

/**
 * Create an empty string.
 * 
 * @return Empty AriaString (length=0, data points to empty string)
 */
AriaString* aria_string_empty();

// ═══════════════════════════════════════════════════════════════════════
// String Basic Operations
// ═══════════════════════════════════════════════════════════════════════

/**
 * Get the length of a string in bytes.
 * 
 * @param str String to query
 * @return Length in bytes (NOT Unicode code points)
 */
int64_t aria_string_length(AriaString str);

/**
 * Check if a string is empty.
 * 
 * @param str String to check
 * @return true if length is 0
 */
bool aria_string_is_empty(AriaString str);

/**
 * Compare two strings for equality.
 * 
 * @param a First string
 * @param b Second string
 * @return true if strings are byte-equal
 */
bool aria_string_equals(AriaString a, AriaString b);

/**
 * Get substring from start index to end index (exclusive).
 * 
 * @param str Source string
 * @param start Start index (inclusive, 0-based)
 * @param end End index (exclusive)
 * @return Result containing new AriaString or error
 * 
 * Note: Indices are byte offsets, not character positions.
 * Out of bounds indices return error.
 */
AriaResultPtr aria_string_substring(AriaString str, int64_t start, int64_t end);

/**
 * Find the first occurrence of a substring.
 * 
 * @param haystack String to search in
 * @param needle String to search for
 * @return Result containing index (int64_t) or error if not found
 */
AriaResultI64 aria_string_index_of(AriaString haystack, AriaString needle);

/**
 * Check if string contains substring.
 * 
 * @param haystack String to search in
 * @param needle String to search for
 * @return true if needle is found in haystack
 */
bool aria_string_contains(AriaString haystack, AriaString needle);

/**
 * Check if string starts with prefix.
 * 
 * @param str String to check
 * @param prefix Prefix to match
 * @return true if str starts with prefix
 */
bool aria_string_starts_with(AriaString str, AriaString prefix);

/**
 * Check if string ends with suffix.
 * 
 * @param str String to check
 * @param suffix Suffix to match
 * @return true if str ends with suffix
 */
bool aria_string_ends_with(AriaString str, AriaString suffix);

// ═══════════════════════════════════════════════════════════════════════
// String Manipulation
// ═══════════════════════════════════════════════════════════════════════

/**
 * Trim whitespace from both ends of string.
 * 
 * @param str String to trim
 * @return Result containing new AriaString with whitespace removed
 * 
 * Note: Trims ASCII whitespace: space, tab, newline, carriage return.
 */
AriaResultPtr aria_string_trim(AriaString str);

/**
 * Trim whitespace from start of string.
 * 
 * @param str String to trim
 * @return Result containing new AriaString with leading whitespace removed
 */
AriaResultPtr aria_string_trim_start(AriaString str);

/**
 * Trim whitespace from end of string.
 * 
 * @param str String to trim
 * @return Result containing new AriaString with trailing whitespace removed
 */
AriaResultPtr aria_string_trim_end(AriaString str);

/**
 * Convert string to uppercase.
 * 
 * @param str String to convert
 * @return Result containing new AriaString in uppercase
 * 
 * Note: Only ASCII characters are converted. Unicode uppercase is future work.
 */
AriaResultPtr aria_string_to_upper(AriaString str);

/**
 * Convert string to lowercase.
 * 
 * @param str String to convert
 * @return Result containing new AriaString in lowercase
 * 
 * Note: Only ASCII characters are converted. Unicode lowercase is future work.
 */
AriaResultPtr aria_string_to_lower(AriaString str);

/**
 * Concatenate two strings.
 * 
 * @param a First string
 * @param b Second string
 * @return Result containing new AriaString (a + b)
 */
AriaResultPtr aria_string_concat(AriaString a, AriaString b);

/**
 * Repeat a string n times.
 * 
 * @param str String to repeat
 * @param count Number of repetitions
 * @return Result containing new AriaString or error if count < 0
 */
AriaResultPtr aria_string_repeat(AriaString str, int64_t count);

// ═══════════════════════════════════════════════════════════════════════
// String Splitting and Joining
// ═══════════════════════════════════════════════════════════════════════

/**
 * Split string by delimiter.
 * 
 * @param str String to split
 * @param delimiter Delimiter string
 * @return Result containing AriaArray* of AriaString elements
 * 
 * Note: Returns empty array if str is empty.
 * If delimiter is empty, splits into individual bytes.
 * If delimiter not found, returns array with original string.
 */
AriaResultPtr aria_string_split(AriaString str, AriaString delimiter);

/**
 * Join array of strings with delimiter.
 * 
 * @param strings Array of AriaString elements
 * @param delimiter Delimiter to insert between strings
 * @return Result containing joined AriaString
 * 
 * Note: strings->data must point to array of AriaString structs.
 * strings->element_size must be sizeof(AriaString).
 */
AriaResultPtr aria_string_join(const AriaArray* strings, AriaString delimiter);

// ═══════════════════════════════════════════════════════════════════════
// String Conversion
// ═══════════════════════════════════════════════════════════════════════

/**
 * Convert string to null-terminated C string.
 * Returns a GC-allocated copy with null terminator added.
 * 
 * @param str String to convert
 * @return Result containing char* (null-terminated) or error
 */
AriaResultPtr aria_string_to_cstr(AriaString str);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_STRINGS_H

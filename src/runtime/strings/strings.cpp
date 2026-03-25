/**
 * Phase 6.3 Standard Library - String Operations Implementation
 * 
 * High-level string manipulation functions.
 */

#include "runtime/strings.h"
#include "runtime/gc.h"
#include <cerrno>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <string>

// ═══════════════════════════════════════════════════════════════════════
// Helper Functions
// ═══════════════════════════════════════════════════════════════════════

/**
 * Allocate an AriaString on GC heap and return as result.
 * Helper to wrap string values in result type.
 */
static AriaResultPtr alloc_string_result(const char* data, int64_t length) {
    AriaString* str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0);
    if (!str) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate string structure",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    str->data = data;
    str->length = length;
    return aria_result_ok_ptr(str);
}

// ═══════════════════════════════════════════════════════════════════════
// String Creation
// ═══════════════════════════════════════════════════════════════════════

AriaResultPtr aria_string_from_cstr(const char* cstr) {
    if (!cstr) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Cannot create string from NULL pointer",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    int64_t length = strlen(cstr);
    return aria_string_from_bytes(cstr, length);
}

AriaResultPtr aria_string_from_bytes(const char* data, int64_t length) {
    if (!data && length > 0) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Cannot create string from NULL data with non-zero length",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    if (length < 0) {
        AriaError* error = aria_error_new(
            ARIA_ERR_INVALID_ARG,
            "String length cannot be negative",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    // Allocate GC memory for string data (with null terminator for C interop)
    char* copied_data = (char*)aria_gc_alloc(length + 1, 0);
    if (!copied_data) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate string data",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    // Copy data and add null terminator
    if (length > 0) {
        memcpy(copied_data, data, length);
    }
    copied_data[length] = '\0';
    
    // Allocate AriaString struct on GC heap
    AriaString* str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0);
    if (!str) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate string structure",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    str->data = copied_data;
    str->length = length;
    
    return aria_result_ok_ptr(str);
}

AriaString* aria_string_empty() {
    static const char empty_str[] = "";
    AriaString* str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0);
    if (!str) {
        // For empty string, we can return a static version on allocation failure
        static AriaString static_empty = {empty_str, 0};
        return &static_empty;
    }
    str->data = empty_str;
    str->length = 0;
    return str;
}

// ═══════════════════════════════════════════════════════════════════════
// String Basic Operations
// ═══════════════════════════════════════════════════════════════════════

int64_t aria_string_length(AriaString str) {
    return str.length;
}

bool aria_string_is_empty(AriaString str) {
    return str.length == 0;
}

bool aria_string_equals(AriaString a, AriaString b) {
    if (a.length != b.length) {
        return false;
    }
    if (a.length == 0) {
        return true;  // Both empty
    }
    return memcmp(a.data, b.data, a.length) == 0;
}

int32_t aria_string_compare_str(AriaString a, AriaString b) {
    int64_t min_len = a.length < b.length ? a.length : b.length;
    if (min_len > 0) {
        int result = memcmp(a.data, b.data, min_len);
        if (result != 0) return result < 0 ? -1 : 1;
    }
    if (a.length < b.length) return -1;
    if (a.length > b.length) return 1;
    return 0;
}

AriaResultPtr aria_string_substring(AriaString str, int64_t start, int64_t end) {
    // Bounds checking
    if (start < 0 || start > str.length) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_BOUNDS,
            "Substring start index out of bounds",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    if (end < start || end > str.length) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_BOUNDS,
            "Substring end index out of bounds",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    int64_t sub_length = end - start;
    
    // Empty substring
    if (sub_length == 0) {
        return aria_result_ok_ptr(aria_string_empty());
    }
    
    // Create new string from substring
    return aria_string_from_bytes(str.data + start, sub_length);
}

AriaResultPtr aria_string_from_char(uint8_t ch) {
    // Create a single-character string
    char* char_data = (char*)aria_gc_alloc(2, 0);  // 1 byte + null terminator
    if (!char_data) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate character data",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    char_data[0] = (char)ch;
    char_data[1] = '\0';
    
    AriaString* str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0);
    if (!str) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate string structure",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    str->data = char_data;
    str->length = 1;
    
    return aria_result_ok_ptr(str);
}

AriaResultI64 aria_string_index_of(AriaString haystack, AriaString needle) {
    if (needle.length == 0) {
        // Empty needle always matches at position 0
        return aria_result_ok_i64(0);
    }
    
    if (needle.length > haystack.length) {
        // Needle longer than haystack, cannot match
        return aria_result_ok_i64(-1);
    }
    
    // Simple string search (could be optimized with Boyer-Moore or similar)
    for (int64_t i = 0; i <= (int64_t)(haystack.length - needle.length); i++) {
        if (memcmp(haystack.data + i, needle.data, needle.length) == 0) {
            return aria_result_ok_i64(i);
        }
    }
    
    // Not found
    return aria_result_ok_i64(-1);
}

bool aria_string_contains(AriaString haystack, AriaString needle) {
    AriaResultI64 result = aria_string_index_of(haystack, needle);
    return result.value >= 0;
}

bool aria_string_starts_with(AriaString str, AriaString prefix) {
    if (prefix.length > str.length) {
        return false;
    }
    if (prefix.length == 0) {
        return true;  // Empty prefix always matches
    }
    return memcmp(str.data, prefix.data, prefix.length) == 0;
}

bool aria_string_ends_with(AriaString str, AriaString suffix) {
    if (suffix.length > str.length) {
        return false;
    }
    if (suffix.length == 0) {
        return true;  // Empty suffix always matches
    }
    return memcmp(str.data + str.length - suffix.length, suffix.data, suffix.length) == 0;
}

// ═══════════════════════════════════════════════════════════════════════
// String Manipulation
// ═══════════════════════════════════════════════════════════════════════

// Helper: Check if character is ASCII whitespace
static inline bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

AriaResultPtr aria_string_trim(AriaString str) {
    if (str.length == 0) {
        { AriaString* heap_str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0); if (!heap_str) return aria_result_err_ptr(aria_error_new(ARIA_ERR_OUT_OF_MEMORY, "Failed to allocate string", __FILE__, __LINE__)); *heap_str = str; return aria_result_ok_ptr(heap_str); }
    }
    
    // Find first non-whitespace
    int64_t start = 0;
    while (start < str.length && is_whitespace(str.data[start])) {
        start++;
    }
    
    // All whitespace
    if (start == str.length) {
        return aria_result_ok_ptr(aria_string_empty());
    }
    
    // Find last non-whitespace
    int64_t end = str.length - 1;
    while (end >= start && is_whitespace(str.data[end])) {
        end--;
    }
    
    // Create substring (end + 1 because substring is exclusive)
    return aria_string_substring(str, start, end + 1);
}

AriaResultPtr aria_string_trim_start(AriaString str) {
    if (str.length == 0) {
        { AriaString* heap_str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0); if (!heap_str) return aria_result_err_ptr(aria_error_new(ARIA_ERR_OUT_OF_MEMORY, "Failed to allocate string", __FILE__, __LINE__)); *heap_str = str; return aria_result_ok_ptr(heap_str); }
    }
    
    // Find first non-whitespace
    int64_t start = 0;
    while (start < str.length && is_whitespace(str.data[start])) {
        start++;
    }
    
    // All whitespace
    if (start == str.length) {
        return aria_result_ok_ptr(aria_string_empty());
    }
    
    return aria_string_substring(str, start, str.length);
}

AriaResultPtr aria_string_trim_end(AriaString str) {
    if (str.length == 0) {
        { AriaString* heap_str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0); if (!heap_str) return aria_result_err_ptr(aria_error_new(ARIA_ERR_OUT_OF_MEMORY, "Failed to allocate string", __FILE__, __LINE__)); *heap_str = str; return aria_result_ok_ptr(heap_str); }
    }
    
    // Find last non-whitespace
    int64_t end = str.length - 1;
    while (end >= 0 && is_whitespace(str.data[end])) {
        end--;
    }
    
    // All whitespace
    if (end < 0) {
        return aria_result_ok_ptr(aria_string_empty());
    }
    
    return aria_string_substring(str, 0, end + 1);
}

AriaResultPtr aria_string_to_upper(AriaString str) {
    if (str.length == 0) {
        { AriaString* heap_str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0); if (!heap_str) return aria_result_err_ptr(aria_error_new(ARIA_ERR_OUT_OF_MEMORY, "Failed to allocate string", __FILE__, __LINE__)); *heap_str = str; return aria_result_ok_ptr(heap_str); }
    }
    
    // Allocate new string data
    char* upper_data = (char*)aria_gc_alloc(str.length + 1, 0);
    if (!upper_data) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate uppercase string",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    // Convert to uppercase (ASCII only for now)
    for (int64_t i = 0; i < str.length; i++) {
        upper_data[i] = toupper((unsigned char)str.data[i]);
    }
    upper_data[str.length] = '\0';
    
    AriaString result = {upper_data, str.length};
    { AriaString* heap_str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0); if (!heap_str) return aria_result_err_ptr(aria_error_new(ARIA_ERR_OUT_OF_MEMORY, "Failed to allocate string", __FILE__, __LINE__)); *heap_str = result; return aria_result_ok_ptr(heap_str); }
}

AriaResultPtr aria_string_to_lower(AriaString str) {
    if (str.length == 0) {
        { AriaString* heap_str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0); if (!heap_str) return aria_result_err_ptr(aria_error_new(ARIA_ERR_OUT_OF_MEMORY, "Failed to allocate string", __FILE__, __LINE__)); *heap_str = str; return aria_result_ok_ptr(heap_str); }
    }
    
    // Allocate new string data
    char* lower_data = (char*)aria_gc_alloc(str.length + 1, 0);
    if (!lower_data) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate lowercase string",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    // Convert to lowercase (ASCII only for now)
    for (int64_t i = 0; i < str.length; i++) {
        lower_data[i] = tolower((unsigned char)str.data[i]);
    }
    lower_data[str.length] = '\0';
    
    AriaString result = {lower_data, str.length};
    { AriaString* heap_str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0); if (!heap_str) return aria_result_err_ptr(aria_error_new(ARIA_ERR_OUT_OF_MEMORY, "Failed to allocate string", __FILE__, __LINE__)); *heap_str = result; return aria_result_ok_ptr(heap_str); }
}

AriaResultPtr aria_string_concat(AriaString a, AriaString b) {
    // BUG-CONCAT-001 FIX: Check for integer overflow before addition
    // GEMINI BATCH 04 SAFETY FIX: Prevent heap corruption from overflow wraparound
    // Risk: Large strings summing to > INT64_MAX would wrap to negative/small value,
    //       causing small allocation followed by large memcpy → heap corruption
    if (INT64_MAX - a.length < b.length) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OVERFLOW,
            "String concatenation would overflow (combined length exceeds INT64_MAX)",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    int64_t total_length = a.length + b.length;
    
    // Allocate new string data
    char* concat_data = (char*)aria_gc_alloc(total_length + 1, 0);
    if (!concat_data) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate concatenated string",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    // Copy both strings
    if (a.length > 0) {
        memcpy(concat_data, a.data, a.length);
    }
    if (b.length > 0) {
        memcpy(concat_data + a.length, b.data, b.length);
    }
    concat_data[total_length] = '\0';
    
    AriaString result = {concat_data, total_length};
    { AriaString* heap_str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0); if (!heap_str) return aria_result_err_ptr(aria_error_new(ARIA_ERR_OUT_OF_MEMORY, "Failed to allocate string", __FILE__, __LINE__)); *heap_str = result; return aria_result_ok_ptr(heap_str); }
}

AriaResultPtr aria_string_concat_n(AriaString* strings, int64_t count) {
    // Optimized multi-string concatenation: O(n) instead of O(n²)
    // Single allocation pass - no intermediate allocations

    if (count <= 0 || !strings) {
        return aria_result_ok_ptr(aria_string_empty());
    }

    if (count == 1) {
        // Single string - just copy it
        return aria_string_from_bytes(strings[0].data, strings[0].length);
    }

    // Phase 1: Calculate total length
    int64_t total_length = 0;
    for (int64_t i = 0; i < count; i++) {
        total_length += strings[i].length;
    }

    if (total_length == 0) {
        return aria_result_ok_ptr(aria_string_empty());
    }

    // Phase 2: Single allocation for entire result
    char* concat_data = (char*)aria_gc_alloc(total_length + 1, 0);
    if (!concat_data) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate concatenated string",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }

    // Phase 3: Copy all strings in one pass
    int64_t offset = 0;
    for (int64_t i = 0; i < count; i++) {
        if (strings[i].length > 0) {
            memcpy(concat_data + offset, strings[i].data, strings[i].length);
            offset += strings[i].length;
        }
    }
    concat_data[total_length] = '\0';

    AriaString result = {concat_data, total_length};
    AriaString* heap_str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0);
    if (!heap_str) {
        return aria_result_err_ptr(aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate string structure",
            __FILE__, __LINE__
        ));
    }
    *heap_str = result;
    return aria_result_ok_ptr(heap_str);
}

AriaString* aria_string_concat_n_simple(AriaString* strings, int64_t count) {
    AriaResultPtr result = aria_string_concat_n(strings, count);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

AriaResultPtr aria_string_repeat(AriaString str, int64_t count) {
    if (count < 0) {
        AriaError* error = aria_error_new(
            ARIA_ERR_INVALID_ARG,
            "Repeat count cannot be negative",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    if (count == 0 || str.length == 0) {
        return aria_result_ok_ptr(aria_string_empty());
    }
    
    int64_t total_length = str.length * count;
    
    // Allocate new string data
    char* repeat_data = (char*)aria_gc_alloc(total_length + 1, 0);
    if (!repeat_data) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate repeated string",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    // Repeat string
    for (int64_t i = 0; i < count; i++) {
        memcpy(repeat_data + (i * str.length), str.data, str.length);
    }
    repeat_data[total_length] = '\0';
    
    AriaString result = {repeat_data, total_length};
    { AriaString* heap_str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0); if (!heap_str) return aria_result_err_ptr(aria_error_new(ARIA_ERR_OUT_OF_MEMORY, "Failed to allocate string", __FILE__, __LINE__)); *heap_str = result; return aria_result_ok_ptr(heap_str); }
}

// ═══════════════════════════════════════════════════════════════════════
// String Splitting and Joining
// ═══════════════════════════════════════════════════════════════════════

AriaResultPtr aria_string_split(AriaString str, AriaString delimiter) {
    // Empty string returns empty array
    if (str.length == 0) {
        return aria_array_new(sizeof(AriaString*), 0, 0);
    }
    
    // Empty delimiter: split into individual bytes
    if (delimiter.length == 0) {
        AriaResultPtr array_result = aria_array_new(sizeof(AriaString*), str.length, 0);
        if (array_result.is_error) {
            return aria_result_err_ptr((AriaError*)array_result.error);
        }
        
        AriaArray* array = (AriaArray*)array_result.value;
        for (int64_t i = 0; i < str.length; i++) {
            AriaResultPtr char_str = aria_string_from_bytes(str.data + i, 1);
            if (char_str.is_error) {
                return aria_result_err_ptr((AriaError*)char_str.error);
            }
            
            AriaString* str_ptr = (AriaString*)char_str.value;
            AriaResultVoid push_result = aria_array_push(array, &str_ptr);
            if (push_result.is_error) {
                return aria_result_err_ptr((AriaError*)push_result.error);
            }
        }
        
        return aria_result_ok_ptr(array);
    }
    
    // Count occurrences to pre-allocate array
    int64_t count = 1;  // At least one part
    for (int64_t i = 0; i <= str.length - delimiter.length; i++) {
        if (memcmp(str.data + i, delimiter.data, delimiter.length) == 0) {
            count++;
            i += delimiter.length - 1;  // Skip past delimiter
        }
    }
    
    // Create array
    AriaResultPtr array_result = aria_array_new(sizeof(AriaString*), count, 0);
    if (array_result.is_error) {
        return aria_result_err_ptr((AriaError*)array_result.error);
    }
    
    AriaArray* array = (AriaArray*)array_result.value;
    
    // Split string
    int64_t start = 0;
    for (int64_t i = 0; i <= str.length - delimiter.length; i++) {
        if (memcmp(str.data + i, delimiter.data, delimiter.length) == 0) {
            // Found delimiter, add part before it
            AriaResultPtr part = aria_string_from_bytes(str.data + start, i - start);
            if (part.is_error) {
                return aria_result_err_ptr((AriaError*)part.error);
            }
            
            AriaString* part_ptr = (AriaString*)part.value;
            AriaResultVoid push_result = aria_array_push(array, &part_ptr);
            if (push_result.is_error) {
                return aria_result_err_ptr((AriaError*)push_result.error);
            }
            
            start = i + delimiter.length;
            i += delimiter.length - 1;  // Skip past delimiter
        }
    }
    
    // Add final part
    AriaResultPtr final_part = aria_string_from_bytes(str.data + start, str.length - start);
    if (final_part.is_error) {
        return aria_result_err_ptr((AriaError*)final_part.error);
    }
    
    AriaString* final_ptr = (AriaString*)final_part.value;
    AriaResultVoid push_result = aria_array_push(array, &final_ptr);
    if (push_result.is_error) {
        return aria_result_err_ptr((AriaError*)push_result.error);
    }
    
    return aria_result_ok_ptr(array);
}

AriaResultPtr aria_string_join(const AriaArray* strings, AriaString delimiter) {
    if (!strings || !strings->data) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Cannot join NULL array",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    if (strings->element_size != sizeof(AriaString*)) {
        AriaError* error = aria_error_new(
            ARIA_ERR_INVALID_ARG,
            "Array element size must be sizeof(AriaString*)",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    // Empty array
    if (strings->length == 0) {
        return aria_result_ok_ptr(aria_string_empty());
    }
    
    // Calculate total length
    int64_t total_length = 0;
    AriaString** parts = (AriaString**)strings->data;
    for (size_t i = 0; i < strings->length; i++) {
        total_length += parts[i]->length;
    }
    // Add delimiter lengths
    total_length += delimiter.length * (strings->length - 1);
    
    // Allocate joined string
    char* joined_data = (char*)aria_gc_alloc(total_length + 1, 0);
    if (!joined_data) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate joined string",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    // Join parts
    int64_t pos = 0;
    for (size_t i = 0; i < strings->length; i++) {
        if (i > 0 && delimiter.length > 0) {
            memcpy(joined_data + pos, delimiter.data, delimiter.length);
            pos += delimiter.length;
        }
        if (parts[i]->length > 0) {
            memcpy(joined_data + pos, parts[i]->data, parts[i]->length);
            pos += parts[i]->length;
        }
    }
    joined_data[total_length] = '\0';
    
    AriaString result = {joined_data, total_length};
    { AriaString* heap_str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0); if (!heap_str) return aria_result_err_ptr(aria_error_new(ARIA_ERR_OUT_OF_MEMORY, "Failed to allocate string", __FILE__, __LINE__)); *heap_str = result; return aria_result_ok_ptr(heap_str); }
}

// ═══════════════════════════════════════════════════════════════════════
// String Conversion
// ═══════════════════════════════════════════════════════════════════════

AriaResultPtr aria_string_to_cstr(AriaString str) {
    // String data is already null-terminated in aria_string_from_bytes
    // Just return the data pointer
    return aria_result_ok_ptr((void*)str.data);
}

// ═══════════════════════════════════════════════════════════════════════
// Integer/Float String Conversion
// ═══════════════════════════════════════════════════════════════════════

AriaResultPtr aria_string_from_int(int64_t value) {
    // Maximum length for int64 is 20 characters (including sign)
    char buffer[32];
    int len = snprintf(buffer, sizeof(buffer), "%ld", value);
    if (len < 0 || len >= (int)sizeof(buffer)) {
        AriaError* error = aria_error_new(
            ARIA_ERR_INVALID_ARG,
            "Failed to convert integer to string",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    return aria_string_from_bytes(buffer, len);
}

AriaResultI64 aria_string_to_int(AriaString str) {
    if (str.length == 0) {
        AriaError* error = aria_error_new(
            ARIA_ERR_INVALID_ARG,
            "Cannot parse empty string as integer",
            __FILE__, __LINE__
        );
        return aria_result_err_i64(error);
    }

    // Create null-terminated copy for strtoll
    char* temp = (char*)malloc(str.length + 1);
    if (!temp) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate temporary buffer",
            __FILE__, __LINE__
        );
        return aria_result_err_i64(error);
    }
    memcpy(temp, str.data, str.length);
    temp[str.length] = '\0';

    // BUG-STRTOLL-001 FIX: Reset errno before strtoll and check for overflow
    // GEMINI BATCH 04 SAFETY FIX: Prevent silent clamping to INT64_MAX
    // Risk: Parsing "99999999999999999999" would silently return INT64_MAX,
    //       disabling safety limits (e.g., MAX_FORCE becomes effectively infinite)
    errno = 0;
    char* endptr;
    int64_t result = strtoll(temp, &endptr, 10);

    // Check for numeric overflow
    if (errno == ERANGE) {
        free(temp);
        AriaError* error = aria_error_new(
            ARIA_ERR_OVERFLOW,
            "Integer overflow in string conversion (value exceeds INT64 range)",
            __FILE__, __LINE__
        );
        return aria_result_err_i64(error);
    }

    // Check if entire string was consumed
    bool valid = (endptr == temp + str.length);
    free(temp);

    if (!valid) {
        AriaError* error = aria_error_new(
            ARIA_ERR_INVALID_ARG,
            "Invalid integer format",
            __FILE__, __LINE__
        );
        return aria_result_err_i64(error);
    }

    return aria_result_ok_i64(result);
}

AriaResultPtr aria_string_to_hex(AriaString str) {
    if (str.length == 0) {
        return aria_result_ok_ptr(aria_string_empty());
    }

    // Each byte becomes 2 hex characters
    int64_t hex_len = str.length * 2;
    char* hex_data = (char*)aria_gc_alloc(hex_len + 1, 0);
    if (!hex_data) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate hex string",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }

    static const char hex_chars[] = "0123456789abcdef";
    for (int64_t i = 0; i < str.length; i++) {
        uint8_t byte = (uint8_t)str.data[i];
        hex_data[i * 2] = hex_chars[byte >> 4];
        hex_data[i * 2 + 1] = hex_chars[byte & 0x0F];
    }
    hex_data[hex_len] = '\0';

    AriaString result = {hex_data, hex_len};
    AriaString* heap_str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0);
    if (!heap_str) {
        return aria_result_err_ptr(aria_error_new(ARIA_ERR_OUT_OF_MEMORY, "Failed to allocate string", __FILE__, __LINE__));
    }
    *heap_str = result;
    return aria_result_ok_ptr(heap_str);
}

AriaResultPtr aria_string_pad_right(AriaString str, int64_t total_length, uint8_t pad_char) {
    if (total_length <= str.length) {
        // Already long enough, return copy
        return aria_string_from_bytes(str.data, str.length);
    }

    int64_t padding_needed = total_length - str.length;
    char* padded_data = (char*)aria_gc_alloc(total_length + 1, 0);
    if (!padded_data) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate padded string",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }

    // Copy original string
    if (str.length > 0) {
        memcpy(padded_data, str.data, str.length);
    }

    // Add padding
    memset(padded_data + str.length, (char)pad_char, padding_needed);
    padded_data[total_length] = '\0';

    AriaString result = {padded_data, total_length};
    AriaString* heap_str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0);
    if (!heap_str) {
        return aria_result_err_ptr(aria_error_new(ARIA_ERR_OUT_OF_MEMORY, "Failed to allocate string", __FILE__, __LINE__));
    }
    *heap_str = result;
    return aria_result_ok_ptr(heap_str);
}

AriaResultPtr aria_string_format_float(double value, int32_t precision) {
    if (precision < 0) precision = 6;  // Default precision
    if (precision > 17) precision = 17;  // Max meaningful precision for double

    char buffer[64];
    int len = snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    if (len < 0 || len >= (int)sizeof(buffer)) {
        AriaError* error = aria_error_new(
            ARIA_ERR_INVALID_ARG,
            "Failed to format float",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    return aria_string_from_bytes(buffer, len);
}

// ═══════════════════════════════════════════════════════════════════════
// Simple Wrapper Functions (abort on error, for builtin use)
// ═══════════════════════════════════════════════════════════════════════

AriaString* aria_string_from_int_simple(int64_t value) {
    AriaResultPtr result = aria_string_from_int(value);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

int64_t aria_string_to_int_simple(AriaString* str) {
    AriaResultI64 result = aria_string_to_int(*str);
    if (result.error != NULL) { std::abort(); }
    return result.value;
}

AriaString* aria_string_to_hex_simple(AriaString* str) {
    AriaResultPtr result = aria_string_to_hex(*str);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

AriaString* aria_string_pad_right_simple(AriaString* str, int64_t total_length, uint8_t pad_char) {
    AriaResultPtr result = aria_string_pad_right(*str, total_length, pad_char);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

AriaString* aria_string_format_float_simple(double value, int32_t precision) {
    AriaResultPtr result = aria_string_format_float(value, precision);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

AriaString* aria_string_from_char_simple(uint8_t ch) {
    AriaResultPtr result = aria_string_from_char(ch);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

AriaString* aria_string_from_cstr_simple(const char* cstr) {
    AriaResultPtr result = aria_string_from_cstr(cstr);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

// env_get(name: string) -> string builtin runtime
// Returns the value of the named environment variable, or empty string if unset.
extern "C" AriaString* aria_env_get_builtin(AriaString name) {
    // AriaString.data is always null-terminated (see make_aria_str / alloc paths)
    const char* val = std::getenv(name.data);
    return aria_string_from_cstr_simple(val ? val : "");
}

// sort_lines(content: string) -> string  builtin runtime
// Splits content by newlines, sorts lines lexicographically (O(n log n)),
// then joins them back with newlines.
extern "C" AriaString* aria_sort_lines(AriaString* content) {
    if (!content || content->length == 0) {
        return aria_string_from_cstr_simple("");
    }

    // Split into lines
    std::vector<std::string> lines;
    const char* data = content->data;
    int64_t len = content->length;
    int64_t start = 0;
    for (int64_t i = 0; i < len; i++) {
        if (data[i] == '\n') {
            lines.emplace_back(data + start, i - start);
            start = i + 1;
        }
    }
    // Last line (possibly without trailing newline)
    if (start < len) {
        lines.emplace_back(data + start, len - start);
    }

    // Sort lexicographically — std::string::operator< does exactly this
    std::sort(lines.begin(), lines.end());

    // Join with newlines
    std::string result;
    for (size_t i = 0; i < lines.size(); i++) {
        if (i > 0) result += '\n';
        result += lines[i];
    }
    // Preserve trailing newline if original had one
    if (len > 0 && data[len - 1] == '\n') {
        result += '\n';
    }

    return aria_string_from_cstr_simple(result.c_str());
}

AriaString* aria_string_concat_simple(AriaString* a, AriaString* b) {
    AriaResultPtr result = aria_string_concat(*a, *b);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

// ── New _simple wrappers ─────────────────────────────────────────────────────

AriaString* aria_string_repeat_simple(AriaString* str, int64_t count) {
    AriaResultPtr result = aria_string_repeat(*str, count);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

AriaString* aria_string_trim_start_simple(AriaString* str) {
    AriaResultPtr result = aria_string_trim_start(*str);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

AriaString* aria_string_trim_end_simple(AriaString* str) {
    AriaResultPtr result = aria_string_trim_end(*str);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

AriaString* aria_string_trim_simple(AriaString* str) {
    AriaResultPtr result = aria_string_trim(*str);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

AriaString* aria_string_to_upper_simple(AriaString* str) {
    AriaResultPtr result = aria_string_to_upper(*str);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

AriaString* aria_string_to_lower_simple(AriaString* str) {
    AriaResultPtr result = aria_string_to_lower(*str);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

int64_t aria_string_index_of_simple(AriaString* haystack, AriaString* needle) {
    AriaResultI64 result = aria_string_index_of(*haystack, *needle);
    if (result.error != NULL) { std::abort(); }
    return result.value;
}

AriaString* aria_string_substring_simple(AriaString* str, int64_t start, int64_t end) {
    AriaResultPtr result = aria_string_substring(*str, start, end);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

// pad_left: prepend pad_char until string reaches total_length
AriaResultPtr aria_string_pad_left(AriaString str, int64_t total_length, uint8_t pad_char) {
    if (str.length >= (size_t)total_length) {
        // Already at or over target length — return as-is
        return aria_string_from_bytes(str.data, str.length);
    }
    int64_t pad_count = total_length - (int64_t)str.length;
    int64_t full_len = total_length;
    char* buf = (char*)aria_gc_alloc(full_len + 1, 0);
    if (!buf) {
        return aria_result_err_ptr(aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY, "pad_left: alloc failed", __FILE__, __LINE__));
    }
    memset(buf, (char)pad_char, pad_count);
    memcpy(buf + pad_count, str.data, str.length);
    buf[full_len] = '\0';
    AriaString result = {buf, full_len};
    AriaString* heap_str = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0);
    if (!heap_str) {
        return aria_result_err_ptr(aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY, "pad_left: alloc string failed", __FILE__, __LINE__));
    }
    *heap_str = result;
    return aria_result_ok_ptr(heap_str);
}

AriaString* aria_string_pad_left_simple(AriaString* str, int64_t total_length, uint8_t pad_char) {
    AriaResultPtr result = aria_string_pad_left(*str, total_length, pad_char);
    if (result.is_error) { std::abort(); }
    return (AriaString*)result.value;
}

// Format an int64 value as lowercase hexadecimal string (no prefix).
// Example: 255 → "ff",  0 → "0"
AriaString* aria_string_from_int_hex_simple(int64_t value) {
    // 16 hex digits + sign + null terminator
    char buf[32];
    int len;
    if (value < 0) {
        // Represent as unsigned 64-bit hex for negative values
        len = snprintf(buf, sizeof(buf), "%llx", (unsigned long long)(uint64_t)value);
    } else {
        len = snprintf(buf, sizeof(buf), "%llx", (unsigned long long)value);
    }
    if (len <= 0) {
        buf[0] = '0'; len = 1;
    }
    char* data = (char*)aria_gc_alloc(len + 1, 0);
    if (!data) std::abort();
    memcpy(data, buf, len);
    data[len] = '\0';
    AriaString* result = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0);
    if (!result) std::abort();
    result->data = data;
    result->length = len;
    return result;
}

// ── Byte-level accessor ───────────────────────────────────────────────────────
// Used by JSON parser and other byte-scanning code.
// Extern declaration in Aria: extern func:aria_string_byte_at = int64(string:s, int64:i);
// Note: Aria codegen passes a `string` extern arg as char* (extracts .data field).
// So the first param must be const char*, NOT AriaString* or AriaString by value.
int64_t aria_string_byte_at(const char* data, int64_t idx) {
    if (!data || idx < 0) return -1;
    return (int64_t)(unsigned char)data[idx];
}

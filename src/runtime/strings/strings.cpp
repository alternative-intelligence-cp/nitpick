/**
 * Phase 6.3 Standard Library - String Operations Implementation
 * 
 * High-level string manipulation functions.
 */

#include "runtime/strings.h"
#include "runtime/gc.h"
#include <cstring>
#include <cctype>
#include <cstdlib>

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

AriaResultI64 aria_string_index_of(AriaString haystack, AriaString needle) {
    if (needle.length == 0) {
        // Empty needle always matches at position 0
        return aria_result_ok_i64(0);
    }
    
    if (needle.length > haystack.length) {
        // Needle longer than haystack, cannot match
        AriaError* error = aria_error_new(
            ARIA_ERR_NOT_FOUND,
            "Substring not found",
            __FILE__, __LINE__
        );
        return aria_result_err_i64(error);
    }
    
    // Simple string search (could be optimized with Boyer-Moore or similar)
    for (int64_t i = 0; i <= haystack.length - needle.length; i++) {
        if (memcmp(haystack.data + i, needle.data, needle.length) == 0) {
            return aria_result_ok_i64(i);
        }
    }
    
    // Not found
    AriaError* error = aria_error_new(
        ARIA_ERR_NOT_FOUND,
        "Substring not found",
        __FILE__, __LINE__
    );
    return aria_result_err_i64(error);
}

bool aria_string_contains(AriaString haystack, AriaString needle) {
    AriaResultI64 result = aria_string_index_of(haystack, needle);
    return result.error == NULL;
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

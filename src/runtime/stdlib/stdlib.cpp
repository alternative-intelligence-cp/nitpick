/**
 * Aria Standard Library Runtime Support - Implementation
 * 
 * Provides C++ implementations of standard library functions that can be
 * called from compiled Aria code.
 * 
 * Reference: research_031_essential_stdlib.txt
 */

#include "runtime/stdlib.h"
#include "runtime/gc.h"
#include "runtime/allocators.h"
#include <unistd.h>     // For write()
#include <string.h>     // For strlen(), strcmp()
#include <stdlib.h>     // For malloc(), free()
#include <math.h>       // For sqrt(), pow()
#include <stdio.h>      // For fprintf() (error reporting)

// ============================================================================
// I/O Functions (std.io)
// ============================================================================

void aria_print(const char* str, int64_t len) {
    if (!str || len <= 0) return;
    
    // Write to stdout (fd 1)
    ssize_t written = write(STDOUT_FILENO, str, (size_t)len);
    (void)written;  // Ignore errors for basic print
}

void aria_println(const char* str, int64_t len) {
    aria_print(str, len);
    write(STDOUT_FILENO, "\n", 1);
}

void aria_eprint(const char* str, int64_t len) {
    if (!str || len <= 0) return;
    
    // Write to stderr (fd 2)
    ssize_t written = write(STDERR_FILENO, str, (size_t)len);
    (void)written;
}

void aria_eprintln(const char* str, int64_t len) {
    aria_eprint(str, len);
    write(STDERR_FILENO, "\n", 1);
}

void aria_debug(const char* str, int64_t len) {
    if (!str || len <= 0) return;
    
    // Write to stddbg (fd 3)
    ssize_t written = write(3, str, (size_t)len);
    (void)written;
}

void aria_debugln(const char* str, int64_t len) {
    aria_debug(str, len);
    write(3, "\n", 1);
}

// ============================================================================
// Memory Functions (std.mem)
// ============================================================================

void* aria_std_gc_alloc(int64_t size) {
    if (size <= 0) return NULL;
    // Type ID 0 means generic/unknown type (conservative scanning)
    return aria_gc_alloc((size_t)size, 0);
}

void* aria_std_alloc(int64_t size) {
    if (size <= 0) return NULL;
    return aria_alloc((size_t)size);
}

void aria_std_free(void* ptr) {
    aria_free(ptr);
}

void* aria_std_alloc_exec(int64_t size) {
    if (size <= 0) return NULL;
    WildXGuard guard = aria_alloc_exec((size_t)size);
    // Return just the pointer (caller is responsible for sealing/cleanup)
    return guard.ptr;
}

void aria_std_free_exec(void* ptr) {
    if (!ptr) return;
    // For simplicity, use regular free (wildx pages are just mmapped memory)
    aria_free(ptr);
}

// ============================================================================
// String Functions (std.string)
// ============================================================================

int64_t aria_cstr_length(const char* str) {
    if (!str) return 0;
    return (int64_t)strlen(str);
}

int32_t aria_string_compare(const char* s1, const char* s2) {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    
    int result = strcmp(s1, s2);
    return (int32_t)result;
}

char* aria_cstr_concat(const char* s1, const char* s2) {
    if (!s1 && !s2) return NULL;
    if (!s1) s1 = "";
    if (!s2) s2 = "";
    
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    size_t total = len1 + len2 + 1;  // +1 for null terminator
    
    // Allocate on GC heap (type_id 0 for strings)
    char* result = (char*)aria_gc_alloc(total, 0);
    if (!result) return NULL;
    
    // Copy strings
    memcpy(result, s1, len1);
    memcpy(result + len1, s2, len2);
    result[len1 + len2] = '\0';
    
    return result;
}

// ============================================================================
// Math Functions (std.math)
// ============================================================================

int64_t aria_math_abs_i64(int64_t x) {
    // Handle INT64_MIN specially (abs would overflow)
    if (x == INT64_MIN) {
        return INT64_MAX;  // Best we can do without TBB
    }
    return (x < 0) ? -x : x;
}

double aria_math_abs_f64(double x) {
    return fabs(x);
}

double aria_math_sqrt(double x) {
    return sqrt(x);
}

double aria_math_pow(double x, double y) {
    return pow(x, y);
}

int64_t aria_math_min_i64(int64_t a, int64_t b) {
    return (a < b) ? a : b;
}

int64_t aria_math_max_i64(int64_t a, int64_t b) {
    return (a > b) ? a : b;
}

// ============================================================================
// Math Constants
// ============================================================================

double aria_math_pi(void) {
    return 3.141592653589793238462643383279502884;
}

double aria_math_e(void) {
    return 2.718281828459045235360287471352662498;
}

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

// Single-argument print for null-terminated C strings
// Used by core print() builtin for simple string literals
extern "C" int64_t npk_print_cstr(const char* str) {
    if (!str) return -1;
    size_t len = strlen(str);
    if (len == 0) return 0;
    ssize_t written = write(STDOUT_FILENO, str, len);
    return (int64_t)written;
}

// println: print string + newline (convenience wrapper)
extern "C" int64_t npk_println_cstr(const char* str) {
    if (!str) return -1;
    size_t len = strlen(str);
    ssize_t bytes = 0;
    
    // Write string
    if (len > 0) {
        ssize_t str_written = write(STDOUT_FILENO, str, len);
        if (str_written < 0) return -1;
        bytes += str_written;
    }
    
    // Write newline
    ssize_t nl_written = write(STDOUT_FILENO, "\n", 1);
    if (nl_written < 0) return -1;
    bytes += nl_written;
    
    return (int64_t)bytes;
}

// Two-argument version for stdlib use (length-aware)
void npk_print(const char* str, int64_t len) {
    if (!str || len <= 0) return;
    
    // Write to stdout (fd 1)
    ssize_t written = write(STDOUT_FILENO, str, (size_t)len);
    (void)written;  // Ignore errors for basic print
}

void npk_println(const char* str, int64_t len) {
    npk_print(str, len);
    write(STDOUT_FILENO, "\n", 1);
}

void npk_eprint(const char* str, int64_t len) {
    if (!str || len <= 0) return;
    
    // Write to stderr (fd 2)
    ssize_t written = write(STDERR_FILENO, str, (size_t)len);
    (void)written;
}

void npk_eprintln(const char* str, int64_t len) {
    npk_eprint(str, len);
    write(STDERR_FILENO, "\n", 1);
}

// Single-argument (null-terminated) versions used by eprint()/eprintln() builtins
extern "C" int64_t npk_eprint_cstr(const char* str) {
    if (!str) return -1;
    size_t len = strlen(str);
    if (len == 0) return 0;
    ssize_t written = write(STDERR_FILENO, str, len);
    return (int64_t)written;
}

extern "C" int64_t npk_eprintln_cstr(const char* str) {
    if (!str) return -1;
    size_t len = strlen(str);
    ssize_t bytes = 0;
    if (len > 0) {
        ssize_t str_written = write(STDERR_FILENO, str, len);
        if (str_written < 0) return -1;
        bytes += str_written;
    }
    ssize_t nl_written = write(STDERR_FILENO, "\n", 1);
    if (nl_written < 0) return -1;
    bytes += nl_written;
    return (int64_t)bytes;
}

void npk_debug(const char* str, int64_t len) {
    if (!str || len <= 0) return;
    
    // Write to stddbg (fd 3)
    ssize_t written = write(3, str, (size_t)len);
    (void)written;
}

void npk_debugln(const char* str, int64_t len) {
    npk_debug(str, len);
    write(3, "\n", 1);
}

// ============================================================================
// Memory Functions (std.mem)
// ============================================================================

void* npk_std_gc_alloc(int64_t size) {
    if (size <= 0) return NULL;
    // Type ID 0 means generic/unknown type (conservative scanning)
    return npk_gc_alloc((size_t)size, 0);
}

void* npk_std_alloc(int64_t size) {
    if (size <= 0) return NULL;
    return npk_alloc((size_t)size);
}

void npk_std_free(void* ptr) {
    npk_free(ptr);
}

void* npk_std_alloc_exec(int64_t size) {
    if (size <= 0) return NULL;
    WildXGuard guard = npk_alloc_exec((size_t)size);
    // Return just the pointer (caller is responsible for sealing/cleanup)
    return guard.ptr;
}

void npk_std_free_exec(void* ptr) {
    if (!ptr) return;
    // For simplicity, use regular free (wildx pages are just mmapped memory)
    npk_free(ptr);
}

// ============================================================================
// String Functions (std.string)
// ============================================================================

int64_t npk_cstr_length(const char* str) {
    if (!str) return 0;
    return (int64_t)strlen(str);
}

int32_t npk_string_compare(const char* s1, const char* s2) {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    
    int result = strcmp(s1, s2);
    return (int32_t)result;
}

char* npk_cstr_concat(const char* s1, const char* s2) {
    if (!s1 && !s2) return NULL;
    if (!s1) s1 = "";
    if (!s2) s2 = "";
    
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    size_t total = len1 + len2 + 1;  // +1 for null terminator
    
    // Allocate on GC heap (type_id 0 for strings)
    char* result = (char*)npk_gc_alloc(total, 0);
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

int64_t npk_math_abs_i64(int64_t x) {
    // Handle INT64_MIN specially (abs would overflow)
    if (x == INT64_MIN) {
        return INT64_MAX;  // Best we can do without TBB
    }
    return (x < 0) ? -x : x;
}

double npk_math_abs_f64(double x) {
    return fabs(x);
}

double npk_math_sqrt(double x) {
    return sqrt(x);
}

double npk_math_pow(double x, double y) {
    return pow(x, y);
}

int64_t npk_math_min_i64(int64_t a, int64_t b) {
    return (a < b) ? a : b;
}

int64_t npk_math_max_i64(int64_t a, int64_t b) {
    return (a > b) ? a : b;
}

// ============================================================================
// Math Constants
// ============================================================================

double npk_math_pi(void) {
    return 3.141592653589793238462643383279502884;
}

double npk_math_e(void) {
    return 2.718281828459045235360287471352662498;
}

// ============================================================================
// Terminal / Display Helpers
// (Used by demos, e.g. the spinning donut renderer — ANSI escapes can't be
//  embedded in Aria string literals because the lexer only handles \n \t \r \\)
// ============================================================================

#include <time.h>   // nanosleep

// Clear the terminal screen and move cursor to home position.
extern "C" void npk_term_clear(void) {
    // ESC[2J  = erase display,  ESC[H = cursor home
    write(STDOUT_FILENO, "\033[2J\033[H", 7);
}

// Hide the terminal cursor (reduces flicker during animation).
extern "C" void npk_term_hide_cursor(void) {
    write(STDOUT_FILENO, "\033[?25l", 6);
}

// Restore the terminal cursor.
extern "C" void npk_term_show_cursor(void) {
    write(STDOUT_FILENO, "\033[?25h", 6);
}

// Write a raw byte buffer of `len` bytes to stdout.
// Used to flush a full frame buffer in a single syscall.
extern "C" void npk_write_bytes(const char* buf, int64_t len) {
    if (!buf || len <= 0) return;
    ssize_t written = write(STDOUT_FILENO, buf, (size_t)len);
    (void)written;
}

// Flush stdout (for stdio-buffered output paths).
extern "C" void npk_flush_stdout(void) {
    fflush(stdout);
}

// Sleep for the given number of milliseconds.
extern "C" void npk_sleep_ms(int64_t ms) {
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec  = (time_t)(ms / 1000);
    ts.tv_nsec = (long)((ms % 1000) * 1000000L);
    nanosleep(&ts, NULL);
}

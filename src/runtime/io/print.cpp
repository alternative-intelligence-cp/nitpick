/**
 * src/runtime/io/print.cpp
 *
 * Aria Standard Library - Print and Output Functions
 * Version: 0.0.6
 *
 * Provides print() and related output functions for Aria programs.
 * These are extern "C" functions callable from LLVM IR.
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <unistd.h>

extern "C" {

// Print a null-terminated string to stdout
void aria_print_string(const char* str) {
    if (str) {
        fputs(str, stdout);
    }
}

// Print a string with newline
void aria_println_string(const char* str) {
    if (str) {
        fputs(str, stdout);
        fputc('\n', stdout);
    }
}

// Print an integer (int64)
void aria_print_int64(int64_t value) {
    printf("%lld", (long long)value);
}

// Print an integer with newline
void aria_println_int64(int64_t value) {
    printf("%lld\n", (long long)value);
}

// Print a floating-point number (double)
void aria_print_float64(double value) {
    printf("%g", value);
}

// Print a floating-point number with newline
void aria_println_float64(double value) {
    printf("%g\n", value);
}

// Print a boolean value
void aria_print_bool(bool value) {
    fputs(value ? "true" : "false", stdout);
}

// Print a boolean value with newline
void aria_println_bool(bool value) {
    printf("%s\n", value ? "true" : "false");
}

// Generic print with newline (calls flush)
void aria_println() {
    fputc('\n', stdout);
    fflush(stdout);
}

// Flush stdout
void aria_flush() {
    fflush(stdout);
}

// Print to stderr
void aria_eprint_string(const char* str) {
    if (str) {
        fputs(str, stderr);
    }
}

void aria_eprintln_string(const char* str) {
    if (str) {
        fputs(str, stderr);
        fputc('\n', stderr);
    }
}

// Debug channel output (stddati - data in, channel 4)
void aria_debug_print(const char* str) {
    if (str) {
        // Write to file descriptor 4 (stddati)
        // Falls back to stderr if fd 4 not available
        ssize_t result = write(4, str, strlen(str));
        if (result < 0) {
            fputs(str, stderr);
        }
    }
}

} // extern "C"

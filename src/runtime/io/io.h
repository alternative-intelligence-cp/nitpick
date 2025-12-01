/**
 * src/runtime/io/io.h
 *
 * Aria Runtime I/O Library Header
 * Declares all I/O functions available to Aria programs
 */

#ifndef ARIA_RUNTIME_IO_H
#define ARIA_RUNTIME_IO_H

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Print Functions
// ============================================================================

void aria_print_string(const char* str);
void aria_println_string(const char* str);
void aria_print_int64(int64_t value);
void aria_println_int64(int64_t value);
void aria_print_float64(double value);
void aria_println_float64(double value);
void aria_print_bool(bool value);
void aria_println_bool(bool value);
void aria_println();
void aria_flush();

// Error output
void aria_eprint_string(const char* str);
void aria_eprintln_string(const char* str);

// Debug channel
void aria_debug_print(const char* str);

// ============================================================================
// File I/O Functions
// ============================================================================

char* aria_read_file(const char* path);
int64_t aria_write_file(const char* path, const char* content);
int64_t aria_append_file(const char* path, const char* content);
int64_t aria_file_exists(const char* path);
int64_t aria_file_size(const char* path);
int64_t aria_delete_file(const char* path);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_IO_H

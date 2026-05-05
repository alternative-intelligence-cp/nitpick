/**
 * Minimal C helpers for Aria's pure-Aria string manipulation module.
 *
 * These are the irreducible operations that cannot be expressed in Aria:
 *   - GC-allocated string construction from raw bytes
 *   - Byte-level read access into string data (Aria passes strings as char*)
 *   - Integer parsing via strtoll (needs libc)
 *   - String byte copy into a malloc'd buffer
 *
 * The Aria compiler extracts .data (char*) from AriaString when passing to
 * extern functions. So all string parameters arrive as const char*.
 * String returns go back as AriaString* (heap-allocated on GC).
 */

#include "runtime/strings.h"
#include "runtime/gc.h"
#include <cstring>
#include <cstdlib>
#include <cstdint>

static AriaString* make_aria_string(const char* data, int64_t length) {
    char* buf = (char*)npk_gc_alloc(length + 1, 0);
    if (!buf) std::abort();
    if (length > 0) memcpy(buf, data, length);
    buf[length] = '\0';
    AriaString* s = (AriaString*)npk_gc_alloc(sizeof(AriaString), 0);
    if (!s) std::abort();
    s->data = buf;
    s->length = length;
    return s;
}

extern "C" {

// ═══════════════════════════════════════════════════════════════════════
// String construction from raw buffer
// ═══════════════════════════════════════════════════════════════════════

// Build AriaString from bytes in a malloc'd buffer (buf stored as int64 ptr)
// Used by split/join/replace/reverse which build results in int64-> handles
AriaString* sh_make_str(int64_t* buf, int64_t offset, int64_t len) {
    uint8_t* data = (uint8_t*)(uintptr_t)buf[0];
    return make_aria_string((const char*)(data + offset), len);
}

// Build AriaString directly from char* + length (for str_char_at etc.)
AriaString* sh_make_str_direct(const char* data, int64_t offset, int64_t len) {
    return make_aria_string(data + offset, len);
}

// Return an empty AriaString
AriaString* sh_empty_str() {
    return npk_string_empty();
}

// ═══════════════════════════════════════════════════════════════════════
// Byte-level string access
// ═══════════════════════════════════════════════════════════════════════

// Get byte value at index within a string (string arrives as char*)
int64_t sh_byte_at(const char* s, int64_t idx) {
    if (!s) return -1;
    return (int64_t)(unsigned char)s[idx];
}

// Get string length (strlen on the char* that Aria passes)
int64_t sh_strlen(const char* s) {
    if (!s) return 0;
    return (int64_t)strlen(s);
}

// ═══════════════════════════════════════════════════════════════════════
// Buffer operations for building strings
// ═══════════════════════════════════════════════════════════════════════

// Allocate a working buffer, return pointer as int64
// Handle: int64-> with [0] = buffer_ptr_as_int64
void sh_alloc_buf(int64_t* h, int64_t size) {
    char* buf = (char*)malloc(size);
    if (!buf) std::abort();
    h[0] = (int64_t)(uintptr_t)buf;
}

void sh_free_buf(int64_t* h) {
    char* buf = (char*)(uintptr_t)h[0];
    free(buf);
}

// Set a byte in the working buffer
void sh_set_byte(int64_t* h, int64_t offset, int32_t val) {
    char* buf = (char*)(uintptr_t)h[0];
    buf[offset] = (char)val;
}

// Get a byte from the working buffer
int32_t sh_get_byte(int64_t* h, int64_t offset) {
    char* buf = (char*)(uintptr_t)h[0];
    return (int32_t)(unsigned char)buf[offset];
}

// Copy bytes from a string (char*) into the working buffer
void sh_copy_from_str(int64_t* h, int64_t buf_offset, const char* src, int64_t src_offset, int64_t len) {
    char* buf = (char*)(uintptr_t)h[0];
    memcpy(buf + buf_offset, src + src_offset, len);
}

// ═══════════════════════════════════════════════════════════════════════
// Split handle operations
// ═══════════════════════════════════════════════════════════════════════

// Allocate array of AriaString pointers for split results
// split_handle: int64-> with [0] = parts_array_ptr, [1] = count
void sh_split_alloc(int64_t* h, int64_t count) {
    AriaString** parts = (AriaString**)npk_gc_alloc(sizeof(AriaString*) * count, 0);
    if (!parts) std::abort();
    h[0] = (int64_t)(uintptr_t)parts;
    h[1] = count;
}

// Store a string part into the split array at index
void sh_split_set(int64_t* h, int64_t idx, const char* data, int64_t offset, int64_t len) {
    AriaString** parts = (AriaString**)(uintptr_t)h[0];
    parts[idx] = make_aria_string(data + offset, len);
}

// Get a string part from the split array at index
AriaString* sh_split_get(int64_t* h, int64_t idx) {
    AriaString** parts = (AriaString**)(uintptr_t)h[0];
    return parts[idx];
}

// Get the length of a stored split part (for join)
int64_t sh_split_part_len(int64_t* h, int64_t idx) {
    AriaString** parts = (AriaString**)(uintptr_t)h[0];
    return parts[idx]->length;
}

// Copy a split part's data into a working buffer (for join)
void sh_split_copy_to_buf(int64_t* dst_h, int64_t buf_offset, int64_t* src_h, int64_t idx) {
    char* buf = (char*)(uintptr_t)dst_h[0];
    AriaString** parts = (AriaString**)(uintptr_t)src_h[0];
    AriaString* part = parts[idx];
    if (part->length > 0) {
        memcpy(buf + buf_offset, part->data, part->length);
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Integer parsing
// ═══════════════════════════════════════════════════════════════════════

int64_t sh_parse_int(const char* s) {
    if (!s || !*s) return 0;
    char* endptr = nullptr;
    return strtoll(s, &endptr, 10);
}

}  // extern "C"

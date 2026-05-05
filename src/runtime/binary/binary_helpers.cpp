/**
 * Minimal C helpers for Aria's pure-Aria binary serialization module.
 *
 * These are the irreducible operations that cannot be expressed in Aria:
 *   - Data buffer byte access (pointer stored as int64 in handle)
 *   - String construction from raw bytes (needs GC integration)
 *   - File I/O (needs libc stdio)
 *   - String bytes copy into buffer (needs char* extraction from AriaString)
 *
 * Handle layout (wild int64-> with 4 elements):
 *   h[0] = len       (bytes written)
 *   h[1] = cap       (data buffer capacity)
 *   h[2] = pos       (read cursor)
 *   h[3] = data_ptr  (address of uint8_t* data buffer, stored as int64)
 */

#include "runtime/strings.h"
#include "runtime/gc.h"
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>

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
// Data buffer byte access
// ═══════════════════════════════════════════════════════════════════════

void bh_set_byte(int64_t* h, int64_t offset, int32_t val) {
    uint8_t* data = (uint8_t*)(uintptr_t)h[3];
    data[offset] = (uint8_t)val;
}

int32_t bh_get_byte(int64_t* h, int64_t offset) {
    uint8_t* data = (uint8_t*)(uintptr_t)h[3];
    return (int32_t)data[offset];
}

// ═══════════════════════════════════════════════════════════════════════
// Data buffer lifecycle (alloc / grow / free)
// ═══════════════════════════════════════════════════════════════════════

void bh_alloc_data(int64_t* h, int64_t cap) {
    uint8_t* data = (uint8_t*)malloc(cap);
    if (!data) std::abort();
    h[3] = (int64_t)(uintptr_t)data;
}

void bh_realloc_data(int64_t* h, int64_t new_cap) {
    uint8_t* old = (uint8_t*)(uintptr_t)h[3];
    uint8_t* nd = (uint8_t*)realloc(old, new_cap);
    if (!nd) std::abort();
    h[3] = (int64_t)(uintptr_t)nd;
}

void bh_free_data(int64_t* h) {
    uint8_t* data = (uint8_t*)(uintptr_t)h[3];
    free(data);
}

// ═══════════════════════════════════════════════════════════════════════
// String ↔ buffer operations
// ═══════════════════════════════════════════════════════════════════════

// Create AriaString from bytes in data buffer
AriaString* bh_make_str(int64_t* h, int64_t offset, int64_t len) {
    uint8_t* data = (uint8_t*)(uintptr_t)h[3];
    return make_aria_string((const char*)(data + offset), len);
}

// Copy string bytes into data buffer at offset
void bh_copy_str(int64_t* h, int64_t offset, const char* str, int64_t len) {
    uint8_t* data = (uint8_t*)(uintptr_t)h[3];
    memcpy(data + offset, str, len);
}

// ═══════════════════════════════════════════════════════════════════════
// Float ↔ integer bit reinterpretation
// ═══════════════════════════════════════════════════════════════════════

int64_t bh_flt64_bits(double val) {
    int64_t bits;
    memcpy(&bits, &val, sizeof(bits));
    return bits;
}

double bh_flt64_from_bits(int64_t bits) {
    double val;
    memcpy(&val, &bits, sizeof(val));
    return val;
}

int32_t bh_flt32_bits(double val) {
    float f = (float)val;
    int32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}

double bh_flt32_from_bits(int32_t bits) {
    float f;
    memcpy(&f, &bits, sizeof(f));
    return (double)f;
}

// ═══════════════════════════════════════════════════════════════════════
// File I/O
// ═══════════════════════════════════════════════════════════════════════

// Read file into handle. Sets h[0]=len, h[1]=cap, h[2]=0,
// allocates/replaces data buffer. Returns 1 on success, 0 on error.
int32_t bh_read_file(int64_t* h, const char* path) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return 0;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0) { fclose(fp); return 0; }

    // Free old data if any
    if (h[3] != 0) {
        uint8_t* old = (uint8_t*)(uintptr_t)h[3];
        free(old);
    }

    int64_t cap = (size > 0) ? size : 1;
    uint8_t* data = (uint8_t*)malloc(cap);
    if (!data) { fclose(fp); return 0; }

    size_t rd = fread(data, 1, size, fp);
    fclose(fp);

    h[0] = (int64_t)rd;
    h[1] = cap;
    h[2] = 0;
    h[3] = (int64_t)(uintptr_t)data;
    return 1;
}

// Write handle's data buffer to file. Returns 1 on success, 0 on error.
int32_t bh_write_file(int64_t* h, const char* path) {
    int64_t len = h[0];
    if (len <= 0) return 0;
    uint8_t* data = (uint8_t*)(uintptr_t)h[3];

    FILE* fp = fopen(path, "wb");
    if (!fp) return 0;
    size_t written = fwrite(data, 1, len, fp);
    fclose(fp);
    return (written == (size_t)len) ? 1 : 0;
}

} // extern "C"

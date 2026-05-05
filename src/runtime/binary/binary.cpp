/**
 * Binary serialization runtime for Aria stdlib.
 *
 * Buffer-based binary encoder/decoder for structured binary data.
 *
 * API:
 *  - bin_new()                         → handle   create empty buffer
 *  - bin_from_file(path)               → handle   load from file
 *  - bin_to_file(handle, path)         → int32    save to file (1=ok, 0=err)
 *  - bin_write_int8(h, val)            → void
 *  - bin_write_int16(h, val)           → void
 *  - bin_write_int32(h, val)           → void
 *  - bin_write_int64(h, val)           → void
 *  - bin_write_flt32(h, val)           → void
 *  - bin_write_flt64(h, val)           → void
 *  - bin_write_str(h, val)             → void     length-prefixed string
 *  - bin_write_bool(h, val)            → void
 *  - bin_read_int8(h)                  → int32
 *  - bin_read_int16(h)                 → int32
 *  - bin_read_int32(h)                 → int32
 *  - bin_read_int64(h)                 → int64
 *  - bin_read_flt32(h)                 → flt64
 *  - bin_read_flt64(h)                 → flt64
 *  - bin_read_str(h)                   → string
 *  - bin_read_bool(h)                  → int32
 *  - bin_size(h)                       → int64    total bytes written
 *  - bin_pos(h)                        → int64    current read cursor
 *  - bin_seek(h, pos)                  → void     set read cursor
 *  - bin_remaining(h)                  → int64    bytes left to read
 *  - bin_free(h)                       → void     free buffer
 *
 * ABI: string params as const char*, string returns as AriaString*.
 */

#include "runtime/strings.h"
#include "runtime/gc.h"
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>

// ═══════════════════════════════════════════════════════════════════════
// Internal buffer structure
// ═══════════════════════════════════════════════════════════════════════

struct BinBuf {
    uint8_t* data;
    int64_t  len;   // bytes written
    int64_t  cap;   // allocated capacity
    int64_t  pos;   // read cursor
};

static BinBuf* bin_alloc() {
    BinBuf* b = (BinBuf*)calloc(1, sizeof(BinBuf));
    if (!b) std::abort();
    b->cap = 256;
    b->data = (uint8_t*)malloc(b->cap);
    if (!b->data) std::abort();
    b->len = 0;
    b->pos = 0;
    return b;
}

static void bin_ensure(BinBuf* b, int64_t extra) {
    while (b->len + extra > b->cap) {
        b->cap *= 2;
        b->data = (uint8_t*)realloc(b->data, b->cap);
        if (!b->data) std::abort();
    }
}

static void bin_write(BinBuf* b, const void* src, int64_t n) {
    bin_ensure(b, n);
    memcpy(b->data + b->len, src, n);
    b->len += n;
}

static bool bin_read(BinBuf* b, void* dst, int64_t n) {
    if (b->pos + n > b->len) return false;
    memcpy(dst, b->data + b->pos, n);
    b->pos += n;
    return true;
}

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

// ═══════════════════════════════════════════════════════════════════════
// Public C API
// ═══════════════════════════════════════════════════════════════════════

extern "C" {

// ── Constructor / File I/O ───────────────────────────────────────────

void* bin_new() {
    return (void*)bin_alloc();
}

void* bin_from_file(const char* path) {
    if (!path) return (void*)bin_alloc();
    FILE* f = fopen(path, "rb");
    if (!f) return (void*)bin_alloc();

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    BinBuf* b = bin_alloc();
    if (sz > 0) {
        bin_ensure(b, sz);
        size_t rd = fread(b->data, 1, sz, f);
        b->len = (int64_t)rd;
    }
    fclose(f);
    return (void*)b;
}

int32_t bin_to_file(void* handle, const char* path) {
    if (!handle || !path) return 0;
    BinBuf* b = (BinBuf*)handle;
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t written = fwrite(b->data, 1, b->len, f);
    fclose(f);
    return (written == (size_t)b->len) ? 1 : 0;
}

// ── Writers (little-endian) ──────────────────────────────────────────

void bin_write_int8(void* h, int32_t val) {
    if (!h) return;
    int8_t v = (int8_t)val;
    bin_write((BinBuf*)h, &v, 1);
}

void bin_write_int16(void* h, int32_t val) {
    if (!h) return;
    int16_t v = (int16_t)val;
    bin_write((BinBuf*)h, &v, 2);
}

void bin_write_int32(void* h, int32_t val) {
    if (!h) return;
    bin_write((BinBuf*)h, &val, 4);
}

void bin_write_int64(void* h, int64_t val) {
    if (!h) return;
    bin_write((BinBuf*)h, &val, 8);
}

void bin_write_flt32(void* h, double val) {
    if (!h) return;
    float v = (float)val;
    bin_write((BinBuf*)h, &v, 4);
}

void bin_write_flt64(void* h, double val) {
    if (!h) return;
    bin_write((BinBuf*)h, &val, 8);
}

void bin_write_str(void* h, const char* val) {
    if (!h) return;
    int64_t slen = val ? (int64_t)strlen(val) : 0;
    // Write length prefix (int64), then string bytes
    bin_write((BinBuf*)h, &slen, 8);
    if (slen > 0) bin_write((BinBuf*)h, val, slen);
}

void bin_write_bool(void* h, int32_t val) {
    if (!h) return;
    uint8_t v = val ? 1 : 0;
    bin_write((BinBuf*)h, &v, 1);
}

// ── Readers ──────────────────────────────────────────────────────────

int32_t bin_read_int8(void* h) {
    if (!h) return 0;
    int8_t v = 0;
    bin_read((BinBuf*)h, &v, 1);
    return (int32_t)v;
}

int32_t bin_read_int16(void* h) {
    if (!h) return 0;
    int16_t v = 0;
    bin_read((BinBuf*)h, &v, 2);
    return (int32_t)v;
}

int32_t bin_read_int32(void* h) {
    if (!h) return 0;
    int32_t v = 0;
    bin_read((BinBuf*)h, &v, 4);
    return v;
}

int64_t bin_read_int64(void* h) {
    if (!h) return 0;
    int64_t v = 0;
    bin_read((BinBuf*)h, &v, 8);
    return v;
}

double bin_read_flt32(void* h) {
    if (!h) return 0.0;
    float v = 0.0f;
    bin_read((BinBuf*)h, &v, 4);
    return (double)v;
}

double bin_read_flt64(void* h) {
    if (!h) return 0.0;
    double v = 0.0;
    bin_read((BinBuf*)h, &v, 8);
    return v;
}

AriaString* bin_read_str(void* h) {
    if (!h) return make_aria_string("", 0);
    int64_t slen = 0;
    if (!bin_read((BinBuf*)h, &slen, 8) || slen < 0) return make_aria_string("", 0);
    if (slen == 0) return make_aria_string("", 0);

    BinBuf* b = (BinBuf*)h;
    if (b->pos + slen > b->len) return make_aria_string("", 0);
    AriaString* result = make_aria_string((const char*)(b->data + b->pos), slen);
    b->pos += slen;
    return result;
}

int32_t bin_read_bool(void* h) {
    if (!h) return 0;
    uint8_t v = 0;
    bin_read((BinBuf*)h, &v, 1);
    return v ? 1 : 0;
}

// ── Buffer info ──────────────────────────────────────────────────────

int64_t bin_size(void* h) {
    if (!h) return 0;
    return ((BinBuf*)h)->len;
}

int64_t bin_pos(void* h) {
    if (!h) return 0;
    return ((BinBuf*)h)->pos;
}

void bin_seek(void* h, int64_t pos) {
    if (!h) return;
    BinBuf* b = (BinBuf*)h;
    if (pos < 0) pos = 0;
    if (pos > b->len) pos = b->len;
    b->pos = pos;
}

int64_t bin_remaining(void* h) {
    if (!h) return 0;
    BinBuf* b = (BinBuf*)h;
    return b->len - b->pos;
}

// ── Cleanup ──────────────────────────────────────────────────────────

void bin_free(void* h) {
    if (!h) return;
    BinBuf* b = (BinBuf*)h;
    free(b->data);
    free(b);
}

} // extern "C"

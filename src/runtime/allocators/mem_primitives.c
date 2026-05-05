/**
 * mem_primitives.c — Low-level memory primitives for the Aria runtime
 *
 * Provides byte-level and word-level memory access that cannot be expressed
 * in pure Aria (requires pointer dereference at sub-word granularity).
 * These are linked automatically via libaria_runtime.a — no C shim needed.
 *
 * Aria extern declarations:
 *   extern func:npk_mem_read_byte     = int64(int64:ptr, int64:offset);
 *   extern func:npk_mem_write_byte    = int64(int64:ptr, int64:offset, int64:val);
 *   extern func:npk_mem_read_int32    = int64(int64:ptr, int64:offset);
 *   extern func:npk_mem_read_int64    = int64(int64:ptr, int64:offset);
 *   extern func:npk_mem_write_int64   = int64(int64:ptr, int64:offset, int64:val);
 *   extern func:npk_mem_read_string   = string(int64:ptr, int64:max_len);
 *   extern func:npk_mem_write_string  = int64(int64:ptr, string:s);
 *   extern func:npk_mem_copy          = int64(int64:dst, int64:src, int64:n);
 *   extern func:npk_mem_set           = int64(int64:dst, int64:val, int64:n);
 *   extern func:npk_mem_compare       = int64(int64:a, int64:b, int64:n);
 *   extern func:npk_mem_offset        = int64(int64:ptr, int64:offset);
 *   extern func:npk_mem_ptr_size      = int64();
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* AriaString ABI: {char* data, int64_t length} returned by value */
typedef struct {
    char    *data;
    int64_t  length;
} AriaString;

/* ── Byte-level access ─────────────────────────────────────────────── */

int64_t npk_mem_read_byte(int64_t ptr, int64_t offset) {
    if (ptr == 0) return -1;
    const unsigned char *p = (const unsigned char *)(intptr_t)ptr;
    return (int64_t)p[offset];
}

int64_t npk_mem_write_byte(int64_t ptr, int64_t offset, int64_t val) {
    if (ptr == 0) return -1;
    unsigned char *p = (unsigned char *)(intptr_t)ptr;
    p[offset] = (unsigned char)(val & 0xFF);
    return 0;
}

/* ── Word-level access ─────────────────────────────────────────────── */

int64_t npk_mem_read_int32(int64_t ptr, int64_t offset) {
    if (ptr == 0) return 0;
    const int32_t *p = (const int32_t *)((const char *)(intptr_t)ptr + offset);
    return (int64_t)*p;
}

int64_t npk_mem_read_int64(int64_t ptr, int64_t offset) {
    if (ptr == 0) return 0;
    const int64_t *p = (const int64_t *)((const char *)(intptr_t)ptr + offset);
    return *p;
}

int64_t npk_mem_write_int64(int64_t ptr, int64_t offset, int64_t val) {
    if (ptr == 0) return -1;
    int64_t *p = (int64_t *)((char *)(intptr_t)ptr + offset);
    *p = val;
    return 0;
}

/* ── String <-> memory bridge ──────────────────────────────────────── */

AriaString npk_mem_read_string(int64_t ptr, int64_t max_len) {
    AriaString r = { NULL, 0 };
    if (ptr == 0 || max_len <= 0) return r;
    const char *src = (const char *)(intptr_t)ptr;
    int64_t len = 0;
    while (len < max_len && src[len] != '\0') len++;
    char *copy = (char *)malloc((size_t)len + 1);
    if (!copy) return r;
    memcpy(copy, src, (size_t)len);
    copy[len] = '\0';
    r.data   = copy;
    r.length = len;
    return r;
}

int64_t npk_mem_write_string(int64_t ptr, const char *s) {
    if (ptr == 0 || !s) return -1;
    size_t len = strlen(s);
    memcpy((void *)(intptr_t)ptr, s, len + 1);
    return (int64_t)len;
}

/* ── Bulk memory operations ────────────────────────────────────────── */

int64_t npk_mem_copy(int64_t dst, int64_t src, int64_t n) {
    if (dst == 0 || src == 0 || n <= 0) return 0;
    memcpy((void *)(intptr_t)dst, (const void *)(intptr_t)src, (size_t)n);
    return dst;
}

int64_t npk_mem_set(int64_t dst, int64_t val, int64_t n) {
    if (dst == 0 || n <= 0) return 0;
    memset((void *)(intptr_t)dst, (int)val, (size_t)n);
    return dst;
}

int64_t npk_mem_compare(int64_t a, int64_t b, int64_t n) {
    if (a == 0 || b == 0 || n <= 0) return 0;
    return (int64_t)memcmp((const void *)(intptr_t)a, (const void *)(intptr_t)b, (size_t)n);
}

/* ── Pointer math ──────────────────────────────────────────────────── */

int64_t npk_mem_offset(int64_t ptr, int64_t offset) {
    if (ptr == 0) return 0;
    return (int64_t)((intptr_t)ptr + offset);
}

int64_t npk_mem_ptr_size(void) {
    return (int64_t)sizeof(void *);
}

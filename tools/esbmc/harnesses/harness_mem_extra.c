// ESBMC harness for aria-libc mem_extra — provides valid pointers
// This harness verifies mem functions with properly allocated memory
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { const char* data; int64_t length; } AriaString;

// Function prototypes
void* aria_libc_mem_malloc(int64_t size);
void* aria_libc_mem_calloc(int64_t count, int64_t size);
void* aria_libc_mem_realloc(void* ptr, int64_t size);
void  aria_libc_mem_free(void* ptr);
void  aria_libc_mem_zero(void* ptr, int64_t n);
void  aria_libc_mem_copy(void* dst, void* src, int64_t n);
void  aria_libc_mem_move(void* dst, void* src, int64_t n);
int32_t aria_libc_mem_read_byte(void* ptr, int64_t offset);
void    aria_libc_mem_write_byte(void* ptr, int64_t offset, int32_t val);
int32_t aria_libc_mem_read_i32(void* ptr, int64_t offset);
void    aria_libc_mem_write_i32(void* ptr, int64_t offset, int32_t val);
int64_t aria_libc_mem_read_i64(void* ptr, int64_t offset);
void    aria_libc_mem_write_i64(void* ptr, int64_t offset, int64_t val);
AriaString aria_libc_mem_make_string(void* src_ptr, int64_t offset, int64_t len);
void aria_libc_mem_copy_string(void* dst_ptr, int64_t offset, const char* src, int64_t len);

int main() {
    // Test allocators
    void* p = aria_libc_mem_malloc(128);
    if (!p) return 1;

    // Test zero
    aria_libc_mem_zero(p, 128);

    // Test byte read/write within bounds
    aria_libc_mem_write_byte(p, 0, 0xAB);
    int32_t b = aria_libc_mem_read_byte(p, 0);
    __ESBMC_assert(b == 0xAB, "byte read/write roundtrip");

    // Test i32 read/write within bounds
    aria_libc_mem_write_i32(p, 0, 42);
    int32_t v32 = aria_libc_mem_read_i32(p, 0);
    __ESBMC_assert(v32 == 42, "i32 read/write roundtrip");

    // Test i64 read/write within bounds
    aria_libc_mem_write_i64(p, 0, 999999LL);
    int64_t v64 = aria_libc_mem_read_i64(p, 0);
    __ESBMC_assert(v64 == 999999LL, "i64 read/write roundtrip");

    // Test copy/move
    void* q = aria_libc_mem_malloc(128);
    if (!q) { aria_libc_mem_free(p); return 1; }
    aria_libc_mem_copy(q, p, 64);
    aria_libc_mem_move(q, p, 64); // overlapping-safe

    // Test make_string within bounds
    const char* msg = "hello";
    memcpy(p, msg, 5);
    AriaString s = aria_libc_mem_make_string(p, 0, 5);
    __ESBMC_assert(s.length == 5, "make_string length");

    // Test copy_string within bounds
    aria_libc_mem_copy_string(q, 0, "world", 5);

    // Test calloc and realloc
    void* c = aria_libc_mem_calloc(4, 32);
    if (c) {
        c = aria_libc_mem_realloc(c, 256);
        aria_libc_mem_free(c);
    }

    // Test null cases
    int32_t null_byte = aria_libc_mem_read_byte(NULL, 0);
    __ESBMC_assert(null_byte == -1, "null read_byte returns -1");

    AriaString null_str = aria_libc_mem_make_string(NULL, 0, 5);
    __ESBMC_assert(null_str.length == 0, "null make_string returns empty");

    // Cleanup
    if (s.data && s.data[0] != '\0') free((void*)s.data);
    aria_libc_mem_free(q);
    aria_libc_mem_free(p);
    return 0;
}

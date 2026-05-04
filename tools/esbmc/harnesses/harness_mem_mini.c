// Minimal ESBMC harness for mem_extra core functions
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { const char* data; int64_t length; } AriaString;

// Inline the implementation so ESBMC can analyze it
void* aria_libc_mem_malloc(int64_t size) { return malloc((size_t)size); }
void  aria_libc_mem_free(void* ptr) { free(ptr); }
void  aria_libc_mem_zero(void* ptr, int64_t n) { memset(ptr, 0, (size_t)n); }
void  aria_libc_mem_copy(void* dst, void* src, int64_t n) { memcpy(dst, src, (size_t)n); }

int32_t aria_libc_mem_read_byte(void* ptr, int64_t offset) {
    if (!ptr) return -1;
    return (int32_t)(*(uint8_t*)((char*)ptr + offset));
}
void aria_libc_mem_write_byte(void* ptr, int64_t offset, int32_t val) {
    if (!ptr) return;
    *(uint8_t*)((char*)ptr + offset) = (uint8_t)(val & 0xFF);
}
int32_t aria_libc_mem_read_i32(void* ptr, int64_t offset) {
    int32_t val;
    memcpy(&val, (char*)ptr + offset, sizeof(int32_t));
    return val;
}
void aria_libc_mem_write_i32(void* ptr, int64_t offset, int32_t val) {
    memcpy((char*)ptr + offset, &val, sizeof(int32_t));
}

AriaString aria_libc_mem_make_string(void* src_ptr, int64_t offset, int64_t len) {
    AriaString result;
    if (!src_ptr || len <= 0) {
        result.data = "";
        result.length = 0;
        return result;
    }
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) {
        result.data = "";
        result.length = 0;
        return result;
    }
    memcpy(buf, (char*)src_ptr + offset, (size_t)len);
    buf[len] = '\0';
    result.data = buf;
    result.length = len;
    return result;
}

int main() {
    // Allocate buffer
    void* p = aria_libc_mem_malloc(64);
    if (!p) return 1;
    aria_libc_mem_zero(p, 64);

    // Write and read a byte at valid offset
    aria_libc_mem_write_byte(p, 10, 0xAB);
    int32_t b = aria_libc_mem_read_byte(p, 10);
    __ESBMC_assert(b == 0xAB, "byte roundtrip");

    // Write and read i32 at valid offset
    aria_libc_mem_write_i32(p, 0, 12345);
    int32_t v = aria_libc_mem_read_i32(p, 0);
    __ESBMC_assert(v == 12345, "i32 roundtrip");

    // make_string with valid data
    char *buf = (char*)p;
    buf[0] = 'H'; buf[1] = 'i'; buf[2] = '\0';
    AriaString s = aria_libc_mem_make_string(p, 0, 2);
    if (s.length == 0) {
        __ESBMC_assert(s.data[0] == '\0', "oom returns empty string");
    } else {
        __ESBMC_assert(s.length == 2, "string length");
        free((void*)s.data);
    }

    AriaString empty = aria_libc_mem_make_string(NULL, 0, 2);
    __ESBMC_assert(empty.length == 0, "null make_string length");
    __ESBMC_assert(empty.data[0] == '\0', "null make_string data");

    // Null safety
    __ESBMC_assert(aria_libc_mem_read_byte(NULL, 0) == -1, "null returns -1");

    aria_libc_mem_free(p);
    return 0;
}

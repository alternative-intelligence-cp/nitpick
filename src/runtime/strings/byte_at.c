/**
 * npk_string_byte_at — single-byte accessor for Aria strings
 *
 * AriaString is { const char* data; int64_t length }. This gives Aria code
 * a way to do character-by-character string parsing without depending on the
 * stub char_at in stdlib/core.aria (which always returns 0).
 */
#include <stdint.h>

typedef struct {
    const char *data;
    int64_t     length;
} AriaString;

/**
 * Return the byte value at position `idx` in `str` as an int64.
 * Returns -1 on out-of-bounds.  ASCII printable characters have values 0..127.
 * Returns int64 so Aria code can use the value directly without casts.
 *
 * NOTE: The Aria compiler passes `string` parameters as AriaString* (pointer to
 * the struct), not AriaString by value.  The LLVM IR shows:
 *   declare i64 @npk_string_byte_at(ptr, i64)
 * where the first ptr is &AriaString and the second i64 is idx.
 */
int64_t npk_string_byte_at(const AriaString *str, int64_t idx) {
    if (!str || idx < 0 || idx >= str->length) return -1;
    return (int64_t)(unsigned char)str->data[idx];
}

/**
 * Typed string wrappers for Aria stdlib.
 * 
 * CRITICAL ABI NOTE: Aria codegen extracts the .data (char*) field from
 * AriaString when passing to extern functions. So all string parameters
 * arrive as `const char*`, NOT AriaString*. We use strlen() since all
 * Aria strings are null-terminated.
 * 
 * String return values should be AriaString* (heap-allocated).
 *
 * Functions provided:
 * - replace / replace_first
 * - split / split_count / split_get / join
 * - byte_at / char_at
 * - to_int (parse)
 * - reverse
 * - count_occurrences
 */

#include "runtime/strings.h"
#include "runtime/gc.h"
#include "runtime/collections.h"
#include <cstring>
#include <cstdlib>
#include <cstdint>

extern "C" {

// Helper: allocate an AriaString on the GC heap
static AriaString* make_string(const char* data, int64_t length) {
    char* buf = (char*)aria_gc_alloc(length + 1, 0);
    if (!buf) std::abort();
    if (length > 0) memcpy(buf, data, length);
    buf[length] = '\0';
    AriaString* s = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0);
    if (!s) std::abort();
    s->data = buf;
    s->length = length;
    return s;
}

// ═══════════════════════════════════════════════════════════════════════
// str_replace — replace all non-overlapping occurrences
// ═══════════════════════════════════════════════════════════════════════

AriaString* str_replace(const char* src, const char* old_s, const char* new_s) {
    if (!src) return aria_string_empty();
    int64_t src_len = (int64_t)strlen(src);
    int64_t old_len = old_s ? (int64_t)strlen(old_s) : 0;
    int64_t new_len = new_s ? (int64_t)strlen(new_s) : 0;

    if (old_len == 0) return make_string(src, src_len);

    // Count occurrences
    int64_t count = 0;
    for (int64_t i = 0; i <= src_len - old_len; i++) {
        if (memcmp(src + i, old_s, old_len) == 0) {
            count++;
            i += old_len - 1;
        }
    }
    if (count == 0) return make_string(src, src_len);

    int64_t result_len = src_len + count * (new_len - old_len);
    char* buf = (char*)aria_gc_alloc(result_len + 1, 0);
    if (!buf) std::abort();

    int64_t pos = 0, i = 0;
    while (i < src_len) {
        if (i <= src_len - old_len && memcmp(src + i, old_s, old_len) == 0) {
            if (new_len > 0) memcpy(buf + pos, new_s, new_len);
            pos += new_len;
            i += old_len;
        } else {
            buf[pos++] = src[i++];
        }
    }
    buf[result_len] = '\0';

    return make_string(buf, result_len);
}

// ═══════════════════════════════════════════════════════════════════════
// str_replace_first — replace first occurrence only
// ═══════════════════════════════════════════════════════════════════════

AriaString* str_replace_first(const char* src, const char* old_s, const char* new_s) {
    if (!src) return aria_string_empty();
    int64_t src_len = (int64_t)strlen(src);
    int64_t old_len = old_s ? (int64_t)strlen(old_s) : 0;
    int64_t new_len = new_s ? (int64_t)strlen(new_s) : 0;

    if (old_len == 0) return make_string(src, src_len);

    // Find first occurrence
    const char* found = strstr(src, old_s);
    if (!found) return make_string(src, src_len);

    int64_t pos = found - src;
    int64_t result_len = src_len + (new_len - old_len);
    char* buf = (char*)aria_gc_alloc(result_len + 1, 0);
    if (!buf) std::abort();

    memcpy(buf, src, pos);
    if (new_len > 0) memcpy(buf + pos, new_s, new_len);
    memcpy(buf + pos + new_len, src + pos + old_len, src_len - pos - old_len);
    buf[result_len] = '\0';

    return make_string(buf, result_len);
}

// ═══════════════════════════════════════════════════════════════════════
// str_count — count non-overlapping occurrences
// ═══════════════════════════════════════════════════════════════════════

int64_t str_count(const char* haystack, const char* needle) {
    if (!haystack || !needle) return 0;
    int64_t h_len = (int64_t)strlen(haystack);
    int64_t n_len = (int64_t)strlen(needle);
    if (n_len == 0) return 0;
    int64_t count = 0;
    for (int64_t i = 0; i <= h_len - n_len; i++) {
        if (memcmp(haystack + i, needle, n_len) == 0) {
            count++;
            i += n_len - 1;
        }
    }
    return count;
}

// ═══════════════════════════════════════════════════════════════════════
// str_byte_at — get byte value at index (-1 on OOB)
// ═══════════════════════════════════════════════════════════════════════

int64_t str_byte_at(const char* s, int64_t idx) {
    if (!s || idx < 0) return -1;
    int64_t len = (int64_t)strlen(s);
    if (idx >= len) return -1;
    return (int64_t)(unsigned char)s[idx];
}

// ═══════════════════════════════════════════════════════════════════════
// str_char_at — single-character string at index (aborts on OOB)
// ═══════════════════════════════════════════════════════════════════════

AriaString* str_char_at(const char* s, int64_t idx) {
    if (!s || idx < 0) std::abort();
    int64_t len = (int64_t)strlen(s);
    if (idx >= len) std::abort();
    return make_string(s + idx, 1);
}

// ═══════════════════════════════════════════════════════════════════════
// str_reverse — reverse bytes (ASCII; not Unicode-aware)
// ═══════════════════════════════════════════════════════════════════════

AriaString* str_reverse(const char* s) {
    if (!s) return aria_string_empty();
    int64_t len = (int64_t)strlen(s);
    if (len <= 1) return make_string(s, len);

    char* buf = (char*)aria_gc_alloc(len + 1, 0);
    if (!buf) std::abort();
    for (int64_t i = 0; i < len; i++) {
        buf[i] = s[len - 1 - i];
    }
    buf[len] = '\0';
    return make_string(buf, len);
}

// ═══════════════════════════════════════════════════════════════════════
// str_to_int — parse string to int64
// ═══════════════════════════════════════════════════════════════════════

int64_t str_to_int(const char* s) {
    if (!s || !*s) return 0;
    char* endptr = nullptr;
    return strtoll(s, &endptr, 10);
}

// ═══════════════════════════════════════════════════════════════════════
// Split — split string by delimiter, store results internally
// ═══════════════════════════════════════════════════════════════════════

struct SplitResult {
    AriaString** parts;
    int64_t count;
};

void* str_split(const char* s, const char* delim) {
    SplitResult* result = (SplitResult*)aria_gc_alloc(sizeof(SplitResult), 0);
    if (!result) std::abort();

    if (!s || !*s) {
        result->parts = nullptr;
        result->count = 0;
        return result;
    }

    int64_t s_len = (int64_t)strlen(s);
    int64_t d_len = delim ? (int64_t)strlen(delim) : 0;

    // Empty delimiter: split into individual chars
    if (d_len == 0) {
        result->count = s_len;
        result->parts = (AriaString**)aria_gc_alloc(sizeof(AriaString*) * s_len, 0);
        if (!result->parts) std::abort();
        for (int64_t i = 0; i < s_len; i++) {
            result->parts[i] = make_string(s + i, 1);
        }
        return result;
    }

    // Count parts
    int64_t count = 1;
    for (int64_t i = 0; i <= s_len - d_len; i++) {
        if (memcmp(s + i, delim, d_len) == 0) {
            count++;
            i += d_len - 1;
        }
    }

    result->count = count;
    result->parts = (AriaString**)aria_gc_alloc(sizeof(AriaString*) * count, 0);
    if (!result->parts) std::abort();

    int64_t part_idx = 0;
    int64_t start = 0;
    for (int64_t i = 0; i <= s_len - d_len; i++) {
        if (memcmp(s + i, delim, d_len) == 0) {
            result->parts[part_idx++] = make_string(s + start, i - start);
            start = i + d_len;
            i += d_len - 1;
        }
    }
    result->parts[part_idx] = make_string(s + start, s_len - start);

    return result;
}

int64_t str_split_count(void* split_handle) {
    SplitResult* sr = (SplitResult*)split_handle;
    if (!sr) return 0;
    return sr->count;
}

void str_split_free(void* split_handle) {
    // GC handles memory; this is a no-op to satisfy wild ownership
    (void)split_handle;
}

// Returns AriaString* — Aria receives this as a string (pointer to struct)
AriaString* str_split_get(void* split_handle, int64_t index) {
    SplitResult* sr = (SplitResult*)split_handle;
    if (!sr || index < 0 || index >= sr->count) std::abort();
    return sr->parts[index];
}

// ═══════════════════════════════════════════════════════════════════════
// Join — join split results with delimiter
// ═══════════════════════════════════════════════════════════════════════

AriaString* str_join(void* split_handle, const char* delim) {
    SplitResult* sr = (SplitResult*)split_handle;
    if (!sr || sr->count == 0) return aria_string_empty();

    int64_t d_len = delim ? (int64_t)strlen(delim) : 0;

    // Calculate total length
    int64_t total = 0;
    for (int64_t i = 0; i < sr->count; i++) {
        total += sr->parts[i]->length;
    }
    if (d_len > 0) {
        total += d_len * (sr->count - 1);
    }

    char* buf = (char*)aria_gc_alloc(total + 1, 0);
    if (!buf) std::abort();

    int64_t pos = 0;
    for (int64_t i = 0; i < sr->count; i++) {
        if (i > 0 && d_len > 0) {
            memcpy(buf + pos, delim, d_len);
            pos += d_len;
        }
        if (sr->parts[i]->length > 0) {
            memcpy(buf + pos, sr->parts[i]->data, sr->parts[i]->length);
            pos += sr->parts[i]->length;
        }
    }
    buf[total] = '\0';

    return make_string(buf, total);
}

}  // extern "C"

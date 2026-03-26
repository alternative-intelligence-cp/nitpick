/**
 * Aria WASM Runtime Library
 *
 * Stripped-down runtime for WebAssembly (wasm32-wasi) target.
 * Provides portable implementations of core Aria runtime functions
 * using only WASI-compatible system calls.
 *
 * ABI Notes (CRITICAL):
 *   - _simple functions: take AriaString* params, return AriaString*
 *   - Query functions (contains, starts_with, etc.): take AriaString BY VALUE
 *   - Print functions: take const char* (data pointer extracted by codegen)
 *   - On wasm32: ptr = i32, AriaString = {i32, i64} = 12 bytes
 *   - Struct returns MUST be via pointer (sret mismatch otherwise)
 *
 * NOT supported in WASM:
 *   - Threading, Async I/O, Process management, mmap, AVX/SIMD
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* ========================================================================= */
/* AriaString — Core string type                                             */
/* ========================================================================= */

typedef struct {
    const char* data;
    int64_t     length;
} AriaString;

/* Helper: heap-allocate an AriaString */
static AriaString* aria_new_string(const char* data, int64_t length) {
    AriaString* s = (AriaString*)malloc(sizeof(AriaString));
    if (!s) { fprintf(stderr, "aria: out of memory\n"); abort(); }
    s->data = data;
    s->length = length;
    return s;
}

/* Helper: heap-allocate an AriaString with copied data */
static AriaString* aria_new_string_copy(const char* data, int64_t length) {
    char* buf = (char*)malloc((size_t)(length + 1));
    if (!buf) { fprintf(stderr, "aria: out of memory\n"); abort(); }
    if (data && length > 0) memcpy(buf, data, (size_t)length);
    buf[length] = '\0';
    return aria_new_string(buf, length);
}

static AriaString* aria_empty_string(void) {
    return aria_new_string("", 0);
}

/* ========================================================================= */
/* GC / Allocator — Minimal malloc-based implementation                      */
/* ========================================================================= */

void aria_gc_init(int64_t initial_heap, int64_t max_heap) {
    (void)initial_heap; (void)max_heap;
}

void* aria_gc_alloc(int64_t size) {
    return malloc((size_t)size);
}

void aria_gc_free(void* ptr) { free(ptr); }
void aria_gc_pin(void* ptr) { (void)ptr; }

void* aria_alloc(int64_t size) { return malloc((size_t)size); }
void* aria_realloc(void* ptr, int64_t size) { return realloc(ptr, (size_t)size); }
void aria_free(void* ptr) { free(ptr); }

void aria_panic_oom(void) {
    fprintf(stderr, "aria: out of memory\n");
    abort();
}

/* ========================================================================= */
/* Args — Command-line argument handling                                     */
/* ========================================================================= */

static int    g_argc = 0;
static char** g_argv = NULL;

void aria_args_init(int argc, char** argv) {
    g_argc = argc;
    g_argv = argv;
}

/* ========================================================================= */
/* Streams — I/O stream initialization                                       */
/* ========================================================================= */

void aria_streams_init(void) { /* WASI provides stdio */ }

/* ========================================================================= */
/* Print / I/O — Console output                                              */
/* ========================================================================= */

int64_t aria_print_cstr(const char* s) {
    if (!s) return 0;
    int64_t len = (int64_t)strlen(s);
    fwrite(s, 1, (size_t)len, stdout);
    return len;
}

int64_t aria_println_cstr(const char* s) {
    int64_t n = aria_print_cstr(s);
    fputc('\n', stdout);
    return n + 1;
}

int64_t aria_stdout_write(const char* data) {
    if (!data) return 0;
    size_t len = strlen(data);
    fwrite(data, 1, len, stdout);
    return (int64_t)len;
}

int64_t aria_stderr_write(const char* data) {
    if (!data) return 0;
    size_t len = strlen(data);
    fwrite(data, 1, len, stderr);
    return (int64_t)len;
}

int64_t aria_stddbg_write(const char* data) {
    return aria_stderr_write(data);
}

void aria_flush_stdout(void) { fflush(stdout); }

/* ========================================================================= */
/* String _simple functions — AriaString* params, AriaString* returns        */
/* These match the codegen ABI: (ptr, ptr) -> ptr                            */
/* ========================================================================= */

AriaString* aria_string_concat_simple(AriaString* a, AriaString* b) {
    int64_t la = (a && a->data) ? a->length : 0;
    int64_t lb = (b && b->data) ? b->length : 0;
    int64_t total = la + lb;
    char* buf = (char*)malloc((size_t)(total + 1));
    if (!buf) aria_panic_oom();
    if (la > 0) memcpy(buf, a->data, (size_t)la);
    if (lb > 0) memcpy(buf + la, b->data, (size_t)lb);
    buf[total] = '\0';
    return aria_new_string(buf, total);
}

AriaString* aria_string_concat_n_simple(AriaString* strings, int64_t count) {
    if (!strings || count <= 0) return aria_empty_string();
    int64_t total = 0;
    for (int64_t i = 0; i < count; i++) {
        total += strings[i].length;
    }
    char* buf = (char*)malloc((size_t)(total + 1));
    if (!buf) aria_panic_oom();
    int64_t pos = 0;
    for (int64_t i = 0; i < count; i++) {
        if (strings[i].data && strings[i].length > 0) {
            memcpy(buf + pos, strings[i].data, (size_t)strings[i].length);
            pos += strings[i].length;
        }
    }
    buf[total] = '\0';
    return aria_new_string(buf, total);
}

AriaString* aria_string_repeat_simple(AriaString* str, int64_t count) {
    if (!str || !str->data || count <= 0) return aria_empty_string();
    int64_t slen = str->length;
    int64_t total = slen * count;
    char* buf = (char*)malloc((size_t)(total + 1));
    if (!buf) aria_panic_oom();
    for (int64_t i = 0; i < count; i++) {
        memcpy(buf + i * slen, str->data, (size_t)slen);
    }
    buf[total] = '\0';
    return aria_new_string(buf, total);
}

AriaString* aria_string_trim_simple(AriaString* str) {
    if (!str || !str->data || str->length == 0) return aria_empty_string();
    int64_t start = 0, end = str->length;
    while (start < end && (str->data[start] == ' ' || str->data[start] == '\t' ||
                           str->data[start] == '\n' || str->data[start] == '\r'))
        start++;
    while (end > start && (str->data[end-1] == ' ' || str->data[end-1] == '\t' ||
                           str->data[end-1] == '\n' || str->data[end-1] == '\r'))
        end--;
    return aria_new_string_copy(str->data + start, end - start);
}

AriaString* aria_string_trim_start_simple(AriaString* str) {
    if (!str || !str->data || str->length == 0) return aria_empty_string();
    int64_t start = 0;
    while (start < str->length && (str->data[start] == ' ' || str->data[start] == '\t' ||
                                    str->data[start] == '\n' || str->data[start] == '\r'))
        start++;
    return aria_new_string_copy(str->data + start, str->length - start);
}

AriaString* aria_string_trim_end_simple(AriaString* str) {
    if (!str || !str->data || str->length == 0) return aria_empty_string();
    int64_t end = str->length;
    while (end > 0 && (str->data[end-1] == ' ' || str->data[end-1] == '\t' ||
                       str->data[end-1] == '\n' || str->data[end-1] == '\r'))
        end--;
    return aria_new_string_copy(str->data, end);
}

AriaString* aria_string_to_upper_simple(AriaString* str) {
    if (!str || !str->data || str->length == 0) return aria_empty_string();
    char* buf = (char*)malloc((size_t)(str->length + 1));
    if (!buf) aria_panic_oom();
    for (int64_t i = 0; i < str->length; i++) {
        char c = str->data[i];
        buf[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }
    buf[str->length] = '\0';
    return aria_new_string(buf, str->length);
}

AriaString* aria_string_to_lower_simple(AriaString* str) {
    if (!str || !str->data || str->length == 0) return aria_empty_string();
    char* buf = (char*)malloc((size_t)(str->length + 1));
    if (!buf) aria_panic_oom();
    for (int64_t i = 0; i < str->length; i++) {
        char c = str->data[i];
        buf[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    buf[str->length] = '\0';
    return aria_new_string(buf, str->length);
}

AriaString* aria_string_substring_simple(AriaString* str, int64_t start, int64_t end) {
    if (!str || !str->data) return aria_empty_string();
    if (start < 0) start = 0;
    if (end > str->length) end = str->length;
    if (start >= end) return aria_empty_string();
    return aria_new_string_copy(str->data + start, end - start);
}

AriaString* aria_string_from_int_simple(int64_t val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lld", (long long)val);
    return aria_new_string_copy(buf, (int64_t)len);
}

AriaString* aria_string_from_int_hex_simple(int64_t val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%llx", (unsigned long long)val);
    return aria_new_string_copy(buf, (int64_t)len);
}

AriaString* aria_string_from_char_simple(int32_t c) {
    char buf[2] = { (char)c, '\0' };
    return aria_new_string_copy(buf, 1);
}

AriaString* aria_string_from_cstr_simple(const char* s) {
    if (!s) return aria_empty_string();
    int64_t len = (int64_t)strlen(s);
    return aria_new_string_copy(s, len);
}

AriaString* aria_string_format_float_simple(double val, int32_t precision) {
    char buf[64];
    int len;
    if (precision >= 0) {
        len = snprintf(buf, sizeof(buf), "%.*f", precision, val);
    } else {
        len = snprintf(buf, sizeof(buf), "%g", val);
    }
    return aria_new_string_copy(buf, (int64_t)len);
}

int64_t aria_string_to_int_simple(AriaString* str) {
    if (!str || !str->data) return 0;
    return (int64_t)atoll(str->data);
}

AriaString* aria_string_to_hex_simple(AriaString* str) {
    if (!str || !str->data || str->length == 0) return aria_empty_string();
    int64_t hex_len = str->length * 2;
    char* buf = (char*)malloc((size_t)(hex_len + 1));
    if (!buf) aria_panic_oom();
    for (int64_t i = 0; i < str->length; i++) {
        snprintf(buf + i * 2, 3, "%02x", (unsigned char)str->data[i]);
    }
    buf[hex_len] = '\0';
    return aria_new_string(buf, hex_len);
}

int64_t aria_string_index_of_simple(AriaString* haystack, AriaString* needle) {
    if (!haystack || !haystack->data || !needle || !needle->data) return -1;
    const char* p = strstr(haystack->data, needle->data);
    if (!p) return -1;
    return (int64_t)(p - haystack->data);
}

AriaString* aria_string_pad_left_simple(AriaString* str, int64_t width, uint8_t pad_char) {
    if (!str || !str->data) return aria_empty_string();
    if (str->length >= width) return aria_new_string_copy(str->data, str->length);
    int64_t pad_len = width - str->length;
    char* buf = (char*)malloc((size_t)(width + 1));
    if (!buf) aria_panic_oom();
    memset(buf, pad_char ? pad_char : ' ', (size_t)pad_len);
    memcpy(buf + pad_len, str->data, (size_t)str->length);
    buf[width] = '\0';
    return aria_new_string(buf, width);
}

AriaString* aria_string_pad_right_simple(AriaString* str, int64_t width, uint8_t pad_char) {
    if (!str || !str->data) return aria_empty_string();
    if (str->length >= width) return aria_new_string_copy(str->data, str->length);
    char* buf = (char*)malloc((size_t)(width + 1));
    if (!buf) aria_panic_oom();
    memcpy(buf, str->data, (size_t)str->length);
    memset(buf + str->length, pad_char ? pad_char : ' ', (size_t)(width - str->length));
    buf[width] = '\0';
    return aria_new_string(buf, width);
}

AriaString* aria_int64_to_str(int64_t val) {
    return aria_string_from_int_simple(val);
}

/* ========================================================================= */
/* String query functions — AriaString BY VALUE params (matches codegen)     */
/* Codegen loads AriaString struct and passes by value: (AriaString, AriaString) */
/* ========================================================================= */

int32_t aria_string_equals(AriaString a, AriaString b) {
    if (a.length != b.length) return 0;
    if (a.length == 0) return 1;
    return memcmp(a.data, b.data, (size_t)a.length) == 0 ? 1 : 0;
}

int32_t aria_string_compare_str(AriaString a, AriaString b) {
    int64_t min_len = a.length < b.length ? a.length : b.length;
    int cmp = memcmp(a.data, b.data, (size_t)min_len);
    if (cmp != 0) return cmp < 0 ? -1 : 1;
    if (a.length < b.length) return -1;
    if (a.length > b.length) return 1;
    return 0;
}

int32_t aria_string_contains(AriaString haystack, AriaString needle) {
    if (!haystack.data || !needle.data) return 0;
    if (needle.length == 0) return 1;
    if (needle.length > haystack.length) return 0;
    /* Simple search since we have length info */
    for (int64_t i = 0; i <= haystack.length - needle.length; i++) {
        if (memcmp(haystack.data + i, needle.data, (size_t)needle.length) == 0)
            return 1;
    }
    return 0;
}

int32_t aria_string_starts_with(AriaString s, AriaString prefix) {
    if (!s.data || !prefix.data) return 0;
    if (prefix.length > s.length) return 0;
    return memcmp(s.data, prefix.data, (size_t)prefix.length) == 0 ? 1 : 0;
}

int32_t aria_string_ends_with(AriaString s, AriaString suffix) {
    if (!s.data || !suffix.data) return 0;
    if (suffix.length > s.length) return 0;
    return memcmp(s.data + s.length - suffix.length, suffix.data,
                  (size_t)suffix.length) == 0 ? 1 : 0;
}

/* ========================================================================= */
/* File I/O — WASI compatible                                                */
/* ========================================================================= */

AriaString* aria_read_file_simple(const char* path) {
    if (!path) return aria_empty_string();
    FILE* f = fopen(path, "rb");
    if (!f) return aria_empty_string();
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return aria_empty_string(); }
    char* buf = (char*)malloc((size_t)(size + 1));
    if (!buf) { fclose(f); return aria_empty_string(); }
    size_t rd = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[rd] = '\0';
    return aria_new_string(buf, (int64_t)rd);
}

int32_t aria_write_file_simple(const char* path, const char* data) {
    if (!path || !data) return -1;
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    size_t len = strlen(data);
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    return written == len ? 0 : -1;
}

int32_t aria_delete_file_simple(const char* path) {
    if (!path) return -1;
    return remove(path) == 0 ? 0 : -1;
}

int32_t aria_file_exists(const char* path) {
    if (!path) return 0;
    FILE* f = fopen(path, "r");
    if (f) { fclose(f); return 1; }
    return 0;
}

int64_t aria_file_size(const char* path) {
    if (!path) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return (int64_t)size;
}

AriaString* aria_stdin_read_line(void) {
    char buf[4096];
    if (fgets(buf, sizeof(buf), stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
        return aria_new_string_copy(buf, (int64_t)len);
    }
    return aria_empty_string();
}

AriaString* aria_stdin_read_all(void) {
    size_t cap = 4096, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) aria_panic_oom();
    int c;
    while ((c = fgetc(stdin)) != EOF) {
        if (len + 1 >= cap) {
            cap *= 2;
            buf = (char*)realloc(buf, cap);
            if (!buf) aria_panic_oom();
        }
        buf[len++] = (char)c;
    }
    buf[len] = '\0';
    return aria_new_string(buf, (int64_t)len);
}

/* ========================================================================= */
/* Environment                                                                */
/* ========================================================================= */

AriaString* aria_env_get_builtin(const char* name) {
    const char* val = getenv(name);
    if (!val) return aria_empty_string();
    return aria_new_string_copy(val, (int64_t)strlen(val));
}

/* ========================================================================= */
/* Sleep / Exit                                                               */
/* ========================================================================= */

void aria_sleep_ms(int64_t ms) { (void)ms; /* No sleep in WASI preview 1 */ }
void aria_exit_builtin(int32_t code) { exit(code); }

/* ========================================================================= */
/* Sort                                                                       */
/* ========================================================================= */

static int cmp_str(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

AriaString* aria_sort_lines(const char* input) {
    if (!input || !*input) return aria_empty_string();
    size_t count = 1;
    for (const char* p = input; *p; p++)
        if (*p == '\n') count++;
    char* copy = (char*)malloc(strlen(input) + 1);
    if (!copy) aria_panic_oom();
    strcpy(copy, input);
    char** lines = (char**)malloc(count * sizeof(char*));
    if (!lines) aria_panic_oom();
    size_t n = 0;
    char* tok = copy;
    for (char* p = copy; ; p++) {
        if (*p == '\n' || *p == '\0') {
            int is_end = (*p == '\0');
            *p = '\0';
            if (n < count) lines[n++] = tok;
            if (is_end) break;
            tok = p + 1;
        }
    }
    qsort(lines, n, sizeof(char*), cmp_str);
    size_t total = 0;
    for (size_t i = 0; i < n; i++) total += strlen(lines[i]) + 1;
    char* result = (char*)malloc(total);
    if (!result) aria_panic_oom();
    size_t pos = 0;
    for (size_t i = 0; i < n; i++) {
        size_t l = strlen(lines[i]);
        memcpy(result + pos, lines[i], l);
        pos += l;
        if (i + 1 < n) result[pos++] = '\n';
    }
    result[pos] = '\0';
    free(lines);
    free(copy);
    return aria_new_string(result, (int64_t)pos);
}

/* ========================================================================= */
/* Format helpers                                                              */
/* ========================================================================= */

int aria_snprintf_c_locale(char* buf, int64_t size, const char* fmt, double val) {
    return snprintf(buf, (size_t)size, fmt, val);
}

/* ========================================================================= */
/* Array operations                                                           */
/* ========================================================================= */

typedef struct {
    void*   data;
    int64_t length;
    int64_t capacity;
    int64_t elem_size;
} AriaArray;

void* aria_array_new_simple(int64_t elem_size) {
    AriaArray* arr = (AriaArray*)malloc(sizeof(AriaArray));
    if (!arr) aria_panic_oom();
    arr->elem_size = elem_size;
    arr->length = 0;
    arr->capacity = 8;
    arr->data = malloc((size_t)(elem_size * arr->capacity));
    if (!arr->data) aria_panic_oom();
    return arr;
}

void aria_array_push_simple(void* handle, const void* elem) {
    AriaArray* arr = (AriaArray*)handle;
    if (arr->length >= arr->capacity) {
        arr->capacity *= 2;
        arr->data = realloc(arr->data, (size_t)(arr->elem_size * arr->capacity));
        if (!arr->data) aria_panic_oom();
    }
    memcpy((char*)arr->data + arr->length * arr->elem_size, elem, (size_t)arr->elem_size);
    arr->length++;
}

void* aria_array_get_simple(void* handle, int64_t index) {
    AriaArray* arr = (AriaArray*)handle;
    if (index < 0 || index >= arr->length) return NULL;
    return (char*)arr->data + index * arr->elem_size;
}

void aria_array_set_simple(void* handle, int64_t index, const void* elem) {
    AriaArray* arr = (AriaArray*)handle;
    if (index < 0 || index >= arr->length) return;
    memcpy((char*)arr->data + index * arr->elem_size, elem, (size_t)arr->elem_size);
}

void* aria_array_pop_simple(void* handle) {
    AriaArray* arr = (AriaArray*)handle;
    if (arr->length <= 0) return NULL;
    arr->length--;
    return (char*)arr->data + arr->length * arr->elem_size;
}

int64_t aria_array_length(void* handle) {
    AriaArray* arr = (AriaArray*)handle;
    return arr->length;
}

/* ========================================================================= */
/* Map operations — FNV-1a hash map                                          */
/* ========================================================================= */

typedef struct MapEntry {
    char*  key;
    void*  value;
    int64_t value_size;
    struct MapEntry* next;
} MapEntry;

typedef struct {
    MapEntry** buckets;
    int64_t    count;
    int64_t    capacity;
    int64_t    value_size;
} AriaMap;

static uint64_t fnv1a_hash(const char* key) {
    uint64_t h = 14695981039346656037ULL;
    for (; *key; key++) {
        h ^= (uint64_t)(unsigned char)*key;
        h *= 1099511628211ULL;
    }
    return h;
}

void* aria_map_new_simple(int64_t value_size) {
    AriaMap* map = (AriaMap*)malloc(sizeof(AriaMap));
    if (!map) aria_panic_oom();
    map->capacity = 64;
    map->count = 0;
    map->value_size = value_size;
    map->buckets = (MapEntry**)calloc((size_t)map->capacity, sizeof(MapEntry*));
    if (!map->buckets) aria_panic_oom();
    return map;
}

void aria_map_insert_simple(void* handle, const char* key, const void* value) {
    AriaMap* map = (AriaMap*)handle;
    uint64_t idx = fnv1a_hash(key) % (uint64_t)map->capacity;
    for (MapEntry* e = map->buckets[idx]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            memcpy(e->value, value, (size_t)map->value_size);
            return;
        }
    }
    MapEntry* entry = (MapEntry*)malloc(sizeof(MapEntry));
    if (!entry) aria_panic_oom();
    entry->key = (char*)malloc(strlen(key) + 1);
    if (!entry->key) aria_panic_oom();
    strcpy(entry->key, key);
    entry->value = malloc((size_t)map->value_size);
    if (!entry->value) aria_panic_oom();
    memcpy(entry->value, value, (size_t)map->value_size);
    entry->value_size = map->value_size;
    entry->next = map->buckets[idx];
    map->buckets[idx] = entry;
    map->count++;
}

void* aria_map_get_simple(void* handle, const char* key) {
    AriaMap* map = (AriaMap*)handle;
    uint64_t idx = fnv1a_hash(key) % (uint64_t)map->capacity;
    for (MapEntry* e = map->buckets[idx]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) return e->value;
    }
    return NULL;
}

int32_t aria_map_has(void* handle, const char* key) {
    return aria_map_get_simple(handle, key) != NULL ? 1 : 0;
}

void aria_map_remove(void* handle, const char* key) {
    AriaMap* map = (AriaMap*)handle;
    uint64_t idx = fnv1a_hash(key) % (uint64_t)map->capacity;
    MapEntry** pp = &map->buckets[idx];
    while (*pp) {
        if (strcmp((*pp)->key, key) == 0) {
            MapEntry* dead = *pp;
            *pp = dead->next;
            free(dead->key);
            free(dead->value);
            free(dead);
            map->count--;
            return;
        }
        pp = &(*pp)->next;
    }
}

int64_t aria_map_length(void* handle) {
    AriaMap* map = (AriaMap*)handle;
    return map->count;
}

/* ========================================================================= */
/* Wild allocator — malloc fallback                                           */
/* ========================================================================= */

void* aria_wild_alloc(int64_t size) { return malloc((size_t)size); }
void aria_wild_free(void* ptr, int64_t size) { (void)size; free(ptr); }

/* ========================================================================= */
/* Arena allocator                                                            */
/* ========================================================================= */

typedef struct {
    char*   base;
    int64_t size;
    int64_t offset;
} WasmArena;

void* aria_arena_new_handle(int64_t size) {
    WasmArena* a = (WasmArena*)malloc(sizeof(WasmArena));
    if (!a) aria_panic_oom();
    a->base = (char*)malloc((size_t)size);
    if (!a->base) aria_panic_oom();
    a->size = size;
    a->offset = 0;
    return a;
}

void* aria_arena_alloc_handle(void* handle, int64_t size) {
    WasmArena* a = (WasmArena*)handle;
    int64_t aligned = (a->offset + 7) & ~7;
    if (aligned + size > a->size) return NULL;
    void* ptr = a->base + aligned;
    a->offset = aligned + size;
    return ptr;
}

void aria_arena_reset_handle(void* handle) { ((WasmArena*)handle)->offset = 0; }
void aria_arena_destroy_handle(void* handle) { WasmArena* a = (WasmArena*)handle; free(a->base); free(a); }
int64_t aria_arena_get_allocated_handle(void* handle) { return ((WasmArena*)handle)->offset; }
int64_t aria_arena_get_reserved_handle(void* handle) { return ((WasmArena*)handle)->size; }

/* ========================================================================= */
/* Pool / Slab allocator stubs                                                */
/* ========================================================================= */

void* aria_pool_new_handle(int64_t block_size, int64_t block_count) { (void)block_size; (void)block_count; return malloc(sizeof(int64_t)); }
void* aria_pool_alloc_handle(void* handle) { (void)handle; return malloc(256); }
void aria_pool_free_handle(void* handle, void* ptr) { (void)handle; free(ptr); }
void aria_pool_reset_handle(void* handle) { (void)handle; }
void aria_pool_destroy_handle(void* handle) { free(handle); }
int64_t aria_pool_get_total_blocks_handle(void* handle) { (void)handle; return 0; }
int64_t aria_pool_get_used_blocks_handle(void* handle) { (void)handle; return 0; }

void* aria_slab_cache_new_handle(int64_t obj_size, int64_t count) { (void)obj_size; (void)count; return malloc(sizeof(int64_t)); }
void* aria_slab_cache_alloc_handle(void* handle) { (void)handle; return malloc(256); }
void aria_slab_cache_free_handle(void* handle, void* ptr) { (void)handle; free(ptr); }
void aria_slab_cache_destroy_handle(void* handle) { free(handle); }
int64_t aria_slab_cache_get_total_objects_handle(void* handle) { (void)handle; return 0; }
int64_t aria_slab_cache_get_allocated_objects_handle(void* handle) { (void)handle; return 0; }

/* ========================================================================= */
/* TBB                                                                        */
/* ========================================================================= */

int8_t aria_tbb8_from_int(int64_t val) {
    if (val < -128) return -128;
    if (val > 127) return 127;
    return (int8_t)val;
}

int64_t aria_tbb8_to_int(int8_t val) { return (int64_t)val; }

/* ========================================================================= */
/* Error handling                                                              */
/* ========================================================================= */

AriaString* aria_error_msg(void* err) {
    (void)err;
    return aria_new_string_copy("unknown error", 13);
}

void* aria_error_new(const char* msg) {
    if (!msg) msg = "error";
    size_t len = strlen(msg);
    char* buf = (char*)malloc(len + 1);
    if (!buf) aria_panic_oom();
    memcpy(buf, msg, len + 1);
    return buf;
}

/* ========================================================================= */
/* Path operations                                                             */
/* ========================================================================= */

AriaString* aria_path_basename_string(const char* path) {
    if (!path) return aria_empty_string();
    const char* last_slash = strrchr(path, '/');
    const char* base = last_slash ? last_slash + 1 : path;
    return aria_new_string_copy(base, (int64_t)strlen(base));
}

AriaString* aria_path_dirname_string(const char* path) {
    if (!path) return aria_new_string_copy(".", 1);
    const char* last_slash = strrchr(path, '/');
    if (!last_slash) return aria_new_string_copy(".", 1);
    int64_t len = (int64_t)(last_slash - path);
    if (len == 0) len = 1;
    return aria_new_string_copy(path, len);
}

int32_t aria_path_is_absolute(const char* path) {
    return (path && path[0] == '/') ? 1 : 0;
}

/* ========================================================================= */
/* Math                                                                        */
/* ========================================================================= */

double aria_math_sqrt(double x) { return sqrt(x); }
double aria_math_pow(double x, double y) { return pow(x, y); }
double aria_math_abs_f(double x) { return fabs(x); }
int64_t aria_math_abs_i(int64_t x) { return x < 0 ? -x : x; }
double aria_math_pi(void) { return 3.14159265358979323846; }
double aria_math_e(void) { return 2.71828182845904523536; }
int64_t aria_math_min_i(int64_t a, int64_t b) { return a < b ? a : b; }
int64_t aria_math_max_i(int64_t a, int64_t b) { return a > b ? a : b; }

/* ========================================================================= */
/* Compiler-RT builtins — needed for wasm32 (no libclang_rt.builtins-wasm32) */
/* ========================================================================= */

/* 128-bit multiply: required for int64 * int64 on wasm32 */
/* LLVM __multi3 ABI: __int128 __multi3(__int128 a, __int128 b) */
/* On wasm32: (sret_ptr, a_lo, a_hi, b_lo, b_hi) -> void */
typedef __int128 ti_int;

typedef union {
    ti_int all;
    struct { uint64_t lo; int64_t hi; } parts;
} ti_union;

__attribute__((visibility("default")))
ti_int __multi3(ti_int a, ti_int b) {
    ti_union ua = { .all = a };
    ti_union ub = { .all = b };

    uint64_t a_lo = ua.parts.lo;
    uint64_t b_lo = ub.parts.lo;

    /* 64x64->128 unsigned multiply using 32-bit parts (no recursion) */
    uint32_t a0 = (uint32_t)a_lo;
    uint32_t a1 = (uint32_t)(a_lo >> 32);
    uint32_t b0 = (uint32_t)b_lo;
    uint32_t b1 = (uint32_t)(b_lo >> 32);

    uint64_t p0 = (uint64_t)a0 * b0;
    uint64_t p1 = (uint64_t)a0 * b1;
    uint64_t p2 = (uint64_t)a1 * b0;
    uint64_t p3 = (uint64_t)a1 * b1;

    uint64_t mid = p1 + (p0 >> 32);
    uint64_t carry = (uint64_t)((uint32_t)mid) + p2;

    ti_union result;
    result.parts.lo = ((carry & 0xFFFFFFFF) << 32) | (p0 & 0xFFFFFFFF);
    result.parts.hi = (int64_t)(p3 + (mid >> 32) + (carry >> 32)
                       + ua.parts.hi * (int64_t)b_lo
                       + (int64_t)a_lo * ub.parts.hi);
    return result.all;
}

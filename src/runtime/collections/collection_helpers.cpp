/**
 * Collection helpers for Aria self-hosting (v0.2.0)
 *
 * Thin C helper layer for collections module. Provides:
 * - Typed Vec wrappers (int64, int32, flt64) — bridge &val/memcpy gap
 * - Typed Map wrappers (int64→int64) — bridge &key/&val/memcpy gap
 * - Pointer↔int64 conversion helpers — for Graph self-hosting
 *
 * Set and Graph logic moved to pure Aria in stdlib/collections.aria.
 */

#include <cstdint>
#include <cstring>

// Forward declarations — C-linkage symbols from the runtime
extern "C" {
extern void*   aria_array_new_simple(size_t element_size, int type_id);
extern void    aria_array_push_simple(void* arr, const void* val);
extern void    aria_array_pop_simple(void* arr, void* out_val);
extern void*   aria_array_get_simple(void* arr, size_t index);
extern void    aria_array_set_simple(void* arr, size_t index, const void* val);
extern size_t  aria_array_length(const void* arr);
extern void    aria_array_free(void* arr);

extern void*   aria_map_new_simple(size_t key_size, size_t value_size);
extern void    aria_map_insert_simple(void* map, const void* key, const void* value);
extern void*   aria_map_get_simple(void* map, const void* key);
extern int32_t aria_map_has(void* map, const void* key);
extern void    aria_map_remove(void* map, const void* key);
extern int64_t aria_map_length(const void* map);
extern void    aria_map_clear(void* map);
extern void    aria_map_free(void* map);
}

extern "C" {

// ═══════════════════════════════════════════════════════════════════════
// Pointer↔int64 conversion (for Graph self-hosting)
// ═══════════════════════════════════════════════════════════════════════

int64_t ch_ptr_to_int(void* ptr) {
    return reinterpret_cast<int64_t>(ptr);
}

void* ch_int_to_ptr(int64_t val) {
    return reinterpret_cast<void*>(val);
}

// ═══════════════════════════════════════════════════════════════════════
// Vec<int64> typed wrappers
// ═══════════════════════════════════════════════════════════════════════

void* vec_i64_new(void) {
    return aria_array_new_simple(8, 1);
}

void vec_i64_push(void* arr, int64_t val) {
    aria_array_push_simple(arr, &val);
}

int64_t vec_i64_pop(void* arr) {
    int64_t v = 0;
    aria_array_pop_simple(arr, &v);
    return v;
}

int64_t vec_i64_get(void* arr, int64_t index) {
    void* ptr = aria_array_get_simple(arr, index);
    int64_t v;
    memcpy(&v, ptr, sizeof(int64_t));
    return v;
}

void vec_i64_set(void* arr, int64_t index, int64_t val) {
    aria_array_set_simple(arr, index, &val);
}

int64_t vec_i64_length(void* arr) {
    return aria_array_length(arr);
}

void vec_i64_free(void* arr) {
    aria_array_free(arr);
}

// ═══════════════════════════════════════════════════════════════════════
// Vec<int32> typed wrappers
// ═══════════════════════════════════════════════════════════════════════

void* vec_i32_new(void) {
    return aria_array_new_simple(4, 1);
}

void vec_i32_push(void* arr, int32_t val) {
    aria_array_push_simple(arr, &val);
}

int32_t vec_i32_pop(void* arr) {
    int32_t v = 0;
    aria_array_pop_simple(arr, &v);
    return v;
}

int32_t vec_i32_get(void* arr, int64_t index) {
    void* ptr = aria_array_get_simple(arr, index);
    int32_t v;
    memcpy(&v, ptr, sizeof(int32_t));
    return v;
}

void vec_i32_set(void* arr, int64_t index, int32_t val) {
    aria_array_set_simple(arr, index, &val);
}

int64_t vec_i32_length(void* arr) {
    return aria_array_length(arr);
}

void vec_i32_free(void* arr) {
    aria_array_free(arr);
}

// ═══════════════════════════════════════════════════════════════════════
// Vec<flt64> typed wrappers
// ═══════════════════════════════════════════════════════════════════════

void* vec_f64_new(void) {
    return aria_array_new_simple(8, 1);
}

void vec_f64_push(void* arr, double val) {
    aria_array_push_simple(arr, &val);
}

double vec_f64_pop(void* arr) {
    double v = 0.0;
    aria_array_pop_simple(arr, &v);
    return v;
}

double vec_f64_get(void* arr, int64_t index) {
    void* ptr = aria_array_get_simple(arr, index);
    double v;
    memcpy(&v, ptr, sizeof(double));
    return v;
}

void vec_f64_set(void* arr, int64_t index, double val) {
    aria_array_set_simple(arr, index, &val);
}

int64_t vec_f64_length(void* arr) {
    return aria_array_length(arr);
}

void vec_f64_free(void* arr) {
    aria_array_free(arr);
}

// ═══════════════════════════════════════════════════════════════════════
// Map<int64, int64> typed wrappers
// ═══════════════════════════════════════════════════════════════════════

void* map_i64_new(void) {
    return aria_map_new_simple(8, 8);
}

void map_i64_insert(void* map, int64_t key, int64_t val) {
    aria_map_insert_simple(map, &key, &val);
}

int64_t map_i64_get(void* map, int64_t key) {
    void* ptr = aria_map_get_simple(map, &key);
    int64_t v;
    memcpy(&v, ptr, sizeof(int64_t));
    return v;
}

int32_t map_i64_has(void* map, int64_t key) {
    return aria_map_has(map, &key);
}

void map_i64_remove(void* map, int64_t key) {
    aria_map_remove(map, &key);
}

int64_t map_i64_length(void* map) {
    return aria_map_length(map);
}

void map_i64_clear(void* map) {
    aria_map_clear(map);
}

void map_i64_free(void* map) {
    aria_map_free(map);
}

}  // extern "C"

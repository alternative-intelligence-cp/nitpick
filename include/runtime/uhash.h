/**
 * Aria User Hash Table (ahash) — Header
 * v0.4.5: Scope-local, type-safe, capacity-bounded hash table with string keys.
 *
 * Each entry stores: key (string), value (8 bytes as int64), type tag (int64).
 * The runtime validates type tags on get — failsafe on mismatch.
 * Multiple tables per scope via explicit handle.
 *
 * Hash function: FNV-1a (fast, non-cryptographic).
 * Collision resolution: open addressing with linear probing.
 */

#ifndef ARIA_RUNTIME_UHASH_H
#define ARIA_RUNTIME_UHASH_H

#include <stdint.h>
#include "type_tags.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a new user hash table.
 *
 * @param capacity_bytes  Byte budget for payload (0 = unbounded)
 * @return Opaque handle (as int64), or 0 on allocation failure
 */
int64_t aria_uhash_new(int64_t capacity_bytes);

/**
 * Destroy a user hash table, freeing all memory.
 *
 * @param handle  Table handle from aria_uhash_new
 */
void aria_uhash_destroy(int64_t handle);

/**
 * Set a key/value pair (upsert semantics).
 *
 * @param handle     Table handle
 * @param key        Null-terminated string key
 * @param value      Value widened to int64
 * @param type_tag   Type tag (ARIA_TAG_*)
 * @param value_size Size of the logical value in bytes (for capacity accounting)
 * @return 0 on success, -1 if capacity would be exceeded
 */
int32_t aria_uhash_set(int64_t handle, const char* key, int64_t value,
                       int64_t type_tag, int64_t value_size);

/**
 * Get a value by key.
 *
 * @param handle        Table handle
 * @param key           Null-terminated string key
 * @param expected_tag  Expected type tag — failsafe on mismatch, -1 to skip
 * @return The value (as int64), or 0 if key not found
 */
int64_t aria_uhash_get(int64_t handle, const char* key, int64_t expected_tag);

/**
 * Return all keys as an array of AriaString pointers.
 *
 * @param handle  Table handle
 * @param out_count  Output: number of keys returned
 * @return Heap-allocated array of {char*, int64_t} AriaString structs,
 *         or NULL if empty. Caller (compiler runtime) manages lifetime.
 */
void* aria_uhash_keys(int64_t handle, int64_t* out_count);

/**
 * Return current payload bytes consumed.
 *
 * @param handle  Table handle
 * @return Bytes used by stored values
 */
int64_t aria_uhash_size(int64_t handle);

/**
 * Check if a value of given size fits within remaining capacity.
 *
 * @param handle      Table handle
 * @param value_size  Size of the value to check
 * @return 1 if fits (or unbounded), 0 if would exceed capacity
 */
int64_t aria_uhash_fits(int64_t handle, int64_t value_size);

/**
 * Return the type tag of the value at a given key.
 *
 * @param handle  Table handle
 * @param key     Null-terminated string key
 * @return Type tag (ARIA_TAG_*), or -1 if key not found
 */
int32_t aria_uhash_type(int64_t handle, const char* key);

/**
 * Return the number of entries (key-value pairs) in the table.
 *
 * @param handle  Table handle
 * @return Number of entries, or 0 if handle is null
 */
int64_t aria_uhash_count(int64_t handle);

/**
 * Delete a key from the table (tombstone-based deletion).
 *
 * @param handle  Table handle
 * @param key     Null-terminated string key to delete
 * @return 0 on success, -1 if key not found
 */
int32_t aria_uhash_delete(int64_t handle, const char* key);

/**
 * Check if a key exists in the table.
 *
 * @param handle  Table handle
 * @param key     Null-terminated string key
 * @return 1 if key exists, 0 if not (or handle is null)
 */
int32_t aria_uhash_has(int64_t handle, const char* key);

/**
 * Remove all entries from the table without destroying it.
 *
 * @param handle  Table handle
 */
void aria_uhash_clear(int64_t handle);

/**
 * Fast-path variants (SMT-proven type-homogeneous tables).
 * Skip type tag storage and checking.
 */
int64_t aria_uhash_new_fast(int64_t capacity_bytes);
void    aria_uhash_destroy_fast(int64_t handle);
int32_t aria_uhash_set_fast(int64_t handle, const char* key, int64_t value,
                            int64_t value_size);
int64_t aria_uhash_get_fast(int64_t handle, const char* key);
void*   aria_uhash_keys_fast(int64_t handle, int64_t* out_count);
int64_t aria_uhash_size_fast(int64_t handle);
int64_t aria_uhash_fits_fast(int64_t handle, int64_t value_size);
int64_t aria_uhash_count_fast(int64_t handle);
int32_t aria_uhash_delete_fast(int64_t handle, const char* key);
int32_t aria_uhash_has_fast(int64_t handle, const char* key);
void    aria_uhash_clear_fast(int64_t handle);

#ifdef __cplusplus
}
#endif

#endif /* ARIA_RUNTIME_UHASH_H */

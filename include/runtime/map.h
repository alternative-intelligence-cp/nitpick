/**
 * Aria Runtime - Map (Hash Table) Implementation
 * 
 * Simple hash map implementation for Aria standard library.
 * Uses open addressing with linear probing for collision resolution.
 * 
 * Design:
 * - Generic key-value storage
 * - GC-integrated memory management
 * - Type-safe operations with result types
 * - Simple hash function for common types
 */

#ifndef ARIA_RUNTIME_MAP_H
#define ARIA_RUNTIME_MAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "runtime/result.h"

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════
// Map Structure
// ═══════════════════════════════════════════════════════════════════════

/**
 * Hash map entry (internal).
 */
typedef struct {
    void* key;          // Pointer to key data
    void* value;        // Pointer to value data
    bool occupied;      // Whether this slot is occupied
    uint64_t hash;      // Cached hash value
} AriaMapEntry;

/**
 * Hash map structure.
 * 
 * Memory layout:
 * - entries: GC-allocated array of map entries
 * - capacity: Total number of slots (always power of 2)
 * - length: Current number of key-value pairs
 * - key_size: Size of each key in bytes
 * - value_size: Size of each value in bytes
 * - load_factor: Resize when length/capacity exceeds this (default 0.75)
 */
typedef struct {
    AriaMapEntry* entries;  // Array of entries
    size_t capacity;        // Total slots (power of 2)
    size_t length;          // Number of occupied entries
    size_t key_size;        // Size of keys in bytes
    size_t value_size;      // Size of values in bytes
    float load_factor;      // Max load before resize
} AriaMap;

// ═══════════════════════════════════════════════════════════════════════
// Map Creation and Destruction
// ═══════════════════════════════════════════════════════════════════════

/**
 * Create a new map with specified key and value sizes.
 * 
 * @param key_size Size of each key in bytes
 * @param value_size Size of each value in bytes
 * @param initial_capacity Initial capacity (0 for default 16)
 * @return Result containing pointer to AriaMap or error
 */
AriaResultPtr npk_map_new(size_t key_size, size_t value_size, size_t initial_capacity);

/**
 * Free a map (only needed for Wild allocator, no-op for GC).
 * 
 * @param map Map to free
 */
void npk_map_free(AriaMap* map);

// ═══════════════════════════════════════════════════════════════════════
// Map Basic Operations
// ═══════════════════════════════════════════════════════════════════════

/**
 * Get the number of key-value pairs in the map.
 * 
 * @param map Map to query
 * @return Number of entries
 */
size_t npk_map_length(const AriaMap* map);

/**
 * Insert or update a key-value pair in the map.
 * Copies both key and value data.
 * 
 * @param map Map to modify
 * @param key Pointer to key data
 * @param value Pointer to value data
 * @return Result indicating success or error
 */
AriaResultVoid npk_map_insert(AriaMap* map, const void* key, const void* value);

/**
 * Get value for a given key.
 * 
 * @param map Map to search
 * @param key Pointer to key data
 * @return Result containing pointer to value or error if not found
 */
AriaResultPtr npk_map_get(const AriaMap* map, const void* key);

/**
 * Remove a key-value pair from the map.
 * 
 * @param map Map to modify
 * @param key Pointer to key data
 * @return Result indicating success or error if key not found
 */
AriaResultVoid npk_map_remove(AriaMap* map, const void* key);

/**
 * Check if a key exists in the map.
 * 
 * @param map Map to search
 * @param key Pointer to key data
 * @return true if key exists, false otherwise
 */
bool npk_map_has(const AriaMap* map, const void* key);

/**
 * Clear all entries from the map (sets length to 0, keeps capacity).
 * 
 * @param map Map to clear
 */
void npk_map_clear(AriaMap* map);

// ═══════════════════════════════════════════════════════════════════════
// Hash Functions
// ═══════════════════════════════════════════════════════════════════════

/**
 * Generic hash function for byte data.
 * Uses FNV-1a hash algorithm.
 * 
 * @param data Pointer to data to hash
 * @param size Size of data in bytes
 * @return 64-bit hash value
 */
uint64_t npk_hash_bytes(const void* data, size_t size);

/**
 * Hash function for int64 keys.
 * 
 * @param value Integer value to hash
 * @return 64-bit hash value
 */
uint64_t npk_hash_int64(int64_t value);

/**
 * Hash function for string keys (assumes null-terminated).
 * 
 * @param str String to hash
 * @return 64-bit hash value
 */
uint64_t npk_hash_string(const char* str);

// ═══════════════════════════════════════════════════════════════════════
// Simple API (no error handling, for performance)
// ═══════════════════════════════════════════════════════════════════════

/**
 * Create a new map (panics on allocation failure).
 * 
 * @param key_size Size of each key in bytes
 * @param value_size Size of each value in bytes
 * @return Pointer to new AriaMap
 */
AriaMap* npk_map_new_simple(size_t key_size, size_t value_size);

/**
 * Insert key-value pair (panics on allocation failure).
 * 
 * @param map Map to modify
 * @param key Pointer to key data
 * @param value Pointer to value data
 */
void npk_map_insert_simple(AriaMap* map, const void* key, const void* value);

/**
 * Get value for key (returns NULL if not found).
 * 
 * @param map Map to search
 * @param key Pointer to key data
 * @return Pointer to value or NULL
 */
void* npk_map_get_simple(AriaMap* map, const void* key);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_MAP_H

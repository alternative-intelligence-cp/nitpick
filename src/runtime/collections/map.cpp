/**
 * Aria Runtime - Map (Hash Table) Implementation
 */

#include "runtime/map.h"
#include "runtime/allocators.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Default initial capacity (must be power of 2)
#define DEFAULT_MAP_CAPACITY 16
#define DEFAULT_LOAD_FACTOR 0.75f

// FNV-1a constants
#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

// ═══════════════════════════════════════════════════════════════════════
// Hash Functions
// ═══════════════════════════════════════════════════════════════════════

uint64_t aria_hash_bytes(const void* data, size_t size) {
    if (!data) return 0;
    
    const uint8_t* bytes = (const uint8_t*)data;
    uint64_t hash = FNV_OFFSET_BASIS;
    
    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    
    return hash;
}

uint64_t aria_hash_int64(int64_t value) {
    // Simple integer hash (mix bits)
    uint64_t h = (uint64_t)value;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

uint64_t aria_hash_string(const char* str) {
    if (!str) return 0;
    return aria_hash_bytes(str, strlen(str));
}

// ═══════════════════════════════════════════════════════════════════════
// Internal Helpers
// ═══════════════════════════════════════════════════════════════════════

/**
 * Ensure capacity is power of 2.
 */
static size_t next_power_of_2(size_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}

/**
 * Find entry slot for key (returns index of existing key or first empty slot).
 */
static size_t find_slot(const AriaMap* map, const void* key, uint64_t hash) {
    size_t index = hash & (map->capacity - 1);  // Fast modulo for power-of-2
    size_t start = index;
    
    // Linear probing
    while (map->entries[index].occupied) {
        // Check if this is the key we're looking for
        if (map->entries[index].hash == hash &&
            memcmp(map->entries[index].key, key, map->key_size) == 0) {
            return index;
        }
        
        index = (index + 1) & (map->capacity - 1);
        
        // Prevent infinite loop (should never happen if load factor < 1)
        if (index == start) {
            break;
        }
    }
    
    return index;
}

/**
 * Resize map to new capacity (must be power of 2).
 */
static AriaResultVoid resize_map(AriaMap* map, size_t new_capacity) {
    // Allocate new entries array
    AriaMapEntry* new_entries = (AriaMapEntry*)calloc(new_capacity, sizeof(AriaMapEntry));
    if (!new_entries) {
        return aria_result_err_void(aria_error_msg("Map resize failed: out of memory"));
    }
    
    // Save old entries
    AriaMapEntry* old_entries = map->entries;
    size_t old_capacity = map->capacity;
    
    // Update map with new capacity
    map->entries = new_entries;
    map->capacity = new_capacity;
    map->length = 0;  // Will be rebuilt as we reinsert
    
    // Reinsert all existing entries
    for (size_t i = 0; i < old_capacity; i++) {
        if (old_entries[i].occupied) {
            size_t index = find_slot(map, old_entries[i].key, old_entries[i].hash);
            map->entries[index] = old_entries[i];
            map->length++;
        }
    }
    
    // Free old entries
    free(old_entries);
    
    return aria_result_ok_void();
}

// ═══════════════════════════════════════════════════════════════════════
// Map Creation and Destruction
// ═══════════════════════════════════════════════════════════════════════

AriaResultPtr aria_map_new(size_t key_size, size_t value_size, size_t initial_capacity) {
    if (key_size == 0 || value_size == 0) {
        return aria_result_err_ptr(aria_error_msg("Map creation failed: key_size and value_size must be > 0"));
    }
    
    // Allocate map structure
    AriaMap* map = (AriaMap*)malloc(sizeof(AriaMap));
    if (!map) {
        return aria_result_err_ptr(aria_error_msg("Map creation failed: out of memory"));
    }
    
    // Initialize capacity (power of 2)
    size_t capacity = initial_capacity == 0 ? DEFAULT_MAP_CAPACITY : next_power_of_2(initial_capacity);
    
    // Allocate entries array (calloc zeros the memory)
    map->entries = (AriaMapEntry*)calloc(capacity, sizeof(AriaMapEntry));
    if (!map->entries) {
        free(map);
        return aria_result_err_ptr(aria_error_msg("Map creation failed: out of memory"));
    }
    
    map->capacity = capacity;
    map->length = 0;
    map->key_size = key_size;
    map->value_size = value_size;
    map->load_factor = DEFAULT_LOAD_FACTOR;
    
    return aria_result_ok_ptr(map);
}

void aria_map_free(AriaMap* map) {
    if (!map) return;
    
    // Free all keys and values
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->entries[i].occupied) {
            free(map->entries[i].key);
            free(map->entries[i].value);
        }
    }
    
    // Free entries array and map
    free(map->entries);
    free(map);
}

// ═══════════════════════════════════════════════════════════════════════
// Map Basic Operations
// ═══════════════════════════════════════════════════════════════════════

size_t aria_map_length(const AriaMap* map) {
    return map ? map->length : 0;
}

AriaResultVoid aria_map_insert(AriaMap* map, const void* key, const void* value) {
    if (!map || !key || !value) {
        return aria_result_err_void(aria_error_msg("Map insert failed: NULL argument"));
    }
    
    // Check if we need to resize
    float current_load = (float)map->length / (float)map->capacity;
    if (current_load >= map->load_factor) {
        AriaResultVoid resize_result = resize_map(map, map->capacity * 2);
        if (resize_result.error) {
            return resize_result;
        }
    }
    
    // Hash the key
    uint64_t hash = aria_hash_bytes(key, map->key_size);
    
    // Find slot
    size_t index = find_slot(map, key, hash);
    
    // Check if key already exists (update)
    if (map->entries[index].occupied) {
        // Update existing value
        memcpy(map->entries[index].value, value, map->value_size);
        return aria_result_ok_void();
    }
    
    // Insert new entry
    map->entries[index].key = malloc(map->key_size);
    map->entries[index].value = malloc(map->value_size);
    
    if (!map->entries[index].key || !map->entries[index].value) {
        free(map->entries[index].key);
        free(map->entries[index].value);
        return aria_result_err_void(aria_error_msg("Map insert failed: out of memory"));
    }
    
    memcpy(map->entries[index].key, key, map->key_size);
    memcpy(map->entries[index].value, value, map->value_size);
    map->entries[index].occupied = true;
    map->entries[index].hash = hash;
    map->length++;
    
    return aria_result_ok_void();
}

AriaResultPtr aria_map_get(const AriaMap* map, const void* key) {
    if (!map || !key) {
        return aria_result_err_ptr(aria_error_msg("Map get failed: NULL argument"));
    }
    
    // Hash the key
    uint64_t hash = aria_hash_bytes(key, map->key_size);
    
    // Find slot
    size_t index = find_slot(map, key, hash);
    
    // Check if key exists
    if (map->entries[index].occupied) {
        return aria_result_ok_ptr(map->entries[index].value);
    }
    
    return aria_result_err_ptr(aria_error_msg("Map get failed: key not found"));
}

AriaResultVoid aria_map_remove(AriaMap* map, const void* key) {
    if (!map || !key) {
        return aria_result_err_void(aria_error_msg("Map remove failed: NULL argument"));
    }
    
    // Hash the key
    uint64_t hash = aria_hash_bytes(key, map->key_size);
    
    // Find slot
    size_t index = find_slot(map, key, hash);
    
    // Check if key exists
    if (!map->entries[index].occupied) {
        return aria_result_err_void(aria_error_msg("Map remove failed: key not found"));
    }
    
    // Free key and value
    free(map->entries[index].key);
    free(map->entries[index].value);
    
    // Mark as unoccupied
    map->entries[index].occupied = false;
    map->entries[index].key = NULL;
    map->entries[index].value = NULL;
    map->length--;
    
    // Note: We don't rehash here (tombstone strategy)
    // Could be optimized with proper tombstone handling
    
    return aria_result_ok_void();
}

bool aria_map_has(const AriaMap* map, const void* key) {
    if (!map || !key) return false;
    
    uint64_t hash = aria_hash_bytes(key, map->key_size);
    size_t index = find_slot(map, key, hash);
    
    return map->entries[index].occupied;
}

void aria_map_clear(AriaMap* map) {
    if (!map) return;
    
    // Free all keys and values
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->entries[i].occupied) {
            free(map->entries[i].key);
            free(map->entries[i].value);
            map->entries[i].occupied = false;
            map->entries[i].key = NULL;
            map->entries[i].value = NULL;
        }
    }
    
    map->length = 0;
}

// ═══════════════════════════════════════════════════════════════════════
// Simple API (no error handling)
// ═══════════════════════════════════════════════════════════════════════

AriaMap* aria_map_new_simple(size_t key_size, size_t value_size) {
    AriaResultPtr result = aria_map_new(key_size, value_size, 0);
    if (result.error) {
        fprintf(stderr, "FATAL: Map creation failed: %s\n", result.error);
        exit(1);
    }
    return (AriaMap*)result.value;
}

void aria_map_insert_simple(AriaMap* map, const void* key, const void* value) {
    AriaResultVoid result = aria_map_insert(map, key, value);
    if (result.error) {
        fprintf(stderr, "FATAL: Map insert failed: %s\n", result.error);
        exit(1);
    }
}

void* aria_map_get_simple(AriaMap* map, const void* key) {
    AriaResultPtr result = aria_map_get(map, key);
    if (result.error) {
        return NULL;  // Not found
    }
    return result.value;
}

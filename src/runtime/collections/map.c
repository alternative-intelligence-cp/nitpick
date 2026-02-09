/**
 * Runtime support for map (hash table) operations
 * Stub implementation for testing @ operator
 */

#include "runtime/runtime.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct MapNode {
    void* key;
    void* value;
    struct MapNode* next;
} MapNode;

typedef struct {
    MapNode** buckets;
    size_t bucket_count;
    size_t key_size;
    size_t value_size;
    size_t count;
} HashMap;

// Create a new hash map
void* map_new(int64_t key_size, int64_t value_size) {
    HashMap* map = (HashMap*)malloc(sizeof(HashMap));
    if (!map) return NULL;
    
    map->bucket_count = 16;  // Initial capacity
    map->key_size = (size_t)key_size;
    map->value_size = (size_t)value_size;
    map->count = 0;
    
    map->buckets = (MapNode**)calloc(map->bucket_count, sizeof(MapNode*));
    if (!map->buckets) {
        free(map);
        return NULL;
    }
    
    return map;
}

// Simple hash function
static size_t hash_bytes(const void* data, size_t size) {
    size_t hash = 5381;
    const unsigned char* bytes = (const unsigned char*)data;
    for (size_t i = 0; i < size; i++) {
        hash = ((hash << 5) + hash) + bytes[i];
    }
    return hash;
}

// Insert or update a key-value pair
void map_insert(void* map_ptr, void* key_ptr, void* value_ptr) {
    if (!map_ptr || !key_ptr || !value_ptr) return;
    
    HashMap* map = (HashMap*)map_ptr;
    size_t hash = hash_bytes(key_ptr, map->key_size);
    size_t bucket_idx = hash % map->bucket_count;
    
    // Check if key already exists
    MapNode* node = map->buckets[bucket_idx];
    while (node) {
        if (memcmp(node->key, key_ptr, map->key_size) == 0) {
            // Update existing value
            memcpy(node->value, value_ptr, map->value_size);
            return;
        }
        node = node->next;
    }
    
    // Create new node
    MapNode* new_node = (MapNode*)malloc(sizeof(MapNode));
    if (!new_node) return;
    
    new_node->key = malloc(map->key_size);
    new_node->value = malloc(map->value_size);
    if (!new_node->key || !new_node->value) {
        free(new_node->key);
        free(new_node->value);
        free(new_node);
        return;
    }
    
    memcpy(new_node->key, key_ptr, map->key_size);
    memcpy(new_node->value, value_ptr, map->value_size);
    new_node->next = map->buckets[bucket_idx];
    map->buckets[bucket_idx] = new_node;
    map->count++;
}

// Check if map contains a key
int32_t map_has(void* map_ptr, void* key_ptr) {
    if (!map_ptr || !key_ptr) return 0;
    
    HashMap* map = (HashMap*)map_ptr;
    size_t hash = hash_bytes(key_ptr, map->key_size);
    size_t bucket_idx = hash % map->bucket_count;
    
    MapNode* node = map->buckets[bucket_idx];
    while (node) {
        if (memcmp(node->key, key_ptr, map->key_size) == 0) {
            return 1;  // Found
        }
        node = node->next;
    }
    
    return 0;  // Not found
}

// Get value for a key (returns pointer to value or NULL)
void* map_get(void* map_ptr, void* key_ptr) {
    if (!map_ptr || !key_ptr) return NULL;
    
    HashMap* map = (HashMap*)map_ptr;
    size_t hash = hash_bytes(key_ptr, map->key_size);
    size_t bucket_idx = hash % map->bucket_count;
    
    MapNode* node = map->buckets[bucket_idx];
    while (node) {
        if (memcmp(node->key, key_ptr, map->key_size) == 0) {
            return node->value;  // Found - return pointer to value
        }
        node = node->next;
    }
    
    return NULL;  // Not found
}

// Get number of entries in map
int64_t map_length(void* map_ptr) {
    if (!map_ptr) return 0;
    HashMap* map = (HashMap*)map_ptr;
    return (int64_t)map->count;
}

// Remove a key from map
void map_remove(void* map_ptr, void* key_ptr) {
    if (!map_ptr || !key_ptr) return;
    
    HashMap* map = (HashMap*)map_ptr;
    size_t hash = hash_bytes(key_ptr, map->key_size);
    size_t bucket_idx = hash % map->bucket_count;
    
    MapNode** node_ptr = &map->buckets[bucket_idx];
    while (*node_ptr) {
        MapNode* node = *node_ptr;
        if (memcmp(node->key, key_ptr, map->key_size) == 0) {
            // Found - remove it
            *node_ptr = node->next;
            free(node->key);
            free(node->value);
            free(node);
            map->count--;
            return;
        }
        node_ptr = &node->next;
    }
}

// Free the map
void map_free(void* map_ptr) {
    if (!map_ptr) return;
    
    HashMap* map = (HashMap*)map_ptr;
    
    // Free all nodes
    for (size_t i = 0; i < map->bucket_count; i++) {
        MapNode* node = map->buckets[i];
        while (node) {
            MapNode* next = node->next;
            free(node->key);
            free(node->value);
            free(node);
            node = next;
        }
    }
    
    free(map->buckets);
    free(map);
}

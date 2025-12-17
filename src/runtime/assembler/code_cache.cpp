/**
 * Aria Runtime - Code Cache Implementation
 * 
 * LRU cache for compiled functions with hash-based lookup and eviction.
 */

#include "runtime/code_cache.h"

#include <unordered_map>
#include <list>
#include <cstdlib>
#include <cstring>
#include <ctime>

// ============================================================================
// Internal Structures
// ============================================================================

/**
 * Cache Entry (internal)
 * 
 * Stored in hash table and LRU list.
 */
struct AriaCacheEntry {
    AriaCachedFunction function;     // Public function metadata
    std::list<uint64_t>::iterator lru_iter; // Iterator in LRU list
};

/**
 * Code Cache State (internal)
 */
struct AriaCodeCache {
    // Hash table: hash → cache entry
    std::unordered_map<uint64_t, AriaCacheEntry> entries;
    
    // LRU list: most recent at front, least recent at back
    std::list<uint64_t> lru_list;
    
    // Configuration
    size_t max_entries;
    size_t max_memory_bytes;
    
    // Statistics
    uint64_t total_hits;
    uint64_t total_misses;
    uint64_t total_evictions;
    uint64_t total_inserts;
    size_t current_memory_bytes;
    
    // Monotonic time counter (for access tracking)
    uint64_t time_counter;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Get monotonic timestamp
 */
static uint64_t get_monotonic_time(AriaCodeCache* cache) {
    return cache->time_counter++;
}

/**
 * Move entry to front of LRU list (most recently used)
 */
static void touch_entry(AriaCodeCache* cache, AriaCacheEntry* entry, uint64_t hash) {
    // Remove from current position
    cache->lru_list.erase(entry->lru_iter);
    
    // Add to front
    cache->lru_list.push_front(hash);
    entry->lru_iter = cache->lru_list.begin();
    
    // Update metadata
    entry->function.last_access_time = get_monotonic_time(cache);
    entry->function.access_count++;
}

/**
 * Evict least recently used entry
 */
static void evict_lru(AriaCodeCache* cache) {
    if (cache->lru_list.empty()) return;
    
    // Get LRU hash (back of list)
    uint64_t lru_hash = cache->lru_list.back();
    
    // Find entry
    auto it = cache->entries.find(lru_hash);
    if (it == cache->entries.end()) return; // Should never happen
    
    // Update memory counter
    cache->current_memory_bytes -= it->second.function.code_size;
    
    // Remove from LRU list
    cache->lru_list.pop_back();
    
    // Remove from hash table
    cache->entries.erase(it);
    
    // Update statistics
    cache->total_evictions++;
}

/**
 * Evict entries until constraints are met
 */
static void enforce_limits(AriaCodeCache* cache) {
    // Evict by entry count
    while (cache->max_entries > 0 && cache->entries.size() > cache->max_entries) {
        evict_lru(cache);
    }
    
    // Evict by memory size
    while (cache->max_memory_bytes > 0 && cache->current_memory_bytes > cache->max_memory_bytes) {
        evict_lru(cache);
    }
}

// ============================================================================
// Hash Functions (FNV-1a)
// ============================================================================

// FNV-1a constants
static const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
static const uint64_t FNV_PRIME = 1099511628211ULL;

uint64_t aria_code_cache_hash_bytes(const uint8_t* data, size_t size) {
    if (!data || size == 0) return 0;
    
    uint64_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

uint64_t aria_code_cache_hash_string(const char* str) {
    if (!str) return 0;
    
    uint64_t hash = FNV_OFFSET_BASIS;
    while (*str) {
        hash ^= (uint8_t)(*str);
        hash *= FNV_PRIME;
        str++;
    }
    return hash;
}

uint64_t aria_code_cache_combine_hashes(uint64_t hash1, uint64_t hash2) {
    // XOR and multiply for good mixing
    uint64_t combined = hash1 ^ hash2;
    combined *= FNV_PRIME;
    return combined;
}

// ============================================================================
// Cache Lifecycle
// ============================================================================

AriaCodeCache* aria_code_cache_create(size_t max_entries, size_t max_memory_bytes) {
    AriaCodeCache* cache = new AriaCodeCache();
    if (!cache) return nullptr;
    
    cache->max_entries = max_entries;
    cache->max_memory_bytes = max_memory_bytes;
    cache->total_hits = 0;
    cache->total_misses = 0;
    cache->total_evictions = 0;
    cache->total_inserts = 0;
    cache->current_memory_bytes = 0;
    cache->time_counter = 0;
    
    return cache;
}

void aria_code_cache_destroy(AriaCodeCache* cache) {
    if (!cache) return;
    delete cache;
}

// ============================================================================
// Cache Operations
// ============================================================================

AriaCachedFunction* aria_code_cache_lookup(AriaCodeCache* cache, uint64_t hash) {
    if (!cache) return nullptr;
    
    auto it = cache->entries.find(hash);
    if (it == cache->entries.end()) {
        // Cache miss
        cache->total_misses++;
        return nullptr;
    }
    
    // Cache hit
    cache->total_hits++;
    
    // Update LRU (move to front)
    touch_entry(cache, &it->second, hash);
    
    return &it->second.function;
}

int aria_code_cache_insert(AriaCodeCache* cache, uint64_t hash,
                            void* function_ptr, size_t code_size,
                            int backend_type, int optimization_level) {
    if (!cache || !function_ptr) return -1;
    
    // Check if already exists (update instead of insert)
    auto it = cache->entries.find(hash);
    if (it != cache->entries.end()) {
        // Update existing entry
        it->second.function.function_ptr = function_ptr;
        cache->current_memory_bytes -= it->second.function.code_size;
        cache->current_memory_bytes += code_size;
        it->second.function.code_size = code_size;
        it->second.function.backend_type = backend_type;
        it->second.function.optimization_level = optimization_level;
        
        // Move to front of LRU
        touch_entry(cache, &it->second, hash);
        return 0;
    }
    
    // New entry - enforce limits BEFORE adding
    // Temporarily add memory to see if eviction is needed
    size_t temp_memory = cache->current_memory_bytes + code_size;
    size_t temp_entries = cache->entries.size() + 1;
    
    // Evict until we have room for the new entry
    while ((cache->max_entries > 0 && temp_entries > cache->max_entries) ||
           (cache->max_memory_bytes > 0 && temp_memory > cache->max_memory_bytes)) {
        evict_lru(cache);
        temp_entries = cache->entries.size() + 1;
        temp_memory = cache->current_memory_bytes + code_size;
    }
    
    // Now add the entry
    cache->current_memory_bytes += code_size;
    
    // Create new entry
    AriaCacheEntry entry;
    entry.function.function_ptr = function_ptr;
    entry.function.hash = hash;
    entry.function.code_size = code_size;
    entry.function.access_count = 0;
    entry.function.last_access_time = get_monotonic_time(cache);
    entry.function.backend_type = backend_type;
    entry.function.optimization_level = optimization_level;
    
    // Add to LRU list (front = most recent)
    cache->lru_list.push_front(hash);
    entry.lru_iter = cache->lru_list.begin();
    
    // Add to hash table
    cache->entries[hash] = entry;
    
    // Update statistics
    cache->total_inserts++;
    
    return 0;
}

int aria_code_cache_evict(AriaCodeCache* cache, uint64_t hash) {
    if (!cache) return -1;
    
    auto it = cache->entries.find(hash);
    if (it == cache->entries.end()) {
        return -1; // Not found
    }
    
    // Update memory counter
    cache->current_memory_bytes -= it->second.function.code_size;
    
    // Remove from LRU list
    cache->lru_list.erase(it->second.lru_iter);
    
    // Remove from hash table
    cache->entries.erase(it);
    
    // Update statistics
    cache->total_evictions++;
    
    return 0;
}

void aria_code_cache_clear(AriaCodeCache* cache) {
    if (!cache) return;
    
    cache->entries.clear();
    cache->lru_list.clear();
    cache->current_memory_bytes = 0;
    
    // Reset statistics
    cache->total_hits = 0;
    cache->total_misses = 0;
    cache->total_evictions = 0;
    cache->total_inserts = 0;
    cache->time_counter = 0;
}

// ============================================================================
// Statistics
// ============================================================================

AriaCodeCacheStats aria_code_cache_stats(const AriaCodeCache* cache) {
    AriaCodeCacheStats stats = {0};
    if (!cache) return stats;
    
    stats.total_entries = cache->entries.size();
    stats.total_memory_bytes = cache->current_memory_bytes;
    stats.total_hits = cache->total_hits;
    stats.total_misses = cache->total_misses;
    stats.total_evictions = cache->total_evictions;
    stats.total_inserts = cache->total_inserts;
    
    // Compute hit rate
    uint64_t total_accesses = stats.total_hits + stats.total_misses;
    if (total_accesses > 0) {
        stats.hit_rate = (double)stats.total_hits / (double)total_accesses;
    } else {
        stats.hit_rate = 0.0;
    }
    
    return stats;
}

void aria_code_cache_reset_stats(AriaCodeCache* cache) {
    if (!cache) return;
    
    cache->total_hits = 0;
    cache->total_misses = 0;
    cache->total_evictions = 0;
    cache->total_inserts = 0;
}

// ============================================================================
// Configuration
// ============================================================================

void aria_code_cache_set_max_entries(AriaCodeCache* cache, size_t max_entries) {
    if (!cache) return;
    
    cache->max_entries = max_entries;
    enforce_limits(cache);
}

void aria_code_cache_set_max_memory(AriaCodeCache* cache, size_t max_memory_bytes) {
    if (!cache) return;
    
    cache->max_memory_bytes = max_memory_bytes;
    enforce_limits(cache);
}

size_t aria_code_cache_get_max_entries(const AriaCodeCache* cache) {
    return cache ? cache->max_entries : 0;
}

size_t aria_code_cache_get_max_memory(const AriaCodeCache* cache) {
    return cache ? cache->max_memory_bytes : 0;
}

// ============================================================================
// Persistence (Stub Implementation)
// ============================================================================

int aria_code_cache_save(const AriaCodeCache* cache, const char* path) {
    // TODO: Implement serialization
    // For now, return success (no-op)
    (void)cache;
    (void)path;
    return 0;
}

int aria_code_cache_load(AriaCodeCache* cache, const char* path) {
    // TODO: Implement deserialization
    // For now, return success (no-op)
    (void)cache;
    (void)path;
    return 0;
}

/**
 * Aria Runtime - Code Cache
 * 
 * LRU cache for compiled functions to avoid recompilation overhead.
 * Caches JIT-compiled functions from both ARA assembler and LLVM JIT.
 * 
 * Features:
 * - LRU eviction policy (configurable max entries and memory)
 * - Hash-based lookup (function signature + body content)
 * - Backend-agnostic (works with ARA and LLVM JIT)
 * - Statistics tracking (hits, misses, evictions)
 * - Optional disk persistence for startup performance
 * 
 * Usage:
 *   AriaCodeCache* cache = aria_code_cache_create(1000, 10 * 1024 * 1024); // 1000 entries, 10MB
 *   
 *   // Before compilation, check cache
 *   uint64_t hash = aria_code_cache_hash_bytes(bytecode, size);
 *   AriaJITFunction* func = aria_code_cache_lookup(cache, hash);
 *   if (func == NULL) {
 *       // Cache miss - compile and insert
 *       func = compile_function(...);
 *       aria_code_cache_insert(cache, hash, func, func_size);
 *   }
 *   
 *   aria_code_cache_destroy(cache);
 */

#ifndef ARIA_CODE_CACHE_H
#define ARIA_CODE_CACHE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Types
// ============================================================================

/**
 * Code Cache Handle
 * 
 * Opaque handle to cache state. Stores:
 * - Hash table for O(1) lookup
 * - LRU linked list for eviction
 * - Statistics counters
 * - Configuration (max entries, max memory)
 */
typedef struct AriaCodeCache AriaCodeCache;

/**
 * Cached Function Handle
 * 
 * Opaque handle to a cached function. Contains:
 * - Function pointer (executable code)
 * - Metadata (size, access time, access count)
 * - Backend type (ARA or LLVM)
 */
typedef struct AriaCachedFunction {
    void* function_ptr;          // Executable function pointer
    uint64_t hash;               // Cache key (function signature + body hash)
    size_t code_size;            // Size of compiled code in bytes
    uint64_t access_count;       // Number of times accessed
    uint64_t last_access_time;   // Last access timestamp (monotonic)
    int backend_type;            // 0 = ARA, 1 = LLVM
    int optimization_level;      // Optimization level (for LLVM)
} AriaCachedFunction;

/**
 * Cache Statistics
 * 
 * Runtime statistics for monitoring cache performance.
 */
typedef struct AriaCodeCacheStats {
    size_t total_entries;        // Current number of cached functions
    size_t total_memory_bytes;   // Total memory used by cached code
    uint64_t total_hits;         // Cache hits (successful lookups)
    uint64_t total_misses;       // Cache misses (failed lookups)
    uint64_t total_evictions;    // Number of evictions performed
    uint64_t total_inserts;      // Number of insertions
    double hit_rate;             // Computed hit rate (hits / (hits + misses))
} AriaCodeCacheStats;

// ============================================================================
// Cache Lifecycle
// ============================================================================

/**
 * Create code cache
 * 
 * @param max_entries Maximum number of cached functions (0 = unlimited)
 * @param max_memory_bytes Maximum memory for cached code (0 = unlimited)
 * @return Cache handle or NULL on failure
 * 
 * Example:
 *   AriaCodeCache* cache = aria_code_cache_create(1000, 10 * 1024 * 1024);
 */
AriaCodeCache* aria_code_cache_create(size_t max_entries, size_t max_memory_bytes);

/**
 * Destroy code cache
 * 
 * Frees all cached functions and cache state.
 * Does NOT free the original function pointers (caller owns them).
 * 
 * @param cache Cache handle
 */
void aria_code_cache_destroy(AriaCodeCache* cache);

// ============================================================================
// Cache Operations
// ============================================================================

/**
 * Lookup function in cache
 * 
 * @param cache Cache handle
 * @param hash Function hash (from aria_code_cache_hash_*)
 * @return Cached function handle or NULL if not found
 * 
 * Updates last_access_time and access_count on hit.
 * 
 * Example:
 *   uint64_t hash = aria_code_cache_hash_bytes(bytecode, size);
 *   AriaCachedFunction* func = aria_code_cache_lookup(cache, hash);
 *   if (func != NULL) {
 *       // Cache hit - use func->function_ptr
 *   }
 */
AriaCachedFunction* aria_code_cache_lookup(AriaCodeCache* cache, uint64_t hash);

/**
 * Insert function into cache
 * 
 * @param cache Cache handle
 * @param hash Function hash
 * @param function_ptr Compiled function pointer
 * @param code_size Size of compiled code
 * @param backend_type 0 = ARA, 1 = LLVM
 * @param optimization_level Optimization level (0-3 for LLVM, 0 for ARA)
 * @return 0 on success, -1 on failure
 * 
 * May trigger LRU eviction if cache is full.
 * Takes ownership of function_ptr (caller should not free).
 * 
 * Example:
 *   aria_code_cache_insert(cache, hash, func_ptr, size, 0, 0); // ARA
 *   aria_code_cache_insert(cache, hash, func_ptr, size, 1, 2); // LLVM O2
 */
int aria_code_cache_insert(AriaCodeCache* cache, uint64_t hash, 
                            void* function_ptr, size_t code_size,
                            int backend_type, int optimization_level);

/**
 * Evict function from cache
 * 
 * @param cache Cache handle
 * @param hash Function hash
 * @return 0 on success, -1 if not found
 * 
 * Removes specific function from cache.
 * Caller is responsible for freeing the function pointer if needed.
 */
int aria_code_cache_evict(AriaCodeCache* cache, uint64_t hash);

/**
 * Clear entire cache
 * 
 * @param cache Cache handle
 * 
 * Removes all cached functions. Resets statistics.
 * Caller is responsible for freeing function pointers if needed.
 */
void aria_code_cache_clear(AriaCodeCache* cache);

// ============================================================================
// Hash Functions
// ============================================================================

/**
 * Hash byte array (for bytecode)
 * 
 * @param data Byte array
 * @param size Array size
 * @return 64-bit hash
 * 
 * Uses FNV-1a hash for speed and good distribution.
 * 
 * Example:
 *   uint64_t hash = aria_code_cache_hash_bytes(bytecode, bytecode_size);
 */
uint64_t aria_code_cache_hash_bytes(const uint8_t* data, size_t size);

/**
 * Hash string (for IR or function names)
 * 
 * @param str Null-terminated string
 * @return 64-bit hash
 * 
 * Example:
 *   uint64_t hash = aria_code_cache_hash_string(llvm_ir_text);
 */
uint64_t aria_code_cache_hash_string(const char* str);

/**
 * Combine hashes (for composite keys)
 * 
 * @param hash1 First hash
 * @param hash2 Second hash
 * @return Combined hash
 * 
 * Example:
 *   uint64_t sig_hash = aria_code_cache_hash_string(function_signature);
 *   uint64_t body_hash = aria_code_cache_hash_bytes(bytecode, size);
 *   uint64_t key = aria_code_cache_combine_hashes(sig_hash, body_hash);
 */
uint64_t aria_code_cache_combine_hashes(uint64_t hash1, uint64_t hash2);

// ============================================================================
// Statistics
// ============================================================================

/**
 * Get cache statistics
 * 
 * @param cache Cache handle
 * @return Statistics structure
 * 
 * Example:
 *   AriaCodeCacheStats stats = aria_code_cache_stats(cache);
 *   printf("Hit rate: %.2f%%\n", stats.hit_rate * 100);
 */
AriaCodeCacheStats aria_code_cache_stats(const AriaCodeCache* cache);

/**
 * Reset statistics counters
 * 
 * @param cache Cache handle
 * 
 * Resets hits, misses, evictions, inserts. Does not affect cached entries.
 */
void aria_code_cache_reset_stats(AriaCodeCache* cache);

// ============================================================================
// Persistence (Optional)
// ============================================================================

/**
 * Save cache to disk
 * 
 * @param cache Cache handle
 * @param path File path
 * @return 0 on success, -1 on failure
 * 
 * Saves cache metadata and function mappings.
 * Does NOT save executable code (security risk).
 * Saves hash→size mappings for preallocation on load.
 * 
 * Example:
 *   aria_code_cache_save(cache, "/tmp/aria_code_cache.db");
 */
int aria_code_cache_save(const AriaCodeCache* cache, const char* path);

/**
 * Load cache from disk
 * 
 * @param cache Cache handle
 * @param path File path
 * @return 0 on success, -1 on failure
 * 
 * Loads cache metadata. Functions will be recompiled on first use.
 * Useful for preallocating cache structure at startup.
 */
int aria_code_cache_load(AriaCodeCache* cache, const char* path);

// ============================================================================
// Configuration
// ============================================================================

/**
 * Set maximum entries
 * 
 * @param cache Cache handle
 * @param max_entries Maximum entries (0 = unlimited)
 * 
 * Triggers eviction if current size exceeds new limit.
 */
void aria_code_cache_set_max_entries(AriaCodeCache* cache, size_t max_entries);

/**
 * Set maximum memory
 * 
 * @param cache Cache handle
 * @param max_memory_bytes Maximum memory (0 = unlimited)
 * 
 * Triggers eviction if current memory exceeds new limit.
 */
void aria_code_cache_set_max_memory(AriaCodeCache* cache, size_t max_memory_bytes);

/**
 * Get maximum entries
 * 
 * @param cache Cache handle
 * @return Maximum entries (0 = unlimited)
 */
size_t aria_code_cache_get_max_entries(const AriaCodeCache* cache);

/**
 * Get maximum memory
 * 
 * @param cache Cache handle
 * @return Maximum memory bytes (0 = unlimited)
 */
size_t aria_code_cache_get_max_memory(const AriaCodeCache* cache);

#ifdef __cplusplus
}
#endif

#endif // ARIA_CODE_CACHE_H

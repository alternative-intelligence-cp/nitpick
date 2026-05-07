/**
 * Slab Allocator - Phase 4.2 Stage 4
 * 
 * Object-caching allocator with constructor preservation (SLUB-style).
 * Target: <30ns allocation with object reuse
 * 
 * Based on Gemini research: SLUB architecture with slab coloring for cache optimization.
 * Unlike Pool allocators (raw memory), Slab maintains object state across alloc/free cycles.
 * 
 * Key Features:
 * - Object caching: Free objects keep their initialized state
 * - Constructor/destructor hooks: Initialize once, reuse many times
 * - Slab coloring: Prevent cache conflicts
 * - Partial/Full/Empty list management: Efficient slab reuse
 * - Lockless fast path: Per-CPU active slab optimization
 * 
 * Thread Safety: NOT thread-safe (use ThreadSafe wrapper if needed)
 */

#ifndef ARIA_RUNTIME_SLAB_H
#define ARIA_RUNTIME_SLAB_H

#include "result.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct npk_slab npk_slab;
typedef struct npk_slab_cache npk_slab_cache;

// ============================================================================
// Constructor/Destructor Function Pointers
// ============================================================================

/**
 * Object constructor - called when object is first allocated
 * 
 * @param obj Pointer to object memory
 * @param size Size of object (for validation)
 */
typedef void (*npk_slab_ctor)(void* obj, size_t size);

/**
 * Object destructor - called when slab is destroyed (NOT on free)
 * 
 * @param obj Pointer to object memory
 * @param size Size of object
 */
typedef void (*npk_slab_dtor)(void* obj, size_t size);

// ============================================================================
// Error Codes
// ============================================================================

/**
 * Slab allocator error codes
 */
typedef enum {
    ARIA_SLAB_OK = 0,
    ARIA_SLAB_ERR_INVALID_OBJECT_SIZE,   // object_size == 0
    ARIA_SLAB_ERR_INVALID_SLAB_SIZE,     // slab_size < object_size
    ARIA_SLAB_ERR_OUT_OF_MEMORY          // System malloc failed
} AriaSlabError;

// ============================================================================
// Result Types
// ============================================================================

/**
 * Result type for slab cache creation
 */
typedef struct {
    npk_slab_cache* cache;        // Slab cache handle (NULL on error)
    AriaSlabError error_code;      // Error code
    size_t requested_slab_size;    // Requested slab size
    size_t object_size;            // Fixed object size
} AriaSlabResult;

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Slab state enumeration
 */
typedef enum {
    ARIA_SLAB_EMPTY,    // No objects allocated (can be freed to OS)
    ARIA_SLAB_PARTIAL,  // Some objects allocated, some free
    ARIA_SLAB_FULL      // All objects allocated (no free objects)
} AriaSlabState;

/**
 * Individual slab - contiguous memory divided into fixed-size objects
 * 
 * Manages a single page or multi-page allocation.
 * Uses intrusive free list (like Pool) but tracks state for cache optimization.
 */
struct npk_slab {
    npk_slab* next;           // Next slab in partial/full/empty list
    void* memory;              // Slab memory region
    void* freelist_head;       // Head of free object list (intrusive)
    
    size_t capacity;           // Total bytes in slab
    size_t object_size;        // Size of each object
    size_t num_objects;        // Total objects in slab
    size_t used_objects;       // Objects currently allocated
    size_t color_offset;       // Slab coloring offset (bytes)
    
    AriaSlabState state;       // Current state (empty/partial/full)
};

/**
 * Slab cache - manages collection of slabs for a specific object type
 * 
 * Maintains three lists:
 * - Partial: Slabs with some free objects (primary allocation source)
 * - Full: Slabs with no free objects (no allocation possible)
 * - Empty: Slabs with no allocated objects (can be freed to OS)
 * 
 * Lifecycle:
 * 1. New slab starts as EMPTY
 * 2. First allocation moves it to PARTIAL
 * 3. Last allocation moves it to FULL
 * 4. First free from FULL moves it to PARTIAL
 * 5. Last free from PARTIAL moves it to EMPTY
 */
struct npk_slab_cache {
    // Slab lists
    npk_slab* partial_list;   // Slabs with free objects
    npk_slab* full_list;      // Slabs with no free objects
    npk_slab* empty_list;     // Slabs with all objects free
    
    // Currently active slab (fast path)
    npk_slab* active_slab;    // Slab for current allocations
    
    // Configuration
    size_t object_size;        // Fixed size of each object
    size_t slab_size;          // Size of each slab (bytes)
    size_t alignment;          // Object alignment
    size_t cache_line_size;    // For slab coloring (typically 64)
    size_t color_next;         // Next color offset to use
    
    // Constructor/destructor
    npk_slab_ctor ctor;       // Object constructor (can be NULL)
    npk_slab_dtor dtor;       // Object destructor (can be NULL)
    
    // Statistics
    size_t total_slabs;        // Total slabs created
    size_t total_objects;      // Total objects across all slabs
    size_t allocated_objects;  // Objects currently in use
    size_t total_capacity;     // Total bytes reserved
};

// ============================================================================
// Lifecycle Functions
// ============================================================================

/**
 * Create a new slab cache
 * 
 * @param object_size Size of each object (must be > 0)
 * @param slab_size Size of each slab in bytes (typically 4096 or 8192)
 * @param alignment Object alignment (0 = default to alignof(void*))
 * @param ctor Object constructor (can be NULL)
 * @param dtor Object destructor (can be NULL)
 * @return AriaSlabResult with cache handle or error
 * 
 * Example:
 *   // Simple slab (no constructor)
 *   AriaSlabResult result = npk_slab_cache_new(sizeof(Node), 4096, 0, NULL, NULL);
 *   
 *   // Slab with initialization
 *   void init_mutex(void* obj, size_t size) {
 *       Mutex* m = (Mutex*)obj;
 *       pthread_mutex_init(&m->lock, NULL);
 *   }
 *   void destroy_mutex(void* obj, size_t size) {
 *       Mutex* m = (Mutex*)obj;
 *       pthread_mutex_destroy(&m->lock);
 *   }
 *   AriaSlabResult result = npk_slab_cache_new(
 *       sizeof(Mutex), 4096, 0, init_mutex, destroy_mutex
 *   );
 */
AriaSlabResult npk_slab_cache_new(
    size_t object_size,
    size_t slab_size,
    size_t alignment,
    npk_slab_ctor ctor,
    npk_slab_dtor dtor
);

/**
 * Destroy slab cache and free all slabs
 * 
 * Calls destructor on all allocated objects before freeing slabs.
 * Walks all lists (partial, full, empty) and frees memory.
 * 
 * @param cache Slab cache to destroy (NULL-safe)
 * 
 * Complexity: O(num_slabs + allocated_objects)
 */
void npk_slab_cache_destroy(npk_slab_cache* cache);

// ============================================================================
// Allocation Functions
// ============================================================================

/**
 * Allocate an object from the slab cache
 * 
 * FAST PATH (active slab has free objects): ~5-10ns
 *   1. Check active_slab->freelist_head
 *   2. Pop object from freelist
 *   3. Update state if slab becomes full
 * 
 * SLOW PATH (need new slab): ~100-200ns
 *   1. Check partial_list for slab with free objects
 *   2. If found, make it active and allocate
 *   3. If not found, allocate new slab, color it, construct objects
 * 
 * @param cache Slab cache
 * @return AriaAllocResult with pointer or error
 * 
 * Object State:
 * - If constructor provided: Object is PRE-INITIALIZED
 * - If no constructor: Object memory is UNINITIALIZED
 * 
 * Performance:
 * - With object reuse: ~5-10ns (freelist pop)
 * - New slab allocation: ~100-200ns (includes coloring + construction)
 * - Constructor amortization: Constructor cost / object_lifetime
 */
AriaAllocResult npk_slab_cache_alloc(npk_slab_cache* cache);

/**
 * Free an object back to the slab cache
 * 
 * CRITICAL: Object is NOT destructed. Its state is preserved for reuse.
 * This is the key difference from Pool allocators.
 * 
 * Algorithm:
 * 1. Determine which slab owns this pointer
 * 2. Push object to slab's freelist
 * 3. Update slab state (full -> partial, partial -> empty)
 * 4. If slab becomes empty, consider moving to empty_list
 * 
 * @param cache Slab cache
 * @param ptr Pointer to free (must have been allocated from this cache)
 * 
 * Performance: ~5-10ns (freelist push + state update)
 * 
 * Complexity: O(1) if object is from active_slab, O(num_slabs) otherwise
 */
void npk_slab_cache_free(npk_slab_cache* cache, void* ptr);

/**
 * Shrink cache by freeing empty slabs
 * 
 * Walks empty_list and frees slabs that have no allocated objects.
 * Calls destructor on all objects before freeing.
 * 
 * Use case: Periodic memory pressure relief in long-running servers.
 * 
 * @param cache Slab cache
 * @return Number of slabs freed
 * 
 * Complexity: O(empty_slabs * objects_per_slab)
 */
size_t npk_slab_cache_shrink(npk_slab_cache* cache);

// ============================================================================
// Query Functions
// ============================================================================

/**
 * Get slab cache statistics
 * 
 * @param cache Slab cache
 * @param total_slabs Out: Total slabs (partial + full + empty) or NULL
 * @param total_objects Out: Total objects across all slabs or NULL
 * @param allocated_objects Out: Objects currently in use or NULL
 * @param free_objects Out: Objects available for allocation or NULL
 * @param total_capacity Out: Total bytes reserved from system or NULL
 */
void npk_slab_cache_get_stats(
    const npk_slab_cache* cache,
    size_t* total_slabs,
    size_t* total_objects,
    size_t* allocated_objects,
    size_t* free_objects,
    size_t* total_capacity
);

/**
 * Get object size for this cache
 */
static inline size_t npk_slab_cache_object_size(const npk_slab_cache* cache) {
    return cache->object_size;
}

/**
 * Get slab size for this cache
 */
static inline size_t npk_slab_cache_slab_size(const npk_slab_cache* cache) {
    return cache->slab_size;
}

// ============================================================================
// Result Helper Functions
// ============================================================================

/**
 * Check if slab cache creation succeeded
 */
static inline bool npk_slab_result_is_ok(AriaSlabResult result) {
    return result.error_code == ARIA_SLAB_OK && result.cache != NULL;
}

/**
 * Check if slab cache creation failed
 */
static inline bool npk_slab_result_is_err(AriaSlabResult result) {
    return result.error_code != ARIA_SLAB_OK || result.cache == NULL;
}

/**
 * Get error message for slab error code
 */
const char* npk_slab_error_message(AriaSlabError error);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_SLAB_H

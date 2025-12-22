/**
 * Slab Allocator Implementation - Phase 4.2 Stage 4
 * 
 * SLUB-style object-caching allocator with slab coloring.
 * See header for architecture details.
 */

#include "runtime/slab.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// ============================================================================
// Constants
// ============================================================================

#define DEFAULT_CACHE_LINE_SIZE 64    // Modern x86-64 CPUs
#define DEFAULT_ALIGNMENT sizeof(void*)
#define MIN_SLAB_SIZE 256

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Free list node (intrusive)
 */
typedef struct FreeNode {
    struct FreeNode* next;
} FreeNode;

/**
 * Align value up to alignment boundary
 */
static inline size_t align_up(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

/**
 * Check if pointer is within slab's memory range
 */
static bool slab_contains(const aria_slab* slab, const void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t start = (uintptr_t)slab->memory + slab->color_offset;
    uintptr_t end = start + (slab->num_objects * slab->object_size);
    return addr >= start && addr < end;
}

/**
 * Find which slab owns a pointer
 * 
 * Walks all slab lists to find the owning slab.
 * This is O(num_slabs) but typically small (<10 slabs per cache).
 */
static aria_slab* find_slab_for_pointer(aria_slab_cache* cache, const void* ptr) {
    // Check active slab first (most likely)
    if (cache->active_slab && slab_contains(cache->active_slab, ptr)) {
        return cache->active_slab;
    }
    
    // Check partial list
    aria_slab* slab = cache->partial_list;
    while (slab) {
        if (slab_contains(slab, ptr)) {
            return slab;
        }
        slab = slab->next;
    }
    
    // Check full list
    slab = cache->full_list;
    while (slab) {
        if (slab_contains(slab, ptr)) {
            return slab;
        }
        slab = slab->next;
    }
    
    // Check empty list
    slab = cache->empty_list;
    while (slab) {
        if (slab_contains(slab, ptr)) {
            return slab;
        }
        slab = slab->next;
    }
    
    return NULL;  // Not found (user error)
}

/**
 * Remove slab from its current list
 */
static void unlink_slab(aria_slab_cache* cache, aria_slab* slab) {
    aria_slab** list_head = NULL;
    
    switch (slab->state) {
        case ARIA_SLAB_EMPTY:
            list_head = &cache->empty_list;
            break;
        case ARIA_SLAB_PARTIAL:
            list_head = &cache->partial_list;
            break;
        case ARIA_SLAB_FULL:
            list_head = &cache->full_list;
            break;
    }
    
    if (!list_head) {
        return;
    }
    
    // Remove from list
    if (*list_head == slab) {
        *list_head = slab->next;
    } else {
        aria_slab* curr = *list_head;
        while (curr && curr->next != slab) {
            curr = curr->next;
        }
        if (curr) {
            curr->next = slab->next;
        }
    }
    
    slab->next = NULL;
}

/**
 * Add slab to appropriate list based on its state
 */
static void link_slab(aria_slab_cache* cache, aria_slab* slab) {
    aria_slab** list_head = NULL;
    
    switch (slab->state) {
        case ARIA_SLAB_EMPTY:
            list_head = &cache->empty_list;
            break;
        case ARIA_SLAB_PARTIAL:
            list_head = &cache->partial_list;
            break;
        case ARIA_SLAB_FULL:
            list_head = &cache->full_list;
            break;
    }
    
    if (!list_head) {
        return;
    }
    
    // Add to head of list
    slab->next = *list_head;
    *list_head = slab;
}

/**
 * Update slab state based on usage
 */
static void update_slab_state(aria_slab_cache* cache, aria_slab* slab) {
    AriaSlabState old_state = slab->state;
    AriaSlabState new_state;
    
    if (slab->used_objects == 0) {
        new_state = ARIA_SLAB_EMPTY;
    } else if (slab->used_objects == slab->num_objects) {
        new_state = ARIA_SLAB_FULL;
    } else {
        new_state = ARIA_SLAB_PARTIAL;
    }
    
    if (old_state != new_state) {
        unlink_slab(cache, slab);
        slab->state = new_state;
        link_slab(cache, slab);
    }
}

/**
 * Allocate and initialize a new slab
 * 
 * Applies slab coloring to reduce cache conflicts.
 * Constructs all objects if constructor provided.
 */
static aria_slab* allocate_slab(aria_slab_cache* cache) {
    // Allocate slab header
    aria_slab* slab = (aria_slab*)malloc(sizeof(aria_slab));
    if (!slab) {
        return NULL;
    }
    
    // Allocate slab memory
    slab->memory = malloc(cache->slab_size);
    if (!slab->memory) {
        free(slab);
        return NULL;
    }
    
    // Calculate slab coloring offset
    // Rotate through cache line offsets to prevent cache conflicts
    size_t wastage = cache->slab_size % cache->object_size;
    size_t max_color = wastage / cache->cache_line_size;
    if (max_color > 0) {
        slab->color_offset = (cache->color_next % max_color) * cache->cache_line_size;
        cache->color_next++;
    } else {
        slab->color_offset = 0;
    }
    
    // Initialize slab
    slab->next = NULL;
    slab->capacity = cache->slab_size;
    slab->object_size = cache->object_size;
    slab->num_objects = (cache->slab_size - slab->color_offset) / cache->object_size;
    slab->used_objects = 0;
    slab->state = ARIA_SLAB_EMPTY;
    
    // Build freelist and construct objects
    uint8_t* mem = (uint8_t*)slab->memory + slab->color_offset;
    slab->freelist_head = NULL;
    
    for (size_t i = slab->num_objects; i > 0; i--) {
        size_t offset = (i - 1) * cache->object_size;
        void* obj = mem + offset;
        
        // Call constructor if provided
        if (cache->ctor) {
            cache->ctor(obj, cache->object_size);
        }
        
        // Add to freelist (LIFO)
        FreeNode* node = (FreeNode*)obj;
        node->next = (FreeNode*)slab->freelist_head;
        slab->freelist_head = node;
    }
    
    return slab;
}

/**
 * Free a slab and call destructors on objects
 */
static void free_slab(aria_slab_cache* cache, aria_slab* slab) {
    if (!slab) {
        return;
    }
    
    // Call destructor on all objects if provided
    if (cache->dtor && slab->memory) {
        uint8_t* mem = (uint8_t*)slab->memory + slab->color_offset;
        for (size_t i = 0; i < slab->num_objects; i++) {
            void* obj = mem + (i * cache->object_size);
            cache->dtor(obj, cache->object_size);
        }
    }
    
    free(slab->memory);
    free(slab);
}

// ============================================================================
// Lifecycle Functions
// ============================================================================

AriaSlabResult aria_slab_cache_new(
    size_t object_size,
    size_t slab_size,
    size_t alignment,
    aria_slab_ctor ctor,
    aria_slab_dtor dtor
) {
    AriaSlabResult result = {NULL, ARIA_SLAB_OK, slab_size, object_size};
    
    // Validate object size
    if (object_size == 0) {
        result.error_code = ARIA_SLAB_ERR_INVALID_OBJECT_SIZE;
        return result;
    }
    
    // Validate slab size
    if (slab_size < object_size || slab_size < MIN_SLAB_SIZE) {
        result.error_code = ARIA_SLAB_ERR_INVALID_SLAB_SIZE;
        return result;
    }
    
    // Allocate cache
    aria_slab_cache* cache = (aria_slab_cache*)malloc(sizeof(aria_slab_cache));
    if (!cache) {
        result.error_code = ARIA_SLAB_ERR_OUT_OF_MEMORY;
        return result;
    }
    
    // Initialize cache
    memset(cache, 0, sizeof(aria_slab_cache));
    cache->object_size = object_size;
    cache->slab_size = slab_size;
    cache->alignment = alignment > 0 ? alignment : DEFAULT_ALIGNMENT;
    cache->cache_line_size = DEFAULT_CACHE_LINE_SIZE;
    cache->color_next = 0;
    cache->ctor = ctor;
    cache->dtor = dtor;
    
    // Lists start empty
    cache->partial_list = NULL;
    cache->full_list = NULL;
    cache->empty_list = NULL;
    cache->active_slab = NULL;
    
    // Statistics
    cache->total_slabs = 0;
    cache->total_objects = 0;
    cache->allocated_objects = 0;
    cache->total_capacity = 0;
    
    result.cache = cache;
    return result;
}

void aria_slab_cache_destroy(aria_slab_cache* cache) {
    if (!cache) {
        return;
    }
    
    // Free all slabs in all lists
    aria_slab* slab;
    
    // Free partial list
    slab = cache->partial_list;
    while (slab) {
        aria_slab* next = slab->next;
        free_slab(cache, slab);
        slab = next;
    }
    
    // Free full list
    slab = cache->full_list;
    while (slab) {
        aria_slab* next = slab->next;
        free_slab(cache, slab);
        slab = next;
    }
    
    // Free empty list
    slab = cache->empty_list;
    while (slab) {
        aria_slab* next = slab->next;
        free_slab(cache, slab);
        slab = next;
    }
    
    // Free cache
    free(cache);
}

// ============================================================================
// Allocation Functions
// ============================================================================

AriaAllocResult aria_slab_cache_alloc(aria_slab_cache* cache) {
    AriaAllocResult result = {NULL, ARIA_ALLOC_OK, cache->object_size, cache->alignment};
    
    // FAST PATH: Try active slab first
    if (cache->active_slab && cache->active_slab->freelist_head) {
        aria_slab* slab = cache->active_slab;
        
        // Pop from freelist
        FreeNode* node = (FreeNode*)slab->freelist_head;
        slab->freelist_head = node->next;
        slab->used_objects++;
        cache->allocated_objects++;
        
        // Update state if slab became full
        update_slab_state(cache, slab);
        
        // If slab is now full, clear active_slab
        if (slab->state == ARIA_SLAB_FULL) {
            cache->active_slab = NULL;
        }
        
        result.value = node;
        return result;
    }
    
    // SLOW PATH: Need to find or allocate a slab
    aria_slab* slab = NULL;
    
    // Try to get a partial slab
    if (cache->partial_list) {
        slab = cache->partial_list;
        cache->active_slab = slab;
    }
    // Try to reuse an empty slab
    else if (cache->empty_list) {
        slab = cache->empty_list;
        cache->active_slab = slab;
    }
    // Allocate new slab
    else {
        slab = allocate_slab(cache);
        if (!slab) {
            result.error_code = ARIA_ALLOC_ERR_OUT_OF_MEMORY;
            return result;
        }
        
        // Add to empty list initially
        cache->empty_list = slab;
        cache->active_slab = slab;
        
        // Update statistics
        cache->total_slabs++;
        cache->total_objects += slab->num_objects;
        cache->total_capacity += cache->slab_size;
    }
    
    // Allocate from slab
    assert(slab->freelist_head != NULL);
    FreeNode* node = (FreeNode*)slab->freelist_head;
    slab->freelist_head = node->next;
    slab->used_objects++;
    cache->allocated_objects++;
    
    // Update state
    update_slab_state(cache, slab);
    
    // If slab is now full, clear active_slab
    if (slab->state == ARIA_SLAB_FULL) {
        cache->active_slab = NULL;
    }
    
    result.value = node;
    return result;
}

void aria_slab_cache_free(aria_slab_cache* cache, void* ptr) {
    if (!cache || !ptr) {
        return;
    }
    
    // Find owning slab
    aria_slab* slab = find_slab_for_pointer(cache, ptr);
    if (!slab) {
        // Invalid pointer (not from this cache)
        return;
    }
    
    // Push to freelist (LIFO)
    // NOTE: We do NOT call destructor - object state is preserved
    FreeNode* node = (FreeNode*)ptr;
    node->next = (FreeNode*)slab->freelist_head;
    slab->freelist_head = node;
    slab->used_objects--;
    cache->allocated_objects--;
    
    // Update state
    AriaSlabState old_state = slab->state;
    update_slab_state(cache, slab);
    
    // If slab was full and now partial, it can be used for allocation
    if (old_state == ARIA_SLAB_FULL && slab->state == ARIA_SLAB_PARTIAL) {
        // Make it active if no current active slab
        if (!cache->active_slab) {
            cache->active_slab = slab;
        }
    }
}

size_t aria_slab_cache_shrink(aria_slab_cache* cache) {
    if (!cache) {
        return 0;
    }
    
    size_t freed_count = 0;
    aria_slab* slab = cache->empty_list;
    
    while (slab) {
        aria_slab* next = slab->next;
        
        // Only free if truly empty (no used objects)
        if (slab->used_objects == 0) {
            unlink_slab(cache, slab);
            free_slab(cache, slab);
            
            freed_count++;
            cache->total_slabs--;
            cache->total_objects -= slab->num_objects;
            cache->total_capacity -= cache->slab_size;
        }
        
        slab = next;
    }
    
    return freed_count;
}

// ============================================================================
// Query Functions
// ============================================================================

void aria_slab_cache_get_stats(
    const aria_slab_cache* cache,
    size_t* total_slabs,
    size_t* total_objects,
    size_t* allocated_objects,
    size_t* free_objects,
    size_t* total_capacity
) {
    if (!cache) {
        return;
    }
    
    if (total_slabs) {
        *total_slabs = cache->total_slabs;
    }
    
    if (total_objects) {
        *total_objects = cache->total_objects;
    }
    
    if (allocated_objects) {
        *allocated_objects = cache->allocated_objects;
    }
    
    if (free_objects) {
        *free_objects = cache->total_objects - cache->allocated_objects;
    }
    
    if (total_capacity) {
        *total_capacity = cache->total_capacity;
    }
}

// ============================================================================
// Error Messages
// ============================================================================

const char* aria_slab_error_message(AriaSlabError error) {
    switch (error) {
        case ARIA_SLAB_OK:
            return "Success";
        case ARIA_SLAB_ERR_INVALID_OBJECT_SIZE:
            return "Invalid object size (must be > 0)";
        case ARIA_SLAB_ERR_INVALID_SLAB_SIZE:
            return "Invalid slab size (must be >= object_size and >= 256)";
        case ARIA_SLAB_ERR_OUT_OF_MEMORY:
            return "Out of memory";
        default:
            return "Unknown error";
    }
}

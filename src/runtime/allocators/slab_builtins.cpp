/**
 * Aria Slab Allocator - Builtin Function Wrappers Implementation
 * Phase 4.2.5.4
 */

#include "runtime/slab_builtins.h"
#include "runtime/slab.h"
#include <cstdint>
#include <cstdlib>

// Functions are declared extern "C" in the header

int64_t aria_slab_cache_new_handle(int64_t object_size, int64_t slab_size) {
    // Create slab cache without constructor/destructor
    // alignment=0 means use default (alignof(void*))
    AriaSlabResult result = aria_slab_cache_new(
        static_cast<size_t>(object_size),
        static_cast<size_t>(slab_size),
        0,      // alignment (0 = default)
        nullptr, // ctor (none)
        nullptr  // dtor (none)
    );
    
    if (result.error_code != ARIA_SLAB_OK) {
        return 0;  // Error: return null handle
    }
    
    // Return cache pointer as int64
    return reinterpret_cast<int64_t>(result.cache);
}

int64_t aria_slab_cache_alloc_handle(int64_t handle) {
    if (handle == 0) {
        return 0;  // Invalid handle
    }
    
    aria_slab_cache* cache = reinterpret_cast<aria_slab_cache*>(handle);
    AriaAllocResult result = aria_slab_cache_alloc(cache);
    
    if (result.error_code != ARIA_ALLOC_OK) {
        return 0;  // Allocation failed
    }
    
    return reinterpret_cast<int64_t>(result.value);
}

void aria_slab_cache_free_handle(int64_t handle, int64_t ptr) {
    if (handle == 0 || ptr == 0) {
        return;  // Invalid handle or pointer
    }
    
    aria_slab_cache* cache = reinterpret_cast<aria_slab_cache*>(handle);
    void* obj = reinterpret_cast<void*>(ptr);
    aria_slab_cache_free(cache, obj);
}

void aria_slab_cache_destroy_handle(int64_t handle) {
    if (handle == 0) {
        return;  // Invalid handle
    }
    
    aria_slab_cache* cache = reinterpret_cast<aria_slab_cache*>(handle);
    aria_slab_cache_destroy(cache);
}

int64_t aria_slab_cache_get_total_objects_handle(int64_t handle) {
    if (handle == 0) {
        return 0;
    }
    
    aria_slab_cache* cache = reinterpret_cast<aria_slab_cache*>(handle);
    size_t total_objects = 0;
    aria_slab_cache_get_stats(cache, nullptr, &total_objects, nullptr, nullptr, nullptr);
    return static_cast<int64_t>(total_objects);
}

int64_t aria_slab_cache_get_allocated_objects_handle(int64_t handle) {
    if (handle == 0) {
        return 0;
    }
    
    aria_slab_cache* cache = reinterpret_cast<aria_slab_cache*>(handle);
    size_t allocated_objects = 0;
    aria_slab_cache_get_stats(cache, nullptr, nullptr, &allocated_objects, nullptr, nullptr);
    return static_cast<int64_t>(allocated_objects);
}

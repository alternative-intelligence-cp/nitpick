/**
 * Aria Slab Allocator - Builtin Function Wrappers
 * Phase 4.2.5.4: Simplified API for compiler builtins
 * 
 * These functions provide a simplified interface to slab allocators
 * using opaque int64 handles instead of struct return types.
 * This allows them to be registered as compiler builtins.
 * 
 * Note: Constructor/destructor support is omitted for now (builtins don't
 * support function pointers yet). Can be added later when needed.
 */

#ifndef ARIA_RUNTIME_SLAB_BUILTINS_H
#define ARIA_RUNTIME_SLAB_BUILTINS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create new slab cache (without constructor/destructor)
 * 
 * @param object_size Size of each object in bytes
 * @param slab_size Size of each slab in bytes (typically 4096)
 * @return Slab cache handle (opaque pointer as int64) or 0 on error
 * 
 * Example:
 *   int64:cache = slab_new(64, 4096);
 *   if (cache == 0) { // error
 *   }
 */
int64_t aria_slab_cache_new_handle(int64_t object_size, int64_t slab_size);

/**
 * Allocate an object from slab cache
 * 
 * @param handle Slab cache handle from slab_new_handle()
 * @return Pointer to allocated object (as int64) or 0 on error
 * 
 * Object is pre-initialized if constructor was provided to cache creation.
 * 
 * Example:
 *   int64:ptr = slab_alloc(cache);
 *   if (ptr == 0) { // allocation failed
 *   }
 */
int64_t aria_slab_cache_alloc_handle(int64_t handle);

/**
 * Free an object back to slab cache
 * 
 * @param handle Slab cache handle
 * @param ptr Pointer to object (as int64)
 * 
 * IMPORTANT: Object is NOT destructed. State is preserved for reuse.
 * This enables fast object recycling without re-initialization.
 * 
 * Example:
 *   slab_free(cache, ptr);
 */
void aria_slab_cache_free_handle(int64_t handle, int64_t ptr);

/**
 * Destroy slab cache and free all memory
 * 
 * @param handle Slab cache handle
 * 
 * Calls destructor on all allocated objects before freeing.
 * 
 * Example:
 *   slab_destroy(cache);
 */
void aria_slab_cache_destroy_handle(int64_t handle);

/**
 * Get total number of objects in cache
 * 
 * @param handle Slab cache handle
 * @return Total objects (allocated + free)
 */
int64_t aria_slab_cache_get_total_objects_handle(int64_t handle);

/**
 * Get number of allocated objects in cache
 * 
 * @param handle Slab cache handle
 * @return Number of currently allocated objects
 */
int64_t aria_slab_cache_get_allocated_objects_handle(int64_t handle);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_SLAB_BUILTINS_H

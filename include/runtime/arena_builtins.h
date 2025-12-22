/**
 * Aria Arena Allocator - Builtin Function Wrappers
 * Phase 4.2.5.2: Simplified API for compiler builtins
 * 
 * These functions provide a simplified interface to arena allocators
 * using opaque int64 handles instead of struct return types.
 * This allows them to be registered as compiler builtins.
 */

#ifndef ARIA_RUNTIME_ARENA_BUILTINS_H
#define ARIA_RUNTIME_ARENA_BUILTINS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create new arena allocator
 * 
 * @param capacity Initial capacity hint (bytes)
 * @return Arena handle (opaque pointer as int64) or 0 on error
 * 
 * Example:
 *   int64:handle = arena_new(4096);
 *   if (handle == 0) { // error
 *   }
 */
int64_t aria_arena_new_handle(int64_t capacity);

/**
 * Allocate memory from arena
 * 
 * @param handle Arena handle from arena_new_handle()
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory (as int64) or 0 on error
 * 
 * Example:
 *   int64:ptr = arena_alloc(handle, 64);
 *   if (ptr == 0) { allocation failed }
 */
int64_t aria_arena_alloc_handle(int64_t handle, int64_t size);

/**
 * Reset arena to reuse memory
 * 
 * @param handle Arena handle
 * 
 * Invalidates all previous allocations but retains capacity.
 * Zero malloc calls in steady state.
 * 
 * Example:
 *   arena_reset(handle);
 */
void aria_arena_reset_handle(int64_t handle);

/**
 * Destroy arena and free all memory
 * 
 * @param handle Arena handle
 * 
 * Example:
 *   arena_destroy(handle);
 */
void aria_arena_destroy_handle(int64_t handle);

/**
 * Get total bytes allocated by user
 * 
 * @param handle Arena handle
 * @return Total user-requested bytes
 */
int64_t aria_arena_get_allocated_handle(int64_t handle);

/**
 * Get total bytes reserved from OS
 * 
 * @param handle Arena handle  
 * @return Total system-reserved bytes
 */
int64_t aria_arena_get_reserved_handle(int64_t handle);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_ARENA_BUILTINS_H

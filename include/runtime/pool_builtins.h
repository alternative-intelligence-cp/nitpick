/**
 * Aria Pool Allocator - Builtin Function Wrappers
 * Phase 4.2.5.3: Simplified API for compiler builtins
 * 
 * These functions provide a simplified interface to pool allocators
 * using opaque int64 handles instead of struct return types.
 * This allows them to be registered as compiler builtins.
 */

#ifndef ARIA_RUNTIME_POOL_BUILTINS_H
#define ARIA_RUNTIME_POOL_BUILTINS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create new pool allocator
 * 
 * @param block_size Fixed size of each block (must be >= 8 bytes)
 * @param initial_capacity Initial pool capacity in bytes
 * @return Pool handle (opaque pointer as int64) or 0 on error
 * 
 * Example:
 *   int64:pool = pool_new(64, 4096);
 *   if (pool == 0) { // error
 *   }
 */
int64_t npk_pool_new_handle(int64_t block_size, int64_t initial_capacity);

/**
 * Allocate a block from pool
 * 
 * @param handle Pool handle from pool_new_handle()
 * @return Pointer to allocated block (as int64) or 0 on error
 * 
 * Example:
 *   int64:ptr = pool_alloc(pool);
 *   if (ptr == 0) { // allocation failed
 *   }
 */
int64_t npk_pool_alloc_handle(int64_t handle);

/**
 * Free a block back to pool
 * 
 * @param handle Pool handle
 * @param ptr Pointer to block (as int64)
 * 
 * Example:
 *   pool_free(pool, ptr);
 */
void npk_pool_free_handle(int64_t handle, int64_t ptr);

/**
 * Reset pool to reuse all blocks
 * 
 * @param handle Pool handle
 * 
 * Rebuilds free list without deallocating chunks.
 * All previous allocations are invalidated.
 * 
 * Example:
 *   pool_reset(pool);
 */
void npk_pool_reset_handle(int64_t handle);

/**
 * Destroy pool and free all memory
 * 
 * @param handle Pool handle
 * 
 * Example:
 *   pool_destroy(pool);
 */
void npk_pool_destroy_handle(int64_t handle);

/**
 * Get total number of blocks in pool
 * 
 * @param handle Pool handle
 * @return Total blocks (used + free)
 */
int64_t npk_pool_get_total_blocks_handle(int64_t handle);

/**
 * Get number of used blocks in pool
 * 
 * @param handle Pool handle
 * @return Number of currently allocated blocks
 */
int64_t npk_pool_get_used_blocks_handle(int64_t handle);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_POOL_BUILTINS_H

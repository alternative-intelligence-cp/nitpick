/**
 * Aria Pool Allocator - Builtin Function Wrappers Implementation
 * Phase 4.2.5.3
 */

#include "runtime/pool_builtins.h"
#include "runtime/pool.h"
#include <cstdint>
#include <cstdlib>

// Functions are declared extern "C" in the header

int64_t npk_pool_new_handle(int64_t block_size, int64_t initial_capacity) {
    AriaPoolResult result = npk_pool_new(
        static_cast<size_t>(block_size),
        static_cast<size_t>(initial_capacity)
    );
    
    if (result.error_code != ARIA_POOL_OK) {
        return 0;  // Error: return null handle
    }
    
    // Return pool pointer as int64
    return reinterpret_cast<int64_t>(result.pool);
}

int64_t npk_pool_alloc_handle(int64_t handle) {
    if (handle == 0) {
        return 0;  // Invalid handle
    }
    
    npk_pool* pool = reinterpret_cast<npk_pool*>(handle);
    AriaAllocResult result = npk_pool_alloc(pool);
    
    if (result.error_code != ARIA_ALLOC_OK) {
        return 0;  // Allocation failed
    }
    
    return reinterpret_cast<int64_t>(result.value);
}

void npk_pool_free_handle(int64_t handle, int64_t ptr) {
    if (handle == 0 || ptr == 0) {
        return;  // Invalid handle or pointer
    }
    
    npk_pool* pool = reinterpret_cast<npk_pool*>(handle);
    void* block = reinterpret_cast<void*>(ptr);
    npk_pool_free(pool, block);
}

void npk_pool_reset_handle(int64_t handle) {
    if (handle == 0) {
        return;  // Invalid handle
    }
    
    npk_pool* pool = reinterpret_cast<npk_pool*>(handle);
    npk_pool_reset(pool);
}

void npk_pool_destroy_handle(int64_t handle) {
    if (handle == 0) {
        return;  // Invalid handle
    }
    
    npk_pool* pool = reinterpret_cast<npk_pool*>(handle);
    npk_pool_destroy(pool);
}

int64_t npk_pool_get_total_blocks_handle(int64_t handle) {
    if (handle == 0) {
        return 0;
    }
    
    npk_pool* pool = reinterpret_cast<npk_pool*>(handle);
    size_t total_blocks = 0;
    npk_pool_get_stats(pool, &total_blocks, nullptr, nullptr, nullptr);
    return static_cast<int64_t>(total_blocks);
}

int64_t npk_pool_get_used_blocks_handle(int64_t handle) {
    if (handle == 0) {
        return 0;
    }
    
    npk_pool* pool = reinterpret_cast<npk_pool*>(handle);
    size_t used_blocks = 0;
    npk_pool_get_stats(pool, nullptr, &used_blocks, nullptr, nullptr);
    return static_cast<int64_t>(used_blocks);
}

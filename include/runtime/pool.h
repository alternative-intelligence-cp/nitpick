/**
 * Pool Allocator - Phase 4.2 Stage 4
 * 
 * Fixed-size block allocator using intrusive LIFO free list.
 * Target: <20ns allocation, <15ns deallocation
 * 
 * Based on Gemini research: Stack-based pool with zero per-allocation overhead.
 * Objects are allocated from a fixed-size pool using pointer bump or free list reuse.
 * 
 * Key Features:
 * - Intrusive linked list (metadata stored in free blocks)
 * - LIFO strategy for cache locality
 * - Geometric growth (doubling) when capacity exhausted
 * - Zero bytes overhead per allocation
 * - Type-agnostic (operates on block_size)
 * 
 * Thread Safety: NOT thread-safe (use ThreadSafe wrapper if needed)
 */

#ifndef ARIA_RUNTIME_POOL_H
#define ARIA_RUNTIME_POOL_H

#include "result.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct npk_pool npk_pool;
typedef struct npk_pool_chunk npk_pool_chunk;

// ============================================================================
// Error Codes
// ============================================================================

/**
 * Pool allocator error codes
 */
typedef enum {
    ARIA_POOL_OK = 0,
    ARIA_POOL_ERR_INVALID_BLOCK_SIZE,    // block_size < sizeof(void*) (8 bytes)
    ARIA_POOL_ERR_INVALID_CAPACITY,      // initial_capacity == 0
    ARIA_POOL_ERR_OUT_OF_MEMORY          // System malloc failed
} AriaPoolError;

// ============================================================================
// Result Types
// ============================================================================

/**
 * Result type for pool creation
 */
typedef struct {
    npk_pool* pool;               // Pool handle (NULL on error)
    AriaPoolError error_code;      // Error code
    size_t requested_capacity;     // Requested initial capacity
    size_t block_size;             // Fixed block size
} AriaPoolResult;

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Pool chunk - contiguous memory region divided into fixed-size blocks
 * 
 * Chunks are chained in a singly-linked list for geometric growth.
 * Each chunk manages (capacity / block_size) blocks.
 */
struct npk_pool_chunk {
    npk_pool_chunk* next;         // Next chunk in chain
    void* memory;                  // Allocated memory region
    size_t capacity;               // Total bytes in this chunk
};

/**
 * Pool allocator - fixed-size block manager
 * 
 * Uses intrusive LIFO free list:
 * - Free blocks store a pointer to the next free block in their first 8 bytes
 * - Allocated blocks contain user data only (zero overhead)
 * - LIFO provides excellent cache locality (recently freed blocks are hot)
 * 
 * Growth strategy:
 * - Chunks grow geometrically (double each time) starting from initial_capacity
 * - First chunk: initial_capacity
 * - Second chunk: initial_capacity * 2
 * - Third chunk: initial_capacity * 4
 * - etc.
 */
struct npk_pool {
    // Free list head (intrusive LIFO stack)
    void* head;                    // Points to first free block (NULL if exhausted)
    
    // Chunk management
    npk_pool_chunk* chunks;       // Head of chunk list
    npk_pool_chunk* current_chunk;// Chunk currently being allocated from
    size_t current_offset;         // Offset within current chunk
    
    // Configuration
    size_t block_size;             // Fixed size of each block (bytes)
    size_t initial_capacity;       // Size of first chunk (bytes)
    size_t growth_factor;          // Multiplier for geometric growth (typically 2)
    
    // Statistics
    size_t total_blocks;           // Total blocks across all chunks
    size_t used_blocks;            // Blocks currently allocated
    size_t total_capacity;         // Total bytes reserved from system
};

// ============================================================================
// Lifecycle Functions
// ============================================================================

/**
 * Create a new pool allocator
 * 
 * @param block_size Fixed size of each allocation (must be >= sizeof(void*) = 8)
 * @param initial_capacity Initial chunk size (must be > 0)
 * @return AriaPoolResult with pool handle or error
 * 
 * Example:
 *   AriaPoolResult result = npk_pool_new(64, 4096);  // 64-byte blocks, 4KB initial
 *   if (npk_pool_result_is_ok(result)) {
 *       npk_pool* pool = result.pool;
 *       // Use pool...
 *       npk_pool_destroy(pool);
 *   }
 */
AriaPoolResult npk_pool_new(size_t block_size, size_t initial_capacity);

/**
 * Create a pool with custom growth factor
 * 
 * @param block_size Fixed size of each allocation
 * @param initial_capacity Initial chunk size
 * @param growth_factor Multiplier for geometric growth (e.g., 2 for doubling)
 * @return AriaPoolResult with pool handle or error
 */
AriaPoolResult npk_pool_new_with_growth(size_t block_size, size_t initial_capacity, size_t growth_factor);

/**
 * Destroy pool and free all chunks
 * 
 * Walks the chunk list and frees each chunk's memory.
 * Does NOT call destructors on allocated objects (user must ensure cleanup).
 * 
 * @param pool Pool to destroy (NULL-safe)
 * 
 * Complexity: O(num_chunks) typically < 10 chunks
 */
void npk_pool_destroy(npk_pool* pool);

// ============================================================================
// Allocation Functions
// ============================================================================

/**
 * Allocate a block from the pool
 * 
 * HOT PATH (free list non-empty): ~2-5ns
 *   1. Load head pointer
 *   2. Read next pointer from head block (intrusive list)
 *   3. Update head to next
 *   4. Return original head
 * 
 * COLD PATH (free list empty): ~100ns
 *   1. Check if current chunk has space
 *   2. If yes: bump pointer allocation
 *   3. If no: allocate new chunk (geometric growth)
 * 
 * @param pool Pool allocator
 * @return AriaAllocResult with pointer or error
 * 
 * Error Conditions:
 * - ARIA_ALLOC_ERR_OUT_OF_MEMORY: System malloc failed during growth
 * - Size/alignment in result are always (block_size, alignof(void*))
 */
AriaAllocResult npk_pool_alloc(npk_pool* pool);

/**
 * Free a block back to the pool
 * 
 * LIFO insertion: Block becomes new head of free list.
 * The block's first 8 bytes are overwritten with the old head pointer.
 * 
 * Performance: ~2-5ns (single pointer store)
 * 
 * @param pool Pool allocator
 * @param ptr Pointer to free (must have been allocated from this pool)
 * 
 * SAFETY: Freeing a pointer from a different pool or allocator causes
 * undefined behavior. Use type-safe Aria wrappers to prevent this.
 * 
 * Complexity: O(1)
 */
void npk_pool_free(npk_pool* pool, void* ptr);

/**
 * Reset pool (free all blocks without deallocating chunks)
 * 
 * Rebuilds the free list by threading through all blocks in all chunks.
 * This is O(num_blocks) but avoids calling free() on system memory.
 * 
 * Use case: Server loop pattern (allocate per request, reset between requests)
 * 
 * @param pool Pool allocator
 * 
 * Complexity: O(total_blocks)
 * Performance: ~1-2ns per block (pure pointer writes)
 */
void npk_pool_reset(npk_pool* pool);

// ============================================================================
// Query Functions
// ============================================================================

/**
 * Get pool statistics
 * 
 * @param pool Pool allocator
 * @param total_blocks Out: Total blocks across all chunks (or NULL)
 * @param used_blocks Out: Blocks currently allocated (or NULL)
 * @param free_blocks Out: Blocks available (or NULL)
 * @param total_capacity Out: Total bytes reserved from system (or NULL)
 */
void npk_pool_get_stats(
    const npk_pool* pool,
    size_t* total_blocks,
    size_t* used_blocks,
    size_t* free_blocks,
    size_t* total_capacity
);

/**
 * Get number of free blocks without allocation
 * 
 * Note: This is O(n) as it walks the free list. Use sparingly.
 * 
 * @param pool Pool allocator
 * @return Number of blocks in free list
 */
size_t npk_pool_free_count(const npk_pool* pool);

/**
 * Get pool block size
 * 
 * @param pool Pool allocator
 * @return Fixed block size (bytes)
 */
static inline size_t npk_pool_block_size(const npk_pool* pool) {
    return pool->block_size;
}

// ============================================================================
// Result Helper Functions
// ============================================================================

/**
 * Check if pool creation succeeded
 */
static inline bool npk_pool_result_is_ok(AriaPoolResult result) {
    return result.error_code == ARIA_POOL_OK && result.pool != NULL;
}

/**
 * Check if pool creation failed
 */
static inline bool npk_pool_result_is_err(AriaPoolResult result) {
    return result.error_code != ARIA_POOL_OK || result.pool == NULL;
}

/**
 * Get error message for pool error code
 * 
 * @param error Error code
 * @return Human-readable error message
 */
const char* npk_pool_error_message(AriaPoolError error);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_POOL_H

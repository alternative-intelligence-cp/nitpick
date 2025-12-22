/**
 * Pool Allocator Implementation - Phase 4.2 Stage 4
 * 
 * Stack-based fixed-size block allocator with intrusive LIFO free list.
 * See header for architecture details.
 */

#include "runtime/pool.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// ============================================================================
// Constants
// ============================================================================

#define MIN_BLOCK_SIZE sizeof(void*)    // Minimum block size (8 bytes on 64-bit)
#define DEFAULT_GROWTH_FACTOR 2         // Double chunk size each growth

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Free list node structure (intrusive)
 * When a block is free, its first 8 bytes contain a pointer to the next free block.
 */
typedef struct FreeNode {
    struct FreeNode* next;
} FreeNode;

/**
 * Allocate a new chunk of memory
 * 
 * @param capacity Size in bytes
 * @return Pointer to chunk or NULL on failure
 */
static aria_pool_chunk* allocate_chunk(size_t capacity) {
    // Allocate chunk header
    aria_pool_chunk* chunk = (aria_pool_chunk*)malloc(sizeof(aria_pool_chunk));
    if (!chunk) {
        return NULL;
    }
    
    // Allocate chunk memory
    chunk->memory = malloc(capacity);
    if (!chunk->memory) {
        free(chunk);
        return NULL;
    }
    
    chunk->next = NULL;
    chunk->capacity = capacity;
    return chunk;
}

/**
 * Free a chunk and its memory
 */
static void free_chunk(aria_pool_chunk* chunk) {
    if (chunk) {
        free(chunk->memory);
        free(chunk);
    }
}

/**
 * Thread blocks in a chunk into the free list
 * 
 * Connects blocks in a LIFO chain: Block[N] -> Block[N-1] -> ... -> Block[0] -> tail
 * 
 * @param chunk Chunk to thread
 * @param block_size Size of each block
 * @param tail Existing free list to append to
 * @return New head of free list
 */
static FreeNode* thread_chunk(aria_pool_chunk* chunk, size_t block_size, FreeNode* tail) {
    uint8_t* mem = (uint8_t*)chunk->memory;
    size_t num_blocks = chunk->capacity / block_size;
    
    FreeNode* head = tail;
    
    // Thread blocks in reverse order (LIFO construction)
    for (size_t i = num_blocks; i > 0; i--) {
        size_t offset = (i - 1) * block_size;
        FreeNode* node = (FreeNode*)(mem + offset);
        node->next = head;
        head = node;
    }
    
    return head;
}

/**
 * Grow the pool by adding a new chunk
 * 
 * Uses geometric progression: new_chunk_size = last_chunk_size * growth_factor
 * 
 * @param pool Pool to grow
 * @return true on success, false on OOM
 */
static bool grow_pool(aria_pool* pool) {
    // Calculate next chunk size (geometric growth)
    size_t last_chunk_capacity = pool->current_chunk ? pool->current_chunk->capacity : pool->initial_capacity;
    size_t new_capacity = last_chunk_capacity * pool->growth_factor;
    
    // Allocate new chunk
    aria_pool_chunk* new_chunk = allocate_chunk(new_capacity);
    if (!new_chunk) {
        return false;
    }
    
    // Add to chunk list
    new_chunk->next = pool->chunks;
    pool->chunks = new_chunk;
    pool->current_chunk = new_chunk;
    pool->current_offset = 0;
    
    // Update statistics
    size_t num_blocks = new_capacity / pool->block_size;
    pool->total_blocks += num_blocks;
    pool->total_capacity += new_capacity;
    
    // Thread new chunk into free list
    pool->head = thread_chunk(new_chunk, pool->block_size, (FreeNode*)pool->head);
    
    return true;
}

// ============================================================================
// Lifecycle Functions
// ============================================================================

AriaPoolResult aria_pool_new_with_growth(size_t block_size, size_t initial_capacity, size_t growth_factor) {
    AriaPoolResult result = {NULL, ARIA_POOL_OK, initial_capacity, block_size};
    
    // Validate block size (must hold a pointer)
    if (block_size < MIN_BLOCK_SIZE) {
        result.error_code = ARIA_POOL_ERR_INVALID_BLOCK_SIZE;
        return result;
    }
    
    // Validate initial capacity
    if (initial_capacity == 0) {
        result.error_code = ARIA_POOL_ERR_INVALID_CAPACITY;
        return result;
    }
    
    // Allocate pool handle
    aria_pool* pool = (aria_pool*)malloc(sizeof(aria_pool));
    if (!pool) {
        result.error_code = ARIA_POOL_ERR_OUT_OF_MEMORY;
        return result;
    }
    
    // Initialize pool
    memset(pool, 0, sizeof(aria_pool));
    pool->block_size = block_size;
    pool->initial_capacity = initial_capacity;
    pool->growth_factor = growth_factor;
    pool->head = NULL;
    
    // Allocate first chunk
    aria_pool_chunk* first_chunk = allocate_chunk(initial_capacity);
    if (!first_chunk) {
        free(pool);
        result.error_code = ARIA_POOL_ERR_OUT_OF_MEMORY;
        return result;
    }
    
    // Set up chunk
    pool->chunks = first_chunk;
    pool->current_chunk = first_chunk;
    pool->current_offset = 0;
    
    // Calculate blocks
    size_t num_blocks = initial_capacity / block_size;
    pool->total_blocks = num_blocks;
    pool->used_blocks = 0;
    pool->total_capacity = initial_capacity;
    
    // Thread initial chunk into free list
    pool->head = thread_chunk(first_chunk, block_size, NULL);
    
    result.pool = pool;
    return result;
}

AriaPoolResult aria_pool_new(size_t block_size, size_t initial_capacity) {
    return aria_pool_new_with_growth(block_size, initial_capacity, DEFAULT_GROWTH_FACTOR);
}

void aria_pool_destroy(aria_pool* pool) {
    if (!pool) {
        return;
    }
    
    // Free all chunks
    aria_pool_chunk* chunk = pool->chunks;
    while (chunk) {
        aria_pool_chunk* next = chunk->next;
        free_chunk(chunk);
        chunk = next;
    }
    
    // Free pool handle
    free(pool);
}

// ============================================================================
// Allocation Functions
// ============================================================================

AriaAllocResult aria_pool_alloc(aria_pool* pool) {
    AriaAllocResult result = {NULL, ARIA_ALLOC_OK, pool->block_size, sizeof(void*)};
    
    // HOT PATH: Try free list first
    if (pool->head) {
        // Pop from free list (LIFO)
        FreeNode* node = (FreeNode*)pool->head;
        pool->head = node->next;
        pool->used_blocks++;
        
        result.value = node;
        return result;
    }
    
    // COLD PATH: Free list empty, try bump allocation from current chunk
    if (pool->current_chunk) {
        size_t remaining = pool->current_chunk->capacity - pool->current_offset;
        
        if (remaining >= pool->block_size) {
            // Bump pointer allocation
            uint8_t* mem = (uint8_t*)pool->current_chunk->memory;
            void* ptr = mem + pool->current_offset;
            pool->current_offset += pool->block_size;
            pool->used_blocks++;
            
            result.value = ptr;
            return result;
        }
    }
    
    // COLD PATH: Need to grow pool
    if (!grow_pool(pool)) {
        result.error_code = ARIA_ALLOC_ERR_OUT_OF_MEMORY;
        return result;
    }
    
    // Retry allocation (should succeed now)
    assert(pool->head != NULL);
    FreeNode* node = (FreeNode*)pool->head;
    pool->head = node->next;
    pool->used_blocks++;
    
    result.value = node;
    return result;
}

void aria_pool_free(aria_pool* pool, void* ptr) {
    if (!pool || !ptr) {
        return;
    }
    
    // LIFO insertion: Make this block the new head
    FreeNode* node = (FreeNode*)ptr;
    node->next = (FreeNode*)pool->head;
    pool->head = node;
    pool->used_blocks--;
}

void aria_pool_reset(aria_pool* pool) {
    if (!pool) {
        return;
    }
    
    // Rebuild free list by threading all chunks
    pool->head = NULL;
    
    aria_pool_chunk* chunk = pool->chunks;
    while (chunk) {
        pool->head = thread_chunk(chunk, pool->block_size, (FreeNode*)pool->head);
        chunk = chunk->next;
    }
    
    // Reset statistics
    pool->used_blocks = 0;
    pool->current_chunk = pool->chunks;
    pool->current_offset = pool->chunks ? pool->chunks->capacity : 0;
}

// ============================================================================
// Query Functions
// ============================================================================

void aria_pool_get_stats(
    const aria_pool* pool,
    size_t* total_blocks,
    size_t* used_blocks,
    size_t* free_blocks,
    size_t* total_capacity
) {
    if (!pool) {
        return;
    }
    
    if (total_blocks) {
        *total_blocks = pool->total_blocks;
    }
    
    if (used_blocks) {
        *used_blocks = pool->used_blocks;
    }
    
    if (free_blocks) {
        *free_blocks = pool->total_blocks - pool->used_blocks;
    }
    
    if (total_capacity) {
        *total_capacity = pool->total_capacity;
    }
}

size_t aria_pool_free_count(const aria_pool* pool) {
    if (!pool) {
        return 0;
    }
    
    size_t count = 0;
    FreeNode* node = (FreeNode*)pool->head;
    
    while (node) {
        count++;
        node = node->next;
    }
    
    return count;
}

// ============================================================================
// Error Messages
// ============================================================================

const char* aria_pool_error_message(AriaPoolError error) {
    switch (error) {
        case ARIA_POOL_OK:
            return "Success";
        case ARIA_POOL_ERR_INVALID_BLOCK_SIZE:
            return "Invalid block size (must be >= 8 bytes)";
        case ARIA_POOL_ERR_INVALID_CAPACITY:
            return "Invalid capacity (must be > 0)";
        case ARIA_POOL_ERR_OUT_OF_MEMORY:
            return "Out of memory";
        default:
            return "Unknown error";
    }
}

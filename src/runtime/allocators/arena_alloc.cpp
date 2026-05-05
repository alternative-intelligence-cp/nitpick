/**
 * Aria Arena Allocator Implementation
 * 
 * Fixed-size block chaining with retain-capacity reset.
 */

#include "runtime/arena.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>

// ============================================================================
// Constants
// ============================================================================

// Default block size (4KB - good cache locality, low overhead)
static const size_t ARIA_ARENA_DEFAULT_BLOCK_SIZE = 4096;

// Minimum block size (prevent tiny blocks)
static const size_t ARIA_ARENA_MIN_BLOCK_SIZE = 256;

// Large allocation threshold (allocations > block_size/2 get dedicated blocks)
static const size_t ARIA_ARENA_LARGE_THRESHOLD_DIVISOR = 2;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Check if value is power of 2
 */
static inline bool is_power_of_2(size_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

/**
 * Align pointer forward to alignment boundary
 * Formula: (ptr + align - 1) & ~(align - 1)
 */
static inline size_t align_forward(size_t ptr, size_t align) {
    if (align == 0 || align == 1) {
        return ptr;
    }
    return (ptr + align - 1) & ~(align - 1);
}

/**
 * Allocate a new arena block
 */
static npk_arena_block* allocate_block(size_t capacity) {
    // Allocate block header
    npk_arena_block* block = (npk_arena_block*)std::malloc(sizeof(npk_arena_block));
    if (!block) {
        return nullptr;
    }
    
    // Allocate memory region
    block->memory = (uint8_t*)std::malloc(capacity);
    if (!block->memory) {
        std::free(block);
        return nullptr;
    }
    
    block->next = nullptr;
    block->capacity = capacity;
    
    return block;
}

/**
 * Free a single block
 */
static void free_block(npk_arena_block* block) {
    if (!block) {
        return;
    }
    
    std::free(block->memory);
    std::free(block);
}

// ============================================================================
// Arena Lifecycle Functions
// ============================================================================

AriaArenaResult npk_arena_new(size_t initial_capacity) {
    return npk_arena_new_with_block_size(initial_capacity, ARIA_ARENA_DEFAULT_BLOCK_SIZE);
}

AriaArenaResult npk_arena_new_with_block_size(size_t initial_capacity, size_t block_size) {
    AriaArenaResult result;
    result.arena = nullptr;
    result.requested_capacity = initial_capacity;
    
    // Validate block size
    if (block_size < ARIA_ARENA_MIN_BLOCK_SIZE) {
        result.error_code = ARIA_ARENA_ERR_INVALID_CAPACITY;
        return result;
    }
    
    // Allocate arena handle
    npk_arena* arena = (npk_arena*)std::malloc(sizeof(npk_arena));
    if (!arena) {
        result.error_code = ARIA_ARENA_ERR_OUT_OF_MEMORY;
        return result;
    }
    
    // Determine initial block size (at least block_size, or initial_capacity if larger)
    size_t first_block_size = std::max(block_size, initial_capacity);
    
    // Allocate first block
    npk_arena_block* first_block = allocate_block(first_block_size);
    if (!first_block) {
        std::free(arena);
        result.error_code = ARIA_ARENA_ERR_OUT_OF_MEMORY;
        return result;
    }
    
    // Initialize arena state
    arena->head = first_block;
    arena->current = first_block;
    arena->current_offset = 0;
    arena->default_block_size = block_size;
    arena->total_allocated_user = 0;
    arena->total_reserved_sys = first_block_size;
    arena->num_blocks = 1;
    
    // Success
    result.arena = arena;
    result.error_code = ARIA_ARENA_OK;
    return result;
}

void npk_arena_reset(npk_arena* arena) {
    if (!arena) {
        return;
    }
    
    // ARIA-BUGFIX: CRIT-4 - Don't retain all memory forever
    // Use automatic shrink policy with reasonable default (16 MB)
    // This prevents long-running AGI from accumulating memory after spikes
    // 
    // Default retention: 16 MB (enough for most workloads, not excessive)
    // - Small embedded systems: Won't OOM from spikes
    // - Desktop/server: Trivial amount of RAM
    // - Can override with npk_arena_reset_limit() for custom policy
    const size_t DEFAULT_RETENTION_BYTES = 16 * 1024 * 1024;  // 16 MB
    
    // Delegate to limit-based reset
    npk_arena_reset_limit(arena, DEFAULT_RETENTION_BYTES);
}

void npk_arena_reset_limit(npk_arena* arena, size_t max_retain) {
    if (!arena) {
        return;
    }
    
    // Reset pointers first
    arena->current = arena->head;
    arena->current_offset = 0;
    arena->total_allocated_user = 0;
    
    // Walk chain and free blocks beyond max_retain
    size_t bytes_retained = 0;
    npk_arena_block* block = arena->head;
    npk_arena_block* prev = nullptr;
    
    while (block) {
        if (bytes_retained >= max_retain && prev) {
            // Free this block and all subsequent blocks
            npk_arena_block* to_free = block;
            while (to_free) {
                npk_arena_block* next = to_free->next;
                free_block(to_free);
                arena->num_blocks--;
                arena->total_reserved_sys -= to_free->capacity;
                to_free = next;
            }
            
            // Terminate chain
            prev->next = nullptr;
            break;
        }
        
        bytes_retained += block->capacity;
        prev = block;
        block = block->next;
    }
}

void npk_arena_destroy(npk_arena* arena) {
    if (!arena) {
        return;
    }
    
    // Walk chain and free all blocks
    npk_arena_block* block = arena->head;
    while (block) {
        npk_arena_block* next = block->next;
        free_block(block);
        block = next;
    }
    
    // Free arena handle
    std::free(arena);
}

// ============================================================================
// Arena Allocation Functions
// ============================================================================

AriaAllocResult npk_arena_alloc(npk_arena* arena, size_t size, size_t align) {
    // Validate inputs
    if (!arena) {
        return npk_alloc_result_err(ARIA_ALLOC_ERR_INVALID_SIZE, size, align);
    }
    
    if (size == 0) {
        return npk_alloc_result_err(ARIA_ALLOC_ERR_INVALID_SIZE, size, align);
    }
    
    // Default alignment to 8 bytes if not specified
    if (align == 0) {
        align = 8;
    }
    
    // Validate alignment
    if (!is_power_of_2(align)) {
        return npk_alloc_result_err(ARIA_ALLOC_ERR_INVALID_ALIGNMENT, size, align);
    }
    
    // Calculate aligned offset in current block
    size_t aligned_offset = align_forward(arena->current_offset, align);
    
    // Check if current block has capacity (HOT PATH)
    if (aligned_offset + size <= arena->current->capacity) {
        // Fast path - allocate from current block
        void* ptr = arena->current->memory + aligned_offset;
        arena->current_offset = aligned_offset + size;
        arena->total_allocated_user += size;
        
        return npk_alloc_result_ok(ptr, size, align);
    }
    
    // COLD PATH - current block is full
    
    // Large allocation optimization: dedicated block for size > block_size/2
    if (size > arena->default_block_size / ARIA_ARENA_LARGE_THRESHOLD_DIVISOR) {
        // Allocate dedicated block
        npk_arena_block* large_block = allocate_block(size + align);
        if (!large_block) {
            return npk_alloc_result_err(ARIA_ALLOC_ERR_OUT_OF_MEMORY, size, align);
        }
        
        // Insert after current block (don't advance current)
        large_block->next = arena->current->next;
        arena->current->next = large_block;
        
        // Update statistics
        arena->num_blocks++;
        arena->total_reserved_sys += large_block->capacity;
        arena->total_allocated_user += size;
        
        // Return aligned pointer from large block
        size_t large_aligned = align_forward(0, align);
        return npk_alloc_result_ok(large_block->memory + large_aligned, size, align);
    }
    
    // Standard growth: check for existing next block (reuse from previous cycle)
    if (arena->current->next) {
        // Reuse existing block (retain capacity optimization)
        arena->current = arena->current->next;
        arena->current_offset = 0;
    } else {
        // Allocate new block
        npk_arena_block* new_block = allocate_block(arena->default_block_size);
        if (!new_block) {
            return npk_alloc_result_err(ARIA_ALLOC_ERR_OUT_OF_MEMORY, size, align);
        }
        
        // Link new block
        arena->current->next = new_block;
        arena->current = new_block;
        arena->current_offset = 0;
        
        // Update statistics
        arena->num_blocks++;
        arena->total_reserved_sys += arena->default_block_size;
    }
    
    // Recalculate alignment in new block
    aligned_offset = align_forward(arena->current_offset, align);
    
    // Sanity check (should always fit in new block)
    if (aligned_offset + size > arena->current->capacity) {
        // This should never happen unless default_block_size is too small
        return npk_alloc_result_err(ARIA_ALLOC_ERR_OUT_OF_MEMORY, size, align);
    }
    
    // Allocate from new block
    void* ptr = arena->current->memory + aligned_offset;
    arena->current_offset = aligned_offset + size;
    arena->total_allocated_user += size;
    
    return npk_alloc_result_ok(ptr, size, align);
}

AriaAllocResult npk_arena_alloc_array(npk_arena* arena, size_t elem_size, size_t count, size_t align) {
    // Check for overflow
    if (elem_size == 0 || count == 0) {
        return npk_alloc_result_err(ARIA_ALLOC_ERR_INVALID_SIZE, elem_size * count, align);
    }
    
    if (count > SIZE_MAX / elem_size) {
        return npk_alloc_result_err(ARIA_ALLOC_ERR_SIZE_OVERFLOW, elem_size * count, align);
    }
    
    size_t total_size = elem_size * count;
    return npk_arena_alloc(arena, total_size, align);
}

AriaAllocResult npk_arena_alloc_zeroed(npk_arena* arena, size_t size, size_t align) {
    AriaAllocResult result = npk_arena_alloc(arena, size, align);
    
    if (npk_alloc_result_is_ok(result)) {
        std::memset(result.value, 0, size);
    }
    
    return result;
}

// ============================================================================
// Arena Query Functions
// ============================================================================

void npk_arena_get_stats(const npk_arena* arena, 
                          size_t* total_allocated_user,
                          size_t* total_reserved_sys,
                          size_t* num_blocks) {
    if (!arena) {
        return;
    }
    
    if (total_allocated_user) {
        *total_allocated_user = arena->total_allocated_user;
    }
    
    if (total_reserved_sys) {
        *total_reserved_sys = arena->total_reserved_sys;
    }
    
    if (num_blocks) {
        *num_blocks = arena->num_blocks;
    }
}

size_t npk_arena_remaining_capacity(const npk_arena* arena) {
    if (!arena || !arena->current) {
        return 0;
    }
    
    // Capacity - current offset
    if (arena->current_offset >= arena->current->capacity) {
        return 0;
    }
    
    return arena->current->capacity - arena->current_offset;
}

const char* npk_arena_error_message(AriaArenaError error) {
    switch (error) {
        case ARIA_ARENA_OK:
            return "Success";
        case ARIA_ARENA_ERR_INVALID_CAPACITY:
            return "Invalid capacity (must be >= 256 bytes)";
        case ARIA_ARENA_ERR_OUT_OF_MEMORY:
            return "Out of memory";
        default:
            return "Unknown error";
    }
}

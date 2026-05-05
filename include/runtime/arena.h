/**
 * Aria Arena Allocator - Phase 4.2 Stage 3
 * 
 * High-performance region-based memory allocator with:
 * - Pointer bump allocation (<2ns hot path)
 * - Fixed-size block chaining (4KB blocks)
 * - Retain capacity reset (zero malloc in steady state)
 * - Result-based error handling
 * 
 * Based on research from:
 * - Rust bumpalo (geometric growth, backward linking)
 * - Zig ArenaAllocator (retain capacity reset)
 * - LLVM BumpPtrAllocator (custom-sized slabs for large objects)
 * 
 * Reference: docs/gemini/responses/alloc_001_arena_allocator_patterns.txt
 */

#ifndef ARIA_RUNTIME_ARENA_H
#define ARIA_RUNTIME_ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "result.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Arena Data Structures
// ============================================================================

/**
 * Arena memory block node
 * 
 * Linked list node containing a memory region for bump allocation.
 * Blocks are typically 4KB but can be custom-sized for large allocations.
 */
typedef struct npk_arena_block {
    struct npk_arena_block* next;  // Next block in chain (NULL = end)
    uint8_t* memory;                // Pointer to allocatable memory
    size_t capacity;                // Total usable size of this block
} npk_arena_block;

/**
 * Arena allocator handle
 * 
 * Manages a chain of blocks with pointer-bump allocation.
 * Thread-local by design (not thread-safe).
 */
typedef struct npk_arena {
    npk_arena_block* head;         // First block (kept for reset)
    npk_arena_block* current;      // Current active block
    size_t current_offset;          // Offset into current block
    size_t default_block_size;      // Size for new blocks (typically 4096)
    
    // Statistics
    size_t total_allocated_user;    // Bytes requested by user
    size_t total_reserved_sys;      // Bytes allocated from OS
    size_t num_blocks;              // Total blocks in chain
} npk_arena;

/**
 * Arena creation error codes
 */
typedef enum {
    ARIA_ARENA_OK = 0,
    ARIA_ARENA_ERR_INVALID_CAPACITY = 1,
    ARIA_ARENA_ERR_OUT_OF_MEMORY = 2
} AriaArenaError;

/**
 * Arena creation result
 * 
 * Contains either a valid arena pointer or error details.
 */
typedef struct {
    npk_arena* arena;              // Arena handle (NULL if error)
    AriaArenaError error_code;      // Error code (0 = success)
    size_t requested_capacity;      // Capacity parameter (diagnostic)
} AriaArenaResult;

// ============================================================================
// Arena Lifecycle Functions
// ============================================================================

/**
 * Create a new arena with default block size
 * 
 * @param initial_capacity Suggested initial capacity (rounded to block size)
 * @return Result containing arena or error
 * 
 * The initial capacity is a hint; the arena will allocate at least one block
 * of default_block_size (4KB). If initial_capacity > 4KB, a larger first
 * block is allocated.
 * 
 * Example:
 *   AriaArenaResult res = npk_arena_new(8192);
 *   if (npk_arena_result_is_ok(res)) {
 *       npk_arena* arena = res.arena;
 *       // Use arena...
 *       npk_arena_destroy(arena);
 *   }
 */
AriaArenaResult npk_arena_new(size_t initial_capacity);

/**
 * Create arena with custom block size
 * 
 * @param initial_capacity Initial capacity hint
 * @param block_size Size for subsequent blocks (must be >= 256 bytes)
 * @return Result containing arena or error
 * 
 * Advanced use: Tune block size for specific workloads.
 * - Small blocks (1KB): Better for cache locality, more overhead
 * - Large blocks (64KB): Less overhead, potential waste
 * - Default (4KB): Good balance for most workloads
 */
AriaArenaResult npk_arena_new_with_block_size(size_t initial_capacity, size_t block_size);

/**
 * Reset arena for reuse (retain capacity)
 * 
 * @param arena Arena to reset
 * 
 * Resets allocation pointers to the beginning but KEEPS all allocated blocks.
 * Next allocations will reuse existing blocks without calling malloc.
 * 
 * This is the KEY optimization for server loops:
 *   while (handle_request()) {
 *       npk_arena_reset(arena);  // Zero malloc calls!
 *       // Process request using arena...
 *   }
 * 
 * WARNING: All pointers allocated from this arena are INVALIDATED.
 * The Aria borrow checker should treat this as a mutable borrow that
 * invalidates all previous loans.
 */
void npk_arena_reset(npk_arena* arena);

/**
 * Reset arena and free excess capacity
 * 
 * @param arena Arena to reset
 * @param max_retain Maximum bytes to retain (0 = free all)
 * 
 * Like reset() but frees blocks beyond max_retain bytes.
 * Useful to prevent unbounded growth in long-running servers.
 * 
 * Example:
 *   // Keep first 64KB, free the rest
 *   npk_arena_reset_limit(arena, 65536);
 */
void npk_arena_reset_limit(npk_arena* arena, size_t max_retain);

/**
 * Destroy arena and free all memory
 * 
 * @param arena Arena to destroy (can be NULL)
 * 
 * Walks the block chain and frees all memory to the OS.
 * Complexity: O(num_blocks)
 * 
 * After this call, the arena pointer is invalid.
 */
void npk_arena_destroy(npk_arena* arena);

// ============================================================================
// Arena Allocation Functions
// ============================================================================

/**
 * Allocate memory from arena with result
 * 
 * @param arena Arena to allocate from
 * @param size Number of bytes to allocate
 * @param align Alignment requirement (must be power of 2, 0 = default)
 * @return Result containing pointer or error
 * 
 * Hot path (block has space): ~2ns
 * Cold path (new block needed): ~100ns (malloc overhead)
 * 
 * Large allocation optimization: If size > block_size/2, allocates
 * a dedicated block to avoid wasting standard blocks.
 * 
 * Example:
 *   AriaAllocResult res = npk_arena_alloc(arena, 64, 8);
 *   if (npk_alloc_result_is_ok(res)) {
 *       void* ptr = res.value;
 *       // Use ptr (valid until arena reset/destroy)
 *   }
 */
AriaAllocResult npk_arena_alloc(npk_arena* arena, size_t size, size_t align);

/**
 * Allocate array from arena with result
 * 
 * @param arena Arena to allocate from
 * @param elem_size Size of each element
 * @param count Number of elements
 * @param align Alignment requirement
 * @return Result containing pointer or error
 * 
 * Includes overflow protection (elem_size * count).
 * 
 * Example:
 *   // Allocate 100 int64_t with 8-byte alignment
 *   AriaAllocResult res = npk_arena_alloc_array(arena, 8, 100, 8);
 */
AriaAllocResult npk_arena_alloc_array(npk_arena* arena, size_t elem_size, size_t count, size_t align);

/**
 * Allocate zeroed memory from arena
 * 
 * @param arena Arena to allocate from
 * @param size Number of bytes
 * @param align Alignment requirement
 * @return Result containing zeroed pointer or error
 * 
 * Like npk_arena_alloc but guarantees zero-initialization.
 */
AriaAllocResult npk_arena_alloc_zeroed(npk_arena* arena, size_t size, size_t align);

// ============================================================================
// Arena Query Functions
// ============================================================================

/**
 * Get arena statistics
 * 
 * @param arena Arena to query
 * @param total_allocated_user Out: Bytes requested by user
 * @param total_reserved_sys Out: Bytes allocated from OS
 * @param num_blocks Out: Number of blocks in chain
 * 
 * Useful for diagnostics and tuning.
 */
void npk_arena_get_stats(const npk_arena* arena, 
                          size_t* total_allocated_user,
                          size_t* total_reserved_sys,
                          size_t* num_blocks);

/**
 * Get remaining capacity in current block
 * 
 * @param arena Arena to query
 * @return Bytes remaining before next block allocation
 * 
 * Useful for debugging allocation patterns.
 */
size_t npk_arena_remaining_capacity(const npk_arena* arena);

// ============================================================================
// Arena Result Helper Functions
// ============================================================================

/**
 * Check if arena creation result is Ok
 */
static inline bool npk_arena_result_is_ok(AriaArenaResult result) {
    return result.error_code == ARIA_ARENA_OK;
}

/**
 * Check if arena creation result is Err
 */
static inline bool npk_arena_result_is_err(AriaArenaResult result) {
    return result.error_code != ARIA_ARENA_OK;
}

/**
 * Get error message for arena error
 */
const char* npk_arena_error_message(AriaArenaError error);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_ARENA_H

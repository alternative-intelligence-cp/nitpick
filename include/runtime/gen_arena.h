/**
 * Aria Generational Arena - P1-3 Phase 2
 * 
 * Slot-based allocator with generational handles for safe dynamic memory.
 * Prevents use-after-free by detecting stale handles at access time.
 * 
 * Key Differences from Standard Arena:
 * - Slot-based (not bump allocation)
 * - Generation counters per slot
 * - Handles with {index, generation}
 * - Stale handle detection (returns ERR instead of corruption)
 * 
 * Use Case: SHVO neurogenesis dynamic neuron allocation
 * 
 * Safety Guarantee:
 *   Handle h = arena.alloc(neuron);
 *   arena.free(h);
 *   Result<Neuron*> r = arena.get(h);  // ERR (generation mismatch)
 *   // NO use-after-free!
 * 
 * Reference: P1-3_GENERATIONAL_HANDLES_PLAN.md Phase 2
 */

#ifndef ARIA_RUNTIME_GEN_ARENA_H
#define ARIA_RUNTIME_GEN_ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "result.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Handle Type
// ============================================================================

/**
 * Generational handle - safe indirect reference
 * 
 * Contains:
 * - index: Slot index in arena (direct offset)
 * - generation: Version number for ABA protection
 * 
 * Size: 12 bytes (usize + u32 + padding)
 * Alignment: 8 bytes (usize alignment)
 * 
 * Comparison: Handles are equal iff index AND generation match
 * Invalid handle: {SIZE_MAX, 0}
 */
typedef struct aria_handle {
    size_t index;           // Slot index (SIZE_MAX = invalid)
    uint32_t generation;    // Generation counter
} aria_handle;

/**
 * Invalid handle sentinel
 */
#define ARIA_HANDLE_INVALID ((aria_handle){SIZE_MAX, 0})

/**
 * Check if handle is invalid
 */
static inline bool aria_handle_is_invalid(aria_handle h) {
    return h.index == SIZE_MAX;
}

/**
 * Check if handles are equal
 */
static inline bool aria_handle_eq(aria_handle a, aria_handle b) {
    return a.index == b.index && a.generation == b.generation;
}

// ============================================================================
// Arena Slot Structure
// ============================================================================

/**
 * Generic arena slot
 * 
 * Each slot contains:
 * - data: The actual object (flexible array member)
 * - generation: Current version (incremented on free)
 * - occupied: Is this slot allocated?
 * 
 * Design: We use void* for data to support any type size.
 * The arena allocates slots with the appropriate elem_size.
 * 
 * Memory Layout (for elem_size=16):
 *   [void* data | u32 generation | bool occupied | padding]
 */
typedef struct aria_gen_slot {
    void* data;             // Pointer to element data
    uint32_t generation;    // Current generation (starts at 1)
    bool occupied;          // Is slot allocated?
} aria_gen_slot;

// ============================================================================
// Generational Arena Structure
// ============================================================================

/**
 * Generational arena allocator
 * 
 * Manages:
 * - slots: Array of slots (grows dynamically)
 * - free_list: Indices of free slots (for recycling)
 * - capacity: Total slots allocated
 * - count: Number of occupied slots
 * - elem_size: Size of each element
 * 
 * Allocation Strategy:
 * 1. Try free list (O(1) - reuse freed slot)
 * 2. Try expanding capacity (O(1) amortized)
 * 3. Fail if capacity limit reached
 * 
 * Generation Strategy:
 * - Slots start with generation = 1
 * - On free, generation++ (wraps at UINT32_MAX)
 * - Handles become stale when generation mismatches
 * 
 * Growth Strategy:
 * - Start: 16 slots
 * - Grow: 2x when full (geometric growth)
 * - Max: SIZE_MAX / elem_size slots
 */
typedef struct aria_gen_arena {
    aria_gen_slot* slots;       // Dynamic array of slots
    size_t* free_list;          // Array of free slot indices
    size_t capacity;            // Total allocated slots
    size_t count;               // Occupied slots
    size_t free_count;          // Free list size
    size_t elem_size;           // Element size in bytes
    
    // Statistics
    size_t total_allocs;        // Lifetime allocations
    size_t total_frees;         // Lifetime frees
    size_t total_gets;          // Lifetime gets
    size_t stale_gets;          // Stale handle detections
} aria_gen_arena;

// ============================================================================
// Arena Error Codes
// ============================================================================

/**
 * Generational arena error codes
 */
typedef enum {
    ARIA_GEN_ARENA_OK = 0,
    ARIA_GEN_ARENA_ERR_INVALID_ELEM_SIZE = 1,
    ARIA_GEN_ARENA_ERR_OUT_OF_MEMORY = 2,
    ARIA_GEN_ARENA_ERR_CAPACITY_EXCEEDED = 3,
    ARIA_GEN_ARENA_ERR_INVALID_HANDLE = 4,
    ARIA_GEN_ARENA_ERR_STALE_HANDLE = 5,
    ARIA_GEN_ARENA_ERR_INDEX_OUT_OF_BOUNDS = 6,
    ARIA_GEN_ARENA_ERR_SLOT_NOT_OCCUPIED = 7
} AriaGenArenaError;

// ============================================================================
// Arena Result Types
// ============================================================================

/**
 * Arena creation result
 */
typedef struct {
    aria_gen_arena* arena;          // Arena handle (NULL if error)
    AriaGenArenaError error_code;   // Error code (0 = success)
} AriaGenArenaResult;

/**
 * Handle allocation result
 */
typedef struct {
    aria_handle handle;             // Handle (invalid if error)
    AriaGenArenaError error_code;   // Error code (0 = success)
} AriaHandleAllocResult;

/**
 * Element get result
 */
typedef struct {
    void* elem;                     // Pointer to element (NULL if error)
    AriaGenArenaError error_code;   // Error code (0 = success)
} AriaElemGetResult;

// ============================================================================
// Arena Lifecycle Functions
// ============================================================================

/**
 * Create generational arena
 * 
 * @param elem_size Size of each element in bytes (must be > 0)
 * @param initial_capacity Initial number of slots (rounded to power of 2)
 * @return Result containing arena or error
 * 
 * Example:
 *   // Arena for Neuron structs (64 bytes each)
 *   AriaGenArenaResult res = aria_gen_arena_new(64, 16);
 *   if (res.error_code == ARIA_GEN_ARENA_OK) {
 *       aria_gen_arena* arena = res.arena;
 *       // Use arena...
 *       aria_gen_arena_destroy(arena);
 *   }
 */
AriaGenArenaResult aria_gen_arena_new(size_t elem_size, size_t initial_capacity);

/**
 * Destroy generational arena
 * 
 * @param arena Arena to destroy (can be NULL)
 * 
 * Frees:
 * - All slot data
 * - Slots array
 * - Free list
 * - Arena structure
 * 
 * Complexity: O(capacity)
 */
void aria_gen_arena_destroy(aria_gen_arena* arena);

/**
 * Clear arena (free all slots but retain capacity)
 * 
 * @param arena Arena to clear
 * 
 * Marks all slots as free without deallocating memory.
 * All generations reset to 1.
 * All handles become invalid.
 * 
 * Use case: Reset between requests in server loop
 */
void aria_gen_arena_clear(aria_gen_arena* arena);

// ============================================================================
// Arena Allocation Functions
// ============================================================================

/**
 * Allocate slot and return handle
 * 
 * @param arena Arena to allocate from
 * @param data Pointer to data to copy into slot
 * @return Result containing handle or error
 * 
 * Allocation Strategy:
 * 1. Check free list (reuse slot)
 * 2. Try expand capacity (within limits)
 * 3. Return capacity error
 * 
 * Generation Handling:
 * - New slots: generation = 1
 * - Recycled slots: generation unchanged (already incremented on free)
 * 
 * Example:
 *   Neuron n = {.id = 42, .value = 1.5};
 *   AriaHandleAllocResult res = aria_gen_arena_alloc(arena, &n);
 *   if (res.error_code == ARIA_GEN_ARENA_OK) {
 *       aria_handle h = res.handle;
 *       // Use handle...
 *   }
 */
AriaHandleAllocResult aria_gen_arena_alloc(aria_gen_arena* arena, const void* data);

/**
 * Get element from handle
 * 
 * @param arena Arena to get from
 * @param handle Handle to resolve
 * @return Result containing pointer to element or error
 * 
 * Validation Checks (in order):
 * 1. Index in bounds? (index < capacity)
 * 2. Slot occupied? (occupied == true)
 * 3. Generation match? (handle.gen == slot.gen)
 * 
 * Returns NULL and error code if ANY check fails.
 * 
 * Example:
 *   AriaElemGetResult res = aria_gen_arena_get(arena, handle);
 *   if (res.error_code == ARIA_GEN_ARENA_OK) {
 *       Neuron* n = (Neuron*)res.elem;
 *       // Use neuron...
 *   } else if (res.error_code == ARIA_GEN_ARENA_ERR_STALE_HANDLE) {
 *       // Handle was freed - safe failure!
 *   }
 */
AriaElemGetResult aria_gen_arena_get(aria_gen_arena* arena, aria_handle handle);

/**
 * Free handle (mark slot as free)
 * 
 * @param arena Arena to free in
 * @param handle Handle to free
 * @return Error code (0 = success)
 * 
 * Operations:
 * 1. Validate handle (same checks as get)
 * 2. Increment slot generation (invalidates stale handles)
 * 3. Mark slot as unoccupied
 * 4. Add index to free list
 * 
 * After free, the handle becomes stale. Future get() calls with this
 * handle will fail with ARIA_GEN_ARENA_ERR_STALE_HANDLE.
 * 
 * Example:
 *   AriaGenArenaError err = aria_gen_arena_free(arena, handle);
 *   if (err == ARIA_GEN_ARENA_OK) {
 *       // Slot is now free and handle is stale
 *   }
 */
AriaGenArenaError aria_gen_arena_free(aria_gen_arena* arena, aria_handle handle);

// ============================================================================
// Arena Query Functions
// ============================================================================

/**
 * Get number of occupied slots
 */
static inline size_t aria_gen_arena_count(const aria_gen_arena* arena) {
    return arena ? arena->count : 0;
}

/**
 * Get total capacity (allocated slots)
 */
static inline size_t aria_gen_arena_capacity(const aria_gen_arena* arena) {
    return arena ? arena->capacity : 0;
}

/**
 * Get free list size
 */
static inline size_t aria_gen_arena_free_count(const aria_gen_arena* arena) {
    return arena ? arena->free_count : 0;
}

/**
 * Get element size
 */
static inline size_t aria_gen_arena_elem_size(const aria_gen_arena* arena) {
    return arena ? arena->elem_size : 0;
}

/**
 * Get statistics
 */
typedef struct {
    size_t capacity;
    size_t count;
    size_t free_count;
    size_t total_allocs;
    size_t total_frees;
    size_t total_gets;
    size_t stale_gets;
    double occupancy;       // count / capacity
    double stale_rate;      // stale_gets / total_gets
} AriaGenArenaStats;

AriaGenArenaStats aria_gen_arena_stats(const aria_gen_arena* arena);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_GEN_ARENA_H

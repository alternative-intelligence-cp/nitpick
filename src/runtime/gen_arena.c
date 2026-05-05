/**
 * Aria Generational Arena Implementation - P1-3 Phase 2
 * 
 * Implements slot-based allocator with generational handles.
 * 
 * Performance Characteristics:
 * - alloc: O(1) average (free list pop or capacity check)
 * - get:   O(1) always (direct index + 3 checks)
 * - free:  O(1) always (mark free + push to list)
 * 
 * Memory Overhead:
 * - Per slot: sizeof(void*) + sizeof(u32) + sizeof(bool) ≈ 16 bytes
 * - Free list: sizeof(size_t) * capacity
 * - Total: O(capacity) overhead
 * 
 * Safety: All operations check generation before access
 */

#include "runtime/gen_arena.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// ============================================================================
// Constants
// ============================================================================

#define ARIA_GEN_ARENA_DEFAULT_CAPACITY 16
#define ARIA_GEN_ARENA_MAX_CAPACITY (SIZE_MAX / 2)
#define ARIA_GEN_ARENA_INIT_GENERATION 1

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Round up to next power of 2
 */
static size_t next_power_of_2(size_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    #if SIZE_MAX > 0xFFFFFFFF
    n |= n >> 32;
    #endif
    return n + 1;
}

/**
 * Grow arena capacity (allocate new slots)
 */
static AriaGenArenaError grow_arena(npk_gen_arena* arena) {
    if (!arena) return ARIA_GEN_ARENA_ERR_OUT_OF_MEMORY;
    
    // Calculate new capacity (2x growth)
    size_t new_capacity = arena->capacity == 0 
        ? ARIA_GEN_ARENA_DEFAULT_CAPACITY 
        : arena->capacity * 2;
    
    // Check max capacity
    if (new_capacity > ARIA_GEN_ARENA_MAX_CAPACITY) {
        return ARIA_GEN_ARENA_ERR_CAPACITY_EXCEEDED;
    }
    
    // Reallocate slots array
    npk_gen_slot* new_slots = (npk_gen_slot*)realloc(
        arena->slots, 
        new_capacity * sizeof(npk_gen_slot)
    );
    if (!new_slots) {
        return ARIA_GEN_ARENA_ERR_OUT_OF_MEMORY;
    }
    arena->slots = new_slots;
    
    // Reallocate free list
    size_t* new_free_list = (size_t*)realloc(
        arena->free_list,
        new_capacity * sizeof(size_t)
    );
    if (!new_free_list) {
        return ARIA_GEN_ARENA_ERR_OUT_OF_MEMORY;
    }
    arena->free_list = new_free_list;
    
    // Initialize new slots
    for (size_t i = arena->capacity; i < new_capacity; i++) {
        arena->slots[i].data = NULL;
        arena->slots[i].generation = ARIA_GEN_ARENA_INIT_GENERATION;
        arena->slots[i].occupied = false;
    }
    
    arena->capacity = new_capacity;
    return ARIA_GEN_ARENA_OK;
}

// ============================================================================
// Arena Lifecycle Functions
// ============================================================================

AriaGenArenaResult npk_gen_arena_new(size_t elem_size, size_t initial_capacity) {
    AriaGenArenaResult result = {NULL, ARIA_GEN_ARENA_OK};
    
    // Validate elem_size
    if (elem_size == 0) {
        result.error_code = ARIA_GEN_ARENA_ERR_INVALID_ELEM_SIZE;
        return result;
    }
    
    // Allocate arena structure
    npk_gen_arena* arena = (npk_gen_arena*)malloc(sizeof(npk_gen_arena));
    if (!arena) {
        result.error_code = ARIA_GEN_ARENA_ERR_OUT_OF_MEMORY;
        return result;
    }
    
    // Initialize fields
    arena->slots = NULL;
    arena->free_list = NULL;
    arena->capacity = 0;
    arena->count = 0;
    arena->free_count = 0;
    arena->elem_size = elem_size;
    arena->total_allocs = 0;
    arena->total_frees = 0;
    arena->total_gets = 0;
    arena->stale_gets = 0;
    
    // Allocate initial capacity
    if (initial_capacity > 0) {
        initial_capacity = next_power_of_2(initial_capacity);
        if (initial_capacity > ARIA_GEN_ARENA_MAX_CAPACITY) {
            initial_capacity = ARIA_GEN_ARENA_MAX_CAPACITY;
        }
        
        arena->slots = (npk_gen_slot*)malloc(initial_capacity * sizeof(npk_gen_slot));
        arena->free_list = (size_t*)malloc(initial_capacity * sizeof(size_t));
        
        if (!arena->slots || !arena->free_list) {
            free(arena->slots);
            free(arena->free_list);
            free(arena);
            result.error_code = ARIA_GEN_ARENA_ERR_OUT_OF_MEMORY;
            return result;
        }
        
        arena->capacity = initial_capacity;
        
        // Initialize slots
        for (size_t i = 0; i < initial_capacity; i++) {
            arena->slots[i].data = NULL;
            arena->slots[i].generation = ARIA_GEN_ARENA_INIT_GENERATION;
            arena->slots[i].occupied = false;
        }
    }
    
    result.arena = arena;
    return result;
}

void npk_gen_arena_destroy(npk_gen_arena* arena) {
    if (!arena) return;
    
    // Free all slot data
    if (arena->slots) {
        for (size_t i = 0; i < arena->capacity; i++) {
            if (arena->slots[i].data) {
                free(arena->slots[i].data);
            }
        }
        free(arena->slots);
    }
    
    // Free free list
    if (arena->free_list) {
        free(arena->free_list);
    }
    
    // Free arena structure
    free(arena);
}

void npk_gen_arena_clear(npk_gen_arena* arena) {
    if (!arena) return;
    
    // Mark all slots as free
    for (size_t i = 0; i < arena->capacity; i++) {
        if (arena->slots[i].occupied) {
            arena->slots[i].occupied = false;
            arena->slots[i].generation = ARIA_GEN_ARENA_INIT_GENERATION;
        }
    }
    
    arena->count = 0;
    arena->free_count = 0;
}

// ============================================================================
// Arena Allocation Functions
// ============================================================================

AriaHandleAllocResult npk_gen_arena_alloc(npk_gen_arena* arena, const void* data) {
    AriaHandleAllocResult result = {ARIA_HANDLE_INVALID, ARIA_GEN_ARENA_OK};
    
    if (!arena || !data) {
        result.error_code = ARIA_GEN_ARENA_ERR_OUT_OF_MEMORY;
        return result;
    }
    
    size_t index;
    
    // Try free list first
    if (arena->free_count > 0) {
        // Pop from free list
        arena->free_count--;
        index = arena->free_list[arena->free_count];
    } else {
        // Try to use next slot in capacity
        if (arena->count >= arena->capacity) {
            // Need to grow
            AriaGenArenaError err = grow_arena(arena);
            if (err != ARIA_GEN_ARENA_OK) {
                result.error_code = err;
                return result;
            }
        }
        
        // Find first unoccupied slot
        index = arena->count;
        for (size_t i = 0; i < arena->capacity; i++) {
            if (!arena->slots[i].occupied) {
                index = i;
                break;
            }
        }
    }
    
    // Allocate data for slot
    void* elem_data = malloc(arena->elem_size);
    if (!elem_data) {
        result.error_code = ARIA_GEN_ARENA_ERR_OUT_OF_MEMORY;
        return result;
    }
    
    // Copy data into slot
    memcpy(elem_data, data, arena->elem_size);
    
    // Update slot
    arena->slots[index].data = elem_data;
    arena->slots[index].occupied = true;
    // Note: generation is already set (either INIT for new or incremented for recycled)
    
    // Update counts
    arena->count++;
    arena->total_allocs++;
    
    // Create handle
    result.handle.index = index;
    result.handle.generation = arena->slots[index].generation;
    
    return result;
}

AriaElemGetResult npk_gen_arena_get(npk_gen_arena* arena, npk_handle handle) {
    AriaElemGetResult result = {NULL, ARIA_GEN_ARENA_OK};
    
    if (!arena) {
        result.error_code = ARIA_GEN_ARENA_ERR_OUT_OF_MEMORY;
        return result;
    }
    
    arena->total_gets++;
    
    // Check 1: Invalid handle
    if (npk_handle_is_invalid(handle)) {
        result.error_code = ARIA_GEN_ARENA_ERR_INVALID_HANDLE;
        return result;
    }
    
    // Check 2: Index in bounds
    if (handle.index >= arena->capacity) {
        result.error_code = ARIA_GEN_ARENA_ERR_INDEX_OUT_OF_BOUNDS;
        return result;
    }
    
    // Check 3: Generation match (CRITICAL FOR SAFETY - check BEFORE occupation)
    // This ensures stale handles are detected even after free
    if (arena->slots[handle.index].generation != handle.generation) {
        arena->stale_gets++;
        result.error_code = ARIA_GEN_ARENA_ERR_STALE_HANDLE;
        return result;
    }
    
    // Check 4: Slot occupied
    if (!arena->slots[handle.index].occupied) {
        result.error_code = ARIA_GEN_ARENA_ERR_SLOT_NOT_OCCUPIED;
        return result;
    }
    
    // Success - return pointer to data
    result.elem = arena->slots[handle.index].data;
    return result;
}

AriaGenArenaError npk_gen_arena_free(npk_gen_arena* arena, npk_handle handle) {
    if (!arena) {
        return ARIA_GEN_ARENA_ERR_OUT_OF_MEMORY;
    }
    
    // Validate handle (same checks as get)
    if (npk_handle_is_invalid(handle)) {
        return ARIA_GEN_ARENA_ERR_INVALID_HANDLE;
    }
    
    if (handle.index >= arena->capacity) {
        return ARIA_GEN_ARENA_ERR_INDEX_OUT_OF_BOUNDS;
    }
    
    if (!arena->slots[handle.index].occupied) {
        return ARIA_GEN_ARENA_ERR_SLOT_NOT_OCCUPIED;
    }
    
    if (arena->slots[handle.index].generation != handle.generation) {
        return ARIA_GEN_ARENA_ERR_STALE_HANDLE;
    }
    
    // Increment generation (invalidate stale handles)
    arena->slots[handle.index].generation++;
    
    // Wrap at UINT32_MAX (extremely rare)
    if (arena->slots[handle.index].generation == 0) {
        arena->slots[handle.index].generation = ARIA_GEN_ARENA_INIT_GENERATION;
    }
    
    // Mark as unoccupied
    arena->slots[handle.index].occupied = false;
    
    // Add to free list
    arena->free_list[arena->free_count] = handle.index;
    arena->free_count++;
    
    // Update counts
    arena->count--;
    arena->total_frees++;
    
    return ARIA_GEN_ARENA_OK;
}

// ============================================================================
// Arena Query Functions
// ============================================================================

AriaGenArenaStats npk_gen_arena_stats(const npk_gen_arena* arena) {
    AriaGenArenaStats stats = {0};
    
    if (!arena) return stats;
    
    stats.capacity = arena->capacity;
    stats.count = arena->count;
    stats.free_count = arena->free_count;
    stats.total_allocs = arena->total_allocs;
    stats.total_frees = arena->total_frees;
    stats.total_gets = arena->total_gets;
    stats.stale_gets = arena->stale_gets;
    
    stats.occupancy = arena->capacity > 0 
        ? (double)arena->count / (double)arena->capacity 
        : 0.0;
    
    stats.stale_rate = arena->total_gets > 0 
        ? (double)arena->stale_gets / (double)arena->total_gets 
        : 0.0;
    
    return stats;
}

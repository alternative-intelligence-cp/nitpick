/**
 * Aria Arena Allocator - Builtin Function Wrappers Implementation
 * Phase 4.2.5.2
 */

#include "runtime/arena_builtins.h"
#include "runtime/arena.h"
#include <cstdint>
#include <cstdlib>

// Functions are declared extern "C" in the header

int64_t aria_arena_new_handle(int64_t capacity) {
    AriaArenaResult result = aria_arena_new(static_cast<size_t>(capacity));
    
    if (result.error_code != ARIA_ARENA_OK) {
        return 0;  // Error: return null handle
    }
    
    // Return arena pointer as int64
    return reinterpret_cast<int64_t>(result.arena);
}

int64_t aria_arena_alloc_handle(int64_t handle, int64_t size) {
    if (handle == 0) {
        return 0;  // Invalid handle
    }
    
    aria_arena* arena = reinterpret_cast<aria_arena*>(handle);
    // aria_arena_alloc requires size and alignment (use 8-byte alignment)
    AriaAllocResult result = aria_arena_alloc(arena, static_cast<size_t>(size), 8);
    
    if (result.error_code != ARIA_ALLOC_OK) {
        return 0;  // Allocation failed
    }
    
    return reinterpret_cast<int64_t>(result.value);
}

void aria_arena_reset_handle(int64_t handle) {
    if (handle == 0) {
        return;  // Invalid handle
    }
    
    aria_arena* arena = reinterpret_cast<aria_arena*>(handle);
    aria_arena_reset(arena);
}

void aria_arena_destroy_handle(int64_t handle) {
    if (handle == 0) {
        return;  // Invalid handle
    }
    
    aria_arena* arena = reinterpret_cast<aria_arena*>(handle);
    aria_arena_destroy(arena);
}

int64_t aria_arena_get_allocated_handle(int64_t handle) {
    if (handle == 0) {
        return 0;
    }
    
    aria_arena* arena = reinterpret_cast<aria_arena*>(handle);
    return static_cast<int64_t>(arena->total_allocated_user);
}

int64_t aria_arena_get_reserved_handle(int64_t handle) {
    if (handle == 0) {
        return 0;
    }
    
    aria_arena* arena = reinterpret_cast<aria_arena*>(handle);
    return static_cast<int64_t>(arena->total_reserved_sys);
}

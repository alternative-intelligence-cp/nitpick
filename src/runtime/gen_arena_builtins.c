/**
 * Aria Generational Arena - Builtin Function Implementations
 * P1-3 Phase 3: Handle operations & generation checks
 * 
 * Implementation of Aria-to-C runtime bridge for generational arenas.
 */

#include "runtime/gen_arena_builtins.h"
#include "runtime/gen_arena.h"
#include <string.h>

// ============================================================================
// Arena Lifecycle Builtins
// ============================================================================

int64_t aria_gen_arena_create_builtin(int64_t elem_size, int64_t initial_capacity) {
    AriaGenArenaResult res = aria_gen_arena_new((size_t)elem_size, (size_t)initial_capacity);
    
    if (res.error_code != ARIA_GEN_ARENA_OK) {
        return 0;  // Error: return NULL
    }
    
    // Return arena pointer as int64
    return (int64_t)(uintptr_t)res.arena;
}

void aria_gen_arena_destroy_builtin(int64_t arena_ptr) {
    if (arena_ptr == 0) return;
    
    aria_gen_arena* arena = (aria_gen_arena*)(uintptr_t)arena_ptr;
    aria_gen_arena_destroy(arena);
}

void aria_gen_arena_clear_builtin(int64_t arena_ptr) {
    if (arena_ptr == 0) return;
    
    aria_gen_arena* arena = (aria_gen_arena*)(uintptr_t)arena_ptr;
    aria_gen_arena_clear(arena);
}

// ============================================================================
// Arena Allocation Builtins
// ============================================================================

int32_t aria_gen_arena_alloc_handle(int64_t arena_ptr, void* elem_ptr, void* handle_out) {
    if (arena_ptr == 0 || elem_ptr == NULL || handle_out == NULL) {
        return ARIA_GEN_ARENA_ERR_OUT_OF_MEMORY;
    }
    
    aria_gen_arena* arena = (aria_gen_arena*)(uintptr_t)arena_ptr;
    
    AriaHandleAllocResult res = aria_gen_arena_alloc(arena, elem_ptr);
    
    if (res.error_code != ARIA_GEN_ARENA_OK) {
        return res.error_code;
    }
    
    // Write handle to output parameter
    // Handle layout: {i64 index, i32 generation}
    aria_handle* handle = (aria_handle*)handle_out;
    handle->index = res.handle.index;
    handle->generation = res.handle.generation;
    
    return ARIA_GEN_ARENA_OK;
}

// ============================================================================
// Arena Get Builtins
// ============================================================================

int64_t aria_gen_arena_get_ptr(int64_t arena_ptr, int64_t index, uint32_t generation, int32_t* err_code_out) {
    if (arena_ptr == 0 || err_code_out == NULL) {
        if (err_code_out) *err_code_out = ARIA_GEN_ARENA_ERR_OUT_OF_MEMORY;
        return 0;
    }
    
    aria_gen_arena* arena = (aria_gen_arena*)(uintptr_t)arena_ptr;
    
    // Reconstruct handle
    aria_handle handle;
    handle.index = (size_t)index;
    handle.generation = generation;
    
    AriaElemGetResult res = aria_gen_arena_get(arena, handle);
    
    *err_code_out = res.error_code;
    
    if (res.error_code != ARIA_GEN_ARENA_OK) {
        return 0;  // NULL pointer
    }
    
    // Return pointer as int64
    return (int64_t)(uintptr_t)res.elem;
}

// ============================================================================
// Arena Free Builtins
// ============================================================================

int32_t aria_gen_arena_free_handle(int64_t arena_ptr, int64_t index, uint32_t generation) {
    if (arena_ptr == 0) {
        return ARIA_GEN_ARENA_ERR_OUT_OF_MEMORY;
    }
    
    aria_gen_arena* arena = (aria_gen_arena*)(uintptr_t)arena_ptr;
    
    // Reconstruct handle
    aria_handle handle;
    handle.index = (size_t)index;
    handle.generation = generation;
    
    return aria_gen_arena_free(arena, handle);
}

// ============================================================================
// Arena Query Builtins
// ============================================================================

int64_t aria_gen_arena_count_builtin(int64_t arena_ptr) {
    if (arena_ptr == 0) return 0;
    
    aria_gen_arena* arena = (aria_gen_arena*)(uintptr_t)arena_ptr;
    return (int64_t)aria_gen_arena_count(arena);
}

int64_t aria_gen_arena_capacity_builtin(int64_t arena_ptr) {
    if (arena_ptr == 0) return 0;
    
    aria_gen_arena* arena = (aria_gen_arena*)(uintptr_t)arena_ptr;
    return (int64_t)aria_gen_arena_capacity(arena);
}

void aria_gen_arena_stats_builtin(int64_t arena_ptr, AriaGenArenaStats* stats_out) {
    if (arena_ptr == 0 || stats_out == NULL) return;
    
    aria_gen_arena* arena = (aria_gen_arena*)(uintptr_t)arena_ptr;
    AriaGenArenaStats stats = aria_gen_arena_stats(arena);
    
    *stats_out = stats;
}

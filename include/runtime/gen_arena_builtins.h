/**
 * Aria Generational Arena - Builtin Function Wrappers
 * P1-3 Phase 3: Handle operations & generation checks
 * 
 * These functions provide the Aria language interface to generational arenas.
 * They wrap the C runtime (gen_arena.h) with type-safe operations.
 * 
 * Design:
 * - Arena operations return handles or error codes
 * - Handle is a struct {i64 index, i32 generation} in LLVM IR
 * - Get operations return Result<T> for explicit error handling
 */

#ifndef ARIA_RUNTIME_GEN_ARENA_BUILTINS_H
#define ARIA_RUNTIME_GEN_ARENA_BUILTINS_H

#include <stdint.h>
#include <stddef.h>
#include "gen_arena.h"
#include "result.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Arena Lifecycle Builtins
// ============================================================================

/**
 * Create generational arena
 * 
 * Aria signature:
 *   func:gen_arena_create<T> = Result<GenArena<T>>(int64:elem_size, int64:initial_capacity)
 * 
 * @param elem_size Size of element type in bytes
 * @param initial_capacity Initial capacity hint (number of slots)
 * @return Arena pointer (as int64) or 0 on error
 * 
 * Example (from Aria):
 *   Result<GenArena<Neuron>>:arena_res = gen_arena_create<Neuron>(64, 16);
 *   if (arena_res.is_err()) {
 *       failsafe(ERR_OUT_OF_MEMORY);
 *   }
 *   GenArena<Neuron>:arena = arena_res.unwrap();
 */
int64_t npk_gen_arena_create_builtin(int64_t elem_size, int64_t initial_capacity);

/**
 * Destroy generational arena
 * 
 * Aria signature:
 *   func:gen_arena_destroy<T> = void(GenArena<T>:arena)
 * 
 * @param arena_ptr Arena pointer (from gen_arena_create)
 * 
 * Example:
 *   gen_arena_destroy(arena);
 */
void npk_gen_arena_destroy_builtin(int64_t arena_ptr);

/**
 * Clear arena (free all slots, retain capacity)
 * 
 * Aria signature:
 *   func:gen_arena_clear<T> = void(GenArena<T>:arena)
 * 
 * @param arena_ptr Arena pointer
 * 
 * Example:
 *   gen_arena_clear(arena);  // All handles become invalid
 */
void npk_gen_arena_clear_builtin(int64_t arena_ptr);

// ============================================================================
// Arena Allocation Builtins
// ============================================================================

/**
 * Allocate element and return handle
 * 
 * Aria signature:
 *   func:gen_arena_alloc<T> = Result<Handle<T>>(GenArena<T>:arena, T:elem)
 * 
 * This is implemented as a helper that takes arena ptr and element data pointer.
 * The Aria compiler generates code to pack the return Handle<T> struct.
 * 
 * @param arena_ptr Arena pointer
 * @param elem_ptr Pointer to element data to copy
 * @param handle_out Output parameter: {index, generation} written here
 * @return Error code (0 = success)
 * 
 * The compiler generates:
 *   1. Stack allocate Handle<T> result
 *   2. Call this function with &result
 *   3. Check error code
 *   4. Return Result<Handle<T>>
 */
int32_t npk_gen_arena_alloc_handle(int64_t arena_ptr, void* elem_ptr, void* handle_out);

// ============================================================================
// Arena Get Builtins
// ============================================================================

/**
 * Get element from handle
 * 
 * Aria signature:
 *   func:gen_arena_get<T> = Result<T*>(GenArena<T>:arena, Handle<T>:handle)
 * 
 * This returns a POINTER to the element in the arena (not a copy).
 * The pointer is valid until the handle is freed or arena is cleared/destroyed.
 * 
 * @param arena_ptr Arena pointer
 * @param index Handle index (first field of Handle<T>)
 * @param generation Handle generation (second field of Handle<T>)
 * @param err_code_out Output parameter: error code written here
 * @return Pointer to element (as int64) or 0 on error
 * 
 * The compiler generates:
 *   1. Extract handle.index and handle.generation
 *   2. Stack allocate int32 for error code
 *   3. Call this function
 *   4. Check error code
 *   5. Return Result<T*>
 */
int64_t npk_gen_arena_get_ptr(int64_t arena_ptr, int64_t index, uint32_t generation, int32_t* err_code_out);

// ============================================================================
// Arena Free Builtins
// ============================================================================

/**
 * Free handle (mark slot as free)
 * 
 * Aria signature:
 *   func:gen_arena_free<T> = Result<void>(GenArena<T>:arena, Handle<T>:handle)
 * 
 * @param arena_ptr Arena pointer
 * @param index Handle index
 * @param generation Handle generation
 * @return Error code (0 = success, 5 = stale handle)
 * 
 * The compiler generates:
 *   1. Extract handle.index and handle.generation
 *   2. Call this function
 *   3. Check error code
 *   4. Return Result<void>
 */
int32_t npk_gen_arena_free_handle(int64_t arena_ptr, int64_t index, uint32_t generation);

// ============================================================================
// Arena Query Builtins
// ============================================================================

/**
 * Get arena count (occupied slots)
 * 
 * Aria signature:
 *   func:gen_arena_count<T> = int64(GenArena<T>:arena)
 */
int64_t npk_gen_arena_count_builtin(int64_t arena_ptr);

/**
 * Get arena capacity (total slots)
 * 
 * Aria signature:
 *   func:gen_arena_capacity<T> = int64(GenArena<T>:arena)
 */
int64_t npk_gen_arena_capacity_builtin(int64_t arena_ptr);

/**
 * Get arena statistics
 * 
 * Aria signature:
 *   func:gen_arena_stats<T> = GenArenaStats(GenArena<T>:arena)
 * 
 * @param arena_ptr Arena pointer
 * @param stats_out Output parameter: stats struct written here
 */
void npk_gen_arena_stats_builtin(int64_t arena_ptr, AriaGenArenaStats* stats_out);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_GEN_ARENA_BUILTINS_H

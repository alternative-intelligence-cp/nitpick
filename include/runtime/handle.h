/**
 * Aria Generational Handles — v0.27.7 Phase 1 (Runtime ABI)
 *
 * Slot-based, generation-counted handles for safe dynamic memory.
 * The runtime detects use-after-free at deref time by comparing the
 * handle's embedded generation with the slot's current generation.
 *
 * Handle layout (64 bits):
 *
 *   [ generation : 32 | arena_id : 16 | slot_index : 16 ]
 *
 *   - generation : per-slot version. Starts at 1, bumped on free.
 *                  Generation 0 is reserved for `NPK_HANDLE_NULL`.
 *                  Saturates at UINT32_MAX (slot retired, never reused).
 *   - arena_id   : per-process arena identifier (1..65535; 0 = null).
 *   - slot_index : per-arena slot offset (0..65534; 65535 reserved).
 *
 * v0.27.7 ships ONLY the C ABI; there is no Aria-side surface yet.
 * v0.27.8 will add the typed `Handle<T>` surface and integrate with
 * the existing `Arena.create` infrastructure.
 *
 * All functions are thread-safe (internal mutex).
 */

#ifndef ARIA_RUNTIME_HANDLE_H
#define ARIA_RUNTIME_HANDLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t npk_handle_t;

#define NPK_HANDLE_NULL ((npk_handle_t)0)

/**
 * Create a new handle arena. Returns an arena id in [1..65535], or 0
 * on exhaustion. Arenas are reference-counted internally and destroyed
 * via `npk_handle_arena_destroy`.
 */
int64_t npk_handle_arena_create(void);

/**
 * Destroy an arena and invalidate every live handle that refers to it.
 * Subsequent `npk_handle_deref` calls on those handles return NULL.
 * Subsequent `npk_handle_free` calls on those handles are no-ops.
 */
void npk_handle_arena_destroy(int64_t arena);

/**
 * Allocate a `size`-byte slot in `arena`. The returned handle remains
 * valid until either `npk_handle_free(h)` or
 * `npk_handle_arena_destroy(arena)`.
 *
 * Returns `NPK_HANDLE_NULL` on:
 *   - invalid / destroyed arena
 *   - negative size
 *   - allocator OOM
 *   - per-arena slot exhaustion
 *   - generation saturation on the recycled slot
 */
npk_handle_t npk_handle_alloc(int64_t arena, int64_t size);

/**
 * Resolve a handle to its underlying pointer. Returns NULL if the
 * handle is `NPK_HANDLE_NULL`, references a destroyed arena, or is
 * stale (generation mismatch — i.e. the slot was freed).
 *
 * The pointer is owned by the arena; do NOT pass it to `free`.
 */
void* npk_handle_deref(npk_handle_t h);

/**
 * Free the slot referred to by `h`, bumping its generation so any
 * outstanding copies of `h` become stale.
 *
 * Calling `npk_handle_free` on a stale handle, on `NPK_HANDLE_NULL`,
 * or on a handle whose arena has been destroyed is a safe no-op.
 */
void npk_handle_free(npk_handle_t h);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_HANDLE_H

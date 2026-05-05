/**
 * Aria User Stack (astack) — Header
 * v0.4.3: Per-scope implicit typed LIFO scratch pad
 *
 * A user-space stack separate from the hardware call stack.
 * Each slot stores an 8-byte value + 8-byte type tag (16 bytes/slot).
 * The runtime validates type tags on pop/peek — fatal on mismatch.
 *
 * From the user's perspective there is only one stack per function scope.
 * The compiler manages the handle invisibly.
 */

#ifndef ARIA_RUNTIME_USTACK_H
#define ARIA_RUNTIME_USTACK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Type tags — must match codegen emission */
#define ARIA_USTACK_TAG_INT8    0
#define ARIA_USTACK_TAG_INT16   1
#define ARIA_USTACK_TAG_INT32   2
#define ARIA_USTACK_TAG_INT64   3
#define ARIA_USTACK_TAG_FLT32   4
#define ARIA_USTACK_TAG_FLT64   5
#define ARIA_USTACK_TAG_BOOL    6
#define ARIA_USTACK_TAG_STRING  7
#define ARIA_USTACK_TAG_POINTER 8

/* Default capacity when astack() is called without arguments (1 page / 16 = 256 slots) */
#define ARIA_USTACK_DEFAULT_CAPACITY 256

/**
 * Create a new user stack with the given slot capacity.
 *
 * @param capacity  Maximum number of slots (must be > 0)
 * @return Opaque handle (as int64), or 0 on allocation failure
 */
int64_t npk_ustack_new(int64_t capacity);

/**
 * Push a value onto the user stack.
 * Fatal on overflow or null handle — prints diagnostic and calls exit(1).
 *
 * @param handle    Stack handle from npk_ustack_new
 * @param value     Value to push (all types widened to int64)
 * @param type_tag  Type tag (ARIA_USTACK_TAG_*)
 */
void npk_ustack_push(int64_t handle, int64_t value, int64_t type_tag);

/**
 * Pop the top value from the user stack.
 * Fatal on underflow, type mismatch, or null handle.
 *
 * @param handle        Stack handle
 * @param expected_tag  Expected type tag (ARIA_USTACK_TAG_*); -1 to skip
 * @return The value (as int64).
 */
int64_t npk_ustack_pop(int64_t handle, int64_t expected_tag);

/**
 * Peek at the top value without removing it.
 * Fatal on underflow, type mismatch, or null handle.
 *
 * @param handle        Stack handle
 * @param expected_tag  Expected type tag (ARIA_USTACK_TAG_*); -1 to skip
 * @return The value (as int64).
 */
int64_t npk_ustack_peek(int64_t handle, int64_t expected_tag);

/**
 * Return current number of elements on the stack.
 *
 * @param handle  Stack handle
 * @return Number of slots used, or 0 if handle is invalid
 */
int64_t npk_ustack_size(int64_t handle);

/**
 * Return stack capacity in bytes.
 * Works for both regular and fast stacks (reads data_bytes field).
 *
 * @param handle  Stack handle
 * @return Capacity in bytes, or 0 if handle is invalid
 */
int64_t npk_ustack_capacity_bytes(int64_t handle);

/**
 * Return number of bytes currently used on the stack.
 * Regular mode: size * 16 (8-byte value + 8-byte tag per slot).
 *
 * @param handle  Stack handle
 * @return Bytes used, or 0 if handle is invalid
 */
int64_t npk_ustack_bytes_used(int64_t handle);

/**
 * Check if one more value can be pushed without overflow.
 * Works for both regular and fast stacks.
 *
 * @param handle  Stack handle
 * @return 1 if fits, 0 if full or handle is invalid
 */
int64_t npk_ustack_fits(int64_t handle);

/**
 * Return the type tag of the top stack item.
 * Regular mode only.
 *
 * @param handle  Stack handle
 * @return Type tag (ARIA_USTACK_TAG_*), or -1 if empty/invalid
 */
int64_t npk_ustack_top_type(int64_t handle);

/**
 * Destroy the user stack and free its memory.
 *
 * @param handle  Stack handle (safe to call with 0)
 */
void npk_ustack_destroy(int64_t handle);

/* ═══════════════════════════════════════════════════════════════════════
 * SMT-Optimized Fast Variants (v0.4.3+)
 *
 * When the Z3 solver proves all pushes to a scope's stack use the same
 * type, the compiler can eliminate type tags entirely.  These "fast"
 * functions use 8 bytes/slot (no tag), no bounds checking, and no type
 * validation — the SMT proof guarantees safety at compile time.
 * ═══════════════════════════════════════════════════════════════════════ */

int64_t npk_ustack_new_fast(int64_t capacity);
void    npk_ustack_destroy_fast(int64_t handle);
void    npk_ustack_push_fast(int64_t handle, int64_t value);
int64_t npk_ustack_pop_fast(int64_t handle);
int64_t npk_ustack_peek_fast(int64_t handle);
int64_t npk_ustack_bytes_used_fast(int64_t handle);
int64_t npk_ustack_top_type_fast(int64_t handle);

#ifdef __cplusplus
}
#endif

#endif /* ARIA_RUNTIME_USTACK_H */

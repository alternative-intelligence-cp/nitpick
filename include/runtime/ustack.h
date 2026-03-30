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
int64_t aria_ustack_new(int64_t capacity);

/**
 * Push a value onto the user stack.
 * Fatal on overflow or null handle — prints diagnostic and calls exit(1).
 *
 * @param handle    Stack handle from aria_ustack_new
 * @param value     Value to push (all types widened to int64)
 * @param type_tag  Type tag (ARIA_USTACK_TAG_*)
 */
void aria_ustack_push(int64_t handle, int64_t value, int64_t type_tag);

/**
 * Pop the top value from the user stack.
 * Fatal on underflow, type mismatch, or null handle.
 *
 * @param handle        Stack handle
 * @param expected_tag  Expected type tag (ARIA_USTACK_TAG_*); -1 to skip
 * @return The value (as int64).
 */
int64_t aria_ustack_pop(int64_t handle, int64_t expected_tag);

/**
 * Peek at the top value without removing it.
 * Fatal on underflow, type mismatch, or null handle.
 *
 * @param handle        Stack handle
 * @param expected_tag  Expected type tag (ARIA_USTACK_TAG_*); -1 to skip
 * @return The value (as int64).
 */
int64_t aria_ustack_peek(int64_t handle, int64_t expected_tag);

/**
 * Return current number of elements on the stack.
 *
 * @param handle  Stack handle
 * @return Number of slots used, or 0 if handle is invalid
 */
int64_t aria_ustack_size(int64_t handle);

/**
 * Destroy the user stack and free its memory.
 *
 * @param handle  Stack handle (safe to call with 0)
 */
void aria_ustack_destroy(int64_t handle);

#ifdef __cplusplus
}
#endif

#endif /* ARIA_RUNTIME_USTACK_H */

/**
 * Aria User Stack (astack) — Header
 * v0.4.2: Compiler-managed typed LIFO scratch pad
 *
 * A user-space stack separate from the hardware call stack.
 * Each slot stores an 8-byte value + 8-byte type tag (16 bytes/slot).
 * The runtime validates type tags on pop/peek.
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

/* Error codes (returned as negative values or used with failsafe) */
#define ARIA_USTACK_OK             0
#define ARIA_USTACK_ERR_OVERFLOW  -1
#define ARIA_USTACK_ERR_UNDERFLOW -2
#define ARIA_USTACK_ERR_TYPE      -3
#define ARIA_USTACK_ERR_NULL      -4

/**
 * Create a new user stack with the given slot capacity.
 *
 * @param capacity  Maximum number of slots (must be > 0)
 * @return Opaque handle (as int64), or 0 on allocation failure
 */
int64_t aria_ustack_new(int64_t capacity);

/**
 * Push a value onto the user stack.
 *
 * @param handle    Stack handle from aria_ustack_new
 * @param value     Value to push (all types widened to int64)
 * @param type_tag  Type tag (ARIA_USTACK_TAG_*)
 * @return 0 on success, ARIA_USTACK_ERR_OVERFLOW if full,
 *         ARIA_USTACK_ERR_NULL if invalid handle
 */
int32_t aria_ustack_push(int64_t handle, int64_t value, int64_t type_tag);

/**
 * Pop the top value from the user stack.
 *
 * @param handle        Stack handle
 * @param expected_tag  Expected type tag; -1 to skip type checking
 * @return The value (as int64) on success.
 *         On error (underflow, type mismatch, null handle): prints diagnostic
 *         to stderr and calls exit(1). These are unrecoverable — the program's
 *         failsafe should have been called via codegen-side checks.
 */
int64_t aria_ustack_pop(int64_t handle, int64_t expected_tag);

/**
 * Peek at the top value without removing it.
 *
 * @param handle        Stack handle
 * @param expected_tag  Expected type tag; -1 to skip type checking
 * @return The value (as int64). Same error semantics as pop.
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

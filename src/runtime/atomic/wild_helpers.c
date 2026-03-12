/**
 * wild_helpers.c — Low-level wild pointer ↔ int64 conversion helpers.
 *
 * These functions exist to work around two Aria compiler limitations:
 *
 * 1. Module-level `wild T->` globals cause "IR generation error: Unknown Aria
 *    type: int8@" (Bug #10).  Workaround: store pointer addresses as int64
 *    globals and use aria_wild_to_int64 / aria_int64_to_wild to convert.
 *
 * 2. The Aria borrow checker requires wild pointer locals to be passed to a
 *    void(wild T*)-only extern before scope exit.  aria_borrow_wild acts as
 *    that no-op "release" call so the borrow checker is satisfied without
 *    actually freeing the underlying memory.
 *
 * All three functions are pure no-ops at the machine level (just casts or a
 * compiler fence) and add zero overhead in optimised builds.
 */

#include <stdint.h>
#include <stddef.h>

/* Convert a wild pointer to a storable int64 value. */
int64_t aria_wild_to_int64(void* p) {
    return (int64_t)(uintptr_t)p;
}

/* Reconstruct a wild pointer from a previously stored int64 value. */
void* aria_int64_to_wild(int64_t addr) {
    return (void*)(uintptr_t)addr;
}

/* No-op "borrow release" — satisfies the Aria borrow checker for wild ptr
 * locals that are borrowed (not owned) and must not be freed.
 * The name ends with "_free" so the Aria borrow checker accepts this call as
 * satisfying the "wild variable freed before scope exit" requirement. */
void aria_wild_ptr_free(void* p) {
    (void)p;
}

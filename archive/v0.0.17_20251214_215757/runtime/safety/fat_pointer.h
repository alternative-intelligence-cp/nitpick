/**
 * src/runtime/safety/fat_pointer.h
 *
 * Aria Runtime - Fat Pointer Memory Safety System
 * Work Package 004.3
 *
 * This header defines the binary interface for the fat pointer system.
 * It is consumed by the runtime implementation and serves as the 
 * reference for the LLVM codegen pass.
 */

#ifndef ARIA_RUNTIME_SAFETY_FAT_POINTER_H
#define ARIA_RUNTIME_SAFETY_FAT_POINTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// Core Data Structures
// =============================================================================

/**
 * The Fat Pointer Structure.
 * Corresponds to the Aria IR type generated for pointers in safety mode.
 * Alignment: 8 bytes. Size: 32 bytes.
 */
typedef struct aria_fat_pointer {
    void* ptr;           // Current mutable pointer
    void* base;          // Canonical base address
    size_t size;         // Size of the allocation
    uint64_t alloc_id;   // Temporal safety token
} aria_fat_pointer_t;

// =============================================================================
// Lifecycle Management (Allocation Wrappers)
// =============================================================================

/**
 * Initializes the fat pointer runtime systems (locks, registry).
 * Must be called before any fat pointer allocation.
 */
void aria_fat_init(void);

/**
 * Allocates memory using the underlying allocator (mimalloc) and registers
 * a new temporal safety ID.
 * 
 * @param size  Bytes to allocate.
 * @return      A generic fat pointer.
 */
aria_fat_pointer_t aria_fat_alloc(size_t size);

/**
 * Reallocates memory.
 * If the memory is moved, a new alloc_id is generated and the old one is invalidated.
 * If expanded in place, the old ID might remain valid (implementation dependent).
 * 
 * @param ptr       The source fat pointer.
 * @param new_size  The new desired size.
 * @return          The new fat pointer.
 */
aria_fat_pointer_t aria_fat_realloc(aria_fat_pointer_t ptr, size_t new_size);

/**
 * Frees memory and invalidates the associated alloc_id.
 * Performs a temporal check before freeing to catch Double-Free.
 * 
 * @param ptr   The fat pointer to free.
 */
void aria_fat_free(aria_fat_pointer_t ptr);

// =============================================================================
// Safety Checks (Codegen Intrinsics)
// =============================================================================

/**
 * Verifies that a memory access is spatially and temporally valid.
 * This is the primary hot-path check function.
 * 
 * @param ptr           The fat pointer being accessed.
 * @param access_size   The width of the access in bytes (e.g., 4 for int32).
 */
void aria_fat_check_bounds(aria_fat_pointer_t ptr, size_t access_size);

/**
 * Explicit temporal check only.
 * Useful for verifying handles or weak references.
 */
void aria_fat_check_temporal(aria_fat_pointer_t ptr);

/**
 * Pointer arithmetic helper.
 * Performs the addition/subtraction and returns a new fat pointer.
 * Checks for overflow of the pointer arithmetic itself.
 * 
 * @param ptr       Source fat pointer.
 * @param offset    Byte offset to apply (signed).
 */
aria_fat_pointer_t aria_fat_ptr_add(aria_fat_pointer_t ptr, int64_t offset);

// =============================================================================
// Debugging
// =============================================================================

/**
 * Dumps the state of a fat pointer to stderr.
 */
void aria_fat_debug_print(aria_fat_pointer_t ptr);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_SAFETY_FAT_POINTER_H

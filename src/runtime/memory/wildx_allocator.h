/**
 * src/runtime/memory/wildx_allocator.h
 * 
 * Wildx Allocator: Executable Memory for JIT Compilation
 * Public Interface
 */

#ifndef ARIA_RUNTIME_WILDX_ALLOCATOR_H
#define ARIA_RUNTIME_WILDX_ALLOCATOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allocate executable-capable memory (initial state: RW)
 * @param size: Number of bytes to allocate
 * @return: Pointer to allocated memory, or NULL on failure
 */
void* aria_alloc_exec(size_t size);

/**
 * Deallocate executable memory
 * @param ptr: Pointer returned by aria_alloc_exec
 * @param size: Original size passed to aria_alloc_exec
 */
void aria_free_exec(void* ptr, size_t size);

/**
 * Transition memory to executable (RX) state
 * @param ptr: Pointer to memory allocated with aria_alloc_exec
 * @param size: Size of the region to protect
 * @return: 0 on success, -1 on failure
 */
int aria_mem_protect_exec(void* ptr, size_t size);

/**
 * Transition memory to writable (RW) state  
 * @param ptr: Pointer to memory allocated with aria_alloc_exec
 * @param size: Size of the region to protect
 * @return: 0 on success, -1 on failure
 */
int aria_mem_protect_write(void* ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_WILDX_ALLOCATOR_H

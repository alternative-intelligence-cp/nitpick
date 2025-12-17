/**
 * Aria Wild/WildX Memory Allocators
 * 
 * This header defines the manual memory management subsystem for Aria.
 * It provides three allocation strategies:
 * 
 * 1. Wild: Manual malloc/free-style allocation (unmanaged heap)
 * 2. WildX: Executable memory for JIT compilation (W⊕X security model)
 * 3. Specialized: Buffer, string, and array allocators
 * 
 * Reference: research_022_wild_wildx_memory.txt, research_023_runtime_assembler.txt
 */

#ifndef ARIA_RUNTIME_ALLOCATORS_H
#define ARIA_RUNTIME_ALLOCATORS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Wild Memory Allocator (Manual Heap)
// =============================================================================

/**
 * Allocate unmanaged memory from the wild heap
 * 
 * This is Aria's equivalent to malloc(). Memory is NOT tracked by the GC
 * and must be manually freed via aria_free().
 * 
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 * 
 * Safety: The returned pointer is opaque to the GC. Objects allocated
 * via aria_alloc can contain references to GC objects, but those GC
 * objects must be pinned (# operator) to prevent collection.
 * 
 * Usage:
 *   wild int64:data = aria_alloc(sizeof(int64)) ? NULL;
 *   defer aria_free(data);  // RAII cleanup
 */
void* aria_alloc(size_t size);

/**
 * Free wild memory
 * 
 * @param ptr Pointer returned by aria_alloc (or NULL)
 * 
 * Safety:
 * - Double free: Undefined behavior (use defer to prevent)
 * - Use after free: Undefined behavior (Borrow Checker detects)
 * - Freeing NULL: Safe no-op
 */
void aria_free(void* ptr);

/**
 * Reallocate wild memory
 * 
 * Attempts to resize the allocation. May move the memory block.
 * 
 * @param ptr Existing allocation (or NULL for new allocation)
 * @param new_size New size in bytes
 * @return New pointer (may differ from ptr), or NULL on failure
 * 
 * Critical: If reallocation succeeds, the old pointer is INVALID.
 * Always update: ptr = aria_realloc(ptr, new_size);
 * 
 * If reallocation fails, the original pointer remains valid.
 */
void* aria_realloc(void* ptr, size_t new_size);

// =============================================================================
// Specialized Allocators
// =============================================================================

/**
 * Allocate buffer with alignment and initialization options
 * 
 * @param size Buffer size in bytes
 * @param alignment Power of 2 alignment (0 = default, typically 8 or 16)
 * @param zero_init If true, zero-initialize the buffer
 * @return Allocated buffer, or NULL on failure
 * 
 * Use case: Arena allocators, I/O buffers, custom data structures
 */
void* aria_alloc_buffer(size_t size, size_t alignment, bool zero_init);

/**
 * Allocate memory for string data
 * 
 * Allocates size + 1 bytes to accommodate null terminator.
 * 
 * @param size String length (excluding null terminator)
 * @return Allocated string buffer, or NULL on failure
 */
char* aria_alloc_string(size_t size);

/**
 * Allocate array memory
 * 
 * @param elem_size Size of each element
 * @param count Number of elements
 * @return Allocated array, or NULL on failure (or overflow)
 * 
 * Safety: Checks for size_t overflow (elem_size * count)
 */
void* aria_alloc_array(size_t elem_size, size_t count);

// =============================================================================
// WildX Executable Memory (JIT Support)
// =============================================================================

/**
 * WildX Memory State Machine
 * 
 * Enforces W⊕X (Write XOR Execute) security invariant:
 * Memory can be writable OR executable, but NEVER both.
 */
typedef enum {
    WILDX_STATE_UNINITIALIZED = 0,  // Invalid state
    WILDX_STATE_WRITABLE = 1,        // RW, NX (can write opcodes)
    WILDX_STATE_EXECUTABLE = 2,      // RX, RO (can execute code)
    WILDX_STATE_FREED = 3            // Invalid state
} WildXState;

/**
 * WildX Guard: RAII wrapper for executable memory
 * 
 * Manages the lifecycle and state transitions of JIT-compiled code.
 */
typedef struct {
    void* ptr;              // Memory pointer (page-aligned)
    size_t size;            // Allocation size (bytes)
    WildXState state;       // Current state in W⊕X machine
    bool sealed;            // Has seal() been called?
} WildXGuard;

/**
 * Allocate executable memory (initial state: WRITABLE)
 * 
 * Allocates page-aligned memory with RW permissions (NOT executable).
 * This is the "construction zone" where JIT code can be written.
 * 
 * @param size Number of bytes (rounded up to page size)
 * @return Guard structure, or {NULL, 0, UNINITIALIZED} on failure
 * 
 * Platform: Uses mmap (POSIX) or VirtualAlloc (Windows)
 * Alignment: Guaranteed page-aligned (typically 4KB)
 * 
 * Security: Memory is NOT executable until sealed.
 */
WildXGuard aria_alloc_exec(size_t size);

/**
 * Seal executable memory (transition: WRITABLE → EXECUTABLE)
 * 
 * Flips memory protection from RW to RX, making the code executable
 * but immutable. This is a one-way transition.
 * 
 * @param guard Pointer to WildXGuard structure
 * @return 0 on success, -1 on failure
 * 
 * Process:
 * 1. Flush CPU caches (I-cache / D-cache coherency)
 * 2. Call mprotect (POSIX) or VirtualProtect (Windows)
 * 3. Update guard state to EXECUTABLE
 * 
 * After sealing:
 * - Code can be executed via function pointer cast
 * - Any write attempt triggers SIGSEGV (hardware protection)
 * 
 * Security: Prevents JIT-spray attacks by eliminating RWX window
 */
int aria_mem_protect_exec(WildXGuard* guard);

/**
 * Free executable memory
 * 
 * @param guard Pointer to WildXGuard structure
 * 
 * Deallocates the page-aligned memory. Sets guard to FREED state.
 * Idempotent: Safe to call multiple times.
 */
void aria_free_exec(WildXGuard* guard);

/**
 * Execute JIT-compiled code
 * 
 * Casts the memory to a function pointer and invokes it.
 * 
 * @param guard Guard structure (must be in EXECUTABLE state)
 * @param args Opaque pointer to arguments (cast by generated code)
 * @return Opaque pointer to result (cast by caller)
 * 
 * Safety: The guard must be sealed (EXECUTABLE state). Otherwise,
 * this function returns NULL without executing.
 * 
 * Typical usage:
 *   WildXGuard g = aria_alloc_exec(4096);
 *   // ... write opcodes to g.ptr ...
 *   aria_mem_protect_exec(&g);
 *   typedef int64_t (*jit_func_t)(int64_t);
 *   jit_func_t func = (jit_func_t)g.ptr;
 *   int64_t result = func(42);
 */
void* aria_exec_jit(WildXGuard* guard, void* args);

// =============================================================================
// Memory Diagnostics
// =============================================================================

/**
 * Get allocator statistics
 */
typedef struct {
    size_t total_wild_allocated;      // Total wild heap usage
    size_t total_wildx_allocated;     // Total executable memory
    size_t num_wild_allocations;      // Active wild allocations
    size_t num_wildx_allocations;     // Active wildx allocations
    size_t peak_wild_usage;           // Peak wild memory
    size_t peak_wildx_usage;          // Peak wildx memory
} AllocatorStats;

void aria_allocator_get_stats(AllocatorStats* stats);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_ALLOCATORS_H

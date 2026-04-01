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
#include "result.h"

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
// Result-Based Allocations (Phase 4.2)
// =============================================================================

/**
 * Allocate memory with result-based error handling
 * 
 * Returns AriaAllocResult with rich error context instead of NULL.
 * 
 * @param size Number of bytes to allocate
 * @return Result containing pointer or error details
 * 
 * Advantages over aria_alloc():
 * - Distinguishes out-of-memory from invalid size
 * - Provides diagnostic info (requested size/alignment)
 * - Type-safe error codes (not generic NULL)
 * 
 * Error cases:
 * - ARIA_ALLOC_ERR_INVALID_SIZE: size == 0
 * - ARIA_ALLOC_ERR_OUT_OF_MEMORY: System allocator failed
 */
AriaAllocResult aria_alloc_result(size_t size);

/**
 * Allocate array with result-based error handling
 * 
 * @param elem_size Size of each element
 * @param count Number of elements
 * @return Result containing pointer or error details
 * 
 * Error cases:
 * - ARIA_ALLOC_ERR_INVALID_SIZE: elem_size or count is 0
 * - ARIA_ALLOC_ERR_SIZE_OVERFLOW: elem_size * count overflows size_t
 * - ARIA_ALLOC_ERR_OUT_OF_MEMORY: System allocator failed
 */
AriaAllocResult aria_alloc_array_result(size_t elem_size, size_t count);

/**
 * Allocate aligned memory with result-based error handling
 * 
 * @param size Number of bytes to allocate
 * @param alignment Power-of-2 alignment requirement
 * @return Result containing pointer or error details
 * 
 * Error cases:
 * - ARIA_ALLOC_ERR_INVALID_SIZE: size == 0
 * - ARIA_ALLOC_ERR_INVALID_ALIGNMENT: alignment not power of 2
 * - ARIA_ALLOC_ERR_OUT_OF_MEMORY: System allocator failed
 */
AriaAllocResult aria_alloc_aligned_result(size_t size, size_t alignment);

/**
 * Allocate buffer with result-based error handling
 * 
 * @param size Buffer size in bytes
 * @param alignment Power of 2 alignment (0 = default)
 * @param zero_init If true, zero-initialize the buffer
 * @return Result containing pointer or error details
 */
AriaAllocResult aria_alloc_buffer_result(size_t size, size_t alignment, bool zero_init);

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
    size_t size;            // Allocation size (bytes, user-requested rounded to page)
    size_t map_size;        // Total mmap size (includes guard pages if enabled)
    WildXState state;       // Current state in W⊕X machine
    bool sealed;            // Has seal() been called?
    uint64_t code_hash;     // v0.7.1: FNV-1a hash of code at seal time (0 = unset)
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
    size_t num_wild_frees;            // Total wild frees (v0.7.0)
    size_t num_wildx_allocations;     // Active wildx allocations
    size_t num_wildx_frees;           // Total wildx frees (v0.7.1)
    size_t peak_wild_usage;           // Peak wild memory
    size_t peak_wildx_usage;          // Peak wildx memory
} AllocatorStats;

void aria_allocator_get_stats(AllocatorStats* stats);

/**
 * v0.7.0: Enable/disable guard pages around wild allocations
 * 
 * When enabled, each aria_alloc() call places PROT_NONE guard pages
 * before and after the data region. Any buffer overflow/underflow
 * immediately triggers SIGSEGV instead of silent corruption.
 * 
 * Overhead: ~8KB per allocation (2 guard pages). Use for debugging only.
 * Linux only; no-op on other platforms.
 */
void aria_wild_enable_guard_pages(bool enable);

/**
 * v0.7.0: Print wild memory statistics dashboard to stderr
 * 
 * Shows current/peak usage, active allocations, leak count, quota.
 * Designed to be called at program exit via --wild-stats flag.
 */
void aria_wild_print_stats(void);

/**
 * v0.7.0: Get the allocation size of a wild pointer
 * 
 * Uses the hidden header to retrieve the user-requested size.
 * Returns 0 if ptr is NULL or not a valid wild allocation.
 */
size_t aria_alloc_get_size(void* ptr);

/**
 * v0.7.0: Enable automatic stats printing at program exit
 * 
 * When enabled, aria_wild_print_stats() is called automatically
 * after main() returns (via __attribute__((destructor))).
 */
void aria_wild_enable_stats_at_exit(bool enable);

// =============================================================================
// v0.7.1: WildX Security Hardening
// =============================================================================

/**
 * Enable guard pages (PROT_NONE sentinels) around WildX executable regions.
 * Buffer overflow/underflow in JIT code triggers immediate SIGSEGV.
 * Linux only; no-op on other platforms.
 */
void aria_wildx_enable_guard_pages(bool enable);

/**
 * Enable WildX audit logging to stderr.
 * Logs ALLOC, SEAL, EXEC, FREE, TAMPER events with pointers, sizes, hashes.
 */
void aria_wildx_enable_audit(bool enable);

/**
 * Set the WildX executable memory quota in bytes (default: 64MB).
 * Allocations that would exceed the quota return a failed guard.
 */
void aria_wildx_set_quota(size_t bytes);

/**
 * Compute and return the FNV-1a hash of a WildX guard's code region.
 * Useful for manual integrity verification.
 * Returns 0 if guard is invalid or freed.
 */
uint64_t aria_wildx_verify_hash(WildXGuard* guard);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_ALLOCATORS_H

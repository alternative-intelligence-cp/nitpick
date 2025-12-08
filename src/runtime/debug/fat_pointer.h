/**
 * src/runtime/debug/fat_pointer.h
 * 
 * Aria Runtime - Fat Pointers for Debug Builds
 * Version: 0.0.7
 * 
 * Fat pointers augment raw pointers with scope metadata for runtime
 * validation in debug builds. This prevents dangling pointer dereferences
 * and use-after-scope bugs.
 * 
 * Security Model:
 * - Each scope gets a unique monotonic ID at runtime
 * - ADDRESS_OF (@) operator creates fat pointers with current scope ID
 * - Dereference checks if scope ID is still valid
 * - Scope invalidation occurs at scope exit
 * 
 * Zero-Cost Abstraction:
 * - Debug builds: Full fat pointer instrumentation
 * - Release builds: Compiles to raw pointers (zero overhead)
 * 
 * Dependencies:
 * - stdint.h (uint64_t)
 * - stdbool.h (bool)
 */

#ifndef ARIA_RUNTIME_DEBUG_FAT_POINTER_H
#define ARIA_RUNTIME_DEBUG_FAT_POINTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================

// Enable fat pointers only in debug builds
// Define ARIA_DEBUG or DEBUG to enable, or explicitly set ARIA_FAT_POINTERS
#if defined(ARIA_DEBUG) || defined(DEBUG) || defined(ARIA_FAT_POINTERS)
    #define ARIA_FAT_POINTERS_ENABLED 1
#else
    #define ARIA_FAT_POINTERS_ENABLED 0
#endif

// =============================================================================
// Fat Pointer Structure
// =============================================================================

/**
 * FatPointer: Pointer with scope metadata
 * 
 * Layout (debug builds):
 * - ptr: The actual memory address (void*)
 * - scope_id: Unique identifier for the allocation scope (uint64_t)
 * - alloc_timestamp: Monotonic timestamp when pointer was created (uint64_t)
 * 
 * Layout (release builds):
 * - Just a raw pointer (sizeof(void*))
 * 
 * Size:
 * - Debug: 24 bytes (ptr + scope_id + timestamp)
 * - Release: 8 bytes (ptr only)
 */
#if ARIA_FAT_POINTERS_ENABLED

typedef struct {
    void* ptr;                  // Raw pointer (8 bytes)
    uint64_t scope_id;          // Scope identifier (8 bytes)
    uint64_t alloc_timestamp;   // Allocation timestamp (8 bytes)
} aria_fat_pointer_t;

#else

// In release builds, fat pointers are just raw pointers
typedef void* aria_fat_pointer_t;

#endif

// =============================================================================
// Scope Management
// =============================================================================

/**
 * Enter a new scope
 * Returns: Unique scope ID for this scope
 * 
 * Debug: Allocates new scope ID, pushes to active scope stack
 * Release: No-op, returns 0
 */
uint64_t aria_scope_enter(void);

/**
 * Exit current scope (invalidate all pointers with this scope ID)
 * 
 * Debug: Marks scope as invalid, prevents future dereferences
 * Release: No-op
 */
void aria_scope_exit(uint64_t scope_id);

/**
 * Check if a scope is still valid
 * Returns: true if scope is active, false if exited/invalid
 * 
 * Debug: Looks up scope in active scope table
 * Release: Always returns true (no checks)
 */
bool aria_scope_is_valid(uint64_t scope_id);

// =============================================================================
// Fat Pointer Operations
// =============================================================================

/**
 * Create a fat pointer from a raw pointer
 * 
 * Debug: Captures current scope ID and timestamp
 * Release: Returns raw pointer unchanged
 */
aria_fat_pointer_t aria_fat_ptr_create(void* raw_ptr, uint64_t scope_id);

/**
 * Dereference a fat pointer (with safety checks)
 * 
 * Debug: Validates scope before returning pointer
 * Release: Returns pointer directly (no checks)
 * 
 * Safety: Aborts program if scope is invalid (debug builds)
 */
void* aria_fat_ptr_deref(aria_fat_pointer_t fat_ptr);

/**
 * Extract raw pointer without safety checks (unsafe)
 * Use only when scope validity is guaranteed by other means
 * 
 * Debug: Returns pointer without validation (dangerous!)
 * Release: Returns pointer directly
 */
void* aria_fat_ptr_raw(aria_fat_pointer_t fat_ptr);

/**
 * Check if a fat pointer is valid (non-destructive check)
 * Returns: true if safe to dereference, false if dangling
 * 
 * Debug: Validates scope without aborting
 * Release: Always returns true
 */
bool aria_fat_ptr_is_valid(aria_fat_pointer_t fat_ptr);

// =============================================================================
// Debug Information
// =============================================================================

#if ARIA_FAT_POINTERS_ENABLED

/**
 * Print fat pointer debug info to stderr
 * Format: FatPtr{ptr=0x..., scope=..., timestamp=..., valid=...}
 */
void aria_fat_ptr_debug(aria_fat_pointer_t fat_ptr);

/**
 * Get statistics about scope tracking
 * Returns: Number of currently active scopes
 */
uint64_t aria_scope_get_active_count(void);

/**
 * Reset scope tracking (for testing/debugging only)
 * WARNING: Invalidates ALL existing fat pointers
 */
void aria_scope_reset(void);

#endif

// =============================================================================
// Compiler Integration Helpers
// =============================================================================

/**
 * Macro for creating fat pointers in compiler-generated code
 * Usage: FAT_PTR(&local_var, current_scope_id)
 */
#if ARIA_FAT_POINTERS_ENABLED
    #define ARIA_FAT_PTR(ptr, scope) aria_fat_ptr_create((ptr), (scope))
    #define ARIA_DEREF_FAT_PTR(fat_ptr) aria_fat_ptr_deref(fat_ptr)
#else
    #define ARIA_FAT_PTR(ptr, scope) (ptr)
    #define ARIA_DEREF_FAT_PTR(fat_ptr) (fat_ptr)
#endif

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_DEBUG_FAT_POINTER_H

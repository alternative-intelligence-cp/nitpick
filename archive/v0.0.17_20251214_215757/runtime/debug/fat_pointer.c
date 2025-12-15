/**
 * src/runtime/debug/fat_pointer.c
 * 
 * Aria Runtime - Fat Pointer Implementation
 * Version: 0.0.7
 * 
 * Implements scope tracking and fat pointer validation for debug builds.
 * 
 * Implementation Strategy:
 * - Monotonic scope ID counter (thread-local for concurrency)
 * - Active scope bit set (uint64_t bitmap for fast lookups)
 * - Scope stack tracking for proper nesting
 * 
 * Performance:
 * - Scope enter/exit: O(1) - just counter increment and bit set
 * - Scope validation: O(1) - bit check in active scope set
 * - Memory overhead: ~8 KB for scope tracking (up to 512 active scopes)
 */

#include "fat_pointer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// =============================================================================
// Configuration Constants
// =============================================================================

// Maximum number of simultaneously active scopes (reasonable for typical programs)
#define MAX_ACTIVE_SCOPES 512

// Scope ID 0 is reserved for "invalid/uninitialized"
#define INVALID_SCOPE_ID 0

// =============================================================================
// Global State (Thread-Local in Production)
// =============================================================================

#if ARIA_FAT_POINTERS_ENABLED

// Monotonic scope ID counter
static uint64_t g_next_scope_id = 1;

// Active scope tracking (bit set for fast O(1) lookup)
// Bit i is set if scope ID i is currently active
// Supports up to 512 scopes (512 bits = 64 bytes)
static uint64_t g_active_scopes[MAX_ACTIVE_SCOPES / 64] = {0};

// Scope stack for LIFO validation
static uint64_t g_scope_stack[MAX_ACTIVE_SCOPES];
static size_t g_scope_stack_top = 0;

// Statistics
static uint64_t g_total_scopes_created = 0;
static uint64_t g_total_violations_detected = 0;

#endif

// =============================================================================
// Internal Helpers
// =============================================================================

#if ARIA_FAT_POINTERS_ENABLED

/**
 * Mark a scope ID as active
 */
static inline void scope_activate(uint64_t scope_id) {
    if (scope_id == INVALID_SCOPE_ID || scope_id >= MAX_ACTIVE_SCOPES) {
        return;  // Invalid scope ID
    }
    
    size_t word_idx = scope_id / 64;
    size_t bit_idx = scope_id % 64;
    g_active_scopes[word_idx] |= (1ULL << bit_idx);
}

/**
 * Mark a scope ID as inactive
 */
static inline void scope_deactivate(uint64_t scope_id) {
    if (scope_id == INVALID_SCOPE_ID || scope_id >= MAX_ACTIVE_SCOPES) {
        return;  // Invalid scope ID
    }
    
    size_t word_idx = scope_id / 64;
    size_t bit_idx = scope_id % 64;
    g_active_scopes[word_idx] &= ~(1ULL << bit_idx);
}

/**
 * Check if a scope ID is active (O(1) bit check)
 */
static inline bool scope_is_active(uint64_t scope_id) {
    if (scope_id == INVALID_SCOPE_ID || scope_id >= MAX_ACTIVE_SCOPES) {
        return false;  // Invalid scope IDs are never active
    }
    
    size_t word_idx = scope_id / 64;
    size_t bit_idx = scope_id % 64;
    return (g_active_scopes[word_idx] & (1ULL << bit_idx)) != 0;
}

/**
 * Get current monotonic timestamp (for debugging/logging)
 */
static inline uint64_t get_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#endif

// =============================================================================
// Public API Implementation
// =============================================================================

uint64_t aria_scope_enter(void) {
#if ARIA_FAT_POINTERS_ENABLED
    // Allocate new scope ID
    uint64_t scope_id = g_next_scope_id++;
    
    // Wrap around if we exceed MAX_ACTIVE_SCOPES
    // (This should be rare in well-structured code)
    if (scope_id >= MAX_ACTIVE_SCOPES) {
        fprintf(stderr, "[ARIA FAT PTR] WARNING: Scope ID exceeded MAX_ACTIVE_SCOPES, wrapping around\n");
        g_next_scope_id = 1;
        scope_id = g_next_scope_id++;
    }
    
    // Mark scope as active
    scope_activate(scope_id);
    
    // Push to scope stack
    if (g_scope_stack_top < MAX_ACTIVE_SCOPES) {
        g_scope_stack[g_scope_stack_top++] = scope_id;
    } else {
        fprintf(stderr, "[ARIA FAT PTR] ERROR: Scope stack overflow\n");
        abort();
    }
    
    // Statistics
    g_total_scopes_created++;
    
    return scope_id;
#else
    return 0;  // No-op in release builds
#endif
}

void aria_scope_exit(uint64_t scope_id) {
#if ARIA_FAT_POINTERS_ENABLED
    // Validate scope stack (should match LIFO order)
    if (g_scope_stack_top == 0) {
        fprintf(stderr, "[ARIA FAT PTR] ERROR: Scope stack underflow in aria_scope_exit\n");
        abort();
    }
    
    uint64_t expected_scope = g_scope_stack[--g_scope_stack_top];
    if (expected_scope != scope_id) {
        fprintf(stderr, "[ARIA FAT PTR] ERROR: Scope exit mismatch (expected %lu, got %lu)\n",
                expected_scope, scope_id);
        fprintf(stderr, "  This indicates incorrect scope nesting in generated code.\n");
        // Continue anyway (non-fatal, but indicates compiler bug)
    }
    
    // Deactivate scope
    scope_deactivate(scope_id);
#else
    (void)scope_id;  // Unused in release builds
#endif
}

bool aria_scope_is_valid(uint64_t scope_id) {
#if ARIA_FAT_POINTERS_ENABLED
    return scope_is_active(scope_id);
#else
    (void)scope_id;
    return true;  // Always valid in release builds
#endif
}

aria_fat_pointer_t aria_fat_ptr_create(void* raw_ptr, uint64_t scope_id) {
#if ARIA_FAT_POINTERS_ENABLED
    aria_fat_pointer_t fat_ptr;
    fat_ptr.ptr = raw_ptr;
    fat_ptr.scope_id = scope_id;
    fat_ptr.alloc_timestamp = get_timestamp();
    return fat_ptr;
#else
    (void)scope_id;  // Unused in release builds
    return raw_ptr;
#endif
}

void* aria_fat_ptr_deref(aria_fat_pointer_t fat_ptr) {
#if ARIA_FAT_POINTERS_ENABLED
    // Validate scope before dereferencing
    if (!scope_is_active(fat_ptr.scope_id)) {
        fprintf(stderr, "\n*** DANGLING POINTER DETECTED ***\n");
        fprintf(stderr, "Attempted to dereference pointer from exited scope\n");
        fprintf(stderr, "  Pointer: %p\n", fat_ptr.ptr);
        fprintf(stderr, "  Scope ID: %lu (INVALID - scope has exited)\n", fat_ptr.scope_id);
        fprintf(stderr, "  Allocated at timestamp: %lu\n", fat_ptr.alloc_timestamp);
        fprintf(stderr, "This is a use-after-scope bug.\n");
        fprintf(stderr, "*** END DANGLING POINTER VIOLATION ***\n\n");
        
        g_total_violations_detected++;
        
        // Abort program in debug builds (fail-fast)
        abort();
    }
    
    return fat_ptr.ptr;
#else
    return fat_ptr;  // Just return raw pointer in release builds
#endif
}

void* aria_fat_ptr_raw(aria_fat_pointer_t fat_ptr) {
#if ARIA_FAT_POINTERS_ENABLED
    return fat_ptr.ptr;
#else
    return fat_ptr;
#endif
}

bool aria_fat_ptr_is_valid(aria_fat_pointer_t fat_ptr) {
#if ARIA_FAT_POINTERS_ENABLED
    return scope_is_active(fat_ptr.scope_id);
#else
    (void)fat_ptr;
    return true;
#endif
}

// =============================================================================
// Debug Information
// =============================================================================

#if ARIA_FAT_POINTERS_ENABLED

void aria_fat_ptr_debug(aria_fat_pointer_t fat_ptr) {
    bool valid = scope_is_active(fat_ptr.scope_id);
    fprintf(stderr, "FatPtr{ptr=%p, scope=%lu, timestamp=%lu, valid=%s}\n",
            fat_ptr.ptr,
            fat_ptr.scope_id,
            fat_ptr.alloc_timestamp,
            valid ? "YES" : "NO");
}

uint64_t aria_scope_get_active_count(void) {
    return g_scope_stack_top;
}

void aria_scope_reset(void) {
    // Reset all state (for testing only)
    g_next_scope_id = 1;
    memset(g_active_scopes, 0, sizeof(g_active_scopes));
    g_scope_stack_top = 0;
    
    fprintf(stderr, "[ARIA FAT PTR] Scope tracking reset\n");
    fprintf(stderr, "  Total scopes created: %lu\n", g_total_scopes_created);
    fprintf(stderr, "  Total violations detected: %lu\n", g_total_violations_detected);
    
    g_total_scopes_created = 0;
    g_total_violations_detected = 0;
}

#endif

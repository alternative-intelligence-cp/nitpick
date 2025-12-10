/**
 * src/runtime/safety/fat_pointer.c
 * Implementation of WP 004.3
 */

#include "fat_pointer.h"
#include "../memory/allocator.h"         //  Existing mimalloc interface
#include "../debug/stacktrace.h"  //  Existing stack trace system
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>
#include <inttypes.h>

// =============================================================================
// Temporal Safety Registry
// =============================================================================

// The Global Monotonic Counter for Allocation IDs.
// Starts at 1. ID 0 is reserved for NULL/Invalid.
static atomic_uint_fast64_t global_alloc_counter = 1;

// Hash Table Configuration
// We use a fixed-size bucket array with linked lists (separate chaining).
// 64k buckets reduces collisions.
#define REGISTRY_BUCKETS 65536
#define REGISTRY_MASK (REGISTRY_BUCKETS - 1)

typedef struct registry_node {
    uint64_t alloc_id;
    struct registry_node* next;
} registry_node_t;

// The Registry
static registry_node_t* registry[REGISTRY_BUCKETS];

// Bucket Locks
// We use simple spinlocks (atomic_flag) for bucket-level locking.
// This allows high concurrency as threads accessing different ID ranges won't block each other.
static atomic_flag bucket_locks[REGISTRY_BUCKETS];

// Helper: Acquire lock for a bucket
static void lock_bucket(size_t index) {
    while (atomic_flag_test_and_set_explicit(&bucket_locks[index], memory_order_acquire)) {
        // Busy wait / Spin
        #if defined(__x86_64__) || defined(_M_X64)
            __asm__ volatile("pause");
        #endif
    }
}

// Helper: Release lock for a bucket
static void unlock_bucket(size_t index) {
    atomic_flag_clear_explicit(&bucket_locks[index], memory_order_release);
}

// Internal: Register a new ID as live
static void register_alloc_id(uint64_t id) {
    if (id == 0) return;
    size_t idx = id & REGISTRY_MASK;
    
    // Allocate node (using raw system malloc to avoid recursion into aria_alloc if it were instrumented)
    // However, since aria_alloc is the "user" allocator, we can use standard malloc here safely.
    registry_node_t* node = (registry_node_t*)malloc(sizeof(registry_node_t));
    if (!node) abort(); // Catastrophic OOM in safety system
    
    node->alloc_id = id;
    
    lock_bucket(idx);
    node->next = registry[idx];
    registry[idx] = node;
    unlock_bucket(idx);
}

// Internal: Remove an ID (Mark as dead)
// Returns true if found and removed, false if not found (Double Free case)
static bool unregister_alloc_id(uint64_t id) {
    if (id == 0) return false;
    size_t idx = id & REGISTRY_MASK;
    bool found = false;
    
    lock_bucket(idx);
    registry_node_t** curr = &registry[idx];
    while (*curr) {
        if ((*curr)->alloc_id == id) {
            registry_node_t* to_free = *curr;
            *curr = to_free->next;
            free(to_free);
            found = true;
            break;
        }
        curr = &(*curr)->next;
    }
    unlock_bucket(idx);
    return found;
}

// Internal: Check if ID is live
static bool is_alloc_id_valid(uint64_t id) {
    if (id == 0) return false;
    size_t idx = id & REGISTRY_MASK;
    bool valid = false;
    
    lock_bucket(idx);
    registry_node_t* curr = registry[idx];
    while (curr) {
        if (curr->alloc_id == id) {
            valid = true;
            break;
        }
        curr = curr->next;
    }
    unlock_bucket(idx);
    return valid;
}

// =============================================================================
// Error Reporting (Stack Trace Integration)
// =============================================================================

// ANSI Color Codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_CYAN    "\033[1;36m"

static void panic_with_context(const char* error_type, const char* message, aria_fat_pointer_t ptr) {
    // 1. Lock stderr
    flockfile(stderr);
    
    // 2. Print Violation Type and Details
    fprintf(stderr, "\n");
    fprintf(stderr, "%s=== %s ===%s\n", COLOR_RED, error_type, COLOR_RESET);
    fprintf(stderr, "%s%s%s\n\n", COLOR_YELLOW, message, COLOR_RESET);
    
    // 3. Dump Fat Pointer Metadata
    fprintf(stderr, "%sFat Pointer Details:%s\n", COLOR_CYAN, COLOR_RESET);
    fprintf(stderr, "  Base Address:   %p\n", ptr.base);
    fprintf(stderr, "  Current Ptr:    %p\n", ptr.ptr);
    fprintf(stderr, "  Allocation Size: %zu bytes\n", ptr.size);
    fprintf(stderr, "  Allocation ID:  %" PRIu64 "\n", ptr.alloc_id);
    
    if (ptr.base && ptr.ptr) {
        ptrdiff_t offset = (char*)ptr.ptr - (char*)ptr.base;
        fprintf(stderr, "  Current Offset: %td bytes\n\n", offset);
    } else {
        fprintf(stderr, "\n");
    }
    
    // 4. Capture and Print Stack Trace
    fprintf(stderr, "%sStack Trace:%s\n", COLOR_CYAN, COLOR_RESET);
    aria_stacktrace_t trace;
    // Skip frames: panic_with_context, aria_fat_check_bounds
    int captured = aria_capture_stacktrace(&trace, 2); 
    
    if (captured > 0) {
        aria_print_stacktrace(&trace, 1); // 1 = use color
    } else {
        fprintf(stderr, "Failed to capture stack trace.\n");
    }
    
    // 5. Abort
    fprintf(stderr, "\n%sProcess Terminated by Aria Runtime Safety.%s\n", COLOR_RED, COLOR_RESET);
    funlockfile(stderr);
    abort();
}

// =============================================================================
// Public API Implementation
// =============================================================================

void aria_fat_init(void) {
    // Clear locks and registry
    for (int i = 0; i < REGISTRY_BUCKETS; i++) {
        atomic_flag_clear(&bucket_locks[i]);
        registry[i] = NULL;
    }
}

aria_fat_pointer_t aria_fat_alloc(size_t size) {
    // 1. Delegate to underlying mimalloc allocator
    void* raw_ptr = aria_alloc(size);
    
    if (!raw_ptr) {
        return (aria_fat_pointer_t){0}; // Null fat pointer
    }
    
    // 2. Generate new ID
    uint64_t id = atomic_fetch_add(&global_alloc_counter, 1);
    
    // 3. Register ID
    register_alloc_id(id);
    
    // 4. Construct Fat Pointer
    aria_fat_pointer_t fp;
    fp.ptr = raw_ptr;
    fp.base = raw_ptr;
    fp.size = size;
    fp.alloc_id = id;
    
    return fp;
}

aria_fat_pointer_t aria_fat_realloc(aria_fat_pointer_t ptr, size_t new_size) {
    if (ptr.base == NULL) {
        // Realloc of NULL is equivalent to alloc
        return aria_fat_alloc(new_size);
    }
    
    // Temporal check
    if (!is_alloc_id_valid(ptr.alloc_id)) {
        panic_with_context("FATAL: USE-AFTER-FREE IN REALLOC", 
            "Attempted to realloc memory that has been deallocated.", ptr);
    }
    
    // Invalidate old ID
    unregister_alloc_id(ptr.alloc_id);
    
    // Perform realloc
    void* new_ptr = aria_realloc(ptr.base, new_size);
    
    if (!new_ptr) {
        // Re-register old ID on failure (restore state)
        register_alloc_id(ptr.alloc_id);
        return (aria_fat_pointer_t){0};
    }
    
    // Generate new ID
    uint64_t new_id = atomic_fetch_add(&global_alloc_counter, 1);
    register_alloc_id(new_id);
    
    // Calculate new ptr offset (preserve interior pointer position if possible)
    ptrdiff_t offset = (char*)ptr.ptr - (char*)ptr.base;
    void* new_current_ptr = (char*)new_ptr + (offset < (ptrdiff_t)new_size ? offset : 0);
    
    aria_fat_pointer_t new_fp;
    new_fp.ptr = new_current_ptr;
    new_fp.base = new_ptr;
    new_fp.size = new_size;
    new_fp.alloc_id = new_id;
    
    return new_fp;
}

void aria_fat_free(aria_fat_pointer_t ptr) {
    if (ptr.base == NULL) return;
    
    // 1. Temporal Check: Try to remove ID
    if (!unregister_alloc_id(ptr.alloc_id)) {
        panic_with_context("FATAL: DOUBLE FREE DETECTED", 
            "Attempted to free memory that has already been freed or was never allocated.", ptr);
    }
    
    // 2. Delegate to underlying mimalloc free
    aria_free(ptr.base);
}

void aria_fat_check_bounds(aria_fat_pointer_t ptr, size_t access_size) {
    // 1. Temporal Safety Check
    if (!is_alloc_id_valid(ptr.alloc_id)) {
        panic_with_context("FATAL: USE-AFTER-FREE", 
            "Attempted to access memory region that has been deallocated.", ptr);
    }

    uintptr_t p = (uintptr_t)ptr.ptr;
    uintptr_t base = (uintptr_t)ptr.base;
    size_t size = ptr.size;
    
    // 2. Lower Bound Check (Underflow)
    if (p < base) {
        panic_with_context("FATAL: BUFFER UNDERFLOW", 
            "Pointer has moved before the start of the allocation.", ptr);
    }
    
    // 3. Upper Bound Check (Overflow)
    // Check if (p + access_size) > (base + size)
    // Use subtraction to avoid overflow in the check itself
    if (p > base + size || (p - base) > (size - access_size)) {
        char msg[256];
        snprintf(msg, sizeof(msg), 
            "Access of %zu bytes at offset %ld exceeds allocation size of %zu.",
            access_size, (long)(p - base), size);
        panic_with_context("FATAL: BUFFER OVERFLOW", msg, ptr);
    }
}

void aria_fat_check_temporal(aria_fat_pointer_t ptr) {
    if (!is_alloc_id_valid(ptr.alloc_id)) {
        panic_with_context("FATAL: USE-AFTER-FREE", 
            "Attempted to access memory region that has been deallocated.", ptr);
    }
}

aria_fat_pointer_t aria_fat_ptr_add(aria_fat_pointer_t ptr, int64_t offset) {
    // We allow the pointer to go out of bounds during arithmetic (standard C behavior)
    // Checking is done at dereference time.
    // However, if strict math safety is desired, we could trap here.
    // For WP 004.3, we simply update the pointer.
    
    char* new_p = (char*)ptr.ptr + offset;
    ptr.ptr = (void*)new_p;
    return ptr;
}

void aria_fat_debug_print(aria_fat_pointer_t ptr) {
    fprintf(stderr, "FatPointer { base=%p, ptr=%p, size=%zu, id=%" PRIu64 " }\n", 
        ptr.base, ptr.ptr, ptr.size, ptr.alloc_id);
}

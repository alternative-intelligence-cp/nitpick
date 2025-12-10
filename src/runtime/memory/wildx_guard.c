/**
 * src/runtime/memory/wildx_guard.c
 * 
 * RAII Guard for WildX Memory - Implementation
 * Provides temporal window protection for W^X enforcement
 */

#include "wildx_guard.h"
#include "wildx_allocator.h"
#include <string.h>

/**
 * Create WildX guard and allocate RW memory
 */
WildXGuard wildx_guard_create(size_t size) {
    WildXGuard guard;
    
    // Initialize guard structure
    guard.ptr = NULL;
    guard.size = size;
    guard.state = WILDX_STATE_UNINITIALIZED;
    guard.sealed = false;
    
    // Allocate executable-capable memory (initial state: RW)
    guard.ptr = aria_alloc_exec(size);
    
    if (guard.ptr) {
        // Success - memory is allocated and writable
        guard.state = WILDX_STATE_WRITABLE;
    }
    
    return guard;
}

/**
 * Seal guard - transition RW -> RX
 */
int wildx_guard_seal(WildXGuard* guard) {
    // Validation checks
    if (!guard) return -1;
    if (!guard->ptr) return -1;
    if (guard->state != WILDX_STATE_WRITABLE) return -1;
    if (guard->sealed) return -1;  // Already sealed
    
    // Apply protection: RW -> RX
    int result = aria_mem_protect_exec(guard->ptr, guard->size);
    
    if (result == 0) {
        // Success - mark as sealed
        guard->state = WILDX_STATE_EXECUTABLE;
        guard->sealed = true;
    }
    
    return result;
}

/**
 * Destroy guard and free memory
 */
void wildx_guard_destroy(WildXGuard* guard) {
    if (!guard) return;
    
    // Free memory if allocated
    if (guard->ptr && guard->state != WILDX_STATE_FREED) {
        aria_free_exec(guard->ptr, guard->size);
    }
    
    // Reset guard structure
    guard->ptr = NULL;
    guard->size = 0;
    guard->state = WILDX_STATE_FREED;
    guard->sealed = false;
}

/**
 * Check if guard is in writable state
 */
bool wildx_guard_is_writable(const WildXGuard* guard) {
    if (!guard) return false;
    return (guard->state == WILDX_STATE_WRITABLE && !guard->sealed);
}

/**
 * Check if guard is sealed (executable)
 */
bool wildx_guard_is_sealed(const WildXGuard* guard) {
    if (!guard) return false;
    return guard->sealed;
}

/**
 * Get current state string (for debugging)
 */
const char* wildx_guard_state_string(const WildXGuard* guard) {
    if (!guard) return "NULL";
    
    switch (guard->state) {
        case WILDX_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case WILDX_STATE_WRITABLE:      return "WRITABLE";
        case WILDX_STATE_EXECUTABLE:    return "EXECUTABLE";
        case WILDX_STATE_FREED:         return "FREED";
        default:                        return "UNKNOWN";
    }
}

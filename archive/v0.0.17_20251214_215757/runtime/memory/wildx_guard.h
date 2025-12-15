/**
 * src/runtime/memory/wildx_guard.h
 * 
 * RAII Guard for WildX Memory
 * 
 * Ensures W^X (Write XOR Execute) enforcement by managing the
 * temporal window between allocation and protection transitions.
 * 
 * SECURITY ISSUE (Pre-v0.0.8):
 * Manual protect_exec() calls allowed a temporal window where
 * JIT memory remains RW (writable, not executable). If an attacker
 * can inject code during this window, they could exploit the delay
 * before protection is applied.
 * 
 * SOLUTION:
 * WildXGuard enforces automatic protection via RAII:
 * 1. Allocates RW memory in constructor
 * 2. Provides controlled write access via seal() method
 * 3. Automatically transitions to RX in destructor
 * 4. Prevents manual writes after sealing
 * 
 * USAGE:
 * ```c
 * WildXGuard guard = wildx_guard_create(4096);
 * if (!guard.ptr) { error handling }
 * 
 * // Write code during RW phase
 * memcpy(guard.ptr, opcodes, opcode_size);
 * 
 * // Seal - transitions to RX immediately
 * if (wildx_guard_seal(&guard) != 0) { error }
 * 
 * // Guard automatically frees in wildx_guard_destroy()
 * wildx_guard_destroy(&guard);
 * ```
 */

#ifndef ARIA_RUNTIME_WILDX_GUARD_H
#define ARIA_RUNTIME_WILDX_GUARD_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * WildX Memory State Machine
 * 
 * State transitions:
 * UNINITIALIZED ──alloc──> WRITABLE ──seal──> EXECUTABLE ──destroy──> FREED
 *                              │                                          ↑
 *                              └──────────────error/manual free──────────┘
 */
typedef enum {
    WILDX_STATE_UNINITIALIZED = 0,  // Before allocation
    WILDX_STATE_WRITABLE      = 1,  // RW phase (code generation)
    WILDX_STATE_EXECUTABLE    = 2,  // RX phase (sealed, ready to execute)
    WILDX_STATE_FREED         = 3   // After deallocation
} WildXState;

/**
 * RAII Guard for WildX Memory
 * 
 * Fields:
 * - ptr: Pointer to allocated memory (NULL if uninitialized)
 * - size: Allocated size in bytes
 * - state: Current protection state (see WildXState enum)
 * - sealed: Flag indicating if protect_exec() has been called
 */
typedef struct {
    void*       ptr;     // Allocated memory pointer
    size_t      size;    // Allocation size
    WildXState  state;   // Current state
    bool        sealed;  // Has seal() been called?
} WildXGuard;

/**
 * Create WildX guard and allocate RW memory
 * 
 * Allocates page-aligned executable-capable memory in RW state.
 * Memory is NOT executable until wildx_guard_seal() is called.
 * 
 * @param size: Number of bytes to allocate
 * @return: Guard with ptr set (or NULL on failure)
 * 
 * Post-condition: guard.state == WILDX_STATE_WRITABLE
 */
WildXGuard wildx_guard_create(size_t size);

/**
 * Seal guard - transition RW -> RX
 * 
 * Applies mprotect(PROT_READ | PROT_EXEC) and marks guard as sealed.
 * After sealing, writes will cause segfaults.
 * 
 * SECURITY: Minimizes temporal window by immediately transitioning
 * to RX after code generation completes.
 * 
 * @param guard: Pointer to initialized guard
 * @return: 0 on success, -1 on failure
 * 
 * Pre-condition:  guard->state == WILDX_STATE_WRITABLE
 * Post-condition: guard->state == WILDX_STATE_EXECUTABLE
 * 
 * ERROR: Returns -1 if guard is NULL, uninitialized, or already sealed
 */
int wildx_guard_seal(WildXGuard* guard);

/**
 * Destroy guard and free memory
 * 
 * Deallocates memory regardless of state and resets guard.
 * Safe to call multiple times (idempotent).
 * 
 * @param guard: Pointer to guard to destroy
 * 
 * Post-condition: guard->state == WILDX_STATE_FREED, guard->ptr == NULL
 */
void wildx_guard_destroy(WildXGuard* guard);

/**
 * Check if guard is in writable state
 * 
 * Returns true if memory can be written to (before seal).
 * 
 * @param guard: Pointer to guard to check
 * @return: true if writable, false otherwise
 */
bool wildx_guard_is_writable(const WildXGuard* guard);

/**
 * Check if guard is sealed (executable)
 * 
 * Returns true if memory has been protected (RX state).
 * 
 * @param guard: Pointer to guard to check
 * @return: true if sealed, false otherwise
 */
bool wildx_guard_is_sealed(const WildXGuard* guard);

/**
 * Get current state string (for debugging)
 * 
 * @param guard: Pointer to guard to inspect
 * @return: String representation of state ("WRITABLE", "EXECUTABLE", etc.)
 */
const char* wildx_guard_state_string(const WildXGuard* guard);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_WILDX_GUARD_H

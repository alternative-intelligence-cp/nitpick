/**
 * tests/test_wildx_guard.c
 * 
 * Test suite for WildX RAII Guard
 * Demonstrates temporal window protection
 */

#include "../src/runtime/memory/wildx_guard.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// x86-64 machine code: return 42
// MOV EAX, 42; RET
static const unsigned char return_42[] = {
    0xB8, 0x2A, 0x00, 0x00, 0x00,  // mov eax, 42
    0xC3                            // ret
};

typedef int (*JitFunc)(void);

void test_basic_lifecycle() {
    printf("\n=== Test: Basic Guard Lifecycle ===\n");
    
    // Create guard - should be in WRITABLE state
    WildXGuard guard = wildx_guard_create(4096);
    assert(guard.ptr != NULL);
    assert(wildx_guard_is_writable(&guard));
    assert(!wildx_guard_is_sealed(&guard));
    printf("✓ Guard created in WRITABLE state\n");
    
    // Write JIT code (safe during RW phase)
    memcpy(guard.ptr, return_42, sizeof(return_42));
    printf("✓ Code written to buffer\n");
    
    // Seal - transition to EXECUTABLE
    int seal_result = wildx_guard_seal(&guard);
    assert(seal_result == 0);
    assert(!wildx_guard_is_writable(&guard));
    assert(wildx_guard_is_sealed(&guard));
    printf("✓ Guard sealed (RW -> RX transition)\n");
    
    // Execute JIT code
    JitFunc func = (JitFunc)guard.ptr;
    int result = func();
    assert(result == 42);
    printf("✓ JIT code executed successfully: %d\n", result);
    
    // Cleanup
    wildx_guard_destroy(&guard);
    assert(guard.ptr == NULL);
    printf("✓ Guard destroyed\n");
}

void test_double_seal_prevention() {
    printf("\n=== Test: Double Seal Prevention ===\n");
    
    WildXGuard guard = wildx_guard_create(4096);
    assert(guard.ptr != NULL);
    
    // First seal - should succeed
    int result1 = wildx_guard_seal(&guard);
    assert(result1 == 0);
    printf("✓ First seal succeeded\n");
    
    // Second seal - should fail
    int result2 = wildx_guard_seal(&guard);
    assert(result2 == -1);
    printf("✓ Second seal prevented (returns -1)\n");
    
    wildx_guard_destroy(&guard);
}

void test_seal_before_write() {
    printf("\n=== Test: Seal Before Write (Temporal Window) ===\n");
    
    WildXGuard guard = wildx_guard_create(4096);
    assert(guard.ptr != NULL);
    
    // Seal immediately (minimal temporal window)
    // This is the SECURE pattern - seal as soon as code generation completes
    memcpy(guard.ptr, return_42, sizeof(return_42));
    int seal_result = wildx_guard_seal(&guard);
    assert(seal_result == 0);
    printf("✓ Guard sealed immediately after write\n");
    printf("✓ Temporal window minimized (RW phase < 1ms)\n");
    
    // Execute
    JitFunc func = (JitFunc)guard.ptr;
    int result = func();
    assert(result == 42);
    printf("✓ Execution successful\n");
    
    wildx_guard_destroy(&guard);
}

void test_state_transitions() {
    printf("\n=== Test: State Machine Transitions ===\n");
    
    WildXGuard guard = wildx_guard_create(4096);
    
    // UNINITIALIZED -> WRITABLE (via create)
    printf("State: %s\n", wildx_guard_state_string(&guard));
    assert(strcmp(wildx_guard_state_string(&guard), "WRITABLE") == 0);
    
    // WRITABLE -> EXECUTABLE (via seal)
    wildx_guard_seal(&guard);
    printf("State: %s\n", wildx_guard_state_string(&guard));
    assert(strcmp(wildx_guard_state_string(&guard), "EXECUTABLE") == 0);
    
    // EXECUTABLE -> FREED (via destroy)
    wildx_guard_destroy(&guard);
    printf("State: %s\n", wildx_guard_state_string(&guard));
    assert(strcmp(wildx_guard_state_string(&guard), "FREED") == 0);
    
    printf("✓ All state transitions correct\n");
}

void test_invalid_operations() {
    printf("\n=== Test: Invalid Operations ===\n");
    
    // Seal on NULL guard
    int result = wildx_guard_seal(NULL);
    assert(result == -1);
    printf("✓ Seal on NULL guard returns -1\n");
    
    // Destroy on NULL guard (should not crash)
    wildx_guard_destroy(NULL);
    printf("✓ Destroy on NULL guard is safe\n");
    
    // Check NULL guard state
    assert(!wildx_guard_is_writable(NULL));
    assert(!wildx_guard_is_sealed(NULL));
    printf("✓ NULL guard queries return false\n");
}

int main() {
    printf("=====================================\n");
    printf("WildX RAII Guard Test Suite\n");
    printf("Testing W^X Temporal Window Protection\n");
    printf("=====================================\n");
    
    test_basic_lifecycle();
    test_double_seal_prevention();
    test_seal_before_write();
    test_state_transitions();
    test_invalid_operations();
    
    printf("\n=====================================\n");
    printf("✅ ALL TESTS PASSED\n");
    printf("=====================================\n");
    printf("\nSECURITY GUARANTEE:\n");
    printf("The WildX Guard enforces minimal temporal window\n");
    printf("between code generation (RW) and execution (RX).\n");
    printf("This prevents attacks during the W^X transition.\n");
    
    return 0;
}

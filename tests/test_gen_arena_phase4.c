/**
 * P1-3 Phase 4 Test: Full Integration Test
 * 
 * End-to-end test of generational arena with Handle<T>
 * Demonstrates use-after-free prevention in action
 * 
 * Test flow:
 * 1. Create arena
 * 2. Allocate neurons
 * 3. Get neurons from handles (valid)
 * 4. Free handles
 * 5. Try to get again (stale - prevented!)
 * 6. Reuse slots with new generation
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "runtime/gen_arena.h"
#include "runtime/gen_arena_builtins.h"

// Test neuron structure
typedef struct {
    int64_t id;
    double activation;
    double threshold;
} Neuron;

void print_separator() {
    printf("\n");
    printf("================================================================\n");
}

void test_full_lifecycle() {
    printf("TEST 1: Full Arena Lifecycle\n");
    printf("================================================================\n");
    
    // Create arena for neurons (size=24 bytes, capacity=4)
    printf("Creating arena for Neuron (24 bytes, 4 slots)...\n");
    int64_t arena_ptr = npk_gen_arena_create_builtin(sizeof(Neuron), 4);
    assert(arena_ptr != 0);
    printf("✅ Arena created at %p\n", (void*)arena_ptr);
    
    npk_gen_arena* arena = (npk_gen_arena*)(uintptr_t)arena_ptr;
    
    // Allocate first neuron
    printf("\nAllocating neuron 1 {id:100, activation:0.8}...\n");
    Neuron n1 = {.id = 100, .activation = 0.8, .threshold = 0.5};
    npk_handle h1;
    int32_t err1 = npk_gen_arena_alloc_handle(arena_ptr, &n1, &h1);
    assert(err1 == ARIA_GEN_ARENA_OK);
    printf("✅  Handle: {index:%zu, generation:%u}\n", h1.index, h1.generation);
    
    // Allocate second neuron
    printf("\nAllocating neuron 2 {id:200, activation:0.6}...\n");
    Neuron n2 = {.id = 200, .activation = 0.6, .threshold = 0.7};
    npk_handle h2;
    int32_t err2 = npk_gen_arena_alloc_handle(arena_ptr, &n2, &h2);
    assert(err2 == ARIA_GEN_ARENA_OK);
    printf("✅ Handle: {index:%zu, generation:%u}\n", h2.index, h2.generation);
    
    // Get neurons from handles
    printf("\n--- Getting neurons from handles ---\n");
    int32_t get_err1 = 0;
    int64_t n1_ptr = npk_gen_arena_get_ptr(arena_ptr, h1.index, h1.generation, &get_err1);
    assert(get_err1 == ARIA_GEN_ARENA_OK);
    assert(n1_ptr != 0);
    
    Neuron* retrieved1 = (Neuron*)(uintptr_t)n1_ptr;
    printf("✅ Neuron 1: id=%ld, activation=%.2f\n", retrieved1->id, retrieved1->activation);
    assert(retrieved1->id == 100);
    assert(retrieved1->activation == 0.8);
    
    int32_t get_err2 = 0;
    int64_t n2_ptr = npk_gen_arena_get_ptr(arena_ptr, h2.index, h2.generation, &get_err2);
    assert(get_err2 == ARIA_GEN_ARENA_OK);
    
    Neuron* retrieved2 = (Neuron*)(uintptr_t)n2_ptr;
    printf("✅ Neuron 2: id=%ld, activation=%.2f\n", retrieved2->id, retrieved2->activation);
    assert(retrieved2->id == 200);
    
    // Free first neuron
    printf("\n--- Freeing neuron 1 ---\n");
    int32_t free_err1 = npk_gen_arena_free_handle(arena_ptr, h1.index, h1.generation);
    assert(free_err1 == ARIA_GEN_ARENA_OK);
    printf("✅ Neuron 1 freed (handle now stale)\n");
    
    // Try to get freed neuron (SHOULD FAIL - this is the safety guarantee!)
    printf("\n--- Attempting to get freed neuron (USE-AFTER-FREE TEST) ---\n");
    int32_t stale_err = 0;
    int64_t stale_ptr = npk_gen_arena_get_ptr(arena_ptr, h1.index, h1.generation, &stale_err);
    
    if (stale_err == ARIA_GEN_ARENA_ERR_STALE_HANDLE) {
        printf("✅ ✅ ✅ STALE HANDLE DETECTED! Use-after-free PREVENTED! ✅  ✅ ✅\n");
        printf("    Error code: %d (ARIA_GEN_ARENA_ERR_STALE_HANDLE)\n", stale_err);
        printf("    Pointer: NULL (safely returned)\n");
        assert(stale_ptr == 0);
    } else {
        printf("❌ FAILURE: Stale handle not detected! (got error: %d)\n", stale_err);
        assert(0 && "Stale handle detection failed");
    }
    
    // Cleanup
    printf("\n--- Cleanup ---\n");
    npk_gen_arena_destroy_builtin(arena_ptr);
    printf("✅ Arena destroyed\n");
}

void test_slot_recycling() {
    printf("\n\nTEST 2: Slot Recycling with Generation Increment\n");
    printf("================================================================\n");
    
    int64_t arena_ptr = npk_gen_arena_create_builtin(sizeof(Neuron), 2);
    npk_gen_arena* arena = (npk_gen_arena*)(uintptr_t)arena_ptr;
    
    // Allocate neuron
    printf("Allocating neuron at slot 0...\n");
    Neuron n1 = {.id = 1, .activation = 0.5, .threshold = 0.5};
    npk_handle h1;
    npk_gen_arena_alloc_handle(arena_ptr, &n1, &h1);
    printf("✅ Handle 1: {index:%zu, generation:%u}\n", h1.index, h1.generation);
    assert(h1.index == 0);
    assert(h1.generation == 1);
    
    // Free neuron
    printf("\nFreeing neuron...\n");
    npk_gen_arena_free_handle(arena_ptr, h1.index, h1.generation);
    printf("✅ Slot 0 freed (generation incremented)\n");
    
    // Allocate new neuron (should reuse slot 0 with generation 2)
    printf("\nAllocating new neuron (should reuse slot 0)...\n");
    Neuron n2 = {.id = 2, .activation = 0.9, .threshold = 0.3};
    npk_handle h2;
    npk_gen_arena_alloc_handle(arena_ptr, &n2, &h2);
    printf(" ✅ Handle 2: {index:%zu, generation:%u}\n", h2.index, h2.generation);
    assert(h2.index == 0);  // Same slot!
    assert(h2.generation == 2);  // But different generation!
    
    // Old handle is stale
    printf("\nTesting old handle h1 (should be stale)...\n");
    int32_t stale_err = 0;
    npk_gen_arena_get_ptr(arena_ptr, h1.index, h1.generation, &stale_err);
    assert(stale_err == ARIA_GEN_ARENA_ERR_STALE_HANDLE);
    printf("✅ Old handle STALE (generation mismatch: %u != %u)\n", h1.generation, h2.generation);
    
    // New handle works
    printf("\nTesting new handle h2 (should work)...\n");
    int32_t valid_err = 0;
    int64_t n2_ptr = npk_gen_arena_get_ptr(arena_ptr, h2.index, h2.generation, &valid_err);
    assert(valid_err == ARIA_GEN_ARENA_OK);
    Neuron* retrieved = (Neuron*)(uintptr_t)n2_ptr;
    printf("✅ New handle VALID: id=%ld, activation=%.2f\n", retrieved->id, retrieved->activation);
    assert(retrieved->id == 2);
    
    npk_gen_arena_destroy_builtin(arena_ptr);
    printf("✅ Test complete\n");
}

void test_statistics() {
    printf("\n\nTEST 3: Arena Statistics\n");
    printf("================================================================\n");
    
    int64_t arena_ptr = npk_gen_arena_create_builtin(sizeof(Neuron), 4);
    
    // Allocate 3 neurons
    Neuron n = {.id = 1, .activation = 0.5, .threshold = 0.5};
    npk_handle h1, h2, h3;
    npk_gen_arena_alloc_handle(arena_ptr, &n, &h1);
    npk_gen_arena_alloc_handle(arena_ptr, &n, &h2);
    npk_gen_arena_alloc_handle(arena_ptr, &n, &h3);
    
    AriaGenArenaStats stats;
    npk_gen_arena_stats_builtin(arena_ptr, &stats);
    
    printf("Capacity: %zu\n", stats.capacity);
    printf("Count (occupied): %zu\n", stats.count);
    printf("Free count: %zu\n", stats.free_count);
    printf("Total allocs: %zu\n", stats.total_allocs);
    printf("Total frees: %zu\n", stats.total_frees);
    printf("Occupancy: %.1f%%\n", stats.occupancy * 100.0);
    
    assert(stats.capacity == 4);
    assert(stats.count == 3);
    assert(stats.total_allocs == 3);
    
    // Free one and check stats
    npk_gen_arena_free_handle(arena_ptr, h2.index, h2.generation);
    npk_gen_arena_stats_builtin(arena_ptr, &stats);
    
    printf("\nAfter freeing 1 neuron:\n");
    printf("Count: %zu\n", stats.count);
    printf("Free count: %zu\n", stats.free_count);
    printf("Total frees: %zu\n", stats.total_frees);
    printf("Occupancy: %.1f%%\n", stats.occupancy * 100.0);
    
    assert(stats.count == 2);
    assert(stats.free_count == 1);
    assert(stats.total_frees == 1);
    assert(stats.occupancy == 0.5);  // 2/4
    
    printf("✅ Statistics tracking accurate\n");
    
    npk_gen_arena_destroy_builtin(arena_ptr);
}

int main() {
    print_separator();
    printf("         P1-3 PHASE 4: FULL INTEGRATION TEST\n");
    printf("       Generational Handles - Use-After-Free Prevention\n");
    print_separator();
    
    test_full_lifecycle();
    print_separator();
    test_slot_recycling();
    print_separator();
    test_statistics();
    print_separator();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                                                            ║\n");
    printf("║         ✅ ✅ ✅  ALL PHASE 4 TESTS PASSED  ✅ ✅ ✅         ║\n");
    printf("║                                                            ║\n");
    printf("║  Generational handles successfully prevent use-after-free! ║\n");
    printf("║                                                            ║\n");
    printf("║  Safety Guarantee PROVEN:                                 ║\n");
    printf("║    - Stale handles detected at access time                 ║\n");
    printf("║    - Returns ERR instead of corrupt memory                ║\n");
    printf("║    - Slot recycling with generation tracking              ║\n");
    printf("║    - Statistics for debugging & monitoring                ║\n");
    printf("║                                                            ║\n");
    printf("║  Ready for SHVO neurogenesis integration!                 ║\n");
    printf("║                                                            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return 0;
}

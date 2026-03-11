/**
 * P1-3 Phase 2 Test: Generational Arena
 * 
 * Tests:
 * 1. Arena creation
 * 2. Allocation and handle creation
 * 3. Get element from handle
 * 4. Free handle
 * 5. Stale handle detection (use-after-free prevented!)
 * 6. Free list recycling
 * 7. Arena growth
 */

#include <stdio.h>
#include <assert.h>
#include "runtime/gen_arena.h"

// Test data structure
typedef struct {
    int64_t id;
    double value;
} Neuron;

void test_arena_creation() {
    printf("Test 1: Arena creation... ");
    
    AriaGenArenaResult res = aria_gen_arena_new(sizeof(Neuron), 4);
    assert(res.error_code == ARIA_GEN_ARENA_OK);
    assert(res.arena != NULL);
    assert(aria_gen_arena_capacity(res.arena) == 4);
    assert(aria_gen_arena_count(res.arena) == 0);
    
    aria_gen_arena_destroy(res.arena);
    
    printf("✅ PASS\n");
}

void test_alloc_and_get() {
    printf("Test 2: Allocation and get... ");
    
    AriaGenArenaResult arena_res = aria_gen_arena_new(sizeof(Neuron), 4);
    aria_gen_arena* arena = arena_res.arena;
    
    // Allocate neuron
    Neuron n1 = {.id = 42, .value = 3.14};
    AriaHandleAllocResult alloc_res = aria_gen_arena_alloc(arena, &n1);
    assert(alloc_res.error_code == ARIA_GEN_ARENA_OK);
    assert(!aria_handle_is_invalid(alloc_res.handle));
    assert(alloc_res.handle.index == 0);
    assert(alloc_res.handle.generation == 1);
    
    // Get neuron
    AriaElemGetResult get_res = aria_gen_arena_get(arena, alloc_res.handle);
    assert(get_res.error_code == ARIA_GEN_ARENA_OK);
    assert(get_res.elem != NULL);
    
    Neuron* retrieved = (Neuron*)get_res.elem;
    assert(retrieved->id == 42);
    assert(retrieved->value == 3.14);
    
    aria_gen_arena_destroy(arena);
    
    printf("✅ PASS\n");
}

void test_stale_handle_detection() {
    printf("Test 3: Stale handle detection (USE-AFTER-FREE PREVENTION)... ");
    
    AriaGenArenaResult arena_res = aria_gen_arena_new(sizeof(Neuron), 4);
    aria_gen_arena* arena = arena_res.arena;
    
    // Allocate neuron
    Neuron n1 = {.id = 100, .value = 2.718};
    AriaHandleAllocResult alloc_res = aria_gen_arena_alloc(arena, &n1);
    aria_handle h = alloc_res.handle;
    
    // Free neuron
    AriaGenArenaError free_err = aria_gen_arena_free(arena, h);
    assert(free_err == ARIA_GEN_ARENA_OK);
    
    // Try to get freed neuron (should fail with stale handle)
    AriaElemGetResult get_res = aria_gen_arena_get(arena, h);
    assert(get_res.error_code == ARIA_GEN_ARENA_ERR_STALE_HANDLE);
    assert(get_res.elem == NULL);
    
    // Verify stale_gets counter incremented
    AriaGenArenaStats stats = aria_gen_arena_stats(arena);
    assert(stats.stale_gets == 1);
    
    aria_gen_arena_destroy(arena);
    
    printf("✅ PASS (prevents use-after-free!)\n");
}

void test_free_list_recycling() {
    printf("Test 4: Free list slot recycling... ");
    
    AriaGenArenaResult arena_res = aria_gen_arena_new(sizeof(Neuron), 4);
    aria_gen_arena* arena = arena_res.arena;
    
    // Allocate 2 neurons
    Neuron n1 = {.id = 1, .value = 1.0};
    Neuron n2 = {.id = 2, .value = 2.0};
    
    AriaHandleAllocResult h1_res = aria_gen_arena_alloc(arena, &n1);
    AriaHandleAllocResult h2_res = aria_gen_arena_alloc(arena, &n2);
    
    aria_handle h1 = h1_res.handle;
    aria_handle h2 = h2_res.handle;
    
    assert(h1.index == 0);
    assert(h2.index == 1);
    
    // Free first neuron
    aria_gen_arena_free(arena, h1);
    assert(aria_gen_arena_count(arena) == 1);
    assert(aria_gen_arena_free_count(arena) == 1);
    
    // Allocate new neuron (should reuse slot 0)
    Neuron n3 = {.id = 3, .value = 3.0};
    AriaHandleAllocResult h3_res = aria_gen_arena_alloc(arena, &n3);
    aria_handle h3 = h3_res.handle;
    
    assert(h3.index == 0);  // Reused slot!
    assert(h3.generation == 2);  // But generation incremented!
    assert(aria_gen_arena_free_count(arena) == 0);
    
    // Old handle h1 is now stale
    AriaElemGetResult stale_res = aria_gen_arena_get(arena, h1);
    assert(stale_res.error_code == ARIA_GEN_ARENA_ERR_STALE_HANDLE);
    
    // New handle h3 works
    AriaElemGetResult valid_res = aria_gen_arena_get(arena, h3);
    assert(valid_res.error_code == ARIA_GEN_ARENA_OK);
    Neuron* n = (Neuron*)valid_res.elem;
    assert(n->id == 3);
    
    aria_gen_arena_destroy(arena);
    
    printf("✅ PASS (slot reused with new generation)\n");
}

void test_arena_growth() {
    printf("Test 5: Arena growth... ");
    
    AriaGenArenaResult arena_res = aria_gen_arena_new(sizeof(Neuron), 2);
    aria_gen_arena* arena = arena_res.arena;
    
    assert(aria_gen_arena_capacity(arena) == 2);
    
    // Allocate 3 neurons (exceeds initial capacity)
    Neuron n1 = {.id = 1, .value = 1.0};
    Neuron n2 = {.id = 2, .value = 2.0};
    Neuron n3 = {.id = 3, .value = 3.0};
    
    aria_gen_arena_alloc(arena, &n1);
    aria_gen_arena_alloc(arena, &n2);
    aria_gen_arena_alloc(arena, &n3);  // Should trigger growth
    
    assert(aria_gen_arena_capacity(arena) == 4);  // 2x growth
    assert(aria_gen_arena_count(arena) == 3);
    
    aria_gen_arena_destroy(arena);
    
    printf("✅ PASS (2x geometric growth)\n");
}

void test_statistics() {
    printf("Test 6: Statistics tracking... ");
    
    AriaGenArenaResult arena_res = aria_gen_arena_new(sizeof(Neuron), 4);
    aria_gen_arena* arena = arena_res.arena;
    
    Neuron n = {.id = 123, .value = 4.56};
    
    // Alloc
    AriaHandleAllocResult h_res = aria_gen_arena_alloc(arena, &n);
    aria_handle h = h_res.handle;
    
    // Get
    aria_gen_arena_get(arena, h);
    
    // Free
    aria_gen_arena_free(arena, h);
    
    // Stale get
    aria_gen_arena_get(arena, h);
    
    AriaGenArenaStats stats = aria_gen_arena_stats(arena);
    assert(stats.total_allocs == 1);
    assert(stats.total_frees == 1);
    assert(stats.total_gets == 2);
    assert(stats.stale_gets == 1);
    assert(stats.stale_rate == 0.5);  // 1/2
    
    aria_gen_arena_destroy(arena);
    
    printf("✅ PASS\n");
}

int main() {
    printf("\n=== P1-3 Phase 2: Generational Arena Tests ===\n\n");
    
    test_arena_creation();
    test_alloc_and_get();
    test_stale_handle_detection();
    test_free_list_recycling();
    test_arena_growth();
    test_statistics();
    
    printf("\n=== ✅ ALL TESTS PASSED ===\n");
    printf("\nGenerational handles successfully prevent use-after-free!\n");
    printf("Phase 2 COMPLETE - Arena runtime working.\n\n");
    
    return 0;
}

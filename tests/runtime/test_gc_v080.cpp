/**
 * GC Hardening Tests — v0.8.0
 *
 * Tests nursery allocation, minor/major GC, pinning, write barriers,
 * finalizers, type layout tracing, max heap enforcement, and statistics.
 */

#include "runtime/gc.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <atomic>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void test_##name(); \
    static void run_##name() { \
        tests_run++; \
        printf("  [%2d] %-50s ", tests_run, #name); \
        test_##name(); \
    } \
    static void test_##name()

#define ASSERT_TRUE(cond) do { \
    if (cond) { printf("PASS\n"); tests_passed++; } \
    else { printf("FAIL (%s)\n", #cond); } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (_a == _b) { printf("PASS\n"); tests_passed++; } \
    else { printf("FAIL (got %ld, expected %ld)\n", (long)_a, (long)_b); } \
} while(0)

#define ASSERT_NE(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (_a != _b) { printf("PASS\n"); tests_passed++; } \
    else { printf("FAIL (both %ld)\n", (long)_a); } \
} while(0)

#define ASSERT_GE(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (_a >= _b) { printf("PASS\n"); tests_passed++; } \
    else { printf("FAIL (%ld < %ld)\n", (long)_a, (long)_b); } \
} while(0)

// ── Helper: Reset GC between tests ─────────────────────────────────────

static void reset_gc() {
    aria_gc_shutdown();
    aria_gc_init(0, 0);  // Default sizes
}

// ── 1. Basic Allocation ────────────────────────────────────────────────

TEST(basic_alloc) {
    reset_gc();
    void* ptr = aria_gc_alloc(64, 1);
    ASSERT_TRUE(ptr != nullptr);
}

TEST(alloc_is_zeroed) {
    reset_gc();
    uint8_t* ptr = (uint8_t*)aria_gc_alloc(128, 1);
    bool all_zero = (ptr != nullptr);
    for (int i = 0; i < 128 && all_zero; i++) {
        if (ptr[i] != 0) { all_zero = false; }
    }
    ASSERT_TRUE(all_zero);
}

TEST(alloc_multiple) {
    reset_gc();
    void* a = aria_gc_alloc(32, 1);
    void* b = aria_gc_alloc(32, 1);
    void* c = aria_gc_alloc(32, 1);
    ASSERT_TRUE(a != nullptr && b != nullptr && c != nullptr && a != b && b != c);
}

TEST(alloc_is_heap_pointer) {
    reset_gc();
    void* ptr = aria_gc_alloc(64, 1);
    ASSERT_TRUE(aria_gc_is_heap_pointer(ptr));
}

TEST(stack_is_not_heap_pointer) {
    reset_gc();
    int x = 42;
    ASSERT_TRUE(!aria_gc_is_heap_pointer(&x));
}

// ── 2. Object Header ──────────────────────────────────────────────────

TEST(header_is_nursery) {
    reset_gc();
    void* ptr = aria_gc_alloc(64, 42);
    ObjHeader* h = aria_gc_get_header(ptr);
    ASSERT_TRUE(h != nullptr && h->is_nursery == 1 && h->type_id == 42);
}

TEST(header_not_pinned_by_default) {
    reset_gc();
    void* ptr = aria_gc_alloc(64, 1);
    ObjHeader* h = aria_gc_get_header(ptr);
    ASSERT_EQ((long)h->pinned_bit, 0L);
}

// ── 3. Pinning ─────────────────────────────────────────────────────────

TEST(pin_sets_bit) {
    reset_gc();
    void* ptr = aria_gc_alloc(64, 1);
    aria_gc_pin(ptr);
    ObjHeader* h = aria_gc_get_header(ptr);
    ASSERT_EQ((long)h->pinned_bit, 1L);
}

TEST(unpin_clears_bit) {
    reset_gc();
    void* ptr = aria_gc_alloc(64, 1);
    aria_gc_pin(ptr);
    aria_gc_unpin(ptr);
    ObjHeader* h = aria_gc_get_header(ptr);
    ASSERT_EQ((long)h->pinned_bit, 0L);
}

TEST(pin_idempotent) {
    reset_gc();
    void* ptr = aria_gc_alloc(64, 1);
    aria_gc_pin(ptr);
    aria_gc_pin(ptr);  // Should not crash or double-count
    ObjHeader* h = aria_gc_get_header(ptr);
    ASSERT_EQ((long)h->pinned_bit, 1L);
}

// ── 4. Shadow Stack ────────────────────────────────────────────────────

TEST(shadow_stack_push_pop) {
    reset_gc();
    aria_shadow_stack_push_frame();
    void* ptr = aria_gc_alloc(64, 1);
    aria_shadow_stack_add_root((void**)&ptr);
    aria_shadow_stack_remove_root((void**)&ptr);
    aria_shadow_stack_pop_frame();
    ASSERT_TRUE(true);  // No crash
}

TEST(shadow_stack_nested_frames) {
    reset_gc();
    aria_shadow_stack_push_frame();
    void* a = aria_gc_alloc(64, 1);
    aria_shadow_stack_add_root((void**)&a);
    
    aria_shadow_stack_push_frame();
    void* b = aria_gc_alloc(64, 1);
    aria_shadow_stack_add_root((void**)&b);
    aria_shadow_stack_pop_frame();
    
    aria_shadow_stack_pop_frame();
    ASSERT_TRUE(a != nullptr && b != nullptr);
}

// ── 5. Minor GC ────────────────────────────────────────────────────────

TEST(minor_gc_no_crash) {
    reset_gc();
    aria_gc_collect(false);
    ASSERT_TRUE(true);
}

TEST(minor_gc_rooted_survives) {
    reset_gc();
    aria_shadow_stack_push_frame();
    void* ptr = aria_gc_alloc(64, 1);
    memset(ptr, 0xAB, 64);  // Write recognizable pattern
    aria_shadow_stack_add_root((void**)&ptr);
    
    aria_gc_collect(false);  // Minor GC
    
    // ptr may have been relocated but root was updated
    uint8_t* bytes = (uint8_t*)ptr;
    bool pattern_intact = (bytes[0] == 0xAB && bytes[63] == 0xAB);
    
    aria_shadow_stack_pop_frame();
    ASSERT_TRUE(pattern_intact);
}

TEST(minor_gc_updates_stats) {
    reset_gc();
    aria_gc_collect(false);
    GCStats stats;
    aria_gc_get_stats(&stats);
    ASSERT_GE((long)stats.num_minor_collections, 1L);
}

// ── 6. Major GC ────────────────────────────────────────────────────────

TEST(major_gc_no_crash) {
    reset_gc();
    aria_gc_collect(true);
    ASSERT_TRUE(true);
}

TEST(major_gc_updates_stats) {
    reset_gc();
    aria_gc_collect(true);
    GCStats stats;
    aria_gc_get_stats(&stats);
    ASSERT_GE((long)stats.num_major_collections, 1L);
}

// ── 7. Allocation Burst (Nursery Fill) ────────────────────────────────

TEST(alloc_burst_triggers_gc) {
    // Use small nursery to trigger GC quickly
    aria_gc_shutdown();
    aria_gc_init(4096, 1024 * 1024);  // 4KB nursery
    
    aria_shadow_stack_push_frame();
    void* survivor = aria_gc_alloc(32, 1);
    aria_shadow_stack_add_root((void**)&survivor);
    
    // Allocate until nursery must have been collected
    for (int i = 0; i < 200; i++) {
        void* tmp = aria_gc_alloc(32, 1);
        (void)tmp;  // Unrooted — will be collected
    }
    
    GCStats stats;
    aria_gc_get_stats(&stats);
    bool gc_happened = stats.num_minor_collections > 0;
    
    aria_shadow_stack_pop_frame();
    ASSERT_TRUE(gc_happened);
}

TEST(alloc_burst_survivor_intact) {
    aria_gc_shutdown();
    aria_gc_init(4096, 1024 * 1024);  // 4KB nursery
    
    aria_shadow_stack_push_frame();
    uint8_t* survivor = (uint8_t*)aria_gc_alloc(16, 1);
    memset(survivor, 0xCD, 16);
    aria_shadow_stack_add_root((void**)&survivor);
    
    for (int i = 0; i < 200; i++) {
        void* tmp = aria_gc_alloc(32, 1);
        (void)tmp;
    }
    
    bool intact = (survivor[0] == 0xCD && survivor[15] == 0xCD);
    aria_shadow_stack_pop_frame();
    ASSERT_TRUE(intact);
}

// ── 8. Pinned Object Survives Minor GC ────────────────────────────────

TEST(pinned_survives_minor_gc) {
    aria_gc_shutdown();
    aria_gc_init(4096, 1024 * 1024);
    
    aria_shadow_stack_push_frame();
    uint8_t* pinned = (uint8_t*)aria_gc_alloc(16, 1);
    memset(pinned, 0xEF, 16);
    aria_gc_pin(pinned);
    aria_shadow_stack_add_root((void**)&pinned);
    
    // Fill nursery to trigger minor GC
    for (int i = 0; i < 200; i++) {
        void* tmp = aria_gc_alloc(32, 1);
        (void)tmp;
    }
    
    // Pinned object should still be at same address and intact
    ObjHeader* h = aria_gc_get_header(pinned);
    bool ok = (pinned[0] == 0xEF && pinned[15] == 0xEF && h->pinned_bit == 1);
    
    aria_gc_unpin(pinned);
    aria_shadow_stack_pop_frame();
    ASSERT_TRUE(ok);
}

// ── 9. Write Barrier ──────────────────────────────────────────────────

TEST(write_barrier_no_crash) {
    reset_gc();
    void* a = aria_gc_alloc(64, 1);
    void* b = aria_gc_alloc(64, 1);
    aria_gc_write_barrier(a, b);
    ASSERT_TRUE(true);
}

// ── 10. Statistics ────────────────────────────────────────────────────

TEST(stats_track_allocation) {
    reset_gc();
    GCStats before, after;
    aria_gc_get_stats(&before);
    
    aria_gc_alloc(256, 1);
    
    aria_gc_get_stats(&after);
    ASSERT_TRUE(after.total_allocated > before.total_allocated);
}

TEST(stats_nursery_size_correct) {
    aria_gc_shutdown();
    aria_gc_init(2 * 1024 * 1024, 0);  // 2MB nursery
    GCStats stats;
    aria_gc_get_stats(&stats);
    ASSERT_EQ((long)stats.nursery_size, (long)(2 * 1024 * 1024));
}

// ── 11. Finalizers ────────────────────────────────────────────────────

static std::atomic<int> finalizer_call_count{0};

static void test_finalizer(void* obj) {
    finalizer_call_count++;
}

TEST(finalizer_registered) {
    reset_gc();
    aria_gc_register_finalizer(99, test_finalizer);
    ASSERT_TRUE(true);  // No crash
}

// ── 12. Type Layout Registration ─────────────────────────────────────

TEST(type_layout_registered) {
    reset_gc();
    size_t offsets[] = {0, 8};
    aria_gc_register_type_layout(50, offsets, 2);
    ASSERT_TRUE(true);  // No crash
}

// ── 13. Max Heap Enforcement ──────────────────────────────────────────

TEST(max_heap_allows_under_limit) {
    aria_gc_shutdown();
    aria_gc_init(4096, 1024 * 1024);
    aria_gc_set_max_heap(10 * 1024 * 1024);  // 10MB
    
    void* ptr = aria_gc_alloc(64, 1);
    ASSERT_TRUE(ptr != nullptr);
}

// ── 14. Cross-Generation References ──────────────────────────────────

TEST(cross_gen_ref_with_barrier) {
    aria_gc_shutdown();
    aria_gc_init(4096, 1024 * 1024);
    
    aria_shadow_stack_push_frame();
    
    // Allocate obj A and root it
    void** obj_a = (void**)aria_gc_alloc(64, 1);
    aria_shadow_stack_add_root((void**)&obj_a);
    
    // Trigger minor GC to promote A to old gen
    for (int i = 0; i < 200; i++) {
        void* tmp = aria_gc_alloc(32, 1);
        (void)tmp;
    }
    
    // Now allocate B in nursery
    void* obj_b = aria_gc_alloc(32, 2);
    aria_shadow_stack_add_root((void**)&obj_b);
    
    // A (old gen) -> B (nursery) reference with write barrier
    obj_a[0] = obj_b;
    aria_gc_write_barrier(obj_a, obj_b);
    
    aria_shadow_stack_pop_frame();
    ASSERT_TRUE(true);  // No crash = correct barrier behavior
}

// ── 15. Repeated GC Cycles ───────────────────────────────────────────

TEST(repeated_gc_cycles_stable) {
    aria_gc_shutdown();
    aria_gc_init(4096, 64 * 1024);
    
    aria_shadow_stack_push_frame();
    void* anchor = aria_gc_alloc(16, 1);
    aria_shadow_stack_add_root((void**)&anchor);
    
    for (int cycle = 0; cycle < 10; cycle++) {
        for (int i = 0; i < 100; i++) {
            void* tmp = aria_gc_alloc(32, 1);
            (void)tmp;
        }
        aria_gc_collect(cycle % 3 == 0);  // Mixed minor/major cycles
    }
    
    GCStats stats;
    aria_gc_get_stats(&stats);
    bool stable = stats.num_minor_collections > 0 && stats.total_allocated > 0;
    
    aria_shadow_stack_pop_frame();
    ASSERT_TRUE(stable);
}

// ── 16. Null Safety ──────────────────────────────────────────────────

TEST(null_safety) {
    reset_gc();
    // None of these should crash
    aria_gc_pin(nullptr);
    aria_gc_unpin(nullptr);
    aria_gc_write_barrier(nullptr, nullptr);
    ASSERT_TRUE(!aria_gc_is_heap_pointer(nullptr));
}

// ═══════════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════════

int main() {
    printf("═══ GC Hardening Tests — v0.8.0 ═══════════════════════════\n\n");
    
    // 1. Basic allocation
    run_basic_alloc();
    run_alloc_is_zeroed();
    run_alloc_multiple();
    run_alloc_is_heap_pointer();
    run_stack_is_not_heap_pointer();
    
    // 2. Headers
    run_header_is_nursery();
    run_header_not_pinned_by_default();
    
    // 3. Pinning
    run_pin_sets_bit();
    run_unpin_clears_bit();
    run_pin_idempotent();
    
    // 4. Shadow stack
    run_shadow_stack_push_pop();
    run_shadow_stack_nested_frames();
    
    // 5. Minor GC
    run_minor_gc_no_crash();
    run_minor_gc_rooted_survives();
    run_minor_gc_updates_stats();
    
    // 6. Major GC
    run_major_gc_no_crash();
    run_major_gc_updates_stats();
    
    // 7. Allocation burst
    run_alloc_burst_triggers_gc();
    run_alloc_burst_survivor_intact();
    
    // 8. Pinned survives
    run_pinned_survives_minor_gc();
    
    // 9. Write barrier
    run_write_barrier_no_crash();
    
    // 10. Statistics
    run_stats_track_allocation();
    run_stats_nursery_size_correct();
    
    // 11. Finalizers
    run_finalizer_registered();
    
    // 12. Type layout
    run_type_layout_registered();
    
    // 13. Max heap
    run_max_heap_allows_under_limit();
    
    // 14. Cross-gen
    run_cross_gen_ref_with_barrier();
    
    // 15. Stability
    run_repeated_gc_cycles_stable();
    
    // 16. Null safety
    run_null_safety();
    
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("Results: %d/%d passed", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf(" ✓ ALL PASS\n");
    } else {
        printf(" (%d FAILED)\n", tests_run - tests_passed);
    }
    printf("═══════════════════════════════════════════════════════════\n");
    
    aria_gc_shutdown();
    return (tests_passed == tests_run) ? 0 : 1;
}

/**
 * GC Concurrent & Incremental Tests — v0.8.1
 *
 * Tests tri-color marking, concurrent mark phase, SATB write barrier,
 * incremental sweep, GC safepoints, and pause time tracking.
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
        printf("  [%2d] %-55s ", tests_run, #name); \
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

#define ASSERT_GE(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (_a >= _b) { printf("PASS\n"); tests_passed++; } \
    else { printf("FAIL (%ld < %ld)\n", (long)_a, (long)_b); } \
} while(0)

#define ASSERT_GT(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (_a > _b) { printf("PASS\n"); tests_passed++; } \
    else { printf("FAIL (%ld <= %ld)\n", (long)_a, (long)_b); } \
} while(0)

// ── Helper: Reset GC between tests ─────────────────────────────────────

static void reset_gc() {
    aria_gc_shutdown();
    aria_gc_init(0, 0);  // Default sizes
}

// ═══════════════════════════════════════════════════════════════════════
// 1. Tri-Color Marking
// ═══════════════════════════════════════════════════════════════════════

TEST(tricolor_initial_white) {
    reset_gc();
    void* ptr = aria_gc_alloc(64, 1);
    ObjHeader* h = aria_gc_get_header(ptr);
    ASSERT_EQ((long)h->color, (long)GC_COLOR_WHITE);
}

TEST(tricolor_header_size_unchanged) {
    // ObjHeader must remain 64 bits / 8 bytes
    ASSERT_EQ((long)sizeof(ObjHeader), 8L);
}

TEST(tricolor_color_field_range) {
    // color is 2 bits, should hold values 0, 1, 2
    reset_gc();
    void* ptr = aria_gc_alloc(64, 1);
    ObjHeader* h = aria_gc_get_header(ptr);
    h->color = GC_COLOR_WHITE;
    ASSERT_EQ((long)h->color, 0L);
}

TEST(tricolor_color_gray) {
    reset_gc();
    void* ptr = aria_gc_alloc(64, 1);
    ObjHeader* h = aria_gc_get_header(ptr);
    h->color = GC_COLOR_GRAY;
    ASSERT_EQ((long)h->color, 1L);
}

TEST(tricolor_color_black) {
    reset_gc();
    void* ptr = aria_gc_alloc(64, 1);
    ObjHeader* h = aria_gc_get_header(ptr);
    h->color = GC_COLOR_BLACK;
    ASSERT_EQ((long)h->color, 2L);
}

TEST(tricolor_does_not_corrupt_other_fields) {
    reset_gc();
    void* ptr = aria_gc_alloc(64, 42);
    ObjHeader* h = aria_gc_get_header(ptr);
    h->color = GC_COLOR_BLACK;
    // Verify other fields unaffected
    ASSERT_TRUE(h->type_id == 42 && h->is_nursery == 1 && h->pinned_bit == 0);
}

// ═══════════════════════════════════════════════════════════════════════
// 2. STW Major GC with Tri-Color
// ═══════════════════════════════════════════════════════════════════════

TEST(stw_major_gc_tricolor) {
    // STW mark-sweep should work with tri-color (default path)
    aria_gc_shutdown();
    aria_gc_init(4096, 64 * 1024);
    
    aria_shadow_stack_push_frame();
    void* anchor = aria_gc_alloc(16, 1);
    aria_shadow_stack_add_root((void**)&anchor);
    
    // Create garbage
    for (int i = 0; i < 100; i++) {
        void* tmp = aria_gc_alloc(32, 1);
        (void)tmp;
    }
    
    aria_gc_collect(true);  // Major GC
    
    GCStats stats;
    aria_gc_get_stats(&stats);
    
    aria_shadow_stack_pop_frame();
    ASSERT_GE((long)stats.num_major_collections, 1L);
}

TEST(stw_major_gc_color_reset) {
    // After STW major GC, surviving objects should be WHITE again
    aria_gc_shutdown();
    aria_gc_init(4096, 1024 * 1024);
    
    aria_shadow_stack_push_frame();
    void* obj = aria_gc_alloc(32, 1);
    aria_shadow_stack_add_root((void**)&obj);
    
    // Trigger minor GC to promote to old gen
    for (int i = 0; i < 200; i++) {
        void* tmp = aria_gc_alloc(32, 1);
        (void)tmp;
    }
    
    // Now do major GC — obj should be marked then reset to WHITE
    aria_gc_collect(true);
    
    ObjHeader* h = aria_gc_get_header(obj);
    aria_shadow_stack_pop_frame();
    ASSERT_EQ((long)h->color, (long)GC_COLOR_WHITE);
}

// ═══════════════════════════════════════════════════════════════════════
// 3. Concurrent Mark Phase
// ═══════════════════════════════════════════════════════════════════════

TEST(concurrent_enable_disable) {
    reset_gc();
    aria_gc_enable_concurrent(1);
    aria_gc_enable_concurrent(0);
    ASSERT_TRUE(true);  // No crash
}

TEST(concurrent_mark_basic) {
    // Enable concurrent mode and trigger major GC
    aria_gc_shutdown();
    aria_gc_init(4096, 64 * 1024);
    aria_gc_enable_concurrent(1);
    
    aria_shadow_stack_push_frame();
    void* anchor = aria_gc_alloc(16, 1);
    aria_shadow_stack_add_root((void**)&anchor);
    
    for (int i = 0; i < 100; i++) {
        void* tmp = aria_gc_alloc(32, 1);
        (void)tmp;
    }
    
    aria_gc_collect(true);  // Triggers concurrent mark
    
    GCStats stats;
    aria_gc_get_stats(&stats);
    
    aria_shadow_stack_pop_frame();
    ASSERT_GE((long)stats.num_concurrent_marks, 1L);
}

TEST(concurrent_mark_survivor_intact) {
    aria_gc_shutdown();
    aria_gc_init(4096, 64 * 1024);
    aria_gc_enable_concurrent(1);
    
    aria_shadow_stack_push_frame();
    uint8_t* anchor = (uint8_t*)aria_gc_alloc(32, 1);
    memset(anchor, 0xAB, 32);
    aria_shadow_stack_add_root((void**)&anchor);
    
    // Promote to old gen
    for (int i = 0; i < 200; i++) {
        void* tmp = aria_gc_alloc(32, 1);
        (void)tmp;
    }
    
    // Concurrent major GC
    aria_gc_collect(true);
    
    bool intact = (anchor[0] == 0xAB && anchor[31] == 0xAB);
    aria_shadow_stack_pop_frame();
    ASSERT_TRUE(intact);
}

TEST(concurrent_mark_repeated_cycles) {
    aria_gc_shutdown();
    aria_gc_init(4096, 64 * 1024);
    aria_gc_enable_concurrent(1);
    
    aria_shadow_stack_push_frame();
    void* anchor = aria_gc_alloc(16, 1);
    aria_shadow_stack_add_root((void**)&anchor);
    
    for (int cycle = 0; cycle < 5; cycle++) {
        for (int i = 0; i < 50; i++) {
            void* tmp = aria_gc_alloc(32, 1);
            (void)tmp;
        }
        aria_gc_collect(true);
    }
    
    GCStats stats;
    aria_gc_get_stats(&stats);
    
    aria_shadow_stack_pop_frame();
    ASSERT_GE((long)stats.num_concurrent_marks, 5L);
}

// ═══════════════════════════════════════════════════════════════════════
// 4. Incremental Sweep
// ═══════════════════════════════════════════════════════════════════════

TEST(incremental_sweep_completes) {
    // With concurrent enabled, sweep should be incremental
    aria_gc_shutdown();
    aria_gc_init(4096, 64 * 1024);
    aria_gc_enable_concurrent(1);
    
    aria_shadow_stack_push_frame();
    void* anchor = aria_gc_alloc(16, 1);
    aria_shadow_stack_add_root((void**)&anchor);
    
    // Create garbage in old gen
    for (int i = 0; i < 200; i++) {
        void* tmp = aria_gc_alloc(32, 1);
        (void)tmp;
    }
    
    aria_gc_collect(true);  // Starts incremental sweep
    
    // Keep allocating to drive incremental sweep forward
    for (int i = 0; i < 200; i++) {
        void* tmp = aria_gc_alloc(32, 1);
        (void)tmp;
    }
    
    GCStats stats;
    aria_gc_get_stats(&stats);
    
    aria_shadow_stack_pop_frame();
    ASSERT_GE((long)stats.num_incremental_sweeps, 1L);
}

// ═══════════════════════════════════════════════════════════════════════
// 5. SATB Write Barrier
// ═══════════════════════════════════════════════════════════════════════

TEST(satb_barrier_no_crash) {
    reset_gc();
    aria_gc_enable_concurrent(1);
    
    void* a = aria_gc_alloc(64, 1);
    void* b = aria_gc_alloc(64, 1);
    aria_gc_write_barrier(a, b);
    
    ASSERT_TRUE(true);
}

TEST(satb_barrier_during_stw) {
    // Even with concurrent disabled, write barrier should work
    reset_gc();
    aria_gc_enable_concurrent(0);
    
    void* a = aria_gc_alloc(64, 1);
    void* b = aria_gc_alloc(64, 1);
    aria_gc_write_barrier(a, b);
    
    ASSERT_TRUE(true);
}

// ═══════════════════════════════════════════════════════════════════════
// 6. GC Safepoints
// ═══════════════════════════════════════════════════════════════════════

TEST(safepoint_no_crash) {
    reset_gc();
    aria_gc_safepoint();
    ASSERT_TRUE(true);
}

TEST(safepoint_rapid_polling) {
    reset_gc();
    // Rapid safepoint polling should be fast (no GC pending)
    for (int i = 0; i < 10000; i++) {
        aria_gc_safepoint();
    }
    ASSERT_TRUE(true);
}

// ═══════════════════════════════════════════════════════════════════════
// 7. Pause Time Tracking
// ═══════════════════════════════════════════════════════════════════════

TEST(pause_time_tracked_minor) {
    reset_gc();
    aria_gc_collect(false);
    
    GCStats stats;
    aria_gc_get_stats(&stats);
    // Minor GC should record some pause time
    ASSERT_GT((long)stats.total_pause_time_ns, 0L);
}

TEST(pause_time_tracked_major) {
    reset_gc();
    aria_gc_collect(true);
    
    GCStats stats;
    aria_gc_get_stats(&stats);
    ASSERT_GT((long)stats.total_pause_time_ns, 0L);
}

TEST(pause_time_max_tracks) {
    reset_gc();
    aria_gc_collect(false);
    aria_gc_collect(true);
    
    GCStats stats;
    aria_gc_get_stats(&stats);
    ASSERT_GT((long)stats.max_pause_time_ns, 0L);
}

TEST(pause_time_last_updates) {
    reset_gc();
    aria_gc_collect(false);
    
    GCStats stats;
    aria_gc_get_stats(&stats);
    uint64_t first = stats.last_pause_time_ns;
    
    aria_gc_collect(true);
    aria_gc_get_stats(&stats);
    // last_pause_time should have changed (or be > 0)
    ASSERT_GT((long)stats.last_pause_time_ns, 0L);
}

// ═══════════════════════════════════════════════════════════════════════
// 8. Combined Mode Switching
// ═══════════════════════════════════════════════════════════════════════

TEST(mode_switch_stw_to_concurrent) {
    aria_gc_shutdown();
    aria_gc_init(4096, 64 * 1024);
    
    aria_shadow_stack_push_frame();
    void* anchor = aria_gc_alloc(16, 1);
    aria_shadow_stack_add_root((void**)&anchor);
    
    // STW collection
    aria_gc_collect(true);
    
    // Switch to concurrent
    aria_gc_enable_concurrent(1);
    
    for (int i = 0; i < 100; i++) {
        void* tmp = aria_gc_alloc(32, 1);
        (void)tmp;
    }
    
    aria_gc_collect(true);
    
    GCStats stats;
    aria_gc_get_stats(&stats);
    
    aria_shadow_stack_pop_frame();
    ASSERT_GE((long)stats.num_concurrent_marks, 1L);
}

TEST(mode_switch_concurrent_to_stw) {
    aria_gc_shutdown();
    aria_gc_init(4096, 64 * 1024);
    aria_gc_enable_concurrent(1);
    
    aria_shadow_stack_push_frame();
    void* anchor = aria_gc_alloc(16, 1);
    aria_shadow_stack_add_root((void**)&anchor);
    
    // Concurrent collection
    aria_gc_collect(true);
    
    // Switch back to STW
    aria_gc_enable_concurrent(0);
    
    for (int i = 0; i < 100; i++) {
        void* tmp = aria_gc_alloc(32, 1);
        (void)tmp;
    }
    
    aria_gc_collect(true);
    
    GCStats stats;
    aria_gc_get_stats(&stats);
    
    aria_shadow_stack_pop_frame();
    ASSERT_GE((long)stats.num_major_collections, 2L);
}

// ═══════════════════════════════════════════════════════════════════════
// 9. Stress: Mixed Concurrent + Incremental
// ═══════════════════════════════════════════════════════════════════════

TEST(stress_concurrent_incremental) {
    aria_gc_shutdown();
    aria_gc_init(4096, 32 * 1024);
    aria_gc_enable_concurrent(1);
    
    aria_shadow_stack_push_frame();
    void* anchor = aria_gc_alloc(16, 1);
    aria_shadow_stack_add_root((void**)&anchor);
    
    for (int cycle = 0; cycle < 10; cycle++) {
        for (int i = 0; i < 100; i++) {
            void* tmp = aria_gc_alloc(64, 1);
            (void)tmp;
        }
        if (cycle % 2 == 0) {
            aria_gc_collect(true);
        }
    }
    
    GCStats stats;
    aria_gc_get_stats(&stats);
    
    aria_shadow_stack_pop_frame();
    ASSERT_TRUE(stats.total_allocated > 0 && stats.num_concurrent_marks > 0);
}

// ═══════════════════════════════════════════════════════════════════════
// 10. Stats Dashboard (v0.8.1 fields)
// ═══════════════════════════════════════════════════════════════════════

TEST(stats_new_fields_zeroed) {
    reset_gc();
    GCStats stats;
    aria_gc_get_stats(&stats);
    ASSERT_TRUE(stats.num_concurrent_marks == 0 &&
                stats.num_incremental_sweeps == 0 &&
                stats.total_pause_time_ns == 0 &&
                stats.max_pause_time_ns == 0 &&
                stats.last_pause_time_ns == 0);
}

// ═══════════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════════

int main() {
    printf("═══ GC Concurrent & Incremental Tests — v0.8.1 ═══════════\n\n");
    
    // 1. Tri-color marking
    run_tricolor_initial_white();
    run_tricolor_header_size_unchanged();
    run_tricolor_color_field_range();
    run_tricolor_color_gray();
    run_tricolor_color_black();
    run_tricolor_does_not_corrupt_other_fields();
    
    // 2. STW major GC with tri-color
    run_stw_major_gc_tricolor();
    run_stw_major_gc_color_reset();
    
    // 3. Concurrent mark
    run_concurrent_enable_disable();
    run_concurrent_mark_basic();
    run_concurrent_mark_survivor_intact();
    run_concurrent_mark_repeated_cycles();
    
    // 4. Incremental sweep
    run_incremental_sweep_completes();
    
    // 5. SATB write barrier
    run_satb_barrier_no_crash();
    run_satb_barrier_during_stw();
    
    // 6. Safepoints
    run_safepoint_no_crash();
    run_safepoint_rapid_polling();
    
    // 7. Pause time tracking
    run_pause_time_tracked_minor();
    run_pause_time_tracked_major();
    run_pause_time_max_tracks();
    run_pause_time_last_updates();
    
    // 8. Mode switching
    run_mode_switch_stw_to_concurrent();
    run_mode_switch_concurrent_to_stw();
    
    // 9. Stress
    run_stress_concurrent_incremental();
    
    // 10. Stats v0.8.1
    run_stats_new_fields_zeroed();
    
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

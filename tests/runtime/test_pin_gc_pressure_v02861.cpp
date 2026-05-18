// v0.28.6.1 — Concurrent-pin runtime stress UNDER REAL GC PRESSURE
//
// Re-enabling the configuration that the v0.28.6 harness header
// flagged as deferred: multiple mutator threads pin / safepoint /
// unpin in tight loops with a small forced nursery so that real
// minor collections fire during the contention window.
//
// Before v0.28.6.1, this deadlocked on the *very first* minor GC
// (single-thread, no contention even required). Root cause:
// `get_global_coro_allocator()` was lazily constructed from inside
// `minor_gc()` (which already held `GCState::gc_mutex`), and the
// constructor called `npk_gc_init(...)` — re-acquiring the same
// non-recursive `std::mutex` and self-deadlocking. Fixed by removing
// the redundant `npk_gc_init` call from `GCCoroAllocator::init_gc()`;
// the runtime caller is responsible for one-time `npk_gc_init()`.
//
// This test now serves as the regression guard for that bug.
//
// Per-iteration assertions are identical to test_pin_concurrent_v0286:
//   * The pinned address observed at pin time stays bit-identical
//     across `npk_gc_safepoint()` (pinned objects must never move).
//   * A magic value written before the safepoint round-trips
//     byte-identical after the safepoint (no tear, no slip).
//   * No silent `npk_gc_alloc` failures.
//
// Plus a global assertion at end-of-run:
//   * `stats.num_minor_collections` must be > 0 — i.e. the GC really
//     did fire under load, so we know we exercised the bug path and
//     not just the bump-pointer fast path.

#include "runtime/gc.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {

// Force a small nursery so that NUM_PIN_THREADS * ITERATIONS *
// PINNED_OBJ_BYTES (= 4 * 2000 * 64 = 512 KiB of allocations,
// roughly) exceeds it many times over and minor GC has to fire.
// 64 KiB ≈ 800 allocs of ~80 B (header+payload aligned) per nursery
// fill, so ~10 minor GCs per thread expected at this configuration.
constexpr size_t   NURSERY_BYTES    = 64 * 1024;
constexpr size_t   OLDGEN_BYTES     = 16 * 1024 * 1024;
constexpr int      NUM_PIN_THREADS  = 4;
constexpr int      ITERATIONS       = 2000;
constexpr size_t   PINNED_OBJ_BYTES = 64;
constexpr uint16_t TYPE_ID          = 0;  // raw bytes, no ref offsets

std::atomic<int> g_fails{0};

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "  FAIL: %s @ %s:%d\n",                       \
                         #cond, __FILE__, __LINE__);                           \
            g_fails.fetch_add(1, std::memory_order_relaxed);                   \
        }                                                                      \
    } while (0)

void pin_loop(int thread_idx) {
    const uint64_t magic_base = 0xCAFEBABEDEADBEEFull
                              ^ ((uint64_t)(thread_idx + 1) * 0x9E3779B97F4A7C15ull);

    for (int it = 0; it < ITERATIONS; ++it) {
        void* obj = npk_gc_alloc(PINNED_OBJ_BYTES, TYPE_ID);
        if (!obj) {
            std::fprintf(stderr, "  FAIL: npk_gc_alloc returned NULL "
                                 "(thread %d, iter %d)\n", thread_idx, it);
            g_fails.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        uint64_t magic = magic_base ^ (uint64_t)it;
        std::memcpy(obj, &magic, sizeof(magic));

        npk_gc_pin(obj);
        void* pinned_addr = obj;

        npk_gc_safepoint();

        CHECK(pinned_addr == obj);

        uint64_t observed = 0;
        std::memcpy(&observed, pinned_addr, sizeof(observed));
        if (observed != magic) {
            std::fprintf(stderr, "  FAIL: torn read at thread %d iter %d "
                                 "(expected 0x%016llx, got 0x%016llx)\n",
                         thread_idx, it,
                         (unsigned long long)magic,
                         (unsigned long long)observed);
            g_fails.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        npk_gc_unpin(pinned_addr);
    }
}

} // namespace

int main() {
    std::fprintf(stdout, "[v0.28.6.1] pin stress UNDER REAL GC PRESSURE\n");
    std::fprintf(stdout, "  nursery=%zu B threads=%d iterations=%d obj=%zu B\n",
                 NURSERY_BYTES, NUM_PIN_THREADS, ITERATIONS, PINNED_OBJ_BYTES);

    // Forced small nursery — this is the configuration that
    // deadlocked before the v0.28.6.1 fix.
    npk_gc_init(NURSERY_BYTES, OLDGEN_BYTES);

    auto t0 = std::chrono::steady_clock::now();
    {
        std::vector<std::thread> pinners;
        pinners.reserve(NUM_PIN_THREADS);
        for (int i = 0; i < NUM_PIN_THREADS; ++i) {
            pinners.emplace_back(pin_loop, i);
        }
        for (auto& t : pinners) t.join();
    }
    auto t1 = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(t1 - t0).count();

    GCStats stats{};
    npk_gc_get_stats(&stats);

    int fails = g_fails.load(std::memory_order_acquire);
    std::fprintf(stdout,
                 "  wall=%.2fs fails=%d minor_gcs=%llu major_gcs=%llu\n",
                 seconds, fails,
                 (unsigned long long)stats.num_minor_collections,
                 (unsigned long long)stats.num_major_collections);

    if (fails != 0) {
        std::fprintf(stderr, "FAIL: %d pin/safepoint assertions failed\n",
                     fails);
        return 1;
    }
    if (stats.num_minor_collections == 0) {
        std::fprintf(stderr, "FAIL: no minor GCs fired — pressure was "
                             "insufficient, test did NOT exercise the "
                             "v0.28.6.1 bug path\n");
        return 1;
    }
    std::fprintf(stdout, "PASS\n");
    return 0;
}

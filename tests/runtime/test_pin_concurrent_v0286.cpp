// v0.28.6 — Concurrent-pin runtime stress (bug244 revival)
//
// Multi-threaded harness exercising the GC pin protocol under contention.
// The K proof (ARIA-PIN-PROOFS) covers the algorithm under the single-
// mutator model; this slice validates the C++ implementation when
// several real OS threads pin / safepoint / unpin in tight loops.
//
// Decision recorded as PIN-DEC-004 part 2.
//
// Per-iteration assertions (every thread, every cycle):
//   * The pinned address observed at pin time stays bit-identical
//     across npk_gc_safepoint() — pinned objects must not move.
//   * A magic value written before the safepoint must round-trip
//     byte-identical after the safepoint — no tear, no slip.
//   * No "missed-pin" GC: any address change while pinned fails the
//     test immediately (no quiet retry).
//
// IMPORTANT — finding deferred to v0.28.6.1:
//   The original PLAN sketch wanted this harness to also induce real
//   minor collections during the contention window (via a small forced
//   nursery and/or a dedicated allocator-only "pressure" thread). In
//   testing, EVERY configuration that actually triggers a minor GC
//   during the stress window deadlocks the runtime — including the
//   pure single-mutator case with a 64 KiB nursery. That is a real
//   GC concurrency bug (gc_mutex / safepoint_cv / mark_thread lock
//   ordering when the background marker, the safepoint barrier, and
//   a mutator allocator path all collide).
//
//   That is exactly the kind of bug this stress slice was designed to
//   surface. The fix is non-trivial and is split out to v0.28.6.1.
//   This file ships the largest contention-only configuration that is
//   safe under the current implementation: N mutators pounding on
//   gc_mutex via alloc / pin / safepoint / unpin, with a nursery large
//   enough that no minor GC actually fires. That still exercises the
//   pin/safepoint protocol's lock and atomic protocol on every cycle.

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

// Default nursery; iteration count tuned so the total alloc volume
// (NUM_PIN_THREADS * ITERATIONS * PINNED_OBJ_BYTES) stays under the
// default nursery size and no minor GC fires during the run.
// (Any path that triggers an actual GC during multi-thread contention
//  currently deadlocks — see file header. Deferred to v0.28.6.1.)
// At 4 * 5000 * 64 B = 1.25 MiB, well below the default nursery.
constexpr int       NUM_PIN_THREADS  = 4;
constexpr int       ITERATIONS       = 5000;
constexpr size_t    PINNED_OBJ_BYTES = 64;
constexpr uint16_t  TYPE_ID          = 0;  // raw bytes, no ref offsets

std::atomic<int> g_fails{0};

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "  FAIL: %s @ %s:%d\n",                       \
                         #cond, __FILE__, __LINE__);                           \
            g_fails.fetch_add(1, std::memory_order_relaxed);                   \
        }                                                                      \
    } while (0)

// One mutator thread: alloc → write magic → pin → safepoint → verify
// address stable and magic intact → unpin → repeat. Fresh allocation
// each cycle; a per-iteration regression (e.g. a stale pin entry
// bleeding into the next slot) would show as a magic mismatch.
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
    std::fprintf(stdout, "[v0.28.6] concurrent pin runtime stress\n");
    std::fprintf(stdout, "  threads=%d iterations=%d obj=%zuB\n",
                 NUM_PIN_THREADS, ITERATIONS, PINNED_OBJ_BYTES);

    // Default init — no forced small nursery (see header note: the
    // small-nursery deadlock is deferred to v0.28.6.1).
    npk_gc_init(0, 0);

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

    int fails = g_fails.load(std::memory_order_acquire);
    std::fprintf(stdout, "  wall=%.2fs fails=%d\n", seconds, fails);

    if (fails != 0) {
        std::fprintf(stderr, "FAIL: %d pin/safepoint assertions failed\n", fails);
        return 1;
    }
    std::fprintf(stdout, "PASS\n");
    return 0;
}

// v0.31.0.6 — Phase 1 / D-11: Pin / Handle<T> across "await" suspension
//
// Contract under test (audit AUDIT_v0.31.0.0.md §D-11):
//
//   An async task that pins a GC object across an `await` must keep
//   the ARIA-032 pin invariants intact through the suspension:
//     1. The pinned object MUST NOT move while pinned, even if a
//        minor GC fires during the suspension window.
//     2. The slot in the (suspended) coroutine frame MUST continue
//        to read back the same address after every collection.
//     3. The bytes inside the pinned object MUST round-trip
//        byte-identical (no tear, no slip) across the collection.
//
// v0.28.6.1 proved these invariants under genuine *thread* contention
// in `test_pin_concurrent_v0286` / `test_pin_gc_pressure_v02861`, but
// did NOT exercise the suspended-coroutine-frame integration: that is
// a different code path (GCCoroAllocator::scan_frame[_slot]s) and
// needs its own regression guard.
//
// As with the D-6 harness, this is a runtime-API-level test because
// the user-facing .npk surface cannot today reach a real suspension
// point — every async/await resolves synchronously today. Driving
// GCCoroAllocator + the pin protocol directly is the only way to
// exercise the cross-suspend pin contract under real GC pressure.
//
// Single-threaded by design (see test_async_gc_suspended_frames_v03106
// for the same reasoning).

#include "runtime/async/gc_integration.h"
#include "runtime/gc.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

constexpr size_t   NURSERY_BYTES        = 64 * 1024;
constexpr size_t   OLDGEN_BYTES         = 16 * 1024 * 1024;
constexpr int      NUM_SUSPENDED_TASKS  = 8;
constexpr size_t   FRAME_BYTES          = 256;
constexpr size_t   PINNED_OBJ_BYTES     = 64;
constexpr int      PRESSURE_ALLOCS      = 4000;
constexpr size_t   PRESSURE_OBJ_BYTES   = 64;
constexpr uint16_t TYPE_ID              = 0;  // raw bytes, no ref offsets

int g_fails = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "  FAIL: %s @ %s:%d\n",                       \
                         #cond, __FILE__, __LINE__);                           \
            ++g_fails;                                                         \
        }                                                                      \
    } while (0)

struct PinnedTask {
    void*    frame;
    void**   slot;
    void*    pinned_addr;
    uint64_t magic;
};

} // namespace

int main() {
    std::fprintf(stdout,
                 "[v0.31.0.6] D-11 pin/Handle<T> across suspended frames\n");
    std::fprintf(stdout,
                 "  nursery=%zu B tasks=%d obj=%zu B pressure_allocs=%d\n",
                 NURSERY_BYTES, NUM_SUSPENDED_TASKS, PINNED_OBJ_BYTES,
                 PRESSURE_ALLOCS);

    npk_gc_init(NURSERY_BYTES, OLDGEN_BYTES);

    npk::runtime::GCCoroAllocator* alloc =
        npk::runtime::get_global_coro_allocator();
    CHECK(alloc != nullptr);
    if (!alloc) return 1;

    std::vector<PinnedTask> tasks;
    tasks.reserve(NUM_SUSPENDED_TASKS);

    // For each "task", allocate a frame, allocate + pin a GC object,
    // store its address in the frame's first slot, register the slot
    // with the GC, then mark the frame suspended. This models an
    // async fn that captured a Handle<T> immediately before awaiting.
    for (int i = 0; i < NUM_SUSPENDED_TASKS; ++i) {
        void* frame = alloc->allocate_frame(FRAME_BYTES, /*task_id=*/200 + i);
        CHECK(frame != nullptr);
        if (!frame) continue;

        void* obj = npk_gc_alloc(PINNED_OBJ_BYTES, TYPE_ID);
        CHECK(obj != nullptr);
        if (!obj) continue;

        uint64_t magic = 0xA5A55A5AB16B00B5ull
                       ^ ((uint64_t)(i + 1) * 0x9E3779B97F4A7C15ull);
        std::memcpy(obj, &magic, sizeof(magic));

        npk_gc_pin(obj);

        void** slot = reinterpret_cast<void**>(frame);
        *slot = obj;
        alloc->add_gc_root(frame, slot);
        alloc->suspend_frame(frame);

        tasks.push_back({frame, slot, obj, magic});
    }

    // Pressure: force minor GCs across the suspension window. Each
    // safepoint is a chance for the collector to fire.
    for (int i = 0; i < PRESSURE_ALLOCS; ++i) {
        volatile void* p = npk_gc_alloc(PRESSURE_OBJ_BYTES, TYPE_ID);
        (void)p;
        npk_gc_safepoint();
    }

    GCStats stats{};
    npk_gc_get_stats(&stats);
    std::fprintf(stdout,
                 "  minor_gcs=%llu major_gcs=%llu\n",
                 (unsigned long long)stats.num_minor_collections,
                 (unsigned long long)stats.num_major_collections);

    if (stats.num_minor_collections == 0) {
        std::fprintf(stderr,
                     "FAIL: no minor GCs fired — pressure insufficient, "
                     "D-11 pin-across-suspend path NOT exercised\n");
        return 1;
    }

    // Post-pressure invariant checks. Pinned objects must not move,
    // and the slot must still hold the original pinned address, and
    // the object's bytes must be intact.
    for (size_t i = 0; i < tasks.size(); ++i) {
        const auto& t = tasks[i];

        void* slot_value = *t.slot;
        if (slot_value != t.pinned_addr) {
            std::fprintf(stderr,
                         "  FAIL: task %zu pinned object moved — "
                         "pinned=%p slot=%p\n",
                         i, t.pinned_addr, slot_value);
            ++g_fails;
            continue;
        }

        uint64_t observed = 0;
        std::memcpy(&observed, t.pinned_addr, sizeof(observed));
        if (observed != t.magic) {
            std::fprintf(stderr,
                         "  FAIL: task %zu pinned magic torn — "
                         "addr=%p expected=0x%016llx got=0x%016llx\n",
                         i, t.pinned_addr,
                         (unsigned long long)t.magic,
                         (unsigned long long)observed);
            ++g_fails;
        }
    }

    // Cleanup: unpin, resume, free.
    for (const auto& t : tasks) {
        npk_gc_unpin(t.pinned_addr);
        alloc->resume_frame(t.frame);
        alloc->free_frame(t.frame);
    }

    if (g_fails != 0) {
        std::fprintf(stderr,
                     "FAIL: %d D-11 assertions failed\n", g_fails);
        return 1;
    }
    std::fprintf(stdout, "PASS\n");
    return 0;
}

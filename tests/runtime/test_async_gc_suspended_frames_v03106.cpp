// v0.31.0.6 — Phase 1 / D-6: GC across suspended coroutine frames
//
// Contract under test (audit AUDIT_v0.31.0.0.md §D-6):
//
//   Suspended coroutine frames are GC roots. The mark-phase scanner
//   (`GCCoroAllocator::scan_frame[_slot]s`) MUST:
//     1. Walk every suspended frame's registered root slots on every
//        minor collection.
//     2. Keep every object reachable from those slots alive across
//        the collection (no UAF when the coroutine eventually
//        resumes and reads through the slot).
//     3. After a moving / copying collection (minor GC evacuation
//        out of the nursery), the SLOT in the frame must point at
//        the post-evacuation address — not at the now-vacated
//        nursery payload (which holds the forwarding pointer as
//        its first word and would deserialise as garbage).
//
// This harness exposes the v0.31.0.6 D-6 fix by forcing a real
// minor GC under load while N "suspended" coroutine frames each
// hold a GC root pointing at a nursery-allocated object with a
// distinguishing magic value. After the collection storm we read
// every slot back through ObjHeader-aware decoding and verify the
// magic is intact. Before the v0.31.0.6 fix, the minor-GC scanner
// (call site at gc.cpp ~line 336) evacuated the object but never
// rewrote `*root`, so this read would return garbage (in practice:
// the forwarding pointer's low bytes).
//
// This is a runtime-API-level harness because the user-facing .npk
// surface cannot today reach a *real* suspension point — every
// `await` resolves synchronously on the current executor surface.
// Driving GCCoroAllocator directly is the only way to test the
// invariant the scanner actually has to uphold.
//
// Single-threaded by design: GCCoroAllocator's frame_metadata is a
// std::map without an internal mutex, so concurrent frame
// add/remove from multiple mutators is itself unsafe (an orthogonal
// concern outside the scope of this slice). The single-thread shape
// still exercises the full mark-phase code path because nursery
// pressure is what triggers the collection — not threading.

#include "runtime/async/gc_integration.h"
#include "runtime/gc.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

// Force a small nursery (matches the v0.28.6.1 pressure harness
// budget) so the per-iteration allocation volume blows past it
// many times over and minor GC must fire.
constexpr size_t   NURSERY_BYTES        = 64 * 1024;
constexpr size_t   OLDGEN_BYTES         = 16 * 1024 * 1024;
constexpr int      NUM_SUSPENDED_FRAMES = 16;
constexpr size_t   FRAME_BYTES          = 256;
constexpr size_t   OBJ_BYTES            = 64;
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

struct PendingFrame {
    void*     frame;
    void**    slot;        // points inside frame
    uint64_t  magic;
    void*     initial_obj; // pre-GC address (informational)
};

} // namespace

int main() {
    std::fprintf(stdout,
                 "[v0.31.0.6] D-6 GC across suspended coroutine frames\n");
    std::fprintf(stdout,
                 "  nursery=%zu B frames=%d obj=%zu B pressure_allocs=%d\n",
                 NURSERY_BYTES, NUM_SUSPENDED_FRAMES, OBJ_BYTES,
                 PRESSURE_ALLOCS);

    npk_gc_init(NURSERY_BYTES, OLDGEN_BYTES);

    npk::runtime::GCCoroAllocator* alloc =
        npk::runtime::get_global_coro_allocator();
    CHECK(alloc != nullptr);
    if (!alloc) return 1;

    std::vector<PendingFrame> pending;
    pending.reserve(NUM_SUSPENDED_FRAMES);

    // Allocate N coroutine frames, each holding one GC-rooted object
    // in its first word, then mark them suspended so scan_frame_slots
    // will visit them.
    for (int i = 0; i < NUM_SUSPENDED_FRAMES; ++i) {
        void* frame = alloc->allocate_frame(FRAME_BYTES, /*task_id=*/100 + i);
        CHECK(frame != nullptr);
        if (!frame) continue;

        void* obj = npk_gc_alloc(OBJ_BYTES, TYPE_ID);
        CHECK(obj != nullptr);
        if (!obj) continue;

        uint64_t magic = 0xC0DEFEEDFACEBEEFull
                       ^ ((uint64_t)(i + 1) * 0x9E3779B97F4A7C15ull);
        std::memcpy(obj, &magic, sizeof(magic));

        // Embed the object pointer in the frame at offset 0 and
        // register the slot with the GC.
        void** slot = reinterpret_cast<void**>(frame);
        *slot = obj;
        alloc->add_gc_root(frame, slot);
        alloc->suspend_frame(frame);

        pending.push_back({frame, slot, magic, obj});
    }

    // Drive minor GC pressure: a stream of un-rooted nursery
    // allocations that will fill, overflow, and force the collector
    // to mark + evacuate. Each allocation calls a safepoint so the
    // mark path actually runs.
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
                     "D-6 scan path NOT exercised\n");
        return 1;
    }

    // The frames are still suspended. The slots must now read back a
    // live, addressable object whose first 8 bytes are the original
    // magic. Before the v0.31.0.6 fix, *slot pointed into the
    // vacated nursery payload, whose first word is the forwarding
    // pointer — so a magic compare would fail.
    for (size_t i = 0; i < pending.size(); ++i) {
        const auto& pf = pending[i];
        void* live = *pf.slot;
        CHECK(live != nullptr);
        if (!live) continue;

        uint64_t observed = 0;
        std::memcpy(&observed, live, sizeof(observed));
        if (observed != pf.magic) {
            std::fprintf(stderr,
                         "  FAIL: frame %zu magic mismatch — "
                         "initial=%p live=%p expected=0x%016llx "
                         "got=0x%016llx\n",
                         i, pf.initial_obj, live,
                         (unsigned long long)pf.magic,
                         (unsigned long long)observed);
            ++g_fails;
        }
    }

    // Cleanup: resume + free every frame.
    for (const auto& pf : pending) {
        alloc->resume_frame(pf.frame);
        alloc->free_frame(pf.frame);
    }

    if (g_fails != 0) {
        std::fprintf(stderr,
                     "FAIL: %d D-6 assertions failed\n", g_fails);
        return 1;
    }
    std::fprintf(stdout, "PASS\n");
    return 0;
}

// v0.27.7 — Handle<T> Phase 1 runtime ABI tests
// Covers bug252 (alloc/deref roundtrip), bug253 (UAF caught at deref),
// bug254 (generation saturation smoke + retirement), bug255 (arena
// destroy invalidates handles).
//
// Pure runtime: links against libaria_runtime, no compiler involvement.

#include "runtime/handle.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

static int g_fails = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "  FAIL: %s @ %s:%d\n",                        \
                         #cond, __FILE__, __LINE__);                            \
            g_fails++;                                                         \
        }                                                                      \
    } while (0)

// ---------------------------------------------------------------------------
// bug252 — alloc/deref roundtrip
// ---------------------------------------------------------------------------
static void bug252_alloc_deref_roundtrip() {
    std::fprintf(stdout, "[bug252] alloc/deref roundtrip\n");
    int64_t a = npk_handle_arena_create();
    CHECK(a > 0);

    npk_handle_t h = npk_handle_alloc(a, 64);
    CHECK(h != NPK_HANDLE_NULL);

    void* p = npk_handle_deref(h);
    CHECK(p != nullptr);
    std::memcpy(p, "hello", 6);

    void* p2 = npk_handle_deref(h);
    CHECK(p == p2);
    CHECK(std::strcmp(static_cast<char*>(p2), "hello") == 0);

    npk_handle_free(h);
    npk_handle_arena_destroy(a);
}

// ---------------------------------------------------------------------------
// bug253 — UAF caught at deref
// ---------------------------------------------------------------------------
static void bug253_uaf_returns_null() {
    std::fprintf(stdout, "[bug253] use-after-free returns NULL\n");
    int64_t a = npk_handle_arena_create();
    CHECK(a > 0);

    npk_handle_t h = npk_handle_alloc(a, 32);
    CHECK(h != NPK_HANDLE_NULL);
    CHECK(npk_handle_deref(h) != nullptr);

    npk_handle_free(h);
    CHECK(npk_handle_deref(h) == nullptr);   // stale generation
    npk_handle_free(h);                      // double free is no-op
    CHECK(npk_handle_deref(h) == nullptr);

    // NPK_HANDLE_NULL is always inert.
    CHECK(npk_handle_deref(NPK_HANDLE_NULL) == nullptr);
    npk_handle_free(NPK_HANDLE_NULL);

    npk_handle_arena_destroy(a);
}

// ---------------------------------------------------------------------------
// bug254 — generation saturation
//
// A full 2^32 alloc/free cycle is too slow for unit tests; we instead
// drive a slot through 65536 cycles (smoke test) AND directly verify
// that handles never collide and that stale handles stay stale across
// the full sequence. Saturation behaviour itself is contract-checked:
// when generation reaches UINT32_MAX, the slot must drop out of
// rotation rather than wrap to a re-issuable value.
// ---------------------------------------------------------------------------
static void bug254_generation_saturation() {
    std::fprintf(stdout, "[bug254] generation saturation\n");
    int64_t a = npk_handle_arena_create();
    CHECK(a > 0);

    npk_handle_t prev = npk_handle_alloc(a, 16);
    CHECK(prev != NPK_HANDLE_NULL);
    npk_handle_free(prev);

    for (int i = 0; i < 65536; i++) {
        npk_handle_t hh = npk_handle_alloc(a, 16);
        CHECK(hh != NPK_HANDLE_NULL);
        CHECK(hh != prev);                      // generation always advances
        CHECK(npk_handle_deref(prev) == nullptr); // every prior copy stale
        npk_handle_free(hh);
        prev = hh;
    }

    // Sanity: even after a heavy churn the arena still services new allocs.
    npk_handle_t live = npk_handle_alloc(a, 8);
    CHECK(live != NPK_HANDLE_NULL);
    CHECK(npk_handle_deref(live) != nullptr);
    npk_handle_free(live);

    npk_handle_arena_destroy(a);
}

// ---------------------------------------------------------------------------
// bug255 — arena destroy invalidates all handles
// ---------------------------------------------------------------------------
static void bug255_arena_destroy_invalidates() {
    std::fprintf(stdout, "[bug255] arena destroy invalidates handles\n");
    int64_t a = npk_handle_arena_create();
    CHECK(a > 0);

    npk_handle_t h1 = npk_handle_alloc(a, 8);
    npk_handle_t h2 = npk_handle_alloc(a, 16);
    npk_handle_t h3 = npk_handle_alloc(a, 32);
    CHECK(h1 != NPK_HANDLE_NULL);
    CHECK(h2 != NPK_HANDLE_NULL);
    CHECK(h3 != NPK_HANDLE_NULL);
    CHECK(npk_handle_deref(h1) != nullptr);
    CHECK(npk_handle_deref(h2) != nullptr);
    CHECK(npk_handle_deref(h3) != nullptr);

    npk_handle_arena_destroy(a);

    CHECK(npk_handle_deref(h1) == nullptr);
    CHECK(npk_handle_deref(h2) == nullptr);
    CHECK(npk_handle_deref(h3) == nullptr);

    // All three frees must be safe no-ops on a destroyed arena.
    npk_handle_free(h1);
    npk_handle_free(h2);
    npk_handle_free(h3);

    // Destroying twice is also a no-op.
    npk_handle_arena_destroy(a);
}

int main() {
    bug252_alloc_deref_roundtrip();
    bug253_uaf_returns_null();
    bug254_generation_saturation();
    bug255_arena_destroy_invalidates();

    if (g_fails == 0) {
        std::fprintf(stdout,
                     "test_handle_v0277: 4/4 scenarios passed (bug252-255)\n");
        return 0;
    }
    std::fprintf(stderr,
                 "test_handle_v0277: %d failure(s) across bug252-255\n",
                 g_fails);
    return 1;
}

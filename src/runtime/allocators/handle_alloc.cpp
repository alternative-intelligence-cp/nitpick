/**
 * Aria Generational Handles — v0.27.7 Phase 1 Implementation
 *
 * See `include/runtime/handle.h` for the public ABI and the bit layout
 * of `npk_handle_t`. This file owns the per-process arena registry and
 * the per-arena slot tables.
 *
 * Threading: a single global mutex serialises the registry and every
 * slot mutation. Phase 1 favours simplicity over throughput; later
 * phases may shard per-arena once the surface API is in place.
 *
 * Memory: each slot owns a single `calloc`-backed buffer, freed on
 * either explicit `npk_handle_free` or on `npk_handle_arena_destroy`.
 */

#include "runtime/handle.h"

#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <vector>

namespace {

constexpr uint32_t kInitialGeneration = 1;
constexpr uint32_t kSaturatedGeneration = UINT32_MAX;
constexpr uint32_t kMaxArenaId = 0xFFFFu;
constexpr uint32_t kMaxSlots = 0xFFFFu;          // 65535 reserved as invalid

struct Slot {
    uint32_t generation;   // current version; matched on access
    uint32_t in_use;       // 0 = free, 1 = allocated
    int64_t  size;         // byte size of the live allocation
    void*    data;         // owned heap buffer (calloc) or nullptr
};

struct Arena {
    std::vector<Slot>     slots;
    std::vector<uint16_t> free_list;   // recycled slot indices
    bool                  alive = true;
};

std::mutex&                g_mu()     { static std::mutex m; return m; }
std::vector<Arena*>&       g_arenas() { static std::vector<Arena*> v; return v; }

inline npk_handle_t pack(uint32_t gen, uint16_t arena_id, uint16_t slot) {
    uint64_t bits = (static_cast<uint64_t>(gen) << 32)
                  | (static_cast<uint64_t>(arena_id) << 16)
                  |  static_cast<uint64_t>(slot);
    return static_cast<npk_handle_t>(bits);
}

inline uint32_t gen_of(npk_handle_t h) {
    return static_cast<uint32_t>(static_cast<uint64_t>(h) >> 32);
}
inline uint16_t arena_of(npk_handle_t h) {
    return static_cast<uint16_t>((static_cast<uint64_t>(h) >> 16) & 0xFFFFu);
}
inline uint16_t slot_of(npk_handle_t h) {
    return static_cast<uint16_t>(static_cast<uint64_t>(h) & 0xFFFFu);
}

// Caller must hold g_mu().
Arena* arena_locked(uint16_t arena_id) {
    if (arena_id == 0) return nullptr;
    auto& vec = g_arenas();
    if (arena_id >= vec.size()) return nullptr;
    Arena* a = vec[arena_id];
    return (a && a->alive) ? a : nullptr;
}

}  // namespace

extern "C" {

int64_t npk_handle_arena_create(void) {
    std::lock_guard<std::mutex> lock(g_mu());
    auto& vec = g_arenas();
    if (vec.empty()) {
        vec.push_back(nullptr);   // reserve id = 0 for "null arena"
    }
    if (vec.size() > kMaxArenaId) {
        return 0;                  // arena id space exhausted
    }
    vec.push_back(new Arena());
    return static_cast<int64_t>(vec.size() - 1);
}

void npk_handle_arena_destroy(int64_t arena) {
    if (arena <= 0 || arena > kMaxArenaId) return;
    std::lock_guard<std::mutex> lock(g_mu());
    auto& vec = g_arenas();
    if (static_cast<size_t>(arena) >= vec.size()) return;
    Arena* a = vec[static_cast<size_t>(arena)];
    if (!a) return;

    for (auto& s : a->slots) {
        if (s.in_use && s.data) {
            std::free(s.data);
        }
        s.data = nullptr;
        s.in_use = 0;
        // Bump generation so any outstanding handle to this slot
        // fails its generation check on subsequent deref / free.
        if (s.generation < kSaturatedGeneration) {
            s.generation++;
        }
    }
    a->alive = false;
    delete a;
    vec[static_cast<size_t>(arena)] = nullptr;
}

npk_handle_t npk_handle_alloc(int64_t arena, int64_t size) {
    if (size < 0) return NPK_HANDLE_NULL;
    if (arena <= 0 || arena > kMaxArenaId) return NPK_HANDLE_NULL;

    std::lock_guard<std::mutex> lock(g_mu());
    Arena* a = arena_locked(static_cast<uint16_t>(arena));
    if (!a) return NPK_HANDLE_NULL;

    size_t alloc_bytes = (size > 0) ? static_cast<size_t>(size) : 1;
    void* mem = std::calloc(1, alloc_bytes);
    if (!mem) return NPK_HANDLE_NULL;

    // Try the recycle list first. Saturated slots stay out of rotation.
    while (!a->free_list.empty()) {
        uint16_t slot_idx = a->free_list.back();
        a->free_list.pop_back();
        Slot& s = a->slots[slot_idx];
        if (s.generation >= kSaturatedGeneration) {
            // Slot is permanently retired; drop it on the floor.
            continue;
        }
        s.data    = mem;
        s.size    = size;
        s.in_use  = 1;
        return pack(s.generation, static_cast<uint16_t>(arena), slot_idx);
    }

    if (a->slots.size() >= kMaxSlots) {
        std::free(mem);
        return NPK_HANDLE_NULL;
    }

    uint16_t slot_idx = static_cast<uint16_t>(a->slots.size());
    Slot s{};
    s.generation = kInitialGeneration;
    s.in_use     = 1;
    s.size       = size;
    s.data       = mem;
    a->slots.push_back(s);
    return pack(kInitialGeneration, static_cast<uint16_t>(arena), slot_idx);
}

void* npk_handle_deref(npk_handle_t h) {
    if (h == NPK_HANDLE_NULL) return nullptr;
    std::lock_guard<std::mutex> lock(g_mu());
    Arena* a = arena_locked(arena_of(h));
    if (!a) return nullptr;
    uint16_t idx = slot_of(h);
    if (idx >= a->slots.size()) return nullptr;
    Slot& s = a->slots[idx];
    if (!s.in_use) return nullptr;
    if (s.generation != gen_of(h)) return nullptr;
    return s.data;
}

void npk_handle_free(npk_handle_t h) {
    if (h == NPK_HANDLE_NULL) return;
    std::lock_guard<std::mutex> lock(g_mu());
    Arena* a = arena_locked(arena_of(h));
    if (!a) return;
    uint16_t idx = slot_of(h);
    if (idx >= a->slots.size()) return;
    Slot& s = a->slots[idx];
    if (!s.in_use) return;
    if (s.generation != gen_of(h)) return;

    std::free(s.data);
    s.data   = nullptr;
    s.in_use = 0;

    if (s.generation < kSaturatedGeneration) {
        s.generation++;
        a->free_list.push_back(idx);
    }
    // Else: leave saturated; never recycled, no double-free risk.
}

}  // extern "C"

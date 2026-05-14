/**
 * Nitpick GC Allocator Implementation
 * 
 * This file implements the memory allocation logic for the Nitpick
 * Garbage Collection System. It provides the npk_gc_alloc function
 * and the Nursery allocator infrastructure.
 * 
 * Allocation Strategy:
 * 1. Fast path: Bump pointer allocation in nursery (O(1))
 * 2. Fragment search: If pinned objects exist, search free gaps
 * 3. GC trigger: If nursery full, trigger minor GC
 * 4. OOM: If still failing, trigger major GC or return NULL
 * 
 * Reference: research_021_garbage_collection_system.txt
 */

#include "gc_internal.h"
#include "runtime/process.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <algorithm>
#include <sys/mman.h>  // For mmap (nursery allocation)

namespace npk {
namespace runtime {

// =============================================================================
// Nursery Implementation
// =============================================================================

Nursery::Nursery(size_t size) : capacity(size), used(0) {
    // Allocate nursery using mmap for alignment and large pages
    // PROT_READ | PROT_WRITE: Memory is readable and writable
    // MAP_PRIVATE | MAP_ANONYMOUS: Private, not backed by file
    start_addr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (start_addr == MAP_FAILED) {
        // Fallback to malloc if mmap fails
        start_addr = std::malloc(size);
        if (!start_addr) {
            throw std::bad_alloc();
        }
    }
    
    bump_ptr = start_addr;
    end_addr = (char*)start_addr + size;
}

Nursery::~Nursery() {
    // Try to unmap if it was mmapped
    if (start_addr != nullptr) {
        // Check if munmap succeeds (indicates it was mmapped)
        if (munmap(start_addr, capacity) != 0) {
            // Not mmapped, use free
            std::free(start_addr);
        }
    }
}

void* Nursery::allocate(size_t obj_size, uint16_t type_id) {
    // Total allocation size: header + object payload
    size_t total_size = sizeof(ObjHeader) + obj_size;
    
    // Align to 8 bytes for performance
    total_size = (total_size + 7) & ~7;
    
    // Fast path: Bump pointer allocation
    void* alloc_ptr = bump_ptr;
    void* new_bump_ptr = (char*)alloc_ptr + total_size;
    
    if (new_bump_ptr <= end_addr) {
        // Allocation succeeds
        bump_ptr = new_bump_ptr;
        used += total_size;
        
        // Initialize header
        ObjHeader* header = static_cast<ObjHeader*>(alloc_ptr);
        std::memset(header, 0, sizeof(ObjHeader));
        header->is_nursery = 1;
        header->type_id = type_id;
        header->size_class = total_size / 8;  // Simplified size class
        
        // Return pointer after header
        void* obj_ptr = (char*)alloc_ptr + sizeof(ObjHeader);
        
        // Zero-initialize payload (important for safety)
        std::memset(obj_ptr, 0, obj_size);
        
        return obj_ptr;
    }
    
    // Slow path: Try to find a suitable fragment
    for (size_t i = 0; i < fragments.size(); ++i) {
        Fragment& frag = fragments[i];
        
        if (frag.size >= total_size) {
            // Found suitable fragment
            alloc_ptr = frag.start;
            
            // Update fragment (shrink or remove)
            frag.start = (char*)frag.start + total_size;
            frag.size -= total_size;
            
            if (frag.size < sizeof(ObjHeader) + 8) {
                // Fragment too small to be useful, remove it
                fragments.erase(fragments.begin() + i);
            }
            
            used += total_size;
            
            // Initialize header
            ObjHeader* header = static_cast<ObjHeader*>(alloc_ptr);
            std::memset(header, 0, sizeof(ObjHeader));
            header->is_nursery = 1;
            header->type_id = type_id;
            header->size_class = total_size / 8;
            
            // Return pointer after header
            void* obj_ptr = (char*)alloc_ptr + sizeof(ObjHeader);
            std::memset(obj_ptr, 0, obj_size);
            
            return obj_ptr;
        }
    }
    
    // No space available - caller must trigger GC
    return nullptr;
}

void Nursery::reset_with_pinned() {
    /**
     * Fragmented Nursery Reset Algorithm
     * 
     * When pinned objects exist, we cannot simply reset bump_ptr to start_addr
     * (that would overwrite pinned objects on next allocation).
     * 
     * Instead:
     * 1. Collect all pinned object regions
     * 2. Sort by address
     * 3. Find gaps between pinned regions
     * 4. Create Fragment structs for each gap
     * 5. Reset bump_ptr to first available space
     * 
     * This transforms the nursery into a freelist-backed space when pinning
     * is active, with performance degradation from O(1) to O(N) allocation.
     */
    
    if (pinned_objects.empty()) {
        // No pinned objects - simple reset
        bump_ptr = start_addr;
        used = 0;
        fragments.clear();
        return;
    }
    
    // Collect pinned regions (address ranges)
    struct Region {
        void* start;
        void* end;
        
        bool operator<(const Region& other) const {
            return start < other.start;
        }
    };
    
    std::vector<Region> pinned_regions;
    pinned_regions.reserve(pinned_objects.size());
    
    for (void* obj_ptr : pinned_objects) {
        // Get header (before object pointer)
        ObjHeader* header = (ObjHeader*)((char*)obj_ptr - sizeof(ObjHeader));
        
        // Calculate region: [header_start, obj_end)
        void* region_start = (void*)header;
        size_t obj_size = header->size_class * 8;  // Reconstruct size
        void* region_end = (char*)region_start + obj_size;
        
        pinned_regions.push_back({region_start, region_end});
    }
    
    // Sort by address
    std::sort(pinned_regions.begin(), pinned_regions.end());
    
    // Find gaps and create fragments
    fragments.clear();
    void* prev_end = start_addr;
    
    for (const Region& region : pinned_regions) {
        if (region.start > prev_end) {
            // Gap exists: [prev_end, region.start)
            size_t gap_size = (char*)region.start - (char*)prev_end;
            
            // Only create fragment if gap is useful (>= min allocation)
            if (gap_size >= sizeof(ObjHeader) + 8) {
                fragments.emplace_back(prev_end, region.start);
            }
        }
        
        prev_end = region.end;
    }
    
    // Final gap: [last_pinned_end, end_addr)
    if (prev_end < end_addr) {
        size_t gap_size = (char*)end_addr - (char*)prev_end;
        if (gap_size >= sizeof(ObjHeader) + 8) {
            fragments.emplace_back(prev_end, end_addr);
        }
    }
    
    // Update bump pointer to first fragment (or end if no fragments)
    if (!fragments.empty()) {
        bump_ptr = fragments[0].start;
    } else {
        // Completely fragmented - bump_ptr is unusable
        bump_ptr = end_addr;
    }
    
    // Recalculate used memory (sum of pinned regions)
    used = 0;
    for (const Region& region : pinned_regions) {
        used += (char*)region.end - (char*)region.start;
    }
}

// =============================================================================
// OldGeneration Implementation
// =============================================================================

OldGeneration::OldGeneration(size_t threshold) 
    : used(0), threshold(threshold) {
    objects.reserve(1024);  // Pre-allocate for efficiency
}

void* OldGeneration::allocate(size_t obj_size, uint16_t type_id) {
    // Total size: header + payload
    size_t total_size = sizeof(ObjHeader) + obj_size;
    
    // Align to 8 bytes
    total_size = (total_size + 7) & ~7;
    
    // Use malloc for old generation objects
    void* alloc_ptr = std::malloc(total_size);
    if (!alloc_ptr) {
        return nullptr;  // OOM
    }
    
    used += total_size;
    
    // Initialize header
    ObjHeader* header = static_cast<ObjHeader*>(alloc_ptr);
    std::memset(header, 0, sizeof(ObjHeader));
    header->is_nursery = 0;  // Old generation
    header->type_id = type_id;
    header->size_class = total_size / 8;
    
    // Return pointer after header
    void* obj_ptr = (char*)alloc_ptr + sizeof(ObjHeader);
    std::memset(obj_ptr, 0, obj_size);
    
    // Track for sweeping
    objects.push_back(obj_ptr);
    
    return obj_ptr;
}

void OldGeneration::add_object(void* ptr) {
    if (ptr) {
        objects.push_back(ptr);
        
        // Update header: mark as old generation
        ObjHeader* header = (ObjHeader*)((char*)ptr - sizeof(ObjHeader));
        header->is_nursery = 0;
    }
}

bool OldGeneration::contains(void* ptr) const {
    // Check if pointer is in our tracked objects
    // This is O(N) but only called during GC/debugging
    return std::find(objects.begin(), objects.end(), ptr) != objects.end();
}

// =============================================================================
// CardTable Implementation
// =============================================================================

CardTable::CardTable(void* heap_start, size_t heap_size) 
    : heap_start(heap_start) {
    num_cards = (heap_size + CARD_SIZE - 1) / CARD_SIZE;
    cards = new uint8_t[num_cards];
    clear();
}

CardTable::~CardTable() {
    delete[] cards;
}

void CardTable::mark_dirty(void* addr) {
    // Calculate card index
    size_t offset = (char*)addr - (char*)heap_start;
    size_t card_idx = offset >> CARD_SHIFT;  // Divide by 512
    
    if (card_idx < num_cards) {
        cards[card_idx] = CARD_DIRTY;
    }
}

std::vector<void*> CardTable::get_dirty_cards() {
    std::vector<void*> dirty;
    
    for (size_t i = 0; i < num_cards; ++i) {
        if (cards[i] == CARD_DIRTY) {
            void* card_addr = (char*)heap_start + (i * CARD_SIZE);
            dirty.push_back(card_addr);
        }
    }
    
    return dirty;
}

void CardTable::clear() {
    std::memset(cards, CARD_CLEAN, num_cards);
}

// =============================================================================
// C API Implementation
// =============================================================================

extern "C" {

void* npk_gc_alloc(size_t size, uint16_t type_id) {
    return GCState::instance().alloc(size, type_id);
}

void npk_gc_pin(void* ptr) {
    GCState::instance().pin(ptr);
}

void npk_gc_unpin(void* ptr) {
    GCState::instance().unpin(ptr);
}

void npk_gc_collect(bool full_collection) {
    GCState::instance().collect(full_collection);
}

void npk_gc_get_stats(GCStats* stats) {
    GCState::instance().get_stats(stats);
}

void npk_shadow_stack_push_frame(void) {
    GCState::instance().push_frame();
}

void npk_shadow_stack_pop_frame(void) {
    GCState::instance().pop_frame();
}

void npk_shadow_stack_add_root(void** root_addr) {
    GCState::instance().add_root(root_addr);
}

void npk_shadow_stack_remove_root(void** root_addr) {
    GCState::instance().remove_root(root_addr);
}

void npk_gc_write_barrier(void* obj, void* ref) {
    GCState::instance().write_barrier(obj, ref);
}

ObjHeader* npk_gc_get_header(void* ptr) {
    return GCState::instance().get_header(ptr);
}

bool npk_gc_is_heap_pointer(void* ptr) {
    return GCState::instance().is_heap_pointer(ptr);
}

// v0.26.5 / MEM-013: ABI-safe variant — see header for rationale.
int32_t npk_gc_is_heap_pointer_i32(void* ptr) {
    return GCState::instance().is_heap_pointer(ptr) ? 1 : 0;
}

void npk_gc_init(size_t nursery_size, size_t old_gen_threshold) {
    GCState::instance().init(nursery_size, old_gen_threshold);
    
    // Register fork safety handlers so npk_fork() doesn't deadlock
    // on GC mutexes held by concurrent mark/sweep threads
    static bool gc_atfork_registered = false;
    if (!gc_atfork_registered) {
        npk_atfork_register(
            []() { GCState::instance().lock_for_fork(); },
            []() { GCState::instance().unlock_for_fork(); },
            []() { GCState::instance().unlock_for_fork(); }
        );
        gc_atfork_registered = true;
    }
}

void npk_gc_shutdown(void) {
    GCState::instance().shutdown();
}

void npk_gc_free(void* ptr) {
    // Release GC-visible memory (coroutine frames etc.) back to free store
    if (ptr) {
        ::free(ptr);
    }
}

// =============================================================================
// v0.8.0: GC Statistics at Exit & Max Heap
// =============================================================================

static bool gc_stats_at_exit_enabled = false;

static void npk_gc_print_stats_at_exit(void) {
    if (!gc_stats_at_exit_enabled) return;
    
    GCStats stats;
    npk_gc_get_stats(&stats);
    
    fprintf(stderr, "\n[GC STATS] ═══════════════════════════════════════\n");
    fprintf(stderr, "[GC STATS] Nursery:    %zu / %zu bytes (%.1f%% used)\n",
            stats.nursery_used, stats.nursery_size,
            stats.nursery_size ? (100.0 * stats.nursery_used / stats.nursery_size) : 0.0);
    fprintf(stderr, "[GC STATS] Old Gen:    %zu / %zu bytes\n",
            stats.old_gen_used, stats.old_gen_size);
    fprintf(stderr, "[GC STATS] Allocated:  %zu bytes total\n", stats.total_allocated);
    fprintf(stderr, "[GC STATS] Collected:  %zu bytes total\n", stats.total_collected);
    fprintf(stderr, "[GC STATS] Minor GCs:  %" PRIu64 "\n", stats.num_minor_collections);
    fprintf(stderr, "[GC STATS] Major GCs:  %" PRIu64 "\n", stats.num_major_collections);
    fprintf(stderr, "[GC STATS] Pinned:     %zu objects\n", stats.num_pinned_objects);
    fprintf(stderr, "[GC STATS] ── Concurrent (v0.8.1) ──────────────────\n");
    fprintf(stderr, "[GC STATS] Conc Marks: %" PRIu64 "\n", stats.num_concurrent_marks);
    fprintf(stderr, "[GC STATS] Inc Sweeps: %" PRIu64 "\n", stats.num_incremental_sweeps);
    fprintf(stderr, "[GC STATS] Pause Tot:  %" PRIu64 " ns (%.3f ms)\n",
            stats.total_pause_time_ns, stats.total_pause_time_ns / 1000000.0);
    fprintf(stderr, "[GC STATS] Pause Max:  %" PRIu64 " ns (%.3f ms)\n",
            stats.max_pause_time_ns, stats.max_pause_time_ns / 1000000.0);
    fprintf(stderr, "[GC STATS] Pause Last: %" PRIu64 " ns (%.3f ms)\n",
            stats.last_pause_time_ns, stats.last_pause_time_ns / 1000000.0);
    fprintf(stderr, "[GC STATS] ═══════════════════════════════════════\n");
}

void npk_gc_enable_stats_at_exit(uint8_t enable) {
    if (enable && !gc_stats_at_exit_enabled) {
        gc_stats_at_exit_enabled = true;
        atexit(npk_gc_print_stats_at_exit);
    } else if (!enable) {
        gc_stats_at_exit_enabled = false;
    }
}

void npk_gc_set_max_heap(uint64_t max_bytes) {
    GCState::instance().set_max_heap(max_bytes);
}

void npk_gc_register_finalizer(uint16_t type_id, AriaFinalizer finalizer) {
    GCState::instance().register_finalizer(type_id, finalizer);
}

void npk_gc_register_type_layout(uint16_t type_id, const size_t* ref_offsets, size_t num_refs) {
    GCState::instance().register_type_layout(type_id, ref_offsets, num_refs);
}

void npk_gc_enable_concurrent(uint8_t enable) {
    GCState::instance().enable_concurrent(enable != 0);
}

// v0.26.4 / MEM-011: tuning observability — small wrappers over GCStats
// and concurrent_enabled, used by .npk env-var regression fixtures.
size_t npk_gc_nursery_size_bytes(void) {
    auto& gc = GCState::instance();
    if (!gc.is_initialized()) gc.init(0, 0);
    GCStats s;
    gc.get_stats(&s);
    return s.nursery_size;
}

size_t npk_gc_old_gen_threshold_bytes(void) {
    auto& gc = GCState::instance();
    if (!gc.is_initialized()) gc.init(0, 0);
    GCStats s;
    gc.get_stats(&s);
    return s.old_gen_size;
}

uint8_t npk_gc_concurrent_enabled(void) {
    auto& gc = GCState::instance();
    if (!gc.is_initialized()) gc.init(0, 0);
    return gc.is_concurrent_enabled() ? 1u : 0u;
}

void npk_gc_safepoint(void) {
    GCState::instance().safepoint();
}

void npk_gc_register_jit_root(void** root_addr) {
    GCState::instance().register_jit_root(root_addr);
}

void npk_gc_unregister_jit_root(void** root_addr) {
    GCState::instance().unregister_jit_root(root_addr);
}

} // extern "C"

} // namespace runtime
} // namespace npk

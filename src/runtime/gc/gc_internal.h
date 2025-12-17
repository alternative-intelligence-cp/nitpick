/**
 * Aria GC Internal Implementation
 * 
 * This header defines the internal data structures and algorithms
 * for the Aria Garbage Collection System. It should not be included
 * by user code.
 * 
 * Architecture:
 * - Generational: Nursery (young) + Old Generation
 * - Nursery: Copying collector with fragmentation tolerance for pinned objects
 * - Old Gen: Mark-sweep collector with malloc-backed allocation
 * - Rooting: Explicit shadow stack (no stack maps)
 * - Barriers: Card table for old-to-young references
 * 
 * Reference: research_021_garbage_collection_system.txt
 */

#ifndef ARIA_RUNTIME_GC_INTERNAL_H
#define ARIA_RUNTIME_GC_INTERNAL_H

#include "runtime/gc.h"
#include <vector>
#include <unordered_set>
#include <mutex>

namespace aria {
namespace runtime {

// =============================================================================
// Memory Regions
// =============================================================================

/**
 * Fragment: Free region in a fragmented nursery
 * 
 * When objects are pinned during minor GC, the nursery cannot be
 * simply reset. Instead, we track free gaps between pinned objects
 * as fragments. Allocation searches these fragments before falling
 * back to the global bump pointer.
 */
struct Fragment {
    void* start;      // Start address of free region
    void* end;        // End address (exclusive)
    size_t size;      // end - start
    
    Fragment(void* s, void* e) 
        : start(s), end(e), size((char*)e - (char*)s) {}
};

/**
 * Nursery: Young generation allocator
 * 
 * Uses a bump pointer allocator for fast O(1) allocation.
 * Degrades to freelist allocation when objects are pinned.
 * 
 * Allocation Algorithm:
 * 1. Try bump pointer: if (bump_ptr + size <= end_addr)
 * 2. Try fragments: search fragment_list for suitable gap
 * 3. Trigger minor GC and retry
 * 4. If still failing, trigger major GC or OOM
 */
struct Nursery {
    void* start_addr;              // Nursery base address
    void* bump_ptr;                // Current allocation pointer
    void* end_addr;                // Nursery limit
    size_t capacity;               // Total size (bytes)
    size_t used;                   // Current utilization
    
    std::vector<Fragment> fragments;  // Free gaps (when pinned objects exist)
    std::unordered_set<void*> pinned_objects;  // Pinned object set
    
    Nursery(size_t size);
    ~Nursery();
    
    // Allocate from nursery (may trigger GC)
    void* allocate(size_t size, uint16_t type_id);
    
    // Reset after minor GC (reconstruct fragments)
    void reset_with_pinned();
    
    // Check if pointer is in nursery
    bool contains(void* ptr) const {
        return ptr >= start_addr && ptr < end_addr;
    }
};

/**
 * OldGeneration: Tenured object space
 * 
 * Uses malloc/free for allocation (relies on system allocator).
 * Tracks all live objects in a vector for mark-sweep collection.
 */
struct OldGeneration {
    std::vector<void*> objects;    // All old gen objects (for sweeping)
    size_t used;                   // Current utilization (bytes)
    size_t threshold;              // Major GC trigger threshold
    
    OldGeneration(size_t threshold);
    
    // Allocate in old generation
    void* allocate(size_t size, uint16_t type_id);
    
    // Add evacuated object from nursery
    void add_object(void* ptr);
    
    // Check if pointer is in old generation
    bool contains(void* ptr) const;
};

// =============================================================================
// Card Table (Write Barrier Support)
// =============================================================================

/**
 * CardTable: Track old-to-young references
 * 
 * Divides the heap into fixed-size cards (512 bytes).
 * Each card is mapped to a byte:
 * - CLEAN (0): No old-to-young references in this card
 * - DIRTY (1): Card may contain old-to-young references
 * 
 * During minor GC, DIRTY cards in old generation are scanned
 * to find additional roots for nursery tracing.
 */
class CardTable {
public:
    static constexpr size_t CARD_SIZE = 512;  // Bytes per card
    static constexpr size_t CARD_SHIFT = 9;   // log2(512)
    
    static constexpr uint8_t CARD_CLEAN = 0;
    static constexpr uint8_t CARD_DIRTY = 1;
    
    CardTable(void* heap_start, size_t heap_size);
    ~CardTable();
    
    // Mark card as dirty (write barrier)
    void mark_dirty(void* addr);
    
    // Get dirty cards for scanning
    std::vector<void*> get_dirty_cards();
    
    // Clear all cards (after GC)
    void clear();
    
private:
    uint8_t* cards;       // Card array
    size_t num_cards;     // Array length
    void* heap_start;     // Heap base for offset calculation
};

// =============================================================================
// Shadow Stack (Root Tracking)
// =============================================================================

/**
 * ShadowFrame: Stack frame for GC roots
 * 
 * Tracks root addresses for a single function activation.
 */
struct ShadowFrame {
    std::vector<void**> roots;  // Root addresses (e.g., &local_var)
    ShadowFrame* prev;          // Previous frame (linked list)
    
    ShadowFrame(ShadowFrame* p) : prev(p) {}
};

/**
 * ShadowStack: Thread-local root tracking
 * 
 * Maintains a linked list of shadow frames, one per active function
 * that has GC-managed locals. The GC scans this structure to find
 * all roots during collection.
 */
class ShadowStack {
public:
    ShadowStack() : top(nullptr) {}
    ~ShadowStack();
    
    void push_frame();
    void pop_frame();
    void add_root(void** root_addr);
    void remove_root(void** root_addr);
    
    // Get all roots for GC scanning
    std::vector<void**> get_all_roots() const;
    
private:
    ShadowFrame* top;  // Current frame (top of stack)
};

// =============================================================================
// GC State
// =============================================================================

/**
 * GCState: Global garbage collector state
 * 
 * Singleton structure managing all GC components.
 * Access is synchronized via mutexes for thread safety.
 */
class GCState {
public:
    static GCState& instance();
    
    void init(size_t nursery_size, size_t old_gen_threshold);
    void shutdown();
    
    // Allocation
    void* alloc(size_t size, uint16_t type_id);
    void pin(void* ptr);
    void unpin(void* ptr);
    
    // Collection
    void collect(bool full);
    void minor_gc();  // Nursery only
    void major_gc();  // Old generation
    
    // Rooting
    void push_frame();
    void pop_frame();
    void add_root(void** root_addr);
    void remove_root(void** root_addr);
    
    // Write barrier
    void write_barrier(void* obj, void* ref);
    
    // Queries
    bool is_heap_pointer(void* ptr) const;
    ObjHeader* get_header(void* ptr) const;
    void get_stats(GCStats* stats) const;
    
private:
    GCState() : initialized(false), collecting(false) {}
    ~GCState() { shutdown(); }
    
    // No copy/move
    GCState(const GCState&) = delete;
    GCState& operator=(const GCState&) = delete;
    
    bool initialized;
    bool collecting;  // GC in progress flag
    
    Nursery* nursery;
    OldGeneration* old_gen;
    CardTable* card_table;
    
    // Thread-local shadow stacks (for now, single-threaded)
    ShadowStack shadow_stack;
    
    // Statistics
    GCStats stats;
    
    // Synchronization
    mutable std::mutex gc_mutex;
    
    // Collection helpers
    void mark_object(void* ptr);
    void sweep_old_gen();
    void evacuate_nursery();
    void* evacuate_object(void* ptr);  // Copy to old gen
    void scan_roots();
};

} // namespace runtime
} // namespace aria

#endif // ARIA_RUNTIME_GC_INTERNAL_H

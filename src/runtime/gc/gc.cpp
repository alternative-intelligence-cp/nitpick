/**
 * Aria GC Core Implementation
 * 
 * This file implements the main garbage collection algorithms:
 * - Minor GC: Copying collector for nursery (with pinning support)
 * - Major GC: Mark-sweep collector for old generation
 * - Shadow stack management
 * - GC state coordination
 * 
 * Reference: research_021_garbage_collection_system.txt
 */

#include "gc_internal.h"
#include <algorithm>
#include <iostream>
#include <cstring>

namespace aria {
namespace runtime {

// =============================================================================
// ShadowStack Implementation
// =============================================================================

ShadowStack::~ShadowStack() {
    // Clean up all frames
    while (top) {
        pop_frame();
    }
}

void ShadowStack::push_frame() {
    top = new ShadowFrame(top);
}

void ShadowStack::pop_frame() {
    if (!top) return;
    
    ShadowFrame* old_top = top;
    top = top->prev;
    delete old_top;
}

void ShadowStack::add_root(void** root_addr) {
    if (top) {
        top->roots.push_back(root_addr);
    }
}

void ShadowStack::remove_root(void** root_addr) {
    if (!top) return;
    
    auto& roots = top->roots;
    roots.erase(std::remove(roots.begin(), roots.end(), root_addr), roots.end());
}

std::vector<void**> ShadowStack::get_all_roots() const {
    std::vector<void**> all_roots;
    
    // Walk the frame chain
    for (ShadowFrame* frame = top; frame != nullptr; frame = frame->prev) {
        all_roots.insert(all_roots.end(), frame->roots.begin(), frame->roots.end());
    }
    
    return all_roots;
}

// =============================================================================
// GCState Implementation
// =============================================================================

GCState& GCState::instance() {
    static GCState inst;
    return inst;
}

void GCState::init(size_t nursery_size, size_t old_gen_threshold) {
    std::lock_guard<std::mutex> lock(gc_mutex);
    
    if (initialized) {
        return;  // Already initialized
    }
    
    // Default sizes if not specified
    if (nursery_size == 0) {
        nursery_size = 4 * 1024 * 1024;  // 4MB default
    }
    if (old_gen_threshold == 0) {
        old_gen_threshold = 64 * 1024 * 1024;  // 64MB default
    }
    
    // Initialize components
    nursery = new Nursery(nursery_size);
    old_gen = new OldGeneration(old_gen_threshold);
    
    // Card table covers both nursery and old gen (worst case: 128MB = 256K cards)
    size_t total_heap = nursery_size + old_gen_threshold;
    card_table = new CardTable(nursery->start_addr, total_heap);
    
    // Initialize stats
    stats = {};
    stats.nursery_size = nursery_size;
    stats.old_gen_size = 0;
    
    initialized = true;
}

void GCState::shutdown() {
    std::lock_guard<std::mutex> lock(gc_mutex);
    
    if (!initialized) return;
    
    delete nursery;
    delete old_gen;
    delete card_table;
    
    nursery = nullptr;
    old_gen = nullptr;
    card_table = nullptr;
    
    initialized = false;
}

void* GCState::alloc(size_t size, uint16_t type_id) {
    std::lock_guard<std::mutex> lock(gc_mutex);
    
    if (!initialized) {
        init(0, 0);  // Auto-initialize with defaults
    }
    
    // Try to allocate in nursery
    void* ptr = nursery->allocate(size, type_id);
    
    if (ptr) {
        stats.total_allocated += size;
        stats.nursery_used = nursery->used;
        return ptr;
    }
    
    // Nursery full - trigger minor GC
    minor_gc();
    
    // Retry allocation
    ptr = nursery->allocate(size, type_id);
    
    if (ptr) {
        stats.total_allocated += size;
        stats.nursery_used = nursery->used;
        return ptr;
    }
    
    // Still failing - trigger major GC
    major_gc();
    
    // Final retry
    ptr = nursery->allocate(size, type_id);
    
    if (ptr) {
        stats.total_allocated += size;
        stats.nursery_used = nursery->used;
        return ptr;
    }
    
    // Out of memory
    std::cerr << "Aria GC: Out of memory!\n";
    return nullptr;
}

void GCState::pin(void* ptr) {
    std::lock_guard<std::mutex> lock(gc_mutex);
    
    if (!ptr) return;
    
    // Get header
    ObjHeader* header = get_header(ptr);
    if (!header) return;
    
    // Set pinned bit
    header->pinned_bit = 1;
    
    // Track in nursery
    if (header->is_nursery && nursery) {
        nursery->pinned_objects.insert(ptr);
        stats.num_pinned_objects++;
    }
}

void GCState::unpin(void* ptr) {
    std::lock_guard<std::mutex> lock(gc_mutex);
    
    if (!ptr) return;
    
    ObjHeader* header = get_header(ptr);
    if (!header) return;
    
    // Clear pinned bit
    header->pinned_bit = 0;
    
    // Remove from tracking
    if (header->is_nursery && nursery) {
        nursery->pinned_objects.erase(ptr);
        if (stats.num_pinned_objects > 0) {
            stats.num_pinned_objects--;
        }
    }
}

void GCState::collect(bool full) {
    std::lock_guard<std::mutex> lock(gc_mutex);
    
    if (!initialized || collecting) {
        return;  // Already collecting or not initialized
    }
    
    collecting = true;
    
    if (full) {
        major_gc();
    } else {
        minor_gc();
    }
    
    collecting = false;
}

void GCState::minor_gc() {
    /**
     * Minor GC: Evacuate nursery to old generation
     * 
     * Algorithm:
     * 1. Scan all roots (shadow stack)
     * 2. For each root pointing to nursery:
     *    a. If object is pinned: mark as live, don't move
     *    b. If object is unpinned: evacuate to old gen
     * 3. Reconstruct nursery (handle pinned objects)
     * 4. Clear card table
     * 
     * This is a stop-the-world copying collector with pinning support.
     */
    
    if (!initialized) return;
    
    stats.num_minor_collections++;
    
    // Scan roots
    auto roots = shadow_stack.get_all_roots();
    
    for (void** root_addr : roots) {
        void* obj_ptr = *root_addr;
        
        if (!obj_ptr || !nursery->contains(obj_ptr)) {
            continue;  // Not a nursery object
        }
        
        ObjHeader* header = get_header(obj_ptr);
        if (!header) continue;
        
        // Check if pinned
        if (header->pinned_bit) {
            // Pinned object - mark as live but don't move
            header->mark_bit = 1;
            continue;
        }
        
        // Evacuate to old generation
        void* new_ptr = evacuate_object(obj_ptr);
        
        if (new_ptr) {
            // Update root to point to new location
            *root_addr = new_ptr;
        }
    }
    
    // Reconstruct nursery (handle fragments from pinned objects)
    nursery->reset_with_pinned();
    
    // Clear card table
    card_table->clear();
    
    // Update stats
    stats.nursery_used = nursery->used;
    stats.old_gen_used = old_gen->used;
}

void* GCState::evacuate_object(void* ptr) {
    /**
     * Evacuate object from nursery to old generation
     * 
     * Steps:
     * 1. Get object size from header
     * 2. Allocate in old generation
     * 3. Copy header + payload
     * 4. Update header metadata
     * 5. Set forwarding pointer in old location
     * 6. Return new address
     */
    
    if (!ptr) return nullptr;
    
    ObjHeader* old_header = get_header(ptr);
    if (!old_header) return nullptr;
    
    // Check if already forwarded
    if (old_header->forwarded_bit) {
        // Already evacuated - return forwarding address
        // The forwarding address is stored in the first word of the old payload
        void** forward_ptr = (void**)ptr;
        return *forward_ptr;
    }
    
    // Get object size
    size_t obj_size = old_header->size_class * 8 - sizeof(ObjHeader);
    
    // Allocate in old generation
    void* new_ptr = old_gen->allocate(obj_size, old_header->type_id);
    
    if (!new_ptr) {
        // Old gen allocation failed - trigger major GC
        // For now, just return nullptr (proper implementation would retry)
        return nullptr;
    }
    
    // Copy payload
    std::memcpy(new_ptr, ptr, obj_size);
    
    // Mark old location as forwarded
    old_header->forwarded_bit = 1;
    
    // Store forwarding address in old payload
    void** forward_ptr = (void**)ptr;
    *forward_ptr = new_ptr;
    
    // Update statistics
    stats.total_collected += (old_header->size_class * 8);
    
    return new_ptr;
}

void GCState::major_gc() {
    /**
     * Major GC: Mark-sweep for old generation
     * 
     * Algorithm:
     * 1. Mark Phase: Starting from roots, mark all reachable objects
     * 2. Sweep Phase: Free unmarked objects, reset marks for next cycle
     * 
     * This is a simple stop-the-world mark-sweep collector.
     */
    
    if (!initialized) return;
    
    stats.num_major_collections++;
    
    // =========================================================================
    // Mark Phase
    // =========================================================================
    
    // Scan all roots
    auto roots = shadow_stack.get_all_roots();
    
    for (void** root_addr : roots) {
        void* obj_ptr = *root_addr;
        if (obj_ptr) {
            mark_object(obj_ptr);
        }
    }
    
    // Also scan nursery objects that reference old gen
    // (For simplicity, we'll skip this in the basic implementation)
    
    // =========================================================================
    // Sweep Phase
    // =========================================================================
    
    sweep_old_gen();
    
    // Update stats
    stats.old_gen_used = old_gen->used;
}

void GCState::mark_object(void* ptr) {
    /**
     * Mark phase: Recursively mark reachable objects
     * 
     * This is a simplified implementation that marks objects
     * but doesn't trace their references (would require type information).
     * 
     * A full implementation would:
     * 1. Use type_id to look up object layout
     * 2. Scan fields for references
     * 3. Recursively mark referenced objects
     */
    
    if (!ptr) return;
    
    ObjHeader* header = get_header(ptr);
    if (!header) return;
    
    // Already marked?
    if (header->mark_bit) return;
    
    // Mark this object
    header->mark_bit = 1;
    
    // TODO: Trace references (requires type information)
    // For now, we just mark the object itself
}

void GCState::sweep_old_gen() {
    /**
     * Sweep phase: Free unmarked objects
     * 
     * Iterates through old generation objects:
     * - If mark_bit == 1: Object is live, reset mark for next cycle
     * - If mark_bit == 0: Object is dead, free it
     * 
     * Uses swap-remove optimization for O(1) deletion from vector.
     */
    
    auto& objects = old_gen->objects;
    size_t bytes_freed = 0;
    
    for (size_t i = 0; i < objects.size(); ) {
        void* obj_ptr = objects[i];
        ObjHeader* header = get_header(obj_ptr);
        
        if (!header) {
            // Corrupted header - skip
            ++i;
            continue;
        }
        
        if (header->mark_bit) {
            // Live object - reset mark bit for next cycle
            header->mark_bit = 0;
            ++i;
        } else {
            // Dead object - free it
            size_t obj_size = header->size_class * 8;
            bytes_freed += obj_size;
            
            // Free memory
            void* alloc_ptr = (char*)obj_ptr - sizeof(ObjHeader);
            std::free(alloc_ptr);
            
            // Remove from vector (swap with last and pop)
            objects[i] = objects.back();
            objects.pop_back();
            // Don't increment i - we moved a new element to position i
        }
    }
    
    // Update statistics
    old_gen->used -= bytes_freed;
    stats.total_collected += bytes_freed;
}

void GCState::push_frame() {
    shadow_stack.push_frame();
}

void GCState::pop_frame() {
    shadow_stack.pop_frame();
}

void GCState::add_root(void** root_addr) {
    shadow_stack.add_root(root_addr);
}

void GCState::remove_root(void** root_addr) {
    shadow_stack.remove_root(root_addr);
}

void GCState::write_barrier(void* obj, void* ref) {
    /**
     * Write Barrier: Track old-to-young references
     * 
     * Called after: obj.field = ref
     * 
     * If obj is in old generation and ref is in nursery,
     * mark the card containing obj as DIRTY.
     * 
     * During minor GC, DIRTY cards are scanned as additional roots.
     */
    
    if (!obj || !ref) return;
    
    ObjHeader* obj_header = get_header(obj);
    ObjHeader* ref_header = get_header(ref);
    
    if (!obj_header || !ref_header) return;
    
    // Only care about old-to-young references
    if (!obj_header->is_nursery && ref_header->is_nursery) {
        card_table->mark_dirty(obj);
    }
}

bool GCState::is_heap_pointer(void* ptr) const {
    if (!initialized || !ptr) return false;
    
    return (nursery && nursery->contains(ptr)) || 
           (old_gen && old_gen->contains(ptr));
}

ObjHeader* GCState::get_header(void* ptr) const {
    if (!ptr) return nullptr;
    
    // Header is immediately before the object pointer
    return (ObjHeader*)((char*)ptr - sizeof(ObjHeader));
}

void GCState::get_stats(GCStats* stats_out) const {
    if (!stats_out) return;
    
    std::lock_guard<std::mutex> lock(gc_mutex);
    *stats_out = stats;
}

} // namespace runtime
} // namespace aria

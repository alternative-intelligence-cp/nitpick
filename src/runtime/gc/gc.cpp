/**
 * Aria GC Core Implementation — v0.8.1
 * 
 * This file implements the main garbage collection algorithms:
 * - Minor GC: Copying collector for nursery (with pinning support)
 * - Major GC: Mark-sweep for old generation (concurrent or STW)
 * - Tri-color marking: WHITE → GRAY → BLACK
 * - Concurrent mark phase: SATB barrier + background thread
 * - Incremental sweep: Amortized across allocation calls
 * - GC safepoints: Mutator yield points for STW pauses
 * - Shadow stack management
 * - GC state coordination
 * 
 * Reference: research_021_garbage_collection_system.txt
 */

#include "gc_internal.h"
#include "runtime/async/gc_integration.h"
#include <algorithm>
#include <iostream>
#include <cstring>

namespace npk {
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
    stats.old_gen_size = old_gen_threshold;
    
    // v0.8.1: Initialize concurrent state
    concurrent_enabled = false;
    concurrent_marking = false;
    safepoint_requested = false;
    sweep_in_progress = false;
    sweep_cursor = 0;
    sweep_bytes_freed = 0;
    mark_thread = nullptr;
    gray_worklist.clear();
    satb_buffer.clear();
    
    initialized = true;
}

void GCState::shutdown() {
    std::lock_guard<std::mutex> lock(gc_mutex);
    
    if (!initialized) return;
    
    // Join background mark thread if running
    if (mark_thread) {
        concurrent_marking = false;
        if (mark_thread->joinable()) {
            mark_thread->join();
        }
        delete mark_thread;
        mark_thread = nullptr;
    }
    
    // Release safepoints
    safepoint_requested = false;
    safepoint_cv.notify_all();
    
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
    
    // v0.8.1: Incremental sweep — do a few sweep steps each allocation
    if (sweep_in_progress) {
        sweep_incremental(4);  // Sweep up to 4 objects per alloc
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
    
    // Check max heap limit before major GC promotion
    if (max_heap > 0) {
        size_t current_total = stats.nursery_used + stats.old_gen_used;
        if (current_total + size > max_heap) {
            // Try major GC to free old gen space first
            major_gc();
            current_total = stats.nursery_used + stats.old_gen_used;
            if (current_total + size > max_heap) {
                std::cerr << "Aria GC: Max heap limit exceeded (" << max_heap << " bytes)\n";
                return nullptr;
            }
        }
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
     * Minor GC is always stop-the-world (small pause).
     */
    
    if (!initialized) return;
    
    auto stw_start = std::chrono::high_resolution_clock::now();
    
    stats.num_minor_collections++;
    
    // Scan roots
    auto roots = shadow_stack.get_all_roots();
    
    // Worklist for Cheney-style breadth-first evacuation
    std::vector<void*> worklist;
    
    // v0.8.4: Scan suspended coroutine frames for GC roots
    GCCoroAllocator* coro_alloc = get_global_coro_allocator();
    if (coro_alloc) {
        coro_alloc->scan_frames([&](void* obj_ptr) {
            if (!obj_ptr || !nursery->contains(obj_ptr)) return;
            ObjHeader* header = get_header(obj_ptr);
            if (!header) return;
            if (header->pinned_bit) {
                header->color = GC_COLOR_BLACK;
                worklist.push_back(obj_ptr);
            } else {
                void* new_ptr = evacuate_object(obj_ptr);
                if (new_ptr) {
                    worklist.push_back(new_ptr);
                }
            }
        });
    }
    
    // v0.8.4: Scan JIT roots for nursery references
    {
        std::lock_guard<std::mutex> jlock(jit_roots_mutex);
        for (void** jit_root : jit_roots) {
            if (!jit_root) continue;
            void* obj_ptr = *jit_root;
            if (!obj_ptr || !nursery->contains(obj_ptr)) continue;
            ObjHeader* header = get_header(obj_ptr);
            if (!header) continue;
            if (header->pinned_bit) {
                header->color = GC_COLOR_BLACK;
                worklist.push_back(obj_ptr);
            } else {
                void* new_ptr = evacuate_object(obj_ptr);
                if (new_ptr) {
                    *jit_root = new_ptr;  // Update JIT root to new location
                    worklist.push_back(new_ptr);
                }
            }
        }
    }
    
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
            header->color = GC_COLOR_BLACK;
            worklist.push_back(obj_ptr);
            continue;
        }
        
        // Evacuate to old generation
        void* new_ptr = evacuate_object(obj_ptr);
        
        if (new_ptr) {
            // Update root to point to new location
            *root_addr = new_ptr;
            worklist.push_back(new_ptr);
        }
    }
    
    // Also scan dirty card table entries for old→young references
    auto dirty_cards = card_table->get_dirty_cards();
    for (void* card_addr : dirty_cards) {
        // Scan the 512-byte card region for pointers into nursery
        for (size_t off = 0; off < CardTable::CARD_SIZE; off += sizeof(void*)) {
            void** slot = (void**)((char*)card_addr + off);
            void* ref = *slot;
            if (ref && nursery->contains(ref)) {
                ObjHeader* ref_header = get_header(ref);
                if (ref_header && !ref_header->forwarded_bit) {
                    if (ref_header->pinned_bit) {
                        ref_header->color = GC_COLOR_BLACK;
                        worklist.push_back(ref);
                    } else {
                        void* new_ptr = evacuate_object(ref);
                        if (new_ptr) {
                            *slot = new_ptr;
                            worklist.push_back(new_ptr);
                        }
                    }
                }
            }
        }
    }
    
    // Process worklist: trace references from evacuated objects
    // to find and evacuate any nursery objects they point to
    for (size_t i = 0; i < worklist.size(); ++i) {
        void* obj = worklist[i];
        ObjHeader* header = get_header(obj);
        if (!header) continue;
        
        auto layout_it = type_ref_offsets.find(header->type_id);
        if (layout_it == type_ref_offsets.end()) continue;
        
        for (size_t offset : layout_it->second) {
            void** field_addr = (void**)((char*)obj + offset);
            void* ref = *field_addr;
            
            if (!ref || !nursery->contains(ref)) continue;
            
            ObjHeader* ref_header = get_header(ref);
            if (!ref_header) continue;
            
            if (ref_header->forwarded_bit) {
                // Already evacuated — update field to new location
                *field_addr = *(void**)ref;
            } else if (ref_header->pinned_bit) {
                ref_header->color = GC_COLOR_BLACK;
                worklist.push_back(ref);
            } else {
                void* new_ptr = evacuate_object(ref);
                if (new_ptr) {
                    *field_addr = new_ptr;
                    worklist.push_back(new_ptr);
                }
            }
        }
    }
    
    // Reconstruct nursery (handle fragments from pinned objects)
    nursery->reset_with_pinned();
    
    // Clear card table
    card_table->clear();
    
    // Update stats
    stats.nursery_used = nursery->used;
    stats.old_gen_used = old_gen->used;
    
    record_pause(stw_start);
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
     * Two modes:
     * A) STW (default): Stop-the-world mark + sweep
     * B) Concurrent (v0.8.1): Concurrent mark + incremental sweep
     *    - Uses SATB write barrier to maintain tri-color invariant
     *    - Mark runs in background thread while mutator continues
     *    - Sweep is amortized across subsequent allocations
     */
    
    if (!initialized) return;
    
    stats.num_major_collections++;
    
    if (concurrent_enabled) {
        // =================================================================
        // Concurrent Mark + Incremental Sweep
        // =================================================================
        
        // If already marking or sweeping, finish that work first
        if (mark_thread) {
            concurrent_marking = false;
            if (mark_thread->joinable()) {
                mark_thread->join();
            }
            delete mark_thread;
            mark_thread = nullptr;
        }
        
        // Finish any in-progress incremental sweep
        if (sweep_in_progress) {
            while (sweep_in_progress) {
                sweep_incremental(256);
            }
        }
        
        // Reset all colors to WHITE
        for (void* obj : old_gen->objects) {
            ObjHeader* header = get_header(obj);
            if (header) header->color = GC_COLOR_WHITE;
        }
        
        // Short STW pause: snapshot roots → gray worklist
        {
            auto stw_start = std::chrono::high_resolution_clock::now();
            
            auto roots = shadow_stack.get_all_roots();
            
            std::lock_guard<std::mutex> glock(gray_mutex);
            gray_worklist.clear();
            satb_buffer.clear();
            
            for (void** root_addr : roots) {
                void* obj_ptr = *root_addr;
                if (obj_ptr && !nursery->contains(obj_ptr)) {
                    ObjHeader* header = get_header(obj_ptr);
                    if (header && header->color == GC_COLOR_WHITE) {
                        header->color = GC_COLOR_GRAY;
                        gray_worklist.push_back(obj_ptr);
                    }
                }
            }
            
            // v0.8.4: Snapshot coroutine frame roots into gray worklist
            GCCoroAllocator* coro_alloc_conc = get_global_coro_allocator();
            if (coro_alloc_conc) {
                coro_alloc_conc->scan_frames([this](void* obj_ptr) {
                    if (!obj_ptr || nursery->contains(obj_ptr)) return;
                    ObjHeader* header = get_header(obj_ptr);
                    if (header && header->color == GC_COLOR_WHITE) {
                        header->color = GC_COLOR_GRAY;
                        gray_worklist.push_back(obj_ptr);
                    }
                });
            }
            
            // v0.8.4: Snapshot JIT roots into gray worklist
            {
                std::lock_guard<std::mutex> jlock(jit_roots_mutex);
                for (void** jit_root : jit_roots) {
                    if (!jit_root) continue;
                    void* obj_ptr = *jit_root;
                    if (!obj_ptr || nursery->contains(obj_ptr)) continue;
                    ObjHeader* header = get_header(obj_ptr);
                    if (header && header->color == GC_COLOR_WHITE) {
                        header->color = GC_COLOR_GRAY;
                        gray_worklist.push_back(obj_ptr);
                    }
                }
            }
            
            record_pause(stw_start);
        }
        
        // Launch concurrent mark
        concurrent_marking = true;
        stats.num_concurrent_marks++;
        
        mark_thread = new std::thread([this]() {
            concurrent_mark_phase();
        });
        
        // Wait for mark to complete (in a real multi-threaded runtime,
        // the mutator would continue; for now we join inline)
        mark_thread->join();
        delete mark_thread;
        mark_thread = nullptr;
        
        // Start incremental sweep
        sweep_in_progress = true;
        sweep_cursor = 0;
        sweep_bytes_freed = 0;
        stats.num_incremental_sweeps++;
        
    } else {
        // =================================================================
        // Stop-the-World Mark-Sweep (v0.8.0 behavior with tri-color)
        // =================================================================
        
        auto stw_start = std::chrono::high_resolution_clock::now();
        
        // Reset all colors to WHITE
        for (void* obj : old_gen->objects) {
            ObjHeader* header = get_header(obj);
            if (header) header->color = GC_COLOR_WHITE;
        }
        
        // Mark Phase: tri-color using gray worklist
        auto roots = shadow_stack.get_all_roots();
        
        gray_worklist.clear();
        for (void** root_addr : roots) {
            void* obj_ptr = *root_addr;
            if (obj_ptr) {
                mark_object(obj_ptr);
            }
        }
        
        // v0.8.4: Mark coroutine frame roots
        GCCoroAllocator* coro_alloc_stw = get_global_coro_allocator();
        if (coro_alloc_stw) {
            coro_alloc_stw->scan_frames([this](void* obj_ptr) {
                mark_object(obj_ptr);
            });
        }
        
        // v0.8.4: Mark JIT roots
        {
            std::lock_guard<std::mutex> jlock(jit_roots_mutex);
            for (void** jit_root : jit_roots) {
                if (jit_root && *jit_root) {
                    mark_object(*jit_root);
                }
            }
        }
        
        // Process gray worklist until empty
        process_gray_worklist();
        
        // Sweep Phase
        sweep_old_gen();
        
        // Update stats
        stats.old_gen_used = old_gen->used;
        
        record_pause(stw_start);
    }
}

// =============================================================================
// Tri-Color Marking
// =============================================================================

void GCState::mark_object(void* ptr) {
    /**
     * Mark phase: Set object to GRAY and push to worklist
     * 
     * Tri-color invariant:
     * - WHITE: unmarked (candidate for collection)
     * - GRAY: marked but children not yet traced
     * - BLACK: marked and all children traced
     * 
     * A BLACK object never points to a WHITE object (strong invariant).
     */
    
    if (!ptr) return;
    
    ObjHeader* header = get_header(ptr);
    if (!header) return;
    
    // Already gray or black? Skip
    if (header->color != GC_COLOR_WHITE) return;
    
    // Mark gray and push to worklist
    header->color = GC_COLOR_GRAY;
    gray_worklist.push_back(ptr);
}

void GCState::mark_object_concurrent(void* ptr) {
    /**
     * Thread-safe version of mark_object for use during concurrent marking.
     * Pushes to gray worklist under lock.
     */
    
    if (!ptr) return;
    
    ObjHeader* header = get_header(ptr);
    if (!header) return;
    
    if (header->color != GC_COLOR_WHITE) return;
    
    header->color = GC_COLOR_GRAY;
    
    std::lock_guard<std::mutex> lock(gray_mutex);
    gray_worklist.push_back(ptr);
}

void GCState::process_gray_worklist() {
    /**
     * Drain the gray worklist: for each gray object,
     * trace its reference fields, then mark it BLACK.
     * 
     * Any WHITE children found are marked GRAY and added to the worklist.
     * Loop continues until worklist is empty.
     */
    
    while (!gray_worklist.empty()) {
        void* obj = gray_worklist.back();
        gray_worklist.pop_back();
        
        ObjHeader* header = get_header(obj);
        if (!header || header->color == GC_COLOR_BLACK) continue;
        
        // Trace reference fields
        auto it = type_ref_offsets.find(header->type_id);
        if (it != type_ref_offsets.end()) {
            for (size_t offset : it->second) {
                void** field_addr = (void**)((char*)obj + offset);
                void* ref = *field_addr;
                
                if (ref && is_heap_pointer(ref)) {
                    ObjHeader* ref_header = get_header(ref);
                    if (ref_header && ref_header->color == GC_COLOR_WHITE) {
                        ref_header->color = GC_COLOR_GRAY;
                        gray_worklist.push_back(ref);
                    }
                }
            }
        }
        
        // All children traced — mark BLACK
        header->color = GC_COLOR_BLACK;
    }
}

void GCState::drain_satb_buffer() {
    /**
     * Process SATB snapshot entries. 
     * Each entry is a pointer that was about to be overwritten.
     * Mark these as gray to ensure they survive the current cycle.
     */
    
    std::lock_guard<std::mutex> lock(gray_mutex);
    
    for (void* ptr : satb_buffer) {
        if (!ptr) continue;
        ObjHeader* header = get_header(ptr);
        if (header && header->color == GC_COLOR_WHITE) {
            header->color = GC_COLOR_GRAY;
            gray_worklist.push_back(ptr);
        }
    }
    satb_buffer.clear();
}

void GCState::concurrent_mark_phase() {
    /**
     * Background mark thread.
     * Drains gray worklist repeatedly, also draining SATB buffer,
     * until both are empty.
     */
    
    while (concurrent_marking.load(std::memory_order_relaxed)) {
        // Process gray worklist (thread-safe: we own gray_mutex for pops)
        {
            std::lock_guard<std::mutex> lock(gray_mutex);
            
            // Process a batch from the worklist
            size_t batch = std::min(gray_worklist.size(), (size_t)64);
            for (size_t i = 0; i < batch; ++i) {
                void* obj = gray_worklist.back();
                gray_worklist.pop_back();
                
                ObjHeader* header = get_header(obj);
                if (!header || header->color == GC_COLOR_BLACK) continue;
                
                // Trace reference fields
                auto it = type_ref_offsets.find(header->type_id);
                if (it != type_ref_offsets.end()) {
                    for (size_t offset : it->second) {
                        void** field_addr = (void**)((char*)obj + offset);
                        void* ref = *field_addr;
                        
                        if (ref && is_heap_pointer(ref)) {
                            ObjHeader* ref_header = get_header(ref);
                            if (ref_header && ref_header->color == GC_COLOR_WHITE) {
                                ref_header->color = GC_COLOR_GRAY;
                                gray_worklist.push_back(ref);
                            }
                        }
                    }
                }
                
                header->color = GC_COLOR_BLACK;
            }
        }
        
        // Drain SATB buffer
        drain_satb_buffer();
        
        // Check termination: both worklist and SATB buffer empty
        {
            std::lock_guard<std::mutex> lock(gray_mutex);
            if (gray_worklist.empty() && satb_buffer.empty()) {
                concurrent_marking = false;
                break;
            }
        }
    }
}

void GCState::sweep_old_gen() {
    /**
     * Full sweep phase: Free all WHITE objects (not reached by mark)
     * 
     * Tri-color scheme:
     * - BLACK objects are live → reset to WHITE for next cycle
     * - WHITE objects are dead → finalize and free
     * - GRAY should not exist after marking (indicates bug)
     */
    
    auto& objects = old_gen->objects;
    size_t bytes_freed = 0;
    
    for (size_t i = 0; i < objects.size(); ) {
        void* obj_ptr = objects[i];
        ObjHeader* header = get_header(obj_ptr);
        
        if (!header) {
            ++i;
            continue;
        }
        
        if (header->color != GC_COLOR_WHITE) {
            // Live object - reset to WHITE for next cycle
            header->color = GC_COLOR_WHITE;
            ++i;
        } else {
            // Dead object - run finalizer if registered, then free
            size_t obj_size = header->size_class * 8;
            bytes_freed += obj_size;
            
            auto it = finalizers.find(header->type_id);
            if (it != finalizers.end() && it->second) {
                it->second(obj_ptr);
            }
            
            void* alloc_ptr = (char*)obj_ptr - sizeof(ObjHeader);
            std::free(alloc_ptr);
            
            objects[i] = objects.back();
            objects.pop_back();
        }
    }
    
    old_gen->used -= bytes_freed;
    stats.total_collected += bytes_freed;
}

void GCState::sweep_incremental(size_t steps) {
    /**
     * Incremental sweep: Process up to 'steps' objects per call.
     * Called from alloc() to amortize sweep cost.
     * 
     * sweeps WHITE objects (dead) and resets BLACK to WHITE (live).
     */
    
    if (!sweep_in_progress) return;
    
    auto& objects = old_gen->objects;
    size_t processed = 0;
    
    while (sweep_cursor < objects.size() && processed < steps) {
        void* obj_ptr = objects[sweep_cursor];
        ObjHeader* header = get_header(obj_ptr);
        
        if (!header) {
            ++sweep_cursor;
            ++processed;
            continue;
        }
        
        if (header->color != GC_COLOR_WHITE) {
            // Live object - reset to WHITE for next cycle
            header->color = GC_COLOR_WHITE;
            ++sweep_cursor;
        } else {
            // Dead object - finalize and free
            size_t obj_size = header->size_class * 8;
            sweep_bytes_freed += obj_size;
            
            auto it = finalizers.find(header->type_id);
            if (it != finalizers.end() && it->second) {
                it->second(obj_ptr);
            }
            
            void* alloc_ptr = (char*)obj_ptr - sizeof(ObjHeader);
            std::free(alloc_ptr);
            
            // Swap-remove: move last to current position
            objects[sweep_cursor] = objects.back();
            objects.pop_back();
            // Don't advance cursor — new element at position
        }
        
        ++processed;
    }
    
    // Check if sweep is complete
    if (sweep_cursor >= objects.size()) {
        old_gen->used -= sweep_bytes_freed;
        stats.total_collected += sweep_bytes_freed;
        stats.old_gen_used = old_gen->used;
        sweep_in_progress = false;
        sweep_cursor = 0;
        sweep_bytes_freed = 0;
    }
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
     * Write Barrier: Track old-to-young references + SATB snapshot
     * 
     * Called after: obj.field = ref
     * 
     * Two purposes:
     * 1. Card table: If obj is old and ref is nursery, mark card DIRTY
     * 2. SATB (v0.8.1): During concurrent mark, snapshot the OLD value
     *    before it's overwritten. This maintains the tri-color invariant:
     *    a black object cannot point to a white object.
     */
    
    if (!obj || !ref) return;
    
    ObjHeader* obj_header = get_header(obj);
    ObjHeader* ref_header = get_header(ref);
    
    if (!obj_header || !ref_header) return;
    
    // Card table: old-to-young reference tracking
    if (!obj_header->is_nursery && ref_header->is_nursery) {
        card_table->mark_dirty(obj);
    }
    
    // SATB barrier: During concurrent mark, snapshot the old value
    // The caller has already stored the new ref; we snapshot what WAS there
    // This is called with ref = NEW value. For true SATB, the compiler should
    // pass the OLD value. We use a simplified approach: mark the new ref as
    // gray if it's white, preventing it from being collected.
    if (concurrent_marking.load(std::memory_order_relaxed)) {
        if (ref_header->color == GC_COLOR_WHITE) {
            std::lock_guard<std::mutex> lock(gray_mutex);
            satb_buffer.push_back(ref);
        }
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

void GCState::set_max_heap(size_t max_bytes) {
    std::lock_guard<std::mutex> lock(gc_mutex);
    max_heap = max_bytes;
}

void GCState::register_finalizer(uint16_t type_id, AriaFinalizer finalizer) {
    std::lock_guard<std::mutex> lock(gc_mutex);
    finalizers[type_id] = finalizer;
}

void GCState::register_type_layout(uint16_t type_id, const size_t* ref_offsets, size_t num_refs) {
    std::lock_guard<std::mutex> lock(gc_mutex);
    type_ref_offsets[type_id] = std::vector<size_t>(ref_offsets, ref_offsets + num_refs);
}

// =============================================================================
// v0.8.1: Safepoints & Pause Timing
// =============================================================================

void GCState::enable_concurrent(bool enable) {
    std::lock_guard<std::mutex> lock(gc_mutex);
    concurrent_enabled = enable;
}

void GCState::safepoint() {
    /**
     * GC safepoint poll.
     * 
     * Fast path: single atomic load — no overhead when GC is idle.
     * Slow path: block on condition variable until GC releases us.
     */
    
    if (!safepoint_requested.load(std::memory_order_relaxed)) {
        return;  // Fast path: no GC pending
    }
    
    // Slow path: GC is requesting a pause
    std::unique_lock<std::mutex> lock(safepoint_mutex);
    mutators_stopped.fetch_add(1, std::memory_order_release);
    safepoint_ack.notify_one();
    
    // Wait until GC clears the safepoint request
    safepoint_cv.wait(lock, [this]() {
        return !safepoint_requested.load(std::memory_order_acquire);
    });
    
    mutators_stopped.fetch_sub(1, std::memory_order_release);
}

void GCState::start_stw_pause() {
    /**
     * Request all mutators to stop at next safepoint.
     * Called by GC thread before a STW phase.
     */
    safepoint_requested.store(true, std::memory_order_release);
    pause_start = std::chrono::high_resolution_clock::now();
}

void GCState::end_stw_pause() {
    /**
     * Release all mutators from safepoint.
     */
    safepoint_requested.store(false, std::memory_order_release);
    safepoint_cv.notify_all();
    
    record_pause(pause_start);
}

void GCState::record_pause(std::chrono::high_resolution_clock::time_point start) {
    auto end = std::chrono::high_resolution_clock::now();
    uint64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    stats.last_pause_time_ns = ns;
    stats.total_pause_time_ns += ns;
    if (ns > stats.max_pause_time_ns) {
        stats.max_pause_time_ns = ns;
    }
}

// =============================================================================
// v0.8.4: JIT Root Tracking
// =============================================================================

void GCState::register_jit_root(void** root_addr) {
    if (!root_addr) return;
    std::lock_guard<std::mutex> lock(jit_roots_mutex);
    jit_roots.push_back(root_addr);
}

void GCState::unregister_jit_root(void** root_addr) {
    if (!root_addr) return;
    std::lock_guard<std::mutex> lock(jit_roots_mutex);
    jit_roots.erase(
        std::remove(jit_roots.begin(), jit_roots.end(), root_addr),
        jit_roots.end()
    );
}

// =========================================================================
// Fork Safety
// =========================================================================

void GCState::lock_for_fork() {
    gc_mutex.lock();
    gray_mutex.lock();
    safepoint_mutex.lock();
    jit_roots_mutex.lock();
}

void GCState::unlock_for_fork() {
    jit_roots_mutex.unlock();
    safepoint_mutex.unlock();
    gray_mutex.unlock();
    gc_mutex.unlock();
}

} // namespace runtime
} // namespace npk

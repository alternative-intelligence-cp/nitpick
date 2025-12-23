// GC integration for async coroutines
// Automatic memory management for coroutine frames

#ifndef ARIA_RUNTIME_ASYNC_GC_INTEGRATION_H
#define ARIA_RUNTIME_ASYNC_GC_INTEGRATION_H

#include "runtime/async/executor.h"
#include "runtime/async/future.h"
#include <cstdint>
#include <vector>
#include <unordered_set>
#include <map>
#include <functional>

namespace aria {
namespace runtime {

/**
 * Coroutine frame metadata for GC
 */
struct CoroFrameMetadata {
    void* frame_ptr;              // Coroutine frame pointer
    size_t frame_size;            // Frame size in bytes
    bool is_suspended;            // Currently suspended?
    uint64_t task_id;             // Associated task ID
    uint64_t last_resume_time;    // Last resume timestamp
    std::vector<void**> gc_roots; // Pointers to GC-managed objects in frame
    
    CoroFrameMetadata()
        : frame_ptr(nullptr), frame_size(0), is_suspended(false),
          task_id(0), last_resume_time(0) {}
};

/**
 * GC-aware coroutine allocator
 * 
 * Integrates coroutine frames with garbage collector:
 * - Tracks coroutine frames as GC roots
 * - Scans suspended coroutines for live references
 * - Frees frames when coroutines complete
 */
class GCCoroAllocator {
private:
    // Active coroutine frames
    std::unordered_set<void*> active_frames;
    
    // Frame metadata
    std::map<void*, CoroFrameMetadata*> frame_metadata;
    
    // Statistics
    uint64_t frames_allocated;
    uint64_t frames_freed;
    uint64_t total_frame_bytes;
    
    // GC integration
    void* gc_handle;  // Handle to GC system
    
public:
    GCCoroAllocator();
    ~GCCoroAllocator();
    
    /**
     * Allocate coroutine frame
     * @param size Frame size
     * @param task_id Associated task ID
     * @return Frame pointer
     */
    void* allocate_frame(size_t size, uint64_t task_id);
    
    /**
     * Free coroutine frame
     * @param frame Frame pointer
     */
    void free_frame(void* frame);
    
    /**
     * Mark frame as suspended
     * Tells GC to scan this frame for live references
     * @param frame Frame pointer
     */
    void suspend_frame(void* frame);
    
    /**
     * Mark frame as resumed
     * @param frame Frame pointer
     */
    void resume_frame(void* frame);
    
    /**
     * Register GC root in coroutine frame
     * @param frame Frame pointer
     * @param root Pointer to GC object
     */
    void add_gc_root(void* frame, void** root);
    
    /**
     * Scan all suspended frames for GC
     * Called by GC during mark phase
     * @param mark_fn Function to mark live objects
     */
    void scan_frames(std::function<void(void*)> mark_fn);
    
    /**
     * Get statistics
     */
    uint64_t get_frames_allocated() const { return frames_allocated; }
    uint64_t get_frames_freed() const { return frames_freed; }
    uint64_t get_active_frames() const { return active_frames.size(); }
    uint64_t get_total_bytes() const { return total_frame_bytes; }
    
private:
    /**
     * Initialize GC integration
     */
    void init_gc();
    
    /**
     * Shutdown GC integration
     */
    void shutdown_gc();
};

/**
 * RAII wrapper for coroutine frame
 * Automatically frees frame on destruction
 */
class CoroFrameGuard {
private:
    GCCoroAllocator* allocator;
    void* frame;
    
public:
    CoroFrameGuard(GCCoroAllocator* alloc, void* f)
        : allocator(alloc), frame(f) {}
    
    ~CoroFrameGuard() {
        if (frame && allocator) {
            allocator->free_frame(frame);
        }
    }
    
    void* get() const { return frame; }
    
    // Disable copy
    CoroFrameGuard(const CoroFrameGuard&) = delete;
    CoroFrameGuard& operator=(const CoroFrameGuard&) = delete;
    
    // Allow move
    CoroFrameGuard(CoroFrameGuard&& other) noexcept
        : allocator(other.allocator), frame(other.frame) {
        other.frame = nullptr;
    }
};

/**
 * Global coroutine allocator instance
 */
extern GCCoroAllocator* global_coro_allocator;

/**
 * Get global allocator
 */
GCCoroAllocator* get_global_coro_allocator();

/**
 * C API for LLVM-generated code
 */
extern "C" {
    /**
     * Allocate coroutine frame with GC integration
     */
    void* __aria_coro_alloc_gc(size_t size, uint64_t task_id);
    
    /**
     * Free coroutine frame
     */
    void __aria_coro_free_gc(void* frame);
    
    /**
     * Suspend notification
     */
    void __aria_coro_suspend_gc(void* frame);
    
    /**
     * Resume notification
     */
    void __aria_coro_resume_gc(void* frame);
    
    /**
     * Register GC root
     */
    void __aria_coro_add_root_gc(void* frame, void** root);
}

/**
 * Future extensions
 */

// Incremental coroutine GC - scan a few frames per GC cycle
class IncrementalCoroGC {
    // TODO: Incremental scanning
    // TODO: Write barriers for coroutine modifications
};

// Generational coroutine GC - most coroutines are short-lived
class GenerationalCoroGC {
    // TODO: Young/old generation for coroutines
    // TODO: Fast collection of short-lived coroutines
};

} // namespace runtime
} // namespace aria

#endif // ARIA_RUNTIME_ASYNC_GC_INTEGRATION_H

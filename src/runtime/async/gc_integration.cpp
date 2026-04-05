// GC integration implementation

#include "runtime/async/gc_integration.h"
#include "runtime/gc.h"
#include <cstdlib>
#include <cstring>
#include <chrono>

namespace aria {
namespace runtime {

// Global instance
GCCoroAllocator* global_coro_allocator = nullptr;

GCCoroAllocator* get_global_coro_allocator() {
    if (!global_coro_allocator) {
        global_coro_allocator = new GCCoroAllocator();
    }
    return global_coro_allocator;
}

// ============================================================================
// GCCoroAllocator
// ============================================================================

GCCoroAllocator::GCCoroAllocator()
    : frames_allocated(0), frames_freed(0), total_frame_bytes(0),
      gc_handle(nullptr) {
    init_gc();
}

GCCoroAllocator::~GCCoroAllocator() {
    shutdown_gc();
    
    // Free all remaining frames
    for (auto& pair : frame_metadata) {
        delete pair.second;
    }
    frame_metadata.clear();
    active_frames.clear();
}

void* GCCoroAllocator::allocate_frame(size_t size, uint64_t task_id) {
    // Allocate frame
    void* frame = std::malloc(size);
    if (!frame) {
        return nullptr;
    }
    
    std::memset(frame, 0, size);
    
    // Create metadata
    CoroFrameMetadata* metadata = new CoroFrameMetadata();
    metadata->frame_ptr = frame;
    metadata->frame_size = size;
    metadata->is_suspended = false;
    metadata->task_id = task_id;
    metadata->last_resume_time = 0;
    
    // Track frame
    active_frames.insert(frame);
    frame_metadata[frame] = metadata;
    
    // Update statistics
    frames_allocated++;
    total_frame_bytes += size;
    
    return frame;
}

void GCCoroAllocator::free_frame(void* frame) {
    if (!frame) return;
    
    auto it = frame_metadata.find(frame);
    if (it == frame_metadata.end()) {
        return;  // Unknown frame
    }
    
    CoroFrameMetadata* metadata = it->second;
    
    // Update statistics
    total_frame_bytes -= metadata->frame_size;
    frames_freed++;
    
    // Remove from tracking
    active_frames.erase(frame);
    frame_metadata.erase(it);
    
    // Free metadata
    delete metadata;
    
    // Free frame
    std::free(frame);
}

void GCCoroAllocator::suspend_frame(void* frame) {
    auto it = frame_metadata.find(frame);
    if (it == frame_metadata.end()) {
        return;
    }
    
    CoroFrameMetadata* metadata = it->second;
    metadata->is_suspended = true;
    
    // v0.8.4: Frame roots will be scanned during next GC cycle
    // (GC calls scan_frames() during minor_gc/major_gc mark phase)
}

void GCCoroAllocator::resume_frame(void* frame) {
    auto it = frame_metadata.find(frame);
    if (it == frame_metadata.end()) {
        return;
    }
    
    CoroFrameMetadata* metadata = it->second;
    metadata->is_suspended = false;
    
    // Update resume timestamp
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch());
    metadata->last_resume_time = ms.count();
}

void GCCoroAllocator::add_gc_root(void* frame, void** root) {
    auto it = frame_metadata.find(frame);
    if (it == frame_metadata.end()) {
        return;
    }
    
    CoroFrameMetadata* metadata = it->second;
    metadata->gc_roots.push_back(root);
}

void GCCoroAllocator::scan_frames(std::function<void(void*)> mark_fn) {
    // Scan all suspended frames
    for (auto& pair : frame_metadata) {
        CoroFrameMetadata* metadata = pair.second;
        
        if (metadata->is_suspended) {
            // Mark all GC roots in this frame
            for (void** root : metadata->gc_roots) {
                if (root && *root) {
                    mark_fn(*root);
                }
            }
        }
    }
}

void GCCoroAllocator::init_gc() {
    // Initialize GC subsystem if not already running
    // (aria_gc_init is idempotent)
    aria_gc_init(4 * 1024 * 1024, 64 * 1024 * 1024);
    
    // Store a self-pointer as the gc_handle so we can identify
    // this allocator instance from C callbacks if needed
    gc_handle = static_cast<void*>(this);
}

void GCCoroAllocator::shutdown_gc() {
    // Unregister any JIT roots held by active coroutine frames
    for (auto& pair : frame_metadata) {
        CoroFrameMetadata* metadata = pair.second;
        for (void** root : metadata->gc_roots) {
            aria_gc_unregister_jit_root(root);
        }
    }
    
    gc_handle = nullptr;
}

} // namespace runtime
} // namespace aria

// ============================================================================
// C API
// ============================================================================

extern "C" {

void* __aria_coro_alloc_gc(size_t size, uint64_t task_id) {
    auto* allocator = aria::runtime::get_global_coro_allocator();
    return allocator->allocate_frame(size, task_id);
}

void __aria_coro_free_gc(void* frame) {
    auto* allocator = aria::runtime::get_global_coro_allocator();
    allocator->free_frame(frame);
}

void __aria_coro_suspend_gc(void* frame) {
    auto* allocator = aria::runtime::get_global_coro_allocator();
    allocator->suspend_frame(frame);
}

void __aria_coro_resume_gc(void* frame) {
    auto* allocator = aria::runtime::get_global_coro_allocator();
    allocator->resume_frame(frame);
}

void __aria_coro_add_root_gc(void* frame, void** root) {
    auto* allocator = aria::runtime::get_global_coro_allocator();
    allocator->add_gc_root(frame, root);
}

} // extern "C"

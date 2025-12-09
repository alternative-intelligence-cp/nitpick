// Shadow Stack Implementation for Accurate GC Root Tracking
// 
// The shadow stack is a parallel stack maintained alongside the native call stack.
// It tracks all GC-managed pointers in the current execution context, enabling
// precise root identification during garbage collection.
//
// Architecture:
// - Thread-local shadow stack (one per thread)
// - Stack frame structure: each function pushes a frame on entry, pops on exit
// - Slot tracking: pointers registered/unregistered as variables enter/exit scope
// - Zero overhead when not using GC (calls compile away if no GC allocations)

#include "shadow_stack.h"
#include <cstdlib>
#include <cstring>
#include <vector>

// ==============================================================================
// Shadow Stack Frame Structure
// ==============================================================================

// Maximum slots per frame (can be dynamically grown if needed)
const size_t MAX_FRAME_SLOTS = 32;

struct ShadowStackFrame {
    ShadowStackFrame* prev;           // Previous frame (forming a linked list)
    void** roots;                      // Array of GC root pointers in this frame
    size_t num_roots;                  // Number of active roots in this frame
    size_t capacity;                   // Allocated capacity for roots array
};

// ==============================================================================
// Thread-Local Shadow Stack
// ==============================================================================

// Thread-local pointer to the top of the shadow stack
static thread_local ShadowStackFrame* shadow_stack_top = nullptr;

// ==============================================================================
// Frame Management (Called by generated code)
// ==============================================================================

// Push a new shadow stack frame (called at function entry)
extern "C" void aria_shadow_stack_push_frame() {
    ShadowStackFrame* frame = (ShadowStackFrame*)malloc(sizeof(ShadowStackFrame));
    frame->prev = shadow_stack_top;
    frame->roots = (void**)malloc(sizeof(void*) * MAX_FRAME_SLOTS);
    frame->num_roots = 0;
    frame->capacity = MAX_FRAME_SLOTS;
    shadow_stack_top = frame;
}

// Pop a shadow stack frame (called at function exit)
extern "C" void aria_shadow_stack_pop_frame() {
    if (!shadow_stack_top) return;
    
    ShadowStackFrame* frame = shadow_stack_top;
    shadow_stack_top = frame->prev;
    
    free(frame->roots);
    free(frame);
}

// ==============================================================================
// Root Registration (Called when GC pointers are stored in locals)
// ==============================================================================

// Register a GC root pointer in the current frame
// ptr_addr: address of the local variable holding the GC pointer
extern "C" void aria_shadow_stack_add_root(void** ptr_addr) {
    if (!shadow_stack_top) return;
    
    ShadowStackFrame* frame = shadow_stack_top;
    
    // Grow array if needed
    if (frame->num_roots >= frame->capacity) {
        size_t new_capacity = frame->capacity * 2;
        void** new_roots = (void**)realloc(frame->roots, sizeof(void*) * new_capacity);
        if (!new_roots) {
            // Allocation failed - this is a critical error
            // In production, should abort or throw
            return;
        }
        frame->roots = new_roots;
        frame->capacity = new_capacity;
    }
    
    // Add the pointer address to the roots array
    frame->roots[frame->num_roots++] = (void*)ptr_addr;
}

// Remove a GC root pointer from the current frame (when variable goes out of scope)
// ptr_addr: address of the local variable that's being destroyed
extern "C" void aria_shadow_stack_remove_root(void** ptr_addr) {
    if (!shadow_stack_top) return;
    
    ShadowStackFrame* frame = shadow_stack_top;
    
    // Find and remove the root (swap with last element)
    for (size_t i = 0; i < frame->num_roots; i++) {
        if (frame->roots[i] == (void*)ptr_addr) {
            // Swap with last element and decrement count
            frame->roots[i] = frame->roots[frame->num_roots - 1];
            frame->num_roots--;
            return;
        }
    }
}

// ==============================================================================
// Root Scanning (Called by GC during collection)
// ==============================================================================

// Collect all GC roots from the shadow stack
// Returns a vector of all live GC-managed pointers
std::vector<void*> aria_shadow_stack_get_roots() {
    std::vector<void*> roots;
    
    // Walk the shadow stack from top to bottom
    ShadowStackFrame* frame = shadow_stack_top;
    while (frame) {
        // For each root in this frame
        for (size_t i = 0; i < frame->num_roots; i++) {
            // Dereference the pointer address to get the actual GC pointer
            void** ptr_addr = (void**)frame->roots[i];
            void* gc_ptr = *ptr_addr;
            
            if (gc_ptr) {
                roots.push_back(gc_ptr);
            }
        }
        frame = frame->prev;
    }
    
    return roots;
}

// ==============================================================================
// Thread Lifecycle Management
// ==============================================================================

// Initialize shadow stack for current thread (optional - lazy init also works)
extern "C" void aria_shadow_stack_init() {
    // Shadow stack starts empty - frames pushed as functions are called
    shadow_stack_top = nullptr;
}

// Cleanup shadow stack on thread exit
extern "C" void aria_shadow_stack_cleanup() {
    // Pop all remaining frames (shouldn't happen in normal execution)
    while (shadow_stack_top) {
        aria_shadow_stack_pop_frame();
    }
}

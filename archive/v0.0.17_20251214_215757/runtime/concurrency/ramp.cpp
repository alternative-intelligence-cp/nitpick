//
// Implementation of the RAMP (Resource Allocation for Minimal Pause) Optimization
//
// Theory:
// An async function should not allocate a heap coroutine frame if it completes synchronously.
// We introduce a 'FastResult' which is a union of a direct value and a heap pointer.

#include "scheduler.h"
#include "../memory/allocator.h"
#include <cstring>

enum RampState {
    RAMP_COMPLETE,
    RAMP_PENDING
};

// The lightweight return object for async calls
struct RampResult {
    RampState state;
    union {
        void* value;         // Valid if RAMP_COMPLETE
        CoroutineFrame* coro;// Valid if RAMP_PENDING
    } payload;
};

// Represents the layout of an async frame on the STACK
struct StackFrame {
    //... captured variables...
    void* return_address;
    StackFrame* parent;
};

// Helper: Promotes a stack frame to the heap
// This is called by the compiler-generated code when 'await' returns Pending.
extern "C" CoroutineFrame* __aria_ramp_promote(void* stack_vars, size_t size, void* instruction_ptr) {
    // 1. Allocate Heap Frame (Wild or GC depending on strictness, usually GC for coroutines)
    // We use the specialized Aria Coroutine Allocator
    CoroutineFrame* heap_frame = (CoroutineFrame*)aria_alloc_aligned(sizeof(CoroutineFrame) + size, 64);
    
    // 2. Copy State
    // Move the local variables from the stack to the heap payload area
    memcpy(heap_frame->data, stack_vars, size);
    
    // 3. Set Resume Point (now stores LLVM coroutine handle)
    heap_frame->coro_handle = instruction_ptr;
    heap_frame->state = CORO_SUSPENDED;
    
    return heap_frame;
}

// The 'await' operator implementation
// This is the intrinsic called by "await <expr>"
//
// Args:
//   future: The RampResult from the child async function
//   caller_stack: Pointer to caller's stack vars (for promotion if needed)
//   caller_size: Size of caller's stack frame
extern "C" RampResult __aria_await(RampResult future, void* caller_stack, size_t caller_size, void* resume_pc) {
    // Fast Path: Child finished immediately
    if (future.state == RAMP_COMPLETE) {
        return future; // Caller continues synchronously
    }
    
    // Slow Path: Child is pending. 
    // We must suspend the Caller.
    
    // 1. Promote Caller to Heap (if not already)
    CoroutineFrame* caller_frame = __aria_ramp_promote(caller_stack, caller_size, resume_pc);
    
    // 2. Link Dependency
    // The caller frame is now waiting on the child (future.payload.coro)
    caller_frame->waiting_on = future.payload.coro;
    
    // 3. Return Pending to the *Caller's Caller*
    // This creates the chain reaction up the stack until the root (scheduler) is reached.
    RampResult res;
    res.state = RAMP_PENDING;
    res.payload.coro = caller_frame;
    return res;
}

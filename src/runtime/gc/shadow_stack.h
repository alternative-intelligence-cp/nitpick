#ifndef ARIA_SHADOW_STACK_H
#define ARIA_SHADOW_STACK_H

#include <vector>

// Shadow Stack API for GC Root Tracking
// 
// The shadow stack maintains a parallel stack of GC-managed pointers,
// enabling precise root identification during garbage collection.
//
// Usage Pattern (in generated LLVM IR):
//   func:example = void() {
//       aria_shadow_stack_push_frame();     // Function entry
//       
//       auto:obj = gc_alloc(...);
//       aria_shadow_stack_add_root(&obj);   // Register GC pointer
//       
//       // ... use obj ...
//       
//       aria_shadow_stack_remove_root(&obj); // Unregister (optional - frame pop does this)
//       aria_shadow_stack_pop_frame();       // Function exit
//   }

#ifdef __cplusplus
extern "C" {
#endif

// Frame Management
// ----------------
// Push a new shadow stack frame (call at function entry)
void aria_shadow_stack_push_frame();

// Pop the current shadow stack frame (call at function exit)
void aria_shadow_stack_pop_frame();

// Root Registration
// -----------------
// Add a GC root pointer to the current frame
// ptr_addr: address of the local variable holding the GC pointer
void aria_shadow_stack_add_root(void** ptr_addr);

// Remove a GC root pointer from the current frame
// ptr_addr: address of the local variable being destroyed
void aria_shadow_stack_remove_root(void** ptr_addr);

// Thread Lifecycle
// ----------------
// Initialize shadow stack for current thread (optional - lazy init)
void aria_shadow_stack_init();

// Cleanup shadow stack on thread exit
void aria_shadow_stack_cleanup();

#ifdef __cplusplus
}

// C++ API for GC integration
// ---------------------------
// Get all GC roots from the shadow stack (called during collection)
std::vector<void*> aria_shadow_stack_get_roots();

#endif

#endif // ARIA_SHADOW_STACK_H

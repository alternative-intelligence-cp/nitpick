// Implementation of the Fragmented Nursery Allocator
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include "gc_impl.h"
#include "header.h"

// Note: Nursery struct is defined in header.h
// Note: Fragment struct (free region list) is defined in header.h

// Global config
const size_t NURSERY_SIZE = 4 * 1024 * 1024; // 4MB

// Alignment helpers
// Most architectures require 8-byte alignment for optimal performance
// and correctness (especially for int64, double, pointers)
#define ALLOCATION_ALIGNMENT 8
#define ALIGN_UP(n, align) (((n) + (align) - 1) & ~((align) - 1))

// The core allocation routine (Hot Path)
extern "C" void* aria_gc_alloc(Nursery* nursery, size_t size) {
   // 1. Fast Path: Standard Bump Allocation
   // Align the size to ensure proper memory alignment for all types
   size_t aligned_size = ALIGN_UP(size, ALLOCATION_ALIGNMENT);

   // Check if we fit in the current fragment or main buffer
   char* new_bump = nursery->bump_ptr + aligned_size;
   // Check against the end of the current active region (fragment or main)
   if (new_bump <= nursery->end_addr) {
       void* ptr = nursery->bump_ptr;
       nursery->bump_ptr = new_bump;
       return ptr;
   }

   // 2. Slow Path: Fragment Search or Collection Trigger
   // If we are in fragmented mode (fragments list is not null), try next fragment
   if (nursery->fragments) {
       Fragment* prev = nullptr;
       Fragment* curr = nursery->fragments;

       while (curr) {
           if (curr->size >= aligned_size) {
               // Found a fit!
               void* ptr = curr->start;

               // Update fragment (use aligned_size to maintain alignment)
               curr->start += aligned_size;
               curr->size -= aligned_size;
               
               // If fragment is exhausted, remove it from list
               if (curr->size == 0) {
                   Fragment* exhausted = curr;
                   if (prev) prev->next = curr->next;
                   else nursery->fragments = curr->next;

                   // TODO (Memory Leak): Free or recycle 'exhausted' fragment node
                   // Current implementation leaks the Fragment struct.
                   // When fragment pool is implemented, return to pool here instead:
                   //   fragment_pool[fragment_pool_used++] = exhausted;
               }
               
               return ptr;
           }
           prev = curr;
           curr = curr->next;
       }
   }

   // 3. Collection Path: Nursery is truly full
   // Trigger Minor GC. This function will:
   // a) Evacuate unpinned objects to Old Gen
   // b) Identify pinned objects remaining in Nursery
   // c) Rebuild 'nursery->fragments' list with holes between pins
   // d) Reset bump_ptr to first fragment
   aria_gc_collect_minor(nursery);
   // Retry allocation after collection
   return aria_gc_alloc(nursery, size);
}

// ==============================================================================
// Thread-Local Nursery Management
// ==============================================================================

// Thread-local storage for each thread's nursery
static thread_local Nursery* current_thread_nursery = nullptr;

// Get the current thread's nursery (lazy initialization)
extern "C" Nursery* get_current_thread_nursery() {
   if (!current_thread_nursery) {
       // Lazy initialization on first use
       current_thread_nursery = (Nursery*)malloc(sizeof(Nursery));
       
       // Allocate the nursery memory region (char* to match header.h definition)
       current_thread_nursery->start_addr = (char*)malloc(NURSERY_SIZE);
       current_thread_nursery->end_addr = current_thread_nursery->start_addr + NURSERY_SIZE;
       current_thread_nursery->bump_ptr = current_thread_nursery->start_addr;
       current_thread_nursery->fragments = nullptr;
   }
   return current_thread_nursery;
}

// Initialize a nursery for the current thread (optional explicit init)
extern "C" void aria_init_thread_nursery() {
   // Force initialization by calling get function
   get_current_thread_nursery();
}

// Cleanup nursery on thread exit (should be called by thread cleanup handler)
extern "C" void aria_cleanup_thread_nursery() {
   if (current_thread_nursery) {
       free(current_thread_nursery->start_addr);
       free(current_thread_nursery);
       current_thread_nursery = nullptr;
   }
}

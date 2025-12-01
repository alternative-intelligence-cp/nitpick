// Implementation of the Fragmented Nursery Allocator
#include <cstdint>
#include <cstddef>
#include "gc_impl.h"

// Represents a free contiguous region in the Nursery
struct FreeFragment {
   uint8_t* start;
   uint8_t* end;
   FreeFragment* next;
};

// Thread-Local Nursery Context
struct Nursery {
   uint8_t* start_addr;
   uint8_t* end_addr;
   uint8_t* bump_ptr;       // Current allocation head
   FreeFragment* fragments; // Linked list of free regions (if fragmented)

   // TODO: Add fragment pool to avoid malloc/free overhead and prevent memory leaks
   // When removing exhausted fragments (line 56), the FreeFragment nodes should be
   // recycled to a pool rather than leaked. Proposed implementation:
   //   FreeFragment fragment_pool[64];
   //   int fragment_pool_used = 0;
   // This will be needed when aria_gc_collect_minor() is fully implemented.

   // Config
   size_t size;
};

// Global config
const size_t NURSERY_SIZE = 4 * 1024 * 1024; // 4MB

// The core allocation routine (Hot Path)
extern "C" void* aria_gc_alloc(Nursery* nursery, size_t size) {
   // 1. Fast Path: Standard Bump Allocation
   // Check if we fit in the current fragment or main buffer
   uint8_t* new_bump = nursery->bump_ptr + size;
   // Check against the end of the current active region (fragment or main)
   if (new_bump <= nursery->end_addr) {
       void* ptr = nursery->bump_ptr;
       nursery->bump_ptr = new_bump;
       return ptr;
   }

   // 2. Slow Path: Fragment Search or Collection Trigger
   // If we are in fragmented mode (fragments list is not null), try next fragment
   if (nursery->fragments) {
       FreeFragment* prev = nullptr;
       FreeFragment* curr = nursery->fragments;
       
       while (curr) {
           size_t frag_size = curr->end - curr->start;
           if (frag_size >= size) {
               // Found a fit!
               void* ptr = curr->start;
               
               // Update fragment
               curr->start += size;
               // If fragment is exhausted, remove it from list
               if (curr->start == curr->end) {
                   FreeFragment* exhausted = curr;
                   if (prev) prev->next = curr->next;
                   else nursery->fragments = curr->next;

                   // TODO (Memory Leak): Free or recycle 'exhausted' fragment node
                   // Current implementation leaks the FreeFragment struct.
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
   aria_gc_collect_minor();
   // Retry allocation after collection
   return aria_gc_alloc(nursery, size);
}


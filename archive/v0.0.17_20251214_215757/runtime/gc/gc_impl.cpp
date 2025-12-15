// Implementation of the Major and Minor Garbage Collection Logic
#include "gc_impl.h"
#include "header.h"
#include "shadow_stack.h"
#include <vector>
#include <stack>
#include <algorithm>
#include <utility>
#include <cstdlib>
#include <cstring>

// Global List of Old Generation Objects
// In a production system, this would be a paged block allocator.
// For reference, we use a simple vector of pointers.
std::vector<ObjHeader*> old_gen_objects;

// Root scanning using shadow stack
// Returns all GC-managed pointers tracked by the shadow stack
std::vector<void*> get_thread_roots() {
    // Use shadow stack for precise root tracking
    return aria_shadow_stack_get_roots();
}

// Forward declaration of C function
extern "C" Nursery* get_current_thread_nursery();

// Helper: Mark an object and its children (DFS)
void mark_object(ObjHeader* obj) {
   if (!obj || obj->mark_bit) return;
   // 1. Mark Self
   obj->mark_bit = 1;

   // 2. Scan Children
   // Aria uses RTTI (type_id) to know where pointers live in the payload.
   // This switch acts as the "Visitor" pattern.
   switch (obj->type_id) {
       case TYPE_ARRAY_OBJ: {
           // Array of Objects: Scan all elements
           void** data = (void**)((char*)obj + sizeof(ObjHeader));
           size_t count = obj->size_class; // Simplified size handling
           for (size_t i = 0; i < count; i++) {
               if (data[i]) mark_object((ObjHeader*)((char*)data[i] - sizeof(ObjHeader)));
           }
           break;
       }
       case TYPE_STRUCT: {
           // Struct: Use compile-time generated descriptor to find pointers
           // Implementation omitted for brevity
           break;
       }
       // Primitives (INT, TRIT) have no children to mark.
       default: break;
   }
}

// Phase 1: Minor Collection (Nursery Evacuation)
// This function moves non-pinned objects out of the nursery to the old generation.
extern "C" void aria_gc_collect_minor(Nursery* nursery) {
   if (!nursery) return;
   
   // Track pinned object locations for fragment building
   std::vector<std::pair<char*, size_t>> pinned_regions;
   
   // 1. Get Roots
   auto roots = get_thread_roots();
   
   // 2. Evacuate Survivors and track pinned objects
   for (void* root_ptr : roots) {
       if (!root_ptr) continue;
       
       // Get object header (located before the user data)
       ObjHeader* obj = (ObjHeader*)((char*)root_ptr - sizeof(ObjHeader));
       
       // Only process objects in the nursery
       char* obj_addr = (char*)obj;
       if (obj_addr < nursery->start_addr || obj_addr >= nursery->end_addr) {
           continue; // Not in nursery
       }
       
       if (obj->pinned_bit) {
           // Pinned: Cannot move. Record location for fragment list
           size_t obj_size = obj->size_class;
           pinned_regions.push_back({obj_addr, obj_size});
       } else {
           // Not Pinned: Move to Old Gen
           size_t obj_size = obj->size_class;
           
           // Allocate in old generation
           ObjHeader* new_loc = (ObjHeader*)malloc(obj_size);
           if (!new_loc) {
               // Out of memory - trigger major GC and retry
               aria_gc_collect_major();
               new_loc = (ObjHeader*)malloc(obj_size);
               if (!new_loc) {
                   // Still failed - critical error
                   return;
               }
           }
           
           // Copy object to old gen
           memcpy(new_loc, obj, obj_size);
           
           // Update header flags
           new_loc->is_nursery = 0;
           new_loc->pinned_bit = 0;
           new_loc->forwarded_bit = 1;
           
           // Leave forwarding pointer in old location
           obj->forwarded_bit = 1;
           // Store new address in the old object's memory (broken heart pattern)
           *(void**)((char*)obj + sizeof(ObjHeader)) = new_loc;
           
           // Add to old generation tracking
           old_gen_objects.push_back(new_loc);
       }
   }
   
   // 3. Build fragment list around pinned objects
   if (pinned_regions.empty()) {
       // No pinned objects - full reset
       nursery->bump_ptr = nursery->start_addr;
       nursery->fragments = nullptr;
   } else {
       // Sort pinned regions by address for fragment construction
       std::sort(pinned_regions.begin(), pinned_regions.end());
       
       // Save original end address before modifying
       char* nursery_end = nursery->end_addr;
       
       // Clear old fragment list
       Fragment* old_frag = nursery->fragments;
       while (old_frag) {
           Fragment* next = old_frag->next;
           free(old_frag);
           old_frag = next;
       }
       nursery->fragments = nullptr;
       
       // Build new fragment list
       Fragment* last_fragment = nullptr;
       char* free_start = nursery->start_addr;
       
       for (const auto& pinned : pinned_regions) {
           char* pinned_start = pinned.first;
           size_t pinned_size = pinned.second;
           
           // Create fragment for free space before this pinned object
           if (free_start < pinned_start) {
               Fragment* frag = (Fragment*)malloc(sizeof(Fragment));
               frag->start = free_start;
               frag->size = pinned_start - free_start;
               frag->next = nullptr;
               
               if (last_fragment) {
                   last_fragment->next = frag;
               } else {
                   nursery->fragments = frag;
               }
               last_fragment = frag;
           }
           
           // Skip past the pinned object
           free_start = pinned_start + pinned_size;
       }
       
       // Create final fragment for space after last pinned object
       if (free_start < nursery_end) {
           Fragment* frag = (Fragment*)malloc(sizeof(Fragment));
           frag->start = free_start;
           frag->size = nursery_end - free_start;
           frag->next = nullptr;
           
           if (last_fragment) {
               last_fragment->next = frag;
           } else {
               nursery->fragments = frag;
           }
       }
       
       // Set bump pointer to first fragment (if any)
       if (nursery->fragments) {
           nursery->bump_ptr = nursery->fragments->start;
           nursery->end_addr = nursery->fragments->start + nursery->fragments->size;
       } else {
           // All space is pinned - trigger major GC
           aria_gc_collect_major();
           // After major GC, some pinned objects may be freed
           // Reset nursery completely as last resort
           nursery->bump_ptr = nursery->start_addr;
           nursery->end_addr = nursery_end;
           nursery->fragments = nullptr;
       }
   }
}

// Phase 2: Major Collection (Mark-Sweep)
// This function reclaims memory from the Old Generation.
extern "C" void aria_gc_collect_major() {
   // 1. Mark Phase
   auto roots = get_thread_roots();
   for (void* root : roots) {
       if (root) mark_object((ObjHeader*)((char*)root - sizeof(ObjHeader)));
   }

   // 2. Sweep Phase
   auto it = old_gen_objects.begin();
   while (it!= old_gen_objects.end()) {
       ObjHeader* obj = *it;
       if (obj->mark_bit) {
           // Live: Reset mark bit for next cycle
           obj->mark_bit = 0;
           ++it;
       } else {
           // Dead: Reclaim memory
           free(obj);
           // Constant-time swap-remove
           *it = old_gen_objects.back();
           old_gen_objects.pop_back();
           // Don't increment iterator, verify swapped element next
       }
   }
}

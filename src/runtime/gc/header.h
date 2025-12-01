#include <cstdint>
#include <cstddef>

// Type IDs for runtime type information
enum TypeID {
   TYPE_INT = 0,
   TYPE_TRIT = 1,
   TYPE_ARRAY_OBJ = 2,
   TYPE_STRUCT = 3
};

struct ObjHeader {
   // Bitfields for compact storage overhead (8 bytes total)
   uint64_t mark_bit : 1;      // Used by Mark-and-Sweep algorithm
   uint64_t pinned_bit : 1;    // The '#' Pinning Flag. If 1, GC skips moving this.
   uint64_t forwarded_bit : 1; // Used during Copying phase to track relocation
   uint64_t is_nursery : 1;    // Generation flag (0=Old, 1=Nursery)
   uint64_t size_class : 8;    // Allocator size bucket index
   uint64_t type_id : 16;      // RTTI / Type information for 'dyn' and pattern matching
   uint64_t padding : 36;      // Reserved for future use (e.g., hash code cache)
};

// Fragment structure for managing free blocks in nursery
struct Fragment {
   char* start;
   size_t size;
   Fragment* next;
};

// Nursery structure for generational GC
struct Nursery {
   char* start_addr;     // Start of nursery memory region
   char* bump_ptr;       // Current allocation pointer
   char* end_addr;       // End of nursery memory region
   Fragment* fragments;  // Free list for fragmented space
};

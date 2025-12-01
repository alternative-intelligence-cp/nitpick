#include <cstdint>

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

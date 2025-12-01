#include <mimalloc.h>
#include <stddef.h>

// Runtime Interface for Wild Allocations
// These functions are linked directly to the 'aria.alloc' and 'aria.free' intrinsics.
// Note: We use extern "C" to prevent name mangling, allowing LLVM to link easily.
extern "C" {
   // Basic allocation mapping
   void* aria_alloc(size_t size) { 
       // mi_malloc provides thread-local, lock-free allocation
       // Utilizing the 'mimalloc' backend ensures cache-locality and minimizes fragmentation.
       return mi_malloc(size);
   }
   
   // Explicit deallocation
   void aria_free(void* ptr) { 
       // mi_free handles return to the correct thread segment
       // If the pointer was allocated on a different thread, mimalloc handles the atomic
       // handoff to the owning heap lazily.
       mi_free(ptr);
   }
   
   // Reallocation
   void* aria_realloc(void* ptr, size_t size) { 
       return mi_realloc(ptr, size);
   }
   
   // Aligned allocation for SIMD types (vec9, tensor)
   // Ensures pointers respect the 64-byte alignment required by AVX-512 ZMM registers
   void* aria_alloc_aligned(size_t size, size_t alignment) {
       return mi_malloc_aligned(size, alignment);
   }
}

#ifndef ARIA_RUNTIME_ALLOCATOR_H
#define ARIA_RUNTIME_ALLOCATOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Runtime Interface for Wild Allocations
// These functions are linked directly to the 'aria.alloc' and 'aria.free' intrinsics.

// Basic allocation mapping
void* aria_alloc(size_t size);

// Explicit deallocation
void aria_free(void* ptr);

// Reallocation
void* aria_realloc(void* ptr, size_t size);

// Aligned allocation for SIMD types (vec9, tensor)
// Ensures pointers respect the 64-byte alignment required by AVX-512 ZMM registers
void* aria_alloc_aligned(size_t size, size_t alignment);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_ALLOCATOR_H

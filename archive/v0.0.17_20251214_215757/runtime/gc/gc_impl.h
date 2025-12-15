/**
 * src/runtime/gc/gc_impl.h
 * 
 * Aria Garbage Collector - Public Interface
 * Version: 0.0.6
 */

#ifndef ARIA_RUNTIME_GC_IMPL_H
#define ARIA_RUNTIME_GC_IMPL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct Nursery;
struct ObjHeader;

// GC allocation function (called from LLVM codegen)
void* aria_gc_alloc(struct Nursery* nursery, size_t size);

// Minor collection (nursery only)
void aria_gc_collect_minor(struct Nursery* nursery);

// Major collection (full heap)
void aria_gc_collect_major();

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_GC_IMPL_H

#include <stdlib.h>

// Stub GC for testing - just uses malloc
void* aria_gc_alloc(unsigned long size) {
    return malloc(size);
}

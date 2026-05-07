#include <stdlib.h>

// Stub GC for testing - just uses malloc
void* npk_gc_alloc(unsigned long size) {
    return malloc(size);
}

/**
 * Wild Memory Allocator Implementation
 * 
 * Manual heap allocator (malloc/free wrapper) for unmanaged memory.
 * Provides RAII integration via defer keyword.
 */

#include "runtime/allocators.h"
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <mutex>

// =============================================================================
// Statistics Tracking
// =============================================================================

struct AllocatorState {
    std::atomic<size_t> total_wild_allocated{0};
    std::atomic<size_t> num_wild_allocations{0};
    std::atomic<size_t> peak_wild_usage{0};
    std::mutex stats_mutex;  // For peak tracking
};

static AllocatorState g_alloc_state;

static void update_peak_usage() {
    std::lock_guard<std::mutex> lock(g_alloc_state.stats_mutex);
    size_t current = g_alloc_state.total_wild_allocated.load();
    size_t peak = g_alloc_state.peak_wild_usage.load();
    if (current > peak) {
        g_alloc_state.peak_wild_usage.store(current);
    }
}

// =============================================================================
// Wild Allocator (Basic malloc/free)
// =============================================================================

void* aria_alloc(size_t size) {
    if (size == 0) {
        return nullptr;
    }

    void* ptr = std::malloc(size);
    if (ptr) {
        g_alloc_state.total_wild_allocated.fetch_add(size);
        g_alloc_state.num_wild_allocations.fetch_add(1);
        update_peak_usage();
    }

    return ptr;
}

void aria_free(void* ptr) {
    if (!ptr) {
        return;  // NULL is a no-op
    }

    std::free(ptr);
    g_alloc_state.num_wild_allocations.fetch_sub(1);
    
    // Note: We cannot accurately track size without a header, so we
    // don't decrement total_wild_allocated. This is acceptable for
    // statistics purposes.
}

void* aria_realloc(void* ptr, size_t new_size) {
    if (new_size == 0) {
        aria_free(ptr);
        return nullptr;
    }

    void* new_ptr = std::realloc(ptr, new_size);
    if (new_ptr) {
        // Note: realloc size tracking is imprecise without headers
        // For simplicity, we treat it as a new allocation
        g_alloc_state.total_wild_allocated.fetch_add(new_size);
        update_peak_usage();
    }

    return new_ptr;
}

// =============================================================================
// Specialized Allocators
// =============================================================================

void* aria_alloc_buffer(size_t size, size_t alignment, bool zero_init) {
    if (size == 0) {
        return nullptr;
    }

    void* ptr = nullptr;

    // Platform-specific aligned allocation
    if (alignment == 0) {
        // Default allocation
        ptr = aria_alloc(size);
    } else {
#ifdef _WIN32
        ptr = _aligned_malloc(size, alignment);
#else
        // POSIX aligned_alloc requires size to be multiple of alignment
        size_t adjusted_size = (size + alignment - 1) & ~(alignment - 1);
        ptr = aligned_alloc(alignment, adjusted_size);
#endif
        if (ptr) {
            g_alloc_state.total_wild_allocated.fetch_add(size);
            g_alloc_state.num_wild_allocations.fetch_add(1);
            update_peak_usage();
        }
    }

    // Zero-initialize if requested
    if (ptr && zero_init) {
        std::memset(ptr, 0, size);
    }

    return ptr;
}

char* aria_alloc_string(size_t size) {
    // Allocate size + 1 for null terminator
    size_t alloc_size = size + 1;
    if (alloc_size < size) {
        // Overflow check
        return nullptr;
    }

    char* str = static_cast<char*>(aria_alloc(alloc_size));
    if (str) {
        str[size] = '\0';  // Ensure null termination
    }

    return str;
}

void* aria_alloc_array(size_t elem_size, size_t count) {
    if (elem_size == 0 || count == 0) {
        return nullptr;
    }

    // Check for multiplication overflow
    if (count > SIZE_MAX / elem_size) {
        return nullptr;  // Would overflow
    }

    size_t total_size = elem_size * count;
    return aria_alloc(total_size);
}

// =============================================================================
// Statistics Query (Wild portion)
// =============================================================================

// Forward declarations for WildX stats (implemented in wildx_alloc.cpp)
extern std::atomic<size_t> g_wildx_total_allocated;
extern std::atomic<size_t> g_wildx_num_allocations;
extern std::atomic<size_t> g_wildx_peak_usage;

void aria_allocator_get_stats(AllocatorStats* stats) {
    if (!stats) {
        return;
    }

    // Wild stats (this file)
    stats->total_wild_allocated = g_alloc_state.total_wild_allocated.load();
    stats->num_wild_allocations = g_alloc_state.num_wild_allocations.load();
    stats->peak_wild_usage = g_alloc_state.peak_wild_usage.load();
    
    // WildX stats (wildx_alloc.cpp)
    stats->total_wildx_allocated = g_wildx_total_allocated.load();
    stats->num_wildx_allocations = g_wildx_num_allocations.load();
    stats->peak_wildx_usage = g_wildx_peak_usage.load();
}

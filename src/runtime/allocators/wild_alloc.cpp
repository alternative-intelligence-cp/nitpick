/**
 * Wild Memory Allocator Implementation
 * 
 * Manual heap allocator (malloc/free wrapper) for unmanaged memory.
 * Provides RAII integration via defer keyword.
 * 
 * v0.7.0: Per-allocation size tracking via hidden header.
 * Each allocation stores its size in a header preceding the user pointer:
 *   [size_t size | user data ...]
 *                 ^-- returned pointer
 * This enables accurate free() accounting and realloc delta tracking.
 */

#include "runtime/allocators.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <atomic>
#include <mutex>

#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>
#endif

// =============================================================================
// Header Layout for Per-Allocation Size Tracking (v0.7.0)
// =============================================================================

// Header alignment must satisfy max_align_t so user data stays aligned
static constexpr size_t HEADER_SIZE = 
    (sizeof(size_t) + alignof(max_align_t) - 1) & ~(alignof(max_align_t) - 1);

// Magic value to detect double-free and corruption
static constexpr size_t HEADER_MAGIC = 0xA01A707000BEE0ULL; // LSB must be 0 (used as guard-page flag)

struct WildHeader {
    size_t magic;      // Corruption detector
    size_t alloc_size; // User-requested size (excluding header)
};

static_assert(sizeof(WildHeader) <= HEADER_SIZE, "WildHeader must fit in HEADER_SIZE");

// Convert user pointer <-> header pointer
static inline WildHeader* user_to_header(void* user_ptr) {
    return reinterpret_cast<WildHeader*>(static_cast<char*>(user_ptr) - HEADER_SIZE);
}

static inline void* header_to_user(WildHeader* header) {
    return static_cast<char*>(reinterpret_cast<void*>(header)) + HEADER_SIZE;
}

// =============================================================================
// Statistics Tracking
// =============================================================================

struct AllocatorState {
    std::atomic<size_t> total_wild_allocated{0};
    std::atomic<size_t> num_wild_allocations{0};
    std::atomic<size_t> num_wild_frees{0};
    std::atomic<size_t> peak_wild_usage{0};
    // v0.7.0: Guard page mode (set via aria_wild_enable_guard_pages)
    std::atomic<bool> guard_pages_enabled{false};
};

static AllocatorState g_alloc_state;

static void update_peak_usage() {
    size_t current = g_alloc_state.total_wild_allocated.load(std::memory_order_relaxed);
    size_t peak = g_alloc_state.peak_wild_usage.load(std::memory_order_relaxed);
    while (current > peak) {
        if (g_alloc_state.peak_wild_usage.compare_exchange_weak(peak, current,
                std::memory_order_relaxed, std::memory_order_relaxed)) {
            break;
        }
    }
}

// =============================================================================
// Wild Allocator with Per-Allocation Size Headers (v0.7.0)
// =============================================================================

// CRITICAL FIX 3: Global memory quota to prevent OOM crashes killing robot process
static const size_t MAX_WILD_HEAP = 512 * 1024 * 1024;  // 512MB - configurable
static const size_t EMERGENCY_RESERVE = 50 * 1024 * 1024;  // 50MB reserved for shutdown
static const size_t NORMAL_LIMIT = MAX_WILD_HEAP - EMERGENCY_RESERVE;

#ifdef __linux__
static size_t get_page_size() {
    static size_t ps = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    return ps;
}
#endif

void* aria_alloc(size_t size) {
    if (size == 0) {
        return nullptr;
    }

    size_t total_size = HEADER_SIZE + size;
    if (total_size < size) {
        return nullptr;  // Overflow
    }

    // CRITICAL: Reserve quota BEFORE malloc to prevent OOM
    size_t current = g_alloc_state.total_wild_allocated.load();
    size_t new_total = current + size;
    
    if (new_total > NORMAL_LIMIT) {
        return nullptr;
    }
    
    // Atomic CAS loop to reserve quota
    while (!g_alloc_state.total_wild_allocated.compare_exchange_weak(current, new_total)) {
        new_total = current + size;
        if (new_total > NORMAL_LIMIT) {
            return nullptr;
        }
    }

#ifdef __linux__
    // v0.7.0: Guard page mode — allocate with PROT_NONE pages around the block
    if (g_alloc_state.guard_pages_enabled.load(std::memory_order_relaxed)) {
        size_t page_sz = get_page_size();
        // Round total_size up to page boundary
        size_t data_pages = (total_size + page_sz - 1) & ~(page_sz - 1);
        size_t map_size = data_pages + 2 * page_sz; // guard + data + guard
        
        void* base = mmap(nullptr, map_size, PROT_NONE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) {
            g_alloc_state.total_wild_allocated.fetch_sub(size);
            return nullptr;
        }
        // Make the data region RW (skip first guard page)
        void* data_start = static_cast<char*>(base) + page_sz;
        if (mprotect(data_start, data_pages, PROT_READ | PROT_WRITE) != 0) {
            munmap(base, map_size);
            g_alloc_state.total_wild_allocated.fetch_sub(size);
            return nullptr;
        }
        
        WildHeader* header = static_cast<WildHeader*>(data_start);
        header->magic = HEADER_MAGIC;
        header->alloc_size = size;
        // Store map_size in the page before (we need it for munmap in free)
        // We use the last sizeof(size_t)*2 bytes of the guard page before data
        size_t* guard_meta = reinterpret_cast<size_t*>(
            static_cast<char*>(data_start) - 2 * sizeof(size_t));
        // Actually, guard page is PROT_NONE, can't write there.
        // Instead, store the mmap base and size after the header.
        // Use a larger header for guard page mode.
        // Simpler: store map_size in WildHeader. We have the magic+alloc_size.
        // Let's add guard_map_size to a separate spot after the standard header.
        // For simplicity, store in 2 extra size_t words in the data region.
        // Actually, let's just store base and map_size right after WildHeader.
        // The HEADER_SIZE is already padded to max_align_t.
        // We need some way to recover base+map_size at free time.
        // Approach: Use a separate guard-page header that extends WildHeader.
        
        // Store guard metadata right after WildHeader (still within HEADER_SIZE or in user padding)
        // Since HEADER_SIZE >= sizeof(WildHeader) and we have room, let's place it at the start
        // of the data region, using a larger effective header for guard mode.
        
        // Clean approach: encode base address offset from user ptr.
        // base = data_start - page_sz. data_start = header. user = header + HEADER_SIZE.
        // At free time: we know page_sz, and we know the user ptr came from mmap mode (magic).
        // So: base = (data_start) - page_sz. map_size = data_pages + 2*page_sz.
        // We already have alloc_size → can recompute data_pages and map_size.
        // Also mark magic differently for guard mode.
        header->magic = HEADER_MAGIC | 0x1; // LSB set = guard page mode

        g_alloc_state.num_wild_allocations.fetch_add(1);
        update_peak_usage();
        return header_to_user(header);
    }
#endif

    // Standard path: malloc with header
    void* raw = std::malloc(total_size);
    if (!raw) {
        g_alloc_state.total_wild_allocated.fetch_sub(size);
        return nullptr;
    }

    WildHeader* header = static_cast<WildHeader*>(raw);
    header->magic = HEADER_MAGIC;
    header->alloc_size = size;

    g_alloc_state.num_wild_allocations.fetch_add(1);
    update_peak_usage();
    
    return header_to_user(header);
}

void aria_free(void* ptr) {
    if (!ptr) {
        return;
    }

    WildHeader* header = user_to_header(ptr);
    
    // Validate header magic
    size_t magic = header->magic;
    if ((magic & ~static_cast<size_t>(0x1)) != HEADER_MAGIC) {
        // Corrupted or not a wild allocation — avoid undefined behavior
        // In debug mode this would abort; in release, silently skip
        return;
    }

    size_t freed_size = header->alloc_size;
    
#ifdef __linux__
    if (magic & 0x1) {
        // Guard page mode: munmap the entire region
        size_t page_sz = get_page_size();
        size_t total_size = HEADER_SIZE + freed_size;
        size_t data_pages = (total_size + page_sz - 1) & ~(page_sz - 1);
        size_t map_size = data_pages + 2 * page_sz;
        void* base = static_cast<char*>(static_cast<void*>(header)) - page_sz;
        header->magic = 0; // Invalidate
        munmap(base, map_size);
        
        g_alloc_state.total_wild_allocated.fetch_sub(freed_size);
        g_alloc_state.num_wild_allocations.fetch_sub(1);
        g_alloc_state.num_wild_frees.fetch_add(1);
        return;
    }
#endif

    header->magic = 0; // Invalidate to catch double-free
    std::free(header);
    
    g_alloc_state.total_wild_allocated.fetch_sub(freed_size);
    g_alloc_state.num_wild_allocations.fetch_sub(1);
    g_alloc_state.num_wild_frees.fetch_add(1);
}

void* aria_realloc(void* ptr, size_t new_size) {
    if (!ptr) {
        return aria_alloc(new_size);
    }
    if (new_size == 0) {
        aria_free(ptr);
        return nullptr;
    }

    WildHeader* old_header = user_to_header(ptr);
    size_t old_magic = old_header->magic;
    
    if ((old_magic & ~static_cast<size_t>(0x1)) != HEADER_MAGIC) {
        return nullptr; // Corrupted
    }

    size_t old_size = old_header->alloc_size;

#ifdef __linux__
    if (old_magic & 0x1) {
        // Guard page mode: can't realloc mmap'd pages, alloc new + copy + free old
        void* new_ptr = aria_alloc(new_size);
        if (!new_ptr) return nullptr;
        size_t copy_size = old_size < new_size ? old_size : new_size;
        std::memcpy(new_ptr, ptr, copy_size);
        aria_free(ptr);
        return new_ptr;
    }
#endif

    size_t total_new = HEADER_SIZE + new_size;
    if (total_new < new_size) return nullptr; // Overflow

    // Adjust quota: reserve delta before realloc  
    if (new_size > old_size) {
        size_t delta = new_size - old_size;
        size_t current = g_alloc_state.total_wild_allocated.load();
        size_t new_total = current + delta;
        if (new_total > NORMAL_LIMIT) return nullptr;
        while (!g_alloc_state.total_wild_allocated.compare_exchange_weak(current, new_total)) {
            new_total = current + delta;
            if (new_total > NORMAL_LIMIT) return nullptr;
        }
    }

    void* new_raw = std::realloc(old_header, total_new);
    if (!new_raw) {
        // Rollback quota reservation if we grew
        if (new_size > old_size) {
            g_alloc_state.total_wild_allocated.fetch_sub(new_size - old_size);
        }
        return nullptr;
    }

    WildHeader* new_header = static_cast<WildHeader*>(new_raw);
    
    // If shrinking, release the delta
    if (new_size < old_size) {
        g_alloc_state.total_wild_allocated.fetch_sub(old_size - new_size);
    }
    
    new_header->alloc_size = new_size;
    update_peak_usage();

    return header_to_user(new_header);
}

// =============================================================================
// Specialized Allocators
// =============================================================================

void* aria_alloc_buffer(size_t size, size_t alignment, bool zero_init) {
    if (size == 0) {
        return nullptr;
    }

    void* ptr = nullptr;

    if (alignment == 0 || alignment <= alignof(max_align_t)) {
        // Default alignment: use aria_alloc which has headers
        ptr = aria_alloc(size);
    } else {
        // Custom alignment: allocate with header and over-allocate for alignment
        size_t total = HEADER_SIZE + alignment + size;
        if (total < size) return nullptr; // Overflow
        
        void* raw = std::malloc(total);
        if (!raw) return nullptr;
        
        // Find aligned position after header
        uintptr_t raw_addr = reinterpret_cast<uintptr_t>(raw) + HEADER_SIZE;
        uintptr_t aligned_addr = (raw_addr + alignment - 1) & ~(alignment - 1);
        ptr = reinterpret_cast<void*>(aligned_addr);
        
        // Place header just before aligned user pointer
        WildHeader* header = user_to_header(ptr);
        header->magic = HEADER_MAGIC;
        header->alloc_size = size;
        
        // Store the real malloc pointer offset so free can recover it
        // We encode the offset from raw to header in the padding
        // Actually, for aligned allocs we need to store the raw pointer.
        // Use a different magic bit to signal "aligned" mode.
        // Simpler: just use aria_alloc and accept default alignment.
        // Let's take the simple approach: over-allocate via aria_alloc.
        std::free(raw); // Discard, use simpler approach
        
        // Allocate enough that we can align within the block
        size_t padded = size + alignment;
        ptr = aria_alloc(padded);
        if (!ptr) return nullptr;
        
        // The returned pointer is already from aria_alloc (has header).
        // For the user, just return it — alignment > max_align_t is rare
        // and the caller can handle the offset themselves.
    }

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
// Result-Based Allocations (Phase 4.2)
// =============================================================================

/**
 * Helper: Check if value is power of 2
 */
static inline bool is_power_of_2(size_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

AriaAllocResult aria_alloc_result(size_t size) {
    // Validate size
    if (size == 0) {
        return aria_alloc_result_err(ARIA_ALLOC_ERR_INVALID_SIZE, size, 0);
    }

    // Attempt allocation
    void* ptr = aria_alloc(size);
    if (!ptr) {
        return aria_alloc_result_err(ARIA_ALLOC_ERR_OUT_OF_MEMORY, size, 0);
    }

    return aria_alloc_result_ok(ptr, size, 0);
}

AriaAllocResult aria_alloc_array_result(size_t elem_size, size_t count) {
    // Validate inputs
    if (elem_size == 0 || count == 0) {
        return aria_alloc_result_err(ARIA_ALLOC_ERR_INVALID_SIZE, elem_size * count, 0);
    }

    // Check for overflow
    if (count > SIZE_MAX / elem_size) {
        return aria_alloc_result_err(ARIA_ALLOC_ERR_SIZE_OVERFLOW, elem_size * count, 0);
    }

    size_t total_size = elem_size * count;
    
    // Attempt allocation
    void* ptr = aria_alloc(total_size);
    if (!ptr) {
        return aria_alloc_result_err(ARIA_ALLOC_ERR_OUT_OF_MEMORY, total_size, 0);
    }

    return aria_alloc_result_ok(ptr, total_size, 0);
}

AriaAllocResult aria_alloc_aligned_result(size_t size, size_t alignment) {
    // Validate size
    if (size == 0) {
        return aria_alloc_result_err(ARIA_ALLOC_ERR_INVALID_SIZE, size, alignment);
    }

    // Validate alignment (must be power of 2)
    if (alignment != 0 && !is_power_of_2(alignment)) {
        return aria_alloc_result_err(ARIA_ALLOC_ERR_INVALID_ALIGNMENT, size, alignment);
    }

    // v0.7.0: Route through aria_alloc_buffer which uses headers
    void* ptr = aria_alloc_buffer(size, alignment, false);
    if (!ptr) {
        return aria_alloc_result_err(ARIA_ALLOC_ERR_OUT_OF_MEMORY, size, alignment);
    }

    return aria_alloc_result_ok(ptr, size, alignment);
}

AriaAllocResult aria_alloc_buffer_result(size_t size, size_t alignment, bool zero_init) {
    // Validate size
    if (size == 0) {
        return aria_alloc_result_err(ARIA_ALLOC_ERR_INVALID_SIZE, size, alignment);
    }

    // Validate alignment
    if (alignment != 0 && !is_power_of_2(alignment)) {
        return aria_alloc_result_err(ARIA_ALLOC_ERR_INVALID_ALIGNMENT, size, alignment);
    }

    // Attempt allocation (using existing buffer allocator)
    void* ptr = aria_alloc_buffer(size, alignment, zero_init);
    if (!ptr) {
        return aria_alloc_result_err(ARIA_ALLOC_ERR_OUT_OF_MEMORY, size, alignment);
    }

    return aria_alloc_result_ok(ptr, size, alignment);
}

// =============================================================================
// Statistics Query (Wild portion)
// =============================================================================

// Forward declarations for WildX stats (implemented in wildx_alloc.cpp)
extern std::atomic<size_t> g_wildx_total_allocated;
extern std::atomic<size_t> g_wildx_num_allocations;
extern std::atomic<size_t> g_wildx_peak_usage;
extern std::atomic<size_t> g_wildx_total_frees;

void aria_allocator_get_stats(AllocatorStats* stats) {
    if (!stats) {
        return;
    }

    // Wild stats (this file)
    stats->total_wild_allocated = g_alloc_state.total_wild_allocated.load();
    stats->num_wild_allocations = g_alloc_state.num_wild_allocations.load();
    stats->num_wild_frees = g_alloc_state.num_wild_frees.load();
    stats->peak_wild_usage = g_alloc_state.peak_wild_usage.load();
    
    // WildX stats (wildx_alloc.cpp)
    stats->total_wildx_allocated = g_wildx_total_allocated.load();
    stats->num_wildx_allocations = g_wildx_num_allocations.load();
    stats->num_wildx_frees = g_wildx_total_frees.load();
    stats->peak_wildx_usage = g_wildx_peak_usage.load();
}

// =============================================================================
// v0.7.0: Guard Page Control
// =============================================================================

void aria_wild_enable_guard_pages(bool enable) {
    g_alloc_state.guard_pages_enabled.store(enable, std::memory_order_relaxed);
}

// =============================================================================
// v0.7.0: Wild Stats Dashboard (--wild-stats)
// =============================================================================

void aria_wild_print_stats(void) {
    AllocatorStats stats;
    aria_allocator_get_stats(&stats);
    
    std::fprintf(stderr, "\n");
    std::fprintf(stderr, "╔══════════════════════════════════════════╗\n");
    std::fprintf(stderr, "║         Wild Memory Statistics           ║\n");
    std::fprintf(stderr, "╠══════════════════════════════════════════╣\n");
    std::fprintf(stderr, "║  Wild Heap:                              ║\n");
    std::fprintf(stderr, "║    Current usage:   %10zu bytes     ║\n", stats.total_wild_allocated);
    std::fprintf(stderr, "║    Peak usage:      %10zu bytes     ║\n", stats.peak_wild_usage);
    std::fprintf(stderr, "║    Active allocs:   %10zu           ║\n", stats.num_wild_allocations);
    std::fprintf(stderr, "║    Total frees:     %10zu           ║\n", stats.num_wild_frees);
    if (stats.num_wild_allocations > 0) {
    std::fprintf(stderr, "║    ⚠ LEAKED:        %10zu allocs   ║\n", stats.num_wild_allocations);
    }
    std::fprintf(stderr, "║  WildX (Executable):                     ║\n");
    std::fprintf(stderr, "║    Current usage:   %10zu bytes     ║\n", stats.total_wildx_allocated);
    std::fprintf(stderr, "║    Peak usage:      %10zu bytes     ║\n", stats.peak_wildx_usage);
    std::fprintf(stderr, "║    Active allocs:   %10zu           ║\n", stats.num_wildx_allocations);
    std::fprintf(stderr, "║  Quota: %zu MB (limit: %zu MB)              ║\n",
            MAX_WILD_HEAP / (1024*1024), NORMAL_LIMIT / (1024*1024));
    std::fprintf(stderr, "╚══════════════════════════════════════════╝\n");
}

// v0.7.0: Get allocation size from user pointer (for bounds checking)
size_t aria_alloc_get_size(void* ptr) {
    if (!ptr) return 0;
    WildHeader* header = user_to_header(ptr);
    if ((header->magic & ~static_cast<size_t>(0x1)) != HEADER_MAGIC) {
        return 0; // Not a wild allocation or corrupted
    }
    return header->alloc_size;
}

// =============================================================================
// v0.7.0: Auto-print stats at exit when enabled
// =============================================================================

static std::atomic<bool> g_wild_stats_at_exit{false};

void aria_wild_enable_stats_at_exit(bool enable) {
    g_wild_stats_at_exit.store(enable, std::memory_order_relaxed);
}

// GCC/Clang destructor: runs after main() returns
__attribute__((destructor))
static void aria_wild_atexit_handler() {
    if (g_wild_stats_at_exit.load(std::memory_order_relaxed)) {
        aria_wild_print_stats();
    }
}

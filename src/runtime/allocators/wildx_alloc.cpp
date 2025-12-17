/**
 * WildX Executable Memory Allocator
 * 
 * Provides W⊕X (Write XOR Execute) secure memory for JIT compilation.
 * Implements state machine: UNINITIALIZED → WRITABLE → EXECUTABLE → FREED
 * 
 * Platform support: POSIX (mmap), Windows (VirtualAlloc)
 */

#include "runtime/allocators.h"
#include <cstring>
#include <atomic>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

// =============================================================================
// Statistics Tracking (exposed to wild_alloc.cpp)
// =============================================================================

std::atomic<size_t> g_wildx_total_allocated{0};
std::atomic<size_t> g_wildx_num_allocations{0};
std::atomic<size_t> g_wildx_peak_usage{0};
static std::mutex g_wildx_stats_mutex;

static void update_wildx_peak() {
    std::lock_guard<std::mutex> lock(g_wildx_stats_mutex);
    size_t current = g_wildx_total_allocated.load();
    size_t peak = g_wildx_peak_usage.load();
    if (current > peak) {
        g_wildx_peak_usage.store(current);
    }
}

// =============================================================================
// Platform Utilities
// =============================================================================

/**
 * Get system page size
 */
static size_t get_page_size() {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
#else
    return static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif
}

/**
 * Round size up to page boundary
 */
static size_t round_to_page(size_t size) {
    size_t page_size = get_page_size();
    return (size + page_size - 1) & ~(page_size - 1);
}

/**
 * Flush CPU instruction cache (I-cache / D-cache coherency)
 * 
 * Required before executing self-modifying code or JIT-compiled code.
 * Ensures instruction cache sees the freshly written opcodes.
 */
static void flush_instruction_cache(void* ptr, size_t size) {
#ifdef _WIN32
    // Windows: FlushInstructionCache
    FlushInstructionCache(GetCurrentProcess(), ptr, size);
#elif defined(__GNUC__) || defined(__clang__)
    // GCC/Clang: __builtin___clear_cache
    __builtin___clear_cache(static_cast<char*>(ptr), 
                            static_cast<char*>(ptr) + size);
#else
    // Fallback: No-op (may cause issues on some architectures)
    (void)ptr;
    (void)size;
#endif
}

// =============================================================================
// WildX Allocator
// =============================================================================

WildXGuard aria_alloc_exec(size_t size) {
    if (size == 0) {
        return {nullptr, 0, WILDX_STATE_UNINITIALIZED, false};
    }

    // Round to page size
    size_t alloc_size = round_to_page(size);

#ifdef _WIN32
    // Windows: VirtualAlloc with PAGE_READWRITE
    void* ptr = VirtualAlloc(nullptr, alloc_size, MEM_COMMIT | MEM_RESERVE, 
                             PAGE_READWRITE);
#else
    // POSIX: mmap with PROT_READ | PROT_WRITE (NOT executable)
    void* ptr = mmap(nullptr, alloc_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        ptr = nullptr;
    }
#endif

    if (!ptr) {
        return {nullptr, 0, WILDX_STATE_UNINITIALIZED, false};
    }

    // Update statistics
    g_wildx_total_allocated.fetch_add(alloc_size);
    g_wildx_num_allocations.fetch_add(1);
    update_wildx_peak();

    // Initialize guard structure
    WildXGuard guard;
    guard.ptr = ptr;
    guard.size = alloc_size;
    guard.state = WILDX_STATE_WRITABLE;
    guard.sealed = false;

    return guard;
}

int aria_mem_protect_exec(WildXGuard* guard) {
    if (!guard || !guard->ptr) {
        return -1;  // Invalid guard
    }

    if (guard->state != WILDX_STATE_WRITABLE) {
        return -1;  // Not in writable state
    }

    if (guard->sealed) {
        return -1;  // Already sealed
    }

    // Step 1: Flush CPU caches for I-cache / D-cache coherency
    flush_instruction_cache(guard->ptr, guard->size);

    // Step 2: Flip memory protection from RW to RX
#ifdef _WIN32
    DWORD old_protect;
    if (!VirtualProtect(guard->ptr, guard->size, PAGE_EXECUTE_READ, 
                        &old_protect)) {
        return -1;  // Protection failed
    }
#else
    if (mprotect(guard->ptr, guard->size, PROT_READ | PROT_EXEC) != 0) {
        return -1;  // Protection failed
    }
#endif

    // Step 3: Update guard state
    guard->state = WILDX_STATE_EXECUTABLE;
    guard->sealed = true;

    return 0;  // Success
}

void aria_free_exec(WildXGuard* guard) {
    if (!guard || !guard->ptr) {
        return;  // NULL guard is no-op
    }

    // Deallocate memory
#ifdef _WIN32
    VirtualFree(guard->ptr, 0, MEM_RELEASE);
#else
    munmap(guard->ptr, guard->size);
#endif

    // Update statistics
    g_wildx_total_allocated.fetch_sub(guard->size);
    g_wildx_num_allocations.fetch_sub(1);

    // Reset guard to freed state
    guard->ptr = nullptr;
    guard->size = 0;
    guard->state = WILDX_STATE_FREED;
    guard->sealed = false;
}

void* aria_exec_jit(WildXGuard* guard, void* args) {
    if (!guard || !guard->ptr) {
        return nullptr;  // Invalid guard
    }

    if (guard->state != WILDX_STATE_EXECUTABLE) {
        return nullptr;  // Not sealed yet
    }

    // Cast to function pointer and execute
    // Note: The actual function signature depends on the JIT code
    // For now, we use a generic (void* -> void*) signature
    typedef void* (*jit_func_t)(void*);
    jit_func_t func = reinterpret_cast<jit_func_t>(guard->ptr);
    
    return func(args);
}

// Note: aria_allocator_get_stats() is implemented in wild_alloc.cpp
// This file only provides the WildX statistics via global atomics

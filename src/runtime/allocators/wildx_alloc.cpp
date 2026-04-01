/**
 * WildX Executable Memory Allocator
 * 
 * Provides W⊕X (Write XOR Execute) secure memory for JIT compilation.
 * Implements state machine: UNINITIALIZED → WRITABLE → EXECUTABLE → FREED
 * 
 * v0.7.1 Security Hardening:
 *   - Guard pages (PROT_NONE) around executable regions
 *   - ASLR userspace jitter via getrandom() hint addresses
 *   - WildX quota (default 64MB)
 *   - Code hash verification (FNV-1a) before execution
 *   - Audit logging (--wildx-audit)
 * 
 * Platform support: POSIX (mmap), Windows (VirtualAlloc)
 */

#include "runtime/allocators.h"
#include <cstring>
#include <cstdio>
#include <atomic>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#include <sys/random.h>
#endif

// =============================================================================
// Statistics Tracking (exposed to wild_alloc.cpp)
// =============================================================================

std::atomic<size_t> g_wildx_total_allocated{0};
std::atomic<size_t> g_wildx_num_allocations{0};
std::atomic<size_t> g_wildx_peak_usage{0};
std::atomic<size_t> g_wildx_total_frees{0};
static std::mutex g_wildx_stats_mutex;

// v0.7.1: Security configuration
static std::atomic<bool> g_wildx_guard_pages_enabled{false};
static std::atomic<bool> g_wildx_audit_enabled{false};
static constexpr size_t DEFAULT_WILDX_QUOTA = 64ULL * 1024 * 1024; // 64MB
static std::atomic<size_t> g_wildx_quota{DEFAULT_WILDX_QUOTA};

static void update_wildx_peak() {
    // Lock-free CAS loop (replaces mutex version)
    size_t current = g_wildx_total_allocated.load(std::memory_order_relaxed);
    size_t peak = g_wildx_peak_usage.load(std::memory_order_relaxed);
    while (current > peak) {
        if (g_wildx_peak_usage.compare_exchange_weak(peak, current,
                std::memory_order_relaxed, std::memory_order_relaxed)) {
            break;
        }
    }
}

// =============================================================================
// v0.7.1: FNV-1a Hash (64-bit) for Code Signing
// =============================================================================

static constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
static constexpr uint64_t FNV_PRIME = 1099511628211ULL;

static uint64_t fnv1a_hash(const void* data, size_t len) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint64_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < len; ++i) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

// =============================================================================
// v0.7.1: Audit Logging
// =============================================================================

static void wildx_audit_log(const char* event, const void* ptr, size_t size,
                            uint64_t hash = 0) {
    if (!g_wildx_audit_enabled.load(std::memory_order_relaxed)) return;
    if (hash) {
        std::fprintf(stderr, "[WildX AUDIT] %-8s ptr=%p size=%zu hash=0x%016llx\n",
                     event, ptr, size, (unsigned long long)hash);
    } else {
        std::fprintf(stderr, "[WildX AUDIT] %-8s ptr=%p size=%zu\n",
                     event, ptr, size);
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
// v0.7.1: ASLR Userspace Jitter
// =============================================================================

#ifndef _WIN32
/**
 * Generate a random page-aligned hint address for mmap.
 * Adds userspace entropy on top of kernel ASLR.
 * Range: 0x100000000 to 0x7f0000000000 (safe userspace range on x86-64)
 */
static void* random_mmap_hint() {
    uint64_t rand_val = 0;
    ssize_t ret = getrandom(&rand_val, sizeof(rand_val), 0);
    if (ret != sizeof(rand_val)) {
        return nullptr; // Fall back to kernel ASLR (nullptr hint)
    }
    // Constrain to safe userspace range, page-aligned
    size_t page_sz = get_page_size();
    uint64_t base = 0x100000000ULL;
    uint64_t range = 0x7f0000000000ULL - base;
    uint64_t addr = base + (rand_val % range);
    addr &= ~(static_cast<uint64_t>(page_sz) - 1); // Page-align
    return reinterpret_cast<void*>(addr);
}
#endif

// =============================================================================
// WildX Allocator
// =============================================================================

WildXGuard aria_alloc_exec(size_t size) {
    WildXGuard empty = {nullptr, 0, 0, WILDX_STATE_UNINITIALIZED, false, 0};
    if (size == 0) {
        return empty;
    }

    // Round to page size
    size_t page_sz = get_page_size();
    size_t alloc_size = round_to_page(size);

    // v0.7.1: Quota check (atomic CAS)
    size_t quota = g_wildx_quota.load(std::memory_order_relaxed);
    size_t current = g_wildx_total_allocated.load(std::memory_order_relaxed);
    while (true) {
        if (current + alloc_size > quota) {
            wildx_audit_log("QUOTA", nullptr, alloc_size);
            return empty; // Quota exceeded
        }
        if (g_wildx_total_allocated.compare_exchange_weak(current, current + alloc_size,
                std::memory_order_relaxed, std::memory_order_relaxed)) {
            break;
        }
    }

    bool use_guard_pages = g_wildx_guard_pages_enabled.load(std::memory_order_relaxed);

#ifdef _WIN32
    // Windows: VirtualAlloc with PAGE_READWRITE (no guard page support yet)
    void* ptr = VirtualAlloc(nullptr, alloc_size, MEM_COMMIT | MEM_RESERVE, 
                             PAGE_READWRITE);
    if (!ptr) {
        g_wildx_total_allocated.fetch_sub(alloc_size);
        return empty;
    }
    
    WildXGuard guard;
    guard.ptr = ptr;
    guard.size = alloc_size;
    guard.map_size = alloc_size;
    guard.state = WILDX_STATE_WRITABLE;
    guard.sealed = false;
    guard.code_hash = 0;
#else
    void* ptr = nullptr;
    size_t map_size = alloc_size;

    if (use_guard_pages) {
        // v0.7.1: Guard pages — allocate guard + data + guard
        map_size = alloc_size + 2 * page_sz;
        
        // ASLR: random hint address
        void* hint = random_mmap_hint();
        void* base = mmap(hint, map_size, PROT_NONE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) {
            // Retry without hint
            base = mmap(nullptr, map_size, PROT_NONE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }
        if (base == MAP_FAILED) {
            g_wildx_total_allocated.fetch_sub(alloc_size);
            return empty;
        }
        // Make data pages RW (between guard pages)
        ptr = static_cast<char*>(base) + page_sz;
        if (mprotect(ptr, alloc_size, PROT_READ | PROT_WRITE) != 0) {
            munmap(base, map_size);
            g_wildx_total_allocated.fetch_sub(alloc_size);
            return empty;
        }
    } else {
        // Standard path: ASLR hint + mmap RW
        void* hint = random_mmap_hint();
        ptr = mmap(hint, alloc_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) {
            // Retry without hint
            ptr = mmap(nullptr, alloc_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }
        if (ptr == MAP_FAILED) {
            g_wildx_total_allocated.fetch_sub(alloc_size);
            return empty;
        }
    }
#endif

    g_wildx_num_allocations.fetch_add(1);
    update_wildx_peak();

    WildXGuard guard;
    guard.ptr = ptr;
    guard.size = alloc_size;
    guard.map_size = map_size;
    guard.state = WILDX_STATE_WRITABLE;
    guard.sealed = false;
    guard.code_hash = 0;

    wildx_audit_log("ALLOC", ptr, alloc_size);
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

    // v0.7.1: Compute code hash BEFORE sealing (integrity baseline)
    guard->code_hash = fnv1a_hash(guard->ptr, guard->size);

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

    wildx_audit_log("SEAL", guard->ptr, guard->size, guard->code_hash);
    return 0;  // Success
}

void aria_free_exec(WildXGuard* guard) {
    if (!guard || !guard->ptr) {
        return;  // NULL guard is no-op
    }

    wildx_audit_log("FREE", guard->ptr, guard->size, guard->code_hash);

    // Deallocate memory
#ifdef _WIN32
    VirtualFree(guard->ptr, 0, MEM_RELEASE);
#else
    if (guard->map_size > guard->size) {
        // Guard page mode: munmap from base (ptr - page_size)
        size_t page_sz = get_page_size();
        void* base = static_cast<char*>(guard->ptr) - page_sz;
        munmap(base, guard->map_size);
    } else {
        munmap(guard->ptr, guard->size);
    }
#endif

    // Update statistics
    g_wildx_total_allocated.fetch_sub(guard->size);
    g_wildx_num_allocations.fetch_sub(1);
    g_wildx_total_frees.fetch_add(1);

    // Reset guard to freed state
    guard->ptr = nullptr;
    guard->size = 0;
    guard->map_size = 0;
    guard->state = WILDX_STATE_FREED;
    guard->sealed = false;
    guard->code_hash = 0;
}

void* aria_exec_jit(WildXGuard* guard, void* args) {
    if (!guard || !guard->ptr) {
        return nullptr;  // Invalid guard
    }

    if (guard->state != WILDX_STATE_EXECUTABLE) {
        return nullptr;  // Not sealed yet
    }

    // v0.7.1: Code hash verification — detect post-seal tampering
    if (guard->code_hash != 0) {
        uint64_t current_hash = fnv1a_hash(guard->ptr, guard->size);
        if (current_hash != guard->code_hash) {
            wildx_audit_log("TAMPER!", guard->ptr, guard->size, current_hash);
            std::fprintf(stderr, "[WildX] FATAL: Code integrity check failed! "
                        "Expected hash 0x%016llx, got 0x%016llx\n",
                        (unsigned long long)guard->code_hash,
                        (unsigned long long)current_hash);
            return nullptr;  // Refuse to execute tampered code
        }
    }

    wildx_audit_log("EXEC", guard->ptr, guard->size, guard->code_hash);

    // Cast to function pointer and execute
    typedef void* (*jit_func_t)(void*);
    jit_func_t func = reinterpret_cast<jit_func_t>(guard->ptr);
    
    return func(args);
}

// =============================================================================
// v0.7.1: Configuration API
// =============================================================================

void aria_wildx_enable_guard_pages(bool enable) {
    g_wildx_guard_pages_enabled.store(enable, std::memory_order_relaxed);
}

void aria_wildx_enable_audit(bool enable) {
    g_wildx_audit_enabled.store(enable, std::memory_order_relaxed);
}

void aria_wildx_set_quota(size_t bytes) {
    g_wildx_quota.store(bytes, std::memory_order_relaxed);
}

uint64_t aria_wildx_verify_hash(WildXGuard* guard) {
    if (!guard || !guard->ptr || guard->state == WILDX_STATE_FREED) {
        return 0;
    }
    return fnv1a_hash(guard->ptr, guard->size);
}

// Note: aria_allocator_get_stats() is implemented in wild_alloc.cpp
// This file only provides the WildX statistics via global atomics

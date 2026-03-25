/**
 * Aria Atomic Shim — Handle-Based API for Aria Stdlib
 *
 * Wraps the pointer-based atomic runtime with int64 handles that
 * Aria extern blocks can consume directly. Avoids the {i1, ptr}
 * optional wrapper corruption that occurs with wild ?* returns
 * stored in struct fields.
 *
 * Same handle pool pattern as thread_shim.cpp.
 * All functions prefixed with aria_shim_atomic_* to distinguish
 * from the underlying runtime API.
 */

#include "runtime/atomic.h"
#include <cstdint>
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Handle Pools
// ============================================================================

#define MAX_ATOMIC_INT32  256
#define MAX_ATOMIC_INT64  256
#define MAX_ATOMIC_BOOL   128

static AriaAtomicInt32* g_atomic_int32[MAX_ATOMIC_INT32];
static AriaAtomicInt64* g_atomic_int64[MAX_ATOMIC_INT64];
static AriaAtomicBool*  g_atomic_bool[MAX_ATOMIC_BOOL];

static int64_t alloc_atomic_int32_slot(AriaAtomicInt32* a) {
    for (int i = 0; i < MAX_ATOMIC_INT32; i++) {
        if (!g_atomic_int32[i]) { g_atomic_int32[i] = a; return (int64_t)i; }
    }
    return -1;
}

static int64_t alloc_atomic_int64_slot(AriaAtomicInt64* a) {
    for (int i = 0; i < MAX_ATOMIC_INT64; i++) {
        if (!g_atomic_int64[i]) { g_atomic_int64[i] = a; return (int64_t)i; }
    }
    return -1;
}

static int64_t alloc_atomic_bool_slot(AriaAtomicBool* a) {
    for (int i = 0; i < MAX_ATOMIC_BOOL; i++) {
        if (!g_atomic_bool[i]) { g_atomic_bool[i] = a; return (int64_t)i; }
    }
    return -1;
}

// ============================================================================
// AtomicInt32 Shim Functions
// ============================================================================

int64_t aria_shim_atomic_int32_create(int32_t initial) {
    AriaAtomicInt32* a = aria_atomic_int32_create(initial);
    if (!a) return -1;
    return alloc_atomic_int32_slot(a);
}

int32_t aria_shim_atomic_int32_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_ATOMIC_INT32 || !g_atomic_int32[handle]) return -1;
    aria_atomic_int32_destroy(g_atomic_int32[handle]);
    g_atomic_int32[handle] = nullptr;
    return 0;
}

int32_t aria_shim_atomic_int32_load(int64_t handle, int32_t order) {
    if (handle < 0 || handle >= MAX_ATOMIC_INT32 || !g_atomic_int32[handle]) return 0;
    return aria_atomic_int32_load(g_atomic_int32[handle], (AriaMemoryOrder)order);
}

int32_t aria_shim_atomic_int32_store(int64_t handle, int32_t value, int32_t order) {
    if (handle < 0 || handle >= MAX_ATOMIC_INT32 || !g_atomic_int32[handle]) return -1;
    aria_atomic_int32_store(g_atomic_int32[handle], value, (AriaMemoryOrder)order);
    return 0;
}

int32_t aria_shim_atomic_int32_fetch_add(int64_t handle, int32_t value, int32_t order) {
    if (handle < 0 || handle >= MAX_ATOMIC_INT32 || !g_atomic_int32[handle]) return 0;
    return aria_atomic_int32_fetch_add(g_atomic_int32[handle], value, (AriaMemoryOrder)order);
}

int32_t aria_shim_atomic_int32_fetch_sub(int64_t handle, int32_t value, int32_t order) {
    if (handle < 0 || handle >= MAX_ATOMIC_INT32 || !g_atomic_int32[handle]) return 0;
    return aria_atomic_int32_fetch_sub(g_atomic_int32[handle], value, (AriaMemoryOrder)order);
}

int32_t aria_shim_atomic_int32_exchange(int64_t handle, int32_t value, int32_t order) {
    if (handle < 0 || handle >= MAX_ATOMIC_INT32 || !g_atomic_int32[handle]) return 0;
    return aria_atomic_int32_exchange(g_atomic_int32[handle], value, (AriaMemoryOrder)order);
}

// ============================================================================
// AtomicInt64 Shim Functions
// ============================================================================

int64_t aria_shim_atomic_int64_create(int64_t initial) {
    AriaAtomicInt64* a = aria_atomic_int64_create(initial);
    if (!a) return -1;
    return alloc_atomic_int64_slot(a);
}

int32_t aria_shim_atomic_int64_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_ATOMIC_INT64 || !g_atomic_int64[handle]) return -1;
    aria_atomic_int64_destroy(g_atomic_int64[handle]);
    g_atomic_int64[handle] = nullptr;
    return 0;
}

int64_t aria_shim_atomic_int64_load(int64_t handle, int32_t order) {
    if (handle < 0 || handle >= MAX_ATOMIC_INT64 || !g_atomic_int64[handle]) return 0;
    return aria_atomic_int64_load(g_atomic_int64[handle], (AriaMemoryOrder)order);
}

int32_t aria_shim_atomic_int64_store(int64_t handle, int64_t value, int32_t order) {
    if (handle < 0 || handle >= MAX_ATOMIC_INT64 || !g_atomic_int64[handle]) return -1;
    aria_atomic_int64_store(g_atomic_int64[handle], value, (AriaMemoryOrder)order);
    return 0;
}

int64_t aria_shim_atomic_int64_fetch_add(int64_t handle, int64_t value, int32_t order) {
    if (handle < 0 || handle >= MAX_ATOMIC_INT64 || !g_atomic_int64[handle]) return 0;
    return aria_atomic_int64_fetch_add(g_atomic_int64[handle], value, (AriaMemoryOrder)order);
}

int64_t aria_shim_atomic_int64_fetch_sub(int64_t handle, int64_t value, int32_t order) {
    if (handle < 0 || handle >= MAX_ATOMIC_INT64 || !g_atomic_int64[handle]) return 0;
    return aria_atomic_int64_fetch_sub(g_atomic_int64[handle], value, (AriaMemoryOrder)order);
}

int64_t aria_shim_atomic_int64_exchange(int64_t handle, int64_t value, int32_t order) {
    if (handle < 0 || handle >= MAX_ATOMIC_INT64 || !g_atomic_int64[handle]) return 0;
    return aria_atomic_int64_exchange(g_atomic_int64[handle], value, (AriaMemoryOrder)order);
}

// ============================================================================
// AtomicBool Shim Functions
// ============================================================================

int64_t aria_shim_atomic_bool_create(int32_t initial) {
    AriaAtomicBool* a = aria_atomic_bool_create(initial != 0);
    if (!a) return -1;
    return alloc_atomic_bool_slot(a);
}

int32_t aria_shim_atomic_bool_destroy(int64_t handle) {
    if (handle < 0 || handle >= MAX_ATOMIC_BOOL || !g_atomic_bool[handle]) return -1;
    aria_atomic_bool_destroy(g_atomic_bool[handle]);
    g_atomic_bool[handle] = nullptr;
    return 0;
}

int32_t aria_shim_atomic_bool_load(int64_t handle, int32_t order) {
    if (handle < 0 || handle >= MAX_ATOMIC_BOOL || !g_atomic_bool[handle]) return 0;
    return aria_atomic_bool_load(g_atomic_bool[handle], (AriaMemoryOrder)order) ? 1 : 0;
}

int32_t aria_shim_atomic_bool_store(int64_t handle, int32_t value, int32_t order) {
    if (handle < 0 || handle >= MAX_ATOMIC_BOOL || !g_atomic_bool[handle]) return -1;
    aria_atomic_bool_store(g_atomic_bool[handle], value != 0, (AriaMemoryOrder)order);
    return 0;
}

int32_t aria_shim_atomic_bool_exchange(int64_t handle, int32_t value, int32_t order) {
    if (handle < 0 || handle >= MAX_ATOMIC_BOOL || !g_atomic_bool[handle]) return 0;
    return aria_atomic_bool_exchange(g_atomic_bool[handle], value != 0, (AriaMemoryOrder)order) ? 1 : 0;
}

// ============================================================================
// Fence & Lock-free Query Shims (pass-through, no handles needed)
// ============================================================================

void aria_shim_atomic_thread_fence(int32_t order) {
    aria_atomic_thread_fence((AriaMemoryOrder)order);
}

void aria_shim_atomic_signal_fence(int32_t order) {
    aria_atomic_signal_fence((AriaMemoryOrder)order);
}

int32_t aria_shim_atomic_is_lock_free_int32(void) {
    return aria_atomic_is_lock_free_int32() ? 1 : 0;
}

int32_t aria_shim_atomic_is_lock_free_int64(void) {
    return aria_atomic_is_lock_free_int64() ? 1 : 0;
}

int32_t aria_shim_atomic_is_lock_free_bool(void) {
    return aria_atomic_is_lock_free_bool() ? 1 : 0;
}

#ifdef __cplusplus
}
#endif

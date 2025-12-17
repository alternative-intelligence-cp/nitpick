/**
 * Aria Atomics Library Implementation
 * 
 * Provides lock-free atomic operations using C++11 atomics.
 * Special handling for TBB types with sticky error propagation via CAS loops.
 */

#include "runtime/atomic.h"
#include <cstdlib>
#include <cstring>
#include <atomic>

/* ============================================================================
 * Atomic Type Definitions (C++11 atomics)
 * ============================================================================ */

struct AriaAtomicBool { std::atomic<bool> value; };
struct AriaAtomicInt8 { std::atomic<int8_t> value; };
struct AriaAtomicUint8 { std::atomic<uint8_t> value; };
struct AriaAtomicInt16 { std::atomic<int16_t> value; };
struct AriaAtomicUint16 { std::atomic<uint16_t> value; };
struct AriaAtomicInt32 { std::atomic<int32_t> value; };
struct AriaAtomicUint32 { std::atomic<uint32_t> value; };
struct AriaAtomicInt64 { std::atomic<int64_t> value; };
struct AriaAtomicUint64 { std::atomic<uint64_t> value; };
struct AriaAtomicPtr { std::atomic<void*> value; };

struct AriaAtomicTBB8 { std::atomic<int8_t> value; };
struct AriaAtomicTBB16 { std::atomic<int16_t> value; };
struct AriaAtomicTBB32 { std::atomic<int32_t> value; };
struct AriaAtomicTBB64 { std::atomic<int64_t> value; };

/* ============================================================================
 * Helper: Convert Aria memory order to C++11 memory order
 * ============================================================================ */

static inline std::memory_order aria_to_cpp_order(AriaMemoryOrder order) {
    switch (order) {
        case ARIA_MEMORY_ORDER_RELAXED: return std::memory_order_relaxed;
        case ARIA_MEMORY_ORDER_ACQUIRE: return std::memory_order_acquire;
        case ARIA_MEMORY_ORDER_RELEASE: return std::memory_order_release;
        case ARIA_MEMORY_ORDER_ACQ_REL: return std::memory_order_acq_rel;
        case ARIA_MEMORY_ORDER_SEQ_CST: return std::memory_order_seq_cst;
        default: return std::memory_order_seq_cst;
    }
}

/* ============================================================================
 * TBB Arithmetic Helpers (Sticky Error Propagation)
 * ============================================================================ */

static inline int8_t tbb8_add(int8_t a, int8_t b) {
    if (a == ARIA_TBB8_ERR || b == ARIA_TBB8_ERR) return ARIA_TBB8_ERR;
    int16_t result = (int16_t)a + (int16_t)b;
    if (result > ARIA_TBB8_MAX || result < ARIA_TBB8_MIN) return ARIA_TBB8_ERR;
    return (int8_t)result;
}

static inline int8_t tbb8_sub(int8_t a, int8_t b) {
    if (a == ARIA_TBB8_ERR || b == ARIA_TBB8_ERR) return ARIA_TBB8_ERR;
    int16_t result = (int16_t)a - (int16_t)b;
    if (result > ARIA_TBB8_MAX || result < ARIA_TBB8_MIN) return ARIA_TBB8_ERR;
    return (int8_t)result;
}

static inline int32_t tbb32_add(int32_t a, int32_t b) {
    if (a == ARIA_TBB32_ERR || b == ARIA_TBB32_ERR) return ARIA_TBB32_ERR;
    int64_t result = (int64_t)a + (int64_t)b;
    if (result > ARIA_TBB32_MAX || result < ARIA_TBB32_MIN) return ARIA_TBB32_ERR;
    return (int32_t)result;
}

static inline int32_t tbb32_sub(int32_t a, int32_t b) {
    if (a == ARIA_TBB32_ERR || b == ARIA_TBB32_ERR) return ARIA_TBB32_ERR;
    int64_t result = (int64_t)a - (int64_t)b;
    if (result > ARIA_TBB32_MAX || result < ARIA_TBB32_MIN) return ARIA_TBB32_ERR;
    return (int32_t)result;
}

static inline int64_t tbb64_add(int64_t a, int64_t b) {
    if (a == ARIA_TBB64_ERR || b == ARIA_TBB64_ERR) return ARIA_TBB64_ERR;
    
    // Check for overflow using compiler builtins if available
    #if defined(__has_builtin) && __has_builtin(__builtin_add_overflow)
        int64_t result;
        if (__builtin_add_overflow(a, b, &result)) return ARIA_TBB64_ERR;
        if (result == ARIA_TBB64_ERR) return ARIA_TBB64_ERR;  // Overflow to ERR sentinel
        return result;
    #else
        // Fallback: manual overflow check
        if (b > 0 && a > ARIA_TBB64_MAX - b) return ARIA_TBB64_ERR;
        if (b < 0 && a < ARIA_TBB64_MIN - b) return ARIA_TBB64_ERR;
        int64_t result = a + b;
        if (result == ARIA_TBB64_ERR) return ARIA_TBB64_ERR;
        return result;
    #endif
}

static inline int64_t tbb64_sub(int64_t a, int64_t b) {
    if (a == ARIA_TBB64_ERR || b == ARIA_TBB64_ERR) return ARIA_TBB64_ERR;
    
    #if defined(__has_builtin) && __has_builtin(__builtin_sub_overflow)
        int64_t result;
        if (__builtin_sub_overflow(a, b, &result)) return ARIA_TBB64_ERR;
        if (result == ARIA_TBB64_ERR) return ARIA_TBB64_ERR;
        return result;
    #else
        if (b < 0 && a > ARIA_TBB64_MAX + b) return ARIA_TBB64_ERR;
        if (b > 0 && a < ARIA_TBB64_MIN + b) return ARIA_TBB64_ERR;
        int64_t result = a - b;
        if (result == ARIA_TBB64_ERR) return ARIA_TBB64_ERR;
        return result;
    #endif
}

/* ============================================================================
 * Atomic Boolean Operations
 * ============================================================================ */

AriaAtomicBool* aria_atomic_bool_create(bool initial_value) {
    AriaAtomicBool* atomic = (AriaAtomicBool*)malloc(sizeof(AriaAtomicBool));
    if (!atomic) return NULL;
    atomic->value.store(initial_value);
    return atomic;
}

void aria_atomic_bool_destroy(AriaAtomicBool* atomic) {
    free(atomic);
}

bool aria_atomic_bool_load(AriaAtomicBool* atomic, AriaMemoryOrder order) {
    return atomic->value.load(aria_to_cpp_order(order));
}

void aria_atomic_bool_store(AriaAtomicBool* atomic, bool value, AriaMemoryOrder order) {
    atomic->value.store(value, aria_to_cpp_order(order));
}

bool aria_atomic_bool_exchange(AriaAtomicBool* atomic, bool value, AriaMemoryOrder order) {
    return atomic->value.exchange(value, aria_to_cpp_order(order));
}

bool aria_atomic_bool_compare_exchange_strong(AriaAtomicBool* atomic, bool* expected,
                                               bool desired, AriaMemoryOrder success_order,
                                               AriaMemoryOrder failure_order) {
    return atomic->value.compare_exchange_strong(*expected, desired, aria_to_cpp_order(success_order), aria_to_cpp_order(failure_order));
}

bool aria_atomic_bool_compare_exchange_weak(AriaAtomicBool* atomic, bool* expected,
                                             bool desired, AriaMemoryOrder success_order,
                                             AriaMemoryOrder failure_order) {
    return atomic_compare_exchange_weak_explicit(&atomic->value, expected, desired,
                                                  aria_to_cpp_order(success_order),
                                                  aria_to_cpp_order(failure_order));
}

/* ============================================================================
 * Atomic Int8 Operations
 * ============================================================================ */

AriaAtomicInt8* aria_atomic_int8_create(int8_t initial_value) {
    AriaAtomicInt8* atomic = (AriaAtomicInt8*)malloc(sizeof(AriaAtomicInt8));
    if (!atomic) return NULL;
    atomic->value.store(initial_value);
    return atomic;
}

void aria_atomic_int8_destroy(AriaAtomicInt8* atomic) {
    free(atomic);
}

int8_t aria_atomic_int8_load(AriaAtomicInt8* atomic, AriaMemoryOrder order) {
    return atomic->value.load(aria_to_cpp_order(order));
}

void aria_atomic_int8_store(AriaAtomicInt8* atomic, int8_t value, AriaMemoryOrder order) {
    atomic->value.store(value, aria_to_cpp_order(order));
}

int8_t aria_atomic_int8_exchange(AriaAtomicInt8* atomic, int8_t value, AriaMemoryOrder order) {
    return atomic->value.exchange(value, aria_to_cpp_order(order));
}

bool aria_atomic_int8_compare_exchange_strong(AriaAtomicInt8* atomic, int8_t* expected,
                                               int8_t desired, AriaMemoryOrder success_order,
                                               AriaMemoryOrder failure_order) {
    return atomic->value.compare_exchange_strong(*expected, desired, aria_to_cpp_order(success_order), aria_to_cpp_order(failure_order));
}

int8_t aria_atomic_int8_fetch_add(AriaAtomicInt8* atomic, int8_t value, AriaMemoryOrder order) {
    return atomic->value.fetch_add(value, aria_to_cpp_order(order));
}

int8_t aria_atomic_int8_fetch_sub(AriaAtomicInt8* atomic, int8_t value, AriaMemoryOrder order) {
    return atomic->value.fetch_sub(value, aria_to_cpp_order(order));
}

/* ============================================================================
 * Atomic Uint8 Operations
 * ============================================================================ */

AriaAtomicUint8* aria_atomic_uint8_create(uint8_t initial_value) {
    AriaAtomicUint8* atomic = (AriaAtomicUint8*)malloc(sizeof(AriaAtomicUint8));
    if (!atomic) return NULL;
    atomic->value.store(initial_value);
    return atomic;
}

void aria_atomic_uint8_destroy(AriaAtomicUint8* atomic) {
    free(atomic);
}

uint8_t aria_atomic_uint8_load(AriaAtomicUint8* atomic, AriaMemoryOrder order) {
    return atomic->value.load(aria_to_cpp_order(order));
}

void aria_atomic_uint8_store(AriaAtomicUint8* atomic, uint8_t value, AriaMemoryOrder order) {
    atomic->value.store(value, aria_to_cpp_order(order));
}

uint8_t aria_atomic_uint8_fetch_add(AriaAtomicUint8* atomic, uint8_t value, AriaMemoryOrder order) {
    return atomic->value.fetch_add(value, aria_to_cpp_order(order));
}

/* ============================================================================
 * Atomic Int32 Operations
 * ============================================================================ */

AriaAtomicInt32* aria_atomic_int32_create(int32_t initial_value) {
    AriaAtomicInt32* atomic = (AriaAtomicInt32*)malloc(sizeof(AriaAtomicInt32));
    if (!atomic) return NULL;
    atomic->value.store(initial_value);
    return atomic;
}

void aria_atomic_int32_destroy(AriaAtomicInt32* atomic) {
    free(atomic);
}

int32_t aria_atomic_int32_load(AriaAtomicInt32* atomic, AriaMemoryOrder order) {
    return atomic->value.load(aria_to_cpp_order(order));
}

void aria_atomic_int32_store(AriaAtomicInt32* atomic, int32_t value, AriaMemoryOrder order) {
    atomic->value.store(value, aria_to_cpp_order(order));
}

int32_t aria_atomic_int32_exchange(AriaAtomicInt32* atomic, int32_t value, AriaMemoryOrder order) {
    return atomic->value.exchange(aria_to_cpp_order(order));
}

bool aria_atomic_int32_compare_exchange_strong(AriaAtomicInt32* atomic, int32_t* expected,
                                                int32_t desired, AriaMemoryOrder success_order,
                                                AriaMemoryOrder failure_order) {
    return atomic->value.compare_exchange_strong(*expected, desired, aria_to_cpp_order(success_order), aria_to_cpp_order(failure_order));
}

int32_t aria_atomic_int32_fetch_add(AriaAtomicInt32* atomic, int32_t value, AriaMemoryOrder order) {
    return atomic->value.fetch_add(value, aria_to_cpp_order(order));
}

int32_t aria_atomic_int32_fetch_sub(AriaAtomicInt32* atomic, int32_t value, AriaMemoryOrder order) {
    return atomic->value.fetch_sub(value, aria_to_cpp_order(order));
}

/* ============================================================================
 * Atomic Uint32 Operations
 * ============================================================================ */

AriaAtomicUint32* aria_atomic_uint32_create(uint32_t initial_value) {
    AriaAtomicUint32* atomic = (AriaAtomicUint32*)malloc(sizeof(AriaAtomicUint32));
    if (!atomic) return NULL;
    atomic->value.store(initial_value);
    return atomic;
}

void aria_atomic_uint32_destroy(AriaAtomicUint32* atomic) {
    free(atomic);
}

uint32_t aria_atomic_uint32_load(AriaAtomicUint32* atomic, AriaMemoryOrder order) {
    return atomic->value.load(aria_to_cpp_order(order));
}

void aria_atomic_uint32_store(AriaAtomicUint32* atomic, uint32_t value, AriaMemoryOrder order) {
    atomic->value.store(value, aria_to_cpp_order(order));
}

uint32_t aria_atomic_uint32_fetch_add(AriaAtomicUint32* atomic, uint32_t value, AriaMemoryOrder order) {
    return atomic->value.fetch_add(value, aria_to_cpp_order(order));
}

/* ============================================================================
 * Atomic Int64 Operations
 * ============================================================================ */

AriaAtomicInt64* aria_atomic_int64_create(int64_t initial_value) {
    AriaAtomicInt64* atomic = (AriaAtomicInt64*)malloc(sizeof(AriaAtomicInt64));
    if (!atomic) return NULL;
    atomic->value.store(initial_value);
    return atomic;
}

void aria_atomic_int64_destroy(AriaAtomicInt64* atomic) {
    free(atomic);
}

int64_t aria_atomic_int64_load(AriaAtomicInt64* atomic, AriaMemoryOrder order) {
    return atomic->value.load(aria_to_cpp_order(order));
}

void aria_atomic_int64_store(AriaAtomicInt64* atomic, int64_t value, AriaMemoryOrder order) {
    atomic->value.store(value, aria_to_cpp_order(order));
}

int64_t aria_atomic_int64_exchange(AriaAtomicInt64* atomic, int64_t value, AriaMemoryOrder order) {
    return atomic->value.exchange(value, aria_to_cpp_order(order));
}

bool aria_atomic_int64_compare_exchange_strong(AriaAtomicInt64* atomic, int64_t* expected,
                                                int64_t desired, AriaMemoryOrder success_order,
                                                AriaMemoryOrder failure_order) {
    return atomic->value.compare_exchange_strong(*expected, desired, aria_to_cpp_order(success_order), aria_to_cpp_order(failure_order));
}

int64_t aria_atomic_int64_fetch_add(AriaAtomicInt64* atomic, int64_t value, AriaMemoryOrder order) {
    return atomic->value.fetch_add(value, aria_to_cpp_order(order));
}

int64_t aria_atomic_int64_fetch_sub(AriaAtomicInt64* atomic, int64_t value, AriaMemoryOrder order) {
    return atomic->value.fetch_sub(value, aria_to_cpp_order(order));
}

/* ============================================================================
 * Atomic Uint64 Operations
 * ============================================================================ */

AriaAtomicUint64* aria_atomic_uint64_create(uint64_t initial_value) {
    AriaAtomicUint64* atomic = (AriaAtomicUint64*)malloc(sizeof(AriaAtomicUint64));
    if (!atomic) return NULL;
    atomic->value.store(initial_value);
    return atomic;
}

void aria_atomic_uint64_destroy(AriaAtomicUint64* atomic) {
    free(atomic);
}

uint64_t aria_atomic_uint64_load(AriaAtomicUint64* atomic, AriaMemoryOrder order) {
    return atomic->value.load(aria_to_cpp_order(order));
}

void aria_atomic_uint64_store(AriaAtomicUint64* atomic, uint64_t value, AriaMemoryOrder order) {
    atomic->value.store(value, aria_to_cpp_order(order));
}

uint64_t aria_atomic_uint64_fetch_add(AriaAtomicUint64* atomic, uint64_t value, AriaMemoryOrder order) {
    return atomic->value.fetch_add(value, aria_to_cpp_order(order));
}

/* ============================================================================
 * Atomic Pointer Operations
 * ============================================================================ */

AriaAtomicPtr* aria_atomic_ptr_create(void* initial_value) {
    AriaAtomicPtr* atomic = (AriaAtomicPtr*)malloc(sizeof(AriaAtomicPtr));
    if (!atomic) return NULL;
    atomic->value.store(initial_value);
    return atomic;
}

void aria_atomic_ptr_destroy(AriaAtomicPtr* atomic) {
    free(atomic);
}

void* aria_atomic_ptr_load(AriaAtomicPtr* atomic, AriaMemoryOrder order) {
    return atomic->value.load(aria_to_cpp_order(order));
}

void aria_atomic_ptr_store(AriaAtomicPtr* atomic, void* value, AriaMemoryOrder order) {
    atomic->value.store(value, aria_to_cpp_order(order));
}

void* aria_atomic_ptr_exchange(AriaAtomicPtr* atomic, void* value, AriaMemoryOrder order) {
    return atomic->value.exchange(value, aria_to_cpp_order(order));
}

bool aria_atomic_ptr_compare_exchange_strong(AriaAtomicPtr* atomic, void** expected,
                                              void* desired, AriaMemoryOrder success_order,
                                              AriaMemoryOrder failure_order) {
    return atomic->value.compare_exchange_strong(*expected, desired, aria_to_cpp_order(success_order), aria_to_cpp_order(failure_order));
}

/* ============================================================================
 * Atomic TBB8 Operations (With Sticky Error Propagation)
 * ============================================================================ */

AriaAtomicTBB8* aria_atomic_tbb8_create(int8_t initial_value) {
    AriaAtomicTBB8* atomic = (AriaAtomicTBB8*)malloc(sizeof(AriaAtomicTBB8));
    if (!atomic) return NULL;
    atomic->value.store(initial_value);
    return atomic;
}

void aria_atomic_tbb8_destroy(AriaAtomicTBB8* atomic) {
    free(atomic);
}

int8_t aria_atomic_tbb8_load(AriaAtomicTBB8* atomic, AriaMemoryOrder order) {
    return atomic->value.load(aria_to_cpp_order(order));
}

void aria_atomic_tbb8_store(AriaAtomicTBB8* atomic, int8_t value, AriaMemoryOrder order) {
    atomic->value.store(value, aria_to_cpp_order(order));
}

int8_t aria_atomic_tbb8_exchange(AriaAtomicTBB8* atomic, int8_t value, AriaMemoryOrder order) {
    return atomic->value.exchange(value, aria_to_cpp_order(order));
}

bool aria_atomic_tbb8_compare_exchange_strong(AriaAtomicTBB8* atomic, int8_t* expected,
                                               int8_t desired, AriaMemoryOrder success_order,
                                               AriaMemoryOrder failure_order) {
    return atomic->value.compare_exchange_strong(*expected, desired, aria_to_cpp_order(success_order), aria_to_cpp_order(failure_order));
}

int8_t aria_atomic_tbb8_fetch_add(AriaAtomicTBB8* atomic, int8_t value, AriaMemoryOrder order) {
    std::memory_order mo = aria_to_cpp_order(order);
    int8_t old_val = atomic->value.load(mo);
    int8_t new_val;
    
    // CAS loop for sticky error propagation
    do {
        new_val = tbb8_add(old_val, value);
    } while (!atomic->value.compare_exchange_weak(old_val, new_val, mo));
    
    return old_val;
}

int8_t aria_atomic_tbb8_fetch_sub(AriaAtomicTBB8* atomic, int8_t value, AriaMemoryOrder order) {
    std::memory_order mo = aria_to_cpp_order(order);
    int8_t old_val = atomic->value.load(mo);
    int8_t new_val;
    
    // CAS loop for sticky error propagation
    do {
        new_val = tbb8_sub(old_val, value);
    } while (!atomic->value.compare_exchange_weak(old_val, new_val, mo));
    
    return old_val;
}

/* ============================================================================
 * Atomic TBB32 Operations (With Sticky Error Propagation)
 * ============================================================================ */

AriaAtomicTBB32* aria_atomic_tbb32_create(int32_t initial_value) {
    AriaAtomicTBB32* atomic = (AriaAtomicTBB32*)malloc(sizeof(AriaAtomicTBB32));
    if (!atomic) return NULL;
    atomic->value.store(initial_value);
    return atomic;
}

void aria_atomic_tbb32_destroy(AriaAtomicTBB32* atomic) {
    free(atomic);
}

int32_t aria_atomic_tbb32_load(AriaAtomicTBB32* atomic, AriaMemoryOrder order) {
    return atomic->value.load(aria_to_cpp_order(order));
}

void aria_atomic_tbb32_store(AriaAtomicTBB32* atomic, int32_t value, AriaMemoryOrder order) {
    atomic->value.store(value, aria_to_cpp_order(order));
}

int32_t aria_atomic_tbb32_exchange(AriaAtomicTBB32* atomic, int32_t value, AriaMemoryOrder order) {
    return atomic->value.exchange(value, aria_to_cpp_order(order));
}

bool aria_atomic_tbb32_compare_exchange_strong(AriaAtomicTBB32* atomic, int32_t* expected,
                                                int32_t desired, AriaMemoryOrder success_order,
                                                AriaMemoryOrder failure_order) {
    return atomic->value.compare_exchange_strong(*expected, desired, aria_to_cpp_order(success_order), aria_to_cpp_order(failure_order));
}

int32_t aria_atomic_tbb32_fetch_add(AriaAtomicTBB32* atomic, int32_t value, AriaMemoryOrder order) {
    std::memory_order mo = aria_to_cpp_order(order);
    int32_t old_val = atomic->value.load(mo);
    int32_t new_val;
    
    // CAS loop for sticky error propagation
    do {
        new_val = tbb32_add(old_val, value);
    } while (!atomic->value.compare_exchange_weak(old_val, new_val, mo));
    
    return old_val;
}

int32_t aria_atomic_tbb32_fetch_sub(AriaAtomicTBB32* atomic, int32_t value, AriaMemoryOrder order) {
    std::memory_order mo = aria_to_cpp_order(order);
    int32_t old_val = atomic->value.load(mo);
    int32_t new_val;
    
    // CAS loop for sticky error propagation
    do {
        new_val = tbb32_sub(old_val, value);
    } while (!atomic->value.compare_exchange_weak(old_val, new_val, mo));
    
    return old_val;
}

/* ============================================================================
 * Atomic TBB64 Operations (With Sticky Error Propagation)
 * ============================================================================ */

AriaAtomicTBB64* aria_atomic_tbb64_create(int64_t initial_value) {
    AriaAtomicTBB64* atomic = (AriaAtomicTBB64*)malloc(sizeof(AriaAtomicTBB64));
    if (!atomic) return NULL;
    atomic->value.store(initial_value);
    return atomic;
}

void aria_atomic_tbb64_destroy(AriaAtomicTBB64* atomic) {
    free(atomic);
}

int64_t aria_atomic_tbb64_load(AriaAtomicTBB64* atomic, AriaMemoryOrder order) {
    return atomic->value.load(aria_to_cpp_order(order));
}

void aria_atomic_tbb64_store(AriaAtomicTBB64* atomic, int64_t value, AriaMemoryOrder order) {
    atomic->value.store(value, aria_to_cpp_order(order));
}

int64_t aria_atomic_tbb64_exchange(AriaAtomicTBB64* atomic, int64_t value, AriaMemoryOrder order) {
    return atomic->value.exchange(value, aria_to_cpp_order(order));
}

bool aria_atomic_tbb64_compare_exchange_strong(AriaAtomicTBB64* atomic, int64_t* expected,
                                                int64_t desired, AriaMemoryOrder success_order,
                                                AriaMemoryOrder failure_order) {
    return atomic->value.compare_exchange_strong(*expected, desired, aria_to_cpp_order(success_order), aria_to_cpp_order(failure_order));
}

int64_t aria_atomic_tbb64_fetch_add(AriaAtomicTBB64* atomic, int64_t value, AriaMemoryOrder order) {
    std::memory_order mo = aria_to_cpp_order(order);
    int64_t old_val = atomic->value.load(mo);
    int64_t new_val;
    
    // CAS loop for sticky error propagation
    do {
        new_val = tbb64_add(old_val, value);
    } while (!atomic->value.compare_exchange_weak(old_val, new_val, mo));
    
    return old_val;
}

int64_t aria_atomic_tbb64_fetch_sub(AriaAtomicTBB64* atomic, int64_t value, AriaMemoryOrder order) {
    std::memory_order mo = aria_to_cpp_order(order);
    int64_t old_val = atomic->value.load(mo);
    int64_t new_val;
    
    // CAS loop for sticky error propagation
    do {
        new_val = tbb64_sub(old_val, value);
    } while (!atomic->value.compare_exchange_weak(old_val, new_val, mo));
    
    return old_val;
}

/* ============================================================================
 * Memory Fences
 * ============================================================================ */

void aria_atomic_thread_fence(AriaMemoryOrder order) {
    std::atomic_thread_fence(aria_to_cpp_order(order));
}

void aria_atomic_signal_fence(AriaMemoryOrder order) {
    std::atomic_signal_fence(aria_to_cpp_order(order));
}

/* ============================================================================
 * Lock-Free Property Queries
 * ============================================================================ */

bool aria_atomic_is_lock_free_bool(void) {
    return std::atomic<bool>{}.is_lock_free();
}

bool aria_atomic_is_lock_free_int8(void) {
    return std::atomic<int8_t>{}.is_lock_free();
}

bool aria_atomic_is_lock_free_int32(void) {
    return std::atomic<int32_t>{}.is_lock_free();
}

bool aria_atomic_is_lock_free_int64(void) {
    return std::atomic<int64_t>{}.is_lock_free();
}

bool aria_atomic_is_lock_free_ptr(void) {
    return std::atomic<void*>{}.is_lock_free();
}

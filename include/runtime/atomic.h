/**
 * Aria Atomics Library - Phase 5.8
 * 
 * Provides lock-free atomic operations with explicit memory ordering semantics.
 * Follows C++11/LLVM memory model for cross-platform compatibility.
 * 
 * Memory Model:
 * - Relaxed: No synchronization, only atomicity
 * - Acquire: Load barrier (no subsequent ops hoist above)
 * - Release: Store barrier (no prior ops sink below)
 * - AcqRel: Both acquire and release (for RMW operations)
 * - SeqCst: Sequential consistency (global total order)
 * 
 * Special Handling for TBB Types:
 * - TBB types use sticky error propagation (ERR sentinel)
 * - Hardware atomics don't support this, so we use CAS loops
 * - Standard types use efficient hardware instructions
 * 
 * Architecture Support:
 * - x86-64: Total Store Order (TSO) - many orderings are "free"
 * - ARMv8: Weak ordering - uses LDAR/STLR instructions
 */

#ifndef ARIA_RUNTIME_ATOMIC_H
#define ARIA_RUNTIME_ATOMIC_H

#include "runtime/io.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Memory Ordering
 * ============================================================================ */

/**
 * Memory ordering constraints for atomic operations.
 * These map directly to C11/C++11 memory_order semantics.
 */
typedef enum {
    ARIA_MEMORY_ORDER_RELAXED,  // No synchronization, only atomicity
    ARIA_MEMORY_ORDER_ACQUIRE,  // Load barrier (acquire semantics)
    ARIA_MEMORY_ORDER_RELEASE,  // Store barrier (release semantics)
    ARIA_MEMORY_ORDER_ACQ_REL,  // Both acquire and release (RMW ops)
    ARIA_MEMORY_ORDER_SEQ_CST   // Sequential consistency (strongest)
} AriaMemoryOrder;

/* ============================================================================
 * Atomic Types - Opaque Structures
 * Implementation details are hidden in the .c file
 * ============================================================================ */

typedef struct AriaAtomicBool AriaAtomicBool;
typedef struct AriaAtomicInt8 AriaAtomicInt8;
typedef struct AriaAtomicUint8 AriaAtomicUint8;
typedef struct AriaAtomicInt16 AriaAtomicInt16;
typedef struct AriaAtomicUint16 AriaAtomicUint16;
typedef struct AriaAtomicInt32 AriaAtomicInt32;
typedef struct AriaAtomicUint32 AriaAtomicUint32;
typedef struct AriaAtomicInt64 AriaAtomicInt64;
typedef struct AriaAtomicUint64 AriaAtomicUint64;
typedef struct AriaAtomicPtr AriaAtomicPtr;

/* ============================================================================
 * Atomic Types - TBB (Twisted Balanced Binary)
 * Special handling required for sticky error propagation
 * ============================================================================ */

typedef struct AriaAtomicTBB8 AriaAtomicTBB8;
typedef struct AriaAtomicTBB16 AriaAtomicTBB16;
typedef struct AriaAtomicTBB32 AriaAtomicTBB32;
typedef struct AriaAtomicTBB64 AriaAtomicTBB64;

/* TBB Error Sentinels */
#define ARIA_TBB8_ERR   ((int8_t)-128)
#define ARIA_TBB16_ERR  ((int16_t)-32768)
#define ARIA_TBB32_ERR  ((int32_t)-2147483648)
#define ARIA_TBB64_ERR  ((int64_t)-9223372036854775807LL - 1)

/* TBB Max Values */
#define ARIA_TBB8_MAX   ((int8_t)127)
#define ARIA_TBB8_MIN   ((int8_t)-127)
#define ARIA_TBB16_MAX  ((int16_t)32767)
#define ARIA_TBB16_MIN  ((int16_t)-32767)
#define ARIA_TBB32_MAX  ((int32_t)2147483647)
#define ARIA_TBB32_MIN  ((int32_t)-2147483647)
#define ARIA_TBB64_MAX  ((int64_t)9223372036854775807LL)
#define ARIA_TBB64_MIN  ((int64_t)-9223372036854775807LL)

/* ============================================================================
 * Atomic Boolean Operations
 * ============================================================================ */

AriaAtomicBool* aria_atomic_bool_create(bool initial_value);
void aria_atomic_bool_destroy(AriaAtomicBool* atomic);
bool aria_atomic_bool_load(AriaAtomicBool* atomic, AriaMemoryOrder order);
void aria_atomic_bool_store(AriaAtomicBool* atomic, bool value, AriaMemoryOrder order);
bool aria_atomic_bool_exchange(AriaAtomicBool* atomic, bool value, AriaMemoryOrder order);
bool aria_atomic_bool_compare_exchange_strong(AriaAtomicBool* atomic, bool* expected, 
                                               bool desired, AriaMemoryOrder success_order,
                                               AriaMemoryOrder failure_order);
bool aria_atomic_bool_compare_exchange_weak(AriaAtomicBool* atomic, bool* expected,
                                             bool desired, AriaMemoryOrder success_order,
                                             AriaMemoryOrder failure_order);

/* ============================================================================
 * Atomic Integer Operations (Standard Types)
 * ============================================================================ */

// Int8
AriaAtomicInt8* aria_atomic_int8_create(int8_t initial_value);
void aria_atomic_int8_destroy(AriaAtomicInt8* atomic);
int8_t aria_atomic_int8_load(AriaAtomicInt8* atomic, AriaMemoryOrder order);
void aria_atomic_int8_store(AriaAtomicInt8* atomic, int8_t value, AriaMemoryOrder order);
int8_t aria_atomic_int8_exchange(AriaAtomicInt8* atomic, int8_t value, AriaMemoryOrder order);
bool aria_atomic_int8_compare_exchange_strong(AriaAtomicInt8* atomic, int8_t* expected,
                                               int8_t desired, AriaMemoryOrder success_order,
                                               AriaMemoryOrder failure_order);
int8_t aria_atomic_int8_fetch_add(AriaAtomicInt8* atomic, int8_t value, AriaMemoryOrder order);
int8_t aria_atomic_int8_fetch_sub(AriaAtomicInt8* atomic, int8_t value, AriaMemoryOrder order);

// Uint8
AriaAtomicUint8* aria_atomic_uint8_create(uint8_t initial_value);
void aria_atomic_uint8_destroy(AriaAtomicUint8* atomic);
uint8_t aria_atomic_uint8_load(AriaAtomicUint8* atomic, AriaMemoryOrder order);
void aria_atomic_uint8_store(AriaAtomicUint8* atomic, uint8_t value, AriaMemoryOrder order);
uint8_t aria_atomic_uint8_fetch_add(AriaAtomicUint8* atomic, uint8_t value, AriaMemoryOrder order);

// Int32
AriaAtomicInt32* aria_atomic_int32_create(int32_t initial_value);
void aria_atomic_int32_destroy(AriaAtomicInt32* atomic);
int32_t aria_atomic_int32_load(AriaAtomicInt32* atomic, AriaMemoryOrder order);
void aria_atomic_int32_store(AriaAtomicInt32* atomic, int32_t value, AriaMemoryOrder order);
int32_t aria_atomic_int32_exchange(AriaAtomicInt32* atomic, int32_t value, AriaMemoryOrder order);
bool aria_atomic_int32_compare_exchange_strong(AriaAtomicInt32* atomic, int32_t* expected,
                                                int32_t desired, AriaMemoryOrder success_order,
                                                AriaMemoryOrder failure_order);
int32_t aria_atomic_int32_fetch_add(AriaAtomicInt32* atomic, int32_t value, AriaMemoryOrder order);
int32_t aria_atomic_int32_fetch_sub(AriaAtomicInt32* atomic, int32_t value, AriaMemoryOrder order);

// Uint32
AriaAtomicUint32* aria_atomic_uint32_create(uint32_t initial_value);
void aria_atomic_uint32_destroy(AriaAtomicUint32* atomic);
uint32_t aria_atomic_uint32_load(AriaAtomicUint32* atomic, AriaMemoryOrder order);
void aria_atomic_uint32_store(AriaAtomicUint32* atomic, uint32_t value, AriaMemoryOrder order);
uint32_t aria_atomic_uint32_fetch_add(AriaAtomicUint32* atomic, uint32_t value, AriaMemoryOrder order);

// Int64
AriaAtomicInt64* aria_atomic_int64_create(int64_t initial_value);
void aria_atomic_int64_destroy(AriaAtomicInt64* atomic);
int64_t aria_atomic_int64_load(AriaAtomicInt64* atomic, AriaMemoryOrder order);
void aria_atomic_int64_store(AriaAtomicInt64* atomic, int64_t value, AriaMemoryOrder order);
int64_t aria_atomic_int64_exchange(AriaAtomicInt64* atomic, int64_t value, AriaMemoryOrder order);
bool aria_atomic_int64_compare_exchange_strong(AriaAtomicInt64* atomic, int64_t* expected,
                                                int64_t desired, AriaMemoryOrder success_order,
                                                AriaMemoryOrder failure_order);
int64_t aria_atomic_int64_fetch_add(AriaAtomicInt64* atomic, int64_t value, AriaMemoryOrder order);
int64_t aria_atomic_int64_fetch_sub(AriaAtomicInt64* atomic, int64_t value, AriaMemoryOrder order);

// Uint64
AriaAtomicUint64* aria_atomic_uint64_create(uint64_t initial_value);
void aria_atomic_uint64_destroy(AriaAtomicUint64* atomic);
uint64_t aria_atomic_uint64_load(AriaAtomicUint64* atomic, AriaMemoryOrder order);
void aria_atomic_uint64_store(AriaAtomicUint64* atomic, uint64_t value, AriaMemoryOrder order);
uint64_t aria_atomic_uint64_fetch_add(AriaAtomicUint64* atomic, uint64_t value, AriaMemoryOrder order);

// Pointer
AriaAtomicPtr* aria_atomic_ptr_create(void* initial_value);
void aria_atomic_ptr_destroy(AriaAtomicPtr* atomic);
void* aria_atomic_ptr_load(AriaAtomicPtr* atomic, AriaMemoryOrder order);
void aria_atomic_ptr_store(AriaAtomicPtr* atomic, void* value, AriaMemoryOrder order);
void* aria_atomic_ptr_exchange(AriaAtomicPtr* atomic, void* value, AriaMemoryOrder order);
bool aria_atomic_ptr_compare_exchange_strong(AriaAtomicPtr* atomic, void** expected,
                                              void* desired, AriaMemoryOrder success_order,
                                              AriaMemoryOrder failure_order);

/* ============================================================================
 * Atomic TBB Operations (Sticky Error Propagation)
 * These use CAS loops to enforce TBB semantics
 * ============================================================================ */

// TBB8
AriaAtomicTBB8* aria_atomic_tbb8_create(int8_t initial_value);
void aria_atomic_tbb8_destroy(AriaAtomicTBB8* atomic);
int8_t aria_atomic_tbb8_load(AriaAtomicTBB8* atomic, AriaMemoryOrder order);
void aria_atomic_tbb8_store(AriaAtomicTBB8* atomic, int8_t value, AriaMemoryOrder order);
int8_t aria_atomic_tbb8_exchange(AriaAtomicTBB8* atomic, int8_t value, AriaMemoryOrder order);
bool aria_atomic_tbb8_compare_exchange_strong(AriaAtomicTBB8* atomic, int8_t* expected,
                                               int8_t desired, AriaMemoryOrder success_order,
                                               AriaMemoryOrder failure_order);
int8_t aria_atomic_tbb8_fetch_add(AriaAtomicTBB8* atomic, int8_t value, AriaMemoryOrder order);
int8_t aria_atomic_tbb8_fetch_sub(AriaAtomicTBB8* atomic, int8_t value, AriaMemoryOrder order);

// TBB32
AriaAtomicTBB32* aria_atomic_tbb32_create(int32_t initial_value);
void aria_atomic_tbb32_destroy(AriaAtomicTBB32* atomic);
int32_t aria_atomic_tbb32_load(AriaAtomicTBB32* atomic, AriaMemoryOrder order);
void aria_atomic_tbb32_store(AriaAtomicTBB32* atomic, int32_t value, AriaMemoryOrder order);
int32_t aria_atomic_tbb32_exchange(AriaAtomicTBB32* atomic, int32_t value, AriaMemoryOrder order);
bool aria_atomic_tbb32_compare_exchange_strong(AriaAtomicTBB32* atomic, int32_t* expected,
                                                int32_t desired, AriaMemoryOrder success_order,
                                                AriaMemoryOrder failure_order);
int32_t aria_atomic_tbb32_fetch_add(AriaAtomicTBB32* atomic, int32_t value, AriaMemoryOrder order);
int32_t aria_atomic_tbb32_fetch_sub(AriaAtomicTBB32* atomic, int32_t value, AriaMemoryOrder order);

// TBB64
AriaAtomicTBB64* aria_atomic_tbb64_create(int64_t initial_value);
void aria_atomic_tbb64_destroy(AriaAtomicTBB64* atomic);
int64_t aria_atomic_tbb64_load(AriaAtomicTBB64* atomic, AriaMemoryOrder order);
void aria_atomic_tbb64_store(AriaAtomicTBB64* atomic, int64_t value, AriaMemoryOrder order);
int64_t aria_atomic_tbb64_exchange(AriaAtomicTBB64* atomic, int64_t value, AriaMemoryOrder order);
bool aria_atomic_tbb64_compare_exchange_strong(AriaAtomicTBB64* atomic, int64_t* expected,
                                                int64_t desired, AriaMemoryOrder success_order,
                                                AriaMemoryOrder failure_order);
int64_t aria_atomic_tbb64_fetch_add(AriaAtomicTBB64* atomic, int64_t value, AriaMemoryOrder order);
int64_t aria_atomic_tbb64_fetch_sub(AriaAtomicTBB64* atomic, int64_t value, AriaMemoryOrder order);

/* ============================================================================
 * Memory Fences
 * ============================================================================ */

/**
 * Atomic thread fence (memory barrier).
 * Establishes synchronization ordering without an atomic operation.
 */
void aria_atomic_thread_fence(AriaMemoryOrder order);

/**
 * Atomic signal fence (compiler barrier only).
 * Prevents compiler reordering but does not affect hardware ordering.
 */
void aria_atomic_signal_fence(AriaMemoryOrder order);

/* ============================================================================
 * Lock-Free Property Queries
 * ============================================================================ */

/**
 * Check if atomic operations on a specific type are lock-free.
 * Returns true if the operation uses hardware instructions, false if it uses locks.
 */
bool aria_atomic_is_lock_free_bool(void);
bool aria_atomic_is_lock_free_int8(void);
bool aria_atomic_is_lock_free_int32(void);
bool aria_atomic_is_lock_free_int64(void);
bool aria_atomic_is_lock_free_ptr(void);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_ATOMIC_H

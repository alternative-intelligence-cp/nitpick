/**
 * Aria Threading Library - Phase 5.7
 * 
 * Provides cross-platform threading primitives for the Aria runtime.
 * This library wraps POSIX threads (pthread) on Unix/Linux/macOS and 
 * Windows threads on Windows, providing a unified C API.
 * 
 * Thread Model:
 * - 1:1 mapping: Each Aria thread maps to one OS thread
 * - Preemptive scheduling: OS handles thread scheduling
 * - Explicit synchronization: Mutexes, condition variables, etc.
 * 
 * Design Philosophy:
 * - Explicit over implicit (no hidden thread pools)
 * - Fail-fast with Result types (errors are not exceptions)
 * - Minimal overhead (thin wrapper over OS primitives)
 */

#ifndef ARIA_RUNTIME_THREAD_H
#define ARIA_RUNTIME_THREAD_H

#include "runtime/io.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Thread Handle and Information
 * ============================================================================ */

/**
 * Opaque thread handle.
 * Platform-specific implementation details are hidden.
 */
typedef struct AriaThread AriaThread;

/**
 * Thread identifier.
 * Portable representation of thread ID.
 */
typedef struct {
    uint64_t id;  // Platform-specific thread ID
} AriaThreadId;

/**
 * Thread function signature.
 * @param arg User-provided argument passed to thread
 * @return Return value (accessible via join)
 */
typedef void* (*AriaThreadFunc)(void* arg);

/**
 * Thread configuration options.
 */
typedef struct {
    size_t stack_size;      // Stack size in bytes (0 = use default)
    int priority;           // Thread priority (-1 = use default)
    bool detached;          // Create as detached thread
    const char* name;       // Thread name (optional, for debugging)
} AriaThreadOptions;

/* ============================================================================
 * Thread Lifecycle Management
 * ============================================================================ */

/**
 * Create a new thread with default options.
 * 
 * @param func Thread function to execute
 * @param arg Argument to pass to thread function
 * @return Result containing thread handle on success
 */
AriaResult* aria_thread_create(AriaThreadFunc func, void* arg);

/**
 * Create a new thread with specific options.
 * 
 * @param func Thread function to execute
 * @param arg Argument to pass to thread function
 * @param options Thread configuration options
 * @return Result containing thread handle on success
 */
AriaResult* aria_thread_create_with_options(AriaThreadFunc func, void* arg, 
                                             const AriaThreadOptions* options);

/**
 * Wait for a thread to complete and retrieve its return value.
 * This function blocks until the thread terminates.
 * 
 * @param thread Thread handle to wait for
 * @param ret_val Pointer to store thread return value (can be NULL)
 * @return Result indicating success or error
 */
AriaResult* aria_thread_join(AriaThread* thread, void** ret_val);

/**
 * Detach a thread, allowing it to run independently.
 * Once detached, the thread cannot be joined and resources
 * are automatically released when the thread exits.
 * 
 * @param thread Thread handle to detach
 * @return Result indicating success or error
 */
AriaResult* aria_thread_detach(AriaThread* thread);

/**
 * Get the current thread's ID.
 * 
 * @return Current thread identifier
 */
AriaThreadId aria_thread_current_id(void);

/**
 * Get a thread's ID from its handle.
 * 
 * @param thread Thread handle
 * @return Thread identifier
 */
AriaThreadId aria_thread_get_id(AriaThread* thread);

/**
 * Compare two thread IDs for equality.
 * 
 * @param tid1 First thread ID
 * @param tid2 Second thread ID
 * @return true if IDs refer to the same thread
 */
bool aria_thread_id_equal(AriaThreadId tid1, AriaThreadId tid2);

/**
 * Yield the CPU to other threads.
 * Hints to the scheduler that this thread is willing to give up its timeslice.
 */
void aria_thread_yield(void);

/**
 * Sleep for a specified duration.
 * 
 * @param nanoseconds Duration to sleep in nanoseconds
 */
void aria_thread_sleep_ns(uint64_t nanoseconds);

/**
 * Set the current thread's name (for debugging).
 * 
 * @param name Thread name (max 15 characters on Linux)
 */
void aria_thread_set_name(const char* name);

/* ============================================================================
 * Mutex Synchronization
 * ============================================================================ */

/**
 * Mutex handle.
 * Provides mutual exclusion for shared resources.
 */
typedef struct AriaMutex AriaMutex;

/**
 * Mutex types.
 */
typedef enum {
    ARIA_MUTEX_NORMAL,     // Standard non-recursive mutex
    ARIA_MUTEX_RECURSIVE,  // Allows same thread to lock multiple times
} AriaMutexType;

/**
 * Create a new mutex.
 * 
 * @param type Mutex type (normal or recursive)
 * @return Result containing mutex handle on success
 */
AriaResult* aria_mutex_create(AriaMutexType type);

/**
 * Destroy a mutex.
 * The mutex must not be locked when destroyed.
 * 
 * @param mutex Mutex to destroy
 * @return Result indicating success or error
 */
AriaResult* aria_mutex_destroy(AriaMutex* mutex);

/**
 * Lock a mutex (blocking).
 * If the mutex is already locked, the calling thread blocks until it becomes available.
 * 
 * @param mutex Mutex to lock
 * @return Result indicating success or error
 */
AriaResult* aria_mutex_lock(AriaMutex* mutex);

/**
 * Try to lock a mutex (non-blocking).
 * Returns immediately with success or failure.
 * 
 * @param mutex Mutex to lock
 * @return Result with success=true if locked, success=false if already locked
 */
AriaResult* aria_mutex_trylock(AriaMutex* mutex);

/**
 * Unlock a mutex.
 * The mutex must be locked by the calling thread.
 * 
 * @param mutex Mutex to unlock
 * @return Result indicating success or error
 */
AriaResult* aria_mutex_unlock(AriaMutex* mutex);

/* ============================================================================
 * Condition Variables
 * ============================================================================ */

/**
 * Condition variable handle.
 * Allows threads to wait for specific conditions.
 */
typedef struct AriaCondVar AriaCondVar;

/**
 * Create a new condition variable.
 * 
 * @return Result containing condition variable handle on success
 */
AriaResult* aria_condvar_create(void);

/**
 * Destroy a condition variable.
 * 
 * @param condvar Condition variable to destroy
 * @return Result indicating success or error
 */
AriaResult* aria_condvar_destroy(AriaCondVar* condvar);

/**
 * Wait on a condition variable.
 * Atomically unlocks the mutex and blocks until signaled.
 * When awakened, the mutex is re-locked before returning.
 * 
 * @param condvar Condition variable to wait on
 * @param mutex Mutex to unlock while waiting (must be locked by caller)
 * @return Result indicating success or error
 */
AriaResult* aria_condvar_wait(AriaCondVar* condvar, AriaMutex* mutex);

/**
 * Wait on a condition variable with timeout.
 * 
 * @param condvar Condition variable to wait on
 * @param mutex Mutex to unlock while waiting
 * @param timeout_ns Timeout in nanoseconds
 * @return Result with success=true if signaled, success=false if timeout
 */
AriaResult* aria_condvar_timedwait(AriaCondVar* condvar, AriaMutex* mutex, 
                                    uint64_t timeout_ns);

/**
 * Wake up one thread waiting on a condition variable.
 * 
 * @param condvar Condition variable to signal
 * @return Result indicating success or error
 */
AriaResult* aria_condvar_signal(AriaCondVar* condvar);

/**
 * Wake up all threads waiting on a condition variable.
 * 
 * @param condvar Condition variable to broadcast
 * @return Result indicating success or error
 */
AriaResult* aria_condvar_broadcast(AriaCondVar* condvar);

/* ============================================================================
 * Thread-Local Storage
 * ============================================================================ */

/**
 * Thread-local storage key.
 */
typedef struct AriaThreadLocal AriaThreadLocal;

/**
 * Destructor function for thread-local data.
 * Called when thread exits with non-NULL value.
 */
typedef void (*AriaThreadLocalDestructor)(void* value);

/**
 * Create a thread-local storage key.
 * 
 * @param destructor Optional destructor called when thread exits (can be NULL)
 * @return Result containing TLS key on success
 */
AriaResult* aria_thread_local_create(AriaThreadLocalDestructor destructor);

/**
 * Destroy a thread-local storage key.
 * 
 * @param key TLS key to destroy
 * @return Result indicating success or error
 */
AriaResult* aria_thread_local_destroy(AriaThreadLocal* key);

/**
 * Get the value associated with a TLS key for the current thread.
 * 
 * @param key TLS key
 * @return Stored value (or NULL if not set)
 */
void* aria_thread_local_get(AriaThreadLocal* key);

/**
 * Set the value associated with a TLS key for the current thread.
 * 
 * @param key TLS key
 * @param value Value to store
 * @return Result indicating success or error
 */
AriaResult* aria_thread_local_set(AriaThreadLocal* key, void* value);

/* ============================================================================
 * Read-Write Locks
 * ============================================================================ */

/**
 * Read-write lock handle.
 * Allows multiple readers or single writer.
 */
typedef struct AriaRWLock AriaRWLock;

/**
 * Create a new read-write lock.
 * 
 * @return Result containing RW lock handle on success
 */
AriaResult* aria_rwlock_create(void);

/**
 * Destroy a read-write lock.
 * 
 * @param rwlock RW lock to destroy
 * @return Result indicating success or error
 */
AriaResult* aria_rwlock_destroy(AriaRWLock* rwlock);

/**
 * Acquire a read lock (shared access).
 * Multiple threads can hold read locks simultaneously.
 * 
 * @param rwlock RW lock to lock for reading
 * @return Result indicating success or error
 */
AriaResult* aria_rwlock_rdlock(AriaRWLock* rwlock);

/**
 * Try to acquire a read lock (non-blocking).
 * 
 * @param rwlock RW lock to lock for reading
 * @return Result with success=true if locked, success=false if unavailable
 */
AriaResult* aria_rwlock_tryrdlock(AriaRWLock* rwlock);

/**
 * Acquire a write lock (exclusive access).
 * Only one thread can hold a write lock.
 * 
 * @param rwlock RW lock to lock for writing
 * @return Result indicating success or error
 */
AriaResult* aria_rwlock_wrlock(AriaRWLock* rwlock);

/**
 * Try to acquire a write lock (non-blocking).
 * 
 * @param rwlock RW lock to lock for writing
 * @return Result with success=true if locked, success=false if unavailable
 */
AriaResult* aria_rwlock_trywrlock(AriaRWLock* rwlock);

/**
 * Release a read or write lock.
 * 
 * @param rwlock RW lock to unlock
 * @return Result indicating success or error
 */
AriaResult* aria_rwlock_unlock(AriaRWLock* rwlock);

/* ============================================================================
 * Barriers
 * ============================================================================ */

/**
 * Barrier handle.
 * Synchronization point for multiple threads.
 */
typedef struct AriaBarrier AriaBarrier;

/**
 * Create a new barrier.
 * 
 * @param count Number of threads that must reach the barrier
 * @return Result containing barrier handle on success
 */
AriaResult* aria_barrier_create(uint32_t count);

/**
 * Destroy a barrier.
 * 
 * @param barrier Barrier to destroy
 * @return Result indicating success or error
 */
AriaResult* aria_barrier_destroy(AriaBarrier* barrier);

/**
 * Wait at a barrier.
 * Blocks until all threads reach the barrier.
 * 
 * @param barrier Barrier to wait at
 * @return Result indicating success or error
 */
AriaResult* aria_barrier_wait(AriaBarrier* barrier);

/* ============================================================================
 * Hardware Information
 * ============================================================================ */

/**
 * Get the number of hardware threads (logical CPUs) available.
 * 
 * @return Number of hardware threads
 */
uint32_t aria_thread_hardware_concurrency(void);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_THREAD_H

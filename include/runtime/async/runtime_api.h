// Aria Async Runtime C API
// Functions callable from LLVM-generated Aria code
// These provide the bridge between generated coroutines and the executor

#ifndef ARIA_RUNTIME_ASYNC_API_H
#define ARIA_RUNTIME_ASYNC_API_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque types for FFI
typedef void* AriaCoroutineHandle;  // LLVM i8* coroutine handle
typedef void* AriaFutureHandle;     // Future<T>*
typedef uint64_t AriaTaskId;        // Task identifier
typedef void* AriaExecutorHandle;   // Executor*

/**
 * Global executor instance
 * Aria programs use a single global executor for simplicity
 */
AriaExecutorHandle aria_get_global_executor(void);

/**
 * Create a new executor
 * Returns executor handle for multi-executor scenarios
 */
AriaExecutorHandle aria_executor_create(void);

/**
 * Destroy an executor
 */
void aria_executor_destroy(AriaExecutorHandle executor);

/**
 * Spawn an async task on the executor
 * @param executor Executor handle (or NULL for global)
 * @param coro_handle LLVM coroutine handle (i8*)
 * @return Task ID
 */
AriaTaskId aria_executor_spawn(AriaExecutorHandle executor, AriaCoroutineHandle coro_handle);

/**
 * Run executor to completion
 * Executes all spawned tasks until ready queue is empty
 * @param executor Executor handle (or NULL for global)
 */
void aria_executor_run(AriaExecutorHandle executor);

/**
 * Step executor (run one task)
 * @param executor Executor handle (or NULL for global)
 * @return true if more tasks remain, false if done
 */
bool aria_executor_step(AriaExecutorHandle executor);

/**
 * Create a future for type with given size
 * @param type_size Size of T in bytes
 * @return Future handle
 */
AriaFutureHandle aria_future_create(size_t type_size);

/**
 * Destroy a future
 */
void aria_future_destroy(AriaFutureHandle future);

/**
 * Poll a future
 * @param future Future handle
 * @return true if ready, false if pending
 */
bool aria_future_poll(AriaFutureHandle future);

/**
 * Check if future is ready
 * @param future Future handle
 * @return true if ready
 */
bool aria_future_is_ready(AriaFutureHandle future);

/**
 * Get value from ready future
 * @param future Future handle
 * @return Pointer to value (caller must know type)
 */
void* aria_future_get_value(AriaFutureHandle future);

/**
 * Set future value (complete the future)
 * @param future Future handle
 * @param value Pointer to value
 * @param size Size of value
 */
void aria_future_set_value(AriaFutureHandle future, const void* value, size_t size);

/**
 * Set future error
 * @param future Future handle
 */
void aria_future_set_error(AriaFutureHandle future);

/**
 * LLVM Coroutine Intrinsic Wrappers
 * These are called by LLVM-generated code
 */

/**
 * Resume a coroutine
 * Wrapper for llvm.coro.resume
 */
void aria_coro_resume(AriaCoroutineHandle handle);

/**
 * Check if coroutine is done
 * Wrapper for llvm.coro.done
 */
bool aria_coro_done(AriaCoroutineHandle handle);

/**
 * Destroy a coroutine
 * Wrapper for llvm.coro.destroy
 */
void aria_coro_destroy(AriaCoroutineHandle handle);

/**
 * Allocate coroutine frame
 * Called by malloc in generated code
 */
void* aria_coro_alloc(size_t size);

/**
 * Free coroutine frame
 * Called by free in generated code
 */
void aria_coro_free(void* ptr);

/**
 * Helper: Await a coroutine and get its result
 * Simplifies common pattern of spawn + run + get result
 * @param coro_handle Coroutine to await
 * @return Result pointer (type-erased)
 */
void* aria_await_sync(AriaCoroutineHandle coro_handle);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_ASYNC_API_H

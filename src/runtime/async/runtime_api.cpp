// Aria Async Runtime C API Implementation
// Bridge between LLVM-generated code and C++ runtime

#include "runtime/async/runtime_api.h"
#include "runtime/async/executor.h"
#include "runtime/async/coroutine.h"
#include "runtime/async/future.h"
#include <cstdlib>
#include <cstring>

using namespace aria::runtime;

// Global executor instance
static Executor* globalExecutor = nullptr;

extern "C" {

AriaExecutorHandle aria_get_global_executor(void) {
    if (!globalExecutor) {
        globalExecutor = new Executor();
    }
    return static_cast<AriaExecutorHandle>(globalExecutor);
}

AriaExecutorHandle aria_executor_create(void) {
    return static_cast<AriaExecutorHandle>(new Executor());
}

void aria_executor_destroy(AriaExecutorHandle executor) {
    if (executor) {
        delete static_cast<Executor*>(executor);
    }
}

AriaTaskId aria_executor_spawn(AriaExecutorHandle executor, AriaCoroutineHandle coro_handle) {
    Executor* exec = executor ? static_cast<Executor*>(executor) : 
                               static_cast<Executor*>(aria_get_global_executor());
    
    if (!exec || !coro_handle) {
        return 0;  // Invalid task ID
    }
    
    return exec->spawn(coro_handle);
}

void aria_executor_run(AriaExecutorHandle executor) {
    Executor* exec = executor ? static_cast<Executor*>(executor) : 
                               static_cast<Executor*>(aria_get_global_executor());
    
    if (exec) {
        exec->runToCompletion();
    }
}

bool aria_executor_step(AriaExecutorHandle executor) {
    Executor* exec = executor ? static_cast<Executor*>(executor) : 
                               static_cast<Executor*>(aria_get_global_executor());
    
    if (exec) {
        return exec->step();
    }
    return false;
}

AriaFutureHandle aria_future_create(size_t type_size) {
    return static_cast<AriaFutureHandle>(new Future(type_size));
}

// Defined in async_io_api.cpp — releases registry entry for shared_ptr-managed futures.
// Returns true if the handle was found and released, false otherwise.
extern "C" bool aria_future_release_check(void* handle);

void aria_future_destroy(AriaFutureHandle future) {
    if (future) {
        // Try the shared_ptr registry first (async_io API futures).
        // If found, the shared_ptr handles deletion.
        if (!aria_future_release_check(future)) {
            // Not in registry — raw-allocated via aria_future_create.
            delete static_cast<Future*>(future);
        }
    }
}

bool aria_future_poll(AriaFutureHandle future) {
    if (!future) return false;
    
    Future* fut = static_cast<Future*>(future);
    return fut->poll() == PollResult::READY;
}

bool aria_future_is_ready(AriaFutureHandle future) {
    if (!future) return false;
    
    Future* fut = static_cast<Future*>(future);
    return fut->isReady();
}

void* aria_future_get_value(AriaFutureHandle future) {
    if (!future) return nullptr;
    
    Future* fut = static_cast<Future*>(future);
    return fut->getValue();
}

void aria_future_set_value(AriaFutureHandle future, const void* value, size_t size) {
    if (!future) return;
    
    Future* fut = static_cast<Future*>(future);
    fut->setValue(value, size);
}

void aria_future_set_error(AriaFutureHandle future) {
    if (!future) return;
    
    Future* fut = static_cast<Future*>(future);
    fut->setError(true);
}

void aria_coro_resume(AriaCoroutineHandle handle) {
    if (handle) {
        __aria_coro_resume(handle);
    }
}

bool aria_coro_done(AriaCoroutineHandle handle) {
    if (handle) {
        return __aria_coro_done(handle);
    }
    return true;
}

void aria_coro_destroy(AriaCoroutineHandle handle) {
    if (handle) {
        __aria_coro_destroy(handle);
    }
}

void* aria_coro_alloc(size_t size) {
    return malloc(size);
}

void aria_coro_free(void* ptr) {
    free(ptr);
}

void* aria_await_sync(AriaCoroutineHandle coro_handle) {
    if (!coro_handle) return nullptr;
    
    // Get global executor
    Executor* exec = static_cast<Executor*>(aria_get_global_executor());
    
    // Spawn the coroutine as a task
    AriaTaskId taskId = exec->spawn(coro_handle);
    
    // Run to completion
    exec->runToCompletion();
    
    // Get the task
    Task* task = exec->getTask(taskId);
    if (!task) return nullptr;
    
    // Return result storage
    return task->getResultStorage();
}

} // extern "C"

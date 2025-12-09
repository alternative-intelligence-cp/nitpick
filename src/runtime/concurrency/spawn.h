#pragma once
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>
#include <cstring>

// Future - type-erased result container for spawned tasks
// Uses void* storage to avoid template instantiation in C
struct Future {
    void* result;                       // Pointer to result value (heap allocated)
    std::atomic<bool> completed;        // Has task finished?
    std::mutex mutex;                   // Protects result access
    std::condition_variable cv;         // For efficient blocking in get()
    size_t result_size;                 // Size of result type for allocation
    
    Future(size_t size) : result(nullptr), completed(false), result_size(size) {
        if (size > 0) {
            result = malloc(size);
            memset(result, 0, size);
        }
    }
    
    ~Future() {
        if (result) {
            free(result);
        }
    }
    
    // Block until result is ready
    void wait() {
        std::unique_lock<std::mutex> lock(mutex);
        while (!completed.load()) {
            cv.wait(lock);
        }
    }
    
    // Called by worker thread when task completes
    void set(void* value) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (result && value && result_size > 0) {
                memcpy(result, value, result_size);
            }
            completed.store(true);
        }
        cv.notify_all();  // Wake up anyone waiting
    }
    
    // Get result pointer (caller must cast to correct type)
    void* get() {
        wait();
        return result;
    }
};

// SpawnTask - simpler than CoroutineFrame, just a function call
struct SpawnTask {
    void (*function)(void*);   // The function to execute
    void* args;                // Arguments bundled as struct
    Future* future;            // Future to write result into
    
    // Type-specific completion that knows how to extract result
    // Signature: void completion(void* future, void* result)
    void (*completion)(void* future, void* result);
};

// C API for spawning tasks
extern "C" {
    // Schedule a spawn task (simpler than coroutine scheduling)
    void aria_spawn_schedule(SpawnTask* task);
    
    // Initialize spawn runtime (may reuse coroutine scheduler)
    void aria_spawn_init(int num_threads);
    
    // Shutdown spawn runtime
    void aria_spawn_shutdown();
    
    // Allocate a Future for a given type size
    Future* aria_future_create(size_t result_size);
    
    // Wait for Future and get result pointer
    void* aria_future_get(Future* future);
    
    // Free a Future
    void aria_future_free(Future* future);
}

#pragma once
#include <mutex>
#include <condition_variable>
#include <atomic>

// Future<T> - represents the eventual result of a spawned task
// Generic design: stores void* for any type, caller casts back
template<typename T>
struct Future {
    T result;                           // The actual result value
    std::atomic<bool> completed;        // Has task finished?
    std::mutex mutex;                   // Protects result access
    std::condition_variable cv;         // For efficient blocking in get()
    
    Future() : completed(false) {}
    
    // Block until result is ready, then return it
    T get() {
        std::unique_lock<std::mutex> lock(mutex);
        while (!completed.load()) {
            cv.wait(lock);  // Efficient blocking (like your flags + polling, but OS-level)
        }
        return result;
    }
    
    // Called by worker thread when task completes
    void set(T value) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            result = value;
            completed.store(true);
        }
        cv.notify_all();  // Wake up anyone waiting in get()
    }
};

// SpawnTask - simpler than CoroutineFrame, just a function call
struct SpawnTask {
    void (*function)(void*);   // The function to execute
    void* args;                // Arguments bundled as struct
    void* future;              // Future to write result into (type-erased)
    void (*completion)(void* future, void* result);  // Type-specific completion handler
};

// C API for spawning tasks
extern "C" {
    // Schedule a spawn task (simpler than coroutine scheduling)
    void aria_spawn_schedule(SpawnTask* task);
    
    // Initialize spawn runtime (may reuse coroutine scheduler)
    void aria_spawn_init(int num_threads);
    
    // Shutdown spawn runtime
    void aria_spawn_shutdown();
}

// LLVM coroutine intrinsic wrappers
// Provides C++ interface to LLVM coroutine operations
// These will be linked with LLVM-generated code

#ifndef ARIA_RUNTIME_ASYNC_COROUTINE_H
#define ARIA_RUNTIME_ASYNC_COROUTINE_H

#include <cstddef>  // For std::size_t
#include <cstdint>

namespace aria {
namespace runtime {

// Forward declarations for LLVM intrinsics
// These are implemented by LLVM's coroutine passes
extern "C" {
    // Resume a suspended coroutine
    // Equivalent to: @llvm.coro.resume(i8*)
    void __aria_coro_resume(void* handle);
    
    // Destroy a coroutine
    // Equivalent to: @llvm.coro.destroy(i8*)
    void __aria_coro_destroy(void* handle);
    
    // Check if coroutine is done
    // Equivalent to: @llvm.coro.done(i8*)
    bool __aria_coro_done(void* handle);
}

/**
 * CoroutineHandle - Wrapper for LLVM i8* coroutine handle
 */
class CoroutineHandle {
private:
    void* handle;
    
public:
    explicit CoroutineHandle(void* h = nullptr) : handle(h) {}
    
    void* get() const { return handle; }
    bool valid() const { return handle != nullptr; }
    
    /**
     * Resume coroutine execution
     * Calls LLVM coro.resume intrinsic
     */
    void resume() {
        if (handle) {
            __aria_coro_resume(handle);
        }
    }
    
    /**
     * Check if coroutine is done
     * Calls LLVM coro.done intrinsic
     */
    bool done() const {
        if (handle) {
            return __aria_coro_done(handle);
        }
        return true;
    }
    
    /**
     * Destroy coroutine
     * Calls LLVM coro.destroy intrinsic
     */
    void destroy() {
        if (handle) {
            __aria_coro_destroy(handle);
            handle = nullptr;
        }
    }
};

/**
 * Coroutine runtime support functions
 * Called by LLVM-generated async function code
 */
namespace coro_support {

/**
 * Allocate coroutine frame
 * Called by LLVM's coroutine frame allocation
 */
void* allocate_frame(std::size_t size);

/**
 * Free coroutine frame
 * Called by LLVM's coroutine frame deallocation
 */
void free_frame(void* ptr);

/**
 * Create future for async function result
 * Called at beginning of async function
 */
void* create_future(std::size_t typeSize);

/**
 * Complete future with value
 * Called when async function returns
 */
void complete_future(void* futurePtr, const void* value, std::size_t size);

/**
 * Complete future with error
 * Called when async function encounters error
 */
void complete_future_error(void* futurePtr);

} // namespace coro_support

} // namespace runtime
} // namespace aria

#endif // ARIA_RUNTIME_ASYNC_COROUTINE_H

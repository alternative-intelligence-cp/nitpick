// LLVM coroutine runtime support implementation
#include "runtime/async/coroutine.h"
#include "runtime/async/future.h"
#include <cstdlib>
#include <cstring>

namespace npk {
namespace runtime {

// LLVM coroutine frame ABI: the first two words of a coroutine frame are
// function pointers for resume and destroy. This matches the layout produced
// by LLVM's CoroSplit pass. When a coroutine reaches its final suspend point,
// LLVM sets the resume pointer to nullptr, signaling completion.
struct AriaCoroFrame {
    void (*resumeFn)(void*);   // Resume continuation (nullptr when done)
    void (*destroyFn)(void*);  // Cleanup and frame deallocation function
    // Coroutine-specific state, locals, and spills follow...
};

extern "C" {
    void __aria_coro_resume(void* handle) {
        if (!handle) return;
        auto* frame = reinterpret_cast<AriaCoroFrame*>(handle);
        if (frame->resumeFn) {
            frame->resumeFn(handle);
        }
    }
    
    void __aria_coro_destroy(void* handle) {
        if (!handle) return;
        auto* frame = reinterpret_cast<AriaCoroFrame*>(handle);
        if (frame->destroyFn) {
            frame->destroyFn(handle);
        }
    }
    
    bool __aria_coro_done(void* handle) {
        if (!handle) return true;
        auto* frame = reinterpret_cast<AriaCoroFrame*>(handle);
        // LLVM convention: coroutine is done when resume pointer is null
        return frame->resumeFn == nullptr;
    }
}

namespace coro_support {

void* allocate_frame(std::size_t size) {
    // Allocate coroutine frame on heap
    // In production, this could use a pool allocator
    return std::malloc(size);
}

void free_frame(void* ptr) {
    // Free coroutine frame
    if (ptr) {
        std::free(ptr);
    }
}

void* create_future(std::size_t typeSize) {
    // Allocate Future<T> for async function result
    Future* future = new Future(typeSize);
    return future;
}

void complete_future(void* futurePtr, const void* value, std::size_t size) {
    // Complete future with result value
    if (!futurePtr) return;
    
    Future* future = static_cast<Future*>(futurePtr);
    future->setValue(value, size);
}

void complete_future_error(void* futurePtr) {
    // Complete future with error
    if (!futurePtr) return;
    
    Future* future = static_cast<Future*>(futurePtr);
    future->setError(true);
}

} // namespace coro_support

} // namespace runtime
} // namespace npk

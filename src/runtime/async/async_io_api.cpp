// C API implementation for async I/O

#include "runtime/async/async_io_api.h"
#include "runtime/async/async_io.h"
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

using namespace npk::runtime;
using namespace npk::runtime::async_io;

// Registry to keep shared_ptrs alive while C API handles exist.
// Entries are removed when aria_future_destroy() is called.
namespace {
    std::mutex future_registry_mtx;
    std::unordered_map<Future*, std::shared_ptr<Future>> future_registry;

    AriaFutureHandle register_future(std::shared_ptr<Future> f) {
        Future* raw = f.get();
        std::lock_guard<std::mutex> lock(future_registry_mtx);
        future_registry[raw] = std::move(f);
        return static_cast<AriaFutureHandle>(raw);
    }
}

// Called by aria_future_destroy (in runtime_api.cpp) to release shared_ptr.
// Returns true if handle was in the registry and released.
extern "C" bool aria_future_release_check(void* handle) {
    if (!handle) return false;
    std::lock_guard<std::mutex> lock(future_registry_mtx);
    return future_registry.erase(static_cast<Future*>(handle)) > 0;
}

extern "C" {

AriaFutureHandle aria_read_file_async(const char* path) {
    if (!path) return nullptr;
    return register_future(read_file_async(std::string(path)));
}

AriaFutureHandle aria_write_file_async(const char* path, const char* content) {
    if (!path || !content) return nullptr;
    return register_future(write_file_async(std::string(path), std::string(content)));
}

AriaFutureHandle aria_append_file_async(const char* path, const char* content) {
    if (!path || !content) return nullptr;
    return register_future(append_file_async(std::string(path), std::string(content)));
}

AriaFutureHandle aria_file_exists_async(const char* path) {
    if (!path) return nullptr;
    return register_future(file_exists_async(std::string(path)));
}

AriaFutureHandle aria_delete_file_async(const char* path) {
    if (!path) return nullptr;
    return register_future(delete_file_async(std::string(path)));
}

AriaFutureHandle aria_sleep_async(uint64_t milliseconds) {
    return register_future(sleep_async(milliseconds));
}

AriaFutureHandle aria_join_all(AriaFutureHandle* futures, size_t count) {
    if (!futures || count == 0) return nullptr;
    
    // Convert handles back to shared_ptrs
    std::vector<std::shared_ptr<Future>> shared_futs;
    shared_futs.reserve(count);
    {
        std::lock_guard<std::mutex> lock(future_registry_mtx);
        for (size_t i = 0; i < count; i++) {
            Future* raw = static_cast<Future*>(futures[i]);
            auto it = future_registry.find(raw);
            if (it != future_registry.end()) {
                shared_futs.push_back(it->second);
            }
        }
    }
    if (shared_futs.size() != count) return nullptr;
    
    auto result = join_all(shared_futs.data(), count);
    return register_future(result);
}

AriaFutureHandle aria_race(AriaFutureHandle* futures, size_t count) {
    if (!futures || count == 0) return nullptr;
    
    std::vector<std::shared_ptr<Future>> shared_futs;
    shared_futs.reserve(count);
    {
        std::lock_guard<std::mutex> lock(future_registry_mtx);
        for (size_t i = 0; i < count; i++) {
            Future* raw = static_cast<Future*>(futures[i]);
            auto it = future_registry.find(raw);
            if (it != future_registry.end()) {
                shared_futs.push_back(it->second);
            }
        }
    }
    if (shared_futs.size() != count) return nullptr;
    
    auto result = race(shared_futs.data(), count);
    return register_future(result);
}

AriaFutureHandle aria_with_timeout(AriaFutureHandle future, uint64_t milliseconds) {
    if (!future) return nullptr;
    
    std::shared_ptr<Future> fut;
    {
        std::lock_guard<std::mutex> lock(future_registry_mtx);
        auto it = future_registry.find(static_cast<Future*>(future));
        if (it != future_registry.end()) {
            fut = it->second;
        }
    }
    if (!fut) return nullptr;
    
    return register_future(with_timeout(fut, milliseconds));
}

} // extern "C"

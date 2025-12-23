// C API implementation for async I/O

#include "runtime/async/async_io_api.h"
#include "runtime/async/async_io.h"
#include <cstring>

using namespace aria::runtime;
using namespace aria::runtime::async_io;

extern "C" {

AriaFutureHandle aria_read_file_async(const char* path) {
    if (!path) return nullptr;
    
    std::string path_str(path);
    Future* future = read_file_async(path_str);
    
    return static_cast<AriaFutureHandle>(future);
}

AriaFutureHandle aria_write_file_async(const char* path, const char* content) {
    if (!path || !content) return nullptr;
    
    std::string path_str(path);
    std::string content_str(content);
    Future* future = write_file_async(path_str, content_str);
    
    return static_cast<AriaFutureHandle>(future);
}

AriaFutureHandle aria_append_file_async(const char* path, const char* content) {
    if (!path || !content) return nullptr;
    
    std::string path_str(path);
    std::string content_str(content);
    Future* future = append_file_async(path_str, content_str);
    
    return static_cast<AriaFutureHandle>(future);
}

AriaFutureHandle aria_file_exists_async(const char* path) {
    if (!path) return nullptr;
    
    std::string path_str(path);
    Future* future = file_exists_async(path_str);
    
    return static_cast<AriaFutureHandle>(future);
}

AriaFutureHandle aria_delete_file_async(const char* path) {
    if (!path) return nullptr;
    
    std::string path_str(path);
    Future* future = delete_file_async(path_str);
    
    return static_cast<AriaFutureHandle>(future);
}

AriaFutureHandle aria_sleep_async(uint64_t milliseconds) {
    Future* future = sleep_async(milliseconds);
    return static_cast<AriaFutureHandle>(future);
}

AriaFutureHandle aria_join_all(AriaFutureHandle* futures, size_t count) {
    if (!futures || count == 0) return nullptr;
    
    Future** future_array = reinterpret_cast<Future**>(futures);
    Future* result = join_all(future_array, count);
    
    return static_cast<AriaFutureHandle>(result);
}

AriaFutureHandle aria_race(AriaFutureHandle* futures, size_t count) {
    if (!futures || count == 0) return nullptr;
    
    Future** future_array = reinterpret_cast<Future**>(futures);
    Future* result = race(future_array, count);
    
    return static_cast<AriaFutureHandle>(result);
}

AriaFutureHandle aria_with_timeout(AriaFutureHandle future, uint64_t milliseconds) {
    if (!future) return nullptr;
    
    Future* fut = static_cast<Future*>(future);
    Future* result = with_timeout(fut, milliseconds);
    
    return static_cast<AriaFutureHandle>(result);
}

} // extern "C"

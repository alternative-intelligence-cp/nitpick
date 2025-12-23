// C API for async I/O operations
// Callable from LLVM-generated Aria code

#ifndef ARIA_RUNTIME_ASYNC_IO_API_H
#define ARIA_RUNTIME_ASYNC_IO_API_H

#include "runtime/async/runtime_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Async file operations
 */

// Read file asynchronously
// Returns future handle containing file contents
AriaFutureHandle aria_read_file_async(const char* path);

// Write file asynchronously
// Returns future handle containing bool success
AriaFutureHandle aria_write_file_async(const char* path, const char* content);

// Append to file asynchronously
AriaFutureHandle aria_append_file_async(const char* path, const char* content);

// Check if file exists
AriaFutureHandle aria_file_exists_async(const char* path);

// Delete file
AriaFutureHandle aria_delete_file_async(const char* path);

/**
 * Async timers
 */

// Sleep for milliseconds
AriaFutureHandle aria_sleep_async(uint64_t milliseconds);

/**
 * Combinators
 */

// Join multiple futures (all must complete)
AriaFutureHandle aria_join_all(AriaFutureHandle* futures, size_t count);

// Race multiple futures (first to complete wins)
AriaFutureHandle aria_race(AriaFutureHandle* futures, size_t count);

// Add timeout to future
AriaFutureHandle aria_with_timeout(AriaFutureHandle future, uint64_t milliseconds);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_ASYNC_IO_API_H

// Async I/O primitives for Aria
// Provides non-blocking file and network operations

#ifndef ARIA_RUNTIME_ASYNC_IO_H
#define ARIA_RUNTIME_ASYNC_IO_H

#include <cstdint>
#include <string>
#include <functional>
#include <memory>
#include "runtime/async/future.h"

namespace npk {
namespace runtime {
namespace async_io {

/**
 * Async file operations
 */

// Read entire file asynchronously
// Returns Future<string> with file contents
std::shared_ptr<Future> read_file_async(const std::string& path);

// Write file asynchronously
// Returns Future<bool> - true on success
std::shared_ptr<Future> write_file_async(const std::string& path, const std::string& content);

// Append to file asynchronously
std::shared_ptr<Future> append_file_async(const std::string& path, const std::string& content);

// Check if file exists asynchronously
std::shared_ptr<Future> file_exists_async(const std::string& path);

// Delete file asynchronously
std::shared_ptr<Future> delete_file_async(const std::string& path);

/**
 * Async network operations (future work)
 */

// HTTP GET request
// Future* http_get_async(const std::string& url);

// HTTP POST request
// Future* http_post_async(const std::string& url, const std::string& body);

/**
 * Async timers and delays
 */

// Sleep for specified milliseconds
// Returns Future<void> that completes after delay
std::shared_ptr<Future> sleep_async(uint64_t milliseconds);

// Schedule callback after delay
void schedule_callback(uint64_t milliseconds, std::function<void()> callback);

/**
 * Async combinators
 */

// Join multiple futures - completes when ALL complete
// Returns Future<void>
std::shared_ptr<Future> join_all(std::shared_ptr<Future>* futures, size_t count);

// Race multiple futures - completes when FIRST completes
// Returns Future<size_t> with index of completed future
std::shared_ptr<Future> race(std::shared_ptr<Future>* futures, size_t count);

// Timeout wrapper - completes with error if future doesn't complete in time
// Returns Future<T> that errors after timeout
std::shared_ptr<Future> with_timeout(std::shared_ptr<Future> future, uint64_t milliseconds);

} // namespace async_io
} // namespace runtime
} // namespace npk

#endif // ARIA_RUNTIME_ASYNC_IO_H

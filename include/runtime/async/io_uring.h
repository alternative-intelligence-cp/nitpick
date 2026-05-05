// io_uring backend for async file I/O
// High-performance async file operations on Linux

#ifndef ARIA_RUNTIME_ASYNC_IO_URING_H
#define ARIA_RUNTIME_ASYNC_IO_URING_H

#include "runtime/async/future.h"
#include "runtime/async/event_loop.h"
#include <string>
#include <vector>
#include <cstdint>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>
#include <functional>

// Forward declare io_uring structures (avoid including liburing.h in header)
struct io_uring;
struct io_uring_sqe;
struct io_uring_cqe;

namespace npk {
namespace runtime {

/**
 * Simple buffer pool for reducing allocation overhead
 */
class BufferPool {
private:
    std::map<size_t, std::vector<void*>> pools;  // size -> available buffers
    std::mutex pool_mutex;
    size_t max_buffers_per_size;
    
public:
    BufferPool(size_t max_per_size = 32) : max_buffers_per_size(max_per_size) {}
    
    ~BufferPool() {
        // Free all pooled buffers
        for (auto& pair : pools) {
            for (void* buf : pair.second) {
                free(buf);
            }
        }
    }
    
    void* allocate(size_t size) {
        std::lock_guard<std::mutex> lock(pool_mutex);
        
        auto it = pools.find(size);
        if (it != pools.end() && !it->second.empty()) {
            void* buf = it->second.back();
            it->second.pop_back();
            return buf;
        }
        
        // No pooled buffer available, allocate new
        return malloc(size);
    }
    
    void deallocate(void* buf, size_t size) {
        if (!buf) return;
        
        std::lock_guard<std::mutex> lock(pool_mutex);
        
        auto& pool = pools[size];
        if (pool.size() < max_buffers_per_size) {
            pool.push_back(buf);
        } else {
            free(buf);
        }
    }
};

/**
 * I/O operation types
 */
enum class IOOpType {
    READ,
    WRITE,
    OPEN,
    CLOSE,
    STAT,
    UNLINK,
    FSYNC
};

/**
 * I/O operation context
 */
struct IOOperation {
    IOOpType type;
    Future* future;
    void* buffer;
    size_t buffer_size;
    std::string path;
    int fd;
    uint64_t offset;
    
    IOOperation()
        : type(IOOpType::READ), future(nullptr), buffer(nullptr),
          buffer_size(0), fd(-1), offset(0) {}
};

/**
 * io_uring based async file I/O backend
 * 
 * Provides true async file operations using Linux io_uring.
 * Much faster than thread pool for file I/O.
 */
class IoUringBackend {
private:
    struct io_uring* ring;
    EventLoop* event_loop;
    bool initialized;
    
    // Pending operations (user_data -> IOOperation)
    std::map<uint64_t, IOOperation*> pending_ops;
    uint64_t next_op_id;
    
    // Statistics
    uint64_t ops_submitted;
    uint64_t ops_completed;
    uint64_t ops_failed;
    
    // Batch submission tracking
    int pending_submissions;
    
    // Buffer pool for reducing allocation overhead
    BufferPool buffer_pool;
    
    // Error tracking
    std::string last_error;
    
public:
    IoUringBackend(EventLoop* loop);
    ~IoUringBackend();
    
    /**
     * Initialize io_uring
     * @param queue_depth Submission queue depth
     * @return true on success
     */
    bool init(uint32_t queue_depth = 256);
    
    /**
     * Get last error message
     */
    const std::string& get_last_error() const { return last_error; }
    
    /**
     * Submit all pending operations
     * @return Number of operations submitted
     */
    int submit();
    
    /**
     * Async file read
     * @param path File path
     * @param offset File offset
     * @param size Bytes to read
     * @return Future<std::vector<uint8_t>> - file data
     */
    Future* read_file(const std::string& path, uint64_t offset, size_t size);
    
    /**
     * Async file write
     * @param path File path
     * @param offset File offset
     * @param data Data to write
     * @return Future<int64_t> - bytes written
     */
    Future* write_file(const std::string& path, uint64_t offset, const std::vector<uint8_t>& data);
    
    /**
     * Async file stat
     * @param path File path
     * @return Future<bool> - true if exists
     */
    Future* stat_file(const std::string& path);
    
    /**
     * Async file delete
     * @param path File path
     * @return Future<bool> - true on success
     */
    Future* unlink_file(const std::string& path);
    
    /**
     * Async file sync (flush to disk)
     * @param fd File descriptor
     * @return Future<bool> - true on success
     */
    Future* fsync_file(int fd);
    
    /**
     * Async file open
     * @param path File path
     * @param flags Open flags (O_RDONLY, O_WRONLY, O_RDWR, etc.)
     * @param mode File mode (permissions) for creation
     * @return Future<int> - file descriptor (or -1 on error)
     */
    Future* open_file(const std::string& path, int flags, mode_t mode = 0644);
    
    /**
     * Async file close
     * @param fd File descriptor
     * @return Future<bool> - true on success
     */
    Future* close_file(int fd);
    
    /**
     * Process completion queue
     * Should be called from event loop
     * @return Number of completions processed
     */
    int process_completions();
    
    /**
     * Get statistics
     */
    uint64_t get_ops_submitted() const { return ops_submitted; }
    uint64_t get_ops_completed() const { return ops_completed; }
    uint64_t get_ops_failed() const { return ops_failed; }
    
    /**
     * Get all statistics as a map
     */
    std::map<std::string, uint64_t> get_stats() const {
        return {
            {"submitted", ops_submitted},
            {"completed", ops_completed},
            {"failed", ops_failed},
            {"pending", pending_ops.size()}
        };
    }
    
private:
    /**
     * Complete operation
     */
    void complete_op(IOOperation* op, int result);
};

/**
 * Thread pool backend for platforms without io_uring
 * Fallback for macOS, BSD, etc.
 */
class ThreadPoolBackend {
private:
    EventLoop* event_loop;
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool shutdown;
    
public:
    ThreadPoolBackend(EventLoop* loop, size_t num_threads = 4);
    ~ThreadPoolBackend();
    
    /**
     * Async file read (using thread pool)
     */
    Future* read_file(const std::string& path);
    
    /**
     * Async file write (using thread pool)
     */
    Future* write_file(const std::string& path, const std::string& content);
    
private:
    /**
     * Worker thread function
     */
    void worker_thread();
    
    /**
     * Submit task to pool
     */
    void submit_task(std::function<void()> task);
};

} // namespace runtime
} // namespace npk

#endif // ARIA_RUNTIME_ASYNC_IO_URING_H

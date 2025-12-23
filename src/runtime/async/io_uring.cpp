// io_uring backend implementation
// True async file I/O using Linux io_uring

#include "runtime/async/io_uring.h"
#include <liburing.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

namespace aria {
namespace runtime {

// ============================================================================
// IoUringBackend
// ============================================================================

IoUringBackend::IoUringBackend(EventLoop* loop)
    : ring(nullptr), event_loop(loop), initialized(false),
      next_op_id(1), ops_submitted(0), ops_completed(0), ops_failed(0),
      pending_submissions(0) {
    
    // Allocate io_uring structure
    ring = new struct io_uring();
}

IoUringBackend::~IoUringBackend() {
    // Clean up pending operations
    for (auto& pair : pending_ops) {
        delete pair.second;
    }
    pending_ops.clear();
    
    // Cleanup io_uring instance
    if (ring && initialized) {
        io_uring_queue_exit(ring);
    }
    
    if (ring) {
        delete ring;
        ring = nullptr;
    }
}

bool IoUringBackend::init(uint32_t queue_depth) {
    if (initialized) return true;
    
    // Initialize io_uring with specified queue depth
    int ret = io_uring_queue_init(queue_depth, ring, 0);
    if (ret < 0) {
        last_error = "io_uring_queue_init failed with error " + std::to_string(ret);
        return false;
    }
    
    // Register with event loop for automatic completion processing
    int ring_fd = ring->ring_fd;
    if (ring_fd >= 0 && event_loop) {
        // Add io_uring fd to event loop for completion notifications
        event_loop->add_io_event(ring_fd, EventType::READ, [this]() {
            process_completions();
        });
    }
    
    initialized = true;
    last_error.clear();
    return true;
}

int IoUringBackend::submit() {
    if (!initialized || pending_submissions == 0) return 0;
    
    int ret = io_uring_submit(ring);
    if (ret > 0) {
        ops_submitted += ret;
        pending_submissions = 0;
    }
    
    return ret;
}

Future* IoUringBackend::read_file(const std::string& path, uint64_t offset, size_t size) {
    Future* future = new Future(sizeof(std::vector<uint8_t>));
    
    if (!initialized) {
        std::vector<uint8_t> empty;
        future->setValue(&empty, sizeof(empty));
        return future;
    }
    
    // Open file
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        std::vector<uint8_t> empty;
        future->setValue(&empty, sizeof(empty));
        return future;
    }
    
    // Allocate buffer
    void* buffer = buffer_pool.allocate(size);
    if (!buffer) {
        close(fd);
        std::vector<uint8_t> empty;
        future->setValue(&empty, sizeof(empty));
        return future;
    }
    
    // Create operation
    IOOperation* op = new IOOperation();
    op->type = IOOpType::READ;
    op->future = future;
    op->buffer = buffer;
    op->buffer_size = size;
    op->path = path;
    op->fd = fd;
    op->offset = offset;
    
    // Get submission queue entry
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        buffer_pool.deallocate(buffer, size);
        close(fd);
        delete op;
        std::vector<uint8_t> empty;
        future->setValue(&empty, sizeof(empty));
        return future;
    }
    
    // Prepare read operation
    io_uring_prep_read(sqe, fd, buffer, size, offset);
    
    // Set user data to operation ID
    uint64_t op_id = next_op_id++;
    io_uring_sqe_set_data(sqe, (void*)op_id);
    
    // Track operation
    pending_ops[op_id] = op;
    
    // Queue for batch submission
    pending_submissions++;
    
    return future;
}

Future* IoUringBackend::write_file(const std::string& path, uint64_t offset, const std::vector<uint8_t>& data) {
    Future* future = new Future(sizeof(int64_t));
    
    if (!initialized) {
        int64_t error = -1;
        future->setValue(&error, sizeof(int64_t));
        return future;
    }
    
    // Open file
    int fd = open(path.c_str(), O_WRONLY | O_CREAT, 0644);
    if (fd < 0) {
        int64_t error = -1;
        future->setValue(&error, sizeof(int64_t));
        return future;
    }
    
    // Copy data to heap (will be freed after completion)
    void* buffer = buffer_pool.allocate(data.size());
    if (!buffer) {
        close(fd);
        int64_t error = -1;
        future->setValue(&error, sizeof(int64_t));
        return future;
    }
    std::memcpy(buffer, data.data(), data.size());
    
    // Create operation
    IOOperation* op = new IOOperation();
    op->type = IOOpType::WRITE;
    op->future = future;
    op->buffer = buffer;
    op->buffer_size = data.size();
    op->path = path;
    op->fd = fd;
    op->offset = offset;
    
    // Get submission queue entry
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        buffer_pool.deallocate(buffer, data.size());
        close(fd);
        delete op;
        int64_t error = -1;
        future->setValue(&error, sizeof(int64_t));
        return future;
    }
    
    // Prepare write operation
    io_uring_prep_write(sqe, fd, buffer, data.size(), offset);
    
    // Set user data
    uint64_t op_id = next_op_id++;
    io_uring_sqe_set_data(sqe, (void*)op_id);
    
    // Track operation
    pending_ops[op_id] = op;
    
    // Queue for batch submission
    pending_submissions++;
    
    return future;
}

Future* IoUringBackend::stat_file(const std::string& path) {
    Future* future = new Future(sizeof(bool));
    
    if (!initialized) {
        bool result = false;
        future->setValue(&result, sizeof(bool));
        return future;
    }
    
    // Allocate statx buffer
    struct statx* statxbuf = (struct statx*)malloc(sizeof(struct statx));
    if (!statxbuf) {
        bool result = false;
        future->setValue(&result, sizeof(bool));
        return future;
    }
    
    // Create operation
    IOOperation* op = new IOOperation();
    op->type = IOOpType::STAT;
    op->future = future;
    op->buffer = statxbuf;
    op->buffer_size = sizeof(struct statx);
    op->path = path;
    op->fd = -1;
    
    // Get submission queue entry
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        free(statxbuf);
        delete op;
        bool result = false;
        future->setValue(&result, sizeof(bool));
        return future;
    }
    
    // Prepare statx operation
    io_uring_prep_statx(sqe, AT_FDCWD, path.c_str(), 0, STATX_BASIC_STATS, statxbuf);
    
    // Set user data
    uint64_t op_id = next_op_id++;
    io_uring_sqe_set_data(sqe, (void*)op_id);
    
    // Track operation
    pending_ops[op_id] = op;
    
    // Queue for batch submission
    pending_submissions++;
    
    return future;
}

Future* IoUringBackend::unlink_file(const std::string& path) {
    Future* future = new Future(sizeof(bool));
    
    if (!initialized) {
        bool result = false;
        future->setValue(&result, sizeof(bool));
        return future;
    }
    
    // Create operation
    IOOperation* op = new IOOperation();
    op->type = IOOpType::UNLINK;
    op->future = future;
    op->buffer = nullptr;
    op->buffer_size = 0;
    op->path = path;
    op->fd = -1;
    
    // Get submission queue entry
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        delete op;
        bool result = false;
        future->setValue(&result, sizeof(bool));
        return future;
    }
    
    // Prepare unlinkat operation
    io_uring_prep_unlinkat(sqe, AT_FDCWD, path.c_str(), 0);
    
    // Set user data
    uint64_t op_id = next_op_id++;
    io_uring_sqe_set_data(sqe, (void*)op_id);
    
    // Track operation
    pending_ops[op_id] = op;
    
    // Queue for batch submission
    pending_submissions++;
    
    return future;
}

Future* IoUringBackend::fsync_file(int fd) {
    Future* future = new Future(sizeof(bool));
    
    if (!initialized) {
        bool result = false;
        future->setValue(&result, sizeof(bool));
        return future;
    }
    
    if (fd < 0) {
        bool result = false;
        future->setValue(&result, sizeof(bool));
        return future;
    }
    
    // Create operation
    IOOperation* op = new IOOperation();
    op->type = IOOpType::FSYNC;
    op->future = future;
    op->buffer = nullptr;
    op->buffer_size = 0;
    op->fd = fd;
    
    // Get submission queue entry
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        delete op;
        bool result = false;
        future->setValue(&result, sizeof(bool));
        return future;
    }
    
    // Prepare fsync operation
    io_uring_prep_fsync(sqe, fd, 0);
    
    // Set user data
    uint64_t op_id = next_op_id++;
    io_uring_sqe_set_data(sqe, (void*)op_id);
    
    // Track operation
    pending_ops[op_id] = op;
    
    // Queue for batch submission
    pending_submissions++;
    
    return future;
}

Future* IoUringBackend::open_file(const std::string& path, int flags, mode_t mode) {
    Future* future = new Future(sizeof(int));
    
    if (!initialized) {
        int result = -1;
        future->setValue(&result, sizeof(int));
        return future;
    }
    
    // Create operation
    IOOperation* op = new IOOperation();
    op->type = IOOpType::OPEN;
    op->future = future;
    op->buffer = nullptr;
    op->buffer_size = 0;
    op->path = path;
    
    // Get submission queue entry
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        delete op;
        int result = -1;
        future->setValue(&result, sizeof(int));
        return future;
    }
    
    // Prepare openat operation (AT_FDCWD = open relative to current working directory)
    io_uring_prep_openat(sqe, AT_FDCWD, path.c_str(), flags, mode);
    
    // Set user data
    uint64_t op_id = next_op_id++;
    io_uring_sqe_set_data(sqe, (void*)op_id);
    
    // Track operation
    pending_ops[op_id] = op;
    
    // Queue for batch submission
    pending_submissions++;
    
    return future;
}

Future* IoUringBackend::close_file(int fd) {
    Future* future = new Future(sizeof(bool));
    
    if (!initialized) {
        bool result = false;
        future->setValue(&result, sizeof(bool));
        return future;
    }
    
    if (fd < 0) {
        bool result = false;
        future->setValue(&result, sizeof(bool));
        return future;
    }
    
    // Create operation
    IOOperation* op = new IOOperation();
    op->type = IOOpType::CLOSE;
    op->future = future;
    op->buffer = nullptr;
    op->buffer_size = 0;
    op->fd = fd;
    
    // Get submission queue entry
    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        delete op;
        bool result = false;
        future->setValue(&result, sizeof(bool));
        return future;
    }
    
    // Prepare close operation
    io_uring_prep_close(sqe, fd);
    
    // Set user data
    uint64_t op_id = next_op_id++;
    io_uring_sqe_set_data(sqe, (void*)op_id);
    
    // Track operation
    pending_ops[op_id] = op;
    
    // Queue for batch submission
    pending_submissions++;
    
    return future;
}

int IoUringBackend::process_completions() {
    if (!initialized) return 0;
    
    // Submit any pending operations first
    if (pending_submissions > 0) {
        submit();
    }
    
    int processed = 0;
    struct io_uring_cqe* cqe;
    
    // Process all available completions
    while (io_uring_peek_cqe(ring, &cqe) == 0) {
        // Get operation ID from user data
        uint64_t op_id = (uint64_t)io_uring_cqe_get_data(cqe);
        
        // Find operation
        auto it = pending_ops.find(op_id);
        if (it != pending_ops.end()) {
            IOOperation* op = it->second;
            
            // Get result
            int result = cqe->res;
            
            // Complete operation
            complete_op(op, result);
            
            // Remove from pending
            pending_ops.erase(it);
        }
        
        // Mark as seen
        io_uring_cqe_seen(ring, cqe);
        processed++;
    }
    
    return processed;
}

// Result structure for file read operations
struct ReadResult {
    uint8_t* data;
    size_t size;
};

void IoUringBackend::complete_op(IOOperation* op, int result) {
    ops_completed++;
    
    if (result < 0) {
        ops_failed++;
    }
    
    // Set future value based on operation type
    switch (op->type) {
        case IOOpType::READ: {
            if (result > 0) {
                // Success - store buffer pointer and size
                // The buffer is already allocated, just transfer ownership to the result
                ReadResult res;
                res.data = (uint8_t*)op->buffer;
                res.size = result;
                op->future->setValue(&res, sizeof(res));
                // Don't free the buffer here - it's now owned by the ReadResult
                op->buffer = nullptr;
            } else {
                // Error or EOF
                ReadResult res;
                res.data = nullptr;
                res.size = 0;
                op->future->setValue(&res, sizeof(res));
            }
            break;
        }
        
        case IOOpType::WRITE: {
            int64_t bytes_written = result > 0 ? result : -1;
            op->future->setValue(&bytes_written, sizeof(int64_t));
            break;
        }
        
        case IOOpType::STAT: {
            bool exists = result >= 0;
            op->future->setValue(&exists, sizeof(bool));
            break;
        }
        
        case IOOpType::UNLINK: {
            bool success = result == 0;
            op->future->setValue(&success, sizeof(bool));
            break;
        }
        
        case IOOpType::FSYNC: {
            bool success = result == 0;
            op->future->setValue(&success, sizeof(bool));
            break;
        }
        
        case IOOpType::OPEN: {
            int fd = result >= 0 ? result : -1;
            op->future->setValue(&fd, sizeof(int));
            break;
        }
        
        case IOOpType::CLOSE: {
            bool success = result == 0;
            op->future->setValue(&success, sizeof(bool));
            break;
        }
        
        default:
            break;
    }
    
    // Clean up
    if (op->buffer) {
        // For READ operations, buffer ownership was transferred to ReadResult
        // For WRITE/STAT operations, we can return buffer to pool
        if (op->type == IOOpType::WRITE || op->type == IOOpType::STAT) {
            buffer_pool.deallocate(op->buffer, op->buffer_size);
        } else if (op->type != IOOpType::READ) {
            // Other operation types that we didn't transfer ownership
            free(op->buffer);
        }
        // READ buffers are freed by caller after using ReadResult
    }
    if (op->fd >= 0) {
        close(op->fd);
    }
    delete op;
}

// ============================================================================
// ThreadPoolBackend
// ============================================================================

ThreadPoolBackend::ThreadPoolBackend(EventLoop* loop, size_t num_threads)
    : event_loop(loop), shutdown(false) {
    
    // Create worker threads
    for (size_t i = 0; i < num_threads; i++) {
        threads.emplace_back(&ThreadPoolBackend::worker_thread, this);
    }
}

ThreadPoolBackend::~ThreadPoolBackend() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        shutdown = true;
    }
    cv.notify_all();
    
    // Join all threads
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

Future* ThreadPoolBackend::read_file(const std::string& path) {
    Future* future = new Future(sizeof(std::string));
    
    submit_task([path, future, this]() {
        // Read file in worker thread
        std::ifstream file(path);
        std::string content;
        
        if (file) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            content = buffer.str();
        }
        
        // Post result to event loop
        event_loop->post_task([future, content]() {
            future->setValue(&content, sizeof(content));
        });
    });
    
    return future;
}

Future* ThreadPoolBackend::write_file(const std::string& path, const std::string& content) {
    Future* future = new Future(sizeof(bool));
    
    submit_task([path, content, future, this]() {
        // Write file in worker thread
        std::ofstream file(path, std::ios::trunc);
        bool success = false;
        
        if (file) {
            file << content;
            success = file.good();
        }
        
        // Post result to event loop
        event_loop->post_task([future, success]() {
            future->setValue(&success, sizeof(success));
        });
    });
    
    return future;
}

void ThreadPoolBackend::worker_thread() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait(lock, [this]() { return shutdown || !tasks.empty(); });
            
            if (shutdown && tasks.empty()) {
                return;
            }
            
            task = std::move(tasks.front());
            tasks.pop();
        }
        
        if (task) {
            task();
        }
    }
}

void ThreadPoolBackend::submit_task(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        tasks.push(std::move(task));
    }
    cv.notify_one();
}

} // namespace runtime
} // namespace aria

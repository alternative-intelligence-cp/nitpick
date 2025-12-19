#include "tools/lsp/thread_pool.h"
#include <algorithm>

namespace aria {
namespace lsp {

ThreadPool::ThreadPool(size_t worker_count)
    : result_callback_(nullptr) {
    
    // Determine worker count
    size_t count = (worker_count == 0) ? determine_worker_count() : worker_count;
    
    // Spawn worker threads
    workers_.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        workers_.emplace_back(&ThreadPool::worker_main, this);
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::submit(Task task) {
    work_queue_.push(std::move(task));
}

void ThreadPool::cancel_request(const json& request_id) {
    work_queue_.cancel_request(request_id);
}

void ThreadPool::shutdown() {
    // Signal shutdown
    work_queue_.shutdown();
    
    // Wait for all workers to finish
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::worker_main() {
    while (true) {
        // Block until task available (or shutdown)
        auto task_opt = work_queue_.pop();
        
        if (!task_opt.has_value()) {
            // Shutdown signal received
            break;
        }
        
        Task& task = task_opt.value();
        
        // Check if task was cancelled while queued
        if (task.cancellation_token->is_cancelled()) {
            continue;
        }
        
        // Execute the task
        json result;
        try {
            result = task.work();
        } catch (const std::exception& e) {
            // Task threw exception - create error result
            result = {
                {"error", {
                    {"code", -32603},  // InternalError
                    {"message", std::string("Task execution failed: ") + e.what()}
                }}
            };
        }
        
        // If this was a request (not notification), send result back
        if (!task.request_id.is_null() && result_callback_) {
            result_callback_(task.request_id, result);
        }
    }
}

size_t ThreadPool::determine_worker_count() {
    unsigned int hw_threads = std::thread::hardware_concurrency();
    
    if (hw_threads == 0) {
        // Couldn't detect - default to 2
        return 2;
    }
    
    // Reserve 1 core for I/O thread, use rest for workers
    // Clamp between 2 and 8
    size_t workers = std::max(2u, hw_threads - 1);
    workers = std::min(workers, size_t(8));
    
    return workers;
}

} // namespace lsp
} // namespace aria

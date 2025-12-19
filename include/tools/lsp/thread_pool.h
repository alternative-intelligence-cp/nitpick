#ifndef ARIA_LSP_THREAD_POOL_H
#define ARIA_LSP_THREAD_POOL_H

#include "tools/lsp/work_queue.h"
#include <vector>
#include <thread>
#include <functional>

namespace aria {
namespace lsp {

/**
 * Thread Pool for LSP Server
 * 
 * Architecture (from research_035):
 * - Fixed number of worker threads
 * - Each worker pulls tasks from shared WorkQueue
 * - Workers check cancellation tokens before/during work
 * - Clean shutdown waits for all workers to finish current tasks
 * 
 * Worker count heuristics:
 * - Default: std::thread::hardware_concurrency() - 1 (reserve 1 for I/O thread)
 * - Minimum: 2 workers (maintain responsiveness)
 * - Maximum: 8 workers (diminishing returns for LSP workload)
 */
class ThreadPool {
public:
    /**
     * Create thread pool with specified worker count
     * 
     * If worker_count == 0, uses heuristic based on CPU count
     */
    explicit ThreadPool(size_t worker_count = 0);
    
    /**
     * Destructor - shuts down pool and waits for workers
     */
    ~ThreadPool();
    
    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    /**
     * Submit a task to the work queue
     */
    void submit(Task task);
    
    /**
     * Cancel a specific request
     */
    void cancel_request(const json& request_id);
    
    /**
     * Initiate graceful shutdown
     * - Stops accepting new tasks
     * - Waits for all current tasks to complete
     */
    void shutdown();
    
    /**
     * Check if pool is running
     */
    bool is_running() const {
        return !work_queue_.is_shutdown();
    }
    
    /**
     * Get current queue size
     */
    size_t queue_size() const {
        return work_queue_.size();
    }
    
    /**
     * Set result callback
     * 
     * Called when a request task completes with a result.
     * The I/O thread uses this to send responses back to the client.
     */
    void set_result_callback(std::function<void(const json& id, const json& result)> callback) {
        result_callback_ = std::move(callback);
    }
    
private:
    WorkQueue work_queue_;
    std::vector<std::thread> workers_;
    std::function<void(const json& id, const json& result)> result_callback_;
    
    /**
     * Worker thread main loop
     * 
     * Continuously pulls tasks from queue and executes them
     */
    void worker_main();
    
    /**
     * Determine optimal worker count based on CPU cores
     */
    static size_t determine_worker_count();
};

} // namespace lsp
} // namespace aria

#endif // ARIA_LSP_THREAD_POOL_H

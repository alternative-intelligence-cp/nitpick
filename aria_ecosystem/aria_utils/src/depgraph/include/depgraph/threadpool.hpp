/**
 * threadpool.hpp
 * Work-Stealing Thread Pool for Parallel Builds
 *
 * M:N threading model mapping build tasks to worker threads.
 * Supports dynamic load balancing via work-stealing.
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_DEPGRAPH_THREADPOOL_HPP
#define ARIA_DEPGRAPH_THREADPOOL_HPP

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <future>

namespace aria {
namespace depgraph {

// -----------------------------------------------------------------------------
// Task Type
// -----------------------------------------------------------------------------
using Task = std::function<void()>;

// -----------------------------------------------------------------------------
// Thread Pool
// -----------------------------------------------------------------------------
class ThreadPool {
public:
    /**
     * Create thread pool with specified number of workers.
     * Default: hardware_concurrency threads.
     */
    explicit ThreadPool(size_t num_threads = 0);

    /**
     * Destructor waits for all tasks to complete.
     */
    ~ThreadPool();

    // No copying
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * Submit a task for execution.
     */
    void submit(Task task);

    /**
     * Submit a task and get a future for the result.
     */
    template<typename F, typename... Args>
    auto submit_with_result(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>;

    /**
     * Wait for all submitted tasks to complete.
     */
    void wait_all();

    /**
     * Get number of worker threads.
     */
    size_t thread_count() const { return workers_.size(); }

    /**
     * Get number of pending tasks.
     */
    size_t pending_count() const;

    /**
     * Get number of active (running) tasks.
     */
    size_t active_count() const { return active_tasks_.load(); }

    /**
     * Check if pool is idle (no pending or active tasks).
     */
    bool idle() const;

    /**
     * Shutdown the pool (cancel pending tasks).
     */
    void shutdown();

    /**
     * Check if pool is stopped.
     */
    bool stopped() const { return stop_.load(); }

private:
    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;

    mutable std::mutex queue_mutex_;
    std::condition_variable task_available_;
    std::condition_variable task_complete_;

    std::atomic<bool> stop_{false};
    std::atomic<size_t> active_tasks_{0};
    std::atomic<size_t> completed_tasks_{0};
    size_t submitted_tasks_ = 0;

    // Worker thread function
    void worker_loop();
};

// Template implementation
template<typename F, typename... Args>
auto ThreadPool::submit_with_result(F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type>
{
    using ReturnType = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<ReturnType> result = task->get_future();

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (stop_) {
            throw std::runtime_error("Cannot submit to stopped ThreadPool");
        }
        tasks_.push([task]() { (*task)(); });
        submitted_tasks_++;
    }

    task_available_.notify_one();
    return result;
}

} // namespace depgraph
} // namespace aria

#endif // ARIA_DEPGRAPH_THREADPOOL_HPP

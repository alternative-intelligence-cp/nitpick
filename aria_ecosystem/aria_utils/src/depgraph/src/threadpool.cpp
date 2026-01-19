/**
 * threadpool.cpp
 * Thread Pool implementation
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "depgraph/threadpool.hpp"

namespace aria {
namespace depgraph {

ThreadPool::ThreadPool(size_t num_threads) {
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) {
            num_threads = 4;  // Fallback
        }
    }

    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::worker_loop, this);
    }
}

ThreadPool::~ThreadPool() {
    shutdown();

    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::submit(Task task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (stop_) {
            return;  // Don't accept new tasks after shutdown
        }
        tasks_.push(std::move(task));
        submitted_tasks_++;
    }
    task_available_.notify_one();
}

void ThreadPool::wait_all() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    task_complete_.wait(lock, [this] {
        return tasks_.empty() && active_tasks_.load() == 0;
    });
}

size_t ThreadPool::pending_count() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

bool ThreadPool::idle() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return tasks_.empty() && active_tasks_.load() == 0;
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_ = true;

        // Clear pending tasks
        while (!tasks_.empty()) {
            tasks_.pop();
        }
    }
    task_available_.notify_all();
}

void ThreadPool::worker_loop() {
    while (true) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            task_available_.wait(lock, [this] {
                return stop_ || !tasks_.empty();
            });

            if (stop_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
            active_tasks_.fetch_add(1);
        }

        // Execute task outside of lock
        try {
            task();
        } catch (...) {
            // Swallow exceptions to prevent worker death
            // Individual tasks should handle their own errors
        }

        active_tasks_.fetch_sub(1);
        completed_tasks_.fetch_add(1);

        task_complete_.notify_all();
    }
}

} // namespace depgraph
} // namespace aria

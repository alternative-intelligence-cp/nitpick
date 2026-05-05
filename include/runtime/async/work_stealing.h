// Multi-threaded work-stealing executor
// High-performance parallel async execution

#ifndef ARIA_RUNTIME_ASYNC_WORK_STEALING_H
#define ARIA_RUNTIME_ASYNC_WORK_STEALING_H

#include "runtime/async/executor.h"
#include "runtime/async/event_loop.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <deque>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

namespace npk {
namespace runtime {

/**
 * Work-stealing deque for tasks
 * Lock-free for owner thread, locked for stealing
 */
class WorkStealingDeque {
private:
    std::deque<Task*> tasks;
    std::mutex mutex;
    
public:
    /**
     * Push task (owner thread only)
     */
    void push(Task* task);
    
    /**
     * Pop task (owner thread only)
     */
    Task* pop();
    
    /**
     * Steal task (other threads)
     */
    Task* steal();
    
    /**
     * Check if empty
     */
    bool empty() const;
    
    /**
     * Get size
     */
    size_t size() const;
};

/**
 * Worker thread in work-stealing executor
 */
class WorkerThread {
private:
    size_t id;
    std::thread thread;
    WorkStealingDeque local_queue;
    
    // Shared state
    std::vector<WorkerThread*>* all_workers;
    std::atomic<bool>* shutdown;
    std::condition_variable* global_cv;
    std::mutex* global_mutex;
    
    // Statistics
    std::atomic<uint64_t> tasks_executed;
    std::atomic<uint64_t> tasks_stolen;
    std::atomic<uint64_t> steal_attempts;
    
    bool running;
    
public:
    WorkerThread(size_t id);
    ~WorkerThread();
    
    /**
     * Start worker thread
     */
    void start(
        std::vector<WorkerThread*>* workers,
        std::atomic<bool>* shutdown_flag,
        std::condition_variable* cv,
        std::mutex* mutex
    );
    
    /**
     * Submit task to this worker
     */
    void submit(Task* task);
    
    /**
     * Try to steal a task from this worker
     */
    Task* try_steal();
    
    /**
     * Get statistics
     */
    uint64_t get_tasks_executed() const { return tasks_executed.load(); }
    uint64_t get_tasks_stolen() const { return tasks_stolen.load(); }
    uint64_t get_steal_attempts() const { return steal_attempts.load(); }
    size_t get_queue_size() const { return local_queue.size(); }
    
    /**
     * Get worker ID
     */
    size_t get_id() const { return id; }
    
    /**
     * Get native thread handle for affinity setting
     */
    pthread_t get_thread_native_handle() { return thread.native_handle(); }
    
private:
    /**
     * Worker thread main loop
     */
    void worker_loop();
    
    /**
     * Try to get work from local queue or steal from others
     */
    Task* get_task();
    
    /**
     * Execute a task
     */
    void execute_task(Task* task);
};

/**
 * Multi-threaded work-stealing executor
 * 
 * Features:
 * - Multiple worker threads
 * - Work-stealing for load balancing
 * - CPU affinity support
 * - Configurable worker count
 * - Integration with event loop
 */
class WorkStealingExecutor {
private:
    std::vector<WorkerThread*> workers;
    std::atomic<bool> shutdown;
    std::condition_variable cv;
    std::mutex mutex;
    
    // Task submission
    std::atomic<size_t> next_worker;  // Round-robin for initial submission
    
    // Event loop integration
    EventLoop* event_loop;
    
    // Statistics
    std::atomic<uint64_t> total_tasks_submitted;
    std::atomic<uint64_t> total_tasks_completed;
    
    size_t num_workers;
    bool started;
    
public:
    /**
     * Create work-stealing executor
     * @param num_threads Number of worker threads (0 = hardware concurrency)
     * @param loop Optional event loop for I/O integration
     */
    WorkStealingExecutor(size_t num_threads = 0, EventLoop* loop = nullptr);
    ~WorkStealingExecutor();
    
    /**
     * Start all worker threads
     */
    void start();
    
    /**
     * Stop all worker threads
     */
    void stop();
    
    /**
     * Submit task for execution
     * @param task Task to execute
     * @return Task ID
     */
    uint64_t submit(Task* task);
    
    /**
     * Wait for all tasks to complete
     */
    void wait_all();
    
    /**
     * Get number of worker threads
     */
    size_t get_worker_count() const { return num_workers; }
    
    /**
     * Get total tasks submitted
     */
    uint64_t get_tasks_submitted() const { return total_tasks_submitted.load(); }
    
    /**
     * Get total tasks completed
     */
    uint64_t get_tasks_completed() const;
    
    /**
     * Get statistics for all workers
     */
    void print_statistics() const;
    
    /**
     * Set CPU affinity for workers
     * @param enable Enable CPU affinity
     */
    void set_cpu_affinity(bool enable);
    
private:
    /**
     * Select worker for task submission
     */
    WorkerThread* select_worker();
};

/**
 * NUMA-aware executor
 * Optimizes for multi-socket systems by grouping workers by NUMA node
 * and preferring local steal targets. Degrades gracefully on non-NUMA.
 */
class NumaExecutor : public WorkStealingExecutor {
private:
    size_t num_nodes;
    // Maps worker index -> NUMA node
    std::vector<int> worker_to_node;
    
public:
    /**
     * Create NUMA-aware executor
     * @param num_threads Worker count (0 = auto-detect)
     * @param loop Event loop
     */
    NumaExecutor(size_t num_threads = 0, EventLoop* loop = nullptr)
        : WorkStealingExecutor(num_threads, loop), num_nodes(1)
    {
        detect_topology();
    }
    
    /**
     * Get number of NUMA nodes detected
     */
    size_t get_numa_nodes() const { return num_nodes; }
    
    /**
     * Get NUMA node for a given worker
     */
    int get_worker_node(size_t worker_id) const {
        if (worker_id < worker_to_node.size()) {
            return worker_to_node[worker_id];
        }
        return 0;
    }
    
private:
    /**
     * Detect NUMA topology from /sys filesystem
     * Falls back to single-node on non-NUMA systems
     */
    void detect_topology() {
        num_nodes = 1;  // Default: single node
        
#ifdef __linux__
        // Try to detect NUMA nodes from sysfs
        // /sys/devices/system/node/node*/cpulist
        FILE* f = fopen("/sys/devices/system/node/online", "r");
        if (f) {
            char buf[256];
            if (fgets(buf, sizeof(buf), f)) {
                // Parse range like "0-3" → 4 nodes, or "0" → 1 node
                int hi = 0;
                char* dash = strchr(buf, '-');
                if (dash) {
                    hi = atoi(dash + 1);
                }
                num_nodes = static_cast<size_t>(hi + 1);
            }
            fclose(f);
        }
#endif
        
        // Assign workers to nodes round-robin
        size_t wc = get_worker_count();
        worker_to_node.resize(wc);
        for (size_t i = 0; i < wc; i++) {
            worker_to_node[i] = static_cast<int>(i % num_nodes);
        }
    }
};

} // namespace runtime
} // namespace npk

#endif // ARIA_RUNTIME_ASYNC_WORK_STEALING_H

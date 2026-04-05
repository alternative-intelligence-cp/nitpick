// Work-stealing executor implementation

#include "runtime/async/work_stealing.h"
#include "runtime/async/coroutine.h"
#include <algorithm>
#include <random>
#include <iostream>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

// Forward declare coroutine intrinsics
extern "C" {
    void __aria_coro_resume(void*);
    bool __aria_coro_done(void*);
}

namespace aria {
namespace runtime {

// ============================================================================
// WorkStealingDeque
// ============================================================================

void WorkStealingDeque::push(Task* task) {
    std::lock_guard<std::mutex> lock(mutex);
    tasks.push_back(task);
}

Task* WorkStealingDeque::pop() {
    std::lock_guard<std::mutex> lock(mutex);
    if (tasks.empty()) {
        return nullptr;
    }
    Task* task = tasks.back();
    tasks.pop_back();
    return task;
}

Task* WorkStealingDeque::steal() {
    std::lock_guard<std::mutex> lock(mutex);
    if (tasks.empty()) {
        return nullptr;
    }
    Task* task = tasks.front();
    tasks.pop_front();
    return task;
}

bool WorkStealingDeque::empty() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex));
    return tasks.empty();
}

size_t WorkStealingDeque::size() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex));
    return tasks.size();
}

// ============================================================================
// WorkerThread
// ============================================================================

WorkerThread::WorkerThread(size_t id)
    : id(id), all_workers(nullptr), shutdown(nullptr),
      global_cv(nullptr), global_mutex(nullptr),
      tasks_executed(0), tasks_stolen(0), steal_attempts(0),
      running(false) {
}

WorkerThread::~WorkerThread() {
    if (thread.joinable()) {
        thread.join();
    }
}

void WorkerThread::start(
    std::vector<WorkerThread*>* workers,
    std::atomic<bool>* shutdown_flag,
    std::condition_variable* cv,
    std::mutex* mutex) {
    
    all_workers = workers;
    shutdown = shutdown_flag;
    global_cv = cv;
    global_mutex = mutex;
    
    running = true;
    thread = std::thread(&WorkerThread::worker_loop, this);
}

void WorkerThread::submit(Task* task) {
    local_queue.push(task);
    global_cv->notify_one();
}

Task* WorkerThread::try_steal() {
    return local_queue.steal();
}

void WorkerThread::worker_loop() {
    while (running && !shutdown->load(std::memory_order_relaxed)) {
        Task* task = get_task();
        
        if (task) {
            execute_task(task);
        } else {
            // No work available, wait a bit
            std::unique_lock<std::mutex> lock(*global_mutex);
            global_cv->wait_for(lock, std::chrono::milliseconds(1));
        }
    }
}

Task* WorkerThread::get_task() {
    // Try local queue first (LIFO for cache locality)
    Task* task = local_queue.pop();
    if (task) {
        return task;
    }
    
    // Try stealing from other workers (random victim selection)
    if (all_workers && all_workers->size() > 1) {
        // Random starting point
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, all_workers->size() - 1);
        
        size_t start = dis(gen);
        
        // Try each worker once
        for (size_t i = 0; i < all_workers->size(); i++) {
            size_t victim_id = (start + i) % all_workers->size();
            
            if (victim_id == id) {
                continue;  // Don't steal from ourselves
            }
            
            WorkerThread* victim = (*all_workers)[victim_id];
            steal_attempts.fetch_add(1, std::memory_order_relaxed);
            
            task = victim->try_steal();
            if (task) {
                tasks_stolen.fetch_add(1, std::memory_order_relaxed);
                return task;
            }
        }
    }
    
    return nullptr;
}

void WorkerThread::execute_task(Task* task) {
    if (!task) return;
    
    // Execute task
    task->setState(TaskState::RUNNING);
    
    // Get coroutine handle
    void* coro_handle = task->getHandle();
    
    if (coro_handle) {
        // Resume coroutine using declared C functions
        __aria_coro_resume(coro_handle);
        
        if (__aria_coro_done(coro_handle)) {
            task->setState(TaskState::COMPLETED);
        } else {
            task->setState(TaskState::SUSPENDED);
            // Re-queue suspended task
            local_queue.push(task);
        }
    }
    
    tasks_executed.fetch_add(1, std::memory_order_relaxed);
}

// ============================================================================
// WorkStealingExecutor
// ============================================================================

WorkStealingExecutor::WorkStealingExecutor(size_t num_threads, EventLoop* loop)
    : shutdown(false), next_worker(0), event_loop(loop),
      total_tasks_submitted(0), total_tasks_completed(0),
      num_workers(num_threads), started(false) {
    
    if (num_workers == 0) {
        num_workers = std::thread::hardware_concurrency();
        if (num_workers == 0) {
            num_workers = 4;  // Fallback
        }
    }
    
    // Create workers
    for (size_t i = 0; i < num_workers; i++) {
        workers.push_back(new WorkerThread(i));
    }
}

WorkStealingExecutor::~WorkStealingExecutor() {
    stop();
    
    // Delete workers
    for (auto* worker : workers) {
        delete worker;
    }
    workers.clear();
}

void WorkStealingExecutor::start() {
    if (started) return;
    
    shutdown.store(false, std::memory_order_relaxed);
    
    // Start all workers
    for (auto* worker : workers) {
        worker->start(&workers, &shutdown, &cv, &mutex);
    }
    
    started = true;
}

void WorkStealingExecutor::stop() {
    if (!started) return;
    
    shutdown.store(true, std::memory_order_relaxed);
    cv.notify_all();
    
    // Wait for workers to finish
    // (WorkerThread destructor joins threads)
    
    started = false;
}

uint64_t WorkStealingExecutor::submit(Task* task) {
    if (!started) {
        start();
    }
    
    // Select worker (round-robin)
    WorkerThread* worker = select_worker();
    
    // Submit task
    worker->submit(task);
    
    total_tasks_submitted.fetch_add(1, std::memory_order_relaxed);
    
    return task->getId();
}

void WorkStealingExecutor::wait_all() {
    // Condition-variable wait with periodic polling fallback
    while (get_tasks_completed() < total_tasks_submitted.load()) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, std::chrono::milliseconds(1), [this]() {
            return get_tasks_completed() >= total_tasks_submitted.load();
        });
    }
}

uint64_t WorkStealingExecutor::get_tasks_completed() const {
    uint64_t total = 0;
    for (const auto* worker : workers) {
        total += worker->get_tasks_executed();
    }
    return total;
}

void WorkStealingExecutor::print_statistics() const {
    std::cout << "\n=== Work-Stealing Executor Statistics ===\n";
    std::cout << "Workers: " << num_workers << "\n";
    std::cout << "Tasks submitted: " << total_tasks_submitted.load() << "\n";
    std::cout << "Tasks completed: " << get_tasks_completed() << "\n\n";
    
    std::cout << "Per-Worker Statistics:\n";
    for (const auto* worker : workers) {
        std::cout << "Worker " << worker->get_id() << ": "
                  << "executed=" << worker->get_tasks_executed()
                  << ", stolen=" << worker->get_tasks_stolen()
                  << ", steal_attempts=" << worker->get_steal_attempts()
                  << ", queue_size=" << worker->get_queue_size()
                  << "\n";
    }
    std::cout << "\n";
}

void WorkStealingExecutor::set_cpu_affinity(bool enable) {
#ifdef __linux__
    if (!enable) return;
    
    unsigned int num_cpus = std::thread::hardware_concurrency();
    if (num_cpus == 0) num_cpus = 1;
    
    // Set CPU affinity for each worker — pin worker i to CPU i % num_cpus
    for (size_t i = 0; i < workers.size(); i++) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(i % num_cpus, &cpuset);
        
        // WorkerThread::thread is std::thread — get native handle
        auto native = workers[i]->get_thread_native_handle();
        if (native) {
            pthread_setaffinity_np(native, sizeof(cpu_set_t), &cpuset);
        }
    }
#endif
}

WorkerThread* WorkStealingExecutor::select_worker() {
    // Round-robin selection
    size_t idx = next_worker.fetch_add(1, std::memory_order_relaxed) % num_workers;
    return workers[idx];
}

} // namespace runtime
} // namespace aria

// Implementation of the Work-Stealing Loop with Wild Affinity Support
#include "scheduler.h"
#include <algorithm>
#include <random>

// Global scheduler instance (singleton)
static Scheduler* global_scheduler = nullptr;

// Initialize the global scheduler with N worker threads
extern "C" void aria_scheduler_init(int num_threads) {
    if (global_scheduler) return;  // Already initialized
    
    // Auto-detect number of threads if 0
    if (num_threads <= 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;  // Fallback if detection fails
    }
    
    global_scheduler = new Scheduler();
    global_scheduler->workers.reserve(num_threads);
    global_scheduler->queues.reserve(num_threads);
    
    // Create worker queues
    for (int i = 0; i < num_threads; ++i) {
        Worker* worker = new Worker();
        worker->id = i;
        global_scheduler->queues.push_back(worker);
    }
    
    // Start worker threads
    for (int i = 0; i < num_threads; ++i) {
        global_scheduler->workers.emplace_back([i]() {
            global_scheduler->queues[i]->run();
        });
    }
}

// Shutdown the scheduler and join all threads
extern "C" void aria_scheduler_shutdown() {
    if (!global_scheduler) return;
    
    // Signal shutdown to all workers (would need a shutdown flag)
    // For now, threads run indefinitely
    
    // Clean up
    for (auto* worker : global_scheduler->queues) {
        delete worker;
    }
    delete global_scheduler;
    global_scheduler = nullptr;
}

// Schedule a task on the current thread's queue
extern "C" void aria_scheduler_schedule(CoroutineFrame* frame) {
    if (!global_scheduler) {
        aria_scheduler_init(std::thread::hardware_concurrency());
    }
    
    Task* task = new Task();
    task->frame = frame;
    task->has_wild_affinity = false;  // No affinity by default
    
    // Get current thread ID and schedule to that worker
    // For now, use round-robin to first available worker
    static int next_worker = 0;
    int worker_id = next_worker++ % global_scheduler->queues.size();
    
    Worker* worker = global_scheduler->queues[worker_id];
    {
        std::lock_guard<std::mutex> lock(worker->queue_lock);
        worker->local_queue.push_back(task);
    }
}

// Resume a coroutine (called when awaited operation completes)
extern "C" void aria_scheduler_resume(CoroutineFrame* frame) {
    aria_scheduler_schedule(frame);
}

// The main loop for every OS thread (Worker)
void Worker::run() {
 while (true) {
     Task* task = nullptr;
     // 1. Try to pop from local queue (LIFO)
     // LIFO provides better cache locality for tasks that were just spawned.
     {
         std::lock_guard<std::mutex> lock(queue_lock);
         if (!local_queue.empty()) {
             task = local_queue.back();
             local_queue.pop_back();
         }
     }

     // 2. If local is empty, try to steal (FIFO)
     if (!task && global_scheduler) {
         // Try to steal from other workers
         std::vector<int> victims;
         for (size_t i = 0; i < global_scheduler->queues.size(); ++i) {
             if ((int)i != this->id) {
                 victims.push_back(i);
             }
         }
         
         // Randomize victim order to reduce contention
         std::random_device rd;
         std::mt19937 g(rd());
         std::shuffle(victims.begin(), victims.end(), g);
         
         // Try to steal from victims
         for (int victim_id : victims) {
             Worker* victim = global_scheduler->queues[victim_id];
             std::lock_guard<std::mutex> lock(victim->queue_lock);
             
             if (!victim->local_queue.empty()) {
                 // Steal from front (FIFO for better load balancing)
                 Task* candidate = victim->local_queue.front();
                 
                 // CRITICAL: Wild Affinity Check
                 // If task has wild affinity, can only run on its designated thread
                 if (candidate->has_wild_affinity && candidate->affinity_thread_id != this->id) {
                     continue;  // Skip this task
                 }
                 
                 // Steal the task
                 task = candidate;
                 victim->local_queue.pop_front();
                 break;
             }
         }
     }

     // 3. Execution
     if (task) {
         // Execute the coroutine by calling the bridge to llvm.coro.resume
         if (task->frame && task->frame->coro_handle) {
             aria_coro_resume_bridge(task->frame->coro_handle);
             
             // Check if coroutine is complete
             if (task->frame->state == CORO_COMPLETE) {
                 // Free the task (coroutine is done)
                 delete task;
             } else if (task->frame->state == CORO_SUSPENDED) {
                 // Coroutine suspended, it will be rescheduled when ready
                 // Don't delete the task yet
             }
         }
     } else {
         // No work found, yield to OS to prevent 100% CPU spin
         std::this_thread::yield();
     }
 }
}

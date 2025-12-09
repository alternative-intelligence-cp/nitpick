#include "spawn.h"
#include <thread>
#include <vector>
#include <deque>
#include <algorithm>
#include <random>

// Reuse Worker concept but for SpawnTask instead of CoroutineFrame
struct SpawnWorker {
    int id;
    std::deque<SpawnTask*> local_queue;
    std::mutex queue_lock;
    
    void run();
};

struct SpawnScheduler {
    std::vector<std::thread> workers;
    std::vector<SpawnWorker*> queues;
    std::mutex init_mutex;
};

// Global spawn scheduler (separate from coroutine scheduler)
static SpawnScheduler* global_spawn_scheduler = nullptr;

extern "C" void aria_spawn_init(int num_threads) {
    if (global_spawn_scheduler) return;  // Already initialized
    
    if (num_threads <= 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;
    }
    
    global_spawn_scheduler = new SpawnScheduler();
    global_spawn_scheduler->workers.reserve(num_threads);
    global_spawn_scheduler->queues.reserve(num_threads);
    
    // Create worker queues
    for (int i = 0; i < num_threads; ++i) {
        SpawnWorker* worker = new SpawnWorker();
        worker->id = i;
        global_spawn_scheduler->queues.push_back(worker);
    }
    
    // Start worker threads
    for (int i = 0; i < num_threads; ++i) {
        global_spawn_scheduler->workers.emplace_back([i]() {
            global_spawn_scheduler->queues[i]->run();
        });
    }
}

extern "C" void aria_spawn_shutdown() {
    if (!global_spawn_scheduler) return;
    
    // TODO: Proper shutdown signaling
    for (auto* worker : global_spawn_scheduler->queues) {
        delete worker;
    }
    delete global_spawn_scheduler;
    global_spawn_scheduler = nullptr;
}

extern "C" void aria_spawn_schedule(SpawnTask* task) {
    if (!global_spawn_scheduler) {
        aria_spawn_init(0);  // Auto-initialize
    }
    
    // Round-robin scheduling for now
    static std::atomic<int> next_worker(0);
    int worker_id = next_worker++ % global_spawn_scheduler->queues.size();
    
    SpawnWorker* worker = global_spawn_scheduler->queues[worker_id];
    {
        std::lock_guard<std::mutex> lock(worker->queue_lock);
        worker->local_queue.push_back(task);
    }
}

// Worker thread main loop
void SpawnWorker::run() {
    while (true) {
        SpawnTask* task = nullptr;
        
        // 1. Try local queue (LIFO for cache locality)
        {
            std::lock_guard<std::mutex> lock(queue_lock);
            if (!local_queue.empty()) {
                task = local_queue.back();
                local_queue.pop_back();
            }
        }
        
        // 2. Try work stealing (FIFO for load balancing)
        if (!task && global_spawn_scheduler) {
            std::vector<int> victims;
            for (size_t i = 0; i < global_spawn_scheduler->queues.size(); ++i) {
                if ((int)i != this->id) {
                    victims.push_back(i);
                }
            }
            
            // Randomize to reduce contention
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(victims.begin(), victims.end(), g);
            
            for (int victim_id : victims) {
                SpawnWorker* victim = global_spawn_scheduler->queues[victim_id];
                std::lock_guard<std::mutex> lock(victim->queue_lock);
                
                if (!victim->local_queue.empty()) {
                    task = victim->local_queue.front();
                    victim->local_queue.pop_front();
                    break;
                }
            }
        }
        
        // 3. Execute task
        if (task) {
            // Call the spawned function, passing the entire task pointer
            // The wrapper will extract args, call the function, and handle the future
            task->function(task);
            
            // Clean up task structure
            // Note: args and future are heap-allocated and managed separately
            if (task->args) {
                free(task->args);
            }
            delete task;
        } else {
            // No work, yield CPU
            std::this_thread::yield();
        }
    }
}

// C API implementations
extern "C" Future* aria_future_create(size_t result_size) {
    return new Future(result_size);
}

extern "C" void* aria_future_get(Future* future) {
    if (!future) return nullptr;
    return future->get();
}

extern "C" void aria_future_free(Future* future) {
    if (future) {
        delete future;
    }
}

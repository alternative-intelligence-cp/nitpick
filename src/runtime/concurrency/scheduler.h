#include <vector>
#include <deque>
#include <thread>
#include <mutex>

struct CoroutineFrame; // Forward decl

// Task represents a suspended coroutine
struct Task {
   CoroutineFrame* frame;
   bool has_wild_affinity; // If true, cannot be stolen by other workers
   int affinity_thread_id; 
};
// Worker represents an OS thread
struct Worker {
   int id;
   std::deque<Task*> local_queue;
   // The Work-Stealing Deque
   std::mutex queue_lock;         // Spinlock for steal operations
   void run();
};
// Global Scheduler Context
struct Scheduler {
   std::vector<std::thread> workers;
   std::vector<Worker*> queues;
   // Global lock only used during runtime initialization/shutdown
   std::mutex init_mutex;
   // Helper to push task to current thread's queue
   void schedule(Task* t) {
       // Implementation details omitted for brevity
   }
};

// RAMP: Coroutine Frame definition
struct CoroutineFrame {
    void* resume_pc;       // Function pointer for resumption
    void* data;            // Captured state (promoted from stack)
    CoroutineFrame* waiting_on; 
    int state;             // RUNNING, SUSPENDED, COMPLETE
    char padding;      // Alignment for AVX
};


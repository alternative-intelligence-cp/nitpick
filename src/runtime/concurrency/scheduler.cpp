// Implementation of the Work-Stealing Loop with Wild Affinity Support
#include "scheduler.h"

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
     if (!task) {
         // Access global scheduler
         // for (auto* victim : global_scheduler->queues)...
         // Implementation omitted for brevity, logic identical to snippet
         // CRITICAL: Wild Affinity Check goes here
         // if (candidate->has_wild_affinity && candidate->affinity_thread_id!= this->id) continue;
     }

     // 3. Execution
     if (task) {
         // execute_coroutine(task->frame);
         auto func = (void (*)(CoroutineFrame*))task->frame->resume_pc;
         func(task->frame);
     } else {
         // No work found, yield to OS to prevent 100% CPU spin
         std::this_thread::yield();
     }
 }
}

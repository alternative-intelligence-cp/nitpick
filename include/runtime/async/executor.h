// Basic async executor for running async tasks to completion
// Phase 4.5.3 Task #6: Single-threaded run-to-completion executor
// Reference: research_029 Section 6 (Task Scheduler & Executor)

#ifndef ARIA_RUNTIME_ASYNC_EXECUTOR_H
#define ARIA_RUNTIME_ASYNC_EXECUTOR_H

#include <vector>
#include <queue>
#include <functional>
#include <cstdint>

namespace aria {
namespace runtime {

// Forward declarations
class Future;
class Task;

/**
 * ExecutorStatus - Status codes for executor operations
 */
enum class ExecutorStatus {
    IDLE,           // No tasks running
    RUNNING,        // Actively executing tasks
    COMPLETED,      // All tasks completed
    ERROR           // Error occurred during execution
};

/**
 * TaskState - State of an async task
 */
enum class TaskState {
    PENDING,        // Not yet started
    RUNNING,        // Currently executing
    SUSPENDED,      // Suspended at await point
    READY,          // Ready to resume
    COMPLETED,      // Finished execution
    FAILED          // Task failed with error
};

/**
 * Task - Represents an async task (coroutine)
 */
class Task {
public:
    using TaskId = uint64_t;
    using CoroutineHandle = void*;  // i8* in LLVM IR
    
private:
    TaskId id;
    CoroutineHandle handle;
    TaskState state;
    void* resultStorage;  // Stores the result value
    bool hasError;
    
public:
    Task(TaskId id, CoroutineHandle handle)
        : id(id), handle(handle), state(TaskState::PENDING), 
          resultStorage(nullptr), hasError(false) {}
    
    ~Task() {
        // Cleanup result storage if allocated
        if (resultStorage) {
            // TODO: Proper cleanup based on result type
        }
    }
    
    TaskId getId() const { return id; }
    CoroutineHandle getHandle() const { return handle; }
    TaskState getState() const { return state; }
    void setState(TaskState newState) { state = newState; }
    
    bool isPending() const { return state == TaskState::PENDING; }
    bool isRunning() const { return state == TaskState::RUNNING; }
    bool isSuspended() const { return state == TaskState::SUSPENDED; }
    bool isReady() const { return state == TaskState::READY; }
    bool isCompleted() const { return state == TaskState::COMPLETED; }
    bool isFailed() const { return state == TaskState::FAILED; }
    
    void* getResultStorage() const { return resultStorage; }
    void setResultStorage(void* storage) { resultStorage = storage; }
    
    bool hasErrorFlag() const { return hasError; }
    void setError(bool error) { hasError = error; }
};

/**
 * Executor - Single-threaded async task executor
 * 
 * Implements run-to-completion execution model:
 * - Tasks run on single thread
 * - No preemption (cooperative multitasking)
 * - Tasks suspend at await points
 * - Executor resumes tasks when dependencies complete
 * 
 * Reference: research_029 Section 6 (Scheduler)
 */
class Executor {
private:
    std::vector<Task*> tasks;           // All registered tasks
    std::queue<Task*> readyQueue;       // Tasks ready to run
    Task::TaskId nextTaskId;
    ExecutorStatus status;
    
    // Statistics
    uint64_t tasksExecuted;
    uint64_t tasksCompleted;
    uint64_t tasksFailed;
    
public:
    Executor() 
        : nextTaskId(1), status(ExecutorStatus::IDLE),
          tasksExecuted(0), tasksCompleted(0), tasksFailed(0) {}
    
    ~Executor() {
        // Cleanup all tasks
        for (Task* task : tasks) {
            delete task;
        }
        tasks.clear();
    }
    
    /**
     * Spawn a new async task
     * @param handle Coroutine handle (i8* from LLVM)
     * @return Task ID
     */
    Task::TaskId spawn(Task::CoroutineHandle handle);
    
    /**
     * Run the executor until all tasks complete
     * Run-to-completion: execute until ready queue is empty
     */
    void runToCompletion();
    
    /**
     * Step execution: run one task from ready queue
     * @return true if more tasks remain, false if done
     */
    bool step();
    
    /**
     * Get executor status
     */
    ExecutorStatus getStatus() const { return status; }
    
    /**
     * Get task by ID
     */
    Task* getTask(Task::TaskId id);
    
    /**
     * Mark task as ready to resume
     * Called when an awaited dependency completes
     */
    void markReady(Task::TaskId id);
    
    /**
     * Get execution statistics
     */
    uint64_t getTasksExecuted() const { return tasksExecuted; }
    uint64_t getTasksCompleted() const { return tasksCompleted; }
    uint64_t getTasksFailed() const { return tasksFailed; }
    uint64_t getPendingTasks() const { return readyQueue.size(); }
};

} // namespace runtime
} // namespace aria

#endif // ARIA_RUNTIME_ASYNC_EXECUTOR_H

// Basic async executor implementation
// Phase 4.5.3 Task #6: Single-threaded run-to-completion executor

#include "runtime/async/executor.h"
#include <algorithm>
#include <stdexcept>

namespace aria {
namespace runtime {

Task::TaskId Executor::spawn(Task::CoroutineHandle handle) {
    if (!handle) {
        throw std::runtime_error("Cannot spawn task with null handle");
    }
    
    // Create new task
    Task::TaskId id = nextTaskId++;
    Task* task = new Task(id, handle);
    
    // Register task
    tasks.push_back(task);
    
    // Add to ready queue (tasks start in ready state)
    task->setState(TaskState::READY);
    readyQueue.push(task);
    
    return id;
}

void Executor::runToCompletion() {
    status = ExecutorStatus::RUNNING;
    
    // Run until no tasks remain in ready queue
    while (!readyQueue.empty()) {
        if (!step()) {
            break;  // Error occurred
        }
    }
    
    // Check if all tasks completed successfully
    bool allCompleted = true;
    bool anyFailed = false;
    
    for (Task* task : tasks) {
        if (task->isFailed()) {
            anyFailed = true;
            allCompleted = false;
        } else if (!task->isCompleted()) {
            allCompleted = false;
        }
    }
    
    if (anyFailed) {
        status = ExecutorStatus::ERROR;
    } else if (allCompleted) {
        status = ExecutorStatus::COMPLETED;
    } else {
        status = ExecutorStatus::IDLE;
    }
}

bool Executor::step() {
    if (readyQueue.empty()) {
        return false;  // No tasks to run
    }
    
    // Get next ready task
    Task* task = readyQueue.front();
    readyQueue.pop();
    
    // Update state
    task->setState(TaskState::RUNNING);
    tasksExecuted++;
    
    // Execute task (simplified - actual execution requires LLVM coroutine resume)
    // In real implementation, this would call:
    //   llvm::Value* resume = coro_resume(task->getHandle());
    // For now, we simulate completion for testing
    
    // Simulate: task runs and either:
    // 1. Completes (no more await points)
    // 2. Suspends at await point
    // 3. Fails with error
    
    // For testing purposes, assume tasks complete immediately
    // Real implementation would resume coroutine and check suspend result
    task->setState(TaskState::COMPLETED);
    tasksCompleted++;
    
    return true;
}

Task* Executor::getTask(Task::TaskId id) {
    for (Task* task : tasks) {
        if (task->getId() == id) {
            return task;
        }
    }
    return nullptr;
}

void Executor::markReady(Task::TaskId id) {
    Task* task = getTask(id);
    if (!task) {
        throw std::runtime_error("Task not found");
    }
    
    // Can only mark suspended tasks as ready
    if (!task->isSuspended()) {
        throw std::runtime_error("Can only mark suspended tasks as ready");
    }
    
    task->setState(TaskState::READY);
    readyQueue.push(task);
}

} // namespace runtime
} // namespace aria

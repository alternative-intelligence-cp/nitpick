#ifndef ARIA_LSP_WORK_QUEUE_H
#define ARIA_LSP_WORK_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <string>
#include <atomic>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace aria {
namespace lsp {

/**
 * Task Priority Levels
 * 
 * Based on research_035 recommendations:
 * - CRITICAL: State-changing notifications (didOpen, didChange)
 * - HIGH: User-facing queries (hover, completion)
 * - NORMAL: Background analysis
 * - LOW: Indexing, symbol search
 */
enum class TaskPriority {
    CRITICAL = 0,
    HIGH = 1,
    NORMAL = 2,
    LOW = 3
};

/**
 * Task Type Classification
 * 
 * Used for debouncing - only one task of same type+uri should be queued
 */
enum class TaskType {
    INITIALIZE,
    SHUTDOWN,
    DID_OPEN,
    DID_CHANGE,
    DID_CLOSE,
    DID_SAVE,
    HOVER,
    DEFINITION,
    COMPLETION,
    DOCUMENT_SYMBOL,
    OTHER
};

/**
 * Cancellation Token
 * 
 * Allows canceling in-flight requests ($/cancelRequest support)
 */
class CancellationToken {
public:
    CancellationToken() : cancelled_(false) {}
    
    void cancel() {
        cancelled_.store(true, std::memory_order_release);
    }
    
    bool is_cancelled() const {
        return cancelled_.load(std::memory_order_acquire);
    }
    
private:
    std::atomic<bool> cancelled_;
};

/**
 * LSP Task
 * 
 * Represents a unit of work for the thread pool.
 * Wraps the actual work function with metadata for prioritization.
 */
struct Task {
    // Unique ID (for cancellation)
    json request_id;
    
    // Task classification
    TaskType type;
    TaskPriority priority;
    
    // Document URI (for debouncing)
    std::string uri;
    
    // The actual work to perform
    // Returns json result (or null for notifications)
    std::function<json()> work;
    
    // Cancellation support
    std::shared_ptr<CancellationToken> cancellation_token;
    
    // Timestamp for ordering
    std::chrono::steady_clock::time_point enqueue_time;
    
    Task(TaskType t, TaskPriority p, const std::string& u, std::function<json()> w)
        : type(t), priority(p), uri(u), work(std::move(w)),
          cancellation_token(std::make_shared<CancellationToken>()),
          enqueue_time(std::chrono::steady_clock::now()) {}
    
    // Comparison for priority queue (lower priority value = higher priority)
    bool operator<(const Task& other) const {
        if (priority != other.priority) {
            return priority > other.priority; // Reverse for max-heap behavior
        }
        // Same priority: FIFO (older tasks first)
        return enqueue_time > other.enqueue_time;
    }
};

/**
 * Thread-Safe Work Queue with Prioritization and Debouncing
 * 
 * Features (from research_035):
 * - Priority-based task scheduling
 * - Debouncing: Coalesce multiple didChange for same document
 * - Blocking wait for workers
 * - Clean shutdown support
 */
class WorkQueue {
public:
    WorkQueue() : shutdown_(false) {}
    
    /**
     * Push a task onto the queue
     * 
     * Debouncing: If a pending task exists for the same (type, uri),
     * mark it as cancelled and replace it with the new task.
     */
    void push(Task task);
    
    /**
     * Pop the highest priority task (blocking)
     * 
     * Returns: Task to execute, or empty optional if shutting down
     */
    std::optional<Task> pop();
    
    /**
     * Cancel a specific request by ID
     * 
     * Used for $/cancelRequest notifications
     */
    void cancel_request(const json& request_id);
    
    /**
     * Initiate shutdown - wake all waiting threads
     */
    void shutdown();
    
    /**
     * Check if queue is shutting down
     */
    bool is_shutdown() const {
        return shutdown_.load(std::memory_order_acquire);
    }
    
    /**
     * Get current queue size (for diagnostics)
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::priority_queue<Task> queue_;
    std::atomic<bool> shutdown_;
    
    // Debouncing map: (type, uri) -> cancellation_token
    // When a new task arrives, cancel any existing task for same (type, uri)
    std::unordered_map<std::string, std::shared_ptr<CancellationToken>> pending_tasks_;
    
    // Request ID -> cancellation_token (for $/cancelRequest)
    std::unordered_map<std::string, std::shared_ptr<CancellationToken>> request_map_;
    
    // Helper: Create debouncing key
    std::string make_key(TaskType type, const std::string& uri) const;
};

} // namespace lsp
} // namespace aria

#endif // ARIA_LSP_WORK_QUEUE_H

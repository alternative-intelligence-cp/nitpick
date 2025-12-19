#include "tools/lsp/work_queue.h"
#include <sstream>

namespace aria {
namespace lsp {

void WorkQueue::push(Task task) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Debouncing logic: Cancel any existing task for same (type, uri)
    if (!task.uri.empty() && 
        (task.type == TaskType::DID_CHANGE || task.type == TaskType::DID_SAVE)) {
        std::string key = make_key(task.type, task.uri);
        auto it = pending_tasks_.find(key);
        if (it != pending_tasks_.end()) {
            // Cancel the old task
            it->second->cancel();
        }
        // Store new task's cancellation token
        pending_tasks_[key] = task.cancellation_token;
    }
    
    // Store request ID mapping for cancellation
    if (!task.request_id.is_null()) {
        std::string id_str = task.request_id.dump();
        request_map_[id_str] = task.cancellation_token;
    }
    
    queue_.push(std::move(task));
    cv_.notify_one();
}

std::optional<Task> WorkQueue::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Wait until: (queue not empty) OR (shutting down)
    cv_.wait(lock, [this] { return !queue_.empty() || shutdown_; });
    
    if (shutdown_ && queue_.empty()) {
        return std::nullopt;
    }
    
    // Get highest priority task
    Task task = queue_.top();
    queue_.pop();
    
    // Remove from debouncing map
    if (!task.uri.empty()) {
        std::string key = make_key(task.type, task.uri);
        pending_tasks_.erase(key);
    }
    
    // Remove from request map
    if (!task.request_id.is_null()) {
        std::string id_str = task.request_id.dump();
        request_map_.erase(id_str);
    }
    
    return task;
}

void WorkQueue::cancel_request(const json& request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string id_str = request_id.dump();
    auto it = request_map_.find(id_str);
    if (it != request_map_.end()) {
        it->second->cancel();
        request_map_.erase(it);
    }
}

void WorkQueue::shutdown() {
    shutdown_.store(true, std::memory_order_release);
    cv_.notify_all();
}

std::string WorkQueue::make_key(TaskType type, const std::string& uri) const {
    std::ostringstream oss;
    oss << static_cast<int>(type) << ":" << uri;
    return oss.str();
}

} // namespace lsp
} // namespace aria

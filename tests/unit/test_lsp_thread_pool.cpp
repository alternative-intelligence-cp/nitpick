#include "test_helpers.h"
#include "tools/lsp/work_queue.h"
#include "tools/lsp/thread_pool.h"
#include <thread>
#include <chrono>
#include <atomic>

using namespace aria::lsp;

TEST_CASE(WorkQueue_push_and_pop) {
    WorkQueue queue;
    
    Task task(TaskType::DID_CHANGE, TaskPriority::CRITICAL, "file.aria", []() {
        return json{{"result", "success"}};
    });
    
    queue.push(std::move(task));
    
    auto popped = queue.pop();
    ASSERT(popped.has_value(), "Should pop task");
    ASSERT(popped->type == TaskType::DID_CHANGE, "Task type preserved");
    ASSERT(popped->priority == TaskPriority::CRITICAL, "Priority preserved");
}

TEST_CASE(WorkQueue_priority_ordering) {
    WorkQueue queue;
    
    // Push tasks with different priorities
    Task low(TaskType::DOCUMENT_SYMBOL, TaskPriority::LOW, "", []() { return json{{"id", 1}}; });
    Task high(TaskType::HOVER, TaskPriority::HIGH, "", []() { return json{{"id", 2}}; });
    Task critical(TaskType::DID_CHANGE, TaskPriority::CRITICAL, "", []() { return json{{"id", 3}}; });
    Task normal(TaskType::DID_SAVE, TaskPriority::NORMAL, "", []() { return json{{"id", 4}}; });
    
    queue.push(std::move(low));
    queue.push(std::move(high));
    queue.push(std::move(critical));
    queue.push(std::move(normal));
    
    // Should pop in priority order: CRITICAL, HIGH, NORMAL, LOW
    auto t1 = queue.pop();
    ASSERT(t1.has_value() && t1->priority == TaskPriority::CRITICAL, "First: CRITICAL");
    
    auto t2 = queue.pop();
    ASSERT(t2.has_value() && t2->priority == TaskPriority::HIGH, "Second: HIGH");
    
    auto t3 = queue.pop();
    ASSERT(t3.has_value() && t3->priority == TaskPriority::NORMAL, "Third: NORMAL");
    
    auto t4 = queue.pop();
    ASSERT(t4.has_value() && t4->priority == TaskPriority::LOW, "Fourth: LOW");
}

TEST_CASE(WorkQueue_debouncing) {
    WorkQueue queue;
    
    std::string uri = "file://test.aria";
    
    // Push multiple didChange tasks for same file
    Task task1(TaskType::DID_CHANGE, TaskPriority::CRITICAL, uri, []() { 
        return json{{"version", 1}}; 
    });
    auto token1 = task1.cancellation_token;
    
    Task task2(TaskType::DID_CHANGE, TaskPriority::CRITICAL, uri, []() { 
        return json{{"version", 2}}; 
    });
    auto token2 = task2.cancellation_token;
    
    Task task3(TaskType::DID_CHANGE, TaskPriority::CRITICAL, uri, []() { 
        return json{{"version", 3}}; 
    });
    auto token3 = task3.cancellation_token;
    
    queue.push(std::move(task1));
    queue.push(std::move(task2));
    queue.push(std::move(task3));
    
    // First two should be cancelled by the third
    ASSERT(token1->is_cancelled(), "Task 1 should be cancelled");
    ASSERT(token2->is_cancelled(), "Task 2 should be cancelled");
    ASSERT(!token3->is_cancelled(), "Task 3 should not be cancelled");
}

TEST_CASE(WorkQueue_cancellation_by_request_ID) {
    WorkQueue queue;
    
    Task task(TaskType::HOVER, TaskPriority::HIGH, "file.aria", []() {
        return json{{"result", "hover info"}};
    });
    task.request_id = json(42);
    auto token = task.cancellation_token;
    
    queue.push(std::move(task));
    
    // Cancel the request
    queue.cancel_request(json(42));
    
    ASSERT(token->is_cancelled(), "Task should be cancelled");
}

TEST_CASE(WorkQueue_shutdown) {
    WorkQueue queue;
    
    std::atomic<bool> thread_exited{false};
    
    std::thread worker([&]() {
        auto task = queue.pop();
        thread_exited = true;
    });
    
    // Give thread time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Shutdown should wake the thread
    queue.shutdown();
    
    worker.join();
    ASSERT(thread_exited, "Worker thread should exit on shutdown");
}

TEST_CASE(CancellationToken_basic_usage) {
    CancellationToken token;
    
    ASSERT(!token.is_cancelled(), "Initially not cancelled");
    
    token.cancel();
    
    ASSERT(token.is_cancelled(), "Should be cancelled after cancel()");
}

TEST_CASE(ThreadPool_basic_execution) {
    ThreadPool pool(2);  // 2 workers
    
    std::atomic<int> counter{0};
    
    Task task(TaskType::OTHER, TaskPriority::NORMAL, "", [&counter]() {
        counter++;
        return json{{"result", "done"}};
    });
    
    pool.submit(std::move(task));
    
    // Give time for execution
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    ASSERT(counter == 1, "Task should have executed");
}

TEST_CASE(ThreadPool_multiple_tasks) {
    ThreadPool pool(4);  // 4 workers
    
    std::atomic<int> counter{0};
    
    // Submit 10 tasks
    for (int i = 0; i < 10; i++) {
        Task task(TaskType::OTHER, TaskPriority::NORMAL, "", [&counter]() {
            counter++;
            return nullptr;
        });
        pool.submit(std::move(task));
    }
    
    // Wait for all tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT(counter == 10, "All tasks should execute");
}

TEST_CASE(ThreadPool_result_callback) {
    ThreadPool pool(2);
    
    std::atomic<bool> callback_called{false};
    json result_id;
    json result_value;
    
    pool.set_result_callback([&](const json& id, const json& result) {
        callback_called = true;
        result_id = id;
        result_value = result;
    });
    
    Task task(TaskType::HOVER, TaskPriority::HIGH, "", []() {
        return json{{"contents", "hover text"}};
    });
    task.request_id = json(123);
    
    pool.submit(std::move(task));
    
    // Wait for execution
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    ASSERT(callback_called, "Callback should be called");
    ASSERT(result_id == json(123), "Correct request ID");
    ASSERT(result_value.contains("contents"), "Result passed to callback");
}

TEST_CASE(ThreadPool_respects_cancellation) {
    ThreadPool pool(1);  // Single worker to ensure ordering
    
    std::atomic<int> executed{0};
    
    // Submit a long-running task
    Task blocker(TaskType::OTHER, TaskPriority::NORMAL, "", [&executed]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        executed++;
        return nullptr;
    });
    pool.submit(std::move(blocker));
    
    // Submit a task and immediately cancel it
    Task task(TaskType::HOVER, TaskPriority::HIGH, "", [&executed]() {
        executed++;
        return json{{"result", "should not execute"}};
    });
    task.request_id = json(999);
    pool.submit(std::move(task));
    
    // Cancel immediately
    pool.cancel_request(json(999));
    
    // Wait for blocker to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Only blocker should have executed (cancelled task should be skipped)
    ASSERT(executed == 1, "Cancelled task should not execute");
}

TEST_CASE(ThreadPool_shutdown) {
    ThreadPool pool(4);
    
    std::atomic<int> counter{0};
    
    // Submit some tasks
    for (int i = 0; i < 5; i++) {
        Task task(TaskType::OTHER, TaskPriority::NORMAL, "", [&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            counter++;
            return nullptr;
        });
        pool.submit(std::move(task));
    }
    
    // Shutdown should wait for tasks to complete
    pool.shutdown();
    
    // All tasks should have executed
    ASSERT(counter == 5, "All tasks should complete before shutdown");
    ASSERT(!pool.is_running(), "Pool should not be running after shutdown");
}

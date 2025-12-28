// Standalone test for async runtime API
// No dependencies on test framework

#include "runtime/async/runtime_api.h"
#include "runtime/async/executor.h"
#include <iostream>
#include <cassert>

using namespace aria::runtime;

// Mock coroutine for testing
struct MockCoroState {
    int step;
    int64_t value;
    bool done;
};

// Override the runtime stub implementations for testing
// These will be used instead of the stubs in coroutine.cpp
extern "C" {
    void __aria_coro_resume(void* handle) {
        if (!handle) return;
        MockCoroState* state = static_cast<MockCoroState*>(handle);
        if (state->step == 0) {
            state->value = 42;
            state->step = 1;
            state->done = true;  // Complete immediately for simple test
        }
    }
    
    bool __aria_coro_done(void* handle) {
        return handle ? static_cast<MockCoroState*>(handle)->done : true;
    }
    
    void __aria_coro_destroy(void* handle) {
        if (handle) delete static_cast<MockCoroState*>(handle);
    }
}

int main() {
    std::cout << "=== Aria Async Runtime C API Test ===\n\n";
    
    int tests_passed = 0;
    int tests_failed = 0;
    
    // Test 1: Create executor
    std::cout << "Test 1: Create executor... ";
    AriaExecutorHandle exec = aria_executor_create();
    if (exec) {
        std::cout << "✓ PASS\n";
        tests_passed++;
    } else {
        std::cout << "✗ FAIL\n";
        tests_failed++;
    }
    
    // Test 2: Global executor
    std::cout << "Test 2: Global executor singleton... ";
    AriaExecutorHandle global1 = aria_get_global_executor();
    AriaExecutorHandle global2 = aria_get_global_executor();
    if (global1 && global1 == global2) {
        std::cout << "✓ PASS\n";
        tests_passed++;
    } else {
        std::cout << "✗ FAIL\n";
        tests_failed++;
    }
    
    // Test 3: Future operations
    std::cout << "Test 3: Future create/set/get... ";
    AriaFutureHandle fut = aria_future_create(sizeof(int64_t));
    int64_t value = 99;
    aria_future_set_value(fut, &value, sizeof(value));
    void* result = aria_future_get_value(fut);
    if (result && *static_cast<int64_t*>(result) == 99) {
        std::cout << "✓ PASS\n";
        tests_passed++;
    } else {
        std::cout << "✗ FAIL\n";
        tests_failed++;
    }
    aria_future_destroy(fut);
    
    // Test 4: Spawn task
    std::cout << "Test 4: Spawn task... ";
    MockCoroState* coro1 = new MockCoroState{0, 0, false};
    AriaTaskId taskId = aria_executor_spawn(exec, coro1);
    if (taskId > 0) {
        std::cout << "✓ PASS\n";
        tests_passed++;
    } else {
        std::cout << "✗ FAIL\n";
        tests_failed++;
    }
    
    // Test 5: Run executor  
    std::cout << "Test 5: Run executor to completion... ";
    aria_executor_run(exec);  // This will run the task we just spawned
    Executor* execPtr = static_cast<Executor*>(exec);
    std::cout << "(completed: " << execPtr->getTasksCompleted() << ") ";
    if (execPtr->getTasksCompleted() >= 1) {
        std::cout << "✓ PASS\n";
        tests_passed++;
    } else {
        std::cout << "✗ FAIL (executed: " << execPtr->getTasksExecuted() << ")\n";
        tests_failed++;
    }
    
    // Test 6: Coroutine intrinsics
    std::cout << "Test 6: Coroutine intrinsics... ";
    MockCoroState* test_coro = new MockCoroState{0, 0, false};
    bool initially_not_done = !aria_coro_done(test_coro);  // Should not be done initially
    aria_coro_resume(test_coro);
    bool after_resume_done = aria_coro_done(test_coro);     // Should be done after resume
    if (initially_not_done && after_resume_done) {
        std::cout << "✓ PASS\n";
        tests_passed++;
    } else {
        std::cout << "✗ FAIL (init=" << initially_not_done << ", after=" << after_resume_done << ")\n";
        tests_failed++;
    }
    aria_coro_destroy(test_coro);
    
    // Test 7: Memory operations
    std::cout << "Test 7: Memory alloc/free... ";
    void* mem = aria_coro_alloc(1024);
    if (mem) {
        aria_coro_free(mem);
        std::cout << "✓ PASS\n";
        tests_passed++;
    } else {
        std::cout << "✗ FAIL\n";
        tests_failed++;
    }
    
    // Cleanup
    aria_executor_destroy(exec);
    
    // Summary
    std::cout << "\n=== Test Summary ===\n";
    std::cout << "Passed: " << tests_passed << "\n";
    std::cout << "Failed: " << tests_failed << "\n";
    std::cout << "Total:  " << (tests_passed + tests_failed) << "\n";
    
    if (tests_failed == 0) {
        std::cout << "\n🎉 All tests passed!\n";
        return 0;
    } else {
        std::cout << "\n❌ Some tests failed\n";
        return 1;
    }
}

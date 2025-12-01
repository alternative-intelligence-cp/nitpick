#include <iostream>
#include <cstring>
#include <vector>
#include <cstdint>

// ============================================================================
// RAMP (Resource Allocation for Minimal Pause) Test
// ============================================================================

// Mock allocator
extern "C" void* aria_alloc_aligned(size_t size, size_t alignment) {
    // Use posix_memalign for aligned allocation
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, size) == 0) {
        return ptr;
    }
    return nullptr;
}

// RAMP structures (from ramp.cpp)
enum RampState {
    RAMP_COMPLETE,
    RAMP_PENDING
};

enum CoroState {
    CORO_RUNNING,
    CORO_SUSPENDED,
    CORO_COMPLETE
};

struct CoroutineFrame {
    void* resume_pc;
    void* data;
    CoroutineFrame* waiting_on;
    int state;
    char padding[4]; // Alignment padding
};

struct RampResult {
    RampState state;
    union {
        void* value;
        CoroutineFrame* coro;
    } payload;
};

// RAMP functions (from ramp.cpp)
extern "C" CoroutineFrame* __aria_ramp_promote(void* stack_vars, size_t size, void* instruction_ptr) {
    CoroutineFrame* heap_frame = (CoroutineFrame*)aria_alloc_aligned(
        sizeof(CoroutineFrame) + size, 64);

    if (heap_frame) {
        heap_frame->data = (char*)heap_frame + sizeof(CoroutineFrame);
        memcpy(heap_frame->data, stack_vars, size);
        heap_frame->resume_pc = instruction_ptr;
        heap_frame->state = CORO_SUSPENDED;
        heap_frame->waiting_on = nullptr;
    }

    return heap_frame;
}

extern "C" RampResult __aria_await(RampResult future, void* caller_stack,
                                    size_t caller_size, void* resume_pc) {
    // Fast Path: Child finished immediately
    if (future.state == RAMP_COMPLETE) {
        return future;
    }

    // Slow Path: Child is pending
    CoroutineFrame* caller_frame = __aria_ramp_promote(caller_stack, caller_size, resume_pc);

    caller_frame->waiting_on = future.payload.coro;

    RampResult res;
    res.state = RAMP_PENDING;
    res.payload.coro = caller_frame;
    return res;
}

// Test utilities
struct TestResult {
    std::string name;
    bool passed;
    std::string message;
};

std::vector<TestResult> results;

void report_test(const std::string& name, bool passed, const std::string& msg = "") {
    results.push_back({name, passed, msg});
    std::cout << (passed ? "[PASS] " : "[FAIL] ") << name;
    if (!msg.empty()) std::cout << " - " << msg;
    std::cout << std::endl;
}

// ============================================================================
// Test Cases
// ============================================================================

// Test 1: Promote stack frame to heap
void test_ramp_promote() {
    int stack_var = 42;
    void* fake_pc = (void*)0x1234;

    CoroutineFrame* frame = __aria_ramp_promote(&stack_var, sizeof(stack_var), fake_pc);

    bool allocated = (frame != nullptr);
    bool pc_set = (frame->resume_pc == fake_pc);
    bool state_set = (frame->state == CORO_SUSPENDED);
    bool data_copied = (*(int*)frame->data == 42);

    bool success = allocated && pc_set && state_set && data_copied;

    report_test("ramp_promote", success,
                success ? "" : "Frame promotion failed");

    if (frame) free(frame);
}

// Test 2: Await on completed future (fast path)
void test_await_complete() {
    RampResult future;
    future.state = RAMP_COMPLETE;
    future.payload.value = (void*)0xDEADBEEF;

    int caller_stack = 100;
    void* resume_pc = (void*)0x5678;

    RampResult result = __aria_await(future, &caller_stack, sizeof(caller_stack), resume_pc);

    bool is_complete = (result.state == RAMP_COMPLETE);
    bool value_preserved = (result.payload.value == (void*)0xDEADBEEF);

    bool success = is_complete && value_preserved;

    report_test("await_complete", success,
                success ? "" : "Await fast path failed");
}

// Test 3: Await on pending future (slow path)
void test_await_pending() {
    // Create a pending child coroutine
    CoroutineFrame* child_coro = (CoroutineFrame*)aria_alloc_aligned(sizeof(CoroutineFrame), 64);
    child_coro->state = CORO_SUSPENDED;
    child_coro->resume_pc = (void*)0x9999;
    child_coro->waiting_on = nullptr;

    RampResult future;
    future.state = RAMP_PENDING;
    future.payload.coro = child_coro;

    int caller_stack = 200;
    void* resume_pc = (void*)0xAAAA;

    RampResult result = __aria_await(future, &caller_stack, sizeof(caller_stack), resume_pc);

    bool is_pending = (result.state == RAMP_PENDING);
    bool has_frame = (result.payload.coro != nullptr);
    bool linked = (result.payload.coro->waiting_on == child_coro);

    bool success = is_pending && has_frame && linked;

    report_test("await_pending", success,
                success ? "" : "Await slow path failed");

    if (result.payload.coro) free(result.payload.coro);
    free(child_coro);
}

// Test 4: Frame alignment
void test_frame_alignment() {
    int stack_var = 123;
    CoroutineFrame* frame = __aria_ramp_promote(&stack_var, sizeof(stack_var), nullptr);

    // Check 64-byte alignment
    bool aligned = ((uintptr_t)frame % 64 == 0);

    bool success = aligned && (frame != nullptr);

    report_test("frame_alignment", success,
                success ? "" : "Frame not 64-byte aligned");

    if (frame) free(frame);
}

// Test 5: Multiple promotions
void test_multiple_promotions() {
    std::vector<CoroutineFrame*> frames;

    for (int i = 0; i < 10; i++) {
        int stack_var = i * 10;
        CoroutineFrame* frame = __aria_ramp_promote(&stack_var, sizeof(stack_var), nullptr);
        if (frame) {
            frames.push_back(frame);
        }
    }

    bool all_allocated = (frames.size() == 10);
    bool data_correct = true;

    for (size_t i = 0; i < frames.size(); i++) {
        if (*(int*)frames[i]->data != (int)(i * 10)) {
            data_correct = false;
            break;
        }
    }

    bool success = all_allocated && data_correct;

    report_test("multiple_promotions", success,
                success ? "" : "Multiple promotions failed");

    for (auto frame : frames) {
        free(frame);
    }
}

// Test 6: Large stack frame promotion
void test_large_frame_promotion() {
    const size_t large_size = 4096;
    char* large_stack = new char[large_size];
    memset(large_stack, 0xAB, large_size);

    CoroutineFrame* frame = __aria_ramp_promote(large_stack, large_size, nullptr);

    bool allocated = (frame != nullptr);
    bool data_copied = true;

    if (frame) {
        char* data = (char*)frame->data;
        for (size_t i = 0; i < large_size; i++) {
            if (data[i] != (char)0xAB) {
                data_copied = false;
                break;
            }
        }
    }

    bool success = allocated && data_copied;

    report_test("large_frame_promotion", success,
                success ? "" : "Large frame promotion failed");

    delete[] large_stack;
    if (frame) free(frame);
}

// Test 7: Chained awaits
void test_chained_awaits() {
    // Create chain: caller -> middle -> child
    CoroutineFrame* child = (CoroutineFrame*)aria_alloc_aligned(sizeof(CoroutineFrame), 64);
    child->state = CORO_SUSPENDED;
    child->waiting_on = nullptr;

    RampResult child_result;
    child_result.state = RAMP_PENDING;
    child_result.payload.coro = child;

    int middle_stack = 1;
    RampResult middle_result = __aria_await(child_result, &middle_stack, sizeof(middle_stack), nullptr);

    int caller_stack = 2;
    RampResult caller_result = __aria_await(middle_result, &caller_stack, sizeof(caller_stack), nullptr);

    bool chain_valid = (caller_result.state == RAMP_PENDING) &&
                       (caller_result.payload.coro->waiting_on == middle_result.payload.coro) &&
                       (middle_result.payload.coro->waiting_on == child);

    bool success = chain_valid;

    report_test("chained_awaits", success,
                success ? "" : "Chained awaits failed");

    if (caller_result.payload.coro) free(caller_result.payload.coro);
    if (middle_result.payload.coro) free(middle_result.payload.coro);
    free(child);
}

// Test 8: Coroutine state transitions
void test_state_transitions() {
    CoroutineFrame* frame = (CoroutineFrame*)aria_alloc_aligned(sizeof(CoroutineFrame), 64);

    frame->state = CORO_RUNNING;
    bool running_ok = (frame->state == CORO_RUNNING);

    frame->state = CORO_SUSPENDED;
    bool suspended_ok = (frame->state == CORO_SUSPENDED);

    frame->state = CORO_COMPLETE;
    bool complete_ok = (frame->state == CORO_COMPLETE);

    bool success = running_ok && suspended_ok && complete_ok;

    report_test("state_transitions", success,
                success ? "" : "State transitions failed");

    free(frame);
}

// Test 9: Resume PC preservation
void test_resume_pc_preservation() {
    void* expected_pc = (void*)0xC0FFEE;
    int stack_var = 999;

    CoroutineFrame* frame = __aria_ramp_promote(&stack_var, sizeof(stack_var), expected_pc);

    bool pc_preserved = (frame->resume_pc == expected_pc);

    bool success = pc_preserved;

    report_test("resume_pc_preservation", success,
                success ? "" : "Resume PC not preserved");

    if (frame) free(frame);
}

// Test 10: Null waiting_on initialization
void test_null_waiting_on() {
    int stack_var = 777;
    CoroutineFrame* frame = __aria_ramp_promote(&stack_var, sizeof(stack_var), nullptr);

    bool waiting_on_null = (frame->waiting_on == nullptr);

    bool success = waiting_on_null;

    report_test("null_waiting_on", success,
                success ? "" : "waiting_on not initialized to null");

    if (frame) free(frame);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Aria RAMP Optimization Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    test_ramp_promote();
    test_await_complete();
    test_await_pending();
    test_frame_alignment();
    test_multiple_promotions();
    test_large_frame_promotion();
    test_chained_awaits();
    test_state_transitions();
    test_resume_pc_preservation();
    test_null_waiting_on();

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;

    int passed = 0, failed = 0;
    for (const auto& result : results) {
        if (result.passed) passed++;
        else failed++;
    }

    std::cout << "Total Tests: " << results.size() << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << std::endl;

    return (failed == 0) ? 0 : 1;
}

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include <thread>

// Import the allocator functions
extern "C" {
    void* aria_alloc(size_t size);
    void aria_free(void* ptr);
    void* aria_realloc(void* ptr, size_t size);
    void* aria_alloc_aligned(size_t size, size_t alignment);
}

// Test Result Tracking
struct TestResult {
    const char* name;
    bool passed;
    const char* message;
};

std::vector<TestResult> results;

void report_test(const char* name, bool passed, const char* message = "") {
    results.push_back({name, passed, message});
    if (passed) {
        std::cout << "[PASS] " << name << std::endl;
    } else {
        std::cout << "[FAIL] " << name << ": " << message << std::endl;
    }
}

// Test 1: Basic Allocation and Deallocation
void test_basic_allocation() {
    void* ptr = aria_alloc(1024);

    if (ptr == nullptr) {
        report_test("basic_allocation", false, "aria_alloc returned NULL");
        return;
    }

    // Write some data to verify the memory is writable
    memset(ptr, 0xAA, 1024);

    // Verify we can read it back
    unsigned char* bytes = (unsigned char*)ptr;
    bool data_valid = true;
    for (int i = 0; i < 1024; i++) {
        if (bytes[i] != 0xAA) {
            data_valid = false;
            break;
        }
    }

    aria_free(ptr);

    report_test("basic_allocation", data_valid,
                data_valid ? "" : "Memory corruption detected");
}

// Test 2: Zero-Size Allocation
void test_zero_allocation() {
    void* ptr = aria_alloc(0);
    // Mimalloc typically returns a valid pointer for zero-size allocations
    // This is implementation-defined behavior
    bool valid = (ptr != nullptr);
    if (ptr) aria_free(ptr);
    report_test("zero_allocation", valid, "Zero-size allocation returned NULL");
}

// Test 3: Large Allocation
void test_large_allocation() {
    const size_t large_size = 10 * 1024 * 1024; // 10 MB
    void* ptr = aria_alloc(large_size);

    if (ptr == nullptr) {
        report_test("large_allocation", false, "Failed to allocate 10MB");
        return;
    }

    // Write to first and last pages to ensure it's all valid
    unsigned char* bytes = (unsigned char*)ptr;
    bytes[0] = 0xFF;
    bytes[large_size - 1] = 0xFF;

    bool valid = (bytes[0] == 0xFF && bytes[large_size - 1] == 0xFF);

    aria_free(ptr);
    report_test("large_allocation", valid, "Large allocation memory access failed");
}

// Test 4: Multiple Allocations
void test_multiple_allocations() {
    const int num_allocs = 100;
    void* ptrs[num_allocs];

    // Allocate
    for (int i = 0; i < num_allocs; i++) {
        ptrs[i] = aria_alloc(256 + i * 8); // Variable sizes
        if (ptrs[i] == nullptr) {
            report_test("multiple_allocations", false, "Allocation failed in loop");
            // Clean up what we allocated
            for (int j = 0; j < i; j++) {
                aria_free(ptrs[j]);
            }
            return;
        }
    }

    // Verify all pointers are unique
    bool all_unique = true;
    for (int i = 0; i < num_allocs && all_unique; i++) {
        for (int j = i + 1; j < num_allocs; j++) {
            if (ptrs[i] == ptrs[j]) {
                all_unique = false;
                break;
            }
        }
    }

    // Free all
    for (int i = 0; i < num_allocs; i++) {
        aria_free(ptrs[i]);
    }

    report_test("multiple_allocations", all_unique,
                all_unique ? "" : "Duplicate pointers detected");
}

// Test 5: Reallocation
void test_reallocation() {
    void* ptr = aria_alloc(64);
    if (ptr == nullptr) {
        report_test("reallocation", false, "Initial allocation failed");
        return;
    }

    // Write a pattern
    memset(ptr, 0xBB, 64);

    // Grow the allocation
    void* new_ptr = aria_realloc(ptr, 256);
    if (new_ptr == nullptr) {
        aria_free(ptr);
        report_test("reallocation", false, "Realloc failed");
        return;
    }

    // Verify original data is preserved
    unsigned char* bytes = (unsigned char*)new_ptr;
    bool data_preserved = true;
    for (int i = 0; i < 64; i++) {
        if (bytes[i] != 0xBB) {
            data_preserved = false;
            break;
        }
    }

    aria_free(new_ptr);
    report_test("reallocation", data_preserved,
                data_preserved ? "" : "Data not preserved after realloc");
}

// Test 6: Aligned Allocation
void test_aligned_allocation() {
    const size_t alignment = 64; // Cache line / AVX-512 register alignment
    void* ptr = aria_alloc_aligned(256, alignment);

    if (ptr == nullptr) {
        report_test("aligned_allocation", false, "Aligned allocation failed");
        return;
    }

    // Check alignment
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    bool is_aligned = (addr % alignment) == 0;

    // Verify we can write to it
    if (is_aligned) {
        memset(ptr, 0xCC, 256);
    }

    aria_free(ptr);
    report_test("aligned_allocation", is_aligned,
                is_aligned ? "" : "Pointer not properly aligned");
}

// Test 7: Thread Safety (Multi-threaded Allocation)
void test_thread_safety() {
    const int num_threads = 4;
    const int allocs_per_thread = 100;
    std::atomic<bool> all_succeeded{true};

    auto thread_func = [&]() {
        for (int i = 0; i < allocs_per_thread; i++) {
            void* ptr = aria_alloc(128);
            if (ptr == nullptr) {
                all_succeeded = false;
                return;
            }
            // Write some data
            memset(ptr, i & 0xFF, 128);
            aria_free(ptr);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(thread_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    report_test("thread_safety", all_succeeded,
                all_succeeded ? "" : "Thread-safe allocation failed");
}

// Test 8: Double Free Detection (Should not crash)
void test_double_free_safety() {
    void* ptr = aria_alloc(128);
    if (ptr == nullptr) {
        report_test("double_free_safety", false, "Initial allocation failed");
        return;
    }

    aria_free(ptr);

    // This should ideally be safe with mimalloc (it has guards)
    // We're testing that it doesn't crash
    try {
        aria_free(ptr); // Double free
        report_test("double_free_safety", true, "Mimalloc handled double-free gracefully");
    } catch (...) {
        report_test("double_free_safety", false, "Double-free caused exception");
    }
}

// Main Test Runner
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Aria Memory Allocator Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    test_basic_allocation();
    test_zero_allocation();
    test_large_allocation();
    test_multiple_allocations();
    test_reallocation();
    test_aligned_allocation();
    test_thread_safety();
    test_double_free_safety();

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

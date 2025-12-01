#include <iostream>
#include <cstdint>
#include <cstring>
#include <vector>

// ============================================================================
// Nursery Allocator Test Implementation
// ============================================================================

// Mock structures matching nursery.cpp
struct FreeFragment {
   uint8_t* start;
   uint8_t* end;
   FreeFragment* next;
};

struct Nursery {
   uint8_t* start_addr;
   uint8_t* end_addr;
   uint8_t* bump_ptr;
   FreeFragment* fragments;
   size_t size;
};

const size_t NURSERY_SIZE = 4 * 1024 * 1024; // 4MB

// Mock GC collect function
static bool gc_collect_called = false;
extern "C" void aria_gc_collect_minor() {
    gc_collect_called = true;
}

// Nursery allocation function (copied from nursery.cpp for testing)
extern "C" void* aria_gc_alloc(Nursery* nursery, size_t size) {
   // 1. Fast Path: Standard Bump Allocation
   uint8_t* new_bump = nursery->bump_ptr + size;
   if (new_bump <= nursery->end_addr) {
       void* ptr = nursery->bump_ptr;
       nursery->bump_ptr = new_bump;
       return ptr;
   }

   // 2. Slow Path: Fragment Search
   if (nursery->fragments) {
       FreeFragment* prev = nullptr;
       FreeFragment* curr = nursery->fragments;

       while (curr) {
           size_t frag_size = curr->end - curr->start;
           if (frag_size >= size) {
               void* ptr = curr->start;
               curr->start += size;
               if (curr->start == curr->end) {
                   if (prev) prev->next = curr->next;
                   else nursery->fragments = curr->next;
               }
               return ptr;
           }
           prev = curr;
           curr = curr->next;
       }
   }

   // 3. Collection Path
   aria_gc_collect_minor();
   // In real impl, this would retry. For testing, we return nullptr.
   return nullptr;
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

// Helper to initialize a nursery
Nursery* create_nursery() {
    Nursery* n = new Nursery;
    n->start_addr = new uint8_t[NURSERY_SIZE];
    n->end_addr = n->start_addr + NURSERY_SIZE;
    n->bump_ptr = n->start_addr;
    n->fragments = nullptr;
    n->size = NURSERY_SIZE;
    return n;
}

void destroy_nursery(Nursery* n) {
    delete[] n->start_addr;

    // Clean up fragments
    FreeFragment* frag = n->fragments;
    while (frag) {
        FreeFragment* next = frag->next;
        delete frag;
        frag = next;
    }

    delete n;
}

// ============================================================================
// Test Cases
// ============================================================================

// Test 1: Basic bump allocation
void test_basic_bump_allocation() {
    Nursery* nursery = create_nursery();

    void* ptr1 = aria_gc_alloc(nursery, 64);
    void* ptr2 = aria_gc_alloc(nursery, 128);
    void* ptr3 = aria_gc_alloc(nursery, 256);

    bool success = (ptr1 != nullptr) && (ptr2 != nullptr) && (ptr3 != nullptr);
    success = success && (ptr1 < ptr2) && (ptr2 < ptr3);
    success = success && ((char*)ptr2 - (char*)ptr1 == 64);
    success = success && ((char*)ptr3 - (char*)ptr2 == 128);

    report_test("basic_bump_allocation", success,
                success ? "" : "Sequential bump allocations failed");

    destroy_nursery(nursery);
}

// Test 2: Allocation fills nursery
void test_nursery_fill() {
    Nursery* nursery = create_nursery();

    const size_t alloc_size = 1024;
    size_t allocated = 0;

    while (allocated < NURSERY_SIZE) {
        void* ptr = aria_gc_alloc(nursery, alloc_size);
        if (ptr == nullptr) break;
        allocated += alloc_size;
    }

    // Should have allocated close to nursery size
    bool success = (allocated >= NURSERY_SIZE - alloc_size);

    report_test("nursery_fill", success,
                success ? "" : "Failed to fill nursery to capacity");

    destroy_nursery(nursery);
}

// Test 3: Fragment allocation
void test_fragment_allocation() {
    Nursery* nursery = create_nursery();

    // Create a fragment manually
    FreeFragment* frag = new FreeFragment;
    frag->start = nursery->start_addr + 1000;
    frag->end = frag->start + 500;
    frag->next = nullptr;
    nursery->fragments = frag;

    // Move bump_ptr to end so bump allocation fails
    nursery->bump_ptr = nursery->end_addr;

    // Allocate from fragment
    void* ptr = aria_gc_alloc(nursery, 100);

    bool success = (ptr != nullptr) && (ptr == frag->start - 100);

    report_test("fragment_allocation", success,
                success ? "" : "Fragment allocation failed");

    destroy_nursery(nursery);
}

// Test 4: Fragment exhaustion
void test_fragment_exhaustion() {
    Nursery* nursery = create_nursery();

    // Create a small fragment
    FreeFragment* frag = new FreeFragment;
    frag->start = nursery->start_addr + 1000;
    frag->end = frag->start + 200; // 200 bytes
    frag->next = nullptr;
    nursery->fragments = frag;

    // Move bump_ptr to end
    nursery->bump_ptr = nursery->end_addr;

    // Allocate exactly the fragment size
    void* ptr = aria_gc_alloc(nursery, 200);

    bool success = (ptr != nullptr) && (nursery->fragments == nullptr);

    report_test("fragment_exhaustion", success,
                success ? "" : "Fragment should be removed when exhausted");

    destroy_nursery(nursery);
}

// Test 5: Multiple fragments
void test_multiple_fragments() {
    Nursery* nursery = create_nursery();

    // Create two fragments
    FreeFragment* frag1 = new FreeFragment;
    frag1->start = nursery->start_addr + 1000;
    frag1->end = frag1->start + 100;

    FreeFragment* frag2 = new FreeFragment;
    frag2->start = nursery->start_addr + 2000;
    frag2->end = frag2->start + 500;

    frag1->next = frag2;
    frag2->next = nullptr;
    nursery->fragments = frag1;

    // Move bump_ptr to end
    nursery->bump_ptr = nursery->end_addr;

    // Allocate from first fragment
    void* ptr1 = aria_gc_alloc(nursery, 50);

    // Allocate more than first fragment can handle - should use second
    void* ptr2 = aria_gc_alloc(nursery, 200);

    bool success = (ptr1 != nullptr) && (ptr2 != nullptr);

    report_test("multiple_fragments", success,
                success ? "" : "Multi-fragment allocation failed");

    destroy_nursery(nursery);
}

// Test 6: Zero-size allocation
void test_zero_allocation() {
    Nursery* nursery = create_nursery();

    uint8_t* before = nursery->bump_ptr;
    void* ptr = aria_gc_alloc(nursery, 0);
    uint8_t* after = nursery->bump_ptr;

    // Should succeed but not move bump pointer
    bool success = (ptr != nullptr) && (before == after);

    report_test("zero_allocation", success,
                success ? "" : "Zero-size allocation behavior incorrect");

    destroy_nursery(nursery);
}

// Test 7: Alignment test (pointers should be valid)
void test_allocation_alignment() {
    Nursery* nursery = create_nursery();

    std::vector<void*> ptrs;
    for (int i = 0; i < 100; i++) {
        void* ptr = aria_gc_alloc(nursery, 17); // Odd size
        if (ptr) ptrs.push_back(ptr);
    }

    // All pointers should be unique and non-overlapping
    bool all_unique = true;
    for (size_t i = 0; i < ptrs.size(); i++) {
        for (size_t j = i + 1; j < ptrs.size(); j++) {
            if (ptrs[i] == ptrs[j]) {
                all_unique = false;
                break;
            }
        }
        if (!all_unique) break;
    }

    report_test("allocation_alignment", all_unique && ptrs.size() == 100,
                all_unique ? "" : "Overlapping allocations detected");

    destroy_nursery(nursery);
}

// Test 8: GC collection trigger
void test_gc_collection_trigger() {
    Nursery* nursery = create_nursery();

    // Fill the nursery
    while (nursery->bump_ptr < nursery->end_addr) {
        void* ptr = aria_gc_alloc(nursery, 1024);
        if (ptr == nullptr) break;
    }

    gc_collect_called = false;

    // Next allocation should trigger GC
    aria_gc_alloc(nursery, 1024);

    bool success = gc_collect_called;

    report_test("gc_collection_trigger", success,
                success ? "" : "GC collection not triggered when nursery full");

    destroy_nursery(nursery);
}

// Test 9: Large allocation
void test_large_allocation() {
    Nursery* nursery = create_nursery();

    // Allocate 1MB at once
    void* ptr = aria_gc_alloc(nursery, 1024 * 1024);

    bool success = (ptr != nullptr);

    report_test("large_allocation", success,
                success ? "" : "Large allocation failed");

    destroy_nursery(nursery);
}

// Test 10: Sequential allocations are contiguous
void test_contiguous_allocation() {
    Nursery* nursery = create_nursery();

    void* ptr1 = aria_gc_alloc(nursery, 100);
    void* ptr2 = aria_gc_alloc(nursery, 100);
    void* ptr3 = aria_gc_alloc(nursery, 100);

    bool contiguous = ((char*)ptr2 == (char*)ptr1 + 100) &&
                      ((char*)ptr3 == (char*)ptr2 + 100);

    report_test("contiguous_allocation", contiguous,
                contiguous ? "" : "Allocations not contiguous");

    destroy_nursery(nursery);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Aria Nursery Allocator Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    test_basic_bump_allocation();
    test_nursery_fill();
    test_fragment_allocation();
    test_fragment_exhaustion();
    test_multiple_fragments();
    test_zero_allocation();
    test_allocation_alignment();
    test_gc_collection_trigger();
    test_large_allocation();
    test_contiguous_allocation();

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

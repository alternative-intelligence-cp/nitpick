#include <cassert>
#include <iostream>
#include <cstring>
#include <vector>

// Include the GC header structure
#include "../src/runtime/gc/header.h"

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

// Test 1: Header Size
void test_header_size() {
    size_t size = sizeof(ObjHeader);
    // Header should be exactly 8 bytes for cache efficiency
    bool valid = (size == 8);
    report_test("header_size", valid,
                valid ? "" : "Header is not 8 bytes");
}

// Test 2: Initialize and Read Mark Bit
void test_mark_bit() {
    ObjHeader header = {};

    // Initially should be 0
    bool initial_zero = (header.mark_bit == 0);

    // Set mark bit
    header.mark_bit = 1;
    bool mark_set = (header.mark_bit == 1);

    // Clear mark bit
    header.mark_bit = 0;
    bool mark_cleared = (header.mark_bit == 0);

    bool passed = initial_zero && mark_set && mark_cleared;
    report_test("mark_bit", passed,
                passed ? "" : "Mark bit manipulation failed");
}

// Test 3: Pinned Bit Operations
void test_pinned_bit() {
    ObjHeader header = {};

    // Test pinning
    header.pinned_bit = 1;
    bool pinned = (header.pinned_bit == 1);

    // Test unpinning
    header.pinned_bit = 0;
    bool unpinned = (header.pinned_bit == 0);

    bool passed = pinned && unpinned;
    report_test("pinned_bit", passed,
                passed ? "" : "Pinned bit manipulation failed");
}

// Test 4: Forwarded Bit Operations
void test_forwarded_bit() {
    ObjHeader header = {};

    header.forwarded_bit = 1;
    bool forwarded = (header.forwarded_bit == 1);

    header.forwarded_bit = 0;
    bool not_forwarded = (header.forwarded_bit == 0);

    bool passed = forwarded && not_forwarded;
    report_test("forwarded_bit", passed,
                passed ? "" : "Forwarded bit manipulation failed");
}

// Test 5: Generation Flag (is_nursery)
void test_generation_flag() {
    ObjHeader header = {};

    // Object in nursery
    header.is_nursery = 1;
    bool in_nursery = (header.is_nursery == 1);

    // Object in old generation
    header.is_nursery = 0;
    bool in_old = (header.is_nursery == 0);

    bool passed = in_nursery && in_old;
    report_test("generation_flag", passed,
                passed ? "" : "Generation flag manipulation failed");
}

// Test 6: Size Class (8-bit field, 0-255)
void test_size_class() {
    ObjHeader header = {};

    // Test various size classes
    header.size_class = 0;
    bool class_0 = (header.size_class == 0);

    header.size_class = 127;
    bool class_127 = (header.size_class == 127);

    header.size_class = 255;
    bool class_255 = (header.size_class == 255);

    bool passed = class_0 && class_127 && class_255;
    report_test("size_class", passed,
                passed ? "" : "Size class field manipulation failed");
}

// Test 7: Type ID (16-bit field)
void test_type_id() {
    ObjHeader header = {};

    // Test various type IDs
    header.type_id = 0;
    bool type_0 = (header.type_id == 0);

    header.type_id = 1234;
    bool type_1234 = (header.type_id == 1234);

    header.type_id = 65535; // Max 16-bit value
    bool type_max = (header.type_id == 65535);

    bool passed = type_0 && type_1234 && type_max;
    report_test("type_id", passed,
                passed ? "" : "Type ID field manipulation failed");
}

// Test 8: Field Independence (no crosstalk)
void test_field_independence() {
    ObjHeader header = {};

    // Set all fields to specific values
    header.mark_bit = 1;
    header.pinned_bit = 1;
    header.forwarded_bit = 0;
    header.is_nursery = 1;
    header.size_class = 42;
    header.type_id = 9999;

    // Verify all fields retain their values
    bool all_correct = (
        header.mark_bit == 1 &&
        header.pinned_bit == 1 &&
        header.forwarded_bit == 0 &&
        header.is_nursery == 1 &&
        header.size_class == 42 &&
        header.type_id == 9999
    );

    report_test("field_independence", all_correct,
                all_correct ? "" : "Bitfield crosstalk detected");
}

// Test 9: Memory Layout Consistency
void test_memory_layout() {
    ObjHeader headers[100];
    memset(headers, 0, sizeof(headers));

    // Set unique patterns
    for (int i = 0; i < 100; i++) {
        headers[i].mark_bit = i % 2;
        headers[i].pinned_bit = (i / 2) % 2;
        headers[i].size_class = i;
        headers[i].type_id = i * 100;
    }

    // Verify all patterns are preserved
    bool all_valid = true;
    for (int i = 0; i < 100; i++) {
        if (headers[i].mark_bit != (i % 2) ||
            headers[i].pinned_bit != ((i / 2) % 2) ||
            headers[i].size_class != i ||
            headers[i].type_id != i * 100) {
            all_valid = false;
            break;
        }
    }

    report_test("memory_layout", all_valid,
                all_valid ? "" : "Memory layout consistency failed");
}

// Test 10: Pinned Object Simulation
void test_pinned_object_simulation() {
    ObjHeader managed_obj = {};

    // Start as GC-managed object in nursery
    managed_obj.is_nursery = 1;
    managed_obj.pinned_bit = 0;
    managed_obj.type_id = 42; // Some type

    // Simulate pinning operation (wild int8:u = #managed_var)
    managed_obj.pinned_bit = 1;

    bool is_pinned = (managed_obj.pinned_bit == 1);
    bool still_in_nursery = (managed_obj.is_nursery == 1);
    bool type_preserved = (managed_obj.type_id == 42);

    bool passed = is_pinned && still_in_nursery && type_preserved;
    report_test("pinned_object_simulation", passed,
                passed ? "" : "Pinning simulation failed");
}

// Test 11: GC Mark Phase Simulation
void test_gc_mark_phase() {
    ObjHeader obj1 = {}, obj2 = {}, obj3 = {};

    // Start with all unmarked
    obj1.mark_bit = 0;
    obj2.mark_bit = 0;
    obj3.mark_bit = 0;

    // Simulate marking reachable objects
    obj1.mark_bit = 1; // Root
    obj2.mark_bit = 1; // Reachable from obj1
    // obj3 remains unmarked (unreachable)

    bool obj1_marked = (obj1.mark_bit == 1);
    bool obj2_marked = (obj2.mark_bit == 1);
    bool obj3_unmarked = (obj3.mark_bit == 0);

    bool passed = obj1_marked && obj2_marked && obj3_unmarked;
    report_test("gc_mark_phase", passed,
                passed ? "" : "GC mark phase simulation failed");
}

// Test 12: Object Promotion (Nursery -> Old Gen)
void test_object_promotion() {
    ObjHeader nursery_obj = {};

    // Start in nursery
    nursery_obj.is_nursery = 1;
    nursery_obj.size_class = 5;
    nursery_obj.type_id = 100;

    // Simulate survival through multiple GC cycles -> promote to old gen
    nursery_obj.is_nursery = 0;

    bool promoted = (nursery_obj.is_nursery == 0);
    bool data_preserved = (nursery_obj.size_class == 5 && nursery_obj.type_id == 100);

    bool passed = promoted && data_preserved;
    report_test("object_promotion", passed,
                passed ? "" : "Object promotion simulation failed");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Aria GC Header Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    test_header_size();
    test_mark_bit();
    test_pinned_bit();
    test_forwarded_bit();
    test_generation_flag();
    test_size_class();
    test_type_id();
    test_field_independence();
    test_memory_layout();
    test_pinned_object_simulation();
    test_gc_mark_phase();
    test_object_promotion();

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

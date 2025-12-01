#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>

// ============================================================================
// GC Implementation Test
// ============================================================================

// Type IDs for testing
#define TYPE_INT 0
#define TYPE_ARRAY_OBJ 1
#define TYPE_STRUCT 2
#define TYPE_TRIT 3

// ObjHeader structure (from header.h)
struct ObjHeader {
    uint64_t mark_bit : 1;
    uint64_t pinned_bit : 1;
    uint64_t forwarded_bit : 1;
    uint64_t is_nursery : 1;
    uint64_t size_class : 8;
    uint64_t type_id : 16;
    uint64_t padding : 36;
};

// Mock global old generation
std::vector<ObjHeader*> old_gen_objects;

// Mock root retrieval function
std::vector<void*> mock_roots;
std::vector<void*> get_thread_roots() {
    return mock_roots;
}

// Mark function implementation (from gc_impl.cpp)
void mark_object(ObjHeader* obj) {
    if (!obj || obj->mark_bit) return;

    // Mark self
    obj->mark_bit = 1;

    // Scan children based on type
    switch (obj->type_id) {
        case TYPE_ARRAY_OBJ: {
            void** data = (void**)((char*)obj + sizeof(ObjHeader));
            size_t count = obj->size_class;
            for (size_t i = 0; i < count; i++) {
                if (data[i]) {
                    mark_object((ObjHeader*)((char*)data[i] - sizeof(ObjHeader)));
                }
            }
            break;
        }
        case TYPE_STRUCT:
            // Simplified - no children to mark
            break;
        default:
            break;
    }
}

// Major GC implementation (from gc_impl.cpp)
void aria_gc_collect_major() {
    // 1. Mark Phase
    auto roots = get_thread_roots();
    for (void* root : roots) {
        if (root) {
            mark_object((ObjHeader*)((char*)root - sizeof(ObjHeader)));
        }
    }

    // 2. Sweep Phase
    auto it = old_gen_objects.begin();
    while (it != old_gen_objects.end()) {
        ObjHeader* obj = *it;
        if (obj->mark_bit) {
            // Live: Reset mark bit
            obj->mark_bit = 0;
            ++it;
        } else {
            // Dead: Reclaim
            free(obj);
            *it = old_gen_objects.back();
            old_gen_objects.pop_back();
        }
    }
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

// Helper to create a test object
ObjHeader* create_object(uint16_t type_id, uint8_t size_class, bool is_nursery = false) {
    size_t alloc_size = sizeof(ObjHeader) + size_class;
    ObjHeader* obj = (ObjHeader*)malloc(alloc_size);
    memset(obj, 0, alloc_size);

    obj->type_id = type_id;
    obj->size_class = size_class;
    obj->is_nursery = is_nursery ? 1 : 0;
    obj->mark_bit = 0;
    obj->pinned_bit = 0;

    return obj;
}

// ============================================================================
// Test Cases
// ============================================================================

// Test 1: Mark single object
void test_mark_single_object() {
    ObjHeader* obj = create_object(TYPE_INT, 8);

    mark_object(obj);

    bool success = (obj->mark_bit == 1);

    report_test("mark_single_object", success,
                success ? "" : "Object not marked");

    free(obj);
}

// Test 2: Mark prevents double marking
void test_mark_idempotent() {
    ObjHeader* obj = create_object(TYPE_INT, 8);

    mark_object(obj);
    uint64_t mark_before = obj->mark_bit;
    mark_object(obj);  // Should return early (already marked)
    uint64_t mark_after = obj->mark_bit;

    // Should remain marked (1) after second call
    bool success = (mark_before == 1) && (mark_after == 1);

    report_test("mark_idempotent", success,
                success ? "" : "Mark not idempotent");

    free(obj);
}

// Test 3: Sweep removes unmarked objects
void test_sweep_unmarked() {
    old_gen_objects.clear();
    mock_roots.clear();

    // Create objects
    ObjHeader* obj1 = create_object(TYPE_INT, 8);
    ObjHeader* obj2 = create_object(TYPE_INT, 8);
    ObjHeader* obj3 = create_object(TYPE_INT, 8);

    old_gen_objects.push_back(obj1);
    old_gen_objects.push_back(obj2);
    old_gen_objects.push_back(obj3);

    // Only obj2 is rooted
    mock_roots.push_back((char*)obj2 + sizeof(ObjHeader));

    // Run GC
    aria_gc_collect_major();

    // Should only have obj2 left
    bool success = (old_gen_objects.size() == 1) &&
                   (old_gen_objects[0] == obj2);

    report_test("sweep_unmarked", success,
                success ? "" : "Sweep didn't remove unmarked objects");

    // Cleanup
    for (auto obj : old_gen_objects) {
        free(obj);
    }
    old_gen_objects.clear();
    mock_roots.clear();
}

// Test 4: Multiple roots all survive
void test_multiple_roots() {
    old_gen_objects.clear();
    mock_roots.clear();

    ObjHeader* obj1 = create_object(TYPE_INT, 8);
    ObjHeader* obj2 = create_object(TYPE_INT, 8);
    ObjHeader* obj3 = create_object(TYPE_INT, 8);

    old_gen_objects.push_back(obj1);
    old_gen_objects.push_back(obj2);
    old_gen_objects.push_back(obj3);

    // All three are rooted
    mock_roots.push_back((char*)obj1 + sizeof(ObjHeader));
    mock_roots.push_back((char*)obj2 + sizeof(ObjHeader));
    mock_roots.push_back((char*)obj3 + sizeof(ObjHeader));

    aria_gc_collect_major();

    bool success = (old_gen_objects.size() == 3);

    report_test("multiple_roots", success,
                success ? "" : "Not all rooted objects survived");

    for (auto obj : old_gen_objects) {
        free(obj);
    }
    old_gen_objects.clear();
    mock_roots.clear();
}

// Test 5: Transitive marking (object graph)
void test_transitive_marking() {
    old_gen_objects.clear();
    mock_roots.clear();

    // Create array object that references another object
    ObjHeader* child = create_object(TYPE_INT, 8);
    ObjHeader* parent = create_object(TYPE_ARRAY_OBJ, sizeof(void*)); // Array with 1 element

    // Set parent to reference child
    void** parent_data = (void**)((char*)parent + sizeof(ObjHeader));
    parent_data[0] = (char*)child + sizeof(ObjHeader);
    parent->size_class = 1; // 1 element

    old_gen_objects.push_back(parent);
    old_gen_objects.push_back(child);

    // Only root the parent
    mock_roots.push_back((char*)parent + sizeof(ObjHeader));

    aria_gc_collect_major();

    // Both should survive due to transitive marking
    bool success = (old_gen_objects.size() == 2);

    report_test("transitive_marking", success,
                success ? "" : "Transitive marking failed");

    for (auto obj : old_gen_objects) {
        free(obj);
    }
    old_gen_objects.clear();
    mock_roots.clear();
}

// Test 6: No roots - all objects collected
void test_no_roots_all_collected() {
    old_gen_objects.clear();
    mock_roots.clear();

    for (int i = 0; i < 10; i++) {
        old_gen_objects.push_back(create_object(TYPE_INT, 8));
    }

    aria_gc_collect_major();

    bool success = (old_gen_objects.size() == 0);

    report_test("no_roots_all_collected", success,
                success ? "" : "Objects survived without roots");

    old_gen_objects.clear();
}

// Test 7: Mark bit reset after sweep
void test_mark_bit_reset() {
    old_gen_objects.clear();
    mock_roots.clear();

    ObjHeader* obj = create_object(TYPE_INT, 8);
    old_gen_objects.push_back(obj);

    mock_roots.push_back((char*)obj + sizeof(ObjHeader));

    aria_gc_collect_major();

    // After GC, mark bit should be reset to 0
    bool success = (obj->mark_bit == 0);

    report_test("mark_bit_reset", success,
                success ? "" : "Mark bit not reset after sweep");

    free(obj);
    old_gen_objects.clear();
    mock_roots.clear();
}

// Test 8: Object type differentiation
void test_object_type_handling() {
    old_gen_objects.clear();
    mock_roots.clear();

    ObjHeader* int_obj = create_object(TYPE_INT, 8);
    ObjHeader* trit_obj = create_object(TYPE_TRIT, 8);
    ObjHeader* struct_obj = create_object(TYPE_STRUCT, 16);

    old_gen_objects.push_back(int_obj);
    old_gen_objects.push_back(trit_obj);
    old_gen_objects.push_back(struct_obj);

    // Root all three
    mock_roots.push_back((char*)int_obj + sizeof(ObjHeader));
    mock_roots.push_back((char*)trit_obj + sizeof(ObjHeader));
    mock_roots.push_back((char*)struct_obj + sizeof(ObjHeader));

    aria_gc_collect_major();

    bool success = (old_gen_objects.size() == 3);

    report_test("object_type_handling", success,
                success ? "" : "Different object types not handled correctly");

    for (auto obj : old_gen_objects) {
        free(obj);
    }
    old_gen_objects.clear();
    mock_roots.clear();
}

// Test 9: Empty old generation
void test_empty_old_gen() {
    old_gen_objects.clear();
    mock_roots.clear();

    // GC on empty heap should not crash
    aria_gc_collect_major();

    bool success = (old_gen_objects.size() == 0);

    report_test("empty_old_gen", success);
}

// Test 10: Large object graph
void test_large_object_graph() {
    old_gen_objects.clear();
    mock_roots.clear();

    const int count = 100;
    ObjHeader* root = create_object(TYPE_INT, 8);
    old_gen_objects.push_back(root);
    mock_roots.push_back((char*)root + sizeof(ObjHeader));

    // Create many more objects (not rooted)
    for (int i = 0; i < count; i++) {
        old_gen_objects.push_back(create_object(TYPE_INT, 8));
    }

    aria_gc_collect_major();

    // Should only have 1 object left (the root)
    bool success = (old_gen_objects.size() == 1);

    report_test("large_object_graph", success,
                success ? "" : "Large GC failed");

    for (auto obj : old_gen_objects) {
        free(obj);
    }
    old_gen_objects.clear();
    mock_roots.clear();
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Aria GC Implementation Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    test_mark_single_object();
    test_mark_idempotent();
    test_sweep_unmarked();
    test_multiple_roots();
    test_transitive_marking();
    test_no_roots_all_collected();
    test_mark_bit_reset();
    test_object_type_handling();
    test_empty_old_gen();
    test_large_object_graph();

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

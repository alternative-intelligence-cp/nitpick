// Test shadow stack GC root tracking
#include "../src/runtime/gc/shadow_stack.h"
#include "../src/runtime/gc/gc_impl.h"
#include "../src/runtime/gc/header.h"
#include <cassert>
#include <iostream>
#include <vector>

extern "C" Nursery* get_current_thread_nursery();
extern "C" void* aria_gc_alloc(Nursery* nursery, size_t size);

// Declared in gc_impl.cpp but not exposed in header
std::vector<void*> get_thread_roots();

void test_shadow_stack_basic() {
    std::cout << "Test: Basic shadow stack frame push/pop..." << std::endl;
    
    // Push frame
    aria_shadow_stack_push_frame();
    
    // Get roots (should be empty)
    auto roots = aria_shadow_stack_get_roots();
    assert(roots.size() == 0 && "New frame should have no roots");
    
    // Pop frame
    aria_shadow_stack_pop_frame();
    
    std::cout << "  ✓ Frame push/pop works" << std::endl;
}

void test_shadow_stack_roots() {
    std::cout << "Test: Shadow stack root registration..." << std::endl;
    
    // Push frame
    aria_shadow_stack_push_frame();
    
    // Create some "GC pointers" (just regular pointers for testing)
    int dummy1 = 42;
    int dummy2 = 99;
    void* ptr1 = &dummy1;
    void* ptr2 = &dummy2;
    
    // Register roots
    aria_shadow_stack_add_root(&ptr1);
    aria_shadow_stack_add_root(&ptr2);
    
    // Get roots
    auto roots = aria_shadow_stack_get_roots();
    assert(roots.size() == 2 && "Should have 2 registered roots");
    assert((roots[0] == &dummy1 || roots[1] == &dummy1) && "Should contain first pointer");
    assert((roots[0] == &dummy2 || roots[1] == &dummy2) && "Should contain second pointer");
    
    // Remove one root
    aria_shadow_stack_remove_root(&ptr1);
    roots = aria_shadow_stack_get_roots();
    assert(roots.size() == 1 && "Should have 1 root after removal");
    assert(roots[0] == &dummy2 && "Should contain only second pointer");
    
    // Pop frame (cleans up remaining roots)
    aria_shadow_stack_pop_frame();
    
    std::cout << "  ✓ Root add/remove works" << std::endl;
}

void test_shadow_stack_nested_frames() {
    std::cout << "Test: Nested shadow stack frames..." << std::endl;
    
    // Push first frame
    aria_shadow_stack_push_frame();
    
    int dummy1 = 1;
    void* ptr1 = &dummy1;
    aria_shadow_stack_add_root(&ptr1);
    
    // Push second frame
    aria_shadow_stack_push_frame();
    
    int dummy2 = 2;
    void* ptr2 = &dummy2;
    aria_shadow_stack_add_root(&ptr2);
    
    // Should see both roots
    auto roots = aria_shadow_stack_get_roots();
    assert(roots.size() == 2 && "Should have roots from both frames");
    
    // Pop second frame
    aria_shadow_stack_pop_frame();
    
    // Should only see first frame's root
    roots = aria_shadow_stack_get_roots();
    assert(roots.size() == 1 && "Should have only first frame's root");
    assert(roots[0] == &dummy1 && "Should contain first frame's pointer");
    
    // Pop first frame
    aria_shadow_stack_pop_frame();
    
    std::cout << "  ✓ Nested frames work" << std::endl;
}

void test_gc_integration() {
    std::cout << "Test: GC integration with shadow stack..." << std::endl;
    
    // Get nursery
    Nursery* nursery = get_current_thread_nursery();
    assert(nursery != nullptr && "Should have nursery");
    
    // Push shadow stack frame (simulating function entry)
    aria_shadow_stack_push_frame();
    
    // Allocate some GC memory
    void* obj1 = aria_gc_alloc(nursery, 64);
    void* obj2 = aria_gc_alloc(nursery, 128);
    
    assert(obj1 != nullptr && "Allocation should succeed");
    assert(obj2 != nullptr && "Allocation should succeed");
    
    // Register as roots
    aria_shadow_stack_add_root(&obj1);
    aria_shadow_stack_add_root(&obj2);
    
    // Get roots through GC interface
    auto roots = get_thread_roots();
    assert(roots.size() == 2 && "GC should see shadow stack roots");
    
    // Pop frame (simulating function exit)
    aria_shadow_stack_pop_frame();
    
    // Roots should be empty now
    roots = get_thread_roots();
    assert(roots.size() == 0 && "Roots should be cleared after frame pop");
    
    std::cout << "  ✓ GC integration works" << std::endl;
}

int main() {
    std::cout << "=== Shadow Stack Unit Tests ===" << std::endl << std::endl;
    
    test_shadow_stack_basic();
    test_shadow_stack_roots();
    test_shadow_stack_nested_frames();
    test_gc_integration();
    
    std::cout << std::endl << "✅ All shadow stack tests passed!" << std::endl;
    return 0;
}

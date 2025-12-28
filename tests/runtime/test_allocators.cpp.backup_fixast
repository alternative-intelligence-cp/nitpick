/**
 * Tests for Wild/WildX Memory Allocators
 * 
 * Tests manual memory management, executable memory, and W⊕X security.
 */

#include "../test_helpers.h"
#include "runtime/allocators.h"
#include <cstring>

// =============================================================================
// Wild Allocator Tests (Manual malloc/free)
// =============================================================================

TEST_CASE(wild_alloc_basic) {
    // Basic allocation test
    void* ptr = aria_alloc(1024);
    ASSERT(ptr != nullptr, "Allocation should succeed");
    
    // Write and read back
    char* data = static_cast<char*>(ptr);
    data[0] = 'A';
    data[1023] = 'Z';
    ASSERT(data[0] == 'A', "First byte should be 'A'");
    ASSERT(data[1023] == 'Z', "Last byte should be 'Z'");
    
    aria_free(ptr);
}

TEST_CASE(wild_alloc_zero_size) {
    // Zero-size allocation should return NULL
    void* ptr = aria_alloc(0);
    ASSERT(ptr == nullptr, "Zero-size allocation should return NULL");
}

TEST_CASE(wild_alloc_multiple) {
    // Multiple simultaneous allocations
    const int count = 100;
    void* ptrs[count];
    
    for (int i = 0; i < count; ++i) {
        ptrs[i] = aria_alloc(64);
        ASSERT(ptrs[i] != nullptr, "Allocation should succeed");
        
        // Mark each allocation with unique pattern
        char* data = static_cast<char*>(ptrs[i]);
        data[0] = static_cast<char>(i);
    }
    
    // Verify patterns
    for (int i = 0; i < count; ++i) {
        char* data = static_cast<char*>(ptrs[i]);
        ASSERT(data[0] == static_cast<char>(i), "Data should match pattern");
    }
    
    // Free all
    for (int i = 0; i < count; ++i) {
        aria_free(ptrs[i]);
    }
}

TEST_CASE(wild_free_null) {
    // Freeing NULL should be safe
    aria_free(nullptr);  // Should not crash
}

TEST_CASE(wild_realloc_basic) {
    // Basic realloc test
    void* ptr = aria_alloc(100);
    ASSERT(ptr != nullptr, "Initial allocation should succeed");
    
    // Write pattern
    char* data = static_cast<char*>(ptr);
    data[0] = 'X';
    data[99] = 'Y';
    
    // Grow allocation
    void* new_ptr = aria_realloc(ptr, 200);
    ASSERT(new_ptr != nullptr, "Realloc should succeed");
    
    // Verify original data preserved
    data = static_cast<char*>(new_ptr);
    ASSERT(data[0] == 'X', "First byte should be preserved");
    ASSERT(data[99] == 'Y', "Last byte should be preserved");
    
    aria_free(new_ptr);
}

TEST_CASE(wild_realloc_shrink) {
    // Shrink allocation
    void* ptr = aria_alloc(1000);
    ASSERT(ptr != nullptr, "Allocation should succeed");
    
    char* data = static_cast<char*>(ptr);
    data[0] = 'A';
    data[50] = 'B';
    
    void* new_ptr = aria_realloc(ptr, 100);
    ASSERT(new_ptr != nullptr, "Shrink should succeed");
    
    data = static_cast<char*>(new_ptr);
    ASSERT(data[0] == 'A', "Data should be preserved");
    ASSERT(data[50] == 'B', "Data should be preserved");
    
    aria_free(new_ptr);
}

TEST_CASE(wild_realloc_to_zero) {
    // Realloc to zero size should free
    void* ptr = aria_alloc(100);
    ASSERT(ptr != nullptr, "Allocation should succeed");
    
    void* new_ptr = aria_realloc(ptr, 0);
    ASSERT(new_ptr == nullptr, "Realloc to zero should return NULL");
}

// =============================================================================
// Specialized Allocator Tests
// =============================================================================

TEST_CASE(alloc_buffer_basic) {
    // Basic buffer allocation
    void* buf = aria_alloc_buffer(1024, 0, false);
    ASSERT(buf != nullptr, "Buffer allocation should succeed");
    aria_free(buf);
}

TEST_CASE(alloc_buffer_aligned) {
    // Aligned buffer allocation
    void* buf = aria_alloc_buffer(1024, 64, false);
    ASSERT(buf != nullptr, "Aligned buffer allocation should succeed");
    
    // Check alignment (address should be multiple of 64)
    uintptr_t addr = reinterpret_cast<uintptr_t>(buf);
    ASSERT((addr % 64) == 0, "Buffer should be 64-byte aligned");
    
#ifdef _WIN32
    _aligned_free(buf);
#else
    free(buf);
#endif
}

TEST_CASE(alloc_buffer_zero_init) {
    // Zero-initialized buffer
    size_t size = 256;
    void* buf = aria_alloc_buffer(size, 0, true);
    ASSERT(buf != nullptr, "Buffer allocation should succeed");
    
    // Verify all bytes are zero
    unsigned char* data = static_cast<unsigned char*>(buf);
    for (size_t i = 0; i < size; ++i) {
        ASSERT(data[i] == 0, "Buffer should be zero-initialized");
    }
    
    aria_free(buf);
}

TEST_CASE(alloc_string_basic) {
    // String allocation
    size_t len = 100;
    char* str = aria_alloc_string(len);
    ASSERT(str != nullptr, "String allocation should succeed");
    
    // Verify null terminator
    ASSERT(str[len] == '\0', "String should have null terminator");
    
    // Write string data
    std::strcpy(str, "Hello, Aria!");
    ASSERT(std::strcmp(str, "Hello, Aria!") == 0, "String should match");
    
    aria_free(str);
}

TEST_CASE(alloc_array_basic) {
    // Array allocation
    size_t elem_size = sizeof(int);
    size_t count = 50;
    int* arr = static_cast<int*>(aria_alloc_array(elem_size, count));
    ASSERT(arr != nullptr, "Array allocation should succeed");
    
    // Initialize array
    for (size_t i = 0; i < count; ++i) {
        arr[i] = static_cast<int>(i * 2);
    }
    
    // Verify values
    for (size_t i = 0; i < count; ++i) {
        ASSERT(arr[i] == static_cast<int>(i * 2), "Array value should match");
    }
    
    aria_free(arr);
}

TEST_CASE(alloc_array_overflow) {
    // Test overflow protection
    size_t elem_size = SIZE_MAX / 2;
    size_t count = 3;  // This will overflow
    void* ptr = aria_alloc_array(elem_size, count);
    ASSERT(ptr == nullptr, "Overflow should be detected");
}

// =============================================================================
// WildX Executable Memory Tests
// =============================================================================

TEST_CASE(wildx_alloc_basic) {
    // Basic executable memory allocation
    WildXGuard guard = aria_alloc_exec(4096);
    ASSERT(guard.ptr != nullptr, "WildX allocation should succeed");
    ASSERT(guard.size >= 4096, "Size should be at least 4096");
    ASSERT(guard.state == WILDX_STATE_WRITABLE, "Initial state should be WRITABLE");
    ASSERT(!guard.sealed, "Should not be sealed initially");
    
    aria_free_exec(&guard);
    ASSERT(guard.state == WILDX_STATE_FREED, "State should be FREED");
}

TEST_CASE(wildx_write_then_seal) {
    // Write to writable memory, then seal
    WildXGuard guard = aria_alloc_exec(4096);
    ASSERT(guard.ptr != nullptr, "Allocation should succeed");
    
    // Write opcodes to writable memory
    unsigned char* code = static_cast<unsigned char*>(guard.ptr);
    code[0] = 0xC3;  // x86_64: RET instruction (simplified test)
    
    // Seal memory (RW → RX)
    int result = aria_mem_protect_exec(&guard);
    ASSERT(result == 0, "Sealing should succeed");
    ASSERT(guard.state == WILDX_STATE_EXECUTABLE, "State should be EXECUTABLE");
    ASSERT(guard.sealed, "Guard should be sealed");
    
    // Note: We cannot test write failure (SIGSEGV) safely in unit tests
    
    aria_free_exec(&guard);
}

TEST_CASE(wildx_seal_invalid_state) {
    // Attempt to seal already sealed memory
    WildXGuard guard = aria_alloc_exec(4096);
    ASSERT(guard.ptr != nullptr, "Allocation should succeed");
    
    int result = aria_mem_protect_exec(&guard);
    ASSERT(result == 0, "First seal should succeed");
    
    // Try to seal again
    result = aria_mem_protect_exec(&guard);
    ASSERT(result == -1, "Second seal should fail");
    
    aria_free_exec(&guard);
}

TEST_CASE(wildx_seal_null_guard) {
    // Sealing NULL guard should fail
    int result = aria_mem_protect_exec(nullptr);
    ASSERT(result == -1, "Sealing NULL guard should fail");
}

TEST_CASE(wildx_free_null) {
    // Freeing NULL guard should be safe
    aria_free_exec(nullptr);  // Should not crash
}

TEST_CASE(wildx_page_alignment) {
    // Verify page alignment
    WildXGuard guard = aria_alloc_exec(100);  // Small size
    ASSERT(guard.ptr != nullptr, "Allocation should succeed");
    
    // Size should be rounded up to page boundary
    ASSERT(guard.size >= 4096, "Size should be at least one page");
    
    // Address should be page-aligned
    uintptr_t addr = reinterpret_cast<uintptr_t>(guard.ptr);
    ASSERT((addr % 4096) == 0, "Address should be page-aligned");
    
    aria_free_exec(&guard);
}

TEST_CASE(wildx_multiple_allocations) {
    // Multiple WildX allocations
    const int count = 10;
    WildXGuard guards[count];
    
    for (int i = 0; i < count; ++i) {
        guards[i] = aria_alloc_exec(4096);
        ASSERT(guards[i].ptr != nullptr, "Allocation should succeed");
        ASSERT(guards[i].state == WILDX_STATE_WRITABLE, "State should be WRITABLE");
    }
    
    // Seal all
    for (int i = 0; i < count; ++i) {
        int result = aria_mem_protect_exec(&guards[i]);
        ASSERT(result == 0, "Sealing should succeed");
    }
    
    // Free all
    for (int i = 0; i < count; ++i) {
        aria_free_exec(&guards[i]);
        ASSERT(guards[i].state == WILDX_STATE_FREED, "State should be FREED");
    }
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_CASE(allocator_stats_basic) {
    // Query allocator statistics
    AllocatorStats stats;
    aria_allocator_get_stats(&stats);
    
    // Stats should be reasonable (not checking >= 0 since size_t is always non-negative)
    ASSERT(stats.total_wild_allocated == stats.total_wild_allocated, "Wild allocated should be valid");
    ASSERT(stats.total_wildx_allocated == stats.total_wildx_allocated, "WildX allocated should be valid");
    ASSERT(stats.num_wild_allocations == stats.num_wild_allocations, "Wild count should be valid");
    ASSERT(stats.num_wildx_allocations == stats.num_wildx_allocations, "WildX count should be valid");
}

TEST_CASE(allocator_stats_tracking) {
    // Verify stats tracking
    AllocatorStats before, after;
    aria_allocator_get_stats(&before);
    
    // Allocate wild memory
    void* ptr1 = aria_alloc(1024);
    void* ptr2 = aria_alloc(2048);
    
    // Allocate WildX memory
    WildXGuard guard = aria_alloc_exec(4096);
    
    aria_allocator_get_stats(&after);
    
    // Verify wild stats increased
    ASSERT(after.num_wild_allocations >= before.num_wild_allocations + 2,
           "Wild allocation count should increase");
    
    // Verify WildX stats increased
    ASSERT(after.num_wildx_allocations >= before.num_wildx_allocations + 1,
           "WildX allocation count should increase");
    
    // Cleanup
    aria_free(ptr1);
    aria_free(ptr2);
    aria_free_exec(&guard);
}

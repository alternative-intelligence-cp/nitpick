#include <iostream>
#include <cstring>
#include <vector>
#include <cstdint>

// ============================================================================
// String Implementation Test
// ============================================================================

// Mock nursery allocation
static uint8_t* mock_nursery_buffer = nullptr;
static size_t mock_nursery_offset = 0;
static const size_t MOCK_NURSERY_SIZE = 1024 * 1024; // 1MB

extern "C" void* get_current_thread_nursery() {
    return nullptr; // Mock
}

extern "C" void* aria_gc_alloc(void* nursery, size_t size) {
    if (!mock_nursery_buffer) {
        mock_nursery_buffer = new uint8_t[MOCK_NURSERY_SIZE];
        mock_nursery_offset = 0;
    }

    if (mock_nursery_offset + size > MOCK_NURSERY_SIZE) {
        return nullptr; // Out of memory
    }

    void* ptr = mock_nursery_buffer + mock_nursery_offset;
    mock_nursery_offset += size;
    return ptr;
}

// AriaString structure (from string_impl.cpp)
struct AriaString {
    static const size_t SSO_CAPACITY = 23;

    bool sso_flag; // Explicit flag for testing

    union {
        struct {
            char* ptr;
            uint64_t size;
            uint64_t capacity;
        } heap;

        struct {
            char data[SSO_CAPACITY];
            uint8_t size_byte;
        } sso;
    } storage;

    bool is_sso() const {
        return sso_flag;
    }
};

// String functions (from string_impl.cpp)
extern "C" AriaString* aria_string_from_literal(const char* literal, size_t len) {
    AriaString* str = (AriaString*)aria_gc_alloc(get_current_thread_nursery(), sizeof(AriaString));

    if (len <= AriaString::SSO_CAPACITY) {
        // SSO mode
        str->sso_flag = true;
        memcpy(&str->storage.sso.data, literal, len);
        if (len < AriaString::SSO_CAPACITY) {
            str->storage.sso.data[len] = '\0';
        }
        str->storage.sso.size_byte = (uint8_t)len;
    } else {
        // Heap mode
        str->sso_flag = false;
        char* buffer = (char*)aria_gc_alloc(get_current_thread_nursery(), len + 1);
        memcpy(buffer, literal, len);
        buffer[len] = '\0';

        str->storage.heap.ptr = buffer;
        str->storage.heap.size = len;
        str->storage.heap.capacity = len;
    }

    return str;
}

extern "C" AriaString* aria_string_concat(AriaString* a, AriaString* b) {
    size_t len_a = a->is_sso() ? a->storage.sso.size_byte : a->storage.heap.size;
    size_t len_b = b->is_sso() ? b->storage.sso.size_byte : b->storage.heap.size;
    size_t total_len = len_a + len_b;

    AriaString* res = (AriaString*)aria_gc_alloc(get_current_thread_nursery(), sizeof(AriaString));

    if (total_len <= AriaString::SSO_CAPACITY) {
        // SSO mode
        res->sso_flag = true;
        const char* src_a = a->is_sso() ? a->storage.sso.data : a->storage.heap.ptr;
        const char* src_b = b->is_sso() ? b->storage.sso.data : b->storage.heap.ptr;

        memcpy(&res->storage.sso.data, src_a, len_a);
        memcpy(&res->storage.sso.data[len_a], src_b, len_b);
        if (total_len < AriaString::SSO_CAPACITY) {
            res->storage.sso.data[total_len] = '\0';
        }
        res->storage.sso.size_byte = (uint8_t)total_len;
    } else {
        // Heap mode
        res->sso_flag = false;
        char* buffer = (char*)aria_gc_alloc(get_current_thread_nursery(), total_len + 1);

        const char* src_a = a->is_sso() ? a->storage.sso.data : a->storage.heap.ptr;
        const char* src_b = b->is_sso() ? b->storage.sso.data : b->storage.heap.ptr;

        memcpy(buffer, src_a, len_a);
        memcpy(buffer + len_a, src_b, len_b);
        buffer[total_len] = '\0';

        res->storage.heap.ptr = buffer;
        res->storage.heap.size = total_len;
        res->storage.heap.capacity = total_len;
    }

    return res;
}

extern "C" int8_t aria_string_get_at(AriaString* str, uint64_t index) {
    size_t len = str->is_sso() ? str->storage.sso.size_byte : str->storage.heap.size;
    if (index >= len) {
        return 0;
    }

    const char* buf = str->is_sso() ? str->storage.sso.data : str->storage.heap.ptr;
    return buf[index];
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

// Reset mock nursery between tests
void reset_nursery() {
    mock_nursery_offset = 0;
}

// ============================================================================
// Test Cases
// ============================================================================

// Test 1: Create string from small literal (SSO)
void test_small_string_sso() {
    reset_nursery();

    const char* literal = "hello";
    AriaString* str = aria_string_from_literal(literal, strlen(literal));

    bool is_sso = str->is_sso();
    bool correct_data = (strncmp(str->storage.sso.data, literal, strlen(literal)) == 0);
    bool correct_size = (str->storage.sso.size_byte == strlen(literal));

    bool success = is_sso && correct_data && correct_size;

    report_test("small_string_sso", success,
                success ? "" : "Small string not using SSO correctly");
}

// Test 2: Create string from large literal (Heap)
void test_large_string_heap() {
    reset_nursery();

    const char* literal = "This is a very long string that exceeds SSO capacity";
    AriaString* str = aria_string_from_literal(literal, strlen(literal));

    bool is_heap = !str->is_sso();
    bool correct_data = (strcmp(str->storage.heap.ptr, literal) == 0);
    bool correct_size = (str->storage.heap.size == strlen(literal));

    bool success = is_heap && correct_data && correct_size;

    report_test("large_string_heap", success,
                success ? "" : "Large string not using heap correctly");
}

// Test 3: String concatenation (SSO + SSO -> SSO)
void test_concat_sso_sso() {
    reset_nursery();

    AriaString* a = aria_string_from_literal("hello", 5);
    AriaString* b = aria_string_from_literal(" world", 6);
    AriaString* result = aria_string_concat(a, b);

    bool is_sso = result->is_sso();
    bool correct_data = (strncmp(result->storage.sso.data, "hello world", 11) == 0);
    bool correct_size = (result->storage.sso.size_byte == 11);

    bool success = is_sso && correct_data && correct_size;

    report_test("concat_sso_sso", success,
                success ? "" : "SSO + SSO concatenation failed");
}

// Test 4: String concatenation (SSO + SSO -> Heap)
void test_concat_sso_to_heap() {
    reset_nursery();

    AriaString* a = aria_string_from_literal("hello world ", 12);
    AriaString* b = aria_string_from_literal("from Aria lang", 14);
    AriaString* result = aria_string_concat(a, b);

    bool is_heap = !result->is_sso();
    bool correct_data = (strcmp(result->storage.heap.ptr, "hello world from Aria lang") == 0);
    bool correct_size = (result->storage.heap.size == 26);

    bool success = is_heap && correct_data && correct_size;

    report_test("concat_sso_to_heap", success,
                success ? "" : "SSO concatenation overflow to heap failed");
}

// Test 5: String concatenation (Heap + Heap)
void test_concat_heap_heap() {
    reset_nursery();

    const char* lit_a = "This is the first very long string that uses heap allocation";
    const char* lit_b = " and this is the second very long string";

    AriaString* a = aria_string_from_literal(lit_a, strlen(lit_a));
    AriaString* b = aria_string_from_literal(lit_b, strlen(lit_b));
    AriaString* result = aria_string_concat(a, b);

    bool is_heap = !result->is_sso();
    size_t expected_len = strlen(lit_a) + strlen(lit_b);
    bool correct_size = (result->storage.heap.size == expected_len);

    bool success = is_heap && correct_size;

    report_test("concat_heap_heap", success,
                success ? "" : "Heap + Heap concatenation failed");
}

// Test 6: Character access within bounds
void test_char_access_valid() {
    reset_nursery();

    AriaString* str = aria_string_from_literal("aria", 4);

    bool success = (aria_string_get_at(str, 0) == 'a') &&
                   (aria_string_get_at(str, 1) == 'r') &&
                   (aria_string_get_at(str, 2) == 'i') &&
                   (aria_string_get_at(str, 3) == 'a');

    report_test("char_access_valid", success,
                success ? "" : "Character access incorrect");
}

// Test 7: Character access out of bounds
void test_char_access_oob() {
    reset_nursery();

    AriaString* str = aria_string_from_literal("test", 4);

    int8_t result = aria_string_get_at(str, 100);

    bool success = (result == 0);

    report_test("char_access_oob", success,
                success ? "" : "OOB access didn't return 0");
}

// Test 8: Empty string
void test_empty_string() {
    reset_nursery();

    AriaString* str = aria_string_from_literal("", 0);

    bool is_sso = str->is_sso();
    bool correct_size = (str->storage.sso.size_byte == 0);

    bool success = is_sso && correct_size;

    report_test("empty_string", success,
                success ? "" : "Empty string creation failed");
}

// Test 9: SSO capacity boundary (exactly 23 chars)
void test_sso_boundary() {
    reset_nursery();

    const char* literal = "12345678901234567890123"; // 23 chars
    AriaString* str = aria_string_from_literal(literal, 23);

    bool is_sso = str->is_sso();
    bool correct_size = (str->storage.sso.size_byte == 23);
    bool correct_data = (strncmp(str->storage.sso.data, literal, 23) == 0);

    bool success = is_sso && correct_size && correct_data;

    report_test("sso_boundary", success,
                success ? "" : "SSO boundary case failed");
}

// Test 10: Concatenate empty strings
void test_concat_empty() {
    reset_nursery();

    AriaString* a = aria_string_from_literal("", 0);
    AriaString* b = aria_string_from_literal("", 0);
    AriaString* result = aria_string_concat(a, b);

    bool is_sso = result->is_sso();
    bool correct_size = (result->storage.sso.size_byte == 0);

    bool success = is_sso && correct_size;

    report_test("concat_empty", success,
                success ? "" : "Empty concatenation failed");
}

// Test 11: Mixed mode concat (SSO + Heap)
void test_concat_mixed() {
    reset_nursery();

    AriaString* small = aria_string_from_literal("Hi ", 3);
    const char* big_lit = "this is a long string that uses heap";
    AriaString* big = aria_string_from_literal(big_lit, strlen(big_lit));

    AriaString* result = aria_string_concat(small, big);

    bool is_heap = !result->is_sso();
    size_t expected_len = 3 + strlen(big_lit);
    bool correct_size = (result->storage.heap.size == expected_len);

    bool success = is_heap && correct_size;

    report_test("concat_mixed", success,
                success ? "" : "Mixed SSO+Heap concatenation failed");
}

// Test 12: Repeated concatenation
void test_repeated_concat() {
    reset_nursery();

    AriaString* str = aria_string_from_literal("a", 1);

    for (int i = 0; i < 5; i++) {
        AriaString* append = aria_string_from_literal("b", 1);
        str = aria_string_concat(str, append);
    }

    bool correct_size = (str->storage.sso.size_byte == 6);
    bool correct_data = (strncmp(str->storage.sso.data, "abbbbb", 6) == 0);

    bool success = correct_size && correct_data;

    report_test("repeated_concat", success,
                success ? "" : "Repeated concatenation failed");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Aria String Implementation Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    test_small_string_sso();
    test_large_string_heap();
    test_concat_sso_sso();
    test_concat_sso_to_heap();
    test_concat_heap_heap();
    test_char_access_valid();
    test_char_access_oob();
    test_empty_string();
    test_sso_boundary();
    test_concat_empty();
    test_concat_mixed();
    test_repeated_concat();

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

    // Cleanup
    if (mock_nursery_buffer) {
        delete[] mock_nursery_buffer;
    }

    return (failed == 0) ? 0 : 1;
}

/**
 * @file test_framework.hpp
 * @brief Test Framework - Six-stream test runner with binary result protocol
 * 
 * Implements modern test framework with Aria's architectural patterns:
 * - Six-stream topology (stdout/UI, stddbg/telemetry, stddato/results)
 * - Binary test result protocol
 * - TBB-aware assertions
 * - Parallel test execution
 * - Zero-overhead test registration
 * 
 * Reference: sysUtils_8_test.txt (423 lines)
 */

#ifndef ARIA_ATEST_FRAMEWORK_HPP
#define ARIA_ATEST_FRAMEWORK_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <map>

namespace aria {
namespace test {

// ============================================================================
// TBB Type Definitions
// ============================================================================

using tbb8 = int8_t;
using tbb32 = int32_t;
using tbb64 = int64_t;

// TBB Sentinels - sticky error values
constexpr tbb64 TBB64_ERR = INT64_MIN;
constexpr tbb32 TBB32_ERR = INT32_MIN;
constexpr tbb8 TBB8_ERR = INT8_MIN;

// ============================================================================
// Test Result Protocol (Binary)
// ============================================================================

// Packet types for stddato stream (FD 5)
enum class TestResult : uint8_t {
    START = 0x01,
    PASS  = 0x02,
    FAIL  = 0x03,
    SKIP  = 0x04,
    LOG   = 0x05
};

// Result flags
constexpr uint8_t FLAG_TBB_ERR = 0x01;
constexpr uint8_t FLAG_MEM_LEAK = 0x02;
constexpr uint8_t FLAG_EXCEPTION = 0x04;

// Binary protocol header (32 bytes + name)
struct TestPacket {
    uint32_t magic;         // 0x41544553 "ATES" (Aria TEST)
    TestResult type;        // Result type
    uint8_t flags;          // Error flags
    uint16_t reserved;      // Padding
    uint64_t duration_ns;   // Execution time
    uint64_t alloc_count;   // Allocation count
    uint32_t name_len;      // Test name length
    uint32_t payload_len;   // Extra payload length
    
    TestPacket()
        : magic(0x41544553)
        , type(TestResult::START)
        , flags(0)
        , reserved(0)
        , duration_ns(0)
        , alloc_count(0)
        , name_len(0)
        , payload_len(0)
    {}
};

static_assert(sizeof(TestPacket) == 32, "TestPacket must be 32 bytes");

// ============================================================================
// Test Registration
// ============================================================================

class TestCase {
public:
    using TestFunc = std::function<void()>;
    
    TestCase(const std::string& name, const std::string& group, TestFunc func)
        : name_(name), group_(group), func_(func), skip_(false)
    {}
    
    const std::string& name() const { return name_; }
    const std::string& group() const { return group_; }
    bool skip() const { return skip_; }
    void set_skip(bool skip) { skip_ = skip; }
    
    void run() const { func_(); }
    
private:
    std::string name_;
    std::string group_;
    TestFunc func_;
    bool skip_;
};

// Test registry (singleton)
class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry registry;
        return registry;
    }
    
    void register_test(const std::string& name, const std::string& group,
                      TestCase::TestFunc func) {
        tests_.emplace_back(name, group, func);
    }
    
    const std::vector<TestCase>& tests() const { return tests_; }
    std::vector<TestCase>& tests() { return tests_; }
    
private:
    TestRegistry() = default;
    std::vector<TestCase> tests_;
};

// Auto-registration helper
struct TestRegistrar {
    TestRegistrar(const char* name, const char* group, TestCase::TestFunc func) {
        TestRegistry::instance().register_test(name, group, func);
    }
};

// Test registration macro
#define ARIA_TEST(group, name) \
    static void test_##group##_##name(); \
    static ::aria::test::TestRegistrar registrar_##group##_##name( \
        #name, #group, test_##group##_##name); \
    static void test_##group##_##name()

// ============================================================================
// Assertions
// ============================================================================

class TestFailure : public std::exception {
public:
    explicit TestFailure(const std::string& msg) : msg_(msg) {}
    const char* what() const noexcept override { return msg_.c_str(); }
    
private:
    std::string msg_;
};

// Assert true
inline void assert_true(bool condition, const std::string& msg = "") {
    if (!condition) {
        throw TestFailure(msg.empty() ? "Assertion failed" : msg);
    }
}

// Assert false
inline void assert_false(bool condition, const std::string& msg = "") {
    assert_true(!condition, msg.empty() ? "Expected false" : msg);
}

// Assert equal (generic)
template<typename T>
void assert_eq(const T& actual, const T& expected, const std::string& msg = "") {
    if (actual != expected) {
        throw TestFailure(msg.empty() ? "Values not equal" : msg);
    }
}

// Assert TBB valid (not ERR sentinel)
inline void assert_tbb_valid(tbb64 value, const std::string& msg = "") {
    if (value == TBB64_ERR) {
        throw TestFailure(msg.empty() ? "TBB ERR sentinel detected" : msg);
    }
}

inline void assert_tbb_valid(tbb32 value, const std::string& msg = "") {
    if (value == TBB32_ERR) {
        throw TestFailure(msg.empty() ? "TBB ERR sentinel detected" : msg);
    }
}

// Assert TBB is ERR (for testing error handling)
inline void assert_tbb_err(tbb64 value, const std::string& msg = "") {
    if (value != TBB64_ERR) {
        throw TestFailure(msg.empty() ? "Expected TBB ERR sentinel" : msg);
    }
}

// Assert strings contain
inline void assert_contains(const std::string& haystack, const std::string& needle,
                           const std::string& msg = "") {
    if (haystack.find(needle) == std::string::npos) {
        throw TestFailure(msg.empty() ? "String not found" : msg);
    }
}

// Assert not null
template<typename T>
void assert_not_null(T* ptr, const std::string& msg = "") {
    if (ptr == nullptr) {
        throw TestFailure(msg.empty() ? "Pointer is null" : msg);
    }
}

// ============================================================================
// Six-Stream Writers
// ============================================================================

namespace streams {
    // Write telemetry to stddbg (FD 3)
    void write_telemetry(const std::string& event, const std::string& data);
    
    // Write test result packet to stddato (FD 5)
    void write_test_result(const TestPacket& packet, const std::string& name);
    
    // Check if stream is active
    bool is_stream_active(int fd);
}

// ============================================================================
// Test Runner
// ============================================================================

struct TestStats {
    int total;
    int passed;
    int failed;
    int skipped;
    uint64_t duration_ns;
    
    TestStats() : total(0), passed(0), failed(0), skipped(0), duration_ns(0) {}
};

class TestRunner {
public:
    TestRunner();
    
    // Run all tests
    TestStats run_all(bool verbose = false);
    
    // Run tests matching filter
    TestStats run_filtered(const std::string& filter, bool verbose = false);
    
    // Run specific group
    TestStats run_group(const std::string& group, bool verbose = false);
    
    // List all tests
    void list_tests() const;
    
private:
    void run_test(const TestCase& test, TestStats& stats, bool verbose);
    bool matches_filter(const TestCase& test, const std::string& filter) const;
    
    TestRegistry& registry_;
};

// ============================================================================
// FFI C API
// ============================================================================

extern "C" {
    // Run all tests
    int aria_test_run_all(int verbose);
    
    // Run tests matching pattern
    int aria_test_run_filtered(const char* pattern, int verbose);
    
    // List all registered tests
    void aria_test_list();
    
    // Get test statistics as JSON
    const char* aria_test_get_stats();
}

} // namespace test
} // namespace aria

#endif // ARIA_ATEST_FRAMEWORK_HPP

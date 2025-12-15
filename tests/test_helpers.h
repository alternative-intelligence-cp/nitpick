#ifndef ARIA_TEST_HELPERS_H
#define ARIA_TEST_HELPERS_H

#include <iostream>
#include <string>
#include <sstream>
#include <functional>
#include <vector>

// Color codes for terminal output
#define COLOR_RED     "\033[0;31m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_RESET   "\033[0m"

// Test statistics
struct TestStats {
    int total = 0;
    int passed = 0;
    int failed = 0;
    std::vector<std::string> failures;
};

// Global test stats
extern TestStats g_test_stats;

// Assertion macros
#define ASSERT(condition, message) \
    do { \
        g_test_stats.total++; \
        if (!(condition)) { \
            g_test_stats.failed++; \
            std::stringstream ss; \
            ss << __FILE__ << ":" << __LINE__ << " - " << message; \
            g_test_stats.failures.push_back(ss.str()); \
            std::cerr << COLOR_RED << "✗ FAIL: " << message << COLOR_RESET << std::endl; \
            std::cerr << "  at " << __FILE__ << ":" << __LINE__ << std::endl; \
        } else { \
            g_test_stats.passed++; \
        } \
    } while(0)

#define ASSERT_EQ(actual, expected, message) \
    do { \
        g_test_stats.total++; \
        if ((actual) != (expected)) { \
            g_test_stats.failed++; \
            std::stringstream ss; \
            ss << __FILE__ << ":" << __LINE__ << " - " << message \
               << " (expected: " << (expected) << ", got: " << (actual) << ")"; \
            g_test_stats.failures.push_back(ss.str()); \
            std::cerr << COLOR_RED << "✗ FAIL: " << message << COLOR_RESET << std::endl; \
            std::cerr << "  Expected: " << (expected) << std::endl; \
            std::cerr << "  Got:      " << (actual) << std::endl; \
            std::cerr << "  at " << __FILE__ << ":" << __LINE__ << std::endl; \
        } else { \
            g_test_stats.passed++; \
        } \
    } while(0)

#define ASSERT_NE(actual, not_expected, message) \
    do { \
        g_test_stats.total++; \
        if ((actual) == (not_expected)) { \
            g_test_stats.failed++; \
            std::stringstream ss; \
            ss << __FILE__ << ":" << __LINE__ << " - " << message \
               << " (should not equal: " << (not_expected) << ")"; \
            g_test_stats.failures.push_back(ss.str()); \
            std::cerr << COLOR_RED << "✗ FAIL: " << message << COLOR_RESET << std::endl; \
            std::cerr << "  Should not equal: " << (not_expected) << std::endl; \
            std::cerr << "  at " << __FILE__ << ":" << __LINE__ << std::endl; \
        } else { \
            g_test_stats.passed++; \
        } \
    } while(0)

#define ASSERT_TRUE(condition, message) ASSERT((condition), message)
#define ASSERT_FALSE(condition, message) ASSERT(!(condition), message)

// Test case registration
struct TestCase {
    std::string name;
    std::function<void()> func;
};

extern std::vector<TestCase> g_test_cases;

#define TEST_CASE(name) \
    void test_##name(); \
    struct TestRegistrar_##name { \
        TestRegistrar_##name() { \
            g_test_cases.push_back({#name, test_##name}); \
        } \
    }; \
    static TestRegistrar_##name registrar_##name; \
    void test_##name()

// Test suite helpers
void run_all_tests();
void print_test_summary();

#endif // ARIA_TEST_HELPERS_H

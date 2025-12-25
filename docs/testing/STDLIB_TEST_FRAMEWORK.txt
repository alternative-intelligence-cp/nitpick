# Aria Stdlib Test Framework

**Version**: 0.1.0
**Status**: Design Specification
**Author**: Aria Language Project
**Date**: December 2025

---

## 1. Overview

This document specifies the testing framework for the Aria Standard Library (stdlib). The framework is designed to be:

- **Native**: Written entirely in Aria (no external test runners)
- **Comprehensive**: Covers all stdlib modules
- **Deterministic**: Same inputs always produce same results
- **Fast**: Parallel test execution where possible
- **Informative**: Clear error reporting with source locations

---

## 2. Directory Structure

```
aria/
├── tests/
│   ├── stdlib/
│   │   ├── strings/           # String operation tests
│   │   │   ├── test_concat.aria
│   │   │   ├── test_slice.aria
│   │   │   ├── test_search.aria
│   │   │   └── test_format.aria
│   │   │
│   │   ├── tbb/               # Twisted Balanced Binary tests
│   │   │   ├── test_tbb8.aria
│   │   │   ├── test_tbb16.aria
│   │   │   ├── test_tbb32.aria
│   │   │   ├── test_tbb64.aria
│   │   │   └── test_overflow.aria
│   │   │
│   │   ├── collections/       # Collection type tests
│   │   │   ├── test_array.aria
│   │   │   ├── test_map.aria
│   │   │   ├── test_set.aria
│   │   │   └── test_deque.aria
│   │   │
│   │   ├── memory/            # Memory/allocator tests
│   │   │   ├── test_arena.aria
│   │   │   ├── test_pool.aria
│   │   │   └── test_gc.aria
│   │   │
│   │   ├── io/                # I/O operation tests
│   │   │   ├── test_file.aria
│   │   │   ├── test_stream.aria
│   │   │   └── test_format.aria
│   │   │
│   │   ├── math/              # Math function tests
│   │   │   ├── test_arithmetic.aria
│   │   │   ├── test_bitwise.aria
│   │   │   └── test_ternary.aria
│   │   │
│   │   ├── examples/          # Example/template tests
│   │   │   ├── template_basic.aria
│   │   │   └── template_advanced.aria
│   │   │
│   │   └── runner.aria        # Test runner entry point
│   │
│   └── test_config.abc        # Test configuration
│
├── src/
│   └── stdlib/
│       └── testing/           # Test framework library
│           ├── assert.aria    # Assertion macros
│           ├── runner.aria    # Test runner implementation
│           └── reporter.aria  # Result reporting
│
└── docs/
    └── testing/
        └── STDLIB_TEST_FRAMEWORK.md  # This document
```

---

## 3. Test Framework Library

### 3.1 Assertion Module (`stdlib/testing/assert.aria`)

The assertion module provides type-safe assertion functions that report failures with source location information.

```aria
// Core assertion functions

// Assert that condition is true
func:assert = void(bool:condition, str:message) {
    if (!condition) {
        test_fail(message);
    }
};

// Assert equality for common types
func:assert_eq = void(int64:actual, int64:expected, str:message) {
    if (actual != expected) {
        str:err = string_format("Expected {}, got {}: {}", expected, actual, message);
        test_fail(err);
    }
};

func:assert_eq_str = void(str:actual, str:expected, str:message) {
    if (!string_eq(actual, expected)) {
        str:err = string_format("Expected '{}', got '{}': {}", expected, actual, message);
        test_fail(err);
    }
};

func:assert_eq_bool = void(bool:actual, bool:expected, str:message) {
    if (actual != expected) {
        str:err = string_format("Expected {}, got {}: {}", expected, actual, message);
        test_fail(err);
    }
};

// Assert TBB values
func:assert_tbb_ok = void(tbb64:value, str:message) {
    if (tbb64.is_err(value)) {
        test_fail(string_format("Expected valid tbb64, got ERR: {}", message));
    }
};

func:assert_tbb_err = void(tbb64:value, str:message) {
    if (!tbb64.is_err(value)) {
        test_fail(string_format("Expected ERR, got {}: {}", tbb64.to_int(value), message));
    }
};

func:assert_tbb_eq = void(tbb64:actual, tbb64:expected, str:message) {
    // Both must be ERR, or both must have equal values
    bool:a_err = tbb64.is_err(actual);
    bool:e_err = tbb64.is_err(expected);

    if (a_err != e_err) {
        test_fail(string_format("TBB mismatch: {}", message));
    }
    if (!a_err) {
        if (tbb64.to_int(actual) != tbb64.to_int(expected)) {
            test_fail(string_format("Expected {}, got {}: {}",
                tbb64.to_int(expected), tbb64.to_int(actual), message));
        }
    }
};

// Assert that a value is within range
func:assert_range = void(int64:value, int64:min, int64:max, str:message) {
    if (value < min || value > max) {
        str:err = string_format("Value {} not in range [{}, {}]: {}",
            value, min, max, message);
        test_fail(err);
    }
};

// Assert null/non-null
func:assert_null = void(ptr:value, str:message) {
    if (value != null) {
        test_fail(string_format("Expected null pointer: {}", message));
    }
};

func:assert_not_null = void(ptr:value, str:message) {
    if (value == null) {
        test_fail(string_format("Expected non-null pointer: {}", message));
    }
};

// Assert failure (for negative testing)
func:assert_fails = void(func:action, str:message) {
    // Execute action, expect it to fail
    // If it succeeds, that's a test failure
};
```

### 3.2 Test Registration

Tests are registered using a convention-based approach. Any function starting with `test_` is automatically discovered.

```aria
// Test function signature
// Returns 0 on success, non-zero on failure
type:TestFunc = func() -> int8;

// Test metadata
struct:TestCase {
    str:name;           // Test name (derived from function name)
    str:file;           // Source file
    int64:line;         // Line number
    TestFunc:run;       // Test function
};

// Test suite (collection of related tests)
struct:TestSuite {
    str:name;                    // Suite name (e.g., "strings", "tbb")
    array<TestCase>:tests;       // All tests in suite
    int64:passed;                // Count of passed tests
    int64:failed;                // Count of failed tests
    int64:skipped;               // Count of skipped tests
};
```

### 3.3 Test Runner (`stdlib/testing/runner.aria`)

```aria
// Test runner configuration
struct:RunnerConfig {
    bool:verbose;               // Show all test output
    bool:fail_fast;             // Stop on first failure
    bool:parallel;              // Run tests in parallel
    int64:num_threads;          // Number of parallel threads
    str:filter;                 // Test name filter (glob pattern)
    array<str>:suites;          // Specific suites to run (empty = all)
};

// Test result
struct:TestResult {
    str:name;
    bool:passed;
    str:error_message;          // Empty if passed
    int64:duration_ms;          // Test execution time
};

// Suite result
struct:SuiteResult {
    str:name;
    array<TestResult>:results;
    int64:total;
    int64:passed;
    int64:failed;
    int64:skipped;
    int64:duration_ms;
};

// Run all tests in a suite
func:run_suite = SuiteResult(TestSuite:suite, RunnerConfig:config) {
    SuiteResult:result;
    result.name = suite.name;

    for (TestCase:test in suite.tests) {
        // Check filter
        if (!string_empty(config.filter)) {
            if (!glob_match(config.filter, test.name)) {
                // Skip this test
                continue;
            }
        }

        TestResult:tr = run_test(test, config);
        array_push(result.results, tr);

        if (tr.passed) {
            result.passed = result.passed + 1;
        } else {
            result.failed = result.failed + 1;
            if (config.fail_fast) {
                break;
            }
        }
    }

    result.total = array_len(result.results);
    pass(result);
};

// Run a single test
func:run_test = TestResult(TestCase:test, RunnerConfig:config) {
    TestResult:result;
    result.name = test.name;

    int64:start = time_now_ms();

    // Run the test
    int8:status = test.run();

    int64:end = time_now_ms();
    result.duration_ms = end - start;

    if (status == 0) {
        result.passed = true;
    } else {
        result.passed = false;
        result.error_message = get_last_test_error();
    }

    pass(result);
};
```

### 3.4 Reporter (`stdlib/testing/reporter.aria`)

```aria
// Output formats
enum:ReportFormat {
    CONSOLE,    // Human-readable console output
    JSON,       // Machine-readable JSON
    TAP,        // Test Anything Protocol
    JUNIT       // JUnit XML format (for CI)
};

// Report configuration
struct:ReporterConfig {
    ReportFormat:format;
    bool:show_timing;
    bool:show_summary;
    bool:colorize;
};

// Console reporter implementation
func:report_console = void(SuiteResult:result, ReporterConfig:config) {
    // Header
    print_line(string_format("=== {} ===", result.name));

    // Individual test results
    for (TestResult:tr in result.results) {
        if (tr.passed) {
            if (config.colorize) {
                print_line(string_format("  \033[32mPASS\033[0m {}", tr.name));
            } else {
                print_line(string_format("  PASS {}", tr.name));
            }
        } else {
            if (config.colorize) {
                print_line(string_format("  \033[31mFAIL\033[0m {} - {}",
                    tr.name, tr.error_message));
            } else {
                print_line(string_format("  FAIL {} - {}",
                    tr.name, tr.error_message));
            }
        }

        if (config.show_timing) {
            print_line(string_format("       ({}ms)", tr.duration_ms));
        }
    }

    // Summary
    if (config.show_summary) {
        print_line("");
        print_line(string_format("Results: {}/{} passed ({} failed) in {}ms",
            result.passed, result.total, result.failed, result.duration_ms));
    }
};

// JSON reporter implementation
func:report_json = void(SuiteResult:result, ReporterConfig:config) {
    // Output machine-readable JSON
    print_line("{");
    print_line(string_format("  \"suite\": \"{}\",", result.name));
    print_line(string_format("  \"total\": {},", result.total));
    print_line(string_format("  \"passed\": {},", result.passed));
    print_line(string_format("  \"failed\": {},", result.failed));
    print_line(string_format("  \"duration_ms\": {},", result.duration_ms));
    print_line("  \"tests\": [");

    int64:i = 0;
    for (TestResult:tr in result.results) {
        str:comma = "";
        if (i > 0) { comma = ","; }

        print_line(string_format("    {}{{", comma));
        print_line(string_format("      \"name\": \"{}\",", tr.name));
        print_line(string_format("      \"passed\": {},", tr.passed));
        print_line(string_format("      \"duration_ms\": {},", tr.duration_ms));
        if (!tr.passed) {
            print_line(string_format("      \"error\": \"{}\"", tr.error_message));
        }
        print_line("    }");
        i = i + 1;
    }

    print_line("  ]");
    print_line("}");
};
```

---

## 4. Writing Tests

### 4.1 Basic Test Structure

```aria
// test_example.aria

use testing.assert

// Test functions must start with "test_"
// Return 0 for success, non-zero for failure

func:test_basic_addition = int8() {
    int64:result = 2 + 2;
    assert_eq(result, 4, "2 + 2 should equal 4");
    pass(0);
};

func:test_string_concat = int8() {
    str:hello = "Hello, ";
    str:world = "World!";
    str:result = string_concat(hello, world);
    assert_eq_str(result, "Hello, World!", "String concatenation failed");
    pass(0);
};

func:test_tbb_overflow = int8() {
    // Test that overflow produces ERR
    tbb8:max = tbb8.from_int(127);
    tbb8:result = max + tbb8.from_int(1);
    assert_tbb_err(result, "127 + 1 should overflow to ERR");
    pass(0);
};
```

### 4.2 Test Setup and Teardown

```aria
// test_with_fixtures.aria

use testing.assert

// Global fixture (initialized once per suite)
static ptr:global_fixture = null;

// Setup function (called before each test)
func:setup = void() {
    // Allocate resources
    global_fixture = arena_alloc(1024);
};

// Teardown function (called after each test)
func:teardown = void() {
    // Free resources
    arena_free(global_fixture);
    global_fixture = null;
};

func:test_with_fixture = int8() {
    assert_not_null(global_fixture, "Fixture should be initialized");
    // Use the fixture...
    pass(0);
};
```

### 4.3 Parameterized Tests

```aria
// test_parameterized.aria

use testing.assert

// Test data structure
struct:AddTestCase {
    int64:a;
    int64:b;
    int64:expected;
};

// Test cases
static array<AddTestCase>:add_cases = [
    { a: 1, b: 1, expected: 2 },
    { a: 0, b: 0, expected: 0 },
    { a: -1, b: 1, expected: 0 },
    { a: 100, b: 200, expected: 300 },
    { a: -50, b: -50, expected: -100 }
];

func:test_addition_parameterized = int8() {
    for (AddTestCase:tc in add_cases) {
        int64:result = tc.a + tc.b;
        assert_eq(result, tc.expected,
            string_format("{} + {} should equal {}", tc.a, tc.b, tc.expected));
    }
    pass(0);
};
```

### 4.4 Skip and Expected Failures

```aria
// test_skips.aria

use testing.assert

// Mark test as skipped (e.g., not yet implemented)
func:test_not_implemented = int8() {
    test_skip("Feature not yet implemented");
    pass(0);  // Never reached
};

// Mark test as expected to fail
func:test_known_bug = int8() {
    test_expect_fail("Known bug #123");

    // This will fail, but won't count as a test failure
    int64:result = buggy_function();
    assert_eq(result, 42, "Should be 42 but bug causes 0");
    pass(0);
};
```

---

## 5. Test Categories

### 5.1 String Tests (`tests/stdlib/strings/`)

| Test File | Tests |
|-----------|-------|
| `test_concat.aria` | String concatenation, empty strings, large strings |
| `test_slice.aria` | Substring extraction, bounds checking |
| `test_search.aria` | Find, contains, starts_with, ends_with |
| `test_format.aria` | Format string, escaping, unicode |
| `test_conversion.aria` | int_to_string, string_to_int, parse errors |

### 5.2 TBB Tests (`tests/stdlib/tbb/`)

| Test File | Tests |
|-----------|-------|
| `test_tbb8.aria` | 8-bit TBB arithmetic, overflow, ERR propagation |
| `test_tbb16.aria` | 16-bit TBB arithmetic |
| `test_tbb32.aria` | 32-bit TBB arithmetic |
| `test_tbb64.aria` | 64-bit TBB arithmetic |
| `test_overflow.aria` | Overflow detection, MIN/MAX boundaries |
| `test_error_prop.aria` | Sticky error propagation |

### 5.3 Collection Tests (`tests/stdlib/collections/`)

| Test File | Tests |
|-----------|-------|
| `test_array.aria` | Create, push, pop, index, slice, iteration |
| `test_map.aria` | Insert, get, delete, iteration, collision handling |
| `test_set.aria` | Add, contains, remove, union, intersection |
| `test_deque.aria` | Push front/back, pop front/back |

### 5.4 Memory Tests (`tests/stdlib/memory/`)

| Test File | Tests |
|-----------|-------|
| `test_arena.aria` | Arena allocation, reset, alignment |
| `test_pool.aria` | Pool allocation, free list |
| `test_gc.aria` | GC triggering, root marking, collection |

### 5.5 I/O Tests (`tests/stdlib/io/`)

| Test File | Tests |
|-----------|-------|
| `test_file.aria` | Open, read, write, seek, close |
| `test_stream.aria` | Buffer, flush, line reading |
| `test_format.aria` | Printf-style formatting |

---

## 6. Running Tests

### 6.1 Command Line Interface

```bash
# Run all tests
ariac test

# Run specific suite
ariac test --suite strings

# Run specific test
ariac test --filter "test_concat*"

# Verbose output
ariac test --verbose

# Stop on first failure
ariac test --fail-fast

# Parallel execution
ariac test --parallel --jobs 4

# JSON output (for CI)
ariac test --format json

# JUnit XML (for CI integration)
ariac test --format junit > test-results.xml
```

### 6.2 CMake Integration

```cmake
# tests/CMakeLists.txt

# Discover all test files
file(GLOB_RECURSE TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/stdlib/**/*.aria")

# Add test target
add_custom_target(test
    COMMAND ${ARIAC_EXECUTABLE} test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running stdlib tests..."
)

# Individual suite targets
add_custom_target(test-strings
    COMMAND ${ARIAC_EXECUTABLE} test --suite strings
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(test-tbb
    COMMAND ${ARIAC_EXECUTABLE} test --suite tbb
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# CI target (fail if any test fails)
add_custom_target(test-ci
    COMMAND ${ARIAC_EXECUTABLE} test --format junit > test-results.xml
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
```

### 6.3 Test Configuration (`test_config.abc`)

```abc
[test]
# Default test runner settings
verbose = false
fail_fast = true
parallel = true
num_threads = 0  # 0 = auto-detect

# Suites to run (empty = all)
suites = []

# Test filter pattern
filter = ""

# Output format: console, json, tap, junit
format = "console"

# Show timing information
show_timing = true

# Colorized output
colorize = true

[coverage]
# Code coverage settings
enabled = false
output_dir = ".coverage"
min_coverage = 80  # Fail if below 80%
```

---

## 7. CI/CD Integration

### 7.1 GitHub Actions Example

```yaml
name: Stdlib Tests

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Build Aria
        run: |
          mkdir build && cd build
          cmake ..
          make -j$(nproc)

      - name: Run Tests
        run: |
          ./build/ariac test --format junit > test-results.xml

      - name: Upload Test Results
        uses: actions/upload-artifact@v4
        with:
          name: test-results
          path: test-results.xml

      - name: Publish Test Report
        uses: mikepenz/action-junit-report@v4
        with:
          report_paths: test-results.xml
```

### 7.2 Expected Test Output

```
=== Aria Stdlib Test Suite ===

Running: strings (5 tests)
  PASS test_concat_basic
  PASS test_concat_empty
  PASS test_concat_unicode
  PASS test_slice_basic
  PASS test_slice_bounds
Results: 5/5 passed in 12ms

Running: tbb (8 tests)
  PASS test_tbb8_add
  PASS test_tbb8_overflow
  PASS test_tbb64_multiply
  PASS test_error_propagation
  FAIL test_division_by_zero - Expected ERR, got 0
  PASS test_from_int
  PASS test_to_int
  PASS test_unwrap_or
Results: 7/8 passed (1 failed) in 23ms

Running: collections (6 tests)
  PASS test_array_create
  PASS test_array_push
  PASS test_map_insert
  PASS test_map_get
  PASS test_set_add
  PASS test_set_contains
Results: 6/6 passed in 8ms

=== Summary ===
Total: 19 tests
Passed: 18
Failed: 1
Skipped: 0
Duration: 43ms
```

---

## 8. Best Practices

### 8.1 Test Naming

- Use descriptive names: `test_concat_with_empty_string` not `test_1`
- Group related tests: `test_array_push_single`, `test_array_push_multiple`
- Include expected behavior: `test_overflow_returns_err`

### 8.2 Test Independence

- Each test should be independent (no shared mutable state)
- Use `setup()` and `teardown()` for fixtures
- Don't rely on test execution order

### 8.3 Test Coverage

- Test happy path and error cases
- Test boundary conditions (0, -1, MAX, MIN)
- Test with empty inputs
- Test with large inputs

### 8.4 Error Messages

- Include expected vs actual values
- Include context about what was being tested
- Use `string_format` for detailed messages

---

## 9. Future Enhancements

- **Property-based testing**: Generate random test inputs
- **Fuzzing integration**: Discover edge cases automatically
- **Mutation testing**: Verify test effectiveness
- **Benchmark mode**: Performance regression testing
- **Snapshot testing**: Compare output against golden files

---

*This framework ensures comprehensive, reliable testing of the Aria Standard Library while remaining native to the Aria ecosystem.*

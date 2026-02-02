# ============================================================================
# Aria Compiler - Test Suite (Minimal Configuration)
# ============================================================================

# NOTE: Most unit tests commented out temporarily to allow build to succeed
# TODO: Uncomment test suites as source files are created

# ============================================================================
# Main Test Runner (Stub)
# ============================================================================

# Minimal test runner stub
add_executable(test_runner
    test_runner.cpp
)

# Register minimal test
add_test(NAME test_runner_minimal COMMAND test_runner)

# ============================================================================
# Integration Tests
# ============================================================================

# Find all .aria files in integration directory
file(GLOB INTEGRATION_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/*.aria")

# Add integration test runner if we have .aria files
if(INTEGRATION_TESTS)
    add_test(
        NAME integration_tests
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/integration/runner.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/integration
    )
    set_tests_properties(integration_tests PROPERTIES
        TIMEOUT 600  # 10 minutes
    )
endif()

# CMakeLists.txt for Aria Unit Tests
cmake_minimum_required(VERSION 3.14)

# Include the parent project's settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Find mimalloc (should be available from parent build)
# We need to ensure mimalloc is built before tests
if(NOT TARGET mimalloc-static)
    message(STATUS "Mimalloc not found in current scope, adding from vendor")
    set(MI_BUILD_STATIC ON CACHE BOOL "Build static library")
    set(MI_BUILD_SHARED OFF CACHE BOOL "Build shared library")
    set(MI_BUILD_TESTS OFF CACHE BOOL "Build tests")
    set(MI_OVERRIDE OFF CACHE BOOL "Do not override standard malloc globally yet")
    add_subdirectory(../vendor/mimalloc ${CMAKE_BINARY_DIR}/mimalloc EXCLUDE_FROM_ALL)
endif()

include_directories(${CMAKE_SOURCE_DIR}/src)

# ==============================================================================
# Test 1: Memory Allocator Tests
# ==============================================================================
add_executable(test_allocator test_allocator.cpp
    ../src/runtime/memory/allocator.c
)

target_link_libraries(test_allocator
    PRIVATE
    mimalloc-static
    pthread  # For thread safety tests
)

add_test(NAME AllocatorTests COMMAND test_allocator)

# ==============================================================================
# Test 2: GC Header Tests
# ==============================================================================
add_executable(test_gc_header test_gc_header.cpp)

# GC header is header-only, no additional linking needed
target_include_directories(test_gc_header PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_test(NAME GCHeaderTests COMMAND test_gc_header)

# ==============================================================================
# Custom Test Target
# ==============================================================================
# Add a custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS test_allocator test_gc_header
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all Aria unit tests..."
)

# ==============================================================================
# Test Summary
# ==============================================================================
message(STATUS "Aria Unit Tests Configured:")
message(STATUS "  - test_allocator: Memory allocation tests")
message(STATUS "  - test_gc_header: GC header bitfield tests")
message(STATUS "Run 'make test' or 'ninja test' to execute all tests")

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

# ============================================================================
# K Framework Executable Semantics (v0.18.x)
# ============================================================================

if(EXISTS "${CMAKE_SOURCE_DIR}/k-semantics/run_k_tests.sh")
    add_test(
        NAME k_semantics_core
        COMMAND ${CMAKE_SOURCE_DIR}/k-semantics/run_k_tests.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(k_semantics_core PROPERTIES
        TIMEOUT 120
        LABELS "formal;k-semantics"
        SKIP_RETURN_CODE 77
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/k-semantics/run_k_proofs.sh")
    add_test(
        NAME k_semantics_proofs
        COMMAND bash ${CMAKE_SOURCE_DIR}/k-semantics/run_k_proofs.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(k_semantics_proofs PROPERTIES
        TIMEOUT 180
        LABELS "formal;k-semantics;kprove"
        SKIP_RETURN_CODE 77
    )
endif()

# ============================================================================
# GPU Tests (target-conditional)
# ============================================================================

# GPU test runner — auto-detects CUDA toolkit and GPU availability
# Tests: PTX validation, --emit-ptx codegen, CUDA C++ compilation
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gpu/run_gpu_tests.sh")
    add_test(
        NAME gpu_tests
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/gpu/run_gpu_tests.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gpu
    )
    set_tests_properties(gpu_tests PROPERTIES
        TIMEOUT 120  # 2 minutes
        LABELS "gpu"
    )

    # Optional: GPU tests with --skip-cuda for CI environments without GPU
    add_test(
        NAME gpu_tests_ptx_only
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/gpu/run_gpu_tests.sh --skip-cuda
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gpu
    )
    set_tests_properties(gpu_tests_ptx_only PROPERTIES
        TIMEOUT 60
        LABELS "gpu;ptx"
    )
endif()

# ============================================================================
# Module Loading Tests
# ============================================================================

# Diamond import test: A→B→D, A→C→D — verifies module cache deduplication
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/module_loading/diamond_test.aria")
    add_test(
        NAME module_loading_diamond
        COMMAND ${CMAKE_BINARY_DIR}/ariac
            ${CMAKE_CURRENT_SOURCE_DIR}/module_loading/diamond_test.aria
            -o ${CMAKE_BINARY_DIR}/test_diamond
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/module_loading
    )
    set_tests_properties(module_loading_diamond PROPERTIES
        TIMEOUT 30
        LABELS "module_loading"
    )

    # Run the compiled diamond test
    add_test(
        NAME module_loading_diamond_run
        COMMAND ${CMAKE_BINARY_DIR}/test_diamond
    )
    set_tests_properties(module_loading_diamond_run PROPERTIES
        TIMEOUT 10
        LABELS "module_loading"
        DEPENDS module_loading_diamond
    )
endif()

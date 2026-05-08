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

# Find all .npk files in integration directory
file(GLOB INTEGRATION_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/*.npk")

# Add integration test runner if we have .npk files
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
        TIMEOUT 600
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
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/module_loading/diamond_test.npk")
    add_test(
        NAME module_loading_diamond
        COMMAND ${CMAKE_BINARY_DIR}/npkc
            ${CMAKE_CURRENT_SOURCE_DIR}/module_loading/diamond_test.npk
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

# ============================================================================
# KLEE Symbolic Execution (v0.18.4)
# ============================================================================

if(EXISTS "${CMAKE_SOURCE_DIR}/tools/klee/aria_klee.sh")
    add_test(
        NAME klee_symbolic_execution
        COMMAND bash ${CMAKE_SOURCE_DIR}/tools/klee/aria_klee.sh
            --test-dir ${CMAKE_SOURCE_DIR}/tools/klee/tests
            --out-dir  ${CMAKE_BINARY_DIR}/klee-results
            --timeout  60
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tools/klee
    )
    set_tests_properties(klee_symbolic_execution PROPERTIES
        TIMEOUT 300
        LABELS "symbolic;klee;v0.18.4"
        SKIP_RETURN_CODE 77
        ENVIRONMENT "NPKC=${CMAKE_BINARY_DIR}/npkc;STUBS_BC=${CMAKE_SOURCE_DIR}/tools/klee/aria_rt_stubs.bc"
    )
endif()

# ============================================================================
# Juliet CWE Test Suite (v0.18.5)
# ============================================================================

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/juliet/run_juliet.sh")
    add_test(
        NAME juliet_cwe_tests
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/juliet/run_juliet.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/juliet
    )
    set_tests_properties(juliet_cwe_tests PROPERTIES
        TIMEOUT 300
        LABELS "security;juliet;cwe;v0.18.5"
        ENVIRONMENT "NPKC=${CMAKE_BINARY_DIR}/npkc"
    )
endif()

# ============================================================================
# SV-COMP Benchmark Suite (v0.18.6)
# ============================================================================

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/sv-comp/run_svcomp.sh")
    add_test(
        NAME svcomp_benchmarks
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/sv-comp/run_svcomp.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/sv-comp
    )
    set_tests_properties(svcomp_benchmarks PROPERTIES
        TIMEOUT 300
        LABELS "security;svcomp;v0.18.6"
        ENVIRONMENT "NPKC=${CMAKE_BINARY_DIR}/npkc"
    )
endif()

# ============================================================================
# Warning System Tests (v0.20.0)
# ============================================================================

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/warnings/run_warning_tests.sh")
    add_test(
        NAME warning_system_tests
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/warnings/run_warning_tests.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/warnings
    )
    set_tests_properties(warning_system_tests PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "warnings;diagnostics;v0.20.0"
    )
endif()

# ============================================================================
# Preprocessor Tests (v0.20.1)
# ============================================================================

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/preprocessor/run_preprocessor_tests.sh")
    add_test(
        NAME preprocessor_tests
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/preprocessor/run_preprocessor_tests.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/preprocessor
    )
    set_tests_properties(preprocessor_tests PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "preprocessor;macros;v0.20.1"
    )
endif()

# ============================================================================
# Template Literal & Display Trait Tests (v0.20.2)
# ============================================================================

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/template_literal/run_display_tests.sh")
    add_test(
        NAME template_literal_display_tests
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/template_literal/run_display_tests.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/template_literal
    )
    set_tests_properties(template_literal_display_tests PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "template_literal;display;trait;v0.20.2"
    )
endif()

# ============================================================================
# Comptime Evaluator Tests (v0.20.3)
# ============================================================================

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/comptime/run_comptime_tests.sh")
    add_test(
        NAME comptime_evaluator_tests
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/comptime/run_comptime_tests.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/comptime
    )
    set_tests_properties(comptime_evaluator_tests PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "comptime;const_eval;memoization;v0.20.3"
    )
endif()

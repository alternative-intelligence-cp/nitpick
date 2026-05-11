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

# ============================================================================
# Closure Lifetime Tests (v0.20.4)
# ============================================================================

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/closures/run_closure_tests.sh")
    add_test(
        NAME closure_lifetime_tests
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/closures/run_closure_tests.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/closures
    )
    set_tests_properties(closure_lifetime_tests PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "closures;lifetime;capture;v0.20.4"
    )
endif()

# ============================================================================
# Optional Safe Navigation Tests (v0.20.4)
# ============================================================================

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/optional/run_optional_tests.sh")
    add_test(
        NAME optional_safe_nav_tests
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/optional/run_optional_tests.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/optional
    )
    set_tests_properties(optional_safe_nav_tests PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "optional;safe_nav;v0.20.4"
    )
endif()

# ============================================================================
# Vec9 Dynamic Indexing Tests (v0.20.5)
# ============================================================================

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/vec/run_vec_tests.sh")
    add_test(
        NAME vec9_dynamic_index_tests
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/vec/run_vec_tests.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/vec
    )
    set_tests_properties(vec9_dynamic_index_tests PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "vec9;dynamic_index;v0.20.5"
    )
endif()

# ============================================================================
# cfg Conditional Compilation + derive(Display) + Preprocessor Hardening (v0.21.1)
# ============================================================================

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0211.sh")
    add_test(
        NAME bug_tests_v0211
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0211.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/bugs
    )
    set_tests_properties(bug_tests_v0211 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "cfg;derive;display;catch;preprocessor;v0.21.1"
    )
endif()

# ============================================================================
# Async Borrow Checking (A-003) + Rules/Z3 Index Disjointness (A-004) (v0.21.2)
# ============================================================================

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0212.sh")
    add_test(
        NAME bug_tests_v0212
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0212.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/bugs
    )
    set_tests_properties(bug_tests_v0212 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "async;borrow;await;ARIA-030;disjoint;z3;v0.21.2"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0213.sh")
    add_test(
        NAME bug_tests_v0213
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0213.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/bugs
    )
    set_tests_properties(bug_tests_v0213 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "sema;pick;exhaustiveness;int32;int64;ICE;A-008;A-013;A-014;fixed;v0.21.3"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0214.sh")
    add_test(
        NAME bug_tests_v0214
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0214.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/bugs
    )
    set_tests_properties(bug_tests_v0214 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "backend;ir;codegen;jit;aarch64;DAP;abi;A-005;A-006;A-007;A-015;v0.21.4"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0215.sh")
    add_test(
        NAME bug_tests_v0215
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0215.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0215 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "stdlib;ahash;modules;A-011;A-012;v0.21.5"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0216.sh")
    add_test(
        NAME bug_tests_v0216
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0216.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0216 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "k-semantics;tbb8;tbb16;tbb64;async;await;A-009;A-010;v0.21.6"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0221.sh")
    add_test(
        NAME bug_tests_v0221
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0221.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0221 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "warnings;unused-variable;POLISH-012;POLISH-014;v0.22.1"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0222.sh")
    add_test(
        NAME bug_tests_v0222
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0222.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0222 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "builtins;module-resolver;struct-types;POLISH-001;POLISH-002;POLISH-010;v0.22.2"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0223.sh")
    add_test(
        NAME bug_tests_v0223
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0223.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0223 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "builtins;cli-args;loop-control;POLISH-003;POLISH-011;v0.22.3"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0224.sh")
    add_test(
        NAME bug_tests_v0224
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0224.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0224 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "pick;bitwise;integer-types;POLISH-006;POLISH-007;v0.22.4"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0225.sh")
    add_test(
        NAME bug_tests_v0225
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0225.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0225 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "lexer;escapes;unicode;POLISH-009;v0.22.5"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0226.sh")
    add_test(
        NAME bug_tests_v0226
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0226.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0226 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "parser;diagnostics;POLISH-008;v0.22.6"
    )
endif()

# v0.23.0: Macro system smoke test — basic pipeline end-to-end (MACRO-001)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_macro_basic.npk")
    add_test(
        NAME macro_smoke_compile
        COMMAND ${CMAKE_BINARY_DIR}/npkc
            ${CMAKE_CURRENT_SOURCE_DIR}/test_macro_basic.npk
            -o ${CMAKE_BINARY_DIR}/test_macro_smoke
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_tests_properties(macro_smoke_compile PROPERTIES
        TIMEOUT 30
        LABELS "macros;MACRO-001;v0.23.0"
    )

    add_test(
        NAME macro_smoke_run
        COMMAND ${CMAKE_BINARY_DIR}/test_macro_smoke
    )
    set_tests_properties(macro_smoke_run PROPERTIES
        DEPENDS macro_smoke_compile
        TIMEOUT 10
        LABELS "macros;MACRO-001;v0.23.0"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0231.sh")
    add_test(
        NAME bug_tests_v0231
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0231.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0231 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "macros;MACRO-001;MACRO-002;v0.23.1"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0232.sh")
    add_test(
        NAME bug_tests_v0232
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0232.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0232 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "macros;MACRO-003;v0.23.2"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0233.sh")
    add_test(
        NAME bug_tests_v0233
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0233.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0233 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "macros;MACRO-004;v0.23.3"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0234.sh")
    add_test(
        NAME bug_tests_v0234
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0234.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0234 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "macros;MACRO-005;v0.23.4"
    )
endif()

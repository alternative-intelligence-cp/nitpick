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

    # v0.23.5: Statement macros (MACRO-006) & code-generating macros (MACRO-007)
    add_test(
        NAME bug_tests_v0235
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0235.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0235 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "macros;MACRO-006;MACRO-007;v0.23.5"
    )

    # v0.23.6: Built-in macros (MACRO-008..011)
    add_test(
        NAME bug_tests_v0236
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0236.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0236 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "macros;MACRO-008;MACRO-009;MACRO-010;MACRO-011;v0.23.6"
    )

    # v0.23.7: --expand-macros debug flag (MACRO-012)
    add_test(
        NAME bug_tests_v0237
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0237.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0237 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "macros;MACRO-012;v0.23.7"
    )

    # v0.24.1: loop() + mutable locals in CTFE (COMPTIME-001, COMPTIME-002)
    add_test(
        NAME bug_tests_v0241
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0241.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0241 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "comptime;COMPTIME-001;COMPTIME-002;v0.24.1"
    )

    # v0.24.2: @sizeof, @alignof, comptime string ops (COMPTIME-003, 004, 005)
    add_test(
        NAME bug_tests_v0242
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0242.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0242 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "comptime;COMPTIME-003;COMPTIME-004;COMPTIME-005;v0.24.2"
    )

    # v0.24.3: macro × comptime interaction (COMPTIME-006, COMPTIME-007)
    add_test(
        NAME bug_tests_v0243
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0243.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0243 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "comptime;COMPTIME-006;COMPTIME-007;v0.24.3"
    )

    # v0.24.4: assert_static+comptime + diagnostics (COMPTIME-008, COMPTIME-009)
    add_test(
        NAME bug_tests_v0244
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0244.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0244 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "comptime;COMPTIME-008;COMPTIME-009;v0.24.4"
    )

    # v0.24.5: comptime generics — `type:T` parameters (COMPTIME-010)
    add_test(
        NAME bug_tests_v0245
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0245.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0245 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "comptime;COMPTIME-010;v0.24.5"
    )

    # v0.24.6: limit<Rules> comptime + comptime limitations (COMPTIME-011, COMPTIME-012)
    add_test(
        NAME bug_tests_v0246
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0246.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0246 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "comptime;COMPTIME-011;COMPTIME-012;v0.24.6"
    )

    # v0.24.7: struct field reflection via @typeInfo + @fieldType (COMPTIME-013)
    add_test(
        NAME bug_tests_v0247
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0247.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0247 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "comptime;COMPTIME-013;v0.24.7"
    )

    # v0.25.0: borrow checker triage — till body now visited (BORROW-001)
    add_test(
        NAME bug_tests_v0250
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0250.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0250 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "borrow;BORROW-001;v0.25.0"
    )

    # v0.25.1: defer body + early-exit borrow release (BORROW-002, BORROW-003)
    add_test(
        NAME bug_tests_v0251
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0251.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0251 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "borrow;BORROW-002;BORROW-003;v0.25.1"
    )

    # v0.25.2: multi-dim & nested array borrow paths (BORROW-004)
    add_test(
        NAME bug_tests_v0252
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0252.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0252 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "borrow;BORROW-004;v0.25.2"
    )

    # v0.25.3: deep struct paths + ptr->field borrows (BORROW-005, BORROW-006)
    add_test(
        NAME bug_tests_v0253
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0253.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0253 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "borrow;BORROW-005;BORROW-006;v0.25.3"
    )

    # v0.25.4: inter-procedural borrow summaries (BORROW-007, BORROW-008)
    add_test(
        NAME bug_tests_v0254
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0254.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0254 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "borrow;BORROW-007;BORROW-008;v0.25.4"
    )

    # v0.25.5: two-phase borrows + $$m self in trait impls (BORROW-009, BORROW-010)
    add_test(
        NAME bug_tests_v0255
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0255.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0255 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "borrow;BORROW-009;BORROW-010;v0.25.5"
    )

    # v0.25.6: closure capture borrow + multi-await polish (BORROW-011, BORROW-012, BORROW-013)
    add_test(
        NAME bug_tests_v0256
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0256.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0256 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "borrow;BORROW-011;BORROW-012;BORROW-013;v0.25.6"
    )

    # v0.26.1: stack-escape (MEM-005) regression suite — locks in ARIA-017
    # behavior for `stack` keyword and chained borrow paths.
    add_test(
        NAME bug_tests_v0261
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0261.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0261 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "borrow;memory;MEM-005;v0.26.1"
    )

    # v0.26.2: GC stress smoke tests (MEM-006 alloc stress, MEM-007 large objs,
    # MEM-008 long-lived survivor under churn).
    add_test(
        NAME bug_tests_v0262
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0262.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0262 PROPERTIES
        TIMEOUT 180
        SKIP_RETURN_CODE 77
        LABELS "memory;gc;MEM-006;MEM-007;MEM-008;v0.26.2"
    )

    # v0.26.3: GC borrow + safepoint interaction (MEM-009).
    # MEM-010 (pin syntax `#x`) deferred — runtime exists, lowering pending.
    add_test(
        NAME bug_tests_v0263
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0263.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0263 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "memory;gc;borrow;MEM-009;v0.26.3"
    )

    # v0.26.3.0 (PIN baseline): lock in current `#x` behavior before the
    # pin-lowering work in v0.26.3.{1..5}. See
    # META/NITPICK/ROADMAP/0.26/0.26.3.x/PIN_LOWERING_AUDIT.md.
    add_test(
        NAME bug_tests_v02630
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_02630.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v02630 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "memory;gc;pin;PIN-001;PIN-002;v0.26.3.0"
    )

    # v0.26.3.1 (allocation qualifier wire-up): regression tests that lock in
    # the fix where `gc T:x` lowers to npk_gc_alloc, `wild T:x` lowers to
    # npk_alloc, and `stack T:x` keeps emitting alloca. Before v0.26.3.1 the
    # qualifier was silently ignored and every binding hit the stack.
    add_test(
        NAME bug_tests_v02631
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_02631.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v02631 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "memory;gc;wild;ir;v0.26.3.1"
    )

    # v0.26.3.2 (GC root tracking): regression tests that lock in the fix
    # where every `gc T:x` binding registers a shadow-stack root and pins
    # the heap object, so the value survives a collection cycle.
    add_test(
        NAME bug_tests_v02632
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_02632.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v02632 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "memory;gc;root;shadow_stack;v0.26.3.2"
    )

    # v0.26.3.3 (MEM-010 reinstatement): execution-level regression tests
    # that close the v0.26.3 deferral. bug224 verifies a gc binding
    # survives an explicit npk_gc_safepoint() loop without any borrow or
    # pin alias; bug225 verifies the `wild T->:p = #h;` alias survives
    # the same safepoint loop and reads through correctly.
    add_test(
        NAME bug_tests_v02633
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_02633.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v02633 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "memory;gc;pin;safepoint;MEM-010;v0.26.3.3"
    )

    # v0.26.4 (MEM-011 GC tuning env-vars): four execution-level
    # fixtures that exercise NPK_GC_NURSERY_SIZE, NPK_GC_OLD_GEN_THRESHOLD,
    # NPK_GC_MODE=concurrent, and the no-env-var default fallback. Each
    # case spawns a fresh process so GCState::init() reads the env vars
    # before the singleton is initialized.
    add_test(
        NAME bug_tests_v0264
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0264.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0264 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "memory;gc;tuning;MEM-011;v0.26.4"
    )

    # v0.26.5 (MEM-013 wild ↔ gc interop): two execution fixtures
    # verifying the runtime cleanly partitions gc-heap pointers from
    # wild-allocator pointers (bug230), and that wild alloc/free
    # churn around npk_gc_safepoint() never perturbs gc reachability
    # (bug231).
    add_test(
        NAME bug_tests_v0265
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0265.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0265 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "memory;gc;interop;MEM-013;v0.26.5"
    )

    # v0.26.6 (MEM-014 ARIA-028 STACK_ESCAPE polish): four fixtures.
    # bug232 (escape via fail), bug233 (escape from nested block),
    # bug234 (polished message contents grep), bug235 (positive
    # internal-only borrow). The pre-existing v0.26.1 suite was also
    # re-tagged ARIA-017 → ARIA-028 in this slice.
    add_test(
        NAME bug_tests_v0266
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0266.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0266 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "memory;diagnostics;ARIA-028;MEM-014;v0.26.6"
    )

    # v0.27.2 (ARIA-029 GC_REF_FROM_WILD): three fixtures.
    # bug237 (gc binding into wild slot, no pin → reject),
    # bug238 (pin-derived #h positive), bug239 (raw wild alloc
    # positive — no gc origin so ARIA-029 doesn't fire).
    # Consumes the per-binding Region map landed in v0.27.1
    # (MEM-DEC-007). Indirect-through-struct-field companion
    # case is deferred to v0.27.3 with field-region propagation.
    add_test(
        NAME bug_tests_v0272
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0272.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0272 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "memory;diagnostics;ARIA-029;MEM-DEC-007;v0.27.2"
    )

    # v0.27.3 / ARIA-031 STACK_REF_INTO_GC_FIELD diagnostic.
    # bug240 rejects `h.p = #x` where `h` is gc and `x` is stack
    # (use-after-free if the gc object outlives the stack frame).
    # bug241 (stack→stack) and bug242 (gc→gc) are negative
    # controls — neither shape may regress into ARIA-031. Consumes
    # the per-binding Region map landed in v0.27.1 (MEM-DEC-007)
    # plus the pin/address-of source classification added in
    # v0.27.3.
    add_test(
        NAME bug_tests_v0273
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0273.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0273 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "memory;diagnostics;ARIA-031;MEM-DEC-007;v0.27.3"
    )

    # v0.27.5 WildX W^X lifecycle hardening.
    # bug245 exercises the full alloc -> seal -> free state machine
    # through the new surface builtins `wildx_alloc`, `wildx_seal`,
    # `wildx_free`, which route through a process-wide pointer ->
    # WildXGuard registry in src/runtime/allocators/wildx_alloc.cpp.
    # bug246 confirms the runtime rejects a double-seal (the
    # WRITABLE -> EXECUTABLE transition is one-way). bug247 and bug248
    # confirm the borrow checker still rejects double-free and leaks
    # for the new builtins at compile time, so callers cannot reach
    # the runtime mistakes covered by tests/runtime/test_wildx_security.cpp.
    add_test(
        NAME bug_tests_v0275
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0275.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0275 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "memory;wildx;security;v0.27.5"
    )

    # v0.27.6 wild ↔ borrow checker interaction polish.
    # Codifies that the borrow checker is NOT silently bypassed for
    # `wild` bindings — v0.27.1/v0.27.2 region propagation already
    # treats `$$i wild T:b = wild_buf;` as an Aria-visible borrow
    # that obeys mut-XOR-aliasing (PLAN option (b)). bug249 confirms
    # the immutable borrow path compiles and runs; bug250 confirms
    # two overlapping `$$m` borrows are still rejected (ARIA-023);
    # bug251 confirms passing a wild buffer to `$$i`/`$$m` parameters
    # and free-ing after the borrow drops works end-to-end.
    add_test(
        NAME bug_tests_v0276
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0276.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0276 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "memory;wild;borrow;v0.27.6"
    )
endif()

# ============================================================================
# v0.27.7 — Handle<T> Phase 1 runtime ABI (bug252-bug255)
# ============================================================================
# Pure-runtime test: links libaria_runtime and exercises the npk_handle_*
# C ABI directly. No compiler involvement, no K test (runtime-only slice).
add_executable(test_handle_v0277 runtime/test_handle_v0277.cpp)
target_link_libraries(test_handle_v0277 PRIVATE aria_runtime)
target_include_directories(test_handle_v0277 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
add_test(
    NAME runtime_handle_v0277
    COMMAND test_handle_v0277
)
set_tests_properties(runtime_handle_v0277 PROPERTIES
    TIMEOUT 60
    LABELS "memory;handles;runtime;v0.27.7"
)

# ============================================================================
# v0.27.8 — Handle<T> Phase 2 typed surface (bug256-bug259)
# ============================================================================
# Exercises the Aria-side Handle<T> + HandleArena module on top of the
# v0.27.7 npk_handle_* C ABI. bug256 round-trips alloc/deref/free; bug257
# locks in that Handle<int32> ≠ Handle<int64> at the type checker;
# bug258 confirms use-after-free deref returns NULL (not stale pointer);
# bug259 confirms arena destroy invalidates all of its outstanding handles.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0278
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0278.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0278 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "memory;handles;v0.27.8"
    )
endif()

# ============================================================================
# v0.27.9 — Handle<T> Phase 3: borrow-checker tying + ARIA-032 (bug260-bug263)
# ============================================================================
# Static rejection of `HandleArena.deref(h)` / `HandleArena.free(h)` after the
# arena that backed `h` has been destroyed. bug260/261 are compile-fail cases
# (ARIA-032); bug262/263 are positive controls confirming the rule only fires
# when destroy precedes a use of a handle tied to that exact arena.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0279
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0279.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0279 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "memory;handles;borrow;v0.27.9"
    )
endif()

# ============================================================================
# v0.28.1 — JIT end-to-end smoke (bug264)
# ============================================================================
# Closes the wildx W^X loop opened in v0.27.5: a Nitpick program allocates
# a wildx page, installs hand-assembled x86-64 machine code for
# `int32 add(int32, int32)` via npk_jit_install_add_i32, seals the page,
# invokes the JIT'd function via npk_jit_call_i32_i32, and frees the page.
# x86-64 only this cycle (JIT-DEC-002); the runner self-skips on other
# architectures.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0281
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0281.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0281 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "jit;wildx;v0.28.1"
    )
endif()

# bug_tests_v0282 — JIT helper API via stdlib/jit.npk (v0.28.2).
# Same x86-64-only constraint (JIT-DEC-002); runner self-skips elsewhere.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0282
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0282.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0282 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "jit;wildx;v0.28.2"
    )
endif()

# bug_tests_v0283 — Cross-function ARIA-032 Phase 1 (v0.28.3).
# Verifies that callees calling HandleArena_destroy(<param>) propagate
# arena death back to the caller so subsequent handle use is rejected.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0283
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0283.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0283 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "borrow;handles;aria-032;v0.28.3"
    )
endif()

# bug_tests_v0284 — Cross-function ARIA-032 Phase 2 part A (v0.28.4).
# Rejects returning a Handle bound to a locally-created arena; allows
# returning a Handle bound to an arena threaded in as a parameter.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0284
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0284.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0284 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "borrow;handles;aria-032;v0.28.4"
    )
endif()

# bug_tests_v02841 — Cross-function ARIA-032 Phase 2 part B (v0.28.4.1).
# Rejects returning a struct whose Handle field is bound to a locally-
# created arena; allows the same shape when arena is threaded as param.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v02841
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_02841.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v02841 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "borrow;handles;aria-032;v0.28.4.1"
    )
endif()

# bug_tests_v0285 — ARIA-032 FFI passthrough rule (v0.28.5).
# Warns when a tracked Handle<T> is passed directly to an extern
# function; the explicit @cast<int64>(h) wrapper silences the
# warning; runtime generation check remains the universal backstop.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0285
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0285.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0285 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "borrow;handles;aria-032;ffi;v0.28.5"
    )
endif()

# bug_tests_v0291 — Drop trait parser+sema surface (v0.29.1, DROP-DEC-001).
# Adds the builtin `Drop` trait special-case: `impl:Drop:for:T = { func:drop = NIL($$m T:self) { ... }; };`
# parses and sema-validates without requiring a user-declared `trait:Drop`.
# Exactly one `drop` method per type; method must be named 'drop',
# return NIL, take exactly one self-like parameter; duplicate Drop impls
# rejected; unknown types rejected. The existing `drop <expr>` unary
# discard form is preserved (DROP-DEC-007a regression). Runtime auto-call
# arrives in v0.29.2.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0291
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0291.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0291 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "drop;trait;sema;parser;v0.29.1"
    )
endif()

# bug_tests_v0292 — Drop codegen for stack-scoped bindings (v0.29.2, DROP-DEC-003).
# Adds automatic `T_drop(&local)` emission at block-scope exit for stack
# locals whose static type has a registered `impl:Drop:for:T`. Leaf cases
# only (normal fall-through); pass/fail/return early-exit paths are
# deferred to v0.29.6. Reverse declaration order (LIFO, DROP-DEC-003),
# drops emitted BEFORE defers (DROP-DEC-010), and no overreach on
# Drop-less types — all asserted via emitted LLVM IR plus exit-code
# runtime check.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0292
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0292.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0292 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "drop;trait;sema;codegen;v0.29.2"
    )
endif()

# bug_tests_v0293 — Wild RAII opt-in via `use "drop.npk".*;` (v0.29.3,
# DROP-DEC-007). Importing the stdlib sentinel `NitpickWildRaii` toggles
# automatic `npk_free(...)` emission at scope end for canonical
# `wild T:x = T{...}` struct bindings, with the borrow checker's
# leak-on-early-exit guard (ARIA-014) suppressed for those bindings.
# Scope: leaf canonical wild-struct only — pointer-typed wild and
# `wildx` are deferred to v0.29.3b/v0.29.4. Asserted via emitted LLVM
# IR (`npk_free` calls present), runtime exit code, and an opt-in
# negative test (bug290) proving ARIA-014 still fires without the
# import.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0293
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0293.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0293 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "drop;wild;raii;codegen;v0.29.3"
    )
endif()

# bug_tests_v0294 — Wildx RAII opt-in via `use "drop.npk".*;` (v0.29.4,
# DROP-DEC-007). Importing the stdlib sentinel `NitpickWildxRaii`
# toggles automatic `npk_wildx_free(...)` emission at scope end for
# `wildx T->:p = wildx_alloc(N);` bindings, with the borrow checker's
# leak-on-early-exit guard (ARIA-014) suppressed for those bindings.
# Scope: `wildx + wildx_alloc(...)` call-initializer only — pointer-
# typed wild and raw-pointer wild stay on the explicit-free contract.
# Asserted via emitted LLVM IR (`npk_wildx_free` calls present),
# runtime exit code, and an opt-in negative test (bug293) proving
# ARIA-014 still fires without the import.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0294
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0294.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0294 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "drop;wildx;raii;codegen;v0.29.4"
    )
endif()

# bug_tests_v0295 — HandleArena RAII opt-in via `use "drop.npk".*;`
# (v0.29.5, DROP-DEC-007). The stdlib sentinel `NitpickHandleArenaRaii`
# toggles automatic `npk_handle_arena_destroy(...)` emission at scope
# end for `int64:a = HandleArena.create();` bindings. Handle<T> values
# stay on the explicit-`HandleArena.free(h)` contract; the existing
# ARIA-032 "handle outlives arena" borrow check continues to fire.
# Validates: auto-destroy IR emission, LIFO order for multiple arenas,
# pre-RAII contract preserved without the import, and ARIA-032 still
# rejects local-arena handle escape.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0295
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0295.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0295 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "drop;handle;raii;codegen;v0.29.5"
    )
endif()

# bug_tests_v0296 — JitFn RAII opt-in via `use "drop.npk".*;`
# (v0.29.6, DROP-DEC-007). The stdlib sentinel `NitpickJitFnRaii`
# toggles automatic `npk_wildx_free(...)` emission at scope end for
# `wildx int8->:f = Jit.compile_add_i32();` bindings, closing the
# RAII followup explicitly flagged in JIT-DEC-003. Validates: auto-
# free IR emission, LIFO order for multiple JIT pages, and that the
# pre-RAII explicit-`Jit.free` contract is preserved without the
# import (no compiler-inserted double-free).
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0296
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0296.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0296 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "drop;jit;raii;codegen;v0.29.6"
    )
endif()

# bug_tests_v0297 — Drop ordering on early-exit paths (v0.29.7,
# DROP-DEC-004 + DROP-DEC-008 + DROP-DEC-010). Wires
# `executeAllScopeDrops()` into the PASS / FAIL / RETURN code paths
# so RAII bindings are dropped on `pass v` / `fail e` / `return ...`
# in reverse declaration order, innermost-out across nested scopes,
# BEFORE function-scope defers. Hard `exit N` continues to skip
# drops (DROP-DEC-008). Bare-identifier `pass v` / `return v` skips
# the moved binding's drop (DROP-DEC-004 move semantics). Closes
# the heaviest slice in the v0.29.x cycle.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0297
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0297.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0297 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "drop;ordering;pass;fail;return;codegen;v0.29.7"
    )
endif()

# ============================================================================
# v0.30.1 — ARIA-032 transitive destroy fixpoint (cross-function Phase 2)
# ============================================================================
# Lifts `destroys_param_indices` through callee summaries via a
# fixpoint pass in `BorrowChecker::propagateTransitiveDestroys`, so
# handle-outlives-arena (ARIA-032) catches arenas destroyed by a
# callee-of-a-callee (not just direct callees as in v0.28.3). Per
# IPC-DEC-004, new transitive cases ship as WARNINGS for one cycle
# before promotion to error. Closes bug269 (v0.28.3 documented gap).
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0301
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0301.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0301 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "aria-032;transitive;ipc;handles;v0.30.1"
    )
endif()

# ============================================================================
# v0.30.2 — ARIA-032 transitive arena escape (bug316-320)
# ============================================================================
# Extension of v0.28.4's local-arena escape rule: a function whose return
# value is a handle bound to one of its arena parameters now carries an
# `escapes_param_arena_indices` summary. Caller sites passing a LOCAL
# arena to such a callee trigger an ARIA-032 WARNING (IPC-DEC-004 one-
# cycle migration window). Fixpoint pass also lifts escapes through
# arbitrary call chains.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0302
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0302.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0302 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "aria-032;transitive;ipc;handles;escape;v0.30.2"
    )
endif()

# ============================================================================
# v0.30.3 — Cross-module function summary ingest (IPC-DEC-002/003)
# ============================================================================
# Borrow checker now seeds `func_summaries` from all imported module ASTs
# before analysing the main translation unit. Local re-declarations
# overwrite imported summaries on collision (local wins). This retires
# the bug277 local-extern re-declaration workaround.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0303
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0303.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0303 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "aria-032;cross-module;ipc;handles;v0.30.3"
    )
endif()

# ============================================================================
# v0.30.4 — `#[destroys_arena(<param>)]` declarative attribute (IPC-DEC-005)
# ============================================================================
# Closes the gap that cross-module ingest (v0.30.3) exposed: extern
# declarations have no body for the borrow checker's
# `HandleArena_destroy(<param>)` body-scan to find. The attribute lets
# extern (and self-hosted) declarations declare arena-destroying
# parameters explicitly; the consumer site unions the attribute set
# with the body-derived set so both sources are treated uniformly.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0304
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0304.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0304 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "aria-032;ffi;destroys-arena;ipc;handles;v0.30.4"
    )
endif()

# ============================================================================
# v0.30.5 — Return-borrow inference across modules (v0.25.x carryover)
# ============================================================================
# Pins the cross-module behaviour of `return_borrow_source_param`. The
# v0.25.4 single-source heuristic and the consumer at the borrow-binding
# site already work transparently once v0.30.3's cross-module ingest
# seeds imported summaries via `buildSummary`. These four fixtures lock
# the contract in place so future regressions show up here.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0305
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0305.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0305 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "aria-023;aria-026;borrow;return-borrow;cross-module;v0.30.5"
    )
endif()

# ============================================================================
# v0.30.6 — ARIA-032 precision pass
# ============================================================================
# Drops two false-positive corner cases that v0.30.1–5 surfaced in the
# per-name destroyed-arena tracker:
#   * Reassigning an arena local to a fresh `HandleArena.create()`
#     produces a NEW arena; the stale destroyed marker on the bound
#     name is now cleared (borrow_checker.cpp checkAssignment).
#   * Destroying an arena inside a branch that terminates the
#     function (`exit`/`pass`/`fail`/`return`) no longer leaks into
#     the outer flow (borrow_checker.cpp checkIfStmt snapshot/restore).
# bug330 + bug333 are regression-pins that passed pre-fix; bug331 +
# bug332 are the real fixes.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v0306
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_0306.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v0306 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "aria-032;precision;ipc;handles;v0.30.6"
    )
endif()

# ============================================================================
# v0.31.0.1 — Phase 1 async slice: D-2 + D-3
# ============================================================================
# bug334 — `await` outside an `async func:` is now a hard ARIA-040 error
#          (was a soft-fail printing to stderr but returning 0).
# bug335 — async-bearing modules emit a `npk_executor_run(NULL)` drain
#          call right before the user `exit(code)` in main, so any
#          tasks queued on the global executor are completed before the
#          process terminates.
# bug336 — sync-only modules must NOT reference `npk_executor_run` in
#          their IR (zero-overhead invariant).
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v03101
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_03101.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v03101 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "async;phase1;executor;await;v0.31.0.1"
    )
endif()

# ============================================================================
# v0.31.0.2 — Phase 1 async slice: D-8 Option A (pull `catch` keyword)
# ============================================================================
# bug337 — `catch` is no longer reserved; usable as an ordinary identifier.
# bug338 — `try { ... } catch { ... }` is still rejected (generic parser
#          path now, since neither word is a keyword anymore).
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v03102
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_03102.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v03102 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "async;phase1;keyword;catch;v0.31.0.2"
    )
endif()

# ============================================================================
# v0.31.0.3 — Phase 1 async slice: D-7 (`pass`/`fail` short-circuit in async fns)
# ============================================================================
# bug339 — `fail` mid-body: dead code after must not execute.
# bug340 — `pass` mid-body: dead code after must not execute.
# bug341 — `fail` inside a `while` loop must exit the function, not just the loop.
# bug342 — `fail` inside nested `if` blocks must exit the function.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v03103
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_03103.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v03103 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "async;phase1;pass;fail;d-7;v0.31.0.3"
    )
endif()

# ============================================================================
# v0.31.0.4 — Phase 1 async slice: D-5 (`ARIA-041` borrows-across-await)
# ============================================================================
# bug343..bug350 — eight fixtures covering the new contract:
#   - $$m and $$i live across await both fail with ARIA-041;
#   - borrows released before / re-acquired after await compile clean;
#   - multi-borrow + multi-await + value-copy workaround patterns.
#
# checkAwaitExpr() in src/frontend/sema/borrow_checker.cpp was extended to
# emit ARIA-041 (ERROR) per active loan and to drop the loans afterward so
# downstream code can re-borrow. This supersedes the v0.21.2 ARIA-030
# warning, which only flagged $$m and was non-fatal.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v03104
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_03104.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v03104 PROPERTIES
        TIMEOUT 120
        SKIP_RETURN_CODE 77
        LABELS "async;phase1;borrow;await;d-5;v0.31.0.4"
    )
endif()

# ============================================================================
# v0.31.0.5 — Phase 1 async slice: D-10 (Async + Drop interaction)
# ============================================================================
# bug351..bug353 — verification fixtures (no compiler change):
#   - bug351: Drop fires when an async coroutine completes normally.
#   - bug352: Drop fires during the `fail` unwind in an async body.
#   - bug353: nested-block Drop is LIFO and fires at the inner `}` (not
#             deferred to the end of the coroutine frame).
#
# Audit scenario (b) "executor shutdown of a suspended task" is NOT
# covered: no user-surface awaitable in v0.31.x produces real suspension,
# so a suspended-at-shutdown task is unreachable from `.npk`. Deferred.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v03105
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_03105.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v03105 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "async;phase1;drop;d-10;v0.31.0.5"
    )
endif()

# ============================================================================
# v0.28.6 — Concurrent-pin runtime stress (bug244 revival)
# ============================================================================
# Multi-threaded harness exercising the GC pin protocol under contention.
# The K proof (ARIA-PIN-PROOFS) covers single-mutator behaviour; this
# slice exercises the implementation with N pin threads + an allocator
# pressure thread. Tuned for ~5s wall-clock. Decision: PIN-DEC-004 part 2.
add_executable(test_pin_concurrent_v0286 runtime/test_pin_concurrent_v0286.cpp)
target_link_libraries(test_pin_concurrent_v0286 PRIVATE aria_runtime)
target_include_directories(test_pin_concurrent_v0286 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
add_test(
    NAME test_pin_concurrent_v0286
    COMMAND test_pin_concurrent_v0286
)
set_tests_properties(test_pin_concurrent_v0286 PROPERTIES
    TIMEOUT 60
    LABELS "memory;gc;pin;runtime;stress;v0.28.6"
)

# ============================================================================
# v0.28.6.1 — Pin stress UNDER REAL GC PRESSURE (deadlock regression guard)
# ============================================================================
# Re-enables the small-nursery + multi-mutator configuration that was
# deferred in v0.28.6 due to a self-deadlock in `GCCoroAllocator::init_gc`
# (lazy-constructed inside `minor_gc()` while gc_mutex was held, then
# re-acquiring it via `npk_gc_init`). Fixed in v0.28.6.1.
# Requires `stats.num_minor_collections > 0` to prove the GC really fired.
add_executable(test_pin_gc_pressure_v02861 runtime/test_pin_gc_pressure_v02861.cpp)
target_link_libraries(test_pin_gc_pressure_v02861 PRIVATE aria_runtime)
target_include_directories(test_pin_gc_pressure_v02861 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
add_test(
    NAME test_pin_gc_pressure_v02861
    COMMAND test_pin_gc_pressure_v02861
)
set_tests_properties(test_pin_gc_pressure_v02861 PROPERTIES
    TIMEOUT 60
    LABELS "memory;gc;pin;runtime;stress;v0.28.6.1;deadlock-regression"
)

# ============================================================================
# v0.31.0.6 — Phase 1 / D-6 + D-11: async runtime stress (suspended frames)
# ============================================================================
# Two runtime-API-level harnesses that drive GCCoroAllocator + the pin
# protocol directly. Required because the user-facing .npk surface cannot
# today reach a *real* suspension point (every async/await resolves
# synchronously on the current executor surface), so the only way to
# exercise the coro-frame mark path under genuine GC pressure is via
# the runtime API.
#
# D-6 harness (test_async_gc_suspended_frames_v03106): N suspended
# frames each hold a single GC root; minor GCs are forced via nursery
# pressure; post-pressure the slot must read back a live object with
# its original magic value. Regression guard for the v0.31.0.6 fix
# that added slot-aware scan_frame_slots() and rewrote *root after
# evacuation (previously the slot was left dangling at the vacated
# nursery payload, which holds the forwarding pointer as its first
# word).
#
# D-11 harness (test_async_pin_across_suspend_v03106): N suspended
# frames each hold a *pinned* GC object; minor GCs are forced; the
# pinned address must be bit-identical and the object bytes must
# round-trip across every collection. Async-coroutine analogue of
# the v0.28.6 / v0.28.6.1 thread-based pin stress.
add_executable(test_async_gc_suspended_frames_v03106
    runtime/test_async_gc_suspended_frames_v03106.cpp)
target_link_libraries(test_async_gc_suspended_frames_v03106 PRIVATE aria_runtime)
target_include_directories(test_async_gc_suspended_frames_v03106 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
add_test(
    NAME test_async_gc_suspended_frames_v03106
    COMMAND test_async_gc_suspended_frames_v03106
)
set_tests_properties(test_async_gc_suspended_frames_v03106 PROPERTIES
    TIMEOUT 60
    LABELS "async;phase1;gc;coro-frame;d-6;v0.31.0.6;regression"
)

add_executable(test_async_pin_across_suspend_v03106
    runtime/test_async_pin_across_suspend_v03106.cpp)
target_link_libraries(test_async_pin_across_suspend_v03106 PRIVATE aria_runtime)
target_include_directories(test_async_pin_across_suspend_v03106 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
add_test(
    NAME test_async_pin_across_suspend_v03106
    COMMAND test_async_pin_across_suspend_v03106
)
set_tests_properties(test_async_pin_across_suspend_v03106 PROPERTIES
    TIMEOUT 60
    LABELS "async;phase1;gc;pin;d-11;v0.31.0.6"
)

# ============================================================================
# v0.31.0.7 — D-9: user-facing async file I/O surface fixtures
# ============================================================================
# bug354 (write success), bug355 (read missing → error), bug356 (roundtrip).
# Each fixture compiles a .npk that calls npk_*_async via extern "libc",
# spin-polls npk_future_is_ready, then inspects npk_future_is_error
# (added this slice) to disambiguate success from failure.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v03107
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_03107.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v03107 PROPERTIES
        TIMEOUT 60
        SKIP_RETURN_CODE 77
        LABELS "async;phase1;io;file;d-9;v0.31.0.7"
    )
endif()

# ============================================================================
# v0.31.1.1 — Phase 2 D-6 correction: `?`-with-Drop interaction verification
# ============================================================================
# Single fixture (bug357) verifying that the `?` (UNWRAP) expression
# operator interacts cleanly with scope-end Drop auto-dispatch. The
# v0.31.1.0 audit listed `?`/`?!` as exit paths needing drop emission;
# this fixture confirms they are merge expressions (not exits) and that
# Drop locals remain registered across them. See
# META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.1.1.md for the full correction.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v03111
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_03111.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v03111 PROPERTIES
        TIMEOUT 30
        SKIP_RETURN_CODE 77
        LABELS "drop;phase2;traits;d-6;v0.31.1.1"
    )
endif()

# v0.31.1.2 (D-7): trait method `$$i`/`$$m` return signatures. Parser
# accepts the qualifier before the return type, AST carries it on
# `TraitMethod`, sema reports ARIA-042 on mismatch with the impl. See
# META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.1.0.md (D-7 entry).
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v03112
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_03112.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v03112 PROPERTIES
        TIMEOUT 30
        SKIP_RETURN_CODE 77
        LABELS "traits;phase2;return-borrow;d-7;v0.31.1.2"
    )
endif()

# v0.31.1.3 (D-9): `dyn Trait` surface verification + local-var
# coercion. Vtable/thunk/fat-pointer codegen already shipped in
# v0.2.36; this slice pins the existing surface and extends the
# coercion to the local-binding form. Mismatches emit ARIA-043. See
# META/NITPICK/ROADMAP/0.31/AUDIT_v0.31.1.3.md.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/npkc)
    add_test(
        NAME bug_tests_v03113
        COMMAND bash ${CMAKE_SOURCE_DIR}/tests/bugs/run_bug_tests_03113.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bug_tests_v03113 PROPERTIES
        TIMEOUT 30
        SKIP_RETURN_CODE 77
        LABELS "traits;phase2;dyn;d-9;v0.31.1.3"
    )
endif()

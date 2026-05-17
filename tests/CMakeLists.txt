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

# ============================================================================
# Aria Compiler - Test Suite
# ============================================================================

# Test runner executable
add_executable(test_runner
    test_runner.cpp
    unit/test_sample.cpp
    unit/test_token.cpp
    unit/test_lexer.cpp
    unit/test_ast.cpp
    unit/test_parser.cpp
    unit/test_symbol_table.cpp
    unit/test_module_table.cpp
    unit/test_type_checker.cpp
    unit/test_borrow_checker.cpp
    unit/test_generic_resolver.cpp
    unit/test_generic_parser.cpp
    unit/test_module_resolver.cpp
    unit/test_visibility_checker.cpp
    unit/test_async_await_parser.cpp
    unit/test_await_parser.cpp
    unit/test_async_semantic.cpp
    frontend/test_const_evaluator.cpp
    backend/test_ir_generator.cpp
    backend/test_tbb_codegen.cpp
    backend/test_codegen_expr.cpp
    backend/test_codegen_stmt.cpp
    integration/test_generics_integration.cpp
    integration/test_sema_integration.cpp
    integration/test_ir_compilation.cpp
    integration/test_ir_optimization.cpp
    runtime/test_gc.cpp
    runtime/test_allocators.cpp
    runtime/test_assembler.cpp
    runtime/test_llvm_jit.cpp
    runtime/test_code_cache.cpp
    runtime/test_io.cpp
    runtime/test_streams.cpp
    runtime/test_process.cpp
    runtime/test_thread.cpp
    runtime/test_atomic.cpp
    runtime/test_timer.cpp
    integration/test_runtime_stress.cpp
    # Add more test files here as they are created
)

# Link with lexer, AST, parser, sema, backend, and runtime sources
target_sources(test_runner PRIVATE
    ${CMAKE_SOURCE_DIR}/src/frontend/lexer/token.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/lexer/lexer.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/ast/ast_node.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/ast/expr.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/ast/stmt.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/ast/type.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/parser/parser.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/sema/symbol_table.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/sema/type.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/sema/module_table.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/sema/type_checker.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/sema/borrow_checker.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/sema/generic_resolver.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/sema/module_resolver.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/sema/visibility_checker.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/sema/async_analyzer.cpp
    ${CMAKE_SOURCE_DIR}/src/frontend/sema/const_evaluator.cpp
    ${CMAKE_SOURCE_DIR}/src/backend/ir/ir_generator.cpp
    ${CMAKE_SOURCE_DIR}/src/backend/ir/tbb_codegen.cpp
    ${CMAKE_SOURCE_DIR}/src/backend/ir/codegen_expr.cpp
    ${CMAKE_SOURCE_DIR}/src/backend/ir/codegen_stmt.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime/gc/allocator.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime/gc/gc.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime/allocators/wild_alloc.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime/allocators/wildx_alloc.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime/assembler/assembler.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime/assembler/llvm_jit.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime/assembler/code_cache.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime/io/io.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime/streams/streams.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime/process/process.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime/thread/thread.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime/atomic/atomic.cpp
    ${CMAKE_SOURCE_DIR}/src/runtime/timer/timer.cpp
)

# Include test helpers header
target_include_directories(test_runner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link with LLVM libraries (needed for IR generation)
target_link_libraries(test_runner ${llvm_libs})

# Register with CTest
add_test(NAME unit_tests COMMAND test_runner)

# Set test properties
set_tests_properties(unit_tests PROPERTIES
    TIMEOUT 300  # 5 minutes
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

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
        DEPENDS unit_tests  # Run after unit tests pass
    )
endif()

# ============================================================================
# Code Coverage Target
# ============================================================================

if(USE_COVERAGE)
    # Add coverage target
    add_custom_target(coverage
        COMMAND lcov --capture --directory ${CMAKE_BINARY_DIR} --output-file coverage.info
        COMMAND lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
        COMMAND lcov --list coverage.info
        COMMAND genhtml coverage.info --output-directory coverage_html
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating code coverage report"
    )
    
    # Make coverage depend on running tests
    add_dependencies(coverage test_runner)
    
    message(STATUS "Code coverage enabled. Run 'make coverage' after 'make test' to generate report.")
endif()

# ============================================================================
# Memory Leak Detection
# ============================================================================

if(USE_ASAN)
    # AddressSanitizer is enabled via compiler flags in main CMakeLists.txt
    # Tests will automatically use ASAN when built with -DUSE_ASAN=ON
    message(STATUS "AddressSanitizer enabled for tests. Memory leaks and errors will be reported.")
    
    # Set ASAN options for tests
    set_tests_properties(unit_tests PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=1:halt_on_error=0"
    )
endif()

# ============================================================================
# Benchmark Tests (Phase 0.2 - optional)
# ============================================================================

# Benchmarks will be added later when we have compiler components to benchmark
# add_subdirectory(benchmarks)

# ============================================================================
# Fuzz Tests (Phase 0.2 - optional)
# ============================================================================

# Fuzz tests will be added later when we have parser to fuzz
# add_subdirectory(fuzz)

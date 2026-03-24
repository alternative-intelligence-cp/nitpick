cmake_minimum_required(VERSION 3.20)
project(aria VERSION 0.2.7 LANGUAGES C CXX)

# C++17 standard required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# liburing configuration
set(LIBURING_ROOT ${CMAKE_SOURCE_DIR}/third_party/liburing-master/install)
include_directories(${LIBURING_ROOT}/include)
link_directories(${LIBURING_ROOT}/lib)

# Compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

# Debug build type flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

# Release build type flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Memory sanitizer for debug builds (optional, can enable with -DUSE_ASAN=ON)
option(USE_ASAN "Enable AddressSanitizer" OFF)
if(USE_ASAN)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")
endif()

# Code coverage (optional, can enable with -DUSE_COVERAGE=ON)
option(USE_COVERAGE "Enable code coverage" OFF)
if(USE_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

# ============================================================================
# LLVM Configuration (LLVM 20.1.2)
# ============================================================================

find_package(LLVM 20.1 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Find the LLVM libraries we need
llvm_map_components_to_libnames(llvm_libs
    core
    support
    irreader
    bitwriter
    transformutils
    analysis
    target
    mcjit
    native
    orcjit
    passes
    nvptxcodegen   # NVPTX code generator for GPU/CUDA
    nvptxdesc      # NVPTX target description
    nvptxinfo      # NVPTX target info
)

# ============================================================================
# LLDB Configuration (for debugger formatters)
# ============================================================================

# Find LLDB library
find_library(LLDB_LIBRARY 
    NAMES lldb LLDB lldb-20
    HINTS ${LLVM_LIBRARY_DIRS} /usr/lib/llvm-20/lib
    PATHS /usr/lib/x86_64-linux-gnu
)
if(LLDB_LIBRARY)
    message(STATUS "Found LLDB library: ${LLDB_LIBRARY}")
    # LLDB headers are always in /usr/lib/llvm-*/include regardless of library location
    # Try to find the version-specific directory
    if(EXISTS "/usr/lib/llvm-20/include/lldb")
        set(LLDB_INCLUDE_DIRS "/usr/lib/llvm-20/include")
    elseif(EXISTS "/usr/lib/llvm-19/include/lldb")
        set(LLDB_INCLUDE_DIRS "/usr/lib/llvm-19/include")
    elseif(EXISTS "/usr/lib/llvm-18/include/lldb")
        set(LLDB_INCLUDE_DIRS "/usr/lib/llvm-18/include")
    else()
        # Fallback: try to derive from LLVM_INCLUDE_DIRS
        set(LLDB_INCLUDE_DIRS "${LLVM_INCLUDE_DIRS}")
    endif()
    message(STATUS "LLDB include directory: ${LLDB_INCLUDE_DIRS}")
    set(LLDB_FOUND TRUE)
else()
    message(STATUS "LLDB library not found - debugger formatters will not be built")
    set(LLDB_FOUND FALSE)
endif()

# ============================================================================
# Third-party libraries
# ============================================================================

# toml11 for aria.toml parsing (header-only library)
add_subdirectory(vendor/toml11)

# nlohmann/json for LSP JSON-RPC (header-only library)
# No add_subdirectory needed - just include headers

# ============================================================================
# aglob - GlobEngine for file pattern matching (ARIA-010)
# ============================================================================
# Located in aria_utils repository, provides FFI-safe glob pattern matching
# for the std.fs.glob module and aria_make build system.

set(AGLOB_ROOT ${CMAKE_SOURCE_DIR}/../aria_utils/src/aglob)
if(EXISTS ${AGLOB_ROOT}/include)
    message(STATUS "Found aglob at: ${AGLOB_ROOT}")
    include_directories(${AGLOB_ROOT}/include)

    # Add aglob as external project (build if not already built)
    if(EXISTS ${AGLOB_ROOT}/build/libaglob.a)
        # Use pre-built library
        add_library(aglob STATIC IMPORTED)
        set_target_properties(aglob PROPERTIES
            IMPORTED_LOCATION ${AGLOB_ROOT}/build/libaglob.a
        )
        message(STATUS "Using pre-built aglob library")
    else()
        # Build aglob as part of aria build
        add_subdirectory(${AGLOB_ROOT} ${CMAKE_BINARY_DIR}/aglob)
        message(STATUS "Building aglob from source")
    endif()

    set(AGLOB_FOUND TRUE)
else()
    message(WARNING "aglob not found at ${AGLOB_ROOT} - std.fs.glob will not be available")
    set(AGLOB_FOUND FALSE)
endif()

# ============================================================================
# Include directories
# ============================================================================

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/vendor/toml11/include)
include_directories(${CMAKE_SOURCE_DIR}/vendor/json/include)

# ============================================================================
# Source files (will be populated as we implement each phase)
# ============================================================================

# Phase 0: Preprocessor (NASM-style macro system)
set(PREPROCESSOR_SOURCES
    src/frontend/preprocessor/preprocessor.cpp
)

# Phase 1: Lexer
set(LEXER_SOURCES
    src/frontend/lexer/token.cpp
    src/frontend/lexer/lexer.cpp
)

# Phase 2: AST & Parser
set(AST_SOURCES
    src/frontend/ast/ast_node.cpp
    src/frontend/ast/expr.cpp
    src/frontend/ast/stmt.cpp
    src/frontend/ast/type.cpp
)

set(PARSER_SOURCES
    src/frontend/parser/parser.cpp
    # src/frontend/parser/parse_expr.cpp
    # src/frontend/parser/parse_stmt.cpp
    # src/frontend/parser/parse_type.cpp
    # src/frontend/parser/parse_module.cpp
)

# Diagnostics (Phase 7.2)
set(DIAGNOSTICS_SOURCES
    src/frontend/diagnostics.cpp
    src/frontend/warnings.cpp
)

# Phase 3: Semantic Analysis
set(SEMA_SOURCES
    src/frontend/sema/symbol_table.cpp
    src/frontend/sema/type.cpp
    src/frontend/sema/module_table.cpp
    src/frontend/sema/module_loader.cpp
    src/frontend/sema/type_checker.cpp
    src/frontend/sema/exhaustiveness.cpp
    src/frontend/sema/definite_assignment.cpp
    src/frontend/sema/borrow_checker.cpp
    src/frontend/sema/generic_resolver.cpp
    src/frontend/sema/module_resolver.cpp
    src/frontend/sema/closure_analyzer.cpp
    src/frontend/sema/visibility_checker.cpp
    src/frontend/sema/async_analyzer.cpp
    src/frontend/sema/const_evaluator.cpp
    src/frontend/sema/safety_checker.cpp
    src/semantic/literal_converter.cpp
)

# Phase 4: IR Generation
set(IR_SOURCES
    src/backend/ir/ir_generator.cpp
    src/backend/ir/tbb_codegen.cpp
    src/backend/ir/ternary_codegen.cpp
    src/backend/ir/codegen_expr.cpp
    src/backend/ir/codegen_stmt.cpp
    # src/backend/ir/type_mapper.cpp
    # src/backend/ir/memory_codegen.cpp
)

# Phase 5: Runtime
set(RUNTIME_SOURCES
    src/runtime/gc/allocator.cpp
    src/runtime/gc/gc.cpp
    src/runtime/allocators/wild_alloc.cpp
    src/runtime/allocators/wildx_alloc.cpp
    src/runtime/allocators/arena_alloc.cpp
    src/runtime/allocators/pool_alloc.cpp
    src/runtime/allocators/slab_alloc.cpp
    src/runtime/allocators/arena_builtins.cpp
    src/runtime/allocators/pool_builtins.cpp
    src/runtime/allocators/slab_builtins.cpp
    src/runtime/panic/panic_handler.cpp
    src/runtime/format/locale_safe_format.cpp
    src/runtime/assembler/assembler.cpp
    src/runtime/assembler/llvm_jit.cpp
    src/runtime/assembler/code_cache.cpp
    src/runtime/io/io.cpp
    src/runtime/io/zero_copy_bridge.cpp
    src/runtime/streams/streams.cpp
    src/runtime/process/process.cpp
    src/runtime/thread/thread.cpp
    src/backend/runtime/ternary_ops.cpp
    src/backend/runtime/frac_ops.cpp
    src/backend/runtime/tfp_ops.cpp
    src/backend/runtime/vec9_ops.cpp
    src/backend/runtime/tmatrix_ops.cpp
    src/backend/runtime/ttensor_ops.cpp
    src/runtime/atomic/atomic.cpp
    src/runtime/int_array.cpp
    src/runtime/timer/timer.cpp
    src/runtime/async/executor.cpp
    src/runtime/async/coroutine.cpp
    src/runtime/async/runtime_api.cpp
    src/runtime/async/async_io.cpp
    src/runtime/async/async_io_api.cpp
    src/runtime/async/event_loop.cpp
    src/runtime/async/async_net.cpp
    src/runtime/async/io_uring.cpp
    src/runtime/async/file_stream.cpp
    src/runtime/async/work_stealing.cpp
    src/runtime/async/gc_integration.cpp
    src/runtime/telemetry/telemetry.cpp
    src/runtime/args/args.c
    src/runtime/exotic_types.c
)

# Phase 6: Standard Library Runtime Support
set(STDLIB_SOURCES
    src/runtime/stdlib/stdlib.cpp
    src/runtime/result/result.cpp
    src/runtime/collections/collections.cpp
    src/runtime/collections/map.cpp
    src/runtime/collections/collection_helpers.cpp
    src/runtime/strings/strings.cpp
    src/runtime/strings/string_helpers.cpp
    src/runtime/json/json_helpers.cpp
    src/runtime/toml/toml_helpers.cpp
    src/runtime/binary/binary_helpers.cpp
    src/runtime/lex/lex_helpers.cpp
    src/runtime/ast/ast_helpers.cpp
    src/runtime/sema/sema_helpers.cpp
    src/runtime/sema/pass_helpers.cpp
    src/runtime/net/net.cpp
    src/runtime/math/math.cpp
    src/runtime/math/lbim.cpp
    src/runtime/math/lbim_extended.cpp
    src/runtime/math/fix256.cpp
    src/runtime/tbb/tbb.cpp
    src/runtime/fmt/formatters.cpp
)

# ARIA-017: High-Precision Floating-Point (flt256, flt512)
set(HIGHPREC_SOURCES
    src/runtime/highprec_float.cpp
    src/runtime/highprec_simd.cpp
)

# Phase 5.4: Integration Pipeline — AST converter + validation
# These frontend sources are LLVM-free and needed by pipeline_helpers.cpp
# to convert self-hosted ASTNodeCompat trees to native C++ AST nodes.
# Added ONLY to aria_runtime (ariac already has them via ARIA_SOURCES).
set(PIPELINE_SOURCES
    src/frontend/ast/ast_node.cpp
    src/frontend/ast/expr.cpp
    src/frontend/ast/stmt.cpp
    src/frontend/ast/type.cpp
    src/frontend/lexer/token.cpp
    src/frontend/lexer/lexer.cpp
    src/frontend/parser/parser.cpp
    src/runtime/pipeline/pipeline_helpers.cpp
)

# Phase 7: Tooling
set(TOOLS_SOURCES
    src/tools/project_config.cpp
    src/tools/lsp/transport.cpp
    src/tools/lsp/server.cpp
    src/tools/lsp/vfs.cpp
    src/tools/lsp/work_queue.cpp
    src/tools/lsp/thread_pool.cpp
    src/tools/debugger/web_server.cpp
)

# Phase 7.4: Debugger (optional, requires LLDB)
if(LLDB_FOUND)
    set(DEBUGGER_SOURCES
        src/tools/debugger/aria_formatters.cpp
        src/tools/debugger/dap_server.cpp
        src/tools/debugger/async_debugger.cpp
        src/tools/debugger/memory_visualizer.cpp
    )
endif()

# Phase 7.5: Package Manager
set(PKG_SOURCES
    src/tools/pkg/installer.cpp
)

# Phase 7.6: Documentation Generator
set(DOC_SOURCES
    src/tools/doc/parser.cpp
    src/tools/doc/generator.cpp
)

# Combine all sources
set(ARIA_SOURCES
    ${PREPROCESSOR_SOURCES}
    ${LEXER_SOURCES}
    ${AST_SOURCES}
    ${PARSER_SOURCES}
    ${DIAGNOSTICS_SOURCES}
    ${SEMA_SOURCES}
    ${IR_SOURCES}
    ${RUNTIME_SOURCES}
    ${STDLIB_SOURCES}
    ${TOOLS_SOURCES}
)

# ============================================================================
# Runtime library (for compiled Aria programs)
# ============================================================================

# Static runtime library that gets linked into all Aria executables
add_library(aria_runtime STATIC
    ${RUNTIME_SOURCES}
    ${STDLIB_SOURCES}
    ${HIGHPREC_SOURCES}
    ${PIPELINE_SOURCES}
)

# ARIA-017: Enable AVX-512 and ADX intrinsics for high-precision floating-point
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-mavx512f" COMPILER_SUPPORTS_AVX512F)
check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
check_cxx_compiler_flag("-madx" COMPILER_SUPPORTS_ADX)
check_cxx_compiler_flag("-mbmi2" COMPILER_SUPPORTS_BMI2)

if(COMPILER_SUPPORTS_AVX512F)
    set_source_files_properties(src/runtime/highprec_simd.cpp PROPERTIES
        COMPILE_FLAGS "-mavx512f -mavx512vl -mavx512dq -mavx2 -madx -mbmi2"
    )
    message(STATUS "ARIA-017: AVX-512 SIMD enabled for high-precision arithmetic")
elseif(COMPILER_SUPPORTS_AVX2)
    set_source_files_properties(src/runtime/highprec_simd.cpp PROPERTIES
        COMPILE_FLAGS "-mavx2 -madx -mbmi2"
    )
    message(STATUS "ARIA-017: AVX2 enabled for high-precision arithmetic (AVX-512 not available)")
else()
    message(STATUS "ARIA-017: Using portable implementation for high-precision arithmetic")
endif()

# Link aglob for std.fs.glob support (ARIA-010)
if(AGLOB_FOUND)
    target_link_libraries(aria_runtime aglob atomic)
    message(STATUS "aria_runtime linked with aglob and atomic for glob and atomics support")
endif()

# Install runtime library so the compiler can find it
install(TARGETS aria_runtime DESTINATION lib)

# ============================================================================
# Compiler executable
# ============================================================================

# Main compiler executable
add_executable(ariac src/main.cpp ${ARIA_SOURCES})
target_link_libraries(ariac ${llvm_libs} uring atomic)

# Testing compiler executable (separate binary for testing without disrupting fuzzer)
# Build with: cmake --build build --target ariac_testing
# Use with: ./build/ariac_testing your_test.aria
add_executable(ariac_testing src/main.cpp ${ARIA_SOURCES})
target_link_libraries(ariac_testing ${llvm_libs} uring atomic)

# LSP server executable (Phase 7.3)
add_executable(aria-ls src/tools/lsp/aria-ls.cpp 
    ${TOOLS_SOURCES} 
    ${DIAGNOSTICS_SOURCES}
    ${LEXER_SOURCES}
    ${PARSER_SOURCES}
    ${AST_SOURCES}
)
# LSP server needs lexer, parser, diagnostics for real-time error checking

# DAP server executable (Phase 7.4.3)
if(LLDB_FOUND)
    add_executable(aria-dap 
        src/tools/debugger/aria_dap.cpp
        ${DEBUGGER_SOURCES}
    )
    target_include_directories(aria-dap PRIVATE ${LLDB_INCLUDE_DIRS})
    target_link_libraries(aria-dap ${LLDB_LIBRARY})
    
    message(STATUS "Building DAP server: aria-dap")
    message(STATUS "LLDB include dirs for aria-dap: ${LLDB_INCLUDE_DIRS}")
else()
    message(STATUS "LLDB not found - DAP server will not be built")
endif()

# Package manager executable (Phase 7.5)
add_executable(aria-pkg
    src/tools/pkg/main.cpp
    ${PKG_SOURCES}
)
target_link_libraries(aria-pkg)

# Documentation generator executable (Phase 7.6)
add_executable(aria-doc
    src/tools/doc/main.cpp
    ${DOC_SOURCES}
)
target_link_libraries(aria-doc)

# ============================================================================
# Testing
# ============================================================================

enable_testing()

# Test runner (Phase 0.2) — only if test_runner.cpp exists (gitignored in dev)
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_runner.cpp")
    add_subdirectory(tests)
endif()

# ============================================================================
# Installation
# ============================================================================

# install(TARGETS ariac DESTINATION bin)
# install(DIRECTORY lib/std DESTINATION lib/aria)

# ============================================================================
# Print configuration summary
# ============================================================================

message(STATUS "")
message(STATUS "============================================")
message(STATUS "Aria Compiler Configuration")
message(STATUS "============================================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "LLVM Version: ${LLVM_PACKAGE_VERSION}")
message(STATUS "AddressSanitizer: ${USE_ASAN}")
message(STATUS "Code Coverage: ${USE_COVERAGE}")
message(STATUS "============================================")
message(STATUS "")

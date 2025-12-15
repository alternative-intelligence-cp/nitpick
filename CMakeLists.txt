cmake_minimum_required(VERSION 3.20)
project(aria VERSION 0.1.0 LANGUAGES CXX)

# C++17 standard required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
)

# ============================================================================
# Include directories
# ============================================================================

include_directories(${CMAKE_SOURCE_DIR}/include)

# ============================================================================
# Source files (will be populated as we implement each phase)
# ============================================================================

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
)

set(PARSER_SOURCES
    src/frontend/parser/parser.cpp
    # src/frontend/parser/parse_expr.cpp
    # src/frontend/parser/parse_stmt.cpp
    # src/frontend/parser/parse_type.cpp
    # src/frontend/parser/parse_module.cpp
)

# Phase 3: Semantic Analysis
set(SEMA_SOURCES
    # src/frontend/sema/symbol_table.cpp
    # src/frontend/sema/type_checker.cpp
    # src/frontend/sema/borrow_checker.cpp
    # src/frontend/sema/generic_resolver.cpp
    # src/frontend/sema/module_resolver.cpp
)

# Phase 4: IR Generation
set(IR_SOURCES
    # src/backend/ir/ir_generator.cpp
    # src/backend/ir/type_mapper.cpp
    # src/backend/ir/codegen_expr.cpp
    # src/backend/ir/codegen_stmt.cpp
    # src/backend/ir/memory_codegen.cpp
)

# Phase 5: Runtime
set(RUNTIME_SOURCES
    # src/runtime/gc/allocator.cpp
    # src/runtime/gc/mark.cpp
    # src/runtime/gc/sweep.cpp
    # src/runtime/allocators/wild_alloc.cpp
    # src/runtime/io/file.cpp
    # src/runtime/process/process.cpp
)

# Combine all sources
set(ARIA_SOURCES
    ${LEXER_SOURCES}
    ${AST_SOURCES}
    ${PARSER_SOURCES}
    ${SEMA_SOURCES}
    ${IR_SOURCES}
    ${RUNTIME_SOURCES}
)

# ============================================================================
# Compiler executable (will be enabled when we have sources)
# ============================================================================

# Main compiler executable
# Uncomment when we have main.cpp
# add_executable(ariac src/main.cpp ${ARIA_SOURCES})
# target_link_libraries(ariac ${llvm_libs})

# ============================================================================
# Testing
# ============================================================================

enable_testing()

# Test runner (Phase 0.2)
add_subdirectory(tests)

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

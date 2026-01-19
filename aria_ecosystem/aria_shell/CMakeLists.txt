# aria_shell - Aria Shell (AriaSH)
# Interactive REPL and scripting shell for Aria language
#
# ARIA-021: Shell Job Control State Machine
#
# Build:
#   mkdir build && cd build
#   cmake ..
#   make

cmake_minimum_required(VERSION 3.16)
project(aria_shell
    VERSION 0.1.0
    DESCRIPTION "Interactive Shell for Aria Language"
    LANGUAGES CXX
)

# Require C++20 for std::jthread, std::stop_token, and cooperative cancellation
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror=return-type)
elseif(MSVC)
    add_compile_options(/W4)
endif()

# Build type defaults
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Threading support
find_package(Threads REQUIRED)

# -----------------------------------------------------------------------------
# Library: aria_shell_job (Job Control - ARIA-021)
# -----------------------------------------------------------------------------
set(ARIA_SHELL_JOB_SOURCES
    src/job/job_state.cpp
    src/job/job_control.cpp
    src/job/stream_controller.cpp
    src/hexstream/process.cpp
    src/repl/terminal.cpp
    src/repl/input_engine.cpp
    src/parser/lexer.cpp
    src/parser/ast.cpp
    src/parser/parser.cpp
    src/executor/executor.cpp
)

# Add Windows-specific sources
if(WIN32)
    list(APPEND ARIA_SHELL_JOB_SOURCES
        src/platform/windows_bootstrap.cpp
    )
endif()

add_library(aria_shell_job STATIC ${ARIA_SHELL_JOB_SOURCES})

target_include_directories(aria_shell_job
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(aria_shell_job PUBLIC Threads::Threads)

set_target_properties(aria_shell_job PROPERTIES
    VERSION ${PROJECT_VERSION}
    POSITION_INDEPENDENT_CODE ON
)

# -----------------------------------------------------------------------------
# Main executable: ariash
# -----------------------------------------------------------------------------
add_executable(ariash
    src/repl/ariash_main.cpp
)

target_link_libraries(ariash PRIVATE aria_shell_job)

set_target_properties(ariash PROPERTIES
    OUTPUT_NAME "ariash"
)

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
option(BUILD_TESTS "Build test executable" ON)

if(BUILD_TESTS)
    enable_testing()

    # Stream draining test (C++20 jthread)
    add_executable(test_stream_draining tests/test_stream_draining.cpp)
    target_link_libraries(test_stream_draining PRIVATE aria_shell_job Threads::Threads)
    add_test(NAME stream_draining_tests COMMAND test_stream_draining)
    
    # Hex-stream process test (six-stream I/O)
    add_executable(test_hexstream tests/test_hexstream.cpp)
    target_link_libraries(test_hexstream PRIVATE aria_shell_job Threads::Threads)
    add_test(NAME hexstream_tests COMMAND test_hexstream)
    
    # Multi-line input demo (modal FSM + raw terminal)
    add_executable(test_multiline tests/test_multiline.cpp)
    target_link_libraries(test_multiline PRIVATE aria_shell_job Threads::Threads)
    
    # Lexer test (whitespace-insensitive tokenization)
    add_executable(test_lexer tests/test_lexer.cpp)
    target_link_libraries(test_lexer PRIVATE aria_shell_job Threads::Threads)
    add_test(NAME lexer_tests COMMAND test_lexer)
    
    # Parser test (AST construction and whitespace insensitivity)
    add_executable(test_parser tests/test_parser.cpp)
    target_link_libraries(test_parser PRIVATE aria_shell_job Threads::Threads)
    add_test(NAME parser_tests COMMAND test_parser)
    
    # Executor test (AST interpretation and execution)
    add_executable(test_executor tests/test_executor.cpp)
    target_link_libraries(test_executor PRIVATE aria_shell_job Threads::Threads)
    add_test(NAME executor_tests COMMAND test_executor)
endif()

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS aria_shell_job
    EXPORT aria_shellTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY inc/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "aria_shell ${PROJECT_VERSION} - Aria Interactive Shell")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "")

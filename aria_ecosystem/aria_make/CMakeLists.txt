# aria_make - Aria Build System
# Build system and package manager for Aria projects
#
# Build:
#   mkdir build && cd build
#   cmake ..
#   make
#
# Test:
#   make test
#   ./test_state_manager

cmake_minimum_required(VERSION 3.16)
project(aria_make
    VERSION 0.1.0
    DESCRIPTION "Build System for Aria Language"
    LANGUAGES CXX
)

# Require C++17 for std::filesystem, std::shared_mutex, std::optional
set(CMAKE_CXX_STANDARD 17)
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
# Library: aria_make_state (StateManager component)
# -----------------------------------------------------------------------------
add_library(aria_make_state STATIC
    src/state/state_manager.cpp
)

target_include_directories(aria_make_state
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(aria_make_state PUBLIC Threads::Threads)

# Link filesystem library if needed (older GCC/Clang)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(aria_make_state PUBLIC stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(aria_make_state PUBLIC c++fs)
endif()

set_target_properties(aria_make_state PROPERTIES
    VERSION ${PROJECT_VERSION}
    POSITION_INDEPENDENT_CODE ON
)

# -----------------------------------------------------------------------------
# Library: aria_make_abc (ABC Config Parser - ARIA-019)
# -----------------------------------------------------------------------------
add_library(aria_make_abc STATIC
    src/abc/abc_lexer.cpp
    src/abc/abc_parser.cpp
    src/abc/abc_interpolate.cpp
)

target_include_directories(aria_make_abc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(aria_make_abc PROPERTIES
    VERSION ${PROJECT_VERSION}
    POSITION_INDEPENDENT_CODE ON
)

# -----------------------------------------------------------------------------
# aglob library from aria_utils
# -----------------------------------------------------------------------------
set(AGLOB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../aria_utils/src/aglob"
    CACHE PATH "Path to aglob library")

if(EXISTS "${AGLOB_ROOT}/include/aglob/glob_engine.hpp")
    message(STATUS "Found aglob at: ${AGLOB_ROOT}")
    set(AGLOB_FOUND TRUE)
else()
    message(WARNING "aglob not found at ${AGLOB_ROOT} - glob expansion limited")
    set(AGLOB_FOUND FALSE)
endif()

# -----------------------------------------------------------------------------
# Library: aria_make_glob (Glob bridge component)
# -----------------------------------------------------------------------------
add_library(aria_make_glob STATIC
    src/glob/glob_bridge.cpp
)

target_include_directories(aria_make_glob
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(AGLOB_FOUND)
    # Add aglob subdirectory if not already built
    if(NOT TARGET aglob)
        add_subdirectory(${AGLOB_ROOT} ${CMAKE_BINARY_DIR}/aglob EXCLUDE_FROM_ALL)
    endif()
    target_link_libraries(aria_make_glob PUBLIC aglob)
    target_compile_definitions(aria_make_glob PRIVATE ARIA_MAKE_HAS_AGLOB=1)
endif()

set_target_properties(aria_make_glob PROPERTIES
    VERSION ${PROJECT_VERSION}
    POSITION_INDEPENDENT_CODE ON
)

# -----------------------------------------------------------------------------
# Library: aria_make_compiler (Compiler Interface component)
# -----------------------------------------------------------------------------
add_library(aria_make_compiler STATIC
    src/core/compiler_interface.cpp
)

target_include_directories(aria_make_compiler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(aria_make_compiler PROPERTIES
    VERSION ${PROJECT_VERSION}
    POSITION_INDEPENDENT_CODE ON
)

# -----------------------------------------------------------------------------
# Library: aria_make_c_compiler (C/C++ Compiler Interface component)
# -----------------------------------------------------------------------------
add_library(aria_make_c_compiler STATIC
    src/core/c_compiler_interface.cpp
)

target_include_directories(aria_make_c_compiler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(aria_make_c_compiler PROPERTIES
    VERSION ${PROJECT_VERSION}
    POSITION_INDEPENDENT_CODE ON
)

# -----------------------------------------------------------------------------
# Library: aria_make_core (Build Orchestrator component)
# -----------------------------------------------------------------------------
add_library(aria_make_core STATIC
    src/core/build_orchestrator.cpp
)

target_include_directories(aria_make_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(aria_make_core PUBLIC 
    aria_make_state 
    aria_make_abc 
    aria_make_glob 
    aria_make_compiler
    aria_make_c_compiler
    Threads::Threads
)

set_target_properties(aria_make_core PROPERTIES
    VERSION ${PROJECT_VERSION}
    POSITION_INDEPENDENT_CODE ON
)

# -----------------------------------------------------------------------------
# Main executable: aria_make
# -----------------------------------------------------------------------------
add_executable(aria_make
    src/main.cpp
)

target_link_libraries(aria_make PRIVATE aria_make_core)

set_target_properties(aria_make PROPERTIES
    OUTPUT_NAME "aria_make"
)

# -----------------------------------------------------------------------------
# Test executable
# -----------------------------------------------------------------------------
option(BUILD_TESTS "Build test executable" ON)

if(BUILD_TESTS)
    enable_testing()

    add_executable(test_state_manager
        tests/test_state_manager.cpp
    )

    target_link_libraries(test_state_manager PRIVATE aria_make_state)

    add_test(NAME state_manager_tests COMMAND test_state_manager)
endif()

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS aria_make_state
    EXPORT aria_makeTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT aria_makeTargets
    FILE aria_makeTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aria_make
)

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "aria_make ${PROJECT_VERSION} - Aria Build System")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "")

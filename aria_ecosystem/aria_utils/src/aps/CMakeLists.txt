cmake_minimum_required(VERSION 3.16)
project(aps
    VERSION 0.1.0
    DESCRIPTION "Cross-platform process listing library for Aria"
    LANGUAGES CXX
)

# C++17 required for std::optional, std::filesystem, structured bindings
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

# =============================================================================
# Static Library
# =============================================================================

add_library(aps STATIC
    src/process_list.cpp
    src/ffi.cpp
)

target_include_directories(aps
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(aps PROPERTIES
    VERSION ${PROJECT_VERSION}
    POSITION_INDEPENDENT_CODE ON
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(aps PUBLIC psapi)
elseif(APPLE)
    # libproc is part of system on macOS
endif()

# =============================================================================
# Shared Library (for FFI)
# =============================================================================

add_library(aps_shared SHARED
    src/process_list.cpp
    src/ffi.cpp
)

target_include_directories(aps_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(aps_shared PROPERTIES
    OUTPUT_NAME aps
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    POSITION_INDEPENDENT_CODE ON
)

if(WIN32)
    target_link_libraries(aps_shared PUBLIC psapi)
endif()

# =============================================================================# CLI Utility
# ==================================================================================

option(BUILD_CLI "Build aps command-line utility" ON)

if(BUILD_CLI)
    add_executable(aps_bin aps_cli.cpp)
    target_link_libraries(aps_bin PRIVATE aps)
    set_target_properties(aps_bin PROPERTIES OUTPUT_NAME aps_util)
    install(TARGETS aps_bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# ==================================================================================# Tests
# =============================================================================

option(BUILD_TESTS "Build test executable" ON)

if(BUILD_TESTS)
    enable_testing()

    add_executable(test_aps tests/test_aps.cpp)
    target_link_libraries(test_aps PRIVATE aps)

    add_test(NAME aps_tests COMMAND test_aps)
endif()

# =============================================================================
# Installation
# =============================================================================

include(GNUInstallDirs)

install(TARGETS aps aps_shared
    EXPORT apsTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# CMake package config
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/apsConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/apsConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aps
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/apsConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/apsConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/apsConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aps
)

install(EXPORT apsTargets
    FILE apsTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aps
)

# =============================================================================
# Summary
# =============================================================================

message(STATUS "")
message(STATUS "aps ${PROJECT_VERSION} - Cross-platform process listing")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "")

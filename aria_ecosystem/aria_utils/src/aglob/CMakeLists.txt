# aglob - Aria Globbing Engine
# Part of the Aria Build System (aria_make) and Compiler Toolchain
#
# Build:
#   mkdir build && cd build
#   cmake ..
#   make
#
# Test:
#   make test
#   ./test_aglob

cmake_minimum_required(VERSION 3.16)
project(aglob
    VERSION 0.1.0
    DESCRIPTION "High-performance glob engine for Aria ecosystem"
    LANGUAGES CXX
)

# Require C++17 for std::filesystem
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

# -----------------------------------------------------------------------------
# Library: aglob (static)
# -----------------------------------------------------------------------------
add_library(aglob STATIC
    src/glob_engine.cpp
    src/glob_ffi.cpp
)

target_include_directories(aglob
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link filesystem library if needed (older GCC/Clang)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(aglob PUBLIC stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(aglob PUBLIC c++fs)
endif()

# Set library properties
set_target_properties(aglob PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    POSITION_INDEPENDENT_CODE ON
)

# -----------------------------------------------------------------------------
# Library: aglob_shared (shared library for FFI)
# -----------------------------------------------------------------------------
add_library(aglob_shared SHARED
    src/glob_engine.cpp
    src/glob_ffi.cpp
)

target_include_directories(aglob_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(aglob_shared PUBLIC stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(aglob_shared PUBLIC c++fs)
endif()

set_target_properties(aglob_shared PROPERTIES
    OUTPUT_NAME aglob
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

# -----------------------------------------------------------------------------
# CLI Utility
# -----------------------------------------------------------------------------
option(BUILD_CLI "Build aglob command-line utility" ON)

if(BUILD_CLI)
    add_executable(aglob_bin aglob_cli.cpp)
    target_link_libraries(aglob_bin PRIVATE aglob)
    set_target_properties(aglob_bin PROPERTIES OUTPUT_NAME aglob_util)
    install(TARGETS aglob_bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# -----------------------------------------------------------------------------
# Test executable
# -----------------------------------------------------------------------------
option(BUILD_TESTS "Build test executable" ON)

if(BUILD_TESTS)
    enable_testing()

    add_executable(test_aglob
        tests/test_glob.cpp
    )

    target_link_libraries(test_aglob PRIVATE aglob)

    add_test(NAME aglob_tests COMMAND test_aglob)
endif()

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS aglob aglob_shared
    EXPORT aglobTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT aglobTargets
    FILE aglobTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aglob
)

# Generate config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/aglobConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/aglobConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/aglobConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aglob
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/aglobConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/aglobConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aglob
)

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "aglob ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "")

cmake_minimum_required(VERSION 3.16)
project(acurl
    VERSION 0.1.0
    DESCRIPTION "Hex-Stream HTTP client library for Aria"
    LANGUAGES CXX
)

# C++17 required
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

# Find libcurl
find_package(CURL REQUIRED)

# =============================================================================
# Static Library
# =============================================================================

add_library(acurl STATIC
    src/http_client.cpp
    src/ffi.cpp
)

target_include_directories(acurl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(acurl
    PUBLIC
        CURL::libcurl
)

set_target_properties(acurl PROPERTIES
    VERSION ${PROJECT_VERSION}
    POSITION_INDEPENDENT_CODE ON
)

# =============================================================================
# Shared Library (for FFI)
# =============================================================================

add_library(acurl_shared SHARED
    src/http_client.cpp
    src/ffi.cpp
)

target_include_directories(acurl_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(acurl_shared
    PUBLIC
        CURL::libcurl
)

set_target_properties(acurl_shared PROPERTIES
    OUTPUT_NAME acurl
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    POSITION_INDEPENDENT_CODE ON
)

# =============================================================================
# CLI Utility
# =============================================================================

option(BUILD_CLI "Build acurl command-line utility" ON)
if(BUILD_CLI)
    add_executable(acurl_bin acurl_cli.cpp)
    target_link_libraries(acurl_bin PRIVATE acurl)
    set_target_properties(acurl_bin PROPERTIES OUTPUT_NAME acurl_util)
    install(TARGETS acurl_bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# =============================================================================
# Tests
# =============================================================================

option(BUILD_TESTS "Build test executable" ON)

if(BUILD_TESTS)
    enable_testing()

    add_executable(test_acurl tests/test_acurl.cpp)
    target_link_libraries(test_acurl PRIVATE acurl)

    add_test(NAME acurl_tests COMMAND test_acurl)
endif()

# =============================================================================
# Installation
# =============================================================================

include(GNUInstallDirs)

install(TARGETS acurl acurl_shared
    EXPORT acurlTargets
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
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/acurlConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/acurlConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/acurl
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/acurlConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/acurlConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/acurlConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/acurl
)

install(EXPORT acurlTargets
    FILE acurlTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/acurl
)

# =============================================================================
# Summary
# =============================================================================

message(STATUS "")
message(STATUS "acurl ${PROJECT_VERSION} - Hex-Stream HTTP Client")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  CURL: ${CURL_VERSION_STRING}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "")

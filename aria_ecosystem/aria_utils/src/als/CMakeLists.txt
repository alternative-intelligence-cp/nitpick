cmake_minimum_required(VERSION 3.16)
project(als
    VERSION 0.1.0
    DESCRIPTION "Hex-Stream Filesystem Enumerator for Aria"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror=return-type)
elseif(MSVC)
    add_compile_options(/W4)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Static library
add_library(als STATIC
    src/file_list.cpp
    src/ffi.cpp
)

target_include_directories(als
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(als PROPERTIES
    VERSION ${PROJECT_VERSION}
    POSITION_INDEPENDENT_CODE ON
)

# Shared library
add_library(als_shared SHARED
    src/file_list.cpp
    src/ffi.cpp
)

target_include_directories(als_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(als_shared PROPERTIES
    OUTPUT_NAME als
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    POSITION_INDEPENDENT_CODE ON
)

# Tests
option(BUILD_TESTS "Build test executable" ON)

if(BUILD_TESTS)
    enable_testing()
    add_executable(test_als tests/test_als.cpp)
    target_link_libraries(test_als PRIVATE als)
    add_test(NAME als_tests COMMAND test_als)
endif()

# CLI utility
option(BUILD_CLI "Build als CLI utility" ON)

if(BUILD_CLI)
    add_executable(als_bin als_cli.cpp)
    target_link_libraries(als_bin PRIVATE als)
    set_target_properties(als_bin PROPERTIES OUTPUT_NAME als_util)
    
    install(TARGETS als_bin
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS als als_shared
    EXPORT alsTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/alsConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/alsConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/als
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/alsConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/alsConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/alsConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/als
)

install(EXPORT alsTargets
    FILE alsTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/als
)

message(STATUS "")
message(STATUS "als ${PROJECT_VERSION} - Hex-Stream Filesystem Enumerator")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "")

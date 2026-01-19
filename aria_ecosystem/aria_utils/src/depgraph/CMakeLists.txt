# depgraph - Aria Dependency Graph Engine
# Part of the Aria Build System (aria_make)
#
# Build:
#   mkdir build && cd build
#   cmake ..
#   make
#
# Test:
#   make test
#   ./test_depgraph

cmake_minimum_required(VERSION 3.16)
project(depgraph
    VERSION 0.1.0
    DESCRIPTION "Dependency Graph Engine for AriaBuild"
    LANGUAGES CXX
)

# Require C++17 for std::filesystem, std::variant
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
# Library: depgraph (static)
# -----------------------------------------------------------------------------
add_library(depgraph STATIC
    src/node.cpp
    src/graph.cpp
    src/scheduler.cpp
    src/scanner.cpp
    src/incremental.cpp
    src/threadpool.cpp
    src/builder.cpp
)

target_include_directories(depgraph
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(depgraph PUBLIC Threads::Threads)

# Link filesystem library if needed (older GCC/Clang)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(depgraph PUBLIC stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(depgraph PUBLIC c++fs)
endif()

# Set library properties
set_target_properties(depgraph PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    POSITION_INDEPENDENT_CODE ON
)

# -----------------------------------------------------------------------------
# Library: depgraph_shared (shared library)
# -----------------------------------------------------------------------------
add_library(depgraph_shared SHARED
    src/node.cpp
    src/graph.cpp
    src/scheduler.cpp
    src/scanner.cpp
    src/incremental.cpp
    src/threadpool.cpp
    src/builder.cpp
)

target_include_directories(depgraph_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(depgraph_shared PUBLIC Threads::Threads)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(depgraph_shared PUBLIC stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(depgraph_shared PUBLIC c++fs)
endif()

set_target_properties(depgraph_shared PROPERTIES
    OUTPUT_NAME depgraph
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

# -----------------------------------------------------------------------------
# Test executable
# -----------------------------------------------------------------------------
option(BUILD_TESTS "Build test executable" ON)

if(BUILD_TESTS)
    enable_testing()

    add_executable(test_depgraph
        tests/test_depgraph.cpp
    )

    target_link_libraries(test_depgraph PRIVATE depgraph)

    add_test(NAME depgraph_tests COMMAND test_depgraph)
endif()

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS depgraph depgraph_shared
    EXPORT depgraphTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT depgraphTargets
    FILE depgraphTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/depgraph
)

# Generate config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/depgraphConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/depgraphConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/depgraphConfig.cmake"
    @ONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/depgraphConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/depgraphConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/depgraph
)

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "depgraph ${PROJECT_VERSION} - Dependency Graph Engine")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "")

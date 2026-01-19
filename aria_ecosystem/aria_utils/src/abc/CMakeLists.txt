# abc - Aria Build Configuration Parser
# Part of the Aria Build System (aria_make)
#
# Build:
#   mkdir build && cd build
#   cmake ..
#   make
#
# Test:
#   make test
#   ./test_abc

cmake_minimum_required(VERSION 3.16)
project(abc
    VERSION 0.1.0
    DESCRIPTION "ABC (Aria Build Configuration) Parser"
    LANGUAGES CXX
)

# Require C++17 for std::filesystem, std::variant, std::optional
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

# Find aglob library (sibling component)
# If building as part of aria_utils, aglob should already be available
if(NOT TARGET aglob)
    # Try to find aglob in parent directory
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../aglob/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../aglob" "${CMAKE_BINARY_DIR}/aglob")
    else()
        message(WARNING "aglob not found - source resolver will have limited functionality")
    endif()
endif()

# -----------------------------------------------------------------------------
# Library: abc (static)
# -----------------------------------------------------------------------------
add_library(abc STATIC
    src/token.cpp
    src/lexer.cpp
    src/parser.cpp
    src/interpolator.cpp
    src/source_resolver.cpp
)

target_include_directories(abc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link aglob for source resolution
if(TARGET aglob)
    target_link_libraries(abc PUBLIC aglob)
    target_include_directories(abc PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../aglob/include>
    )
endif()

# Link filesystem library if needed (older GCC/Clang)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(abc PUBLIC stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(abc PUBLIC c++fs)
endif()

# Set library properties
set_target_properties(abc PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    POSITION_INDEPENDENT_CODE ON
)

# -----------------------------------------------------------------------------
# Library: abc_shared (shared library for FFI)
# -----------------------------------------------------------------------------
add_library(abc_shared SHARED
    src/token.cpp
    src/lexer.cpp
    src/parser.cpp
    src/interpolator.cpp
    src/source_resolver.cpp
)

target_include_directories(abc_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(TARGET aglob_shared)
    target_link_libraries(abc_shared PUBLIC aglob_shared)
    target_include_directories(abc_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../aglob/include>
    )
elseif(TARGET aglob)
    target_link_libraries(abc_shared PUBLIC aglob)
    target_include_directories(abc_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../aglob/include>
    )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(abc_shared PUBLIC stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(abc_shared PUBLIC c++fs)
endif()

set_target_properties(abc_shared PROPERTIES
    OUTPUT_NAME abc
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

# -----------------------------------------------------------------------------
# Test executable
# -----------------------------------------------------------------------------
option(BUILD_TESTS "Build test executable" ON)

if(BUILD_TESTS)
    enable_testing()

    add_executable(test_abc
        tests/test_abc.cpp
    )

    target_link_libraries(test_abc PRIVATE abc)

    add_test(NAME abc_tests COMMAND test_abc)
endif()

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS abc abc_shared
    EXPORT abcTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT abcTargets
    FILE abcTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/abc
)

# Generate config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/abcConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/abcConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/abcConfig.cmake"
    @ONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/abcConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/abcConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/abc
)

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "abc ${PROJECT_VERSION} - Aria Build Configuration Parser")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Tests: ${BUILD_TESTS}")
if(TARGET aglob)
    message(STATUS "  aglob integration: YES")
else()
    message(STATUS "  aglob integration: NO")
endif()
message(STATUS "")

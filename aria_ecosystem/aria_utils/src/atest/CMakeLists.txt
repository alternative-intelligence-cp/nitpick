cmake_minimum_required(VERSION 3.16)
project(atest VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library
add_library(atest STATIC test_framework.cpp)

target_include_directories(atest
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(atest PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# -----------------------------------------------------------------------------
# CLI Utility
# -----------------------------------------------------------------------------
option(BUILD_CLI "Build atest command-line utility" ON)
if(BUILD_CLI)
    add_executable(atest_bin atest_cli.cpp)
    target_link_libraries(atest_bin PRIVATE atest)
    set_target_properties(atest_bin PROPERTIES OUTPUT_NAME atest_util)
    install(TARGETS atest_bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
option(BUILD_TESTS "Build atest tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)

install(TARGETS atest
    EXPORT atestTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES test_framework.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atest
)

install(EXPORT atestTargets
    FILE atestTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atest
)

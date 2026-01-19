cmake_minimum_required(VERSION 3.16)
project(astat VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library
add_library(astat STATIC file_stat.cpp)

target_include_directories(astat
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(astat PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# -----------------------------------------------------------------------------
# CLI Utility
# -----------------------------------------------------------------------------
option(BUILD_CLI "Build astat command-line utility" ON)
if(BUILD_CLI)
    add_executable(astat_bin astat_cli.cpp)
    target_link_libraries(astat_bin PRIVATE astat)
    set_target_properties(astat_bin PROPERTIES OUTPUT_NAME astat_util)
    install(TARGETS astat_bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
option(BUILD_TESTS "Build astat tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)

install(TARGETS astat
    EXPORT astatTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES file_stat.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/astat
)

install(EXPORT astatTargets
    FILE astatTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/astat
)

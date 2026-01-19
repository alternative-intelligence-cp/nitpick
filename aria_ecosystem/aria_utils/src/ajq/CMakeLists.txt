cmake_minimum_required(VERSION 3.16)
project(ajq VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library
add_library(ajq STATIC json_query.cpp)

target_include_directories(ajq
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(ajq PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# -----------------------------------------------------------------------------
# CLI Utility
# -----------------------------------------------------------------------------
option(BUILD_CLI "Build ajq command-line utility" ON)
if(BUILD_CLI)
    add_executable(ajq_bin ajq_cli.cpp)
    target_link_libraries(ajq_bin PRIVATE ajq)
    set_target_properties(ajq_bin PROPERTIES OUTPUT_NAME ajq_util)
    install(TARGETS ajq_bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
option(BUILD_TESTS "Build ajq tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)

install(TARGETS ajq
    EXPORT ajqTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES json_query.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ajq
)

install(EXPORT ajqTargets
    FILE ajqTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ajq
)

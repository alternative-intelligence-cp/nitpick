cmake_minimum_required(VERSION 3.16)
project(apic VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library
add_library(apic STATIC media_processor.cpp)

target_include_directories(apic
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(apic PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# -----------------------------------------------------------------------------
# CLI Utility
# -----------------------------------------------------------------------------
option(BUILD_CLI "Build apic command-line utility" ON)
if(BUILD_CLI)
    add_executable(apic_bin apic_cli.cpp)
    target_link_libraries(apic_bin PRIVATE apic)
    set_target_properties(apic_bin PROPERTIES OUTPUT_NAME apic_util)
    install(TARGETS apic_bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
option(BUILD_TESTS "Build apic tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)

install(TARGETS apic
    EXPORT apicTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES media_processor.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/apic
)

install(EXPORT apicTargets
    FILE apicTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/apic
)

cmake_minimum_required(VERSION 3.16)
project(acp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ACP_SOURCES
    src/file_copy.cpp
    src/ffi.cpp
)

add_library(acp STATIC ${ACP_SOURCES})

target_include_directories(acp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(acp PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

option(ACP_BUILD_CLI "Build acp command-line utility" ON)
if(ACP_BUILD_CLI)
    add_executable(acp_bin acp_cli.cpp)
    target_link_libraries(acp_bin PRIVATE acp)
    set_target_properties(acp_bin PROPERTIES OUTPUT_NAME acp)
    install(TARGETS acp_bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

option(ACP_BUILD_TESTS "Build acp tests" ON)
if(ACP_BUILD_TESTS)
    enable_testing()
    add_executable(test_acp tests/test_acp.cpp)
    target_link_libraries(test_acp PRIVATE acp)
    add_test(NAME acp_tests COMMAND test_acp)
endif()

include(GNUInstallDirs)

install(TARGETS acp
    EXPORT acpTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT acpTargets
    FILE acpTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/acp
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/acpConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/acpConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/acp
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/acpConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/acpConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/acpConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/acp
)

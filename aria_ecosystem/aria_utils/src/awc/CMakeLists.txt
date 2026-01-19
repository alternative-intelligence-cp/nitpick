cmake_minimum_required(VERSION 3.16)
project(awc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(AWC_SOURCES
    src/word_count.cpp
)

add_library(awc STATIC ${AWC_SOURCES})

target_include_directories(awc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(awc PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

option(AWC_BUILD_CLI "Build awc command-line utility" ON)
if(AWC_BUILD_CLI)
    add_executable(awc_bin awc_cli.cpp)
    target_link_libraries(awc_bin PRIVATE awc)
    set_target_properties(awc_bin PROPERTIES OUTPUT_NAME awc)
    install(TARGETS awc_bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

option(AWC_BUILD_TESTS "Build awc tests" OFF)
if(AWC_BUILD_TESTS)
    enable_testing()
    add_executable(test_awc tests/test_awc.cpp)
    target_link_libraries(test_awc PRIVATE awc)
    add_test(NAME awc_tests COMMAND test_awc)
endif()

include(GNUInstallDirs)

install(TARGETS awc
    EXPORT awcTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT awcTargets
    FILE awcTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/awc
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/awcConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/awcConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/awc
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/awcConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/awcConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/awcConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/awc
)

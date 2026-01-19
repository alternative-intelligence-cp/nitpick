cmake_minimum_required(VERSION 3.16)
project(agrep VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(AGREP_SOURCES
    src/pattern_search.cpp
)

add_library(agrep STATIC ${AGREP_SOURCES})

target_include_directories(agrep
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(agrep PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

option(AGREP_BUILD_CLI "Build agrep command-line utility" ON)
if(AGREP_BUILD_CLI)
    add_executable(agrep_bin agrep_cli.cpp)
    target_link_libraries(agrep_bin PRIVATE agrep)
    set_target_properties(agrep_bin PROPERTIES OUTPUT_NAME agrep)
    install(TARGETS agrep_bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

option(AGREP_BUILD_TESTS "Build agrep tests" OFF)
if(AGREP_BUILD_TESTS)
    enable_testing()
    add_executable(test_agrep tests/test_agrep.cpp)
    target_link_libraries(test_agrep PRIVATE agrep)
    add_test(NAME agrep_tests COMMAND test_agrep)
endif()

include(GNUInstallDirs)

install(TARGETS agrep
    EXPORT agrepTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT agrepTargets
    FILE agrepTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/agrep
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/agrepConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/agrepConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/agrep
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/agrepConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/agrepConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/agrepConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/agrep
)

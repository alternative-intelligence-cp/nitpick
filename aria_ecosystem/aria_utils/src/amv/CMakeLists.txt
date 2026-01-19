cmake_minimum_required(VERSION 3.16)
project(amv VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(AMV_SOURCES
    src/file_move.cpp
    src/ffi.cpp
)

add_library(amv STATIC ${AMV_SOURCES})

target_include_directories(amv
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(amv PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

option(AMV_BUILD_CLI "Build amv command-line utility" ON)
if(AMV_BUILD_CLI)
    add_executable(amv_bin amv_cli.cpp)
    target_link_libraries(amv_bin PRIVATE amv)
    set_target_properties(amv_bin PROPERTIES OUTPUT_NAME amv)
    install(TARGETS amv_bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

option(AMV_BUILD_TESTS "Build amv tests" ON)
if(AMV_BUILD_TESTS)
    enable_testing()
    add_executable(test_amv tests/test_amv.cpp)
    target_link_libraries(test_amv PRIVATE amv)
    add_test(NAME amv_tests COMMAND test_amv)
endif()

include(GNUInstallDirs)

install(TARGETS amv
    EXPORT amvTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT amvTargets
    FILE amvTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/amv
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/amvConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/amvConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/amv
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/amvConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/amvConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/amvConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/amv
)

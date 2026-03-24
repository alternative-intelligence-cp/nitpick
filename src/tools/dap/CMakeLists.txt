cmake_minimum_required(VERSION 3.16)
project(adap VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library sources
set(ADAP_SOURCES
    src/dap_protocol.cpp
    src/ffi.cpp
)

# Create static library
add_library(adap STATIC ${ADAP_SOURCES})

# Include directories
target_include_directories(adap
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Compiler warnings
target_compile_options(adap PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# Build tests
option(ADAP_BUILD_TESTS "Build adap tests" ON)
if(ADAP_BUILD_TESTS)
    enable_testing()

    add_executable(test_adap tests/test_adap.cpp)
    target_link_libraries(test_adap PRIVATE adap)

    add_test(NAME adap_tests COMMAND test_adap)
endif()

# Install rules
include(GNUInstallDirs)

install(TARGETS adap
    EXPORT adapTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets
install(EXPORT adapTargets
    FILE adapTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/adap
)

# Package config
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/adapConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/adapConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/adap
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/adapConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/adapConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/adapConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/adap
)

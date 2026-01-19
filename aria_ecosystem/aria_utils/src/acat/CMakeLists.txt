cmake_minimum_required(VERSION 3.16)
project(acat VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library sources
set(ACAT_SOURCES
    src/file_cat.cpp
    src/ffi.cpp
)

# Create static library
add_library(acat STATIC ${ACAT_SOURCES})

# Include directories
target_include_directories(acat
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Compiler warnings
target_compile_options(acat PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# Build tests
option(ACAT_BUILD_TESTS "Build acat tests" ON)
if(ACAT_BUILD_TESTS)
    enable_testing()

    add_executable(test_acat tests/test_acat.cpp)
    target_link_libraries(test_acat PRIVATE acat)

    add_test(NAME acat_tests COMMAND test_acat)
endif()

# Build CLI utility
option(ACAT_BUILD_CLI "Build acat CLI utility" ON)
if(ACAT_BUILD_CLI)
    add_executable(acat_bin acat_cli.cpp)
    target_link_libraries(acat_bin PRIVATE acat)
    set_target_properties(acat_bin PROPERTIES OUTPUT_NAME acat)
    
    install(TARGETS acat_bin
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Install rules
include(GNUInstallDirs)

install(TARGETS acat
    EXPORT acatTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets
install(EXPORT acatTargets
    FILE acatTargets.cmake
    NAMESPACE aria::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/acat
)

# Package config
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/acatConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/acatConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/acat
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/acatConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/acatConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/acatConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/acat
)

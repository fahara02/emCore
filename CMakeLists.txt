cmake_minimum_required(VERSION 3.16)

project(emCore 
    VERSION 1.0.0
    DESCRIPTION "Embedded C++ Core Library - Header-only, MCU Agnostic"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use Ninja generator by default
set(CMAKE_GENERATOR "Ninja" CACHE STRING "CMake generator")

# Compiler flags for embedded systems
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og -g")

# Header-only library
add_library(emCore INTERFACE)

# Include directories
target_include_directories(emCore INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# ETL dependency
find_package(etl QUIET)
if(NOT etl_FOUND)
    # Fallback to ETL as header-only
    target_include_directories(emCore INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/etl/include>
    )
endif()

# Compiler features
target_compile_features(emCore INTERFACE cxx_std_17)

# Compile definitions for embedded systems
target_compile_definitions(emCore INTERFACE
    ETL_NO_STL
    ETL_TARGET_DEVICE_GENERIC
    $<$<CONFIG:Debug>:EMCORE_DEBUG>
)

# Installation
include(GNUInstallDirs)
install(TARGETS emCore
    EXPORT emCoreTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT emCoreTargets
    FILE emCoreTargets.cmake
    NAMESPACE emCore::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/emCore
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    emCoreConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/emCoreConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/emCoreConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/emCore
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/emCoreConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/emCoreConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/emCore
)

# Task generation from YAML
include(cmake/GenerateTasks.cmake)
option(EMCORE_GENERATE_TASKS "Generate task configuration from YAML" ON)
if(EMCORE_GENERATE_TASKS)
    emcore_generate_tasks()
endif()

# Testing
option(EMCORE_BUILD_TESTS "Build emCore tests" OFF)
if(EMCORE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Examples
option(EMCORE_BUILD_EXAMPLES "Build emCore examples" OFF)
if(EMCORE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

cmake_minimum_required(VERSION 3.16)

project(emCore 
    VERSION 1.0.0
    DESCRIPTION "Embedded C++ Core Library with automatic task generation"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Download ETL library if not present (only mandatory dependency)
include(FetchContent)
FetchContent_Declare(
    etl
    GIT_REPOSITORY https://github.com/ETLCPP/etl.git
    GIT_TAG        20.39.4
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/external/Embedded\ Template\ Library
)

# Make ETL available
FetchContent_MakeAvailable(etl)

# Library with dummy source to trigger CMake processing
add_library(emCore STATIC src/emCore_dummy.cpp)

# Include directories
target_include_directories(emCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/Embedded\ Template\ Library/include>
    $<INSTALL_INTERFACE:include>
)

# Optional: Fetch eAlloc (TLSF) when enabled. This keeps ETL as the only
# mandatory dependency and makes TLSF opt-in.
option(EMCORE_ENABLE_TLSF "Enable TLSF allocator (fetch eAlloc and expose header)" OFF)
if(EMCORE_ENABLE_TLSF)
    message(STATUS "emCore: TLSF enabled – fetching eAlloc…")
    FetchContent_Declare(
        ealloc
        GIT_REPOSITORY https://github.com/fahara02/eAlloc.git
        GIT_TAG        main
        SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/external/eAlloc
    )
    FetchContent_MakeAvailable(ealloc)
    target_include_directories(emCore PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/eAlloc>
    )
    target_compile_definitions(emCore PUBLIC EMCORE_ENABLE_TLSF=1)
endif()


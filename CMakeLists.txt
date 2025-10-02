cmake_minimum_required(VERSION 3.16)

project(emCore 
    VERSION 1.0.0
    DESCRIPTION "Embedded C++ Core Library with automatic task generation"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Library with dummy source to trigger CMake processing
add_library(emCore STATIC src/emCore_dummy.cpp)

# Include directories
target_include_directories(emCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# Task generation from YAML - triggered when library is built
include(cmake/GenerateTasks.cmake)

# FUCK IT - JUST ALWAYS GENERATE IF TASKS.YAML EXISTS!
message(STATUS "emCore: 🔥 CHECKING FOR TASK GENERATION...")

# Look for tasks.yaml in the parent project (user's project)
set(USER_TASKS_YAML "${CMAKE_SOURCE_DIR}/tasks.yaml")
message(STATUS "emCore: Looking for tasks.yaml at: ${USER_TASKS_YAML}")

if(EXISTS ${USER_TASKS_YAML})
    message(STATUS "emCore: 🚀 FOUND TASKS.YAML - GENERATING NOW!")
    set(EMCORE_USER_TASKS_YAML ${USER_TASKS_YAML})
    emcore_generate_tasks()
else()
    message(STATUS "emCore: No tasks.yaml found - skipping generation")
    message(STATUS "  Create ${USER_TASKS_YAML} to enable automatic task generation")
endif()

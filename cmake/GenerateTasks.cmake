# CMake module to generate task configuration from YAML

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Function to generate task configuration from YAML
function(emcore_generate_tasks)
    set(options "")
    set(oneValueArgs YAML_FILE OUTPUT_FILE)
    set(multiValueArgs "")
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Default to default_tasks.yaml if not specified
    if(NOT ARG_YAML_FILE)
        if(DEFINED EMCORE_USER_TASKS_YAML AND EXISTS ${EMCORE_USER_TASKS_YAML})
            set(ARG_YAML_FILE ${EMCORE_USER_TASKS_YAML})
            message(STATUS "Using user tasks: ${ARG_YAML_FILE}")
        else()
            set(ARG_YAML_FILE ${CMAKE_CURRENT_SOURCE_DIR}/config/default_tasks.yaml)
            message(STATUS "Using default tasks: ${ARG_YAML_FILE}")
        endif()
    endif()
    
    # Default output file
    if(NOT ARG_OUTPUT_FILE)
        set(ARG_OUTPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/emCore/task/generated_tasks.hpp)
    endif()
    
    # Generate task configuration
    add_custom_command(
        OUTPUT ${ARG_OUTPUT_FILE}
        COMMAND ${Python3_EXECUTABLE} 
                ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_tasks.py
                ${ARG_YAML_FILE}
                ${ARG_OUTPUT_FILE}
        DEPENDS ${ARG_YAML_FILE}
                ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_tasks.py
        COMMENT "Generating task configuration from ${ARG_YAML_FILE}"
        VERBATIM
    )
    
    # Add to sources
    set(GENERATED_TASKS_HEADER ${ARG_OUTPUT_FILE} PARENT_SCOPE)
endfunction()

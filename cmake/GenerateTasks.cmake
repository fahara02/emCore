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
    
    # Default output file - put it in the user's project src directory
    if(NOT ARG_OUTPUT_FILE)
        if(DEFINED EMCORE_USER_TASKS_YAML)
            # User project - put generated file in their src directory
            get_filename_component(USER_PROJECT_DIR ${EMCORE_USER_TASKS_YAML} DIRECTORY)
            set(ARG_OUTPUT_FILE ${USER_PROJECT_DIR}/src/generated_tasks.hpp)
        else()
            # Library build - put in library directory
            set(ARG_OUTPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/emCore/task/generated_tasks.hpp)
        endif()
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
    
    # Create a custom target to ensure the file is generated
    add_custom_target(generate_tasks_target
        DEPENDS ${ARG_OUTPUT_FILE}
        COMMENT "Ensuring task configuration is generated"
    )
    
    # IMMEDIATELY run the generator during CMake configuration
    message(STATUS "emCore: 🔥 RUNNING TASK GENERATOR NOW...")
    execute_process(
        COMMAND ${Python3_EXECUTABLE} 
                ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_tasks.py
                ${ARG_YAML_FILE}
                ${ARG_OUTPUT_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GENERATOR_RESULT
        OUTPUT_VARIABLE GENERATOR_OUTPUT
        ERROR_VARIABLE GENERATOR_ERROR
    )
    
    if(GENERATOR_RESULT EQUAL 0)
        message(STATUS "emCore: ✅ Task generation completed successfully!")
        if(GENERATOR_OUTPUT)
            message(STATUS "emCore: ${GENERATOR_OUTPUT}")
        endif()
    else()
        message(WARNING "emCore: ❌ Task generation failed!")
        if(GENERATOR_ERROR)
            message(WARNING "emCore: ${GENERATOR_ERROR}")
        endif()
    endif()
    
    # Add to sources
    set(GENERATED_TASKS_HEADER ${ARG_OUTPUT_FILE} PARENT_SCOPE)
    
    message(STATUS "emCore: Task generation configured")
    message(STATUS "  Input:  ${ARG_YAML_FILE}")
    message(STATUS "  Output: ${ARG_OUTPUT_FILE}")
endfunction()

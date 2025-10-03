#pragma once

/**
 * @file emCore.hpp
 * @brief Main header for emCore - Embedded C++ Core Library
 * 
 * Header-only, MCU agnostic library with no RTTI and no dynamic allocation.
 * Depends only on ETL (Embedded Template Library).
 * 
 * @version 1.0.0
 * @date 2025
 */

// Platform detection
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    #define EMCORE_PLATFORM_ESP32
#elif defined(ARDUINO)
    #define EMCORE_PLATFORM_ARDUINO
#elif defined(__arm__)
    #define EMCORE_PLATFORM_ARM
#else
    #define EMCORE_PLATFORM_GENERIC
#endif

// Configuration
#ifndef EMCORE_MAX_TASKS
    #define EMCORE_MAX_TASKS 16
#endif

#ifndef EMCORE_MAX_EVENTS
    #define EMCORE_MAX_EVENTS 32
#endif

// Core library modules (types brings in etl_compat which ensures initializer_list availability)
#include "emCore/core/types.hpp"
#include "emCore/core/config.hpp"

// ETL includes
#include <etl/vector.h>
#include <etl/queue.h>
#include <etl/function.h>
#include <etl/delegate.h>
#include <etl/optional.h>
#include <etl/variant.h>
#include <etl/string.h>
#include <etl/array.h>
#include "emCore/task/taskmaster.hpp"
#include "emCore/memory/pool.hpp"
#include "emCore/event/dispatcher.hpp"
#include "emCore/utils/helpers.hpp"

/**
 * @namespace emCore
 * @brief Main namespace for the embedded core library
 */
namespace emCore {
    
    /**
     * @brief Initialize the emCore library
     * @return true if initialization successful, false otherwise
     */
    inline bool initialize() noexcept {
        // Initialize subsystems
        return true;
    }
    
    /**
     * @brief Get library version
     */
    constexpr const char* version() noexcept {
        return "1.0.0";
    }
    
    // Optional auto-boot wiring for generated systems (commands + YAML tasks)
    // Define EMCORE_ENABLE_AUTO_BOOT to enable calling generated setup functions.
    // This keeps init unified without forcing integrators.
    #if defined(__has_include)
    
    // Command table
    #  if __has_include("generated_command_table.hpp")
    #    include "generated_command_table.hpp"
    #    define EMCORE_HAS_GEN_CMD 1
    #  elif __has_include(<emCore/generated_command_table.hpp>)
    #    include <emCore/generated_command_table.hpp>
    #    define EMCORE_HAS_GEN_CMD 1
    #  endif
    
    // Tasks YAML setup
    #  if __has_include("generated_tasks.hpp")
    #    include "generated_tasks.hpp"
    #    define EMCORE_HAS_GEN_TASKS 1
    #  elif __has_include(<emCore/generated_tasks.hpp>)
    #    include <emCore/generated_tasks.hpp>
    #  endif
    
    #endif // __has_include
    
    // Unified boot: conditionally call generated setup if available
    inline void boot(taskmaster& task_mgr) noexcept {
#ifdef EMCORE_ENABLE_AUTO_BOOT
#if defined(EMCORE_HAS_GEN_CMD)
        emCore::commands::setup_command_dispatcher();
#endif
#if defined(EMCORE_HAS_GEN_TASKS)
        setup_yaml_system(task_mgr);
#else
        (void)task_mgr;
#endif
#else
        (void)task_mgr; // Auto boot disabled
#endif
    }
    
} // namespace emCore

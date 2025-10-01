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

// ETL includes
#include <etl/vector.h>
#include <etl/queue.h>
#include <etl/function.h>
#include <etl/delegate.h>
#include <etl/optional.h>
#include <etl/variant.h>
#include <etl/string.h>
#include <etl/array.h>

// Core library modules
#include "emCore/core/types.hpp"
#include "emCore/core/config.hpp"
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
     * @return Version string
     */
    constexpr const char* version() noexcept {
        return "1.0.0";
    }
    
} // namespace emCore

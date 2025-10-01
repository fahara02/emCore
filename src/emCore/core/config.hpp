#pragma once

#include <cstddef>

#include "types.hpp"

namespace emCore {
namespace config {
        
        // Task system configuration
        constexpr size_t max_tasks = EMCORE_MAX_TASKS;
        constexpr size_t max_task_name_length = 32;
        constexpr duration_t default_task_timeout = 1000; // ms
        
        // Event system configuration
        constexpr size_t max_events = EMCORE_MAX_EVENTS;
        constexpr size_t max_event_handlers = 16;
        constexpr size_t event_queue_size = 64;
        
        // Memory pool configuration
        constexpr size_t small_block_size = 32;
        constexpr size_t medium_block_size = 128;
        constexpr size_t large_block_size = 512;
        
        constexpr size_t small_pool_count = 16;
        constexpr size_t medium_pool_count = 8;
        constexpr size_t large_pool_count = 4;
        
        // Platform specific configurations
        #ifdef EMCORE_PLATFORM_ESP32
            constexpr u32 system_clock_hz = 240000000; // 240MHz
            constexpr size_t stack_size_default = 4096;
        #elif defined(EMCORE_PLATFORM_ARDUINO)
            constexpr u32 system_clock_hz = 16000000; // 16MHz
            constexpr size_t stack_size_default = 1024;
        #else
            constexpr u32 system_clock_hz = 1000000; // 1MHz default
            constexpr size_t stack_size_default = 2048;
        #endif
        
        // Debug configuration
        #ifdef EMCORE_DEBUG
            constexpr bool debug_enabled = true;
            constexpr bool assert_enabled = true;
        #else
            constexpr bool debug_enabled = false;
            constexpr bool assert_enabled = false;
        #endif
        
}  // namespace config
}  // namespace emCore

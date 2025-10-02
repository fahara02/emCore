#pragma once

#include "../core/types.hpp"
#include "../core/config.hpp"
#include "watchdog.hpp"  // For watchdog_action enum

namespace emCore {

// Strong types for task configuration - using unique tag types for distinctness!
struct stack_size_tag {};
struct rtos_priority_tag {};

using stack_size_bytes = core::strong_type<u32, stack_size_tag>;
using rtos_priority_level = core::strong_type<u32, rtos_priority_tag>;

// Convenience factory functions
namespace make {
    constexpr stack_size_bytes stack_size(u32 bytes) noexcept { return stack_size_bytes(bytes); }
    constexpr rtos_priority_level rtos_priority(u32 priority) noexcept { return rtos_priority_level(priority); }
}

// Forward declaration - watchdog_action is defined in watchdog.hpp
enum class watchdog_action : u8;

// Strong types for advanced task attributes - using automatic unique IDs
using watchdog_timeout_ms = core::strong_type_generator<u32>::type;
using max_execution_us = core::strong_type_generator<u32>::type;
using cpu_affinity_core = core::strong_type_generator<i8>::type;

// Convenience factory functions for advanced attributes
namespace make {
    constexpr watchdog_timeout_ms watchdog_timeout(u32 ms) noexcept { return watchdog_timeout_ms(ms); }
    constexpr max_execution_us max_execution(u32 us) noexcept { return max_execution_us(us); }
    constexpr cpu_affinity_core cpu_affinity(i8 core) noexcept { return cpu_affinity_core(core); }
}

/**
 * @brief Extended task configuration structure supporting all YAML attributes
 * 
 * This structure defines all parameters needed to create a task with full YAML support.
 * Tasks should be configured at compile-time in a configuration table
 * and created once during initialization.
 */
struct task_config {
    using task_function_ptr = void (*)(void*);
    
    // Basic task attributes
    task_function_ptr function;      // Pointer to task function
    const char* name;                // Task name (max 32 chars)
    void* parameters;                // Optional parameters pointer
    priority priority_level;         // Task priority
    duration_t period_ms;            // Task period in milliseconds (0 = run once)
    bool enabled;                    // Is task enabled at startup
    stack_size_bytes stack_size;     // Stack size in bytes (for native RTOS tasks)
    rtos_priority_level rtos_priority; // RTOS priority (0-31 for FreeRTOS)
    bool create_native;              // True = create native RTOS task, False = cooperative
    
    // Advanced YAML attributes
    cpu_affinity_core cpu_affinity;  // CPU core to pin task to (-1 = no affinity)
    watchdog_timeout_ms watchdog_timeout; // Watchdog timeout in milliseconds
    watchdog_action watchdog_action_type;  // What to do on watchdog timeout
    max_execution_us max_execution_time;   // Maximum execution time in microseconds (0 = no limit)
    
    constexpr task_config(
        task_function_ptr func,
        const char* task_name,
        priority prio = priority::normal,
        duration_t period = 0,
        void* params = nullptr,
        bool is_enabled = true,
        stack_size_bytes stack = stack_size_bytes(4096),
        rtos_priority_level rtos_prio = rtos_priority_level(5),
        bool native = false,
        cpu_affinity_core affinity = cpu_affinity_core(-1),
        watchdog_timeout_ms wd_timeout = watchdog_timeout_ms(10000),
        watchdog_action wd_action = watchdog_action::log_warning,
        max_execution_us max_exec = max_execution_us(0)
    ) noexcept
        : function(func)
        , name(task_name)
        , parameters(params)
        , priority_level(prio)
        , period_ms(period)
        , enabled(is_enabled)
        , stack_size(stack)
        , rtos_priority(rtos_prio)
        , create_native(native)
        , cpu_affinity(affinity)
        , watchdog_timeout(wd_timeout)
        , watchdog_action_type(wd_action)
        , max_execution_time(max_exec) {}
};

}  // namespace emCore

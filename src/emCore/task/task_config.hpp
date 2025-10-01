#pragma once

#include "../core/types.hpp"

namespace emCore {

/**
 * @brief Task configuration structure for static task initialization
 * 
 * This structure defines all parameters needed to create a task.
 * Tasks should be configured at compile-time in a configuration table
 * and created once during initialization.
 */
struct task_config {
    using task_function_ptr = void (*)(void*);
    
    task_function_ptr function;      // Pointer to task function
    const char* name;                // Task name (max 32 chars)
    void* parameters;                // Optional parameters pointer
    priority priority_level;         // Task priority
    duration_t period_ms;            // Task period in milliseconds (0 = run once)
    bool enabled;                    // Is task enabled at startup
    u32 stack_size;                  // Stack size in bytes (for native RTOS tasks)
    u32 rtos_priority;               // RTOS priority (0-31 for FreeRTOS)
    bool create_native;              // True = create native RTOS task, False = cooperative
    
    constexpr task_config(
        task_function_ptr func,
        const char* task_name,
        priority prio = priority::normal,
        duration_t period = 0,
        void* params = nullptr,
        bool is_enabled = true,
        u32 stack = 4096,
        u32 rtos_prio = 5,
        bool native = false
    ) noexcept
        : function(func)
        , name(task_name)
        , parameters(params)
        , priority_level(prio)
        , period_ms(period)
        , enabled(is_enabled)
        , stack_size(stack)
        , rtos_priority(rtos_prio)
        , create_native(native) {}
};

}  // namespace emCore

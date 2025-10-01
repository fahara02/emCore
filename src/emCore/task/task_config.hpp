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
    
    constexpr task_config(
        task_function_ptr func,
        const char* task_name,
        priority prio = priority::normal,
        duration_t period = 0,
        void* params = nullptr,
        bool is_enabled = true
    ) noexcept
        : function(func)
        , name(task_name)
        , parameters(params)
        , priority_level(prio)
        , period_ms(period)
        , enabled(is_enabled) {}
};

}  // namespace emCore

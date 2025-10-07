#pragma once

#include "../core/types.hpp"
#include "../platform/platform.hpp"

namespace emCore::os {

using task_handle_t = platform::task_handle_t;
using task_function_t = platform::task_function_t;

struct task_create_params {
    task_function_t function{nullptr};
    const char* name{nullptr};
    u32 stack_size{0};           // bytes; platform converts to words if needed
    void* parameters{nullptr};
    u32 priority{0};             // platform-native priority value
    task_handle_t* handle{nullptr};
    bool start_suspended{false};
    bool pin_to_core{false};
    int core_id{-1};
};

inline bool create_native_task(const task_create_params& params) noexcept {
    platform::task_create_params create_params{};
    create_params.function = params.function;
    create_params.name = params.name;
    create_params.stack_size = params.stack_size;
    create_params.parameters = params.parameters;
    create_params.priority = params.priority;
    create_params.handle = params.handle;
    create_params.start_suspended = params.start_suspended;
    create_params.pin_to_core = params.pin_to_core;
    create_params.core_id = params.core_id;
    return platform::create_native_task(create_params);
}

inline bool delete_native_task(task_handle_t handle) noexcept { return platform::delete_native_task(handle); }
inline bool suspend_native_task(task_handle_t handle) noexcept { return platform::suspend_native_task(handle); }
inline bool resume_native_task(task_handle_t handle) noexcept { return platform::resume_native_task(handle); }

inline bool notify_task(task_handle_t handle, u32 value = 0x01) noexcept { return platform::notify_task(handle, value); }
inline bool wait_notification(u32 timeout_ms, u32* out_value) noexcept { return platform::wait_notification(timeout_ms, out_value); }
inline void clear_notification() noexcept { platform::clear_notification(); }
inline task_handle_t current_task() noexcept { return platform::get_current_task_handle(); }
inline void yield() noexcept { platform::task_yield(); }
inline size_t stack_high_water_mark() noexcept { return platform::get_stack_high_water_mark(); }

} // namespace emCore::os

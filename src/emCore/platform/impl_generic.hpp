#pragma once

#include "platform_base.hpp"

namespace emCore::platform::impl_generic {

struct critical_section {
    void enter() const noexcept {}
    void exit() const noexcept {}
};

inline timestamp_t get_system_time_us() noexcept {
    static timestamp_t counter = 0;
    return ++counter; // monotonic stub
}
inline timestamp_t get_system_time() noexcept { return get_system_time_us() / 1000; }

inline void delay_ms(duration_t ms) noexcept {
    const timestamp_t start = get_system_time();
    while ((get_system_time() - start) < ms) {}
}
inline void delay_us(u32 us) noexcept { (void)us; }

/* logging provided centrally by platform.hpp */

inline void system_reset() noexcept { while (true) { /* hang */ } }
inline void task_yield() noexcept {}
inline size_t get_stack_high_water_mark() noexcept { return 0; }

using task_handle_t = platform::task_handle_t;
using task_function_t = platform::task_function_t;
using task_create_params = platform::task_create_params;

inline bool create_native_task(const task_create_params& params) noexcept {
    (void)params; return false;
}
inline bool delete_native_task(task_handle_t) noexcept { return false; }
inline bool suspend_native_task(task_handle_t) noexcept { return false; }
inline bool resume_native_task(task_handle_t) noexcept { return false; }
inline task_handle_t get_current_task_handle() noexcept { return nullptr; }

inline bool notify_task(task_handle_t, u32) noexcept { return false; }
inline bool wait_notification(u32, u32*) noexcept { return false; }
inline void clear_notification() noexcept {}

using semaphore_handle_t = void*;
inline semaphore_handle_t create_binary_semaphore() noexcept { return nullptr; }
inline void delete_semaphore(semaphore_handle_t) noexcept {}
inline bool semaphore_give(semaphore_handle_t) noexcept { return false; }
inline bool semaphore_take(semaphore_handle_t, duration_t) noexcept { return false; }

inline constexpr platform_info get_platform_info() noexcept { return {"Generic", 1000000U, false}; }

} // namespace emCore::platform::impl_generic

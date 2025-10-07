#pragma once

#include "platform_base.hpp"

#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <cstdio>

namespace emCore::platform::impl_posix {

struct critical_section {
    mutable pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    void enter() const noexcept { (void)pthread_mutex_lock(&mtx); }
    void exit() const noexcept { (void)pthread_mutex_unlock(&mtx); }
};

inline timestamp_t get_system_time_us() noexcept {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<timestamp_t>(ts.tv_sec) * 1000000ULL + static_cast<timestamp_t>(ts.tv_nsec / 1000);
}
inline timestamp_t get_system_time() noexcept { return get_system_time_us() / 1000ULL; }

inline void delay_ms(duration_t ms) noexcept { usleep(static_cast<useconds_t>(ms * 1000ULL)); }
inline void delay_us(u32 us) noexcept { usleep(static_cast<useconds_t>(us)); }

/* logging provided centrally by platform.hpp */

inline void system_reset() noexcept { _exit(1); }
inline void task_yield() noexcept { sched_yield(); }
inline size_t get_stack_high_water_mark() noexcept { return 0; }

using task_handle_t = platform::task_handle_t;
using task_function_t = platform::task_function_t;
using task_create_params = platform::task_create_params;

inline bool create_native_task(const task_create_params&) noexcept { return false; }
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

inline constexpr platform_info get_platform_info() noexcept { return {"POSIX", 1000000000U, false}; }

} // namespace emCore::platform::impl_posix

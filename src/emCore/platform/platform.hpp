#pragma once

#if 1 // EMCORE platform facade (new)

#include "platform_base.hpp"
#include <cstdio>

// Auto-detect platform if the build system did not provide one
#if !defined(EMCORE_PLATFORM_ESP32) && \
    !defined(EMCORE_PLATFORM_ARDUINO) && \
    !defined(EMCORE_PLATFORM_STM32) && \
    !defined(EMCORE_PLATFORM_POSIX) && \
    !defined(EMCORE_PLATFORM_GENERIC)
  #if defined(ESP32) || defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
    #define EMCORE_PLATFORM_ESP32
  #elif defined(ARDUINO)
    #define EMCORE_PLATFORM_ARDUINO
  #elif defined(__unix__) || defined(__APPLE__)
    #define EMCORE_PLATFORM_POSIX
  #else
    #define EMCORE_PLATFORM_GENERIC
  #endif
#endif

#if defined(EMCORE_PLATFORM_ESP32)
#  include "impl_esp32.hpp"
   namespace emCore::platform { namespace impl = emCore::platform::impl_esp32; }
#elif defined(EMCORE_PLATFORM_ARDUINO)
#  if defined(__has_include)
#    if __has_include(<Arduino.h>)
#      include "impl_arduino.hpp"
       namespace emCore::platform { namespace impl = emCore::platform::impl_arduino; }
#    else
#      include "impl_generic.hpp"
       namespace emCore::platform { namespace impl = emCore::platform::impl_generic; }
#    endif
#  else
#    include "impl_arduino.hpp"
     namespace emCore::platform { namespace impl = emCore::platform::impl_arduino; }
#  endif
#elif defined(EMCORE_PLATFORM_STM32)
#  include "impl_stm32.hpp"
   namespace emCore::platform { namespace impl = emCore::platform::impl_stm32; }
#elif defined(EMCORE_PLATFORM_POSIX)
#  include "impl_posix.hpp"
   namespace emCore::platform { namespace impl = emCore::platform::impl_posix; }
#else
#  include "impl_generic.hpp"
   namespace emCore::platform { namespace impl = emCore::platform::impl_generic; }
#endif

namespace emCore::platform {

// Re-export primitives and functions from selected impl
using critical_section = impl::critical_section;

inline timestamp_t get_system_time_us() noexcept { return impl::get_system_time_us(); }
inline timestamp_t get_system_time() noexcept { return impl::get_system_time(); }
inline void delay_ms(duration_t milliseconds) noexcept { impl::delay_ms(milliseconds); }
inline void delay_us(u32 microseconds) noexcept { impl::delay_us(microseconds); }

inline void system_reset() noexcept { impl::system_reset(); }
inline void task_yield() noexcept { impl::task_yield(); }
inline size_t get_stack_high_water_mark() noexcept { return impl::get_stack_high_water_mark(); }

// Types come from platform_base.hpp
inline bool create_native_task(const task_create_params& params) noexcept { return impl::create_native_task(params); }
inline bool delete_native_task(task_handle_t handle) noexcept { return impl::delete_native_task(handle); }
inline bool suspend_native_task(task_handle_t handle) noexcept { return impl::suspend_native_task(handle); }
inline bool resume_native_task(task_handle_t handle) noexcept { return impl::resume_native_task(handle); }
inline task_handle_t get_current_task_handle() noexcept { return impl::get_current_task_handle(); }

inline bool notify_task(task_handle_t handle, u32 value = 0x01) noexcept { return impl::notify_task(handle, value); }
inline bool wait_notification(u32 timeout_ms, u32* out_value) noexcept { return impl::wait_notification(timeout_ms, out_value); }
inline void clear_notification() noexcept { impl::clear_notification(); }

using semaphore_handle_t = decltype(impl::create_binary_semaphore());
inline semaphore_handle_t create_binary_semaphore() noexcept { return impl::create_binary_semaphore(); }
inline void delete_semaphore(semaphore_handle_t handle) noexcept { impl::delete_semaphore(handle); }
inline bool semaphore_give(semaphore_handle_t handle) noexcept { return impl::semaphore_give(handle); }
inline bool semaphore_take(semaphore_handle_t handle, duration_t timeout_us) noexcept { return impl::semaphore_take(handle, timeout_us); }

constexpr platform_info get_platform_info() noexcept { return impl::get_platform_info(); }

// Centralized logging
#ifndef EMCORE_ENABLE_LOGGING
#define EMCORE_ENABLE_LOGGING 1 /* NOLINT(cppcoreguidelines-macro-usage) */
#endif

namespace detail {
inline void log_sink(const char* msg) noexcept {
#if defined(EMCORE_PLATFORM_ARDUINO)
    if (msg) { Serial.println(msg); }
#elif defined(EMCORE_PLATFORM_ESP32)
    if (msg) { std::printf("%s\n", msg); }
#elif defined(EMCORE_PLATFORM_POSIX)
    if (msg) { std::puts(msg); }
#elif defined(EMCORE_PLATFORM_STM32)
    (void)msg; // Optionally add ITM here if CMSIS is included
#else
    (void)msg;
#endif
}
} // namespace detail

inline void log(const char* message) noexcept {
#if EMCORE_ENABLE_LOGGING
    detail::log_sink(message);
#else
    (void)message;
#endif
}

inline void logf(const char* fmt, u32 arg1) noexcept {
#if EMCORE_ENABLE_LOGGING
    char buffer[256]; std::snprintf(buffer, sizeof(buffer), fmt, arg1); /* NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg,cppcoreguidelines-pro-bounds-array-to-pointer-decay) */ log(buffer);
#else
    (void)fmt; (void)arg1;
#endif
}
inline void logf(const char* fmt, u32 arg1, u32 arg2) noexcept { char buffer[256]; std::snprintf(buffer, sizeof(buffer), fmt, arg1, arg2); /* NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg,cppcoreguidelines-pro-bounds-array-to-pointer-decay) */ log(buffer); }
inline void logf(const char* fmt, u32 arg1, u32 arg2, u32 arg3) noexcept { char buffer[256]; std::snprintf(buffer, sizeof(buffer), fmt, arg1, arg2, arg3); /* NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg,cppcoreguidelines-pro-bounds-array-to-pointer-decay) */ log(buffer); }
inline void logf(const char* fmt, u32 arg1, u32 arg2, u32 arg3, u32 arg4) noexcept { char buffer[256]; std::snprintf(buffer, sizeof(buffer), fmt, arg1, arg2, arg3, arg4); /* NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg,cppcoreguidelines-pro-bounds-array-to-pointer-decay) */ log(buffer); }
inline void logf(const char* fmt, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5) noexcept { char buffer[256]; std::snprintf(buffer, sizeof(buffer), fmt, arg1, arg2, arg3, arg4, arg5); /* NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg,cppcoreguidelines-pro-bounds-array-to-pointer-decay) */ log(buffer); }

} // namespace emCore::platform

#endif // EMCORE platform facade (new)


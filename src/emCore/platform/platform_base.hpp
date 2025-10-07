#pragma once

#include "../core/types.hpp"

namespace emCore::platform {

// Compile-time platform kind selection
enum class platform_kind : unsigned {
    esp32,
    arduino,
    stm32,
    posix,
    generic
};

#ifndef EMCORE_PLATFORM_KIND
// Map legacy macros to platform_kind if EMCORE_PLATFORM_KIND not provided
#  if defined(EMCORE_PLATFORM_ESP32)
#    define EMCORE_PLATFORM_KIND emCore::platform::platform_kind::esp32
#  elif defined(EMCORE_PLATFORM_ARDUINO)
#    define EMCORE_PLATFORM_KIND emCore::platform::platform_kind::arduino
#  elif defined(EMCORE_PLATFORM_STM32)
#    define EMCORE_PLATFORM_KIND emCore::platform::platform_kind::stm32
#  elif defined(EMCORE_PLATFORM_POSIX)
#    define EMCORE_PLATFORM_KIND emCore::platform::platform_kind::posix
#  else
#    define EMCORE_PLATFORM_KIND emCore::platform::platform_kind::generic
#  endif
#endif

// Common API types kept stable for the facade
using task_handle_t = void*;
using task_function_t = void (*)(void*);

struct task_create_params {
    task_function_t function{nullptr};
    const char* name{nullptr};
    u32 stack_size{0};
    void* parameters{nullptr};
    u32 priority{0};
    task_handle_t* handle{nullptr};
    bool start_suspended{false};
    bool pin_to_core{false};
    int core_id{-1};
};

struct platform_info {
    const char* name;
    u32 clock_hz;
    bool has_rtos;
};

// Selected platform as a compile-time constant for constexpr dispatch
inline constexpr platform_kind selected_platform = EMCORE_PLATFORM_KIND;

} // namespace emCore::platform

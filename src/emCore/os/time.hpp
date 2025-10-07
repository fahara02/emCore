#pragma once

#include "../core/types.hpp"
#include "../platform/platform.hpp"

namespace emCore::os {

inline timestamp_t time_us() noexcept { return platform::get_system_time_us(); }
inline timestamp_t time_ms() noexcept { return platform::get_system_time(); }

inline void delay_ms(duration_t milliseconds) noexcept { platform::delay_ms(milliseconds); }
inline void delay_us(u32 microseconds) noexcept { platform::delay_us(microseconds); }

} // namespace emCore::os

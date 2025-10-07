#pragma once

#include "../core/types.hpp"
#include "../platform/platform.hpp"

namespace emCore::os {

using critical_section = platform::critical_section;
using semaphore_handle_t = platform::semaphore_handle_t;

inline semaphore_handle_t create_binary_semaphore() noexcept { return platform::create_binary_semaphore(); }
inline void delete_semaphore(semaphore_handle_t handle) noexcept { platform::delete_semaphore(handle); }
inline bool semaphore_give(semaphore_handle_t handle) noexcept { return platform::semaphore_give(handle); }
inline bool semaphore_take(semaphore_handle_t handle, duration_t timeout_us) noexcept { return platform::semaphore_take(handle, timeout_us); }

} // namespace emCore::os

#pragma once

// Centralized arena allocator for emCore (TLSF via eAlloc) — header-only
// Always static allocation: the TLSF heap uses the OS region of emCore's
// static runtime arena. No user flags required beyond having eAlloc available.
//
// Usage:
//   #include "emCore/memory/arena_allocator.hpp"
//   emCore::memory::arena::ensure_initialized();
//   auto* tlsf = emCore::memory::arena::get();
//   void* p = tlsf ? tlsf->malloc(256) : nullptr;
//   if (p) { tlsf->free(p); }
//
// Notes:
// - We do NOT override global new/delete.
// - Core subsystems remain ETL/fixed-capacity; use this only where dynamic memory
//   is explicitly intended.

#if !defined(FREERTOS) && !defined(ESP_PLATFORM) && !defined(ARDUINO) && \
    !defined(POSIX) && !defined(STM32_CMSIS_RTOS) && !defined(STM32_CMSIS_RTOS2) && \
    !defined(ZEPHYR) && !defined(THREADX) && !defined(MBED_OS) && \
    !defined(BAREMETAL) && !defined(EALLOC_PC_HOST)
  // Default for IDE/host analysis when no platform macro is provided
  #define EALLOC_PC_HOST 1
#endif

#if __has_include(<eAlloc.hpp>)
  #include <eAlloc.hpp>
#else
  #error "eAlloc not found. Add https://github.com/fahara02/eAlloc to includes/deps so <eAlloc.hpp> is available."
#endif

#include <cstddef>
#include "layout.hpp"
#include "../runtime.hpp"

namespace emCore::memory::arena {

namespace detail {
  inline dsa::eAlloc*& instance_ref() {
    static dsa::eAlloc* inst = nullptr; // NOLINT
    return inst;
  }
}

// Initialize TLSF over the OS region in the static arena. Idempotent.
inline bool ensure_initialized() noexcept {
  if (detail::instance_ref() != nullptr) { return true; }
  const auto& L = emCore::memory::kLayout;
  if (L.os.size == 0U) { return false; }

  auto* base = static_cast<unsigned char*>(emCore::runtime::os_region());
  const std::size_t bytes = static_cast<std::size_t>(L.os.size);

  static alignas(dsa::eAlloc) unsigned char s_ealloc_buf[sizeof(dsa::eAlloc)]; // NOLINT
  dsa::eAlloc* inst = new (s_ealloc_buf) dsa::eAlloc(base, bytes);
  detail::instance_ref() = inst;
  return true;
}

inline dsa::eAlloc* get() noexcept { return detail::instance_ref(); }

} // namespace emCore::memory::arena

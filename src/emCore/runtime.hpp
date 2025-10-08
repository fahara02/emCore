#pragma once

// emCore central runtime arena and region accessors.
// Header-only interface; storage defined in runtime.cpp.
// No dynamic allocation, no RTTI.

#include <cstddef>

#include "memory/budget.hpp"
#include "memory/layout.hpp"

namespace emCore::runtime {

// Extern storage for the central arena (defined in runtime.cpp)
extern unsigned char g_arena[emCore::memory::required_bytes]; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Sizes and regions
constexpr std::size_t arena_size() noexcept { return emCore::memory::required_bytes; }
constexpr emCore::memory::layout layout() noexcept { return emCore::memory::kLayout; }
constexpr emCore::memory::budget_report budget() noexcept { return emCore::memory::report(); }

// Region base pointers (for placement-new of heavy singletons if desired)
inline void* messaging_region() noexcept { return static_cast<void*>(&g_arena[emCore::memory::kLayout.messaging.offset]); }
inline void* events_region()    noexcept { return static_cast<void*>(&g_arena[emCore::memory::kLayout.events.offset]); }
inline void* tasks_region()     noexcept { return static_cast<void*>(&g_arena[emCore::memory::kLayout.tasks.offset]); }
inline void* os_region()        noexcept { return static_cast<void*>(&g_arena[emCore::memory::kLayout.os.offset]); }
inline void* protocol_region()  noexcept { return static_cast<void*>(&g_arena[emCore::memory::kLayout.protocol.offset]); }
inline void* diagnostics_region() noexcept { return static_cast<void*>(&g_arena[emCore::memory::kLayout.diagnostics.offset]); }

} // namespace emCore::runtime

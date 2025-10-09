#pragma once

// Runtime singletons and helpers for the universal event system
// - Header-only, ETL-only, no RTTI/dynamic allocation

#include "event.hpp"
#include "event_bus.hpp"
#include "event_types.hpp"
#include "events_global.hpp"

#if defined(__has_include)
  #if __has_include(<emCore/generated/event_config.hpp>)
    #include <emCore/generated/event_config.hpp>
    #define EMCORE_EVENT_CONFIG_AVAILABLE 1
  #else
    #define EMCORE_EVENT_CONFIG_AVAILABLE 0
  #endif
#else
  #define EMCORE_EVENT_CONFIG_AVAILABLE 0
#endif

namespace emCore::events::runtime {

// Global event bus
inline event_bus& bus() noexcept {
    return ::emCore::events::global_event_bus();
}

// Post helpers
inline bool post(const Event& event) noexcept { return bus().post(event); }
inline bool post(category cat, code_t code, severity level = severity::info, flags flag_bits = flags::none) noexcept {
    return bus().post(cat, code, level, flag_bits);
}

#if EMCORE_EVENT_CONFIG_AVAILABLE
// Optional name-based posting if a generated catalog exists
// The generated header should define:
//   bool lookup_category(const char* name, category& out);
//   bool lookup_code(category cat, const char* name, code_t& out);
inline bool post_named(const char* cat_name, const char* code_name, // NOLINT(bugprone-easily-swappable-parameters)
                       severity lvl = severity::info, flags flag_bits = flags::none) noexcept {
    category cat{}; code_t code{};
    if (!::emCore::events::gen::lookup_category(cat_name, cat)) { return false; }
    if (!::emCore::events::gen::lookup_code(cat, code_name, code)) { return false; }
    return post(cat, code, lvl, flag_bits);
}
#else
inline bool post_named(const char* cat_name, const char* code_name, // NOLINT(bugprone-easily-swappable-parameters)
                       severity lvl = severity::info, flags flag_bits = flags::none) noexcept {
    (void)cat_name; (void)code_name; (void)lvl; (void)flag_bits;
    return false;
}
#endif

} // namespace emCore::events::runtime

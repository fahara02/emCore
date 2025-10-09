#pragma once

#include <new>
#include "event_bus.hpp"
#include "../memory/layout.hpp"
#include "../runtime.hpp"

namespace emCore::events {

using event_bus_t = event_bus;

inline event_bus_t& global_event_bus() noexcept {
#if EMCORE_ENABLE_EVENTS
    if constexpr (emCore::memory::kLayout.events.size > 0) {
        static bool constructed = false;
        void* base = emCore::runtime::events_region();
        auto* obj = static_cast<event_bus_t*>(base);
        if (!constructed) {
            static_assert(alignof(event_bus_t) <= 8, "event_bus alignment must be <= arena alignment");
            static_assert(sizeof(event_bus_t) <= emCore::memory::kLayout.events.size,
                          "event_bus exceeds events region size; adjust budget/layout");
            ::new (obj) event_bus_t();
            constructed = true;
            (void)obj->initialize();
        }
        return *obj;
    } else {
        static event_bus_t fallback;
        static bool inited = fallback.initialize(); (void)inited;
        return fallback;
    }
#else
    static event_bus_t fallback;
    static bool inited = fallback.initialize(); (void)inited;
    return fallback;
#endif
}

} // namespace emCore::events

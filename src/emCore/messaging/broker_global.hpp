#pragma once

#include <new>
#include "message_broker.hpp"
#include "message_types.hpp"
#include "../core/config.hpp"
#include "../memory/layout.hpp"
#include "../runtime.hpp"

namespace emCore::messaging {

// Medium-message broker, constructed inside the central arena messaging region (if enabled),
// otherwise falls back to a translation-unit static instance (no arena usage).
using medium_broker_t = message_broker<medium_message, config::max_tasks>;

#if EMCORE_ENABLE_MESSAGING
static_assert(::emCore::memory::kLayout.messaging.size >= sizeof(medium_broker_t),
              "emCore messaging region too small for medium_broker_t. Increase EMCORE_MEMORY_BUDGET_BYTES or lower messaging caps (tasks/queue capacity/topics/subs). Alternatively disable EMCORE_ENABLE_MESSAGING.");
#endif

inline medium_broker_t& global_medium_broker() noexcept {
#if EMCORE_ENABLE_MESSAGING
    if constexpr (emCore::memory::kLayout.messaging.size >= sizeof(medium_broker_t)) {
        static bool constructed = false;
        void* base = emCore::runtime::messaging_region();
        auto* obj = static_cast<medium_broker_t*>(base);
        if (!constructed) {
            static_assert(alignof(medium_broker_t) <= 8, "medium_broker_t alignment must be <= arena alignment");
            ::new (obj) medium_broker_t();
            constructed = true;
        }
        return *obj;
    } else {
        static medium_broker_t fallback;
        return fallback;
    }
#else
    static medium_broker_t fallback;
    return fallback;
#endif
}

} // namespace emCore::messaging

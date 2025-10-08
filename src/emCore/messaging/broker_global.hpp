#pragma once

#include <new>
#include "message_broker.hpp"
#include "message_types.hpp"
#include "../core/config.hpp"
#include "../memory/layout.hpp"
#include "../runtime.hpp"

namespace emCore::messaging {

// Medium-message broker, constructed inside the central arena messaging region.
using medium_broker_t = message_broker<medium_message, config::max_tasks>;

inline medium_broker_t& global_medium_broker() noexcept {
    static bool constructed = false;
    void* base = emCore::runtime::messaging_region();
    auto* obj = static_cast<medium_broker_t*>(base);
    if (!constructed) {
        static_assert(alignof(medium_broker_t) <= 8, "medium_broker_t alignment must be <= arena alignment");
        static_assert(sizeof(medium_broker_t) <= emCore::memory::kLayout.messaging.size,
                      "medium_broker_t exceeds messaging region size; adjust budget/layout");
        ::new (obj) medium_broker_t();
        constructed = true;
    }
    return *obj;
}

} // namespace emCore::messaging

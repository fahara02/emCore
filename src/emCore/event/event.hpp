#pragma once

#include "../core/types.hpp"
#include "event_types.hpp"
#include <etl/variant.h>
#include <etl/array.h>
#include <etl/monostate.h>

namespace emCore::events {

// User payload types should be small, trivially copyable, and optionally tag-derive from IEvent.
// We provide a universal payload variant that covers common embedded use-cases without RTTI.
using payload_t = etl::variant<
    etl::monostate,
    i32,
    u32,
    f32,
    bool,
    string32,
    etl::array<u8, 16>,
    etl::array<u8, 64>
>;

struct Event {
    id ident{};            // category + code
    severity level{severity::info};
    flags attr{flags::none};
    timestamp_t ts{0};
    payload_t data;

    Event() = default;
    static Event make(category cat, code_t code, severity lvl = severity::info, flags flag_bits = flags::none) noexcept {
        Event evt; evt.ident = id{cat, code}; evt.level = lvl; evt.attr = flag_bits; evt.ts = 0; return evt;
    }
};

} // namespace emCore::events

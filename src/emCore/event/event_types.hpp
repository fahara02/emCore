#pragma once

#include "../core/types.hpp"

namespace emCore::events {

// Broad event categories used across systems and state machines
enum class category : u8 {
    any            = 0xFF,
    system         = 0,
    task           = 1,
    messaging      = 2,
    protocol       = 3,
    io             = 4,
    sensor         = 5,
    network        = 6,
    storage        = 7,
    security       = 8,
    power          = 9,
    timer          = 10,
    statemachine   = 11,
    user           = 12,
    custom         = 13,
};

// Severity levels for events
enum class severity : u8 {
    trace = 0,
    debug = 1,
    info  = 2,
    warn  = 3,
    error = 4,
    critical = 5,
};

// Event flags (bitmask)
enum class flags : u8 {
    none         = 0x00,
    sticky       = 0x01,   // keep last occurrence available
    high_priority= 0x02,   // prioritize delivery
    throttled    = 0x04,   // subject to rate limiting
    aggregated   = 0x08,   // represents aggregated data
};

inline flags operator|(flags lhs, flags rhs) noexcept {
    return static_cast<flags>(static_cast<u8>(lhs) | static_cast<u8>(rhs));
}
inline flags operator&(flags lhs, flags rhs) noexcept {
    return static_cast<flags>(static_cast<u8>(lhs) & static_cast<u8>(rhs));
}
inline bool has_flag(flags value, flags check) noexcept { return (value & check) == check; }

// Event code space (per category)
using code_t = u16;

struct id {
    category cat{category::system};
    code_t code{0};
};

} // namespace emCore::events

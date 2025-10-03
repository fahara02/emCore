#pragma once

#include <emCore/core/types.hpp>

namespace emCore::protocol {

// Compute Fletcher-16 checksum (RFC 1146 variant for 8-bit data)
// Returns 16-bit checksum: high byte first in typical wire format
constexpr u16 fletcher16(const u8* data, size_t len) noexcept {
    u32 sum1 = 0;
    u32 sum2 = 0;
    for (size_t i = 0; i < len; ++i) {
        sum1 = (sum1 + data[i]) % 255U;
        sum2 = (sum2 + sum1) % 255U;
    }
    return static_cast<u16>((sum2 << 8) | sum1);
}

// Incremental Fletcher-16 accumulator for streaming scenarios
struct fletcher16_accum {
    u32 sum1{0};
    u32 sum2{0};
    inline void reset() noexcept { sum1 = 0; sum2 = 0; }
    inline void add(u8 b) noexcept {
        sum1 = (sum1 + b) % 255U;
        sum2 = (sum2 + sum1) % 255U;
    }
    inline u16 value() const noexcept { return static_cast<u16>((sum2 << 8) | sum1); }
};

} // namespace emCore::protocol

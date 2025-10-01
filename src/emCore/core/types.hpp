#pragma once

#include <cstddef>
#include <cstdint>

#include "etl_compat.hpp"
#include <etl/optional.h>
#include <etl/string.h>

namespace emCore {

// Basic integer types
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

// Floating point types
using f32 = float;
using f64 = double;

// String types (fixed size, no dynamic allocation)
template<size_t N>
using string = etl::string<N>;

using string32 = etl::string<32>;
using string64 = etl::string<64>;
using string128 = etl::string<128>;

// Optional types
template<typename T>
using optional = etl::optional<T>;

// Time types (milliseconds since boot)
using timestamp_t = u64;
using duration_t = u32;

// Priority levels
enum class priority : u8 {
    idle = 0,
    low = 1,
    normal = 2,
    high = 3,
    critical = 4
};

}  // namespace emCore

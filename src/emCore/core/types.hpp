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

// Time types (microseconds for precision)
using timestamp_t = u64;
using duration_t = u32;

/* Task ID type */
using task_id_t = u16;
constexpr task_id_t invalid_task_id = 0xFFFF;

/* Topic ID strong type to avoid parameter confusion */
struct topic_id_t {
    u16 value;
    
    constexpr explicit topic_id_t(u16 val) noexcept : value(val) {}
    constexpr explicit operator u16() const noexcept { return value; }
    
    constexpr bool operator==(topic_id_t other) const noexcept { return value == other.value; }
    constexpr bool operator!=(topic_id_t other) const noexcept { return value != other.value; }
};

/* Task priority levels */
enum class priority : u8 {
    idle = 0,
    low = 1,
    normal = 2,
    high = 3,
    critical = 4
};

/* Timeout strong type to avoid parameter confusion */
struct timeout_ms_t {
    u32 value;
    
    constexpr explicit timeout_ms_t(u32 val) noexcept : value(val) {}
    constexpr explicit operator u32() const noexcept { return value; }
    
    constexpr bool operator==(timeout_ms_t other) const noexcept { return value == other.value; }
    constexpr bool operator!=(timeout_ms_t other) const noexcept { return value != other.value; }
    
    static constexpr timeout_ms_t infinite() noexcept { return timeout_ms_t(0xFFFFFFFF); }
};

}  // namespace emCore

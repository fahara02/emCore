#pragma once

#include <cstddef>

#include "../core/types.hpp"
#include "../platform/platform.hpp"
#include <etl/array.h>


namespace emCore::messaging {

/* Message priority levels */
enum class message_priority : u8 {
    low = 0,
    normal = 1,
    high = 2,
    critical = 3
};

/* Message flags */
enum class message_flags : u8 {
    none = 0x00,
    requires_ack = 0x01,      /* Sender expects acknowledgment */
    broadcast = 0x02,          /* Broadcast to all subscribers */
    urgent = 0x04,             /* Skip queue, deliver immediately */
    persistent = 0x08          /* Retry on failure */
};

inline message_flags operator|(message_flags lhs, message_flags rhs) noexcept {
    return static_cast<message_flags>(static_cast<u8>(lhs) | static_cast<u8>(rhs));
}

inline message_flags operator&(message_flags lhs, message_flags rhs) noexcept {
    return static_cast<message_flags>(static_cast<u8>(lhs) & static_cast<u8>(rhs));
}

inline bool has_flag(message_flags flags, message_flags check) noexcept {
    return (flags & check) == check;
}

/* Message header - fixed size, always present */
struct message_header {
    u16 type;                       /* Message type ID */
    u16 sender_id;                  /* Sender task ID */
    u16 receiver_id;                /* Receiver task ID (0xFFFF = broadcast) */
    u8 priority;                    /* Message priority */
    u8 flags;                       /* Message flags */
    timestamp_t timestamp;          /* Message creation timestamp */
    u16 payload_size;               /* Actual payload size in bytes */
    u16 sequence_number;            /* For ordering/acknowledgment */
};

/* Maximum payload sizes (overridable via build flags) */
#ifdef EMCORE_SMALL_PAYLOAD_SIZE
constexpr size_t small_payload_size = EMCORE_SMALL_PAYLOAD_SIZE;
#else
constexpr size_t small_payload_size = 16;   /* Inline small messages */
#endif

#ifdef EMCORE_MEDIUM_PAYLOAD_SIZE
constexpr size_t medium_payload_size = EMCORE_MEDIUM_PAYLOAD_SIZE;
#else
constexpr size_t medium_payload_size = 64;  /* Most common size */
#endif

#ifdef EMCORE_LARGE_PAYLOAD_SIZE
constexpr size_t large_payload_size = EMCORE_LARGE_PAYLOAD_SIZE;
#else
constexpr size_t large_payload_size = 256;  /* Large messages */
#endif

template<size_t MaxPayloadSize = medium_payload_size>
struct message_envelope {
    message_header header{};
    alignas(4) u8 payload[MaxPayloadSize]{};  // 4-byte alignment for safe MCU access
    
    message_envelope() = default;
    
    /* Check if message has specific flag */
    [[nodiscard]] bool has_flag(message_flags flag) const noexcept {
        return (static_cast<message_flags>(header.flags) & flag) == flag;
    }
};

/* Common message envelope types */
using small_message = message_envelope<small_payload_size>;
using medium_message = message_envelope<medium_payload_size>;
using large_message = message_envelope<large_payload_size>;

/* Message acknowledgment */
struct message_ack {
    u16 sequence_number;
    u16 sender_id;
    bool success;
    u8 error_code;
};

}  // namespace emCore::messaging



#pragma once

#include <emCore/protocol/packet_parser.hpp>

namespace emCore::protocol::gen {

// Default/stub packet configuration
// This is overridden by userspace generated_packet_config.hpp when available

inline constexpr u8 PACKET_SYNC[2] = { 0x55, 0xAA };
inline constexpr bool PACKET_LENGTH_16BIT = true;
inline constexpr size_t PACKET_MAX_PAYLOAD = 128;
inline constexpr size_t PACKET_SYNC_LEN = 2;

enum class opcode : u8 {
    NOP = 0x00,
    BOOT_EXIT = 0x01,
    ERASE_DEVICE = 0x02,
    PROGRAM_DEVICE = 0x03,
    QUERY_DEVICE = 0x04,
};

using PacketT = packet<PACKET_MAX_PAYLOAD>;
using ParserT = packet_parser<PACKET_MAX_PAYLOAD, PACKET_SYNC_LEN, PACKET_LENGTH_16BIT, PACKET_SYNC>;
template <size_t MaxHandlers>
using DispatcherT = command_dispatcher<MaxHandlers, PacketT>;

} // namespace emCore::protocol::gen

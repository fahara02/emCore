#pragma once

#include <emCore/protocol/packet_parser.hpp>

#ifndef EMCORE_GENERATED_PACKET_CONFIG_HPP
#define EMCORE_GENERATED_PACKET_CONFIG_HPP

namespace emCore::protocol::gen {

inline constexpr u8 PACKET_SYNC[2] = { 0x55, 0xAA };
inline constexpr bool PACKET_LENGTH_16BIT = true;
inline constexpr size_t PACKET_MAX_PAYLOAD = 64;
inline constexpr size_t PACKET_SYNC_LEN = 2;
inline constexpr size_t OPCODE_SPACE = 5;

// Provide a type for encoder/decoder template configuration
struct packet_config {
    static constexpr size_t PACKET_SYNC_LEN = 2;
    static constexpr bool PACKET_LENGTH_16BIT = true;
    static inline constexpr u8 PACKET_SYNC[PACKET_SYNC_LEN] = { 0x55, 0xAA };
};

enum class opcode : u8 {
    BOOT_EXIT = 0x01,
    ERASE_DEVICE = 0x02,
    PROGRAM_DEVICE = 0x03,
    QUERY_DEVICE = 0x04,
};

using PacketT = packet<PACKET_MAX_PAYLOAD>;
using ParserT = packet_parser<PACKET_MAX_PAYLOAD, PACKET_SYNC_LEN, PACKET_LENGTH_16BIT, PACKET_SYNC>;
template <size_t MaxHandlers>
using DispatcherT = command_dispatcher<MaxHandlers, PacketT>;

static_assert(OPCODE_SPACE >= 5, "OPCODE_SPACE must be >= max(opcodes)+1");

} // namespace emCore::protocol::gen

#endif // EMCORE_GENERATED_PACKET_CONFIG_HPP

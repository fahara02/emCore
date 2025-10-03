#pragma once

// Packet pipeline glue: ring buffer -> parser -> dispatcher
// - Peripheral-agnostic: feed bytes into ring via ISR/driver
// - OS-agnostic: call process_available() from any context/task
// - Header-only; no RTTI, no dynamic allocation

#include <emCore/core/types.hpp>
#include <etl/circular_buffer.h> // not used here but kept for future; current ring is custom
#include <emCore/protocol/packet_parser.hpp>
#include <emCore/protocol/byte_ring.hpp>

namespace emCore::protocol {

// Generic pipeline that connects a byte ring with a packet parser and a dispatcher.
// Template parameters:
//  - RingT: provides push/pop/pop_n/empty
//  - ParserT: provides decode(u8), has_packet(), get_packet(PacketT&)
//  - DispatcherT: provides dispatch(const PacketT&)
//  - PacketT: packet type matching ParserT

template <typename RingT, typename ParserT, typename DispatcherT, typename PacketT>
class packet_pipeline {
public:
    packet_pipeline(RingT& ring, ParserT& parser, DispatcherT& dispatcher) noexcept
        : ring_(ring), parser_(parser), dispatcher_(dispatcher) {}

    // Feed a byte directly to ring (driver-friendly). Returns true if stored.
    bool feed_byte(u8 byte) noexcept { return ring_.push(byte); }

    // Feed a buffer to ring. Returns number of bytes stored.
    size_t feed_bytes(const u8* data, size_t len) noexcept { return ring_.push_n(data, len); }

    // Process as many bytes as available; dispatch packets as they complete.
    // Returns number of packets dispatched.
    size_t process_available(size_t max_packets = static_cast<size_t>(-1)) noexcept {
        size_t packets = 0;
        u8 byte = 0;
        while (packets < max_packets) {
            if (!ring_.pop(byte)) { break; }
            const bool done = parser_.decode(byte);
            if (done && parser_.has_packet()) {
                PacketT pkt{};
                if (parser_.get_packet(pkt)) {
                    dispatcher_.dispatch(pkt);
                    ++packets;
                }
            }
        }
        return packets;
    }

    // Drain at most max_bytes; useful for time-slicing
    size_t process_bytes(size_t max_bytes, size_t& packets_out) noexcept {
        size_t processed = 0;
        packets_out = 0;
        u8 byte = 0;
        while (processed < max_bytes && ring_.pop(byte)) {
            ++processed;
            const bool done = parser_.decode(byte);
            if (done && parser_.has_packet()) {
                PacketT pkt{};
                if (parser_.get_packet(pkt)) {
                    dispatcher_.dispatch(pkt);
                    ++packets_out;
                }
            }
        }
        return processed;
    }

private:
    RingT& ring_;
    ParserT& parser_;
    DispatcherT& dispatcher_;
};

} // namespace emCore::protocol

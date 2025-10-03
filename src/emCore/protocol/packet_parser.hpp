#pragma once

/*
 * emCore Packet Parsing Architecture
 * - Peripheral-independent: feed bytes via decode(byte)
 * - OS-agnostic: no OS primitives required
 * - Scalable: templated MaxPayload and Sync length; configurable length field size
 */

#include <emCore/core/types.hpp>
#include <etl/array.h>
#include <emCore/protocol/fletcher16.hpp>
#include <emCore/protocol/command_dispatcher.hpp>
#include <cstddef>

namespace emCore::protocol {

// Parser error codes
enum class parser_error : u8 {
    none = 0,
    boundary_error,
    length_overflow,
    checksum_mismatch,
};

// Packet receive states (table-driven)
enum class PacketRxState : u8 {
    SYNC = 0,
    OP_CODE,
    DATA_LENGTH,
    DATA,
    CHECKSUM,
    PACKET_STATE_END
};

// Field decoder and encoder are now in separate headers
// #include <emCore/protocol/decoder.hpp>
// #include <emCore/protocol/encoder.hpp>

// A parsed packet (wire format: SYNC[SyncLen] | OP | LEN(1 or 2) | DATA | CHKSUM(2))
template <size_t MaxPayload>
struct packet {
    u8 opcode{0};
    u16 length{0};
    etl::array<u8, MaxPayload> data{}; // valid bytes: [0..length)
    u16 checksum_rx{0};               // checksum received in stream
};


/*
 * Packet parser (table-driven FSM)
 * Template parameters:
 *  - MaxPayload: maximum payload size (at compile-time)
 *  - SyncLen: number of sync bytes in header
 *  - Length16Bit: if true, uses 2-byte big-endian length; else 1 byte
 *  - SyncPattern: compile-time sync pattern array reference
 */

template <size_t MaxPayload,
          size_t SyncLen,
          bool Length16Bit,
          const u8 (&SyncPattern)[SyncLen]>
class packet_parser {
public:
    using packet_t = packet<MaxPayload>;

    packet_parser() = default;

    // Reset parser to initial state
    void reset() noexcept {
        state_ = PacketRxState::SYNC;
        sync_index_ = 0;
        pkt_.length = 0;
        data_index_ = 0;
        pkt_.checksum_rx = 0;
        acc_.reset();
        error_ = parser_error::none;
        packet_ready_ = false;
    }

    // Feed one byte; returns true if a complete, validated packet just became available
    bool decode(u8 data) noexcept {
        if (state_ >= PacketRxState::PACKET_STATE_END) {
            // boundary error: invalid state, reset
            reset();
            error_ = parser_error::boundary_error;
            return false;
        }
        return (this->*table_[static_cast<u8>(state_)])(data);
    }

    // Check if a packet is ready after decode() returned true
    [[nodiscard]]  bool has_packet() const noexcept { return packet_ready_; }

    // Copy out the packet and clear ready flag
    bool get_packet(packet_t& out) noexcept {
        if (!packet_ready_) { return false; }
        out = pkt_;
        packet_ready_ = false;
        return true;
    }

    [[nodiscard]]  parser_error last_error() const noexcept { return error_; }

private:
    // State handlers (return true on packet completion)
    bool on_sync(u8 b) noexcept {
        // Match sync pattern with partial overlap support
        if (b == SyncPattern[sync_index_]) {
            ++sync_index_;
            if (sync_index_ == SyncLen) {
                // Full sync matched; move to opcode
                state_ = PacketRxState::OP_CODE;
                acc_.reset();
                sync_index_ = 0; // ready for next sync after packet
            }
        } else {
            // If mismatch but equals first byte, keep 1; else reset to 0
            sync_index_ = (b == SyncPattern[0]) ? 1U : 0U;
        }
        return false;
    }

    bool on_opcode(u8 b) noexcept {
        pkt_.opcode = b;
        acc_.add(b);
        state_ = PacketRxState::DATA_LENGTH;
        len_bytes_read_ = 0;
        pkt_.length = 0;
        return false;
    }

    bool on_length(u8 b) noexcept {
        if constexpr (Length16Bit) {
            // big-endian length
            if (len_bytes_read_ == 0) {
                pkt_.length = static_cast<u16>(b) << 8;
                acc_.add(b);
                len_bytes_read_ = 1;
            } else {
                pkt_.length |= b;
                acc_.add(b);
                // validate length
                if (pkt_.length > MaxPayload) { reset(); error_ = parser_error::length_overflow; return false; }
                if (pkt_.length == 0) { // allow empty payload
                    state_ = PacketRxState::CHECKSUM;
                    chksum_bytes_read_ = 0;
                } else {
                    state_ = PacketRxState::DATA;
                    data_index_ = 0;
                }
            }
        } else {
            pkt_.length = b;
            acc_.add(b);
            if (pkt_.length > MaxPayload) { reset(); error_ = parser_error::length_overflow; return false; }
            state_ = (pkt_.length == 0) ? PacketRxState::CHECKSUM : PacketRxState::DATA;
            if (state_ == PacketRxState::CHECKSUM) { chksum_bytes_read_ = 0; }
            else { data_index_ = 0; }
        }
        return false;
    }

    bool on_data(u8 b) noexcept {
        // store and accumulate
        pkt_.data[data_index_] = b;
        acc_.add(b);
        ++data_index_;
        if (data_index_ >= pkt_.length) {
            // move to checksum
            state_ = PacketRxState::CHECKSUM;
            chksum_bytes_read_ = 0;
        }
        return false;
    }

    bool on_checksum(u8 b) noexcept {
        // two bytes, big-endian
        if (chksum_bytes_read_ == 0) {
            pkt_.checksum_rx = static_cast<u16>(b) << 8;
            chksum_bytes_read_ = 1;
        } else {
            pkt_.checksum_rx |= b;
            // validate
            const u16 calc = acc_.value();
            if (calc == pkt_.checksum_rx) {
                packet_ready_ = true;
                // next packet
                state_ = PacketRxState::SYNC;
                acc_.reset();
                data_index_ = 0;
                error_ = parser_error::none;
                return true;
            } else {
                reset();
                error_ = parser_error::checksum_mismatch;
            }
        }
        return false;
    }

    // FSM table
    using state_fn_t = bool (packet_parser::*)(u8);
    static constexpr etl::array<state_fn_t, static_cast<size_t>(PacketRxState::PACKET_STATE_END)> table_ = {
        &packet_parser::on_sync,
        &packet_parser::on_opcode,
        &packet_parser::on_length,
        &packet_parser::on_data,
        &packet_parser::on_checksum,
    };

private:
    // State
    PacketRxState state_{PacketRxState::SYNC};
    parser_error error_{parser_error::none};
    bool packet_ready_{false};

    // Working vars
    u16 data_index_{0};
    u8 len_bytes_read_{0};
    u8 chksum_bytes_read_{0};
    u8 sync_index_{0};

    // Accumulator and current packet
    fletcher16_accum acc_{};
    packet_t pkt_{};
};





} // namespace emCore::protocol

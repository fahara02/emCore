#pragma once
// Header-only runtime accessors for the packet pipeline singletons.
// Keeps the system peripheral-agnostic and OS-agnostic.
// No dynamic allocation, no RTTI. ETL only.

#include <cstddef>
#include <emCore/protocol/packet_parser.hpp>
#include <emCore/protocol/packet_pipeline.hpp>
#include <emCore/protocol/byte_ring.hpp>
#include <emCore/protocol/decoder.hpp>
#include <emCore/protocol/encoder.hpp>
#include <emCore/protocol/command_dispatcher.hpp>
#if EMCORE_ENABLE_PROTOCOL
#include <emCore/protocol/protocol_global.hpp>
#endif

// Include generated packet configuration if available
#if __has_include(<generated_packet_config.hpp>)
#  include <generated_packet_config.hpp>
#elif __has_include(<emCore/generated/packet_config.hpp>)
#  include <emCore/generated/packet_config.hpp>
#else
#error "Generated packet configuration not found. Run scripts/generate_packet_config.py"
#endif

// -------- Configurable capacities (compile-time) --------
#ifndef EMCORE_PROTOCOL_PACKET_SIZE
#define EMCORE_PROTOCOL_PACKET_SIZE 128
#endif
#ifndef EMCORE_PROTOCOL_MAX_HANDLERS
#define EMCORE_PROTOCOL_MAX_HANDLERS 16
#endif
#ifndef EMCORE_PROTOCOL_RING_SIZE
#define EMCORE_PROTOCOL_RING_SIZE 512
#endif

namespace emCore::protocol::runtime {

namespace gencfg = ::emCore::protocol::gen;

// Prefer generated types from packet_config; sizes can still be overridden by macros
using PacketT = gencfg::PacketT;
using ParserT = gencfg::ParserT;
using DispatcherT = emCore::protocol::command_dispatcher<EMCORE_PROTOCOL_MAX_HANDLERS, PacketT>;
using FieldDecoderT = emCore::protocol::field_decoder<16, gencfg::OPCODE_SPACE>; // Max 16 fields per command
using FieldEncoderT = emCore::protocol::field_encoder<16, gencfg::OPCODE_SPACE>; // Max 16 fields per command
using RingT = emCore::protocol::byte_ring<EMCORE_PROTOCOL_RING_SIZE>;
using PipelineT = emCore::protocol::packet_pipeline<RingT, ParserT, DispatcherT, PacketT>;

#if EMCORE_ENABLE_PROTOCOL
inline RingT&           get_ring()          noexcept { return ::emCore::protocol::global::global_ring(); }
inline ParserT&         get_parser()        noexcept { return ::emCore::protocol::global::global_parser(); }
inline DispatcherT&     get_dispatcher()    noexcept { return ::emCore::protocol::global::global_dispatcher(); }
inline FieldDecoderT&   get_field_decoder() noexcept { return ::emCore::protocol::global::global_field_decoder(); }
inline FieldEncoderT&   get_field_encoder() noexcept { return ::emCore::protocol::global::global_field_encoder(); }
inline PipelineT&       get_pipeline()      noexcept { return ::emCore::protocol::global::global_pipeline(); }
#else
// Fallback internal statics when protocol is disabled (no central region used)
inline RingT&           get_ring()          noexcept { static RingT r; return r; }
inline ParserT&         get_parser()        noexcept { static ParserT p; return p; }
inline DispatcherT&     get_dispatcher()    noexcept { static DispatcherT d; return d; }
inline FieldDecoderT&   get_field_decoder() noexcept { static FieldDecoderT fd; return fd; }
inline FieldEncoderT&   get_field_encoder() noexcept { static FieldEncoderT fe; return fe; }
inline PipelineT&       get_pipeline()      noexcept { static PipelineT pl(get_ring(), get_parser(), get_dispatcher()); return pl; }
#endif

// Convenience processing helpers
inline size_t process_available(size_t max_packets = static_cast<size_t>(-1)) noexcept {
    return get_pipeline().process_available(max_packets);
}

inline size_t process_bytes(size_t max_bytes, size_t& packets_out) noexcept {
    return get_pipeline().process_bytes(max_bytes, packets_out);
}

// Driver-friendly helpers to feed data into the pipeline
inline bool feed_byte(u8 byte) noexcept {
    return get_pipeline().feed_byte(byte);
}

inline size_t feed_bytes(const u8* data, size_t len) noexcept {
    return get_pipeline().feed_bytes(data, len);
}

} // namespace emCore::protocol::runtime

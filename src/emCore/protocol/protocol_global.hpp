#pragma once

#include <new>
#include <cstddef>
#include "../memory/layout.hpp"
#include "../runtime.hpp"
#include <emCore/protocol/packet_parser.hpp>
#include <emCore/protocol/packet_pipeline.hpp>
#include <emCore/protocol/byte_ring.hpp>
#include <emCore/protocol/decoder.hpp>
#include <emCore/protocol/encoder.hpp>
#include <emCore/protocol/command_dispatcher.hpp>

#if __has_include(<generated_packet_config.hpp>)
#  include <generated_packet_config.hpp>
#elif __has_include(<emCore/generated/packet_config.hpp>)
#  include <emCore/generated/packet_config.hpp>
#else
#  error "Generated packet configuration not found. Run scripts/generate_packet_config.py"
#endif

#ifndef EMCORE_PROTOCOL_PACKET_SIZE
#define EMCORE_PROTOCOL_PACKET_SIZE 128
#endif
#ifndef EMCORE_PROTOCOL_MAX_HANDLERS
#define EMCORE_PROTOCOL_MAX_HANDLERS 16
#endif
#ifndef EMCORE_PROTOCOL_RING_SIZE
#define EMCORE_PROTOCOL_RING_SIZE 512
#endif

namespace emCore::protocol::global {

namespace gencfg = ::emCore::protocol::gen;

using PacketT = gencfg::PacketT;
using ParserT = gencfg::ParserT;
using DispatcherT = emCore::protocol::command_dispatcher<EMCORE_PROTOCOL_MAX_HANDLERS, PacketT>;
using FieldDecoderT = emCore::protocol::field_decoder<16, gencfg::OPCODE_SPACE>;
using FieldEncoderT = emCore::protocol::field_encoder<16, gencfg::OPCODE_SPACE>;
using RingT = emCore::protocol::byte_ring<EMCORE_PROTOCOL_RING_SIZE>;
using PipelineT = emCore::protocol::packet_pipeline<RingT, ParserT, DispatcherT, PacketT>;

struct ProtocolBlock {
    RingT ring;
    ParserT parser;
    DispatcherT dispatcher;
    FieldDecoderT decoder;
    FieldEncoderT encoder;
    PipelineT pipeline;

    ProtocolBlock() : ring(), parser(), dispatcher(), decoder(), encoder(), pipeline(ring, parser, dispatcher) {}
    ProtocolBlock(const ProtocolBlock&) = delete;
    ProtocolBlock& operator=(const ProtocolBlock&) = delete;
    ProtocolBlock(ProtocolBlock&&) = delete;
    ProtocolBlock& operator=(ProtocolBlock&&) = delete;
};

#if EMCORE_ENABLE_PROTOCOL
static_assert(::emCore::memory::kLayout.protocol.size >= sizeof(ProtocolBlock),
              "emCore protocol region too small for ProtocolBlock. Define EMCORE_PROTOCOL_MEM_BYTES to a sufficient value or lower EMCORE_PROTOCOL_* sizes / disable EMCORE_ENABLE_PROTOCOL.");
#endif

inline ProtocolBlock& block() noexcept {
#if EMCORE_ENABLE_PROTOCOL
    if constexpr (emCore::memory::kLayout.protocol.size >= sizeof(ProtocolBlock)) {
        static bool constructed = false;
        void* base = emCore::runtime::protocol_region();
        auto* obj = static_cast<ProtocolBlock*>(base);
        if (!constructed) {
            static_assert(alignof(ProtocolBlock) <= 8, "ProtocolBlock alignment must be <= arena alignment");
            ::new (obj) ProtocolBlock();
            constructed = true;
        }
        return *obj;
    } else {
        static ProtocolBlock fallback;
        return fallback;
    }
#else
    static ProtocolBlock fallback;
    return fallback;
#endif
}

inline RingT&       global_ring()       noexcept { return block().ring; }
inline ParserT&     global_parser()     noexcept { return block().parser; }
inline DispatcherT& global_dispatcher() noexcept { return block().dispatcher; }
inline FieldDecoderT& global_field_decoder() noexcept { return block().decoder; }
inline FieldEncoderT& global_field_encoder() noexcept { return block().encoder; }
inline PipelineT&   global_pipeline()   noexcept { return block().pipeline; }

} // namespace emCore::protocol::global

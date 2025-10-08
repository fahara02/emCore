// emCore Field Encoder - Automatic structured data serialization to packets
// - State machine driven field encoding
// - Type-safe struct serialization with offsetof()
// - Automatic endianness conversion (host → big-endian wire)
// - No RTTI, no dynamic allocation; header-only; ETL

#pragma once

#include <emCore/core/types.hpp>
#include <etl/array.h>
#include <cstddef>
#include "decoder.hpp" // reuse FieldType and field_def

namespace emCore::protocol {

// Encoder state machine states
enum class encode_state : u8 {
    ENCODE_SYNC = 0,
    ENCODE_OPCODE,
    ENCODE_LENGTH_HIGH,
    ENCODE_LENGTH_LOW,
    ENCODE_PAYLOAD,
    ENCODE_CHECKSUM_HIGH,
    ENCODE_CHECKSUM_LOW,
    ENCODE_COMPLETE
};

// Field encoder state machine for automatic structured data serialization
// MaxFields defines max fields per opcode layout
template <size_t MaxFields, size_t OpcodeSpace = 256>
class field_encoder {
public:
    field_encoder() = default;
    field_encoder(const field_encoder&) = delete;
    field_encoder& operator=(const field_encoder&) = delete;
    field_encoder(field_encoder&&) = delete;
    field_encoder& operator=(field_encoder&&) = delete;

    // Register field layout for an opcode (same shape as decoder)
    bool set_field_layout(u8 opcode, const field_def* fields, size_t field_count) noexcept {
        if (field_count > MaxFields) { return false; }
        auto& layout_entry = layouts_[opcode];
        layout_entry.field_count = field_count;
        for (size_t i = 0; i < field_count; ++i) {
            layout_entry.fields[i] = field_desc{fields[i].type, fields[i].offset};
        }
        return true;
    }

    // Synchronous encode helper using PacketConfig (must provide PACKET_SYNC, PACKET_SYNC_LEN, PACKET_LENGTH_16BIT)
    template <typename PacketConfig, typename OutputFunc>
    bool encode_command(u8 opcode, const void* source_struct, OutputFunc out) noexcept {
        const auto& layout = layouts_[opcode];
        if (layout.field_count == 0) { return false; }

        // Running Fletcher16 accumulator (sum1/sum2) without dynamic state
        auto chk_reset = [&]() { sum1_ = 0; sum2_ = 0; };
        auto chk_add = [&](u8 byte_value) { sum1_ = (sum1_ + byte_value) % 255U; sum2_ = (sum2_ + sum1_) % 255U; };
        auto chk_value = [&]() -> u16 { return static_cast<u16>((sum2_ << 8) | sum1_); };
        // Ensure re-entrancy for stateless path
        chk_reset();

        // 1) Sync
        for (size_t i = 0; i < PacketConfig::PACKET_SYNC_LEN; ++i) {
            out(PacketConfig::PACKET_SYNC[i]);
        }
        // 2) Opcode
        out(opcode);
        chk_add(opcode);

        // 3) Length
        const u16 payload_len = calculate_payload_length(opcode, source_struct);
        if (PacketConfig::PACKET_LENGTH_16BIT) {
            out(static_cast<u8>(payload_len >> 8));
            out(static_cast<u8>(payload_len & 0xFF));
            chk_add(static_cast<u8>(payload_len >> 8));
            chk_add(static_cast<u8>(payload_len & 0xFF));
        } else {
            out(static_cast<u8>(payload_len));
            chk_add(static_cast<u8>(payload_len));
        }

        // 4) Payload (big-endian fields)
        const u8* src = static_cast<const u8*>(source_struct);
        for (size_t fi = 0; fi < layout.field_count; ++fi) {
            const auto& field = layout.fields[fi];
            const u8* field_ptr = src + field.offset;
            switch (field.type) {
                case FieldType::U8: {
                    u8 value8 = *field_ptr;
                    out(value8); chk_add(value8);
                    break;
                }
                case FieldType::U16: {
                    u16 value16 = *reinterpret_cast<const u16*>(field_ptr);
                    u8 high_byte = static_cast<u8>(value16 >> 8);
                    u8 low_byte = static_cast<u8>(value16 & 0xFF);
                    out(high_byte); chk_add(high_byte);
                    out(low_byte); chk_add(low_byte);
                    break;
                }
                case FieldType::U32: {
                    u32 value32 = *reinterpret_cast<const u32*>(field_ptr);
                    u8 byte_0 = static_cast<u8>(value32 >> 24);
                    u8 byte_1 = static_cast<u8>(value32 >> 16);
                    u8 byte_2 = static_cast<u8>(value32 >> 8);
                    u8 byte_3 = static_cast<u8>(value32 & 0xFF);
                    out(byte_0); chk_add(byte_0);
                    out(byte_1); chk_add(byte_1);
                    out(byte_2); chk_add(byte_2);
                    out(byte_3); chk_add(byte_3);
                    break;
                }
                case FieldType::U8_ARRAY: {
                    const u8* arr = *reinterpret_cast<const u8* const*>(field_ptr);
                    size_t len = *reinterpret_cast<const size_t*>(field_ptr + sizeof(const u8*));
                    for (size_t i = 0; i < len; ++i) {
                        out(arr[i]);
                        chk_add(arr[i]);
                    }
                    break;
                }
            }
        }

        // 5) Checksum (Fletcher16 high,low)
        const u16 checksum = chk_value();
        out(static_cast<u8>(checksum >> 8));
        out(static_cast<u8>(checksum & 0xFF));
        return true;
    }

    // Start stateful encoding (for streaming)
    bool start_encode(u8 opcode, const void* source_struct) noexcept {
        current_opcode_ = opcode;
        source_struct_ = source_struct;
        payload_length_ = calculate_payload_length(opcode, source_struct);
        state_ = encode_state::ENCODE_SYNC;
        sync_index_ = 0;
        field_index_ = 0;
        byte_index_ = 0;
        sum1_ = 0; sum2_ = 0;
        return true;
    }

    // Step the state machine, emitting one byte at a time via reference
    template <typename PacketConfig>
    bool encode_step(u8& out_byte) noexcept {
        switch (state_) {
            case encode_state::ENCODE_SYNC: {
                if (sync_index_ < PacketConfig::PACKET_SYNC_LEN) {
                    out_byte = PacketConfig::PACKET_SYNC[sync_index_++];
                    return true;
                }
                state_ = encode_state::ENCODE_OPCODE;
                [[fallthrough]];
            }
            case encode_state::ENCODE_OPCODE: {
                out_byte = current_opcode_;
                chk_add_inline(current_opcode_);
                state_ = PacketConfig::PACKET_LENGTH_16BIT ? encode_state::ENCODE_LENGTH_HIGH : encode_state::ENCODE_LENGTH_LOW;
                return true;
            }
            case encode_state::ENCODE_LENGTH_HIGH: {
                out_byte = static_cast<u8>(payload_length_ >> 8);
                chk_add_inline(out_byte);
                state_ = encode_state::ENCODE_LENGTH_LOW;
                return true;
            }
            case encode_state::ENCODE_LENGTH_LOW: {
                out_byte = static_cast<u8>(payload_length_ & 0xFF);
                chk_add_inline(out_byte);
                state_ = encode_state::ENCODE_PAYLOAD;
                field_index_ = 0; byte_index_ = 0;
                return true;
            }
            case encode_state::ENCODE_PAYLOAD: {
                if (encode_payload_step_inline(out_byte)) {
                    return true;
                }
                state_ = encode_state::ENCODE_CHECKSUM_HIGH;
                [[fallthrough]];
            }
            case encode_state::ENCODE_CHECKSUM_HIGH: {
                const u16 checksum = chk_value_inline();
                out_byte = static_cast<u8>(checksum >> 8);
                state_ = encode_state::ENCODE_CHECKSUM_LOW;
                return true;
            }
            case encode_state::ENCODE_CHECKSUM_LOW: {
                const u16 checksum = chk_value_inline();
                out_byte = static_cast<u8>(checksum & 0xFF);
                state_ = encode_state::ENCODE_COMPLETE;
                return true;
            }
            case encode_state::ENCODE_COMPLETE:
            default:
                return false;
        }
    }

private:
    // Compact field descriptor used at runtime (no debug name pointer)
    struct field_desc {
        FieldType type;
        size_t offset;
    };

    struct field_layout {
        etl::array<field_desc, MaxFields> fields{};
        size_t field_count{0};
    };

    etl::array<field_layout, OpcodeSpace> layouts_{}; // One layout per opcode

    // Running Fletcher16 (sum1/sum2) inline helpers for stateful path
    void chk_add_inline(u8 byte_value) noexcept { sum1_ = (sum1_ + byte_value) % 255U; sum2_ = (sum2_ + sum1_) % 255U; }
    u16  chk_value_inline() const noexcept { return static_cast<u16>((sum2_ << 8) | sum1_); }

    // Compute total payload length from field layout and source struct
    u16 calculate_payload_length(u8 opcode, const void* source_struct) const noexcept {
        const auto& layout = layouts_[opcode];
        const u8* src = static_cast<const u8*>(source_struct);
        u16 total = 0;
        for (size_t i = 0; i < layout.field_count; ++i) {
            const auto& f = layout.fields[i];
            switch (f.type) {
                case FieldType::U8: total += 1; break;
                case FieldType::U16: total += 2; break;
                case FieldType::U32: total += 4; break;
                case FieldType::U8_ARRAY: {
                    const size_t len = *reinterpret_cast<const size_t*>(src + f.offset + sizeof(const u8*));
                    total += static_cast<u16>(len);
                    break;
                }
            }
        }
        return total;
    }

    // Step-wise payload emission, updates checksum sums
    bool encode_payload_step_inline(u8& out_byte) noexcept {
        const auto& layout = layouts_[current_opcode_];
        if (field_index_ >= layout.field_count) { return false; }
        const auto& field = layout.fields[field_index_];
        const u8* src = static_cast<const u8*>(source_struct_);
        const u8* field_ptr = src + field.offset;
        switch (field.type) {
            case FieldType::U8: {
                out_byte = *field_ptr;
                chk_add_inline(out_byte);
                field_index_++;
                return true;
            }
            case FieldType::U16: {
                const u16 value16 = *reinterpret_cast<const u16*>(field_ptr);
                if (byte_index_ == 0) {
                    out_byte = static_cast<u8>(value16 >> 8);
                    byte_index_ = 1;
                } else {
                    out_byte = static_cast<u8>(value16 & 0xFF);
                    byte_index_ = 0;
                    field_index_++;
                }
                chk_add_inline(out_byte);
                return true;
            }
            case FieldType::U32: {
                const u32 value32 = *reinterpret_cast<const u32*>(field_ptr);
                switch (byte_index_) {
                    case 0: out_byte = static_cast<u8>(value32 >> 24); byte_index_ = 1; break;
                    case 1: out_byte = static_cast<u8>(value32 >> 16); byte_index_ = 2; break;
                    case 2: out_byte = static_cast<u8>(value32 >> 8);  byte_index_ = 3; break;
                    default: out_byte = static_cast<u8>(value32 & 0xFF); byte_index_ = 0; field_index_++; break;
                }
                chk_add_inline(out_byte);
                return true;
            }
            case FieldType::U8_ARRAY: {
                const u8* arr = *reinterpret_cast<const u8* const*>(field_ptr);
                const size_t len = *reinterpret_cast<const size_t*>(field_ptr + sizeof(const u8*));
                if (byte_index_ < len) {
                    out_byte = arr[byte_index_++];
                    chk_add_inline(out_byte);
                    return true;
                }
                byte_index_ = 0;
                field_index_++;
                return encode_payload_step_inline(out_byte);
            }
        }
        return false;
    }

    // Encoder state
    encode_state state_{encode_state::ENCODE_SYNC};
    u8 current_opcode_{0};
    u16 payload_length_{0};
    size_t sync_index_{0};
    size_t field_index_{0};
    size_t byte_index_{0};
    // Fletcher16 sums
    u32 sum1_{0};
    u32 sum2_{0};
    const void* source_struct_{nullptr};
};

} // namespace emCore::protocol

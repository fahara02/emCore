#pragma once

// emCore Field Decoder - Automatic structured data parsing from packets
// - Type-safe struct mapping with offsetof()
// - Automatic endianness conversion (big-endian wire → host)
// - No RTTI, no dynamic allocation; header-only; ETL only

#include <emCore/core/types.hpp>
#include <emCore/protocol/command_dispatcher.hpp>
#include <etl/array.h>
#include <cstddef>

// Forward declare packet template
namespace emCore::protocol {
    template <size_t MaxPayload> struct packet;
}

namespace emCore::protocol {

// Allow projects to configure opcode space size at compile time.
// Prefer the generated value if available, otherwise fallback to 256.
#ifndef EMCORE_PROTOCOL_OPCODE_SPACE
#  if defined(__has_include)
#    if __has_include(<generated_packet_config.hpp>)
#      include <generated_packet_config.hpp>
#      define EMCORE_PROTOCOL_OPCODE_SPACE ::emCore::protocol::gen::OPCODE_SPACE
#    else
#      define EMCORE_PROTOCOL_OPCODE_SPACE 256
#    endif
#  else
#    define EMCORE_PROTOCOL_OPCODE_SPACE 256
#  endif
#endif

// Field type definitions
enum class FieldType : u8 {
    U8 = 0,
    U16,
    U32,
    U8_ARRAY
};

// Field definition for structured decoding
struct field_def {
    FieldType type;
    size_t offset;      // Offset in target struct
    const char* name;   // Field name for debugging
};

// Field decoding states for structured data parsing
enum class FieldDecodeState : u8 {
    FIELD_START = 0,
    FIELD_U8,
    FIELD_U16_HIGH,
    FIELD_U16_LOW,
    FIELD_U32_BYTE0,
    FIELD_U32_BYTE1,
    FIELD_U32_BYTE2,
    FIELD_U32_BYTE3,
    FIELD_ARRAY,
    FIELD_COMPLETE,
    FIELD_STATE_END
};

// Field decoder state machine for automatic structured data parsing
template <size_t MaxFields, size_t OpcodeSpace = EMCORE_PROTOCOL_OPCODE_SPACE>
class field_decoder {
public:
    field_decoder() = default;
    
    // Set field definitions for a specific opcode
    bool set_field_layout(u8 opcode, const field_def* fields, size_t field_count) noexcept {
        if (field_count > MaxFields) {
            return false;
        }
        
        layouts_[opcode].field_count = field_count;
        for (size_t i = 0; i < field_count; ++i) {
            layouts_[opcode].fields[i] = fields[i];
        }
        return true;
    }
    
    // Decode packet data into structured format
    template<size_t MaxPayload>
    bool decode_fields(const packet<MaxPayload>& pkt, void* target_struct) noexcept {
        if (layouts_[pkt.opcode].field_count == 0) {
            return false; // No layout defined for this opcode
        }
        
        const auto& layout = layouts_[pkt.opcode];
        size_t data_offset = 0;
        u8* target = static_cast<u8*>(target_struct);
        
        for (size_t field_idx = 0; field_idx < layout.field_count; ++field_idx) {
            const auto& field = layout.fields[field_idx];
            
            if (!decode_single_field(pkt.data.data(), pkt.length, data_offset, field, target)) {
                return false;
            }
        }
        return true;
    }
    
private:
    struct field_layout {
        etl::array<field_def, MaxFields> fields{};
        size_t field_count{0};
    };
    
    etl::array<field_layout, OpcodeSpace> layouts_{}; // One layout per opcode
    
    bool decode_single_field(const u8* data, u16 data_len, size_t& offset, 
                            const field_def& field, u8* target) noexcept {
        u8* field_ptr = target + field.offset;
        
        switch (field.type) {
            case FieldType::U8:
                if (offset >= data_len) {
                    return false;
                }
                *field_ptr = data[offset];
                offset += 1;
                break;
                
            case FieldType::U16:
                if (offset + 1 >= data_len) {
                    return false;
                }
                *reinterpret_cast<u16*>(field_ptr) = 
                    (static_cast<u16>(data[offset]) << 8) | data[offset + 1];
                offset += 2;
                break;
                
            case FieldType::U32:
                if (offset + 3 >= data_len) {
                    return false;
                }
                *reinterpret_cast<u32*>(field_ptr) = 
                    (static_cast<u32>(data[offset]) << 24) |
                    (static_cast<u32>(data[offset + 1]) << 16) |
                    (static_cast<u32>(data[offset + 2]) << 8) |
                    data[offset + 3];
                offset += 4;
                break;
                
            case FieldType::U8_ARRAY:
                if (offset >= data_len) {
                    return false;
                }
                // Store pointer to array start
                *reinterpret_cast<const u8**>(field_ptr) = &data[offset];
                // Store length in next field (assumed to be size_t)
                *reinterpret_cast<size_t*>(field_ptr + sizeof(const u8*)) = data_len - offset;
                offset = data_len; // Consume rest of packet
                break;
        }
        return true;
    }
};

} // namespace emCore::protocol

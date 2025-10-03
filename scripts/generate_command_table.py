#!/usr/bin/env python3
"""
Command Table Generator for emCore
Generates C++ command dispatch table from commands.yaml
Similar to bootloader CommandRxList_t pattern
"""

import yaml
import sys
from pathlib import Path
from datetime import datetime

def generate_command_table(yaml_file: Path, output_file: Path):
    """Generate C++ command table from YAML configuration"""
    
    with open(yaml_file, 'r') as f:
        config = yaml.safe_load(f)
    
    commands = config.get('commands', [])
    cmd_config = config.get('config', {})
    
    namespace = cmd_config.get('namespace', 'emCore::commands')
    max_commands = cmd_config.get('max_commands', 16)
    sequential = cmd_config.get('sequential_opcodes', False)
    error_handler = cmd_config.get('error_handler', 'cmd_unknown_command')
    
    # Generate header content
    content = f"""#pragma once
// Auto-generated command dispatcher setup from commands.yaml
// Generated on {datetime.now().isoformat()}
// DO NOT EDIT - regenerate with scripts/generate_command_table.py

#include <emCore/core/types.hpp>
#include <emCore/protocol/packet_runtime.hpp>
#include <emCore/protocol/decoder.hpp>
#include <emCore/protocol/encoder.hpp>
#include <cstddef>  // for offsetof

// Note: Packet configuration symbols (emCore::protocol::gen::*) are provided by packet_runtime.hpp

// Ensure dispatcher capacity is available to packet_runtime
#ifndef EMCORE_PROTOCOL_MAX_HANDLERS
#define EMCORE_PROTOCOL_MAX_HANDLERS {max_commands}
#endif

namespace {namespace} {{

// Total commands in this table (for compile-time checks)
constexpr size_t GENERATED_COMMAND_COUNT = {len(commands)};

// Forward declarations of command handler functions
"""
    
    # Generate command enum
    content += "\n// Command opcodes enum\nenum class command_opcode : emCore::u8 {\n"
    for cmd in commands:
        content += f"    {cmd['name']} = 0x{cmd['opcode']:02X},\n"
    content += "};\n\n"
    
    # Generate command parameter structures
    content += "// Auto-generated command parameter structures\n"
    for cmd in commands:
        if 'parameters' in cmd and cmd['parameters']:
            struct_name = f"{cmd['name'].lower()}_params"
            content += f"struct {struct_name} {{\n"
            for param in cmd['parameters']:
                param_type = param['type'].replace('u8[]', 'const u8*')  # Handle arrays
                content += f"    {param_type} {param['name']};\n"
                if param['type'] == 'u8[]':
                    content += f"    size_t {param['name']}_length;\n"
            content += f"}};\n\n"
    
    # Generate typed command handler declarations
    content += "// Typed command handler function declarations\n"
    for cmd in commands:
        if 'parameters' in cmd and cmd['parameters']:
            struct_name = f"{cmd['name'].lower()}_params"
            content += f"void {cmd['function']}(const {struct_name}& params);\n"
        else:
            content += f"void {cmd['function']}();\n"
    content += f"void {error_handler}(emCore::u8 opcode);\n\n"
    
    # Generate field definitions for each command
    content += "// Auto-generated field definitions for structured decoding\n"
    for cmd in commands:
        if 'parameters' in cmd and cmd['parameters']:
            struct_name = f"{cmd['name'].lower()}_params"
            field_array_name = f"{cmd['name'].lower()}_fields"
            
            content += f"constexpr emCore::protocol::field_def {field_array_name}[] = {{\n"
            
            offset = 0
            for param in cmd['parameters']:
                param_name = param['name']
                param_type = param['type']
                
                if param_type == 'u8':
                    field_type = "emCore::protocol::FieldType::U8"
                elif param_type == 'u16':
                    field_type = "emCore::protocol::FieldType::U16"
                elif param_type == 'u32':
                    field_type = "emCore::protocol::FieldType::U32"
                elif param_type == 'u8[]':
                    field_type = "emCore::protocol::FieldType::U8_ARRAY"
                
                content += f"    {{ {field_type}, offsetof({struct_name}, {param_name}), \"{param_name}\" }},\n"
            
            content += f"}};\n\n"
    
    # Generate automatic decoding wrapper functions using field decoder
    content += "// Auto-generated packet decoding wrapper functions using field decoder\n"
    for cmd in commands:
        wrapper_name = f"_decode_{cmd['function']}"
        content += f"inline void {wrapper_name}(const emCore::protocol::runtime::PacketT& packet) {{\n"
        
        if 'parameters' in cmd and cmd['parameters']:
            struct_name = f"{cmd['name'].lower()}_params"
            content += f"    {struct_name} params{{}};\n"
            content += f"    auto& decoder = emCore::protocol::runtime::get_field_decoder();\n"
            content += f"    \n"
            content += f"    if (decoder.decode_fields(packet, &params)) {{\n"
            content += f"        {cmd['function']}(params);\n"
            content += f"    }}\n"
        else:
            content += f"    {cmd['function']}();\n"
        
        content += f"}}\n\n"
    
    # Generate encoding helper functions
    content += "// Auto-generated encoding helper functions\n"
    for cmd in commands:
        if 'parameters' in cmd and cmd['parameters']:
            struct_name = f"{cmd['name'].lower()}_params"
            encode_func_name = f"encode_{cmd['name'].lower()}_command"
            content += f"""template<typename OutputFunc>
bool {encode_func_name}(const {struct_name}& params, OutputFunc output_byte) noexcept {{
    auto& encoder = emCore::protocol::runtime::get_field_encoder();
    return encoder.encode_command<emCore::protocol::gen::packet_config>(0x{cmd['opcode']:02X}, &params, output_byte);
}}

"""
        else:
            encode_func_name = f"encode_{cmd['name'].lower()}_command"
            content += f"""template<typename OutputFunc>
bool {encode_func_name}(OutputFunc output_byte) noexcept {{
    auto& encoder = emCore::protocol::runtime::get_field_encoder();
    return encoder.encode_command<emCore::protocol::gen::packet_config>(0x{cmd['opcode']:02X}, nullptr, output_byte);
}}

"""
    
    # Generate dispatcher setup function
    content += f"""// Setup command dispatcher, field decoder, and field encoder with all handlers
inline void setup_command_dispatcher() noexcept {{
    auto& dispatcher = emCore::protocol::runtime::get_dispatcher();
    auto& decoder = emCore::protocol::runtime::get_field_decoder();
    auto& encoder = emCore::protocol::runtime::get_field_encoder();
    
    // Register field layouts for structured decoding and encoding
"""
    
    for cmd in commands:
        if 'parameters' in cmd and cmd['parameters']:
            field_array_name = f"{cmd['name'].lower()}_fields"
            field_count = len(cmd['parameters'])
            content += f"    decoder.set_field_layout(0x{cmd['opcode']:02X}, &{field_array_name}[0], {field_count});\n"
            content += f"    encoder.set_field_layout(0x{cmd['opcode']:02X}, &{field_array_name}[0], {field_count});\n"
    
    content += f"""
    // Register all command handlers
"""
    
    for cmd in commands:
        wrapper_name = f"_decode_{cmd['function']}"
        content += f"    dispatcher.register_handler(0x{cmd['opcode']:02X}, {wrapper_name});\n"
    
    content += f"""    
    // Set unknown command handler (wrapper for opcode-only signature)
    dispatcher.set_unknown_handler([](const emCore::protocol::runtime::PacketT& packet) {{
        {error_handler}(packet.opcode);
    }});
}}

// Process command using the dispatcher (replaces manual table lookup)
inline void process_command(const emCore::protocol::runtime::PacketT& packet) noexcept {{
    auto& dispatcher = emCore::protocol::runtime::get_dispatcher();
    dispatcher.dispatch(packet);
}}
"""
    
    # Generate command lookup helpers
    content += f"""
// Helper functions for command introspection
inline const char* get_command_name(emCore::u8 opcode) {{
    switch(opcode) {{"""
    
    for cmd in commands:
        content += f"""
        case 0x{cmd['opcode']:02X}: return "{cmd['name']}";"""
    
    content += f"""
        default: return "UNKNOWN";
    }}
}}

inline const char* get_command_description(emCore::u8 opcode) {{
    switch(opcode) {{"""
    
    for cmd in commands:
        content += f"""
        case 0x{cmd['opcode']:02X}: return "{cmd['description']}";"""
    
    content += f"""
        default: return "Unknown command";
    }}
}}

constexpr size_t get_command_count() {{
    return {len(commands)};
}}

}} // namespace {namespace}

static_assert(EMCORE_PROTOCOL_MAX_HANDLERS >= {len(commands)},
              "Increase EMCORE_PROTOCOL_MAX_HANDLERS or reduce number of commands");
"""
    
    # Write to output file
    output_file.parent.mkdir(parents=True, exist_ok=True)
    with open(output_file, 'w') as f:
        f.write(content)

    print(f"Generated command table: {output_file}")
    print(f"Commands: {len(commands)}")
    print(f"Sequential opcodes: {sequential}")
    print(f"Namespace: {namespace}")

def main():
    if len(sys.argv) != 3:
        print("Usage: generate_command_table.py <commands.yaml> <output.hpp>")
        sys.exit(1)
    
    yaml_file = Path(sys.argv[1])
    output_file = Path(sys.argv[2])
    
    if not yaml_file.exists():
        print(f"Error: {yaml_file} not found")
        sys.exit(1)
    
    generate_command_table(yaml_file, output_file)

if __name__ == "__main__":
    main()

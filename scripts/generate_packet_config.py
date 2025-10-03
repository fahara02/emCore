#!/usr/bin/env python3
"""
Generate C++ packet parser configuration header from packet.yml
Usage: python generate_packet_config.py [packet.yml] [output.hpp]

Outputs: src/emCore/generated/packet_config.hpp (by default)
"""
import sys
from pathlib import Path

try:
    import yaml
except ImportError:
    print("ERROR: PyYAML not installed. Install with: pip install pyyaml")
    sys.exit(1)


def _is_hex_byte(x):
    if isinstance(x, int):
        return 0 <= x <= 0xFF
    if isinstance(x, str):
        try:
            v = int(x, 0)
            return 0 <= v <= 0xFF
        except Exception:
            return False
    return False


def _to_byte(x):
    if isinstance(x, int):
        return x & 0xFF
    return int(x, 0) & 0xFF


def validate_packet_yaml(cfg: dict):
    errors = []
    pkt = cfg.get('packet', {}) or {}
    if not isinstance(pkt, dict):
        errors.append("'packet' must be a mapping")
        pkt = {}

    sync = pkt.get('sync', [0x55, 0xAA])
    if not isinstance(sync, list) or len(sync) < 1:
        errors.append("packet.sync must be non-empty list")
    else:
        for i, b in enumerate(sync):
            if not _is_hex_byte(b):
                errors.append(f"packet.sync[{i}] must be a byte (int or 0xNN string)")

    length16 = pkt.get('length_16bit', True)
    if not isinstance(length16, bool):
        errors.append("packet.length_16bit must be boolean")

    max_payload = pkt.get('max_payload', 128)
    if not isinstance(max_payload, int) or max_payload <= 0:
        errors.append("packet.max_payload must be positive integer")

    opcodes = cfg.get('opcodes', []) or []
    if not isinstance(opcodes, list) or len(opcodes) == 0:
        errors.append("opcodes must be a non-empty list")
    else:
        seen_codes = set()
        seen_names = set()
        for idx, op in enumerate(opcodes):
            if not isinstance(op, dict):
                errors.append(f"opcodes[{idx}] must be a mapping")
                continue
            name = op.get('name', '')
            code = op.get('code', None)
            if not isinstance(name, str) or not name:
                errors.append(f"opcodes[{idx}].name must be non-empty string")
            elif name in seen_names:
                errors.append(f"Duplicate opcode name: {name}")
            else:
                seen_names.add(name)
            if code is None or not _is_hex_byte(code):
                errors.append(f"opcodes[{idx}].code must be a byte (int or 0xNN string)")
            else:
                ival = _to_byte(code)
                if ival in seen_codes:
                    errors.append(f"Duplicate opcode value: 0x{ival:02X}")
                else:
                    seen_codes.add(ival)

    if errors:
        raise ValueError("packet.yml validation failed:\n - " + "\n - ".join(errors))


def generate_packet_header(cfg: dict, out_path: Path):
    pkt = cfg.get('packet', {}) or {}
    sync = pkt.get('sync', [0x55, 0xAA])
    sync_bytes = [_to_byte(b) for b in sync]
    length16 = bool(pkt.get('length_16bit', True))
    max_payload = int(pkt.get('max_payload', 128))
    opcodes = cfg.get('opcodes', [])

    sync_len = len(sync_bytes)
    sync_list = ", ".join([f"0x{b:02X}" for b in sync_bytes])

    # Emit header
    header = []
    header.append("#pragma once")
    header.append("")
    header.append("#include <emCore/protocol/packet_parser.hpp>")
    header.append("")
    header.append("#ifndef EMCORE_GENERATED_PACKET_CONFIG_HPP")
    header.append("#define EMCORE_GENERATED_PACKET_CONFIG_HPP")
    header.append("")
    header.append("namespace emCore::protocol::gen {")
    header.append("")
    header.append(f"inline constexpr u8 PACKET_SYNC[{sync_len}] = {{ {sync_list} }};")
    header.append(f"inline constexpr bool PACKET_LENGTH_16BIT = {'true' if length16 else 'false'};")
    header.append(f"inline constexpr size_t PACKET_MAX_PAYLOAD = {max_payload};")
    header.append(f"inline constexpr size_t PACKET_SYNC_LEN = {sync_len};")
    header.append("")
    header.append("// Provide a type for encoder/decoder template configuration")
    header.append("struct packet_config {")
    header.append(f"    static constexpr size_t PACKET_SYNC_LEN = {sync_len};")
    header.append(f"    static constexpr bool PACKET_LENGTH_16BIT = {'true' if length16 else 'false'};")
    header.append(f"    static inline constexpr u8 PACKET_SYNC[PACKET_SYNC_LEN] = {{ {sync_list} }};")
    header.append("};")
    header.append("")

    # Opcodes enum
    header.append("enum class opcode : u8 {")
    for op in opcodes:
        name = op['name']
        code = _to_byte(op['code'])
        header.append(f"    {name} = 0x{code:02X},")
    header.append("};")
    header.append("")

    # Type aliases
    header.append("using PacketT = packet<PACKET_MAX_PAYLOAD>;")
    header.append("using ParserT = packet_parser<PACKET_MAX_PAYLOAD, PACKET_SYNC_LEN, PACKET_LENGTH_16BIT, PACKET_SYNC>;")
    header.append("template <size_t MaxHandlers>")
    header.append("using DispatcherT = command_dispatcher<MaxHandlers, PacketT>;")
    header.append("")

    header.append("} // namespace emCore::protocol::gen")
    header.append("")
    header.append("#endif // EMCORE_GENERATED_PACKET_CONFIG_HPP")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, 'w') as f:
        f.write("\n".join(header) + "\n")


def main():
    if len(sys.argv) < 2:
        print("Usage: python generate_packet_config.py <packet.yml> [output.hpp]")
        sys.exit(1)
    yaml_file = Path(sys.argv[1])
    # Default to userspace output: <CWD>/src/generated_packet_config.hpp
    out_file = Path(sys.argv[2]) if len(sys.argv) > 2 else Path.cwd() / 'src' / 'generated_packet_config.hpp'
    if not yaml_file.exists():
        print(f"Error: {yaml_file} not found")
        sys.exit(1)
    with open(yaml_file, 'r') as f:
        cfg = yaml.safe_load(f)
    validate_packet_yaml(cfg)
    generate_packet_header(cfg, out_file)
    print(f"Generated {out_file} from {yaml_file}")


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""AxiomTrace binary frame decoder (standalone legacy script).

.. deprecated::
    Use ``python -m axiomtrace_tools.cli <file>`` instead.
    This file is kept for backward compatibility. The canonical decoder
    implementation is in ``tool/src/axiomtrace_tools/decoder.py``.
"""

import struct
import sys
import json
from pathlib import Path

FRAME_SYNC = 0xA5  # AXIOM_SYNC_BYTE
WIRE_VERSION_MAJOR = 1

TYPE_TAGS = {
    0x00: ('bool', 1),
    0x01: ('u8', 1),
    0x02: ('i8', 1),
    0x03: ('u16', 2),
    0x04: ('i16', 2),
    0x05: ('u32', 4),
    0x06: ('i32', 4),
    0x07: ('f32', 4),
    0x08: ('timestamp', 4),
    0x09: ('bytes', None),
}


def crc16_ccitt_false(data: bytes) -> int:
    """CRC-16/CCITT-FALSE poly=0x1021 init=0xFFFF."""
    table = []
    for i in range(256):
        crc = 0
        c = i << 8
        for _ in range(8):
            if (crc ^ c) & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
            c <<= 1
        table.append(crc & 0xFFFF)

    crc = 0xFFFF
    for b in data:
        crc = ((crc << 8) ^ table[((crc >> 8) ^ b) & 0xFF]) & 0xFFFF
    return crc


def decode_frame(raw: bytes, offset: int = 0):
    if offset >= len(raw):
        return None, offset

    # Find sync
    while offset < len(raw) and raw[offset] != FRAME_SYNC:
        offset += 1
    if offset + 11 > len(raw):  # minimum frame: 8 header + 1 len + 0 payload + 2 crc
        return None, offset

    start = offset
    sync = raw[offset]
    version = raw[offset + 1]
    major = version >> 4
    minor = version & 0x0F
    level_byte = raw[offset + 2]
    level = level_byte & 0x0F
    reserved = (level_byte >> 4) & 0x0F
    module_id = raw[offset + 3]
    event_id = struct.unpack_from('<H', raw, offset + 4)[0]
    seq = struct.unpack_from('<H', raw, offset + 6)[0]
    payload_len = raw[offset + 8]

    if major != WIRE_VERSION_MAJOR:
        return {'error': 'VERSION_MISMATCH', 'major': major}, offset + 1
    if reserved != 0:
        return {'error': 'RESERVED_NONZERO'}, offset + 1
    if level >= 5:
        return {'error': 'INVALID_LEVEL'}, offset + 1

    frame_end = offset + 9 + payload_len + 2
    if frame_end > len(raw):
        return {'error': 'TRUNCATED'}, offset + 1

    payload_raw = raw[offset + 9:offset + 9 + payload_len]
    crc_expected = struct.unpack_from('<H', raw, offset + 9 + payload_len)[0]
    crc_actual = crc16_ccitt_false(raw[offset:offset + 9 + payload_len])

    if crc_expected != crc_actual:
        return {'error': 'CRC_MISMATCH', 'expected': crc_expected, 'actual': crc_actual}, offset + 1

    # Decode payload
    payload_decoded = []
    p = 0
    while p < payload_len:
        tag = payload_raw[p]
        p += 1
        if tag in TYPE_TAGS:
            name, size = TYPE_TAGS[tag]
            if name == 'bytes':
                if p >= payload_len:
                    break
                blen = payload_raw[p]
                p += 1
                val = payload_raw[p:p + blen]
                p += blen
            elif name == 'bool':
                val = payload_raw[p] != 0
                p += size
            elif name == 'f32':
                val = struct.unpack_from('<f', payload_raw, p)[0]
                p += size
            elif name.startswith('u'):
                if size == 1:
                    val = payload_raw[p]
                elif size == 2:
                    val = struct.unpack_from('<H', payload_raw, p)[0]
                else:
                    val = struct.unpack_from('<I', payload_raw, p)[0]
                p += size
            elif name.startswith('i'):
                if size == 1:
                    val = struct.unpack_from('<b', payload_raw, p)[0]
                elif size == 2:
                    val = struct.unpack_from('<h', payload_raw, p)[0]
                else:
                    val = struct.unpack_from('<i', payload_raw, p)[0]
                p += size
            elif name == 'timestamp':
                val = struct.unpack_from('<I', payload_raw, p)[0]
                p += size
            else:
                val = payload_raw[p:p + size]
                p += size
            payload_decoded.append({'type': name, 'value': val})
        elif 0x0A <= tag <= 0x7F:
            payload_decoded.append({'type': 'unknown_reserved', 'tag': tag})
        else:
            payload_decoded.append({'type': 'unknown_user', 'tag': tag})

    result = {
        'sync': sync,
        'version': (major, minor),
        'level': ['DEBUG', 'INFO', 'WARN', 'ERROR', 'FAULT'][level],
        'module_id': module_id,
        'event_id': event_id,
        'seq': seq,
        'payload_len': payload_len,
        'payload': payload_decoded,
        'crc': crc_expected,
    }
    return result, frame_end


def decode_stream(raw: bytes):
    offset = 0
    frames = []
    while offset < len(raw):
        frame, next_offset = decode_frame(raw, offset)
        if frame is None:
            break
        if next_offset == offset:
            offset += 1
        else:
            offset = next_offset
        frames.append(frame)
    return frames


def main():
    import argparse
    parser = argparse.ArgumentParser(description='AxiomTrace decoder')
    parser.add_argument('input', help='Binary file to decode')
    parser.add_argument('--output', choices=['text', 'json'], default='text')
    args = parser.parse_args()

    raw = Path(args.input).read_bytes()
    frames = decode_stream(raw)

    if args.output == 'json':
        print(json.dumps(frames, indent=2))
    else:
        for f in frames:
            if 'error' in f:
                print(f"[ERROR: {f['error']}]")
                continue
            print(f"[{f['seq']:4d}] [{f['level']:5s}] mod=0x{f['module_id']:02X} evt=0x{f['event_id']:04X} payload={f['payload']}")


if __name__ == '__main__':
    main()

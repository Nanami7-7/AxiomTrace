"""AxiomTrace timestamp-aware binary frame decoder."""

from __future__ import annotations

import os
import struct
from pathlib import Path
from typing import Any

from axiomtrace_tools.dictionary import EventDictionary, WIRE_SIZE_BY_NAME

FRAME_SYNC = 0xA5
WIRE_VERSION_MAJOR = 2
WIRE_VERSION_MINOR = 0
WIRE_VERSION = (WIRE_VERSION_MAJOR << 4) | WIRE_VERSION_MINOR
SUPPORTED_WIRE_VERSION_MAJORS = {1, WIRE_VERSION_MAJOR}

TYPE_TAGS = {
    0x00: ("bool", 1),
    0x01: ("u8", 1),
    0x02: ("i8", 1),
    0x03: ("u16", 2),
    0x04: ("i16", 2),
    0x05: ("u32", 4),
    0x06: ("i32", 4),
    0x07: ("f32", 4),
    0x08: ("timestamp", 4),
    0x09: ("bytes", None),
}

TYPE_META_LOCATION = 0x0A
TYPE_META_IDENTITY = 0x0B
LOCATION_MODE_HASH = 1
LOCATION_MODE_FILE_ID = 2

DICTIONARY_SEARCH_DIRS = [
    "build",
    "build/generated",
    "build/axiomtrace-bundle",
    "gen",
    ".axiom",
    "output",
]


def find_dictionary(start_path: str | None = None) -> Path | None:
    """Search for dictionary.json while walking toward the filesystem root."""
    current = Path(os.path.abspath(start_path or os.getcwd()))
    while True:
        for dirname in DICTIONARY_SEARCH_DIRS:
            dictionary = current / dirname / "dictionary.json"
            if dictionary.is_file():
                return dictionary
        dictionary = current / "dictionary.json"
        if dictionary.is_file():
            return dictionary
        if current.parent == current:
            return None
        current = current.parent


def crc16_ccitt_false(data: bytes) -> int:
    """CRC-16/CCITT-FALSE poly=0x1021 init=0xFFFF."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def _decode_timestamp(raw: bytes, offset: int) -> tuple[int, int] | None:
    if offset >= len(raw):
        return None
    first = raw[offset]
    if first < 0x80:
        return first & 0x7F, 1
    if first < 0xC0:
        if offset + 2 > len(raw):
            return None
        return ((first & 0x3F) << 8) | raw[offset + 1], 2
    if first < 0xFE:
        if offset + 3 > len(raw):
            return None
        return ((first & 0x1F) << 16) | (raw[offset + 1] << 8) | raw[offset + 2], 3
    if offset + 5 > len(raw):
        return None
    return struct.unpack_from("<I", raw, offset + 1)[0], 5


def _error(name: str, **fields: Any) -> dict[str, Any]:
    return {"error": name, **fields}


def decode_frame(
    raw: bytes,
    offset: int = 0,
    dictionary: EventDictionary | dict[str, Any] | None = None,
) -> tuple[dict[str, Any] | None, int]:
    """Decode a single raw frame starting at or after offset."""
    while offset < len(raw) and raw[offset] != FRAME_SYNC:
        offset += 1
    if offset >= len(raw):
        return None, offset
    start = offset
    if offset + 8 > len(raw):
        return _error("TRUNCATED"), len(raw)

    version = raw[offset + 1]
    major = version >> 4
    minor = version & 0x0F
    level_byte = raw[offset + 2]
    level = level_byte & 0x0F
    reserved = level_byte >> 4
    if major not in SUPPORTED_WIRE_VERSION_MAJORS:
        return _error("VERSION_MISMATCH", major=major, minor=minor), offset + 1
    if reserved != 0:
        return _error("RESERVED_NONZERO"), offset + 1
    if level >= 5:
        return _error("INVALID_LEVEL"), offset + 1

    if major == 1 and minor == 0:
        timestamp_delta = None
        timestamp_len = 0
        payload_len_offset = offset + 8
    else:
        timestamp = _decode_timestamp(raw, offset + 8)
        if timestamp is None:
            return _error("TRUNCATED_TIMESTAMP"), len(raw)
        timestamp_delta, timestamp_len = timestamp
        payload_len_offset = offset + 8 + timestamp_len
    if payload_len_offset >= len(raw):
        return _error("TRUNCATED"), len(raw)
    payload_len = raw[payload_len_offset]
    payload_start = payload_len_offset + 1
    frame_end = payload_start + payload_len + 2
    if frame_end > len(raw):
        return _error("TRUNCATED"), len(raw)

    crc_expected = struct.unpack_from("<H", raw, payload_start + payload_len)[0]
    crc_actual = crc16_ccitt_false(raw[start:payload_start + payload_len])
    if crc_expected != crc_actual:
        return _error("CRC_MISMATCH", expected=crc_expected, actual=crc_actual), offset + 1

    module_id = raw[offset + 3]
    event_id = struct.unpack_from("<H", raw, offset + 4)[0]

    payload, location, metadata_id, warnings = _decode_payload(
        raw[payload_start:payload_start + payload_len],
        major,
        dictionary,
        module_id,
        event_id,
    )
    result: dict[str, Any] = {
        "sync": FRAME_SYNC,
        "version": (major, minor),
        "level": ["DEBUG", "INFO", "WARN", "ERROR", "FAULT"][level],
        "module_id": raw[offset + 3],
        "event_id": struct.unpack_from("<H", raw, offset + 4)[0],
        "seq": struct.unpack_from("<H", raw, offset + 6)[0],
        "payload_len": payload_len,
        "payload": payload,
        "crc": crc_expected,
    }
    if timestamp_len:
        result["timestamp_delta"] = timestamp_delta
        result["timestamp_len"] = timestamp_len
    if location:
        result["location"] = location
    if metadata_id:
        result["metadata_id"] = metadata_id
    if warnings:
        result["warnings"] = warnings
    return result, frame_end


def _decode_payload(
    payload_raw: bytes,
    wire_major: int,
    dictionary: EventDictionary | dict[str, Any] | None,
    module_id: int,
    event_id: int,
) -> tuple[list[dict[str, Any]], dict[str, Any] | None, str | None, list[str]]:
    if module_id == 0 and event_id == 0:
        return _decode_typed_payload(payload_raw)

    if module_id == 0 and event_id == 2:
        if len(payload_raw) >= 9 and payload_raw[0] == TYPE_META_IDENTITY:
            warnings = [] if len(payload_raw) == 9 else ["INVALID_METADATA_IDENTITY"]
            return [], None, payload_raw[1:9].hex(), warnings
        return [], None, None, ["INVALID_METADATA_IDENTITY"]

    if wire_major == 1:
        return _decode_typed_payload(payload_raw)
    return _decode_packed_payload(payload_raw, dictionary, module_id, event_id)


def _decode_typed_payload(payload_raw: bytes) -> tuple[list[dict[str, Any]], dict[str, Any] | None, str | None, list[str]]:
    payload: list[dict[str, Any]] = []
    location: dict[str, Any] | None = None
    metadata_id: str | None = None
    warnings: list[str] = []
    position = 0

    while position < len(payload_raw):
        tag = payload_raw[position]
        position += 1
        if tag == TYPE_META_LOCATION:
            location, position, warning = _decode_location(payload_raw, position)
            if warning:
                warnings.append(warning)
                break
            continue
        if tag == TYPE_META_IDENTITY:
            if position + 8 > len(payload_raw):
                warnings.append("TRUNCATED_METADATA_ID")
                break
            metadata_id = payload_raw[position:position + 8].hex()
            position += 8
            continue
        if tag not in TYPE_TAGS:
            payload.append({"type": "unknown", "tag": tag})
            warnings.append("UNKNOWN_TYPE_TAG")
            break
        type_name, size = TYPE_TAGS[tag]
        if type_name == "bytes":
            if position >= len(payload_raw):
                warnings.append("TRUNCATED_PAYLOAD")
                break
            size = payload_raw[position]
            position += 1
        if size is None or position + size > len(payload_raw):
            warnings.append("TRUNCATED_PAYLOAD")
            break
        value = _decode_value(type_name, payload_raw[position:position + size])
        position += size
        payload.append({"type": type_name, "value": value})

    return payload, location, metadata_id, warnings


def _decode_packed_payload(
    payload_raw: bytes,
    dictionary: EventDictionary | dict[str, Any] | None,
    module_id: int,
    event_id: int,
) -> tuple[list[dict[str, Any]], dict[str, Any] | None, str | None, list[str]]:
    payload: list[dict[str, Any]] = []
    warnings: list[str] = []
    position = 0

    if (module_id, event_id) == (0, 1):
        args = [{"type": "u32"}, {"type": "u8"}, {"type": "u16"}]
    else:
        event_dictionary = dictionary if isinstance(dictionary, EventDictionary) else EventDictionary(dictionary) if dictionary else None
        metadata = event_dictionary.find_event(module_id, event_id) if event_dictionary else None
        if metadata is None:
            if payload_raw:
                payload.append({"type": "raw", "value": payload_raw.hex()})
            if dictionary is not None:
                warnings.append("UNKNOWN_EVENT_SCHEMA")
            return payload, None, None, warnings
        args = metadata.args

    for arg in args:
        type_name = str(arg.get("type", "unknown"))
        if type_name == "bytes":
            if position >= len(payload_raw):
                warnings.append("TRUNCATED_PAYLOAD")
                return payload, None, None, warnings
            size = payload_raw[position]
            position += 1
        else:
            size = arg.get("wire_size", WIRE_SIZE_BY_NAME.get(type_name))
            if size is None:
                warnings.append("UNSUPPORTED_PACKED_TYPE")
                return payload, None, None, warnings
            size = int(size)
        if position + size > len(payload_raw):
            warnings.append("TRUNCATED_PAYLOAD")
            return payload, None, None, warnings
        value = _decode_value(type_name, payload_raw[position:position + size])
        position += size
        payload.append({"type": type_name, "value": value})

    location: dict[str, Any] | None = None
    metadata_id: str | None = None
    while position < len(payload_raw):
        tag = payload_raw[position]
        position += 1
        if tag == TYPE_META_LOCATION:
            location, position, warning = _decode_location(payload_raw, position)
            if warning:
                warnings.append(warning)
                break
            continue
        if tag == TYPE_META_IDENTITY:
            if position + 8 > len(payload_raw):
                warnings.append("TRUNCATED_METADATA_ID")
                break
            metadata_id = payload_raw[position:position + 8].hex()
            position += 8
            continue
        warnings.append("UNKNOWN_METADATA_SUFFIX")
        break

    return payload, location, metadata_id, warnings


def _decode_location(payload: bytes, position: int) -> tuple[dict[str, Any] | None, int, str | None]:
    if position >= len(payload):
        return None, position, "TRUNCATED_LOCATION"
    mode = payload[position]
    position += 1
    if mode == LOCATION_MODE_FILE_ID:
        if position + 4 > len(payload):
            return None, position, "TRUNCATED_LOCATION"
        return {
            "mode": "file_id",
            "file_id": struct.unpack_from("<H", payload, position)[0],
            "line": struct.unpack_from("<H", payload, position + 2)[0],
        }, position + 4, None
    if mode == LOCATION_MODE_HASH:
        if position + 6 > len(payload):
            return None, position, "TRUNCATED_LOCATION"
        return {
            "mode": "hash",
            "file_hash": struct.unpack_from("<H", payload, position)[0],
            "line": struct.unpack_from("<H", payload, position + 2)[0],
            "function_hash": struct.unpack_from("<H", payload, position + 4)[0],
        }, position + 6, None
    return {"mode": "unknown", "mode_id": mode}, position, "UNKNOWN_LOCATION_MODE"


def _decode_value(type_name: str, data: bytes) -> Any:
    if type_name == "bool":
        return data[0] != 0
    if type_name == "u8":
        return data[0]
    if type_name == "i8":
        return struct.unpack("<b", data)[0]
    if type_name == "u16":
        return struct.unpack("<H", data)[0]
    if type_name == "i16":
        return struct.unpack("<h", data)[0]
    if type_name in {"u32", "ts", "timestamp"}:
        return struct.unpack("<I", data)[0]
    if type_name == "i32":
        return struct.unpack("<i", data)[0]
    if type_name == "f32":
        return struct.unpack("<f", data)[0]
    return data.hex()


def decode_stream(
    raw: bytes,
    dictionary: EventDictionary | dict[str, Any] | None = None,
) -> list[dict[str, Any]]:
    """Decode frames from a raw stream, recovering at the next sync byte."""
    frames: list[dict[str, Any]] = []
    elapsed = 0
    offset = 0
    while offset < len(raw):
        frame, next_offset = decode_frame(raw, offset, dictionary)
        if frame is None:
            break
        if "error" not in frame:
            if "timestamp_delta" in frame:
                elapsed += int(frame["timestamp_delta"])
                frame["timestamp"] = elapsed
        frames.append(frame)
        offset = next_offset if next_offset > offset else offset + 1
    return frames


def extract_metadata_id(frames: list[dict[str, Any]]) -> str | None:
    """Return the first decode-metadata identity embedded in a decoded trace."""
    for frame in frames:
        if frame.get("module_id") == 0 and frame.get("event_id") == 2 and frame.get("metadata_id"):
            return str(frame["metadata_id"])
    return None

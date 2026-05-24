"""AxiomTrace Tools - Binary frame decoder and utilities."""

from axiomtrace_tools.decoder import (
    FRAME_SYNC,
    WIRE_VERSION_MAJOR,
    TYPE_TAGS,
    crc16_ccitt_false,
    decode_frame,
    decode_stream,
)
from axiomtrace_tools.dictionary import EventDictionary, EventMetadata

__all__ = [
    "FRAME_SYNC",
    "WIRE_VERSION_MAJOR",
    "TYPE_TAGS",
    "crc16_ccitt_false",
    "decode_frame",
    "decode_stream",
    "EventDictionary",
    "EventMetadata",
]

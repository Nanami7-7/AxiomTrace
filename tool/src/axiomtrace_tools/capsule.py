"""Host-side report support for the draft AxiomTrace fault capsule format."""

from __future__ import annotations

import binascii
import json
import struct
from pathlib import Path
from typing import Any

from axiomtrace_tools.decoder import decode_stream, extract_metadata_id
from axiomtrace_tools.render import render_json, render_text
from axiomtrace_tools.dictionary import EventDictionary

CAPSULE_MAGIC = b"AXCP"


def decode_capsule(data: bytes) -> dict[str, Any]:
    """Decode the v1.0-draft host fixture format and validate its CRC-32."""
    if len(data) < 25 or data[:4] != CAPSULE_MAGIC:
        raise ValueError("invalid capsule magic or truncated capsule")
    version = data[4]
    capsule_len = struct.unpack_from("<H", data, 5)[0]
    if capsule_len != len(data):
        raise ValueError("capsule length mismatch")
    expected_crc = struct.unpack_from("<I", data, len(data) - 4)[0]
    actual_crc = binascii.crc32(data[:-4]) & 0xFFFFFFFF
    if actual_crc != expected_crc:
        raise ValueError("capsule CRC mismatch")
    position = 19
    snapshot_id = data[position]
    snapshot_len = data[position + 1]
    position += 2
    if position + snapshot_len > len(data) - 4:
        raise ValueError("capsule snapshot is truncated")
    snapshot = data[position:position + snapshot_len]
    position += snapshot_len
    frame_stream = data[position:-4]
    frames = decode_stream(frame_stream)
    return {
        "format": "v1.0-draft",
        "version": version,
        "reset_reason": data[7],
        "fault_type": data[8],
        "firmware_hash": data[9:13].hex(),
        "drop_count": struct.unpack_from("<I", data, 13)[0],
        "pre_window_count": data[17],
        "post_window_count": data[18],
        "snapshot_id": snapshot_id,
        "snapshot": _snapshot_fields(snapshot_id, snapshot),
        "frames": frames,
        "_frame_stream": frame_stream,
        "crc32": expected_crc,
    }


def load_capsule(path: str | Path) -> dict[str, Any]:
    return decode_capsule(Path(path).read_bytes())


def render_capsule_report(
    report: dict[str, Any],
    output_format: str,
    dictionary: EventDictionary | None = None,
    bundle_metadata_id: str | None = None,
    source_map: dict[str, Any] | None = None,
) -> str:
    """Render a decoded capsule to JSON, Markdown, or compact HTML."""
    frames = report["frames"]
    if dictionary is not None and report.get("_frame_stream") is not None:
        frames = decode_stream(report["_frame_stream"], dictionary)
    trace_metadata_id = extract_metadata_id(frames)
    if bundle_metadata_id:
        if not trace_metadata_id:
            raise ValueError("semantic capsule report requires a metadata identity event")
        if trace_metadata_id != bundle_metadata_id:
            raise ValueError("capsule metadata identity does not match bundle")
    semantic_json = json.loads(render_json(frames, dictionary, bundle_metadata_id, source_map))
    result = {**report, "events": semantic_json}
    result.pop("frames", None)
    result.pop("_frame_stream", None)
    result["trace_metadata_id"] = trace_metadata_id
    result["metadata_identity_matches_bundle"] = True if bundle_metadata_id else None
    if output_format == "json":
        return json.dumps(result, indent=2)
    events_text = render_text(frames, dictionary, bundle_metadata_id, source_map)
    markdown = "\n".join(
        [
            "# AxiomTrace Fault Capsule Report",
            "",
            f"- Format: `{report['format']}`",
            f"- Firmware hash: `{report['firmware_hash']}`",
            f"- Drop count: `{report['drop_count']}`",
            f"- Reset reason: `{report['reset_reason']}`",
            f"- Fault type: `{report['fault_type']}`",
            "",
            "## Snapshot",
            "",
            "```json",
            json.dumps(report["snapshot"], indent=2),
            "```",
            "",
            "## Events",
            "",
            "```text",
            events_text,
            "```",
        ]
    )
    if output_format == "markdown":
        return markdown
    return "<html><body><pre>" + _escape_html(markdown) + "</pre></body></html>"


def _snapshot_fields(snapshot_id: int, data: bytes) -> dict[str, Any]:
    fields = ["pc", "lr", "sp", "xpsr"] if snapshot_id == 1 else ["mepc", "mcause", "mtval", "ra", "sp"]
    result: dict[str, Any] = {"snapshot_id": snapshot_id, "raw_hex": data.hex()}
    for index, field in enumerate(fields):
        start = index * 4
        if start + 4 <= len(data):
            result[field] = struct.unpack_from("<I", data, start)[0]
    return result


def _escape_html(value: str) -> str:
    return value.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

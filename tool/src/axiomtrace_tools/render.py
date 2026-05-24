"""Render decoded AxiomTrace frames to host-facing formats."""

from __future__ import annotations

import json
from typing import Any, Iterable

from axiomtrace_tools.dictionary import EventDictionary, EventMetadata
from axiomtrace_tools.source_map import resolve_location


def semantic_frame(
    frame: dict[str, Any],
    dictionary: EventDictionary | None = None,
    metadata_id: str | None = None,
    source_map: dict[str, Any] | None = None,
) -> dict[str, Any]:
    """Convert a raw decoded frame to a semantic event object."""
    if "error" in frame:
        return {"error": frame["error"], **{k: v for k, v in frame.items() if k != "error"}}

    module_id = int(frame["module_id"])
    event_id = int(frame["event_id"])
    metadata = dictionary.find_event(module_id, event_id) if dictionary else None
    args = _payload_args(frame.get("payload", []), metadata)
    system_names = {
        (0x00, 0x0001): ("SYSTEM", "DROP_SUMMARY"),
        (0x00, 0x0002): ("SYSTEM", "METADATA_ID"),
    }
    fallback_names = system_names.get((module_id, event_id))
    result = {
        "metadata_id": frame.get("metadata_id") or metadata_id,
        "timestamp": frame.get("timestamp"),
        "timestamp_delta": frame.get("timestamp_delta"),
        "seq": frame.get("seq"),
        "level": frame.get("level"),
        "module": metadata.module_name if metadata else (fallback_names[0] if fallback_names else f"0x{module_id:02X}"),
        "event": metadata.event_name if metadata else (fallback_names[1] if fallback_names else f"0x{event_id:04X}"),
        "module_id": module_id,
        "event_id": event_id,
        "args": args,
        "payload": frame.get("payload", []),
        "crc": frame.get("crc"),
    }
    location = resolve_location(frame.get("location"), source_map)
    if location:
        result["location"] = location
    if frame.get("warnings"):
        result["warnings"] = frame["warnings"]
    if metadata and metadata.text:
        result["message"] = _format_message(metadata.text, args)
    return result


def render_text(
    frames: Iterable[dict[str, Any]],
    dictionary: EventDictionary | None = None,
    metadata_id: str | None = None,
    source_map: dict[str, Any] | None = None,
) -> str:
    """Render frames as human-readable text."""
    lines = []
    for frame in frames:
        event = semantic_frame(frame, dictionary, metadata_id, source_map)
        if "error" in event:
            lines.append(f"[ERROR: {event['error']}]")
            continue
        message = event.get("message") or f"args={event['args'] if event['args'] else event['payload']}"
        location = event.get("location")
        suffix = ""
        if location:
            source = location.get("file") or (
                f"file_id={location.get('file_id')}" if location.get("mode") == "file_id"
                else f"file_hash=0x{int(location.get('file_hash', 0)):04X}"
            )
            suffix = f" ({source}:{location.get('line')})"
        lines.append(
            f"[{int(event['seq']):4d}] "
            f"[{str(event['level']):5s}] "
            f"[{event['module']}] {event['event']}: {message}{suffix}"
        )
    return "\n".join(lines)


def render_json(frames: Iterable[dict[str, Any]], dictionary: EventDictionary | None = None, metadata_id: str | None = None, source_map: dict[str, Any] | None = None) -> str:
    """Render frames as a JSON array."""
    return json.dumps([semantic_frame(frame, dictionary, metadata_id, source_map) for frame in frames], indent=2)


def render_jsonl(frames: Iterable[dict[str, Any]], dictionary: EventDictionary | None = None, metadata_id: str | None = None, source_map: dict[str, Any] | None = None) -> str:
    """Render frames as JSON Lines."""
    return "\n".join(json.dumps(semantic_frame(frame, dictionary, metadata_id, source_map)) for frame in frames)


def render_raw_json(frames: Iterable[dict[str, Any]]) -> str:
    """Render raw decoded frames as a JSON array."""
    return json.dumps(list(frames), indent=2)


def _payload_args(payload: list[dict[str, Any]], metadata: EventMetadata | None) -> dict[str, Any]:
    args: dict[str, Any] = {}
    for idx, item in enumerate(payload):
        if metadata and idx < len(metadata.args):
            name = str(metadata.args[idx].get("name", f"arg{idx}"))
        else:
            name = f"arg{idx}"
        args[name] = item.get("value", item)
    return args


def _format_message(template: str, args: dict[str, Any]) -> str:
    message = template
    for name, value in args.items():
        for marker in (f"{{{name}}}",):
            message = message.replace(marker, str(value))
        # Dictionary placeholders are typically {name:type}.
        prefix = "{" + name + ":"
        while prefix in message:
            start = message.find(prefix)
            end = message.find("}", start)
            if end == -1:
                break
            message = message[:start] + str(value) + message[end + 1 :]
    return message

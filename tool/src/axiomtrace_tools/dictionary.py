"""Dictionary loading and semantic lookup for AxiomTrace tools."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any


TYPE_TAG_BY_NAME = {
    "bool": 0x00,
    "u8": 0x01,
    "i8": 0x02,
    "u16": 0x03,
    "i16": 0x04,
    "u32": 0x05,
    "i32": 0x06,
    "f32": 0x07,
    "ts": 0x08,
    "timestamp": 0x08,
    "bytes": 0x09,
}

WIRE_SIZE_BY_NAME = {
    "bool": 1,
    "u8": 1,
    "i8": 1,
    "u16": 2,
    "i16": 2,
    "u32": 4,
    "i32": 4,
    "f32": 4,
    "ts": 4,
    "timestamp": 4,
    "bytes": None,
}


@dataclass(frozen=True)
class EventMetadata:
    """Semantic metadata for a decoded event."""

    module_id: int
    event_id: int
    module_name: str
    event_name: str
    level: str | None
    text: str | None
    args: list[dict[str, Any]]
    raw: dict[str, Any]


def parse_int(value: Any) -> int:
    """Parse decimal or hex integer values from YAML/JSON metadata."""
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        return int(value, 0)
    raise TypeError(f"expected integer-like value, got {type(value).__name__}")


def event_key(module_id: int, event_id: int) -> str:
    """Return the canonical dictionary key for a module/event tuple."""
    return f"0x{module_id:02X}:0x{event_id:04X}"


class EventDictionary:
    """Normalized event dictionary supporting legacy and v1 schemas."""

    def __init__(self, data: dict[str, Any], path: Path | None = None):
        self.data = data
        self.path = path
        self.events = self._normalize(data)

    def find_event(self, module_id: int, event_id: int) -> EventMetadata | None:
        """Return metadata for a module/event pair, or None if unknown."""
        return self.events.get((module_id, event_id))

    @staticmethod
    def _normalize(data: dict[str, Any]) -> dict[tuple[int, int], EventMetadata]:
        if "modules" in data:
            return _normalize_modules(data["modules"])
        if "dictionary" in data and isinstance(data["dictionary"], dict):
            dictionary = data["dictionary"]
            if "modules" in dictionary:
                return _normalize_modules(dictionary["modules"])
            return _normalize_nested_dictionary(dictionary)
        if "events" in data:
            return _normalize_flat_events(data["events"])
        return {}


def _normalize_modules(modules: Any) -> dict[tuple[int, int], EventMetadata]:
    events: dict[tuple[int, int], EventMetadata] = {}
    iterable = modules.items() if isinstance(modules, dict) else enumerate(modules)
    for module_key, module in iterable:
        if not isinstance(module, dict):
            continue
        module_id = parse_int(module.get("id", module_key))
        module_name = str(module.get("name", f"MODULE_{module_id:02X}"))
        module_events = module.get("events", {})
        event_iter = module_events.items() if isinstance(module_events, dict) else enumerate(module_events)
        for event_key_value, event in event_iter:
            if not isinstance(event, dict):
                continue
            event_id = parse_int(event.get("id", event_key_value))
            args = _normalize_args(event.get("args", []))
            events[(module_id, event_id)] = EventMetadata(
                module_id=module_id,
                event_id=event_id,
                module_name=module_name,
                event_name=str(event.get("name", f"EVENT_{event_id:04X}")),
                level=_upper_or_none(event.get("level")),
                text=event.get("text"),
                args=args,
                raw=event,
            )
    return events


def _normalize_nested_dictionary(dictionary: dict[str, Any]) -> dict[tuple[int, int], EventMetadata]:
    events: dict[tuple[int, int], EventMetadata] = {}
    for module_key, module in dictionary.items():
        if not isinstance(module, dict):
            continue
        module_id = parse_int(module_key)
        module_name = str(module.get("name", f"MODULE_{module_id:02X}"))
        for event_key_value, event in module.get("events", {}).items():
            if not isinstance(event, dict):
                continue
            event_id = parse_int(event_key_value)
            events[(module_id, event_id)] = EventMetadata(
                module_id=module_id,
                event_id=event_id,
                module_name=module_name,
                event_name=str(event.get("name", f"EVENT_{event_id:04X}")),
                level=_upper_or_none(event.get("level")),
                text=event.get("text"),
                args=_normalize_args(event.get("args", [])),
                raw=event,
            )
    return events


def _normalize_flat_events(flat_events: dict[str, Any]) -> dict[tuple[int, int], EventMetadata]:
    events: dict[tuple[int, int], EventMetadata] = {}
    for key, event in flat_events.items():
        if not isinstance(event, dict) or ":" not in key:
            continue
        module_text, event_text = key.split(":", 1)
        module_id = parse_int(module_text)
        event_id = parse_int(event_text)
        module_name = str(event.get("module", f"MODULE_{module_id:02X}"))
        events[(module_id, event_id)] = EventMetadata(
            module_id=module_id,
            event_id=event_id,
            module_name=module_name,
            event_name=str(event.get("name", f"EVENT_{event_id:04X}")),
            level=_upper_or_none(event.get("level")),
            text=event.get("text") or event.get("description"),
            args=_normalize_args(event.get("args", [])),
            raw=event,
        )
    return events


def _normalize_args(args: Any) -> list[dict[str, Any]]:
    normalized: list[dict[str, Any]] = []
    if not isinstance(args, list):
        return normalized
    for idx, arg in enumerate(args):
        if not isinstance(arg, dict):
            continue
        arg_type = str(arg.get("type", "unknown"))
        normalized.append(
            {
                **arg,
                "name": str(arg.get("name", f"arg{idx}")),
                "type": arg_type,
                "type_tag": arg.get("type_tag", TYPE_TAG_BY_NAME.get(arg_type)),
                "wire_size": arg.get("wire_size", WIRE_SIZE_BY_NAME.get(arg_type)),
            }
        )
    return normalized


def _upper_or_none(value: Any) -> str | None:
    if value is None:
        return None
    return str(value).upper()


def load_dictionary(path: str | Path | None) -> EventDictionary | None:
    """Load a JSON dictionary file, returning None when no path is provided."""
    if path is None:
        return None
    dict_path = Path(path)
    with dict_path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    return EventDictionary(data, dict_path)

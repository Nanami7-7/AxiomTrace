"""Extract a lightweight host dictionary from documented X-Macro lists."""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any

MODULE_PATTERN = re.compile(r"\bX\(\s*([A-Z][A-Z0-9_]*)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\)")
EVENT_PATTERN = re.compile(
    r"\bX\(\s*([A-Z][A-Z0-9_]*)\s*,\s*([A-Z][A-Z0-9_]*)\s*,\s*"
    r"(0x[0-9A-Fa-f]+|\d+)\s*,\s*(DEBUG|INFO|WARN|ERROR|FAULT)\s*,\s*(\d+)\s*\)"
)


def extract_xmacro_dictionary(source: str | Path) -> dict[str, Any]:
    """Extract IDs, names and levels; YAML remains authoritative for typed templates."""
    text = Path(source).read_text(encoding="utf-8")
    modules = {name: int(identifier, 0) for name, identifier in MODULE_PATTERN.findall(text)}
    output: dict[str, Any] = {"version": "1.0", "modules": {}}
    for name, identifier in modules.items():
        output["modules"][str(identifier)] = {"name": name, "description": "", "events": {}}
    for module, event, identifier, level, argc in EVENT_PATTERN.findall(text):
        if module not in modules:
            raise ValueError(f"event references unknown module: {module}")
        output["modules"][str(modules[module])]["events"][str(int(identifier, 0))] = {
            "name": event,
            "level": level,
            "text": "",
            "args": [],
            "argc": int(argc),
        }
    if not output["modules"] or not any(module["events"] for module in output["modules"].values()):
        raise ValueError("no AXIOM_MODULE_LIST/AXIOM_EVENT_LIST entries found")
    return output


def write_xmacro_dictionary(source: str | Path, output: str | Path) -> Path:
    destination = Path(output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(extract_xmacro_dictionary(source), indent=2), encoding="utf-8")
    return destination

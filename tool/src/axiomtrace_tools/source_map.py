"""Source map generation and loading for host-side source location lookup."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


def generate_source_map(
    compile_db: str | Path | None,
    project_root: str | Path | None = None,
    source_id_map: str | Path | None = None,
    location_mode: str | None = None,
) -> dict[str, Any]:
    """Generate a deterministic file-id source map from compile_commands.json."""
    root = Path(project_root or ".").resolve()
    files: dict[str, dict[str, Any]] = {}
    if source_id_map and Path(source_id_map).is_file():
        with Path(source_id_map).open("r", encoding="utf-8") as handle:
            source_ids = json.load(handle)
        for entry in source_ids.get("files", []):
            # Firmware HASH mode hashes the compiler's exact __FILE__ spelling.
            hash_input = str(entry.get("path", ""))
            path = _normalize_path(hash_input, root)
            file_id = str(int(entry["id"]))
            files[file_id] = {"path": path, "hash16": f"0x{_fnv1a16(hash_input):04X}"}
    elif compile_db:
        db_path = Path(compile_db)
        if db_path.is_file():
            with db_path.open("r", encoding="utf-8") as handle:
                entries = json.load(handle)
            source_paths = sorted(
                {
                    (_normalize_path(str(entry["file"]), root), str(entry["file"]))
                    for entry in entries
                    if entry.get("file")
                }
            )
            for idx, (path, hash_input) in enumerate(source_paths, start=1):
                files[str(idx)] = {"path": path, "hash16": f"0x{_fnv1a16(hash_input):04X}"}

    inferred_mode = "file_id" if files else "none"
    selected_mode = location_mode or inferred_mode
    if selected_mode not in {"none", "hash", "file_id"}:
        raise ValueError(f"unsupported location mode: {selected_mode}")
    if selected_mode == "file_id" and not files:
        raise ValueError("file_id location mode requires a source id map or compile database")
    return {
        "schema": "axiomtrace.source_map.v1",
        "location_mode": selected_mode,
        "path_base": "project",
        "files": files,
        "functions": [],
    }


def resolve_location(location: dict[str, Any] | None, source_map: dict[str, Any] | None) -> dict[str, Any] | None:
    """Enrich decoded location metadata from a loaded source map."""
    if not location:
        return None
    resolved = dict(location)
    if not source_map:
        return resolved
    files = source_map.get("files", {})
    match: dict[str, Any] | None = None
    if location.get("mode") == "file_id":
        match = files.get(str(location.get("file_id")))
    elif location.get("mode") == "hash":
        expected = f"0x{int(location.get('file_hash', 0)):04X}".upper()
        for candidate in files.values():
            if str(candidate.get("hash16", "")).upper() == expected:
                match = candidate
                break
    if match:
        resolved["file"] = match.get("path")
    return resolved


def load_source_map(path: str | Path | None) -> dict[str, Any] | None:
    """Load source_map.json if available."""
    if path is None:
        return None
    source_path = Path(path)
    if not source_path.is_file():
        return None
    with source_path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def _normalize_path(value: str | None, root: Path) -> str:
    if not value:
        return ""
    path = Path(value)
    if not path.is_absolute():
        path = (root / path).resolve()
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()


def _fnv1a16(text: str) -> int:
    h = 0x811C
    for byte in text.encode("utf-8"):
        h ^= byte
        h = (h * 0x0103) & 0xFFFF
    return h

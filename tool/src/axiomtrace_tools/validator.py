"""Validation helpers for AxiomTrace dictionaries, bundles, and golden frames."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from axiomtrace_tools.bundle import AxiomBundle, load_bundle
from axiomtrace_tools.decoder import decode_stream, extract_metadata_id
from axiomtrace_tools.dictionary import EventDictionary, load_dictionary
from axiomtrace_tools.metadata_id import metadata_id_for_data


def validate_dictionary_file(path: str | Path) -> EventDictionary:
    """Validate that a generated dictionary contains semantic event entries."""
    dictionary = load_dictionary(path)
    if dictionary is None or not dictionary.events:
        raise ValueError("dictionary contains no events")
    return dictionary


def validate_bundle(path: str | Path) -> AxiomBundle:
    """Validate bundle artifacts and their shared decode-metadata identity."""
    bundle = load_bundle(path)
    for key in ("dictionary", "source_map", "build_info"):
        artifact = bundle.artifact_path(key)
        if not artifact or not artifact.is_file():
            raise ValueError(f"bundle artifact missing: {key}")
    dictionary_path = bundle.artifact_path("dictionary")
    assert dictionary_path is not None
    validate_dictionary_file(dictionary_path)
    with dictionary_path.open("r", encoding="utf-8") as handle:
        dictionary_data = json.load(handle)
    source_map = bundle.load_source_map()
    if source_map is None:
        raise ValueError("bundle source map could not be loaded")
    location = bundle.manifest.get("location", {})
    expected_id = metadata_id_for_data(dictionary_data, source_map, location)
    if bundle.metadata_id != expected_id:
        raise ValueError("bundle metadata identity does not match decode artifacts")
    if bundle.manifest.get("metadata", {}).get("wire_version") != "1.1":
        raise ValueError("bundle wire_version must be 1.1")
    if location.get("mode") == "file_id" and not source_map.get("files"):
        raise ValueError("file_id bundle must contain source-map file entries")
    with bundle.artifact_path("build_info").open("r", encoding="utf-8") as handle:  # type: ignore[union-attr]
        build_info = json.load(handle)
    if build_info.get("metadata_id") != expected_id:
        raise ValueError("build_info metadata identity mismatch")
    return bundle


def validate_trace_bundle(frames: list[dict[str, Any]], bundle: AxiomBundle) -> None:
    """Require an identity-matched trace before applying bundle semantics."""
    if not frames or any("error" in frame for frame in frames):
        raise ValueError("trace contains undecodable frames")
    trace_id = extract_metadata_id(frames)
    if not trace_id:
        raise ValueError("semantic decode requires a trace metadata identity event")
    if trace_id != bundle.metadata_id:
        raise ValueError("trace metadata identity does not match bundle")

    dictionary = bundle.load_dictionary()
    if dictionary is None:
        raise ValueError("bundle dictionary could not be loaded")
    source_map = bundle.load_source_map()
    for frame in frames:
        metadata = dictionary.find_event(int(frame.get("module_id", -1)), int(frame.get("event_id", -1)))
        if metadata:
            actual = [item.get("type") for item in frame.get("payload", [])]
            expected = [item.get("type") for item in metadata.args]
            if actual != expected:
                raise ValueError(
                    f"payload type mismatch for {metadata.module_name}.{metadata.event_name}: "
                    f"expected {expected}, got {actual}"
                )
        location = frame.get("location")
        if location and location.get("mode") == "file_id":
            if source_map is None or str(location.get("file_id")) not in source_map.get("files", {}):
                raise ValueError(f"source map does not contain file id {location.get('file_id')}")


def validate_golden(path: str | Path) -> list[Path]:
    """Decode every golden frame and compare an available raw JSON expectation."""
    root = Path(path)
    frame_dir = root / "frames" if (root / "frames").is_dir() else root
    expected_dir = root / "expected" if (root / "expected").is_dir() else root.parent / "expected"
    frames = sorted(frame_dir.glob("*.bin"))
    if not frames:
        raise ValueError("golden directory contains no .bin frames")
    checked: list[Path] = []
    for frame_path in frames:
        decoded = decode_stream(frame_path.read_bytes())
        if not decoded or any("error" in frame for frame in decoded):
            raise ValueError(f"golden frame fails decode: {frame_path.name}")
        expected_path = expected_dir / f"{frame_path.stem}.json"
        if expected_path.is_file():
            expected: Any = json.loads(expected_path.read_text(encoding="utf-8"))
            actual = json.loads(json.dumps(decoded))
            if expected != actual:
                raise ValueError(f"golden output mismatch: {frame_path.name}")
        checked.append(frame_path)
    return checked

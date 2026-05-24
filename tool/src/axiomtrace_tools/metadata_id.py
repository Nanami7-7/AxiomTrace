"""Deterministic metadata identity generation for trace decoding bundles."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any


WIRE_VERSION = "1.1"
METADATA_ID_HEX_LEN = 16


def location_contract(mode: str = "none", function_hash: bool = False) -> dict[str, Any]:
    """Return the location fields that affect on-wire metadata decoding."""
    if mode not in {"none", "hash", "file_id"}:
        raise ValueError(f"unsupported location mode: {mode}")
    return {
        "mode": mode,
        "line_width_bits": 16,
        "function_hash": bool(function_hash and mode == "hash"),
    }


def metadata_id_for_data(
    dictionary: dict[str, Any],
    source_map: dict[str, Any],
    location: dict[str, Any],
    wire_version: str = WIRE_VERSION,
) -> str:
    """Hash the host-side data required to safely interpret a trace."""
    identity_input = {
        "schema": "axiomtrace.metadata_identity.v1",
        "wire_version": wire_version,
        "location": location,
        "dictionary": dictionary,
        "source_map": source_map,
    }
    encoded = json.dumps(identity_input, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()[:METADATA_ID_HEX_LEN]


def hash_file(path: str | Path) -> str:
    """Return the SHA-256 digest of an optional diagnostic artifact."""
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()

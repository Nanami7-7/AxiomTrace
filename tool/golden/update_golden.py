#!/usr/bin/env python3
"""Update or verify wire-format golden files emitted by the C encoder."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

GOLDEN_DIR = Path(__file__).parent / "frames"
EXPECTED_DIR = Path(__file__).parent / "expected"
FRAME_NAMES = [
    "frame_01_minimal",
    "frame_02_u16",
    "frame_03_metadata_identity",
    "frame_04_location_file_id",
    "frame_05_system_probe",
]


def run() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--encoder", required=True, type=Path, help="Path to the built generate_golden executable.")
    parser.add_argument("--check", action="store_true", help="Fail rather than rewriting stale tracked vectors.")
    args = parser.parse_args()

    sys.path.insert(0, str(Path(__file__).parent.parent / "src"))
    from axiomtrace_tools.decoder import decode_stream
    from axiomtrace_tools.dictionary import load_dictionary

    dictionary = load_dictionary(Path(__file__).parent / "dictionary.json")

    with tempfile.TemporaryDirectory() as temporary:
        emitted_dir = Path(temporary)
        subprocess.run([str(args.encoder), str(emitted_dir)], check=True)
        generated: dict[str, tuple[bytes, str]] = {}
        for name in FRAME_NAMES:
            frame = (emitted_dir / f"{name}.bin").read_bytes()
            decoded = decode_stream(frame, dictionary)
            if not decoded or any("error" in entry for entry in decoded):
                raise RuntimeError(f"encoder emitted an undecodable golden frame: {name}")
            generated[name] = (frame, json.dumps(decoded, indent=2) + "\n")

    GOLDEN_DIR.mkdir(exist_ok=True)
    EXPECTED_DIR.mkdir(exist_ok=True)
    stale: list[str] = []
    for name, (frame, expected) in generated.items():
        frame_path = GOLDEN_DIR / f"{name}.bin"
        expected_path = EXPECTED_DIR / f"{name}.json"
        if args.check:
            if not frame_path.is_file() or frame_path.read_bytes() != frame:
                stale.append(str(frame_path))
            if not expected_path.is_file() or expected_path.read_text(encoding="utf-8") != expected:
                stale.append(str(expected_path))
        else:
            frame_path.write_bytes(frame)
            expected_path.write_text(expected, encoding="utf-8")
            print(f"Updated {name}.bin + {name}.json")
    if stale:
        print("Stale golden files:", file=sys.stderr)
        for path in stale:
            print(f"  {path}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(run())

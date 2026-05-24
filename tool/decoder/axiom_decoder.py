#!/usr/bin/env python3
"""Deprecated standalone compatibility entry point for AxiomTrace decoding.

Use the installed ``axiom-decoder`` command for bundle-backed semantic output.
This script retains the legacy structural ``--output text|json`` interface but
delegates all wire parsing to ``axiomtrace_tools.decoder``.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from axiomtrace_tools.decoder import decode_stream


def main() -> None:
    parser = argparse.ArgumentParser(description="AxiomTrace legacy structural decoder")
    parser.add_argument("input", help="Binary file to decode")
    parser.add_argument("--output", choices=["text", "json"], default="text")
    args = parser.parse_args()

    frames = decode_stream(Path(args.input).read_bytes())
    if args.output == "json":
        print(json.dumps(frames, indent=2))
        return
    for frame in frames:
        if "error" in frame:
            print(f"[ERROR: {frame['error']}]")
            continue
        print(
            f"[{frame['seq']:4d}] [{frame['level']:5s}] "
            f"mod=0x{frame['module_id']:02X} evt=0x{frame['event_id']:04X} "
            f"payload={frame['payload']}"
        )


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Summarize linked archive sections and compiler stack-usage files."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path

RAM_BUDGETS = {"tiny": 512, "prod": 1536, "field": 4608, "dev": 9216}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--size-tool", default="size")
    parser.add_argument("--preset", choices=["tiny", "prod", "field", "dev"], required=True)
    parser.add_argument("--enforce", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    archives = list(args.build_dir.rglob("libaxiomtrace.a"))
    if not archives:
        raise SystemExit(f"libaxiomtrace.a not found under {args.build_dir}")
    size_output = subprocess.check_output(
        [args.size_tool, "-A", str(archives[0])], text=True, errors="replace"
    )
    sections: dict[str, int] = {}
    for line in size_output.splitlines():
        match = re.match(r"\s*(\.[A-Za-z0-9_.$-]+)\s+(\d+)\s+", line)
        if match:
            sections[match.group(1)] = sections.get(match.group(1), 0) + int(match.group(2))

    ram = sum(
        value for name, value in sections.items()
        if name.startswith((".bss", ".data", ".sbss", ".sdata"))
    )
    flash = sum(
        value for name, value in sections.items()
        if name.startswith((".text", ".rodata", ".data", ".sdata"))
    )
    stack_entries: list[tuple[int, str]] = []
    for usage_file in args.build_dir.rglob("*.su"):
        for line in usage_file.read_text(encoding="utf-8", errors="replace").splitlines():
            fields = line.rsplit("\t", 2)
            if len(fields) >= 2 and fields[-2].isdigit():
                stack_entries.append((int(fields[-2]), fields[0]))
    stack_bytes, stack_symbol = max(stack_entries, default=(0, "unavailable"))

    report = {
        "preset": args.preset,
        "flash_bytes": flash,
        "ram_bytes": ram,
        "ram_budget_bytes": RAM_BUDGETS[args.preset],
        "max_static_stack_bytes": stack_bytes,
        "max_static_stack_symbol": stack_symbol,
    }
    rendered = json.dumps(report, indent=2)
    print(rendered)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered + "\n", encoding="utf-8")
    if args.enforce and ram > RAM_BUDGETS[args.preset]:
        print(f"RAM budget exceeded: {ram} > {RAM_BUDGETS[args.preset]}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

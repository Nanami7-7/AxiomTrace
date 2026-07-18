#!/usr/bin/env python3
"""Release version consistency and local Markdown-link checks."""

from __future__ import annotations

import re
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = "1.0.0"


def config_version() -> str:
    text = (ROOT / "baremetal" / "axiom_config.h").read_text(encoding="utf-8")
    values = []
    for name in ("MAJOR", "MINOR", "PATCH"):
        match = re.search(rf"AXIOMTRACE_VERSION_{name}\s+(\d+)u", text)
        if not match:
            raise AssertionError(f"missing C version component {name}")
        values.append(match.group(1))
    return ".".join(values)


def check_links(path: Path) -> list[str]:
    failures = []
    text = path.read_text(encoding="utf-8")
    for target in re.findall(r"\[[^]]+\]\(([^)]+)\)", text):
        target = target.split("#", 1)[0]
        if not target or "://" in target or target.startswith("mailto:"):
            continue
        if not (path.parent / target).resolve().exists():
            failures.append(f"{path.relative_to(ROOT)} -> {target}")
    return failures


def main() -> int:
    pyproject = tomllib.loads((ROOT / "tool" / "pyproject.toml").read_text(encoding="utf-8"))
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    cmake_match = re.search(r"project\(AxiomTrace VERSION ([0-9.]+)", cmake)
    versions = {
        "C header": config_version(),
        "CMake": cmake_match.group(1) if cmake_match else "missing",
        "Python": pyproject["project"]["version"],
    }
    failures = [f"{name}: {version}" for name, version in versions.items() if version != EXPECTED]
    expected_components = dict(zip(("MAJOR", "MINOR", "PATCH"), EXPECTED.split(".")))
    for path in (ROOT / "spec" / "api_reference.md", ROOT / "spec" / "api_reference_zh.md"):
        text = path.read_text(encoding="utf-8")
        for name, expected in expected_components.items():
            match = re.search(
                rf"\|\s*`AXIOMTRACE_VERSION_{name}`\s*\|\s*(\d+)\s*\|",
                text,
            )
            if not match or match.group(1) != expected:
                actual = match.group(1) if match else "missing"
                failures.append(
                    f"{path.relative_to(ROOT)} AXIOMTRACE_VERSION_{name}: {actual}"
                )
    markdown_paths = {
        ROOT / "README.md",
        ROOT / "README_zh.md",
        *ROOT.joinpath("spec").rglob("*.md"),
        *ROOT.joinpath("docs").rglob("*.md"),
        *ROOT.joinpath("baremetal", "ports").rglob("*.md"),
    }
    for path in sorted(markdown_paths):
        failures.extend(check_links(path))
    if failures:
        print("Release checks FAILED:")
        print("\n".join(f"- {failure}" for failure in failures))
        return 1
    print(f"Release checks PASSED: {EXPECTED}; local documentation links valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

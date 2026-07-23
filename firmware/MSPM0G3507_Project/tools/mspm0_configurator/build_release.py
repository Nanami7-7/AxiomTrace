#!/usr/bin/env python3
"""
Build script for mspm0-configurator standalone executable.

Produces a standalone .exe (Windows) or executable (Linux/macOS) in
tools/release/ using PyInstaller.

Usage:
    python build_release.py          # build for current platform
    python build_release.py --clean  # clean build artifacts before building

Requirements:
    - Python 3.10+
    - Build dependencies: python -m pip install -e ".[build]"
    - PySide6 and pyserial (installed from pyproject.toml)

The output is placed in ../release/ relative to this script, which maps to
firmware/MSPM0G3507_Project/tools/release/.
"""
from __future__ import annotations

import argparse
import platform
import shutil
import subprocess
import sys
from pathlib import Path


def get_project_root() -> Path:
    """Return the mspm0_configurator project root (containing pyproject.toml)."""
    return Path(__file__).resolve().parent


def get_release_dir() -> Path:
    """Return the release output directory (tools/release/)."""
    return get_project_root().parent / "release"


def check_build_deps() -> None:
    """Fail clearly instead of mutating the active Python environment."""
    try:
        import PyInstaller  # noqa: F401
    except ImportError:
        raise RuntimeError(
            'PyInstaller is missing; run: python -m pip install -e ".[build]"'
        )


def clean_build_dirs() -> None:
    """Clean build and dist directories."""
    root = get_project_root()
    for d in [root / "build", root / "dist"]:
        if d.exists():
            print(f"Cleaning {d}...")
            shutil.rmtree(d, ignore_errors=True)


def build_executable() -> Path:
    """Build the standalone executable using PyInstaller."""
    root = get_project_root()
    release_dir = get_release_dir()
    release_dir.mkdir(parents=True, exist_ok=True)

    system = platform.system().lower()
    if system == "windows":
        exe_name = "mspm0-configurator.exe"
    else:
        exe_name = "mspm0-configurator"

    entry_script = root / "src" / "mspm0_configurator" / "__main__.py"

    print(f"Building {exe_name} with PyInstaller...")
    print(f"  Project root: {root}")
    print(f"  Entry script: {entry_script}")
    print(f"  Output dir:   {release_dir}")

    cmd = [
        sys.executable,
        "-m",
        "PyInstaller",
        "--noconfirm",
        "--clean",
        "--name",
        "mspm0-configurator",
        "--windowed",  # GUI application, no console
        "--onedir",    # One-dir mode for faster startup and smaller size
        "--distpath",
        str(release_dir),
        "--workpath",
        str(root / "build"),
        "--specpath",
        str(root / "build"),
        str(entry_script),
    ]

    print(f"  Command: {' '.join(cmd)}")
    subprocess.check_call(cmd)

    # The onedir output is at release/mspm0-configurator/
    onedir = release_dir / "mspm0-configurator"
    if onedir.exists():
        print(f"Build successful: {onedir}")
        return onedir

    # Fallback: onefile mode
    exe_path = release_dir / exe_name
    if exe_path.exists():
        print(f"Build successful: {exe_path}")
        return exe_path

    raise RuntimeError("Build failed: output not found")


def write_release_info(output_dir: Path) -> None:
    """Write a RELEASE_INFO.txt with build metadata."""
    info = output_dir.parent / "RELEASE_INFO.txt"
    import datetime

    content = f"""MSPM0 Configurator - Release Build
===================================

Build timestamp: {datetime.datetime.now().isoformat()}
Platform: {platform.platform()}
Python: {sys.version}
Builder: local build_release.py

Usage:
  Windows:  mspm0-configurator\\mspm0-configurator.exe
  Linux:    ./mspm0-configurator/mspm0-configurator
  macOS:    ./mspm0-configurator/mspm0-configurator

Connect at 115200 8-N-1. See README.md for safety instructions.
"""
    info.write_text(content, encoding="utf-8")
    print(f"Release info written to {info}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build mspm0-configurator standalone executable."
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Clean build artifacts before building.",
    )
    args = parser.parse_args()

    try:
        check_build_deps()
        if args.clean:
            clean_build_dirs()
        output = build_executable()
        write_release_info(output)
        print("\n=== Build Complete ===")
        print(f"Output: {output}")
        return 0
    except subprocess.CalledProcessError as e:
        print(f"Build failed: {e}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

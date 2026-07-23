#!/usr/bin/env python3
"""
Git pre-commit hook: validate README / documentation link integrity.

Checks:
  1. All [text](relative-path) links in README.md / README_zh.md resolve to existing files
  2. All [text](relative-path) links in docs/**/*.md and spec/**/*.md resolve to existing files
  3. Warns when staged docs/ or spec/ files are not referenced by any README

Usage:
  - Standalone:  python tool/scripts/check_links.py [--all]
  - Git hook:    install as .git/hooks/pre-commit (or use core.hooksPath)
                 runs on staged .md files only (--staged mode, default)

Exit codes:
  0 = all checks passed
  1 = broken links found
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path
from typing import Iterable

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
MARKDOWN_LINK_RE = re.compile(r"(?<!!)\[([^\]]*)\]\(([^)]+)\)")

SKIP_PREFIXES = ("http://", "https://", "mailto:", "ftp://")

# Directories containing documentation relative to repo root
DOC_GLOBS = ["docs/**/*.md", "spec/**/*.md"]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def repo_root() -> Path:
    """Return the git repository root."""
    out = subprocess.check_output(
        ["git", "rev-parse", "--show-toplevel"],
        encoding="utf-8",
        stderr=subprocess.DEVNULL,
    )
    return Path(out.strip())


def staged_md_files() -> list[Path]:
    """Return list of staged .md files (added, copied, modified)."""
    out = subprocess.check_output(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR", "--", "*.md"],
        encoding="utf-8",
        stderr=subprocess.DEVNULL,
    )
    return [Path(f) for f in out.splitlines() if f]


def extract_links(filepath: Path) -> list[tuple[str, str, int]]:
    """Extract (link_text, link_target, line_number) from a markdown file."""
    results = []
    with open(filepath, encoding="utf-8") as f:
        for lineno, line in enumerate(f, start=1):
            for m in MARKDOWN_LINK_RE.finditer(line):
                results.append((m.group(1), m.group(2), lineno))
    return results


def resolve_link(target: str, source_file: Path) -> Path | None:
    """Resolve a relative link target against the source file's directory.

    Returns the resolved absolute Path, or None if the target is an
    anchor, URL, or otherwise not a local relative path.
    """
    # Strip anchors
    if "#" in target:
        target = target.split("#")[0]
    if not target:
        return None
    # Skip absolute URLs
    if any(target.startswith(p) for p in SKIP_PREFIXES):
        return None
    # Resolve relative to the markdown file's directory
    return (source_file.parent / target).resolve()


# ---------------------------------------------------------------------------
# Core checks
# ---------------------------------------------------------------------------
def check_link_integrity(
    files: Iterable[Path],
    root: Path,
) -> list[dict]:
    """Check every relative link in *files* resolves to an existing file."""
    errors: list[dict] = []
    for filepath in files:
        if not filepath.exists():
            continue
        for text, target, lineno in extract_links(filepath):
            resolved = resolve_link(target, filepath)
            if resolved is None:
                continue
            if not resolved.exists():
                errors.append(
                    {
                        "file": str(filepath.relative_to(root)),
                        "line": lineno,
                        "link_text": text,
                        "link_target": target,
                        "resolved": str(resolved),
                    }
                )
    return errors


def check_doc_referenced(
    doc_files: Iterable[Path],
    readme_files: Iterable[Path],
    root: Path,
) -> list[str]:
    """Warn about doc files not linked from any README."""
    # Collect all link targets from READMEs
    readme_targets: set[str] = set()
    for readme in readme_files:
        if not readme.exists():
            continue
        for _text, target, _ in extract_links(readme):
            if "#" in target:
                target = target.split("#")[0]
            if target and not any(target.startswith(p) for p in SKIP_PREFIXES):
                resolved = resolve_link(target, readme)
                if resolved:
                    readme_targets.add(str(resolved))

    unreferenced = []
    for doc in doc_files:
        if not doc.exists():
            continue
        if str(doc.resolve()) not in readme_targets:
            unreferenced.append(str(doc.relative_to(root)))
    return unreferenced


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main() -> int:
    parser = argparse.ArgumentParser(description="Validate markdown link integrity")
    parser.add_argument(
        "--all",
        action="store_true",
        help="Check all doc files (default: only staged files)",
    )
    parser.add_argument(
        "--staged",
        action="store_true",
        default=True,
        help="Check only staged .md files (default when used as git hook)",
    )
    args = parser.parse_args()

    root = repo_root()

    if args.all:
        # Check all README + doc files
        readme_files = [root / "README.md", root / "README_zh.md"]
        doc_files = []
        for pattern in DOC_GLOBS:
            doc_files.extend(root.glob(pattern))
        files_to_check = readme_files + doc_files
    else:
        # Staged-only mode (hook): check staged README + any staged doc files
        staged = staged_md_files()
        readme_files = [f for f in staged if f.name.startswith("README")]
        doc_files = [
            f
            for f in staged
            if any(
                f.match(p.replace("*.md", "*")) for p in DOC_GLOBS
            )
            or str(f).startswith("docs/")
            or str(f).startswith("spec/")
        ]
        files_to_check = readme_files + doc_files

        if not files_to_check:
            return 0  # No markdown files staged, nothing to check

    # --- Link integrity ---
    errors = check_link_integrity(files_to_check, root)
    if errors:
        print("\n[check_links] BROKEN LINKS found:\n")
        for e in errors:
            print(
                f"  {e['file']}:{e['line']}  "
                f"[{e['link_text']}]({e['link_target']})"
                f"  -> {e['resolved']}"
            )
        print(f"\n  {len(errors)} broken link(s) detected.\n")
        return 1

    # --- Unreferenced docs (advisory, non-blocking) ---
    if args.all:
        all_doc_files = []
        for pattern in DOC_GLOBS:
            all_doc_files.extend(root.glob(pattern))
        unreferenced = check_doc_referenced(
            all_doc_files,
            [root / "README.md", root / "README_zh.md"],
            root,
        )
        if unreferenced:
            print("\n[check_links] Advisory: doc files not linked from any README:\n")
            for f in unreferenced:
                print(f"  {f}")
            print()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

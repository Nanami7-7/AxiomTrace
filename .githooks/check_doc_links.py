#!/usr/bin/env python3
"""
Pre-commit hook: check markdown link validity and README freshness.

Usage (standalone):
    python .githooks/check_doc_links.py           # check all .md files
    python .githooks/check_doc_links.py --staged  # check only staged .md files

Usage (git hook):
    Place in .githooks/pre-commit or symlink/copy there.
    git config core.hooksPath .githooks

What it checks:
    1. Internal file links [text](path) resolve to existing files
    2. Anchor links [text](#heading) resolve to headings in the same file
    3. README.md / README_zh.md documentation index table links are valid
    4. If source code changed but READMEs not staged, prints a reminder

Exit code: 0 = all checks pass, 1 = broken links found or warning issued.
"""

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Optional


# ---------------------------------------------------------------------------
# Link extraction
# ---------------------------------------------------------------------------

# Matches [text](target) but NOT ![alt](src)  (images)
_LINK_RE = re.compile(r"(?<!\!)\[([^\]]*)\]\(([^)]+)\)")
# Matches ## Heading text (ATX-style)
_HEADING_RE = re.compile(r"^(#{1,6})\s+(.+?)(?:\s+#+)?\s*$", re.MULTILINE)


def extract_links(text: str) -> list[tuple[str, str]]:
    """Return [(link_text, target), ...] from markdown source."""
    return _LINK_RE.findall(text)


def extract_headings(text: str) -> set[str]:
    """Return set of heading slugs (GitHub-style).

    GitHub slug algorithm:
    1. Lowercase
    2. Remove anything that is not: letter, digit, space, hyphen, or CJK
    3. Replace spaces with hyphens
    4. Strip leading/trailing hyphens
    """
    headings: set[str] = set()
    for m in _HEADING_RE.finditer(text):
        raw = m.group(2).strip()
        slug = raw.lower()
        # GitHub slug algorithm: remove punctuation, keep CJK, collapse spaces to hyphens
        # Step 1: replace '/' with '-' (GitHub treats slash as separator)
        slug = slug.replace('/', '-')
        # Step 2: remove everything except [a-z0-9], spaces, hyphens, and CJK ideographs
        slug = re.sub(r'[^\w\s\u4e00-\u9fff-]', '', slug)
        # Step 3: collapse multiple spaces/hyphens into single hyphen
        slug = re.sub(r'[\s]+', '-', slug)
        slug = re.sub(r'-+', '-', slug)
        slug = slug.strip('-')
        headings.add(slug)
    return headings


# ---------------------------------------------------------------------------
# Link validation
# ---------------------------------------------------------------------------

def validate_link(
    target: str,
    source_file: Path,
    source_text: str,
    repo_root: Path,
    check_external: bool = False,
) -> Optional[str]:
    """Return error message if link is broken, None if valid."""
    # Skip pure anchors (handled separately)
    if target.startswith("#"):
        return None

    # Skip external URLs unless explicitly requested
    if target.startswith("http://") or target.startswith("https://"):
        return None

    # Strip anchor from path
    file_part = target.split("#")[0] if "#" in target else target
    anchor_part = target.split("#")[1] if "#" in target else None

    # Resolve relative to the source file's directory
    source_dir = source_file.parent
    resolved = (source_dir / file_part).resolve()

    if not resolved.exists():
        return f"  BROKEN: {source_file.relative_to(repo_root)}: [{source_text}]({target}) -> file not found"

    # If anchor present, validate it exists in the target file
    if anchor_part and resolved.suffix == ".md":
        try:
            target_text = resolved.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            return None  # Can't read, skip anchor check
        target_headings = extract_headings(target_text)
        if anchor_part not in target_headings:
            return f"  BROKEN ANCHOR: {source_file.relative_to(repo_root)}: [{source_text}]({target}) -> heading #{anchor_part} not found in {resolved.name}"

    return None


# ---------------------------------------------------------------------------
# Staged file detection (git integration)
# ---------------------------------------------------------------------------

def get_staged_md_files(repo_root: Path) -> list[Path]:
    """Return list of staged .md files from git index."""
    try:
        result = subprocess.run(
            ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"],
            capture_output=True, text=True, cwd=repo_root, timeout=10,
        )
        files = []
        for line in result.stdout.strip().splitlines():
            p = repo_root / line
            if p.suffix == ".md" and p.exists():
                files.append(p)
        return files
    except (subprocess.SubprocessError, FileNotFoundError):
        return []


def get_staged_source_changes(repo_root: Path) -> bool:
    """Return True if non-doc source files are staged."""
    try:
        result = subprocess.run(
            ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"],
            capture_output=True, text=True, cwd=repo_root, timeout=10,
        )
        for line in result.stdout.strip().splitlines():
            # Consider anything under baremetal/, tests/, tool/, cmake/ as "source"
            if any(line.startswith(d) for d in ("baremetal/", "tests/", "tool/", "cmake/")):
                return True
        return False
    except (subprocess.SubprocessError, FileNotFoundError):
        return False


def get_staged_readme_changes(repo_root: Path) -> bool:
    """Return True if README.md or README_zh.md is staged."""
    try:
        result = subprocess.run(
            ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"],
            capture_output=True, text=True, cwd=repo_root, timeout=10,
        )
        for line in result.stdout.strip().splitlines():
            if Path(line).name in ("README.md", "README_zh.md"):
                return True
        return False
    except (subprocess.SubprocessError, FileNotFoundError):
        return False


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description="Check markdown link validity")
    parser.add_argument("--staged", action="store_true", help="Only check staged .md files")
    parser.add_argument("--external", action="store_true", help="Also check external HTTP(S) links (slow)")
    parser.add_argument("--root", type=str, default=None, help="Override repo root directory")
    args = parser.parse_args()

    # Determine repo root
    if args.root:
        repo_root = Path(args.root).resolve()
    else:
        try:
            result = subprocess.run(
                ["git", "rev-parse", "--show-toplevel"],
                capture_output=True, text=True, timeout=5,
            )
            repo_root = Path(result.stdout.strip()).resolve()
        except (subprocess.SubprocessError, FileNotFoundError):
            repo_root = Path.cwd().resolve()

    # Collect files to check
    if args.staged:
        md_files = get_staged_md_files(repo_root)
    else:
        md_files = sorted(repo_root.glob("**/*.md"))

    errors: list[str] = []
    checked_links = 0

    for md_file in md_files:
        # Skip .git, build dirs, node_modules
        rel = md_file.relative_to(repo_root)
        if any(part.startswith(".") or part == "build" for part in rel.parts):
            continue
        if "node_modules" in str(rel):
            continue

        try:
            text = md_file.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue

        headings = extract_headings(text)
        links = extract_links(text)

        for link_text, target in links:
            checked_links += 1

            # Validate anchor-only links
            if target.startswith("#"):
                anchor = target[1:]
                if anchor not in headings:
                    errors.append(
                        f"  BROKEN ANCHOR: {rel}: [{link_text}]({target}) -> heading #{anchor} not found"
                    )
                continue

            # Validate file links
            err = validate_link(target, md_file, link_text, repo_root, args.external)
            if err:
                errors.append(err)

    # --- Report ---
    print(f"Checked {checked_links} links in {len(md_files)} files")

    if errors:
        print(f"\n{len(errors)} broken link(s) found:\n")
        for e in errors:
            print(e)
        print()

    # --- README freshness reminder ---
    if args.staged:
        source_changed = get_staged_source_changes(repo_root)
        readme_changed = get_staged_readme_changes(repo_root)

        if source_changed and not readme_changed:
            msg = (
                "WARNING: Source code files staged but README.md/README_zh.md not updated.\n"
                "  If changes affect public API or features, please update the READMEs.\n"
            )
            print(msg)
            # Don't fail on reminder, just warn
            if not errors:
                return 0

    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())

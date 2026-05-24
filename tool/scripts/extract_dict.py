#!/usr/bin/env python3
"""Extract dictionary.json from AxiomTrace X-Macro event declarations."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "src"))

from axiomtrace_tools.extractor import write_xmacro_dictionary


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    write_xmacro_dictionary(args.source, args.out)


if __name__ == "__main__":
    main()

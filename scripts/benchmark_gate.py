#!/usr/bin/env python3
"""Compare Host benchmark P99.9 values on the same machine."""

from __future__ import annotations

import argparse
import json
import re
import statistics
import subprocess
from pathlib import Path

SUMMARY_ROW = re.compile(
    r"^(B\d+:[^\s]+)\s+[-+0-9.]+\s+[-+0-9.]+\s+([-+0-9.]+)\s+",
    re.MULTILINE,
)


def parse_summary(output: str) -> dict[str, float]:
    values = {name: float(p999) for name, p999 in SUMMARY_ROW.findall(output)}
    if not values:
        raise ValueError("benchmark summary contains no P99.9 scenario rows")
    return values


def _run(executable: Path) -> dict[str, float]:
    result = subprocess.run(
        [str(executable)],
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    return parse_summary(result.stdout)


def measure_pair(
    baseline_executable: Path, candidate_executable: Path, runs: int
) -> tuple[dict[str, float], dict[str, float]]:
    baseline_samples: dict[str, list[float]] = {}
    candidate_samples: dict[str, list[float]] = {}
    expected: set[str] | None = None
    for index in range(runs):
        # Interleave the executables and reverse their order every round so
        # scheduler/thermal drift does not consistently penalize one side.
        if index % 2 == 0:
            baseline = _run(baseline_executable)
            candidate = _run(candidate_executable)
        else:
            candidate = _run(candidate_executable)
            baseline = _run(baseline_executable)
        if set(baseline) != set(candidate):
            raise ValueError("baseline and candidate benchmark scenarios differ")
        if expected is None:
            expected = set(baseline)
        elif set(baseline) != expected:
            raise ValueError("benchmark scenario set changed between runs")
        for name in baseline:
            baseline_samples.setdefault(name, []).append(baseline[name])
            candidate_samples.setdefault(name, []).append(candidate[name])
    return (
        {name: statistics.median(values) for name, values in baseline_samples.items()},
        {name: statistics.median(values) for name, values in candidate_samples.items()},
    )


def regressions(
    baseline: dict[str, float], candidate: dict[str, float], threshold_percent: float
) -> list[str]:
    if set(baseline) != set(candidate):
        missing = sorted(set(baseline) - set(candidate))
        added = sorted(set(candidate) - set(baseline))
        return [f"scenario mismatch; missing={missing}, added={added}"]
    factor = 1.0 + threshold_percent / 100.0
    failures = []
    for name in sorted(baseline):
        limit = baseline[name] * factor
        if candidate[name] > limit:
            failures.append(
                f"{name}: {candidate[name]:.3f} us > {limit:.3f} us "
                f"(baseline {baseline[name]:.3f} us, +{threshold_percent:.1f}%)"
            )
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=11)
    parser.add_argument("--threshold-percent", type=float, default=20.0)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.runs < 1 or args.threshold_percent < 0:
        parser.error("--runs must be positive and --threshold-percent non-negative")
    for executable in (args.baseline, args.candidate):
        if not executable.is_file():
            parser.error(f"benchmark executable not found: {executable}")

    baseline, candidate = measure_pair(
        args.baseline.resolve(), args.candidate.resolve(), args.runs
    )
    failures = regressions(baseline, candidate, args.threshold_percent)
    report = {
        "runs": args.runs,
        "threshold_percent": args.threshold_percent,
        "baseline_p999_us": baseline,
        "candidate_p999_us": candidate,
        "regressions": failures,
    }
    rendered = json.dumps(report, indent=2, sort_keys=True)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered + "\n", encoding="utf-8")
    print(rendered)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())

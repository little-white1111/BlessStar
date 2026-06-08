#!/usr/bin/env python3
"""Repeat parallel regression runs to surface flaky CTest cases (local CI aid)."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[3]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=_REPO / "build_ci_test",
        help="CMake build directory (default: build_ci_test)",
    )
    parser.add_argument("-C", "--config", default="Release", help="Multi-config generator config")
    parser.add_argument("-j", "--parallel", type=int, default=1, help="CTest parallel jobs (default 1: attach_integration lock)")
    parser.add_argument(
        "-n", "--iterations", type=int, default=20, help="Number of regression sweeps"
    )
    parser.add_argument(
        "--exclude-day17",
        action="store_true",
        help="Match contract GATE-REGRESSION (-LE day17)",
    )
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    if not (build_dir / "CTestTestfile.cmake").is_file():
        print(f"[FAIL] not a CTest build dir: {build_dir}", file=sys.stderr)
        return 2

    label_args = ["-L", "regression"]
    if args.exclude_day17:
        label_args.extend(["-LE", "day17"])

    for i in range(1, args.iterations + 1):
        if sys.platform == "win32":
            prep = _REPO / "tools" / "test" / "stop_stale_ctest.ps1"
            if prep.is_file():
                subprocess.run(
                    ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(prep)],
                    cwd=_REPO,
                    check=False,
                )
        cmd = [
            "ctest",
            "--test-dir",
            str(build_dir),
            "-C",
            args.config,
            *label_args,
            "--output-on-failure",
            "-j",
            str(args.parallel),
        ]
        print(f"[run {i}/{args.iterations}] {' '.join(cmd)}")
        proc = subprocess.run(cmd, cwd=_REPO)
        if proc.returncode != 0:
            print(f"[FAIL] iteration {i} exit {proc.returncode}", file=sys.stderr)
            return proc.returncode

    print(f"[OK] {args.iterations} regression sweep(s) passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

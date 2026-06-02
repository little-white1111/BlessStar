#!/usr/bin/env python3
"""L2 Staging acceptance runner (C-TST-L2-1 / GATE-STAGING-ACCEPTANCE)."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[3]


def _run_ctest(build_dir: Path, config: str, test: str | None, label: str | None) -> int:
    cmd = ["ctest", "--test-dir", str(build_dir), "-C", config, "--output-on-failure"]
    if test:
        cmd.extend(["-R", test])
    elif label:
        cmd.extend(["-L", label])
    else:
        print("[FAIL] ctest scenario needs test or label", file=sys.stderr)
        return 2
    print("[RUN]", " ".join(cmd))
    return subprocess.call(cmd, cwd=_REPO)


def _run_shell(command: str, dry_run: bool) -> int:
    print("[RUN shell]", command)
    if dry_run:
        return 0
    return subprocess.call(command, shell=True, cwd=_REPO)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenarios", type=Path, default=_REPO / "ops/acceptance/staging/scenarios.v1.json")
    parser.add_argument("--build-dir", type=Path, default=_REPO / os.environ.get("BS_STAGING_BUILD_DIR", "build_ci_test"))
    parser.add_argument("-C", "--config", default="Release")
    parser.add_argument("--dry-run", action="store_true", help="Print scenarios only")
    args = parser.parse_args()

    if not os.environ.get("BS_STAGING_ROOT") and not args.dry_run:
        print(
            "[WARN] BS_STAGING_ROOT unset; staging path parity (C-TST-ENV-1) not verified",
            file=sys.stderr,
        )

    data = json.loads(args.scenarios.read_text(encoding="utf-8"))
    failed = 0
    for sc in data.get("scenarios", []):
        sid = sc.get("id", "?")
        if args.dry_run:
            print(f"[dry-run] {sid} kind={sc.get('kind')}")
            continue
        kind = sc.get("kind")
        rc = 0
        if kind == "ctest":
            rc = _run_ctest(args.build_dir, args.config, sc.get("test"), None)
        elif kind == "ctest_label":
            rc = _run_ctest(args.build_dir, args.config, None, sc.get("label"))
        elif kind == "shell":
            rc = _run_shell(sc.get("command", ""), False)
        else:
            print(f"[FAIL] {sid}: unknown kind {kind}", file=sys.stderr)
            rc = 2
        if rc != 0:
            print(f"[FAIL] scenario {sid} exit {rc}", file=sys.stderr)
            failed += 1

    if failed:
        print(f"[FAIL] {failed} staging scenario(s) failed", file=sys.stderr)
        return 1
    print("[OK] staging acceptance passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

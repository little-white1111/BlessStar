#!/usr/bin/env python3
"""L3 Production smoke runner (C-TST-L3-1 / GATE-PROD-SMOKE)."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[3]

_FORBIDDEN_PATTERNS = [
    "ctest -L regression",
    "contract_gate_runner",
    "GATE-REGRESSION",
]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenarios", type=Path, default=_REPO / "ops/smoke/prod/scenarios.v1.json")
    parser.add_argument("--dry-run", action="store_true", help="Validate manifest only (local dev)")
    args = parser.parse_args()

    data = json.loads(args.scenarios.read_text(encoding="utf-8"))
    home = os.environ.get("BS_PROD_BLESSSTAR_HOME", "")
    manifest = os.environ.get("BS_PROD_MANIFEST_ROOT", "")

    if not args.dry_run and not home:
        print("[FAIL] set BS_PROD_BLESSSTAR_HOME or use --dry-run", file=sys.stderr)
        return 2

    failed = 0
    for sc in data.get("scenarios", []):
        sid = sc.get("id", "?")
        if sc.get("destructive"):
            print(f"[FAIL] {sid}: destructive scenario forbidden in L3", file=sys.stderr)
            failed += 1
            continue
        cmd = sc.get("command", "")
        for bad in _FORBIDDEN_PATTERNS:
            if bad in cmd:
                print(f"[FAIL] {sid}: forbidden pattern {bad}", file=sys.stderr)
                failed += 1
                break
        if args.dry_run:
            print(f"[dry-run] {sid}: {cmd}")
            continue
        print(f"[RUN] {sid}")
        rc = subprocess.call(cmd, shell=True, cwd=_REPO)
        if rc != 0 and not sc.get("optional"):
            print(f"[FAIL] {sid} exit {rc}", file=sys.stderr)
            failed += 1

    if failed:
        return 1
    print("[OK] prod smoke passed" + (" (dry-run)" if args.dry_run else ""))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

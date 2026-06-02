#!/usr/bin/env python3
"""GATE-TEST-TIER-ASSIGNMENT: L1 inventory matches cmake/Tests.cmake (C-TST-POL-1)."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[3]
_TIER_FILE = _REPO / "docs/contracts/testing/tier_assignments.json"
_TESTS_CMAKE = _REPO / "cmake/Tests.cmake"


def _ctest_names_from_cmake() -> set[str]:
    text = _TESTS_CMAKE.read_text(encoding="utf-8", errors="replace")
    exe = set(re.findall(r"blessstar_add_unit_test\((\w+)", text))
    script = set(re.findall(r"add_test\(\s*NAME (\w+)", text))
    return exe | script


def main() -> int:
    if not _TIER_FILE.is_file():
        print(f"[FAIL] missing {_TIER_FILE}", file=sys.stderr)
        return 2

    tier = json.loads(_TIER_FILE.read_text(encoding="utf-8"))
    l1 = set(tier.get("L1_dev_ci", {}).get("ctest_names", []))
    cmake_names = _ctest_names_from_cmake()

    missing = sorted(cmake_names - l1)
    extra = sorted(l1 - cmake_names)

    if missing:
        for n in missing:
            print(f"[FAIL] L1 tier_assignments missing ctest: {n}")
    if extra:
        for n in extra:
            print(f"[FAIL] L1 tier_assignments unknown ctest: {n}")

    if missing or extra:
        print("[FAIL] sync tier_assignments.json with cmake/Tests.cmake", file=sys.stderr)
        return 2

    print(f"[OK] L1 tier assignment covers {len(l1)} ctest name(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""GATE-TEST-TIER-ASSIGNMENT: L1 inventory is a subset of cmake/Tests.cmake (C-TST-POL-1).

Policy:
- `tier_assignments.json` is authoritative for which CTest cases belong to **L1_dev_ci**.
- CMake may define additional CTest cases that are **not** part of L1 (e.g. day19 stress).
- This gate ensures:
  1) Every listed L1 ctest name exists in cmake/Tests.cmake
  2) No duplicate names in L1 list
"""

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
    l1_list = tier.get("L1_dev_ci", {}).get("ctest_names", [])
    if not isinstance(l1_list, list):
        print("[FAIL] L1_dev_ci.ctest_names must be a list", file=sys.stderr)
        return 2
    dupes = sorted({n for n in l1_list if l1_list.count(n) > 1})
    if dupes:
        for n in dupes:
            print(f"[FAIL] duplicate L1 ctest name: {n}", file=sys.stderr)
        return 2
    l1 = set(l1_list)
    cmake_names = _ctest_names_from_cmake()

    # L1 must be subset of cmake; extra cmake tests are allowed (non-L1 tiers / out-of-band).
    extra = sorted(l1 - cmake_names)
    non_l1 = sorted(cmake_names - l1)

    if extra:
        for n in extra:
            print(f"[FAIL] L1 tier_assignments unknown ctest: {n}")

    if extra:
        print("[FAIL] sync tier_assignments.json with cmake/Tests.cmake", file=sys.stderr)
        return 2

    # Informational only: list cmake tests not in L1 to aid review.
    if non_l1:
        print(f"[INFO] {len(non_l1)} CTest case(s) are non-L1 (allowed). Examples:")
        for n in non_l1[:10]:
            print(f"  - {n}")

    print(f"[OK] L1 tier assignment covers {len(l1)} ctest name(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

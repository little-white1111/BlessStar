#!/usr/bin/env python3
"""C-ST-3: blessstar_add_unit_test targets must be bs_test_*."""

from __future__ import annotations

import re
import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parents[1] / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import repo_root

ADD_TEST_RE = re.compile(r"blessstar_add_unit_test\s*\(\s*([A-Za-z0-9_]+)")


def main() -> int:
    root = repo_root()
    tests_cmake = root / "cmake" / "Tests.cmake"
    text = tests_cmake.read_text(encoding="utf-8")
    bad: list[str] = []
    for name in ADD_TEST_RE.findall(text):
        if not name.startswith("bs_test_"):
            bad.append(f"C-ST-3: target '{name}' must start with bs_test_")
    if bad:
        for line in bad:
            print(f"[FAIL] {line}")
        return 2
    print("[OK] C-ST-3 cmake target name check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

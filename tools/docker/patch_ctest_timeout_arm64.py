#!/usr/bin/env python3
"""Scale CTest TIMEOUT properties for QEMU ARM64 docker validation."""
from __future__ import annotations

import re
import sys
from pathlib import Path

_MULTIPLIER = 5
_PATTERN = re.compile(r"(PROPERTIES\b.*?TIMEOUT )(\d+)")


def _scale_file(path: Path) -> None:
    text = path.read_text(encoding="utf-8")
    scaled = _PATTERN.sub(
        lambda match: f"{match.group(1)}{int(match.group(2)) * _MULTIPLIER}",
        text,
    )
    if scaled != text:
        path.write_text(scaled, encoding="utf-8")


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "/src/build_ci_test")
    if not root.is_dir():
        print(f"[patch_ctest_timeout_arm64] missing build dir: {root}", file=sys.stderr)
        return 1
    for cmake_file in root.rglob("CTestTestfile.cmake"):
        _scale_file(cmake_file)
    print(f"[patch_ctest_timeout_arm64] scaled TIMEOUT x{_MULTIPLIER} under {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

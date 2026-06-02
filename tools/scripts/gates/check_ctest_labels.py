#!/usr/bin/env python3
"""C-ST-9 / C-ST-11: ctest label rules in cmake/Tests.cmake."""

from __future__ import annotations

import re
import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parents[1] / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import repo_root

LABELS_RE = re.compile(
    r"set_tests_properties\s*\(\s*([A-Za-z0-9_]+)\s+PROPERTIES\s+LABELS\s+\"([^\"]+)\"",
    re.MULTILINE,
)

def _has_day_label(labels: set[str]) -> bool:
    return any(l.startswith("day") and len(l) > 3 and l[3:].isdigit() for l in labels)


def _has_scope_label(labels: set[str]) -> bool:
    return bool(labels & {"integration", "comprehensive", "docs"})


def main() -> int:
    root = repo_root()
    text = (root / "cmake" / "Tests.cmake").read_text(encoding="utf-8")
    bad: list[str] = []
    for test_name, labels_raw in LABELS_RE.findall(text):
        labels = {x.strip() for x in labels_raw.split(";") if x.strip()}
        if "unit" not in labels and "docs" not in labels:
            bad.append(f"C-ST-9: {test_name}: missing label 'unit' (or docs)")
        if not (
            _has_day_label(labels) or _has_scope_label(labels) or "regression" in labels
        ):
            bad.append(
                f"C-ST-9: {test_name}: need dayNN, integration, comprehensive, docs, or regression"
            )
        if "benchmark" in labels and "regression" in labels:
            bad.append(f"C-ST-11: {test_name}: benchmark must not carry regression")
    if bad:
        for line in bad:
            print(f"[FAIL] {line}")
        return 2
    print("[OK] C-ST-9/C-ST-11 ctest labels check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

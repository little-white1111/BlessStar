#!/usr/bin/env python3
"""C-ST-2: app/sdk public headers use bs::app namespace."""

from __future__ import annotations

import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parents[1] / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import repo_root


def main() -> int:
    root = repo_root()
    sdk_inc = root / "app" / "sdk" / "include"
    if not sdk_inc.is_dir():
        print("[FAIL] C-ST-2: app/sdk/include missing")
        return 2
    bad: list[str] = []
    for path in sorted(sdk_inc.rglob("*.h")):
        text = path.read_text(encoding="utf-8", errors="replace")
        if "namespace bs::app" not in text and "namespace bs::app\n" not in text:
            rel = path.relative_to(root)
            bad.append(f"C-ST-2: {rel}: expected namespace bs::app")
    if bad:
        for line in bad:
            print(f"[FAIL] {line}")
        return 2
    print("[OK] C-ST-2 namespace boundary check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

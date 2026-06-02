#!/usr/bin/env python3
"""C-ST-7: public headers must contain a contract documentation block."""

from __future__ import annotations

import sys
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

_LIB = Path(__file__).resolve().parents[1] / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import public_include_dirs, repo_root

MARKER = "C-ST-7 contract block"
LEGACY_SKIP_MARKER = "LEGACY-NOT-BUILT"
REQUIRED_FIELDS = ("Thread safety:", "Error semantics:", "Platform notes:")


def main() -> int:
    root = repo_root()
    bad: list[str] = []
    header_paths: list[Path] = []
    for inc_dir in public_include_dirs(root):
        header_paths.extend(sorted(inc_dir.rglob("*.h")))
    persistence = root / "adapter" / "persistence"
    if persistence.is_dir():
        header_paths.extend(sorted(persistence.glob("*.h")))
    for path in sorted(set(header_paths)):
        text = path.read_text(encoding="utf-8", errors="replace")
        if LEGACY_SKIP_MARKER in text:
            continue
        if MARKER not in text:
            bad.append(f"C-ST-7: {path.relative_to(root)}: missing '{MARKER}'")
            continue
        for field in REQUIRED_FIELDS:
            if field not in text:
                bad.append(f"C-ST-7: {path.relative_to(root)}: missing '{field}'")
    if bad:
        for line in bad[:60]:
            print(f"[FAIL] {line}")
        if len(bad) > 60:
            print(f"[FAIL] ... and {len(bad) - 60} more")
        return 2
    print("[OK] C-ST-7 public header contract block check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

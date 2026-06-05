#!/usr/bin/env python3
"""T20.11: forbid registry->reload_batch_run call edges in adapter sources (lock-order hygiene)."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
ADAPTER = ROOT / "adapter"
FORBIDDEN = re.compile(
    r"bs_registry_facade_.*reload_batch_run|registry_facade.*bs_adapter_attach_reload_batch_run",
    re.IGNORECASE,
)


def main() -> int:
    hits: list[str] = []
    for path in ADAPTER.rglob("*"):
        if path.suffix not in {".c", ".cpp", ".h"}:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for i, line in enumerate(text.splitlines(), 1):
            if FORBIDDEN.search(line):
                hits.append(f"{path.relative_to(ROOT)}:{i}:{line.strip()}")
    if hits:
        print("check_attach_lock_order: FAIL")
        for h in hits:
            print(" ", h)
        return 1
    print("check_attach_lock_order: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())

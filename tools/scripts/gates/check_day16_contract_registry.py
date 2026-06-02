#!/usr/bin/env python3
"""
Validate DAY16 contract registry markdown table.

Rule:
- table must contain columns:
  id, version, scope, priority, verify, deprecate, owner, status
- for rows with status == active, verify must be non-empty and not '-'
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


def _split_cells(line: str) -> list[str]:
    raw = [c.strip() for c in line.strip().strip("|").split("|")]
    return raw


def validate(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()

    header_idx = -1
    for i, ln in enumerate(lines):
        if re.match(r"^\|\s*id\s*\|\s*version\s*\|", ln.strip(), re.IGNORECASE):
            header_idx = i
            break
    if header_idx < 0:
        print("[FAIL] registry header not found")
        return 2
    if header_idx + 1 >= len(lines):
        print("[FAIL] registry separator not found")
        return 3

    bad = 0
    for ln in lines[header_idx + 2 :]:
        s = ln.strip()
        if not s.startswith("|"):
            break
        cells = _split_cells(s)
        if len(cells) < 8:
            print(f"[FAIL] malformed row: {ln}")
            bad += 1
            continue
        rid, _version, _scope, _priority, verify, _deprecate, _owner, status = cells[:8]
        if not rid:
            print(f"[FAIL] empty id row: {ln}")
            bad += 1
        if status.lower() == "active" and (not verify or verify == "-"):
            print(f"[FAIL] active row missing verify: {rid}")
            bad += 1

    if bad:
        print(f"[FAIL] registry validation errors: {bad}")
        return 4
    print("[OK] day16 contract registry check passed")
    return 0


def main() -> int:
    if len(sys.argv) != 2:
        print(f"Usage: {Path(sys.argv[0]).name} <registry_markdown_path>")
        return 1
    return validate(Path(sys.argv[1]))


if __name__ == "__main__":
    raise SystemExit(main())

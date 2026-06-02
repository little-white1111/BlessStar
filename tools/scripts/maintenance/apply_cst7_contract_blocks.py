#!/usr/bin/env python3
"""One-shot helper: insert C-ST-7 contract block after include guard in public headers."""

from __future__ import annotations

import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parents[1] / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import public_include_dirs, repo_root

BLOCK = """
/*
 * C-ST-7 contract block:
 * Thread safety: See implementation; default not thread-safe unless documented otherwise.
 * Error semantics: See bs_status / module-specific return codes in implementation.
 * Platform notes: N/A unless stated in .c/.cpp implementation.
 */
"""


def main() -> int:
    root = repo_root()
    updated = 0
    for inc_dir in public_include_dirs(root):
        for path in sorted(inc_dir.rglob("*.h")):
            text = path.read_text(encoding="utf-8", errors="replace")
            if "C-ST-7 contract block" in text:
                continue
            lines = text.splitlines(keepends=True)
            if len(lines) < 2:
                continue
            # After #ifndef / #define guard (line 0-1), insert block.
            insert_at = 2
            for i, ln in enumerate(lines[:6]):
                if ln.strip().startswith("#define"):
                    insert_at = i + 1
                    break
            lines.insert(insert_at, BLOCK)
            path.write_text("".join(lines), encoding="utf-8", newline="\n")
            updated += 1
    print(f"[OK] inserted C-ST-7 blocks in {updated} headers")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

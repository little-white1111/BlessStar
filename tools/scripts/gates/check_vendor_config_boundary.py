#!/usr/bin/env python3
"""
C-IX-7: vendor-specific config parsing must not live under adapter/parser.

Checks (MVP):
- adapter/parser production sources must not reference vendor/ERP parser markers.
- adapter/parser must not import app/sdk headers.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parents[1] / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import repo_root

FORBIDDEN_IN_ADAPTER_PARSER = re.compile(
    r"(yonyou|kingdee|ufida|用友|金蝶|vendor_config|erp_config|VendorConfig|"
    r"parse_vendor|vendor_parse|厂商)",
    re.IGNORECASE,
)

RE_APP_INCLUDE = re.compile(
    r'#\s*include\s+[<"][^>"]*bs/app/',
    re.IGNORECASE,
)


def _iter_parser_sources(root: Path) -> list[Path]:
    base = root / "adapter/parser"
    if not base.is_dir():
        return []
    out: list[Path] = []
    for ext in ("*.c", "*.h", "*.cpp", "*.hpp"):
        for p in base.rglob(ext):
            rel = p.relative_to(root).as_posix()
            if "/test/" in rel or rel.endswith("Test.cpp"):
                continue
            out.append(p)
    return sorted(out)


def main() -> int:
    root = repo_root()
    errors: list[str] = []

    for path in _iter_parser_sources(root):
        rel = path.relative_to(root).as_posix()
        text = path.read_text(encoding="utf-8", errors="replace")
        for i, line in enumerate(text.splitlines(), 1):
            if FORBIDDEN_IN_ADAPTER_PARSER.search(line):
                errors.append(f"{rel}:{i}: C-IX-7 forbidden vendor/ERP marker in adapter/parser")
            if RE_APP_INCLUDE.search(line):
                errors.append(f"{rel}:{i}: C-IX-7 adapter/parser must not include app/sdk")

    if errors:
        for e in errors:
            print(f"[FAIL] {e}")
        return 2
    print("[OK] C-IX-7 vendor config parse boundary check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

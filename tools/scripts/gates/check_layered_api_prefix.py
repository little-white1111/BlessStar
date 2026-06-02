#!/usr/bin/env python3
"""C-ST-14: forbid deprecated adapter API prefixes on production surfaces."""

from __future__ import annotations

import re
import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parent.parent / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import repo_root  # noqa: E402

LEGACY = "LEGACY-NOT-BUILT"

# Deprecated public C API prefixes (adapter attach/reload/persist/parser migration).
FORBIDDEN = (
    "bs_attach_context_",
    "bs_attach_store_",
    "bs_attach_wal_",
    "bs_attach_watch_",
    "bs_attach_report_",
    "bs_attach_uri_",
    "bs_attach_fsync_",
    "bs_attach_crc32_",
    "bs_reload_batch_controller_",
    "bs_reload_batch_",
    "bs_config_parse_",
    "bs_adapter_attach_execute_",
)

FUNC_RE = re.compile(r"\b(" + "|".join(re.escape(p) for p in FORBIDDEN) + r")[a-z0-9_]*")

SCAN_ROOTS = (
    "adapter/include",
    "adapter/parser/include",
    "adapter/persistence",
    "adapter/src",
    "adapter/orchestration",
)

SKIP_SUFFIX = {".md", ".json", ".txt", ".py"}


def should_scan(path: Path) -> bool:
    if path.suffix.lower() not in {".h", ".c", ".cpp", ".cc"}:
        return False
    if LEGACY in path.read_text(encoding="utf-8", errors="replace"):
        return False
    return True


def main() -> int:
    root = repo_root()
    bad: list[str] = []
    for rel_root in SCAN_ROOTS:
        base = root / rel_root
        if not base.exists():
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file() or not should_scan(path):
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            for m in FUNC_RE.finditer(text):
                rel = path.relative_to(root).as_posix()
                bad.append(f"C-ST-14: {rel}: deprecated prefix '{m.group(1)}' in '{m.group(0)}'")
    if bad:
        for line in bad[:60]:
            print(f"[FAIL] {line}")
        if len(bad) > 60:
            print(f"[FAIL] ... and {len(bad) - 60} more")
        return 2
    print("[OK] C-ST-14 layered API prefix check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

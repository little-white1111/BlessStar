#!/usr/bin/env python3
"""
C-ATTACH-SYNC-2: production paths must not commit attach persistence outside
reload_batch_run / recover Step-2 orchestration.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parents[1] / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import repo_root  # noqa: E402

FORBIDDEN = re.compile(
    r"\bbs_adapter_attach_persist_store_(?:commit_per_path|batch_commit)\s*\("
)

SCAN_ROOTS = [
    "adapter/src",
    "adapter/orchestration",
    "cli",
    "app/cli",
]

WHITELIST = {
    "adapter/orchestration/reload_batch_controller.cpp",
    "adapter/persistence/attach_store.cpp",
}


def iter_sources(root: Path) -> list[Path]:
    out: list[Path] = []
    for rel_root in SCAN_ROOTS:
        base = root / rel_root
        if not base.exists():
            continue
        if base.is_file():
            out.append(base)
            continue
        for ext in ("*.c", "*.cpp", "*.h", "*.hpp"):
            out.extend(base.rglob(ext))
    return sorted(set(out))


def main() -> int:
    root = repo_root()
    errors: list[str] = []
    for path in iter_sources(root):
        rel = path.relative_to(root).as_posix()
        if rel in WHITELIST or "/test/" in rel or rel.startswith("adapter/test/"):
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for line_no, line in enumerate(text.splitlines(), 1):
            if FORBIDDEN.search(line):
                errors.append(
                    f"{rel}:{line_no}: production attach persist commit must go through "
                    "reload_batch_run or recover Step-2"
                )

    if errors:
        for error in errors:
            print(f"[FAIL] {error}")
        return 2
    print("[OK] C-ATTACH-SYNC production persist path check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

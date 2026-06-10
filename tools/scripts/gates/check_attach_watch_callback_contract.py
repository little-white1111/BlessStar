#!/usr/bin/env python3
"""
C-ATTACH-WATCH-CB-1: Watch callbacks must not acquire attach session read/write locks
or invoke reload orchestration (deadlock / reentrancy hazard).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parents[1] / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import repo_root

SCAN_ROOTS = ("adapter/test", "adapter/src")

REGISTER_PATTERNS = (
    re.compile(r"bs_adapter_attach_config_subscribe_state_watch\s*\("),
    re.compile(r"bs_watch_manager_add_watch\s*\("),
    re.compile(r"bs_config_manager_add_watch\s*\("),
    re.compile(r"bs_adapter_attach_persist_watch_subscribe\s*\("),
)

CALLBACK_ARG_RE = re.compile(
    r"(?:subscribe_state_watch|add_watch|persist_watch_subscribe)\s*\("
    r"(?:[^,()]*,\s*){1,3}(\w+)\s*[,)]",
    re.MULTILINE | re.DOTALL,
)

FORBIDDEN_IN_CALLBACK = (
    "bs_adapter_attach_session_try_read_lock",
    "bs_adapter_attach_session_begin_write_window",
    "begin_write_window",
    "reload_batch_run",
    "bs_adapter_attach_reload_batch_run",
)

FUNC_BODY_RE = re.compile(
    r"(?:static\s+)?(?:void|int)\s+(\w+)\s*\([^)]*\)\s*\{",
    re.MULTILINE,
)


def _iter_sources(root: Path) -> list[Path]:
    out: list[Path] = []
    for rel in SCAN_ROOTS:
        base = root / rel
        if not base.is_dir():
            continue
        for ext in ("*.cpp", "*.c", "*.h", "*.hpp"):
            out.extend(sorted(base.rglob(ext)))
    return out


def _extract_brace_block(text: str, open_brace: int) -> str:
    depth = 0
    for i in range(open_brace, len(text)):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[open_brace : i + 1]
    return text[open_brace:]


def _function_bodies(text: str) -> dict[str, str]:
    bodies: dict[str, str] = {}
    for m in FUNC_BODY_RE.finditer(text):
        name = m.group(1)
        open_brace = text.find("{", m.end() - 1)
        if open_brace < 0:
            continue
        bodies[name] = _extract_brace_block(text, open_brace)
    return bodies


def _registered_callbacks(text: str) -> set[str]:
    names: set[str] = set()
    if not any(p.search(text) for p in REGISTER_PATTERNS):
        return names
    for m in CALLBACK_ARG_RE.finditer(text):
        names.add(m.group(1))
    return names


def main() -> int:
    root = repo_root()
    errors: list[str] = []

    for path in _iter_sources(root):
        rel = path.relative_to(root).as_posix()
        text = path.read_text(encoding="utf-8", errors="replace")
        callbacks = _registered_callbacks(text)
        if not callbacks:
            continue
        bodies = _function_bodies(text)
        for cb in sorted(callbacks):
            body = bodies.get(cb)
            if body is None:
                continue
            for forbidden in FORBIDDEN_IN_CALLBACK:
                if forbidden in body:
                    errors.append(
                        f"{rel}: watch callback `{cb}` must not call `{forbidden}`"
                    )

    if errors:
        for e in errors:
            print(f"[FAIL] {e}")
        return 2
    print("[OK] C-ATTACH-WATCH-CB-1 attach watch callback contract check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

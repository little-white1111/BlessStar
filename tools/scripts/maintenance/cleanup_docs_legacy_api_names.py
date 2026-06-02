#!/usr/bin/env python3
"""Replace deprecated adapter API names in documentation (post C-ST-14)."""

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[3]

# Longest match first.
REPLACEMENTS: tuple[tuple[str, str], ...] = (
    ("bs_reload_batch_controller_use_default_gate", "bs_adapter_attach_reload_batch_set_default_gate"),
    ("bs_reload_batch_controller_", "bs_adapter_attach_reload_batch_"),
    ("bs_reload_batch_run_with_report", "bs_adapter_attach_reload_batch_run_with_report"),
    ("bs_reload_batch_", "bs_adapter_attach_reload_batch_"),
    ("bs_attach_context_", "bs_adapter_attach_ctx_"),
    ("bs_config_parse_result_destroy", "bs_adapter_parser_result_destroy"),
    ("bs_config_parse_bytes", "bs_adapter_parser_parse_bytes"),
    ("bs_attach_store_batch_", "bs_adapter_attach_persist_store_batch_"),
    ("bs_attach_store_", "bs_adapter_attach_persist_store_"),
    ("bs_attach_watch_", "bs_adapter_attach_persist_watch_"),
    ("bs_attach_wal_", "bs_adapter_attach_persist_wal_"),
    ("bs_attach_report_", "bs_adapter_attach_persist_report_"),
    ("bs_attach_uri_to_path", "bs_adapter_attach_persist_uri_to_path"),
    ("bs_attach_fsync_file", "bs_adapter_attach_persist_fsync_file"),
    ("bs_attach_crc32", "bs_adapter_attach_persist_crc32"),
    ("bs_adapter_attach_execute_", "bs_adapter_attach_exec_"),
    ("bs_reload_batch_run", "bs_adapter_attach_reload_batch_run"),
    ("bs_attach_fsync_policy", "BsAttachFsyncPolicy"),
    ("bs_attach_limits.h", "bs_adapter_attach_persist_limits.h"),
)

SKIP_PATH_PARTS = (
    "build_ci_test",
    "build_no_tests",
    "_deps",
    ".git",
)

SKIP_FILES = {
    "docs/BLESSSTAR_NAMING_CONTRACT.md",
    "docs/contracts/style/C-ST-14.v1.json",
    "tools/scripts/gates/check_layered_api_prefix.py",
    "tools/scripts/maintenance/cleanup_docs_legacy_api_names.py",
}

def collect_doc_paths() -> list[Path]:
    paths: list[Path] = []
    for name in ("架构方案选择记录.md", "项目修改记录.md", "AGENTS.md"):
        p = _REPO / name
        if p.is_file():
            paths.append(p)
    paths.extend(_REPO.glob("docs/**/*.md"))
    paths.extend(_REPO.glob("adapter/**/README.md"))
    paths.extend(_REPO.glob("tools/scripts/**/*.md"))
    return paths


def should_skip(path: Path) -> bool:
    rel = path.relative_to(_REPO).as_posix()
    if rel in SKIP_FILES:
        return True
    return any(part in path.parts for part in SKIP_PATH_PARTS)


def apply_replacements(text: str) -> str:
    for old, new in REPLACEMENTS:
        text = text.replace(old, new)
    return text


def main() -> int:
    changed: list[str] = []

    for path in sorted(set(collect_doc_paths())):
        if not path.is_file() or should_skip(path):
            continue
        original = path.read_text(encoding="utf-8", errors="replace")
        updated = apply_replacements(original)
        if updated != original:
            path.write_text(updated, encoding="utf-8", newline="\n")
            changed.append(path.relative_to(_REPO).as_posix())

    for line in changed:
        print(f"updated: {line}")
    print(f"[OK] {len(changed)} file(s) updated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

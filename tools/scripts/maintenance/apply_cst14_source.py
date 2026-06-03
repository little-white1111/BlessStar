#!/usr/bin/env python3
"""Apply C-ST-14 public API renames across source/tests (longest match first)."""

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[3]

# Order matters: longest prefixes first.
REPLACEMENTS: tuple[tuple[str, str], ...] = (
    ("bs_reload_batch_controller_use_default_gate", "bs_adapter_attach_reload_batch_set_default_gate"),
    ("bs_reload_batch_controller_", "bs_adapter_attach_reload_batch_"),
    ("bs_reload_batch_run_with_report", "bs_adapter_attach_reload_batch_run_with_report"),
    ("bs_reload_batch_run", "bs_adapter_attach_reload_batch_run"),
    ("bs_reload_batch_outcome", "bs_adapter_attach_reload_batch_outcome"),
    ("bs_reload_batch_path_state", "bs_adapter_attach_reload_batch_path_state"),
    ("bs_reload_batch_add_path", "bs_adapter_attach_reload_batch_add_path"),
    ("bs_attach_context_", "bs_adapter_attach_ctx_"),
    ("bs_config_parse_result_destroy", "bs_adapter_parser_result_destroy"),
    ("bs_config_parse_bytes", "bs_adapter_parser_parse_bytes"),
    ("bs_config_parse_", "bs_adapter_parser_"),
    ("bs_attach_store_batch_", "bs_adapter_attach_persist_store_batch_"),
    ("bs_attach_store_", "bs_adapter_attach_persist_store_"),
    ("bs_attach_watch_", "bs_adapter_attach_persist_watch_"),
    ("bs_attach_wal_", "bs_adapter_attach_persist_wal_"),
    ("bs_attach_report_", "bs_adapter_attach_persist_report_"),
    ("bs_attach_uri_to_path", "bs_adapter_attach_persist_uri_to_path"),
    ("bs_attach_fsync_file", "bs_adapter_attach_persist_fsync_file"),
    ("bs_attach_crc32", "bs_adapter_attach_persist_crc32"),
    ("bs_adapter_attach_execute_", "bs_adapter_attach_exec_"),
    # Kernel IR (public headers already bs_ir_*; sweep stragglers in tests/src).
    ("ir_instruction_list_destroy", "bs_ir_instruction_list_destroy"),
    ("ir_instruction_list_add", "bs_ir_instruction_list_add"),
    ("ir_instruction_list_get", "bs_ir_instruction_list_get"),
    ("ir_instruction_list_size", "bs_ir_instruction_list_size"),
    ("ir_instruction_list_create", "bs_ir_instruction_list_create"),
    ("ir_instruction_get_metadata", "bs_ir_instruction_get_metadata"),
    ("ir_instruction_create", "bs_ir_instruction_create"),
    # config_v1_ir keeps bs_config_v1_generate_ir_from_ast (not bs_ir_*).
    # Kernel report
    ("report_to_json", "bs_report_to_json"),
    ("report_destroy", "bs_report_destroy"),
    # Only bare report_create (tests); skip if already bs_report_*.
    ("report_create", "bs_report_create"),
    # Kernel state event bus / config event
    ("ConfigEvent_Destroy", "bs_config_event_destroy"),
    ("ConfigEvent_Create", "bs_config_event_create"),
    ("EventBus_Unsubscribe", "bs_event_bus_unsubscribe"),
    ("EventBus_Subscribe", "bs_event_bus_subscribe"),
    ("EventBus_Publish", "bs_event_bus_publish"),
    ("EventBus_Drain", "bs_event_bus_drain"),
    ("EventBus_Create", "bs_event_bus_create"),
    ("kernel_get_builtin_requirements", "bs_kernel_get_builtin_requirements"),
    ("config_v1_build_manual_requirements", "bs_config_v1_build_manual_requirements"),
    ("config_v1_ast_destroy", "bs_config_v1_ast_destroy"),
    ("json_parse_config_v1", "bs_json_parse_config_v1"),
)

SCAN_SUFFIX = {".c", ".cpp", ".cc", ".h", ".hpp", ".cmake", ".md"}
SKIP_PARTS = {
    "build_ci_test",
    "build_no_tests",
    "build-san",
    "build-meson",
    "_deps",
    ".git",
    "Source",
}
SKIP_FILES = {
    "docs/BLESSSTAR_NAMING_CONTRACT.md",
    "tools/scripts/maintenance/apply_cst14_source.py",
    "tools/scripts/maintenance/cleanup_docs_legacy_api_names.py",
}


def should_scan(path: Path) -> bool:
    if path.suffix.lower() not in SCAN_SUFFIX and path.name not in {"CMakeLists.txt", "Tests.cmake"}:
        return False
    rel = path.relative_to(_REPO).as_posix()
    if rel in SKIP_FILES:
        return False
    return not any(part in path.parts for part in SKIP_PARTS)


def apply(text: str) -> str:
    for old, new in REPLACEMENTS:
        text = text.replace(old, new)
    return text


def main() -> int:
    changed: list[str] = []
    for path in sorted(_REPO.rglob("*")):
        if not path.is_file() or not should_scan(path):
            continue
        original = path.read_text(encoding="utf-8", errors="replace")
        updated = apply(original)
        if updated != original:
            path.write_text(updated, encoding="utf-8", newline="\n")
            changed.append(path.relative_to(_REPO).as_posix())
    print(f"[OK] C-ST-14 apply: {len(changed)} file(s) updated")
    for rel in changed[:40]:
        print(f"  {rel}")
    if len(changed) > 40:
        print(f"  ... and {len(changed) - 40} more")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

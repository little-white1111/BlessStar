#!/usr/bin/env python3
"""C-ST-10: forbid fixed global temp dirs in tests (e.g. bare bs_day15_watch)."""

from __future__ import annotations

import re
import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parents[1] / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import repo_root

# Exact fixed dir string without unique suffix.
BAD_PATTERNS = [
    re.compile(r"\"bs_day15_watch\""),
    re.compile(r"'bs_day15_watch'"),
    re.compile(r"\"bs_day14_attach\""),
    re.compile(r"\"bs_test_attach\""),
    re.compile(r"\"bs_reload_config_v1_good\.json\""),
    re.compile(r"\"bs_reload_config_v1_bad\.json\""),
    re.compile(r"\"bs_manifest_day9\.json\""),
    re.compile(r"\"bs_day8_attach_full_cfg\.json\""),
    re.compile(r"\"bs_manifest_day8\.json\""),
    re.compile(r"\"bs_app_vendor_reload_temp\""),
    re.compile(r"\"bs_io_attach_cfg\.txt\""),
    re.compile(r"\"bs_io_registry_phase\.txt\""),
    re.compile(r"\"bs_io_local_test\.txt\""),
    re.compile(r"\"bs_io_local_boundary\""),
    re.compile(r"\"bs_io_timeout_test\.txt\""),
    re.compile(r"\"bs_reload_gate_report_integration_cfg\.txt\""),
    re.compile(r"\"bs_test_manifest_line_limit\.tmp\""),
    re.compile(r"\"bs_day12_attach\""),
    re.compile(r"\"bs_vendor_bad\.json\""),
    re.compile(r"\"bs_vendor_temp\""),
]

# fs::absolute("bs_...") under cwd (non-hermetic).
FIXED_ABS_BS = re.compile(
    r"""fs::absolute\s*\(\s*["']bs_[^"']+["']\s*\)"""
)

RELOAD_INTEGRATION_FILES = (
    "ReloadConfigJsonIntegrationTest.cpp",
    "Day8AttachFullIntegrationTest.cpp",
    "AppVendorReloadIntegrationTest.cpp",
)

# Adapter tests that touch filesystem for IO/reload/manifest must use test_temp_dir.h.
IO_HERMETIC_FILES = (
    "IoAttachPipelineTest.cpp",
    "IoRegistryPhaseTest.cpp",
    "IoLocalProviderTest.cpp",
    "IoLocalProviderBoundaryTest.cpp",
    "IoLocalProviderTimeoutTest.cpp",
    "ReloadDefaultGateReportEventBusReentryIntegrationTest.cpp",
    "AttachResilienceTest.cpp",
    "ConfigParseSecurityAuditTest.cpp",
)


def _scan_test_cpp(root: Path) -> list[str]:
    bad: list[str] = []
    for path in sorted(root.rglob("*Test*.cpp")):
        if "build" in path.parts:
            continue
        rel = path.relative_to(root).as_posix()
        text = path.read_text(encoding="utf-8", errors="replace")
        for pat in BAD_PATTERNS:
            for line in text.splitlines():
                if pat.search(line) and "bs_test_unique_temp_dir" not in line:
                    bad.append(f"C-ST-10: {rel}: fixed temp path {pat.pattern}")
                    break
        if FIXED_ABS_BS.search(text):
            bad.append(
                f"C-ST-10: {rel}: fs::absolute(\"bs_...\") is not hermetic; "
                f"use adapter/test/support/test_temp_dir.h"
            )
        if path.name in RELOAD_INTEGRATION_FILES and "test_temp_dir.h" not in text:
            bad.append(
                f"C-ST-10: {rel}: must use adapter/test/support/test_temp_dir.h "
                f"for hermetic temp dirs"
            )
        if path.name in IO_HERMETIC_FILES and "test_temp_dir.h" not in text:
            bad.append(
                f"C-ST-10: {rel}: must use adapter/test/support/test_temp_dir.h "
                f"(P0-HERM-IO)"
            )
    return bad


def main() -> int:
    root = repo_root()
    bad = _scan_test_cpp(root)
    if bad:
        for line in bad:
            print(f"[FAIL] {line}")
        return 2
    print("[OK] C-ST-10 test tempdir uniqueness check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

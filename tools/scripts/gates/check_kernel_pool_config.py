#!/usr/bin/env python3
"""C-KERNEL-POOL-1: validate day21 KernelPool config and boundary."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parents[1] / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import repo_root

EXPECTED = {
    "steady_count": 3,
    "max_instances": 10,
    "dynamic_idle_ttl_ms": 1000,
    "inline_depth_max": 8,
    "fifo_wait_unbounded": True,
    "execute_timeout_ms": 0,
    "per_batch_parallel_exec": True,
    "slot_quarantine_on_failure": False,
    "retain_latest_per_path": True,
    "max_pinned_revisions_per_path": 8,
}

FORBIDDEN_RUNTIME_INCLUDE = re.compile(r'#\s*include\s+[<"][^>"]*ConfigManager\.h[>"]')


def main() -> int:
    root = repo_root()
    contract_path = root / "docs/contracts/kernel/kernel_pool.v1.json"
    source_path = root / "kernel/runtime/src/kernel_pool.c"
    errors: list[str] = []

    if not contract_path.is_file():
        errors.append("missing docs/contracts/kernel/kernel_pool.v1.json")
    else:
        data = json.loads(contract_path.read_text(encoding="utf-8"))
        params = data.get("parameters") or {}
        for key, expected in EXPECTED.items():
            if params.get(key) != expected:
                errors.append(f"{contract_path.relative_to(root)}: parameters.{key}={params.get(key)!r}, expected {expected!r}")
        if data.get("id") != "C-KERNEL-POOL-1":
            errors.append("kernel_pool.v1.json: id must be C-KERNEL-POOL-1")
        if "GATE-KERNEL-POOL-CONFIG" not in data.get("gate_refs", []):
            errors.append("kernel_pool.v1.json: missing GATE-KERNEL-POOL-CONFIG gate_ref")

    if not source_path.is_file():
        errors.append("missing kernel/runtime/src/kernel_pool.c")
    else:
        for line_no, line in enumerate(source_path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            if FORBIDDEN_RUNTIME_INCLUDE.search(line):
                errors.append(f"{source_path.relative_to(root)}:{line_no}: kernel_pool.c must not include ConfigManager.h")

    if errors:
        for error in errors:
            print(f"[FAIL] {error}")
        return 2
    print("[OK] C-KERNEL-POOL-1 config and runtime boundary check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

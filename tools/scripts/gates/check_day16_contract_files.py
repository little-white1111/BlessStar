#!/usr/bin/env python3
"""
Validate structured contract JSON files (architecture / integration / style).

Rules:
- each file must contain root keys: version, type, contracts
- each contract must contain keys:
  id, name, scope, priority, rule, verify, deprecate, status
- if status == active, verify must be a non-empty list
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

REQUIRED_ROOT = {"version", "type", "contracts"}
REQUIRED_ITEM = {"id", "name", "scope", "priority", "rule", "verify", "deprecate", "status"}


def validate_file(path: Path) -> list[str]:
    errors: list[str] = []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        return [f"{path.name}: invalid json: {exc}"]

    missing_root = REQUIRED_ROOT - set(data.keys())
    if missing_root:
        errors.append(f"{path.name}: missing root keys: {sorted(missing_root)}")
        return errors

    contracts = data.get("contracts")
    if not isinstance(contracts, list):
        errors.append(f"{path.name}: contracts must be a list")
        return errors

    for i, item in enumerate(contracts):
        if not isinstance(item, dict):
            errors.append(f"{path.name}: contracts[{i}] must be object")
            continue
        missing_item = REQUIRED_ITEM - set(item.keys())
        if missing_item:
            errors.append(f"{path.name}: contracts[{i}] missing keys: {sorted(missing_item)}")
            continue
        if item.get("status") == "active":
            verify = item.get("verify")
            if not isinstance(verify, list) or len(verify) == 0:
                errors.append(f"{path.name}: contracts[{i}] active but verify is empty")
    return errors


def main() -> int:
    if len(sys.argv) < 2:
        print(f"Usage: {Path(sys.argv[0]).name} <contract_json> [<contract_json> ...]")
        return 1
    all_errors: list[str] = []
    for arg in sys.argv[1:]:
        all_errors.extend(validate_file(Path(arg)))
    if all_errors:
        for line in all_errors:
            print(f"[FAIL] {line}")
        return 2
    print("[OK] structured contract files check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""
Validate D-scheme contract instance files via docs/contracts/index.json.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

REQUIRED_FIELDS = {
    "id",
    "type",
    "version",
    "priority",
    "status",
    "scope",
    "stage",
    "rule",
    "gate_refs",
    "implementations",
    "blocking",
    "owner",
    "evidence",
    "deprecate",
}

VALID_STATUS = {"draft", "active", "deprecated"}
IMPL_REQUIRED_FIELDS = {"gate_id", "runner", "entry", "entry_kind"}
VALID_ENTRY_KIND = {"script", "command"}


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    _lib = Path(__file__).resolve().parents[1] / "lib"
    if str(_lib) not in sys.path:
        sys.path.insert(0, str(_lib))
    from contract_schema import validate_gate_registry_commands, validate_implementations
    from repo_paths import repo_root

    repo = repo_root()
    index_path = repo / "docs/contracts/index.json"
    gate_path = repo / "docs/gates/gate_registry.json"
    if not index_path.exists():
        print("[FAIL] missing docs/contracts/index.json")
        return 2
    idx = _load_json(index_path)
    gate_registry = _load_json(gate_path)
    gates = gate_registry.get("gates", [])
    gate_by_id = {g.get("gate_id"): g for g in gates if g.get("gate_id")}
    allowed_prefixes = idx.get("allowed_entry_prefixes", [])
    roots = idx.get("contract_roots", {})
    priorities = set(idx.get("priority_order", []))
    stages = set(idx.get("stage_order", []))
    errors: list[str] = []
    checked = 0

    errors.extend(validate_gate_registry_commands(gates, repo, allowed_prefixes))

    for ctype, rel in roots.items():
        root = repo / rel
        if not root.is_dir():
            errors.append(f"missing root: {rel}")
            continue
        for p in sorted(root.glob("*.v1.json")):
            checked += 1
            try:
                data = _load_json(p)
            except Exception as exc:  # noqa: BLE001
                errors.append(f"{p.name}: invalid json: {exc}")
                continue
            data["_path"] = str(p.relative_to(repo)).replace("\\", "/")
            miss = REQUIRED_FIELDS - set(data.keys())
            if miss:
                errors.append(f"{p.name}: missing fields {sorted(miss)}")
                continue
            if data.get("type") != ctype:
                errors.append(f"{p.name}: type {data.get('type')} != dir {ctype}")
            if data.get("status") not in VALID_STATUS:
                errors.append(f"{p.name}: invalid status {data.get('status')}")
            if data.get("priority") not in priorities:
                errors.append(f"{p.name}: invalid priority {data.get('priority')}")
            if data.get("stage") not in stages:
                errors.append(f"{p.name}: invalid stage {data.get('stage')}")
            refs = data.get("gate_refs")
            if not isinstance(refs, list) or (data.get("status") == "active" and len(refs) == 0):
                errors.append(f"{p.name}: invalid gate_refs for status={data.get('status')}")
            impls = data.get("implementations")
            if not isinstance(impls, list):
                errors.append(f"{p.name}: implementations must be an array")
            elif data.get("status") == "active" and len(impls) == 0:
                errors.append(f"{p.name}: active contract requires non-empty implementations[]")
            else:
                for i, impl in enumerate(impls):
                    if not isinstance(impl, dict):
                        errors.append(f"{p.name}: implementations[{i}] must be object")
                        continue
                    miss_impl = IMPL_REQUIRED_FIELDS - set(impl.keys())
                    if miss_impl:
                        errors.append(f"{p.name}: implementations[{i}] missing {sorted(miss_impl)}")
                    if impl.get("entry_kind") not in VALID_ENTRY_KIND:
                        errors.append(f"{p.name}: implementations[{i}] invalid entry_kind")
            errors.extend(validate_implementations(data, gate_by_id, repo, allowed_prefixes))

    if checked == 0:
        errors.append("no instance files found")
    if errors:
        for e in errors:
            print(f"[FAIL] {e}")
        return 2
    print(f"[OK] contract instance validation passed ({checked} files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

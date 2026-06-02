#!/usr/bin/env python3
"""
Compile D-scheme contracts into a lock plan.

Inputs:
- docs/contracts/index.json
- docs/gates/gate_registry.json
- docs/contracts/<type>/*.json

Output:
- docs/reports/contract_plan.lock.json
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass
class CompileResult:
    plan: dict[str, Any]
    errors: list[str]


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compile_plan(repo: Path, index_path: Path, gate_path: Path) -> CompileResult:
    _lib = Path(__file__).resolve().parents[1] / "lib"
    if str(_lib) not in sys.path:
        sys.path.insert(0, str(_lib))
    from contract_schema import (
        resolve_implementations,
        validate_gate_registry_commands,
        validate_implementations,
    )

    errors: list[str] = []
    idx = _load_json(index_path)
    gate_registry = _load_json(gate_path)
    allowed_prefixes = idx.get("allowed_entry_prefixes", [])

    contracts: list[dict[str, Any]] = []
    file_hashes: dict[str, str] = {}

    roots = idx.get("contract_roots", {})
    for ctype, rel_root in roots.items():
        root = repo / rel_root
        if not root.is_dir():
            errors.append(f"missing contract root: {rel_root}")
            continue
        for path in sorted(root.glob("*.v1.json")):
            data = _load_json(path)
            data["_path"] = str(path.relative_to(repo)).replace("\\", "/")
            data["_type_dir"] = ctype
            contracts.append(data)
            file_hashes[data["_path"]] = _sha256(path)

    gates = gate_registry.get("gates", [])
    gate_by_id = {g.get("gate_id"): g for g in gates if g.get("gate_id")}
    errors.extend(validate_gate_registry_commands(gates, repo, allowed_prefixes))

    gate_refs_from_contracts: set[tuple[str, str]] = set()
    contract_refs_from_gates: set[tuple[str, str]] = set()

    for c in contracts:
        cid = c.get("id")
        if not cid:
            errors.append(f"{c.get('_path')}: missing id")
            continue
        status = c.get("status")
        refs = c.get("gate_refs")
        if status == "active":
            if not isinstance(refs, list) or not refs:
                errors.append(f"{cid}: active contract must define non-empty gate_refs")
        for gid in refs or []:
            if gid not in gate_by_id:
                errors.append(f"{cid}: unknown gate_ref {gid}")
            gate_refs_from_contracts.add((cid, gid))
        errors.extend(validate_implementations(c, gate_by_id, repo, allowed_prefixes))

    for g in gates:
        gid = g.get("gate_id")
        if not gid:
            errors.append("gate entry missing gate_id")
            continue
        for cid in g.get("covers", []):
            contract_refs_from_gates.add((cid, gid))

    if gate_refs_from_contracts != contract_refs_from_gates:
        only_contract = sorted(gate_refs_from_contracts - contract_refs_from_gates)
        only_gate = sorted(contract_refs_from_gates - gate_refs_from_contracts)
        if only_contract:
            errors.append(f"binding mismatch (contract-only): {only_contract}")
        if only_gate:
            errors.append(f"binding mismatch (gate-only): {only_gate}")

    for c in contracts:
        pr = c.get("priority")
        if pr not in idx.get("priority_order", []):
            errors.append(f"{c.get('id')}: invalid priority {pr}")
        st = c.get("stage")
        if st not in idx.get("stage_order", []):
            errors.append(f"{c.get('id')}: invalid stage {st}")

    plan = {
        "version": "v1",
        "index_file": str(index_path.relative_to(repo)).replace("\\", "/"),
        "gate_registry_file": str(gate_path.relative_to(repo)).replace("\\", "/"),
        "repository_layout": idx.get("repository_layout", {}),
        "script_layout": idx.get("script_layout", {}),
        "add_gate_workflow": idx.get("add_gate_workflow", []),
        "policy": idx.get("global_policy", {}),
        "priority_order": idx.get("priority_order", []),
        "stage_order": idx.get("stage_order", []),
        "contracts": [
            {
                "id": c.get("id"),
                "type": c.get("type"),
                "version": c.get("version"),
                "status": c.get("status"),
                "priority": c.get("priority"),
                "stage": c.get("stage"),
                "blocking": bool(c.get("blocking", False)),
                "gate_refs": c.get("gate_refs", []),
                "implementations": c.get("implementations", []),
                "resolved_implementations": resolve_implementations(c, gate_by_id),
                "path": c.get("_path"),
            }
            for c in contracts
        ],
        "gates": gates,
        "hashes": {
            "index": _sha256(index_path),
            "gate_registry": _sha256(gate_path),
            "contracts": file_hashes,
        },
    }
    return CompileResult(plan=plan, errors=errors)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--index",
        default="docs/contracts/index.json",
        help="Path to contract index",
    )
    parser.add_argument(
        "--gate-registry",
        default="docs/gates/gate_registry.json",
        help="Path to gate registry",
    )
    parser.add_argument(
        "--output",
        default="docs/reports/contract_plan.lock.json",
        help="Output lock plan",
    )
    args = parser.parse_args()

    _lib = Path(__file__).resolve().parents[1] / "lib"
    if str(_lib) not in sys.path:
        sys.path.insert(0, str(_lib))
    from repo_paths import repo_root

    repo = repo_root()
    index_path = repo / args.index
    gate_path = repo / args.gate_registry
    out_path = repo / args.output

    result = compile_plan(repo, index_path, gate_path)
    if result.errors:
        for e in result.errors:
            print(f"[FAIL] {e}")
        return 2

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(result.plan, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"[OK] contract lock plan generated: {out_path.relative_to(repo)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

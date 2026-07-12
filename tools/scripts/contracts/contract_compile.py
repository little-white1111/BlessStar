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


def _compile_gate_rule_def(
    gate_rule_def_path: Path,
    repo: Path,
) -> tuple[list[dict[str, Any]], list[str]]:
    """Compile gate_rule_def.json into lock plan contract entries.

    Only rules with scenario ending in _COMPILE_TIME are included.
    Returns (contracts, errors).
    """
    from contract_schema import (
        COMPILE_TIME_SUFFIX,
        validate_gate_rule_def,
    )

    errors: list[str] = []
    if not gate_rule_def_path.is_file():
        return [], errors

    try:
        rules: list[dict[str, Any]] = json.loads(
            gate_rule_def_path.read_text(encoding="utf-8")
        )
    except json.JSONDecodeError as e:
        errors.append(f"gate_rule_def.json: invalid JSON: {e}")
        return [], errors

    if not isinstance(rules, list):
        errors.append("gate_rule_def.json: root must be a JSON array")
        return [], errors

    # Validate schema
    schema_errors = validate_gate_rule_def(rules)
    errors.extend(schema_errors)
    if schema_errors:
        return [], errors

    # Filter _COMPILE_TIME rules and create lock plan entries
    contracts: list[dict[str, Any]] = []
    for rule in rules:
        scenario = rule.get("scenario", "")
        if not scenario.endswith(COMPILE_TIME_SUFFIX):
            continue

        field_key = rule.get("field_key", "")
        op = rule.get("op", "")
        val = rule.get("value", "")
        error_hint = rule.get("error_hint", "")
        stable_key = rule.get("stable_key", "")

        cid = f"{field_key}_COMPILE"
        contract = {
            "id": cid,
            "type": "gate_rule_def",
            "version": "1.0",
            "status": "active",
            "priority": "must",
            "stage": "compile_time",
            "blocking": True,
            "gate_refs": ["compile_time_scanner"],
            "implementations": [
                {
                    "gate_id": "compile_time_scanner",
                    "runner": "python",
                    "entry": "tools/scripts/contracts/compile_time_scanner.py",
                    "entry_kind": "script",
                }
            ],
            "rule": {
                "field_key": field_key,
                "op": op,
                "value": val,
                "error_hint": error_hint,
                "stable_key": stable_key,
            },
            "_stable_key": stable_key,
        }
        contracts.append(contract)

    return contracts, errors


def compile_plan(
    repo: Path,
    index_path: Path,
    gate_path: Path,
    gate_rule_def_path: Path | None = None,
) -> CompileResult:
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

    # Compile gate_rule_def.json entries (new contract type for CI scanning)
    gate_rule_def_contracts, grd_errors = _compile_gate_rule_def(
        gate_rule_def_path, repo
    ) if gate_rule_def_path else ([], [])
    errors.extend(grd_errors)
    contracts.extend(gate_rule_def_contracts)
    for c in gate_rule_def_contracts:
        cpath = c.get("id", "") + "(gate_rule_def)"
        file_hashes[cpath] = _sha256(gate_rule_def_path) if gate_rule_def_path else ""

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

    # Ensure stage_order contains compile_time if gate_rule_def contracts exist
    stage_order = list(idx.get("stage_order", []))
    if gate_rule_def_contracts and "compile_time" not in stage_order:
        # Insert compile_time after 'ci' or at the beginning
        if "ci" in stage_order:
            ci_idx = stage_order.index("ci")
            stage_order.insert(ci_idx + 1, "compile_time")
        else:
            stage_order.append("compile_time")

    plan = {
        "version": "v1",
        "index_file": str(index_path.relative_to(repo)).replace("\\", "/"),
        "gate_registry_file": str(gate_path.relative_to(repo)).replace("\\", "/"),
        "repository_layout": idx.get("repository_layout", {}),
        "script_layout": idx.get("script_layout", {}),
        "add_gate_workflow": idx.get("add_gate_workflow", []),
        "policy": idx.get("global_policy", {}),
        "priority_order": idx.get("priority_order", []),
        "stage_order": stage_order,
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
        "--gate-rule-def",
        default=None,
        help="Path to gate_rule_def.json (CDD Skill output, optional)",
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
    gate_rule_def_path = repo / args.gate_rule_def if args.gate_rule_def else None

    result = compile_plan(repo, index_path, gate_path, gate_rule_def_path)
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

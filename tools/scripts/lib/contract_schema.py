#!/usr/bin/env python3
"""Shared contract D-scheme validation (index conventions + implementations)."""

from __future__ import annotations

import re
from pathlib import Path
from typing import Any

COMPILE_TIME_SUFFIX = "_COMPILE_TIME"

# ── gate_rule_def schema validation ──────────────────────────────────

VALID_OPS = {"eq", "neq", "gt", "gte", "lt", "lte", "range", "in", "nin", "match", "exists"}

LAYER_SUB_CATEGORY_MAP: dict[int, set[str]] = {
    0: {"threshold", "format", "consistency"},
    1: {"approval", "policy"},
    2: {"approval", "policy"},
}


def is_compile_time_rule(rule: dict[str, Any]) -> bool:
    """Check if a rule scenario ends with the compile-time suffix."""
    scenario = rule.get("scenario", "")
    return scenario.endswith(COMPILE_TIME_SUFFIX)


def validate_gate_rule_def(rules: list[dict[str, Any]]) -> list[str]:
    """Validate a list of gate_rule_def entries.

    Returns a list of error strings (empty = valid).
    """
    errors: list[str] = []
    seen_keys: set[str] = set()

    REQUIRED_FIELDS = [
        "field_key", "field_type", "op", "value",
        "scenario", "layer", "sub_category", "stable_key",
    ]

    for i, rule in enumerate(rules):
        prefix = f"rule[{i}]"

        # Required fields
        for field in REQUIRED_FIELDS:
            if field not in rule:
                errors.append(f"{prefix}: missing required field '{field}'")
                continue

        layer = rule.get("layer")
        sub_cat = rule.get("sub_category", "")
        op = rule.get("op", "")

        # Layer/sub_category compatibility
        if isinstance(layer, int) and sub_cat:
            allowed = LAYER_SUB_CATEGORY_MAP.get(layer, set())
            if allowed and sub_cat not in allowed:
                errors.append(
                    f"{prefix}: layer={layer} sub_category={sub_cat!r} not in allowed set {allowed}; "
                    "layer>=1 should use approval/policy"
                )

        # Valid operator
        if op and op not in VALID_OPS:
            errors.append(f"{prefix}: invalid op={op!r}, valid ops: {sorted(VALID_OPS)}")

        # Compile-time restrictions
        if is_compile_time_rule(rule):
            if op in {"match", "exists"}:
                errors.append(f"{prefix}: op={op!r} is not supported at compile time")

        # Duplicate stable_key check
        stable_key = rule.get("stable_key", "")
        if stable_key:
            if stable_key in seen_keys:
                errors.append(f"{prefix}: duplicate stable_key={stable_key!r}")
            seen_keys.add(stable_key)

    return errors


PYTHON_SCRIPT_RE = re.compile(
    r"(?:^|[\s\"'])((?:tools/(?:scripts|purity)|ops/(?:acceptance|smoke))[^\s\"']+\.py)"
)


def load_index(index: dict[str, Any]) -> dict[str, Any]:
    layout = index.get("script_layout", {})
    prefixes = index.get("allowed_entry_prefixes", [])
    workflow = index.get("add_gate_workflow", [])
    gate_registry = index.get("repository_layout", {}).get(
        "gate_registry", "docs/gates/gate_registry.json"
    )
    return {
        "script_layout": layout,
        "allowed_entry_prefixes": prefixes,
        "add_gate_workflow": workflow,
        "gate_registry": gate_registry,
    }


def extract_python_script(command: str) -> str | None:
    m = PYTHON_SCRIPT_RE.search(command)
    if m:
        return m.group(1).replace("\\", "/")
    return None


def normalize_entry(entry: str) -> str:
    return entry.replace("\\", "/").strip()


def entry_allowed(entry: str, entry_kind: str, allowed_prefixes: list[str]) -> bool:
    e = normalize_entry(entry)
    if entry_kind == "command":
        return e.startswith("ctest ")
    if entry_kind == "script":
        return any(e.startswith(p) for p in allowed_prefixes if p.endswith("/"))
    return False


def validate_implementations(
    contract: dict[str, Any],
    gate_by_id: dict[str, dict[str, Any]],
    repo: Path,
    allowed_prefixes: list[str],
) -> list[str]:
    errors: list[str] = []
    cid = contract.get("id", "?")
    path = contract.get("_path", cid)
    status = contract.get("status")
    gate_refs = contract.get("gate_refs") or []
    impls = contract.get("implementations")

    if status != "active":
        return errors

    if not isinstance(impls, list) or not impls:
        errors.append(f"{path}: active contract must define non-empty implementations[]")
        return errors

    impl_gate_ids = [i.get("gate_id") for i in impls if isinstance(i, dict)]
    if set(impl_gate_ids) != set(gate_refs):
        errors.append(
            f"{path}: implementations gate_id set {sorted(impl_gate_ids)} "
            f"!= gate_refs {sorted(gate_refs)}"
        )

    for impl in impls:
        if not isinstance(impl, dict):
            errors.append(f"{path}: implementations[] entries must be objects")
            continue
        gid = impl.get("gate_id")
        runner = impl.get("runner")
        entry = impl.get("entry")
        entry_kind = impl.get("entry_kind")
        if not gid or not runner or not entry or not entry_kind:
            errors.append(f"{path}: implementation for {gid!r} needs gate_id, runner, entry, entry_kind")
            continue
        if gid not in gate_by_id:
            errors.append(f"{path}: unknown implementation gate_id {gid}")
            continue
        gate = gate_by_id[gid]
        if runner != gate.get("runner"):
            errors.append(f"{path}: {gid} runner {runner!r} != gate_registry {gate.get('runner')!r}")
        gate_cmd = str(gate.get("command", "")).strip()
        entry_n = normalize_entry(str(entry))
        if entry_kind == "command":
            if entry_n != gate_cmd:
                errors.append(f"{path}: {gid} entry command does not match gate_registry.command")
            if not entry_n.startswith("ctest "):
                errors.append(f"{path}: {gid} entry_kind=command must start with 'ctest '")
        elif entry_kind == "script":
            script = extract_python_script(gate_cmd)
            if script and normalize_entry(script) != entry_n:
                errors.append(
                    f"{path}: {gid} entry script {entry_n!r} != parsed from gate command {script!r}"
                )
            if not entry_allowed(entry_n, "script", allowed_prefixes):
                errors.append(f"{path}: {gid} script entry not under allowed_entry_prefixes")
            script_path = repo / entry_n
            if not script_path.is_file():
                errors.append(f"{path}: {gid} script missing: {entry_n}")
        else:
            errors.append(f"{path}: {gid} invalid entry_kind {entry_kind!r} (use script|command)")

    return errors


def validate_gate_registry_commands(
    gates: list[dict[str, Any]],
    repo: Path,
    allowed_prefixes: list[str],
) -> list[str]:
    errors: list[str] = []
    for gate in gates:
        gid = gate.get("gate_id", "?")
        cmd = str(gate.get("command", "")).strip()
        runner = gate.get("runner")
        if runner == "python":
            script = extract_python_script(cmd)
            if not script:
                errors.append(f"{gid}: cannot parse python script from command")
                continue
            if not entry_allowed(script, "script", allowed_prefixes):
                errors.append(f"{gid}: command script not under allowed_entry_prefixes: {script}")
            if not (repo / script).is_file():
                errors.append(f"{gid}: command points to missing script: {script}")
        elif runner == "ctest":
            if not cmd.startswith("ctest "):
                errors.append(f"{gid}: ctest runner command must start with 'ctest '")
        else:
            errors.append(f"{gid}: unknown runner {runner!r}")
    return errors


def resolve_implementations(
    contract: dict[str, Any],
    gate_by_id: dict[str, dict[str, Any]],
) -> list[dict[str, str]]:
    out: list[dict[str, str]] = []
    for impl in contract.get("implementations") or []:
        if not isinstance(impl, dict):
            continue
        gid = str(impl.get("gate_id", ""))
        gate = gate_by_id.get(gid, {})
        out.append(
            {
                "gate_id": gid,
                "runner": str(impl.get("runner", gate.get("runner", ""))),
                "entry": str(impl.get("entry", "")),
                "entry_kind": str(impl.get("entry_kind", "")),
                "resolved_command": str(gate.get("command", "")),
            }
        )
    return out

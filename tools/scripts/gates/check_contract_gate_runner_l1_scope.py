#!/usr/bin/env python3
"""GATE-TEST-L1-GATE-SCOPE: L1 contract_gate_runner stage ceiling and L2/L3 isolation (C-TST-GATE-SCOPE-1)."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[3]
_TESTS_CMAKE = _REPO / "cmake/Tests.cmake"
_GATE_REGISTRY = _REPO / "docs/gates/gate_registry.json"
_TIER_FILE = _REPO / "docs/contracts/testing/tier_assignments.json"
_STAGING_SCENARIOS = _REPO / "ops/acceptance/staging/scenarios.v1.json"
_GATE_RUNNER = _REPO / "tools/scripts/contracts/contract_gate_runner.py"
_L1_CONTRACT = _REPO / "docs/contracts/testing/C-TST-L1-1.v1.json"
_SCOPE_CONTRACT = _REPO / "docs/contracts/testing/C-TST-GATE-SCOPE-1.v1.json"

_GATE_RUNNER_TEST = "bs_test_day17_contract_gate_runner"
_REQUIRED_THROUGH_STAGE = "ci"
_L2_BULK_LABELS = frozenset({"integration", "day14"})


def _fail(msg: str) -> None:
    print(f"[FAIL] {msg}", file=sys.stderr)


def _check_gate_runner_ctest() -> bool:
    text = _TESTS_CMAKE.read_text(encoding="utf-8", errors="replace")
    block = re.search(
        rf"NAME {_GATE_RUNNER_TEST}.*?set_tests_properties\({_GATE_RUNNER_TEST}",
        text,
        re.DOTALL,
    )
    if not block:
        _fail(f"missing ctest NAME {_GATE_RUNNER_TEST} in cmake/Tests.cmake")
        return False
    chunk = block.group(0)
    if "--through-stage" not in chunk or _REQUIRED_THROUGH_STAGE not in chunk:
        _fail(
            f"{_GATE_RUNNER_TEST} must pass --through-stage {_REQUIRED_THROUGH_STAGE} "
            "in cmake/Tests.cmake"
        )
        return False
    if re.search(r"-L\s+integration", chunk) or re.search(r"-L\s+day14", chunk):
        _fail(f"{_GATE_RUNNER_TEST} must not invoke ctest -L integration/day14 in L1")
        return False
    return True


def _gate_command(gates: list[dict], gate_id: str) -> str | None:
    for g in gates:
        if g.get("gate_id") == gate_id:
            return str(g.get("command", ""))
    return None


def _check_gate_registry() -> bool:
    data = json.loads(_GATE_REGISTRY.read_text(encoding="utf-8"))
    gates = data.get("gates", [])
    ok = True
    staging = _gate_command(gates, "GATE-STAGING-ACCEPTANCE") or ""
    if "--dry-run" not in staging:
        _fail("GATE-STAGING-ACCEPTANCE command must include --dry-run for dev registry default")
        ok = False
    prod = _gate_command(gates, "GATE-PROD-SMOKE") or ""
    if "--dry-run" not in prod:
        _fail("GATE-PROD-SMOKE command must include --dry-run for dev registry default")
        ok = False
    return ok


def _check_tier_forbidden_in() -> bool:
    tier = json.loads(_TIER_FILE.read_text(encoding="utf-8"))
    ok = True
    for key in ("L2_staging_acceptance", "L3_prod_smoke"):
        section = tier.get(key)
        if not isinstance(section, dict):
            _fail(f"tier_assignments.json missing {key}")
            ok = False
            continue
        forbidden = section.get("forbidden_in") or []
        if "L1_dev_ci" not in forbidden:
            _fail(f"{key}.forbidden_in must include L1_dev_ci")
            ok = False
    return ok


def _check_l2_bulk_labels_not_in_l1_ctest() -> bool:
    """L2 bulk ctest_label scenarios must not be wired as day17 script tests."""
    if not _STAGING_SCENARIOS.is_file():
        _fail(f"missing {_STAGING_SCENARIOS}")
        return False
    scenarios = json.loads(_STAGING_SCENARIOS.read_text(encoding="utf-8")).get("scenarios", [])
    bulk_labels: set[str] = set()
    for sc in scenarios:
        if sc.get("kind") == "ctest_label" and sc.get("label"):
            bulk_labels.add(str(sc["label"]))

    text = _TESTS_CMAKE.read_text(encoding="utf-8", errors="replace")
    ok = True
    for m in re.finditer(
        r"add_test\(\s*NAME (bs_test_day17_\w+).*?set_tests_properties\(\1",
        text,
        re.DOTALL,
    ):
        block = m.group(0)
        for label in bulk_labels & _L2_BULK_LABELS:
            if re.search(rf"-L\s+{re.escape(label)}\b", block):
                _fail(
                    f"{m.group(1)} must not use ctest -L {label} "
                    "(L2 bulk label; use scenarios.v1.json on Staging only)"
                )
                ok = False
    return ok


def _check_gate_runner_supports_through_stage() -> bool:
    text = _GATE_RUNNER.read_text(encoding="utf-8", errors="replace")
    if "--through-stage" not in text:
        _fail("contract_gate_runner.py must support --through-stage")
        return False
    return True


def _check_contract_rules() -> bool:
    ok = True
    for path, needles in (
        (_SCOPE_CONTRACT, ["through-stage", "ci", "staging"]),
        (_L1_CONTRACT, ["through-stage", "L1"]),
    ):
        if not path.is_file():
            _fail(f"missing contract instance {path.name}")
            ok = False
            continue
        rule = json.loads(path.read_text(encoding="utf-8")).get("rule", "").lower()
        for needle in needles:
            if needle.lower() not in rule:
                _fail(f"{path.name} rule must mention '{needle}'")
                ok = False
    return ok


def main() -> int:
    checks = [
        _check_gate_runner_ctest,
        _check_gate_registry,
        _check_tier_forbidden_in,
        _check_l2_bulk_labels_not_in_l1_ctest,
        _check_gate_runner_supports_through_stage,
        _check_contract_rules,
    ]
    if not all(fn() for fn in checks):
        return 2
    print("[OK] C-TST-GATE-SCOPE-1 L1 gate scope check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

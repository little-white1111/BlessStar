#!/usr/bin/env python3
"""
Run D-scheme contract gate by lock plan (stage-first + fail-fast).
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def _load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _run_command(command: str, cwd: Path) -> tuple[int, str]:
    """Run gate command; capture to a temp file (avoid Windows PIPE buffer deadlock)."""
    with tempfile.NamedTemporaryFile(
        mode="w+", encoding="utf-8", errors="replace", delete=False, suffix=".log"
    ) as log_f:
        log_path = Path(log_f.name)
    try:
        with log_path.open("w", encoding="utf-8", errors="replace") as out_f:
            proc = subprocess.run(
                command,
                cwd=str(cwd),
                shell=True,
                stdout=out_f,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
        output = log_path.read_text(encoding="utf-8", errors="replace")
        return proc.returncode, output
    finally:
        log_path.unlink(missing_ok=True)


def _priority_rank(priority_order: list[str]) -> dict[str, int]:
    return {p: i for i, p in enumerate(priority_order)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--lock-plan",
        default="docs/reports/contract_plan.lock.json",
        help="Path to lock plan",
    )
    parser.add_argument(
        "--draft-policy",
        choices=("warn", "block"),
        default=None,
        help="Override draft policy from lock plan",
    )
    parser.add_argument(
        "--through-stage",
        default=None,
        help="Run gates only through this stage (inclusive), e.g. ci for L1 dev CI; skips staging/prod_ops",
    )
    args = parser.parse_args()

    _lib = Path(__file__).resolve().parents[1] / "lib"
    if str(_lib) not in sys.path:
        sys.path.insert(0, str(_lib))
    from repo_paths import repo_root

    repo = repo_root()
    lock_path = repo / args.lock_plan
    if not lock_path.exists():
        print(f"[FAIL] lock plan missing: {args.lock_plan}")
        print("[HINT] run: python tools/scripts/contracts/contract_compile.py")
        return 3

    plan = _load_json(lock_path)
    policy = dict(plan.get("policy", {}))
    if args.draft_policy:
        policy["draft_policy"] = args.draft_policy

    fail_fast = bool(policy.get("fail_fast", True))
    draft_policy = str(policy.get("draft_policy", "warn"))
    report_dir = repo / str(policy.get("report_dir", "docs/reports"))
    report_dir.mkdir(parents=True, exist_ok=True)

    contracts = plan.get("contracts", [])
    gates = plan.get("gates", [])
    gate_by_id = {g.get("gate_id"): g for g in gates}
    contracts_by_gate: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for c in contracts:
        for gid in c.get("gate_refs", []):
            contracts_by_gate[gid].append(c)

    stage_order = plan.get("stage_order", [])
    through_stage = args.through_stage
    if through_stage and through_stage not in stage_order:
        print(f"[FAIL] unknown --through-stage '{through_stage}'")
        print(f"[HINT] allowed: {', '.join(stage_order)}")
        return 2
    pr_rank = _priority_rank(plan.get("priority_order", []))

    results: list[dict[str, Any]] = []
    overall_ok = True
    stop = False

    for stage in stage_order:
        if through_stage and stage_order.index(stage) > stage_order.index(through_stage):
            results.append(
                {
                    "gate_id": f"(stage:{stage})",
                    "stage": stage,
                    "result": "SKIP",
                    "reason": f"after --through-stage {through_stage}",
                    "contracts": [],
                }
            )
            continue
        if stop:
            break
        stage_gates = [g for g in gates if g.get("stage") == stage]
        stage_gates.sort(key=lambda g: g.get("gate_id", ""))
        for gate in stage_gates:
            gid = gate.get("gate_id")
            bound = contracts_by_gate.get(gid, [])
            active = [c for c in bound if c.get("status") == "active"]
            drafts = [c for c in bound if c.get("status") == "draft"]

            if not active and draft_policy == "warn":
                results.append(
                    {
                        "gate_id": gid,
                        "stage": stage,
                        "result": "SKIP",
                        "reason": "no active contracts",
                        "contracts": [c.get("id") for c in bound],
                    }
                )
                continue

            rc, output = _run_command(str(gate.get("command", "")), repo)
            gate_ok = rc == 0
            if not gate_ok:
                overall_ok = False

            impl_by_gate: dict[str, list[dict[str, str]]] = {}
            for c in bound:
                for ri in c.get("resolved_implementations") or c.get("implementations") or []:
                    if isinstance(ri, dict) and ri.get("gate_id") == gid:
                        impl_by_gate.setdefault(gid, []).append(
                            {
                                "contract_id": str(c.get("id", "")),
                                "entry": str(ri.get("entry", "")),
                                "entry_kind": str(ri.get("entry_kind", "")),
                                "resolved_command": str(
                                    ri.get("resolved_command", gate.get("command", ""))
                                ),
                            }
                        )

            results.append(
                {
                    "gate_id": gid,
                    "stage": stage,
                    "result": "PASS" if gate_ok else "FAIL",
                    "exit_code": rc,
                    "contracts": [c.get("id") for c in bound],
                    "active_contracts": [c.get("id") for c in active],
                    "draft_contracts": [c.get("id") for c in drafts],
                    "command": gate.get("command"),
                    "verify_entries": impl_by_gate.get(gid, []),
                    "output": output[-12000:],
                }
            )

            if not gate_ok and fail_fast:
                # stop when any high-priority blocking contract is linked to this gate
                for c in active:
                    if not c.get("blocking", False):
                        continue
                    pr = c.get("priority")
                    if pr in pr_rank and pr_rank[pr] <= pr_rank.get("style", 2):
                        stop = True
                        break
                if stop:
                    break

    report = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "policy": {
            "fail_fast": fail_fast,
            "draft_policy": draft_policy,
            "report_dir": str(report_dir.relative_to(repo)).replace("\\", "/"),
        },
        "lock_plan": str(lock_path.relative_to(repo)).replace("\\", "/"),
        "result": "PASS" if overall_ok else "FAIL",
        "results": results,
    }

    json_path = report_dir / "contract-gate-report.json"
    md_path = report_dir / "contract-gate-report.md"
    json_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# Contract Gate Report",
        "",
        f"- Result: **{report['result']}**",
        f"- Fail-fast: `{fail_fast}`",
        f"- Draft policy: `{draft_policy}`",
        "",
        "| Gate | Stage | Result | Contracts |",
        "|------|-------|--------|-----------|",
    ]
    for r in results:
        lines.append(
            f"| `{r.get('gate_id')}` | `{r.get('stage')}` | `{r.get('result')}` | "
            f"{', '.join(r.get('contracts', []))} |"
        )
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(f"[OK] report generated: {json_path.relative_to(repo)}")
    print(f"[OK] report generated: {md_path.relative_to(repo)}")

    if not overall_ok:
        failed = [r for r in results if r.get("result") == "FAIL"]
        print(f"[FAIL] {len(failed)} gate(s) failed:")
        for r in failed[:10]:
            gid = r.get("gate_id")
            stage = r.get("stage")
            rc = r.get("exit_code")
            print(f"  - {gid} (stage={stage}, exit_code={rc})")
        # Print tail output of first failed gate to help CI diagnosis.
        if failed:
            r0 = failed[0]
            gid = r0.get("gate_id")
            stage = r0.get("stage")
            rc = r0.get("exit_code")
            out = str(r0.get("output") or "")
            tail = out.splitlines()[-80:]
            print(f"[FAIL] first failed gate output tail: {gid} (stage={stage}, exit_code={rc})")
            for line in tail:
                print(line)
    return 0 if overall_ok else 2


if __name__ == "__main__":
    raise SystemExit(main())

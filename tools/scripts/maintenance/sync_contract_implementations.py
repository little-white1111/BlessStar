#!/usr/bin/env python3
"""One-shot: sync implementations[] from gate_refs + gate_registry (idempotent)."""

from __future__ import annotations

import json
import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parents[1] / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from contract_schema import extract_python_script  # noqa: E402


def _impl_for_gate(gate: dict) -> dict:
    gid = gate["gate_id"]
    cmd = str(gate["command"]).strip()
    if gate["runner"] == "python":
        script = extract_python_script(cmd)
        if not script:
            raise ValueError(f"{gid}: cannot parse python script from command")
        return {
            "gate_id": gid,
            "runner": "python",
            "entry": script,
            "entry_kind": "script",
        }
    return {
        "gate_id": gid,
        "runner": "ctest",
        "entry": cmd,
        "entry_kind": "command",
    }


def main() -> int:
    repo = Path(__file__).resolve().parents[3]
    idx = json.loads((repo / "docs/contracts/index.json").read_text(encoding="utf-8"))
    gates = {
        g["gate_id"]: g
        for g in json.loads((repo / "docs/gates/gate_registry.json").read_text(encoding="utf-8"))[
            "gates"
        ]
    }
    updated = 0
    for root_rel in idx["contract_roots"].values():
        for path in sorted((repo / root_rel).glob("*.v1.json")):
            data = json.loads(path.read_text(encoding="utf-8"))
            refs = data.get("gate_refs") or []
            data["implementations"] = [_impl_for_gate(gates[gid]) for gid in refs]
            path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            updated += 1
            print(f"[OK] {path.relative_to(repo)}")
    print(f"[OK] synced {updated} contract instance(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

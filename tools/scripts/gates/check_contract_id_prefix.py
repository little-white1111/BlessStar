#!/usr/bin/env python3
"""C-ST-4: contract IDs in JSON use allowed prefixes."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parents[1] / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import repo_root

ALLOWED = re.compile(r"^C-(IX|ST|FN|CF|TST)(-[A-Za-z0-9-]+)+$")


def main() -> int:
    root = repo_root()
    contracts_dir = root / "docs" / "contracts"
    bad: list[str] = []
    for path in sorted(contracts_dir.glob("*.contracts.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        for item in data.get("contracts", []):
            cid = item.get("id", "")
            if not ALLOWED.match(cid):
                bad.append(f"C-ST-4: {path.name}: invalid id '{cid}'")
    if bad:
        for line in bad:
            print(f"[FAIL] {line}")
        return 2
    print("[OK] C-ST-4 contract id prefix check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

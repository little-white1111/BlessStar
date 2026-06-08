#!/usr/bin/env python3
"""
Resolve CI loop dispatch parameters: target workflow + branch + route (via-ci | direct).

Examples:
  python tools/ci/resolve_ci_target.py list
  python tools/ci/resolve_ci_target.py --target day21 --ref feat/foo --dispatch-ref main
  python tools/ci/resolve_ci_target.py --target day19-smoke --ref day19-stress-smoke --route direct
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[2]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from tools.ci.workflow_registry import TARGETS, resolve_dispatch  # noqa: E402


def cmd_list() -> int:
    for key, cfg in sorted(TARGETS.items()):
        via = cfg["via_ci"]
        direct = cfg["direct"]
        print(f"{key}")
        print(f"  {cfg['label']}")
        print(
            f"  via-ci : {via['workflow_file']} "
            f"(suite={via.get('suite', 'full')}, timeout={via.get('timeout')}s)"
        )
        print(
            f"  direct : {direct['workflow_file']} "
            f"(timeout={direct.get('timeout')}s)"
        )
    return 0


def main() -> int:
    if len(sys.argv) > 1 and sys.argv[1] == "list":
        return cmd_list()

    ap = argparse.ArgumentParser(description="Resolve CI dispatch target to workflow + refs")
    ap.add_argument("--target", "-t", default="ci", help="ci | day21 | day19-smoke | ...")
    ap.add_argument("--ref", "-r", required=True, help="branch whose code to test")
    ap.add_argument(
        "--dispatch-ref",
        "-d",
        default=None,
        help="workflow definition branch (via-ci default: main; direct default: --ref)",
    )
    ap.add_argument(
        "--route",
        choices=["via-ci", "direct"],
        default="via-ci",
        help="via-ci = ci.yml suite router; direct = standalone workflow yml",
    )
    args = ap.parse_args()

    try:
        plan = resolve_dispatch(
            target=args.target,
            test_ref=args.ref,
            dispatch_ref=args.dispatch_ref,
            route=args.route,
        )
    except ValueError as exc:
        print(f"[resolve] ERROR: {exc}", file=sys.stderr)
        return 1

    print(json.dumps(plan, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())

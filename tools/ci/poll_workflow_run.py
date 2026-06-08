#!/usr/bin/env python3
"""Poll latest workflow run for a branch until completed."""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[2]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from tools.ci.fetch_ci_errors import BASE, REPO, fetch_json, load_github_token  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--workflow", default="day19-stress-smoke.yml")
    ap.add_argument("--branch", default="day19-stress-smoke")
    ap.add_argument("--interval", type=int, default=30)
    ap.add_argument("--max-wait", type=int, default=1800)
    args = ap.parse_args()

    token = load_github_token() or ""
    if not token:
        print("missing token", file=sys.stderr)
        return 1

    url = (
        f"{BASE}/repos/{REPO}/actions/workflows/{args.workflow}/runs"
        f"?branch={args.branch}&per_page=1"
    )
    deadline = time.time() + args.max_wait
    while time.time() < deadline:
        runs = fetch_json(url, token).get("workflow_runs") or []
        if not runs:
            print("waiting for run to appear ...", flush=True)
            time.sleep(args.interval)
            continue
        run = runs[0]
        print(
            f"run={run['id']} status={run['status']} "
            f"conclusion={run.get('conclusion')} url={run.get('html_url')}",
            flush=True,
        )
        if run["status"] == "completed":
            return 0 if run.get("conclusion") == "success" else 1
        time.sleep(args.interval)

    print("poll timeout", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""
Wait for the latest workflow run on a ref to complete, then print run id and conclusion.

Typical usage:
  python tools/ci/trigger_workflow_dispatch.py --workflow ci.yml --ref my-branch
  python tools/ci/wait_workflow_run.py --branch my-branch --workflow_name "full test" --timeout 3600
  python tools/ci/wait_workflow_run.py --branch feat/day21-foo --workflow_name day21 --timeout 1800
  python tools/ci/fetch_ci_errors.py --branch my-branch --run <run_id> --save-logs

Notes:
  - Uses GitHub REST API with token (same loader as fetch_ci_errors.py).
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
import urllib.error
import urllib.parse
import urllib.request

# Ensure repo root is on sys.path no matter where invoked.
_ROOT = Path(__file__).resolve().parents[2]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from tools.ci.fetch_ci_errors import BASE, REPO, load_github_token  # type: ignore


def _headers(token: str) -> dict[str, str]:
    return {
        "Authorization": f"token {token}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "BlessStar-ci-wait/1.0",
    }


def fetch_json(url: str, token: str, max_retries: int = 5) -> dict:
    req = urllib.request.Request(url, headers=_headers(token))
    last_err: Exception | None = None
    for attempt in range(1, max_retries + 1):
        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except urllib.error.HTTPError as exc:
            body = exc.read().decode("utf-8", errors="replace")
            print(f"[HTTP {exc.code}] {url}\n  {body[:300]}", file=sys.stderr)
            raise
        except urllib.error.URLError as exc:
            last_err = exc
            if attempt >= max_retries:
                break
            wait_s = min(5 * attempt, 30)
            print(
                f"[retry {attempt}/{max_retries}] network error: {exc}; sleep {wait_s}s",
                file=sys.stderr,
            )
            time.sleep(wait_s)
    assert last_err is not None
    raise last_err


def list_runs(token: str, branch: str, per_page: int = 5) -> list[dict]:
    q = urllib.parse.urlencode({"branch": branch, "per_page": per_page})
    url = f"{BASE}/repos/{REPO}/actions/runs?{q}"
    data = fetch_json(url, token)
    return data.get("workflow_runs") or []


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--branch", default="main", help="branch/ref to watch (default: main)")
    ap.add_argument(
        "--workflow_name",
        default="full test",
        help='filter by workflow name (default: "full test")',
    )
    ap.add_argument("--interval", type=int, default=30, help="poll interval seconds")
    ap.add_argument("--timeout", type=int, default=3600, help="max wait seconds")
    args = ap.parse_args()

    token = load_github_token() or ""
    if not token:
        print("missing token: set GITHUB_TOKEN or tools/ci/.github_token", file=sys.stderr)
        return 1

    deadline = time.time() + args.timeout
    last_id: int | None = None

    while True:
        runs = list_runs(token, args.branch, per_page=10)
        runs = [r for r in runs if (r.get("name") == args.workflow_name)]
        if runs:
            run = runs[0]
            run_id = int(run["id"])
            status = run.get("status")
            conclusion = run.get("conclusion")
            html_url = run.get("html_url")
            sha = (run.get("head_sha") or "")[:7]
            if last_id != run_id:
                print(f"watching run {run_id} sha={sha} {html_url}")
                last_id = run_id
            else:
                print(f"run {run_id} status={status} conclusion={conclusion}")

            if status == "completed":
                print(f"[DONE] run={run_id} conclusion={conclusion}")
                # print run_id alone on last line for easy scripting
                print(run_id)
                return 0 if conclusion == "success" else 2

        if time.time() >= deadline:
            print("timeout waiting for workflow run", file=sys.stderr)
            return 3
        time.sleep(max(5, args.interval))


if __name__ == "__main__":
    sys.exit(main())


#!/usr/bin/env python3
"""
Trigger a GitHub Actions workflow via workflow_dispatch (no gh CLI needed).

Requires token:
  - env: GITHUB_TOKEN
  - or tools/ci/.github_token (gitignored, see fetch_ci_errors.py)
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
import urllib.error
import urllib.request

# Ensure repo root is on sys.path no matter where invoked.
_ROOT = Path(__file__).resolve().parents[2]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

# Keep consistent with tools/ci/fetch_ci_errors.py
from tools.ci.fetch_ci_errors import BASE, REPO, load_github_token  # type: ignore


def _headers(token: str) -> dict[str, str]:
    return {
        "Authorization": f"token {token}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "BlessStar-ci-trigger/1.0",
    }


def post_json(url: str, token: str, payload: dict) -> None:
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers=_headers(token), method="POST")
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            # workflow_dispatch returns 204 No Content on success
            _ = resp.read()
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        print(f"[HTTP {exc.code}] {url}\n  {body[:300]}", file=sys.stderr)
        raise


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--workflow",
        default="ci.yml",
        help="workflow file name or id (default: ci.yml)",
    )
    ap.add_argument("--ref", default="main", help="git ref (branch or tag), default: main")
    ap.add_argument(
        "--inputs",
        default=None,
        help="optional JSON dict inputs for workflow_dispatch",
    )
    args = ap.parse_args()

    token = load_github_token() or ""
    if not token:
        print("missing token: set GITHUB_TOKEN or tools/ci/.github_token", file=sys.stderr)
        return 1

    url = f"{BASE}/repos/{REPO}/actions/workflows/{args.workflow}/dispatches"
    payload: dict = {"ref": args.ref}
    if args.inputs:
        payload["inputs"] = json.loads(args.inputs)

    post_json(url, token, payload)
    print(f"[OK] dispatched workflow={args.workflow} ref={args.ref} repo={REPO}")
    return 0


if __name__ == "__main__":
    sys.exit(main())


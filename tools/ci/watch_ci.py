#!/usr/bin/env python3
"""Poll GitHub Actions for BlessStar main branch CI status (no auth for public repos)."""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request

API = "https://api.github.com/repos/little-white1111/BlessStar/actions/runs"


def fetch_json(url: str) -> dict:
    req = urllib.request.Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "BlessStar-ci-watch",
        },
    )
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.loads(resp.read().decode("utf-8"))


def summarize_run(run: dict) -> str:
    run_id = run["id"]
    jobs = fetch_json(f"{API}/{run_id}/jobs?per_page=30")
    lines = [
        f"run {run_id} | {run.get('head_sha', '')[:7]} | "
        f"status={run.get('status')} conclusion={run.get('conclusion')}",
        f"  {run.get('html_url')}",
    ]
    for job in jobs.get("jobs", []):
        mark = {"success": "OK", "failure": "FAIL", "cancelled": "SKIP"}.get(
            job.get("conclusion") or "", "..."
        )
        lines.append(f"  [{mark}] {job.get('name')}")
        for step in job.get("steps", []):
            if step.get("conclusion") == "failure":
                lines.append(f"       -> failed step: {step.get('name')}")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Watch latest GitHub Actions CI run")
    parser.add_argument(
        "--interval",
        type=int,
        default=45,
        help="Seconds between polls while in progress (default: 45)",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=3600,
        help="Max seconds to wait (default: 3600)",
    )
    parser.add_argument("--once", action="store_true", help="Print latest run and exit")
    args = parser.parse_args()

    deadline = time.time() + args.timeout
    while True:
        try:
            data = fetch_json(f"{API}?per_page=1&branch=main")
        except urllib.error.URLError as exc:
            print(f"API error: {exc}", file=sys.stderr)
            return 2

        runs = data.get("workflow_runs") or []
        if not runs:
            print("No workflow runs found.", file=sys.stderr)
            return 1

        run = runs[0]
        print(summarize_run(run))

        if args.once:
            return 0 if run.get("conclusion") == "success" else 1

        if run.get("status") == "completed":
            return 0 if run.get("conclusion") == "success" else 1

        if time.time() >= deadline:
            print("Timeout waiting for CI.", file=sys.stderr)
            return 3

        print(f"waiting {args.interval}s...")
        time.sleep(args.interval)


if __name__ == "__main__":
    sys.exit(main())

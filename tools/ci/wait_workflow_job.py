#!/usr/bin/env python3
"""
Wait until a specific GitHub Actions *job* completes (do not wait for the whole workflow).

Typical usage (Mode A):
  python tools/ci/trigger_workflow_dispatch.py --workflow ci.yml --ref main \\
    --inputs '{"branch":"day19-stress-smoke"}'
  python tools/ci/wait_workflow_job.py --branch main --workflow_name "full test" \\
    --job-name "cmake (ubuntu-latest)" --timeout 3600
  python tools/ci/fetch_ci_errors.py --branch main --run <run_id> \\
    --job-name "cmake (ubuntu-latest)" --save-logs

List job names on a run:
  python tools/ci/wait_workflow_job.py --run-id 27142651314 --list-jobs
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[2]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from tools.ci.fetch_ci_errors import BASE, REPO, load_github_token  # noqa: E402


def _headers(token: str) -> dict[str, str]:
    return {
        "Authorization": f"token {token}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "BlessStar-ci-wait-job/1.0",
    }


def fetch_json(url: str, token: str, max_retries: int = 5) -> dict | list:
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


def list_runs(token: str, branch: str, per_page: int = 10) -> list[dict]:
    q = urllib.parse.urlencode({"branch": branch, "per_page": per_page})
    url = f"{BASE}/repos/{REPO}/actions/runs?{q}"
    data = fetch_json(url, token)
    return data.get("workflow_runs") or []


def get_run_jobs(token: str, run_id: int) -> list[dict]:
    url = f"{BASE}/repos/{REPO}/actions/runs/{run_id}/jobs?per_page=100"
    data = fetch_json(url, token)
    return data.get("jobs") or []


def resolve_job(jobs: list[dict], job_name: str) -> dict | None:
    """Exact name first, then case-insensitive fnmatch (supports * wildcards)."""
    if not job_name:
        return None
    for job in jobs:
        if job.get("name") == job_name:
            return job
    needle = job_name.lower()
    for job in jobs:
        name = (job.get("name") or "").lower()
        if name == needle or fnmatch.fnmatch(name, needle):
            return job
    for job in jobs:
        name = (job.get("name") or "").lower()
        if needle in name:
            return job
    return None


def pick_run(
    token: str,
    branch: str,
    workflow_name: str,
    run_id: int | None,
) -> dict | None:
    if run_id is not None:
        url = f"{BASE}/repos/{REPO}/actions/runs/{run_id}"
        return fetch_json(url, token)

    runs = list_runs(token, branch, per_page=15)
    if workflow_name:
        runs = [r for r in runs if r.get("name") == workflow_name]
    return runs[0] if runs else None


def main() -> int:
    ap = argparse.ArgumentParser(description="Wait for a specific GitHub Actions job to finish")
    ap.add_argument("--branch", default="main", help="branch used to find latest run (default: main)")
    ap.add_argument(
        "--workflow_name",
        default="full test",
        help='filter latest run by workflow display name (default: "full test")',
    )
    ap.add_argument(
        "--run-id",
        type=int,
        default=None,
        help="pin to this workflow run id (skip branch/latest lookup)",
    )
    ap.add_argument(
        "--job-name",
        default="",
        help='job name to watch, e.g. "cmake (ubuntu-latest)"; supports * wildcards',
    )
    ap.add_argument(
        "--list-jobs",
        action="store_true",
        help="print all job names/status on --run-id (or latest run) and exit",
    )
    ap.add_argument("--interval", type=int, default=30, help="poll interval seconds")
    ap.add_argument("--timeout", type=int, default=3600, help="max wait seconds")
    args = ap.parse_args()

    token = load_github_token() or ""
    if not token:
        print("missing token: set GITHUB_TOKEN or tools/ci/.github_token", file=sys.stderr)
        return 1

    run = pick_run(token, args.branch, args.workflow_name, args.run_id)
    if not run:
        print("no matching workflow run found", file=sys.stderr)
        return 1

    run_id = int(run["id"])
    sha = (run.get("head_sha") or "")[:7]
    html_url = run.get("html_url", "")

    if args.list_jobs:
        jobs = get_run_jobs(token, run_id)
        print(f"run={run_id} sha={sha} {html_url}")
        for job in jobs:
            print(
                f"  {job.get('name')}  status={job.get('status')} "
                f"conclusion={job.get('conclusion')} id={job.get('id')}"
            )
        return 0

    if not args.job_name:
        print("error: --job-name is required (or use --list-jobs)", file=sys.stderr)
        return 1

    deadline = time.time() + args.timeout
    print(
        f"watching job {args.job_name!r} on run {run_id} sha={sha}\n  {html_url}",
        flush=True,
    )

    while True:
        jobs = get_run_jobs(token, run_id)
        job = resolve_job(jobs, args.job_name)
        if not job:
            print(
                f"job {args.job_name!r} not found yet on run {run_id} "
                f"(workflow may still be queuing jobs)",
                flush=True,
            )
        else:
            status = job.get("status")
            conclusion = job.get("conclusion")
            job_id = job.get("id")
            resolved_name = job.get("name")
            print(
                f"job={job_id} name={resolved_name!r} status={status} conclusion={conclusion}",
                flush=True,
            )
            if status == "completed":
                print(
                    f"[JOB_DONE] run={run_id} job={job_id} name={resolved_name} "
                    f"conclusion={conclusion}"
                )
                # last line: run_id (compatible with existing wait_workflow_run.py parsers)
                print(run_id)
                return 0 if conclusion == "success" else 2

        if time.time() >= deadline:
            print(f"timeout waiting for job {args.job_name!r}", file=sys.stderr)
            return 3
        time.sleep(max(5, args.interval))


if __name__ == "__main__":
    sys.exit(main())

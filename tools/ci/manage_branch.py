#!/usr/bin/env python3
"""
Create / delete / list git branches for BlessStar CI loop (local + origin).

Examples:
  python tools/ci/manage_branch.py create feat/day23-foo --from main
  python tools/ci/manage_branch.py delete feat/day23-foo --local --remote
  python tools/ci/manage_branch.py list --remote
  python tools/ci/manage_branch.py exists feat/day23-foo
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[2]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from tools.ci.fetch_ci_errors import BASE, REPO, load_github_token  # noqa: E402

PROTECTED = frozenset({"main", "master"})
_BRANCH_RE = re.compile(r"^[A-Za-z0-9._/-]+$")


def _run_git(*args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(
        ["git", *args],
        cwd=_ROOT,
        text=True,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    if check and proc.returncode != 0:
        err = (proc.stderr or proc.stdout or "").strip()
        raise RuntimeError(f"git {' '.join(args)} failed ({proc.returncode}): {err}")
    return proc


def _local_branch_exists(name: str) -> bool:
    proc = _run_git("show-ref", "--verify", "--quiet", f"refs/heads/{name}", check=False)
    return proc.returncode == 0


def _remote_branch_exists(name: str, remote: str = "origin") -> bool:
    proc = _run_git("ls-remote", "--heads", remote, name, check=False)
    if proc.returncode != 0:
        return False
    return bool((proc.stdout or "").strip())


def _current_branch() -> str:
    proc = _run_git("branch", "--show-current", check=False)
    return (proc.stdout or "").strip()


def _validate_name(name: str) -> None:
    if not name or name.startswith("-") or name.endswith("/"):
        raise ValueError(f"invalid branch name: {name!r}")
    if not _BRANCH_RE.match(name):
        raise ValueError(f"invalid branch name (allowed: letters, digits, . _ / -): {name!r}")
    if name in PROTECTED:
        raise ValueError(f"protected branch cannot be managed: {name}")


def _api_headers(token: str) -> dict[str, str]:
    return {
        "Authorization": f"token {token}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "BlessStar-ci-branch/1.0",
    }


def _api_delete_remote_branch(name: str, token: str) -> None:
    url = f"{BASE}/repos/{REPO}/git/refs/heads/{name}"
    req = urllib.request.Request(url, headers=_api_headers(token), method="DELETE")
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            _ = resp.read()
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"GitHub API delete failed HTTP {exc.code}: {body[:300]}") from exc


def cmd_create(args: argparse.Namespace) -> int:
    name = args.name
    base = args.from_branch
    remote = args.remote
    _validate_name(name)

    print(f"[branch] fetch {remote} {base}")
    _run_git("fetch", remote, base)

    if _local_branch_exists(name):
        raise RuntimeError(f"local branch already exists: {name}")
    if _remote_branch_exists(name, remote):
        raise RuntimeError(f"remote branch already exists on {remote}: {name}")

    print(f"[branch] create local {name} from {remote}/{base}")
    _run_git("checkout", "-b", name, f"{remote}/{base}")

    if args.push:
        print(f"[branch] push -u {remote} {name}")
        _run_git("push", "-u", remote, name)

    print(json.dumps({"action": "create", "branch": name, "from": base, "remote": remote}))
    return 0


def cmd_delete(args: argparse.Namespace) -> int:
    name = args.name
    remote = args.remote
    _validate_name(name)

    if not args.local and not args.remote:
        print("specify --local and/or --remote", file=sys.stderr)
        return 1

    if args.local and _local_branch_exists(name):
        cur = _current_branch()
        if cur == name:
            fallback = args.fallback
            if not _local_branch_exists(fallback):
                raise RuntimeError(
                    f"on branch {name}; fallback {fallback!r} missing — checkout another branch first"
                )
            print(f"[branch] checkout {fallback} (leaving {name})")
            _run_git("checkout", fallback)

        flag = "-D" if args.force else "-d"
        print(f"[branch] delete local {name} ({flag})")
        proc = _run_git("branch", flag, name, check=False)
        if proc.returncode != 0 and not args.force:
            err = (proc.stderr or "").strip()
            print(f"[branch] local delete failed, retry with --force: {err}", file=sys.stderr)
            return 2
        if proc.returncode != 0:
            _run_git("branch", "-D", name)
    elif args.local:
        print(f"[branch] local branch not found, skip: {name}")

    if args.remote:
        if _remote_branch_exists(name, remote):
            print(f"[branch] delete remote {remote}/{name}")
            proc = _run_git("push", remote, "--delete", name, check=False)
            if proc.returncode != 0:
                token = load_github_token() or ""
                if not token:
                    err = (proc.stderr or "").strip()
                    raise RuntimeError(f"git push --delete failed and no token for API: {err}")
                _api_delete_remote_branch(name, token)
        else:
            print(f"[branch] remote branch not found, skip: {remote}/{name}")

    print(json.dumps({"action": "delete", "branch": name, "local": args.local, "remote": args.remote}))
    return 0


def cmd_list(args: argparse.Namespace) -> int:
    if args.remote:
        proc = _run_git("branch", "-r", "--format=%(refname:short)")
        lines = sorted({ln.strip() for ln in (proc.stdout or "").splitlines() if ln.strip()})
    else:
        proc = _run_git("branch", "--format=%(refname:short)")
        lines = sorted({ln.strip().lstrip("* ").strip() for ln in (proc.stdout or "").splitlines() if ln.strip()})
    for ln in lines:
        print(ln)
    return 0


def cmd_exists(args: argparse.Namespace) -> int:
    local = _local_branch_exists(args.name)
    remote = _remote_branch_exists(args.name, args.remote)
    print(json.dumps({"branch": args.name, "local": local, "remote": remote}))
    return 0 if (local or remote) else 1


def main() -> int:
    ap = argparse.ArgumentParser(description="BlessStar CI branch helper")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p_create = sub.add_parser("create", help="create local branch from base and push to origin")
    p_create.add_argument("name", help="new branch name")
    p_create.add_argument("--from", dest="from_branch", default="main", help="base branch (default: main)")
    p_create.add_argument("--remote", default="origin", help="git remote (default: origin)")
    p_create.add_argument("--no-push", dest="push", action="store_false", help="skip git push")
    p_create.set_defaults(push=True)

    p_delete = sub.add_parser("delete", help="delete local and/or remote branch")
    p_delete.add_argument("name", help="branch name to delete")
    p_delete.add_argument("--local", action="store_true", help="delete local branch")
    p_delete.add_argument("--remote", action="store_true", help="delete remote branch")
    p_delete.add_argument("--remote-name", dest="remote", default="origin")
    p_delete.add_argument("--fallback", default="main", help="checkout target if deleting current branch")
    p_delete.add_argument("--force", action="store_true", help="force delete local branch")

    p_list = sub.add_parser("list", help="list branches")
    p_list.add_argument("--remote", action="store_true", help="list remote-tracking branches")

    p_exists = sub.add_parser("exists", help="check branch existence (exit 1 if missing)")
    p_exists.add_argument("name")
    p_exists.add_argument("--remote-name", dest="remote", default="origin")

    args = ap.parse_args()
    try:
        if args.cmd == "create":
            return cmd_create(args)
        if args.cmd == "delete":
            return cmd_delete(args)
        if args.cmd == "list":
            return cmd_list(args)
        if args.cmd == "exists":
            return cmd_exists(args)
    except (RuntimeError, ValueError) as exc:
        print(f"[branch] ERROR: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

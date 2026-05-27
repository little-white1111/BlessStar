#!/usr/bin/env python3
"""
BlessStar CI 失败日志自动拉取与错误聚合工具。

用法：
    # 设置 token（PowerShell）
    $env:GITHUB_TOKEN = "ghp_xxxx..."

    # 抓 main 分支最新 run 的失败日志
    python tools/ci/fetch_ci_errors.py

    # 抓指定 run
    python tools/ci/fetch_ci_errors.py --run 26497427250

    # 同时保存完整日志到 .log 文件
    python tools/ci/fetch_ci_errors.py --save-logs

    # 自定义匹配关键字（用逗号分隔）
    python tools/ci/fetch_ci_errors.py --keywords "error:,FAILED:,undefined reference"

权限要求：
    公开仓库：PAT 至少需要 actions:read 权限。
    私有仓库：PAT 需要 repo + actions:read 权限。
"""

from __future__ import annotations

import argparse
import gzip
import io
import json
import os
import re
import sys
import urllib.error
import urllib.request
import urllib.parse
from pathlib import Path

REPO = "little-white1111/BlessStar"
BRANCH = "main"
BASE = "https://api.github.com"

DEFAULT_KEYWORDS = [
    r"error:",
    r"fatal error:",
    r"FAILED:",
    r"CMake Error",
    r"clang-format failed:",
    r"LeakSanitizer",
    r"AddressSanitizer",
    r"UndefinedBehaviorSanitizer",
    r"undefined reference",
    r"cannot find",
    r"no such file",
]

# ──────────────────────────────────────────────────────────────────────────────
# HTTP helpers
# ──────────────────────────────────────────────────────────────────────────────

def load_github_token() -> str | None:
    # 1) Prefer env var for CI / temporary use.
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        token = token.strip()
        return token if token else None

    # 2) Fallback to local-only token file (gitignored).
    token_file = Path(__file__).resolve().parent / ".github_token"
    try:
        if token_file.exists():
            token = token_file.read_text(encoding="utf-8").strip()
            return token if token else None
    except OSError:
        return None

    return None


def _headers(token: str) -> dict[str, str]:
    return {
        # GitHub REST API classic PAT: Authorization: token <PAT>
        "Authorization": f"token {token}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "BlessStar-ci-fetch/1.0",
    }


def fetch_json(url: str, token: str) -> dict | list:
    req = urllib.request.Request(url, headers=_headers(token))
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        print(f"[HTTP {exc.code}] {url}\n  {body[:300]}", file=sys.stderr)
        raise


def fetch_bytes(url: str, token: str) -> bytes:
    """
    日志下载：GitHub 先返回 302 Location（1 分钟有效的预签名 URL）。
    关键点：**不要**把 GitHub 的 Authorization header 带到重定向目标（Azure/S3），否则会触发 AuthenticationFailed。
    """

    class _NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, req, fp, code, msg, headers, newurl):  # type: ignore[override]
            return None

    opener = urllib.request.build_opener(_NoRedirect)

    # 1) 先请求 GitHub logs endpoint，获取 Location
    req = urllib.request.Request(url, headers=_headers(token))
    try:
        resp = opener.open(req, timeout=30)
        # 有时可能直接返回 200（少见），则直接读 body
        raw = resp.read()
    except urllib.error.HTTPError as exc:
        # 期望拿到 302
        if exc.code in (301, 302, 303, 307, 308):
            location = exc.headers.get("Location")
            if not location:
                return b""
            raw = b""
        else:
            body = exc.read()
            print(f"[HTTP {exc.code}] 日志下载失败: {url}", file=sys.stderr)
            if body:
                # 控制台编码兼容：只打印可见 ASCII
                snippet = body[:200]
                try:
                    print(f"  {snippet.decode('utf-8', errors='replace')}", file=sys.stderr)
                except Exception:
                    pass
            return b""
    else:
        # 直接 200 的情况，不走重定向
        try:
            return gzip.decompress(raw)
        except (gzip.BadGzipFile, OSError):
            return raw

    # 2) 下载 Location 指向的预签名 URL（不带 Authorization）
    #    注意：urllib 会自动跟随到最终文件，不会再携带 GitHub auth header。
    location = location.strip()  # type: ignore[has-type]
    dl_req = urllib.request.Request(
        location,
        headers={
            "User-Agent": "BlessStar-ci-fetch/1.0",
            "Accept": "*/*",
        },
    )
    try:
        with urllib.request.urlopen(dl_req, timeout=60) as dl_resp:
            raw = dl_resp.read()
            try:
                return gzip.decompress(raw)
            except (gzip.BadGzipFile, OSError):
                return raw
    except urllib.error.HTTPError as exc:
        print(f"[HTTP {exc.code}] 预签名日志下载失败", file=sys.stderr)
        return b""


# ──────────────────────────────────────────────────────────────────────────────
# 核心逻辑
# ──────────────────────────────────────────────────────────────────────────────

def get_latest_run(token: str, branch: str) -> dict:
    url = f"{BASE}/repos/{REPO}/actions/runs?branch={branch}&per_page=1"
    data = fetch_json(url, token)
    runs = data.get("workflow_runs") or []
    if not runs:
        print(f"错误：{branch} 分支没有 workflow run。", file=sys.stderr)
        sys.exit(1)
    return runs[0]


def get_run(token: str, run_id: int) -> dict:
    url = f"{BASE}/repos/{REPO}/actions/runs/{run_id}"
    return fetch_json(url, token)


def get_failed_jobs(token: str, run_id: int) -> list[dict]:
    url = f"{BASE}/repos/{REPO}/actions/runs/{run_id}/jobs?per_page=30"
    data = fetch_json(url, token)
    return [j for j in data.get("jobs", []) if j.get("conclusion") == "failure"]


def download_job_log(token: str, job_id: int) -> str:
    url = f"{BASE}/repos/{REPO}/actions/jobs/{job_id}/logs"
    raw = fetch_bytes(url, token)
    return raw.decode("utf-8", errors="replace")


def find_error_lines(
    log_text: str,
    keywords: list[str],
    context: int = 3,
) -> list[str]:
    """
    在 log_text 中查找包含任意 keyword 的行，
    并附带上下各 context 行，去重后返回。
    """
    lines = log_text.splitlines()
    patterns = [re.compile(k, re.IGNORECASE) for k in keywords]
    hit_indices: set[int] = set()

    for i, line in enumerate(lines):
        if any(p.search(line) for p in patterns):
            for j in range(max(0, i - context), min(len(lines), i + context + 1)):
                hit_indices.add(j)

    if not hit_indices:
        return []

    result: list[str] = []
    sorted_indices = sorted(hit_indices)
    prev = -2
    for idx in sorted_indices:
        if idx > prev + 1:
            result.append("  ...")
        result.append(f"  {idx + 1:5d} | {lines[idx]}")
        prev = idx
    return result


# ──────────────────────────────────────────────────────────────────────────────
# 入口
# ──────────────────────────────────────────────────────────────────────────────

def main() -> int:
    parser = argparse.ArgumentParser(
        description="拉取 GitHub Actions 失败日志并聚合错误行",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--run", type=int, default=None, help="指定 run id（默认取最新）")
    parser.add_argument("--branch", default=BRANCH, help=f"分支（默认 {BRANCH}）")
    parser.add_argument("--save-logs", action="store_true", help="把完整日志保存到本地 .log 文件")
    parser.add_argument(
        "--keywords",
        default=None,
        help="逗号分隔的匹配关键字（默认内置列表）",
    )
    parser.add_argument(
        "--context",
        type=int,
        default=3,
        help="每条匹配行上下各显示几行（默认 3）",
    )
    args = parser.parse_args()

    # ── Token ──
    token = load_github_token() or ""
    if not token:
        print(
            "错误：未找到 GitHub token。\n"
            "  方式1（推荐）：PowerShell：$env:GITHUB_TOKEN = 'ghp_xxx...'\n"
            "  方式2（本地文件）：把 token 写入 tools/ci/.github_token（已被 .gitignore 忽略）",
            file=sys.stderr,
        )
        return 1

    # ── 关键字 ──
    keywords = (
        [k.strip() for k in args.keywords.split(",")]
        if args.keywords
        else DEFAULT_KEYWORDS
    )

    # ── 获取 run ──
    if args.run:
        run = get_run(token, args.run)
    else:
        print(f"正在获取 {REPO} / {args.branch} 最新 run ...")
        run = get_latest_run(token, args.branch)

    run_id = run["id"]
    sha = run.get("head_sha", "")[:7]
    status = run.get("status")
    conclusion = run.get("conclusion")
    html_url = run.get("html_url", "")

    print(
        f"\n{'='*70}\n"
        f"Run #{run_id}  |  commit {sha}  |  {status} / {conclusion}\n"
        f"{html_url}\n"
        f"{'='*70}\n"
    )

    if conclusion == "success":
        print("CI 全部通过，无需修复。")
        return 0

    # ── 失败 job ──
    failed_jobs = get_failed_jobs(token, run_id)
    if not failed_jobs:
        print("未找到失败 job（可能仍在运行中），请稍后再试。")
        return 1

    print(f"发现 {len(failed_jobs)} 个失败 job：")
    for j in failed_jobs:
        failed_steps = [s["name"] for s in j.get("steps", []) if s.get("conclusion") == "failure"]
        failed_steps_str = ", ".join(failed_steps) if failed_steps else "(未知)"
        print(f"  [FAIL] {j['name']}  ->  失败步骤：{failed_steps_str}")
    print()

    # ── 下载并解析日志 ──
    any_errors = False
    log_dir = Path("ci_logs")

    for job in failed_jobs:
        job_id = job["id"]
        job_name = job["name"]
        print("-" * 70)
        print(f"JOB: {job_name} (id={job_id})")

        print("  下载日志 ...", end=" ", flush=True)
        log_text = download_job_log(token, job_id)
        if not log_text:
            print("(日志为空或下载失败)")
            continue
        print(f"共 {len(log_text.splitlines())} 行")

        # 保存完整日志
        if args.save_logs:
            log_dir.mkdir(exist_ok=True)
            safe_name = re.sub(r"[^\w\-]", "_", job_name)
            log_path = log_dir / f"{run_id}_{safe_name}.log"
            log_path.write_text(log_text, encoding="utf-8")
            print(f"  保存完整日志：{log_path}")

        # 解析错误行
        error_lines = find_error_lines(log_text, keywords, context=args.context)
        if error_lines:
            any_errors = True
            key_preview = ", ".join(keywords[:4])
            print(f"  匹配到的错误行（关键字：{key_preview} ...）：\n")
            for line in error_lines:
                print(line)
        else:
            print("  （未匹配到预设关键字，请用 --save-logs 查看完整日志）")
        print()

    return 1 if any_errors else 0


if __name__ == "__main__":
    sys.exit(main())

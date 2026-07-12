#!/usr/bin/env python3
"""
Compile-time config scanner — validate PR config changes against _COMPILE_TIME gate rules.

Inputs:
  --gate-rule-def   Path to gate_rule_def.json (CDD Skill output, required)
  --git-diff        Path to git diff output (optional; auto-runs git diff HEAD~1 if omitted)
  --report-dir      Output directory for reports (default: docs/reports)

Output:
  - docs/reports/compile-time-scanner-report.json
  - docs/reports/compile-time-scanner-report.md

Exit code:
  0  = all rules pass (or no applicable rules)
  1  = some rules violated
  2  = input error
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


# ── Constants ──────────────────────────────────────────────────────────
_COMPILE_TIME_SUFFIX = "_COMPILE_TIME"
_DYNAMIC_REF_RE = re.compile(r"\$\{([^}]+)\}")  # matches ${config.key}
_REPORT_DIR_DEFAULT = "docs/reports"

# Supported operators for offline compile-time validation
_SIMPLE_OPS = {"eq", "ne", "gt", "lt", "gte", "lte", "range", "in"}


# ── Git diff parsing ───────────────────────────────────────────────────
def _run_git_diff(repo: Path, base_ref: str = "HEAD~1") -> str:
    """Run git diff and return the output."""
    try:
        result = subprocess.run(
            ["git", "diff", base_ref],
            cwd=str(repo),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=30,
        )
        return result.stdout
    except subprocess.TimeoutExpired:
        print("[FAIL] git diff timed out", file=sys.stderr)
        return ""
    except FileNotFoundError:
        print("[FAIL] git not found", file=sys.stderr)
        return ""


def _parse_diff(diff_text: str) -> dict[str, dict[str, str]]:
    """Parse git diff into {file_path: {config_key: new_value}}.

    Only extracts additions (lines starting with +) from diff hunks.
    Skips ---/+++ headers, @@ hunk headers, and --- lines (deletions).
    Tries to parse JSON/YAML key-value patterns.
    """
    changes: dict[str, dict[str, str]] = {}
    current_file: str | None = None

    for line in diff_text.splitlines():
        # Track file paths from diff header
        if line.startswith("+++ b/"):
            current_file = line[6:]  # strip "+++ b/"
            if current_file not in changes:
                changes[current_file] = {}
            continue
        if line.startswith("--- "):
            continue
        if line.startswith("@@"):
            continue

        # Only look at additions
        if not line.startswith("+") or line.startswith("+++"):
            continue
        content = line[1:].strip()
        if not content:
            continue

        # Try to parse key: value patterns (JSON, YAML, TOML, INI style)
        # Match: key: value, key = value, "key": value
        m = re.match(
            r'^[\s"\'`]*([\w.]+)[\s"\']*[:=][\s"\']*([^#"\']+)["\'\s,]*$',
            content,
        )
        if m:
            key = m.group(1).strip()
            val = m.group(2).strip().rstrip(",")
            # Strip surrounding quotes from value
            val = val.strip("\"'`")
            if current_file and key not in changes[current_file]:
                changes[current_file][key] = val

    return changes


# ── Rule evaluation ────────────────────────────────────────────────────
def _eval_range(value: str, range_str: str) -> bool:
    """Check value is within [min,max]."""
    try:
        inner = range_str.strip("[]()").strip()
        parts = inner.split(",")
        if len(parts) != 2:
            return False
        v = float(value)
        lo, hi = float(parts[0].strip()), float(parts[1].strip())
        return lo <= v <= hi
    except (ValueError, TypeError):
        return False


def _eval_compare(value: str, op: str, threshold: str) -> bool:
    """Compare value against threshold using op (gt/lt/gte/lte/eq/ne)."""
    try:
        v = float(value)
        t = float(threshold)
        if op == "gt":
            return v > t
        if op == "lt":
            return v < t
        if op == "gte":
            return v >= t
        if op == "lte":
            return v <= t
        if op == "eq":
            return v == t
        if op == "ne":
            return v != t
        return False
    except (ValueError, TypeError):
        # Fall back to string comparison
        if op == "eq":
            return value == threshold
        if op == "ne":
            return value != threshold
        return False


def _eval_in(value: str, enum_str: str) -> bool:
    """Check value is in comma-separated enum."""
    candidates = [s.strip().strip("\"'`") for s in enum_str.split(",") if s.strip()]
    return value.strip() in candidates


def _eval_rule(rule: dict[str, Any], actual_value: str) -> tuple[bool, str]:
    """Evaluate a single gate_rule_def against the actual config value.

    Returns (passed, error_message).
    """
    op = rule.get("op", "")
    expected = rule.get("value", "")
    error_hint = rule.get("error_hint", f"Rule violated: {op} {expected}")

    if op == "range":
        ok = _eval_range(actual_value, expected)
    elif op in ("gt", "lt", "gte", "lte", "eq", "ne"):
        ok = _eval_compare(actual_value, op, expected)
    elif op == "in":
        ok = _eval_in(actual_value, expected)
    else:
        # Unknown operator — skip (runtime gate will handle)
        return True, ""

    if not ok:
        return False, error_hint
    return True, ""


def _is_dynamic_ref(value: str) -> bool:
    """Check if value has ${...} dynamic references."""
    return bool(_DYNAMIC_REF_RE.search(value))


# ── Main scanning logic ────────────────────────────────────────────────
def scan(
    gate_rule_def_path: Path,
    git_diff_text: str,
    repo: Path,
) -> dict[str, Any]:
    """Run compile-time scan and return report dict."""
    # Load gate_rule_def
    if not gate_rule_def_path.is_file():
        return {
            "result": "FAIL",
            "error": f"gate_rule_def file not found: {gate_rule_def_path}",
            "violations": [],
            "warnings": [],
            "passed": 0,
            "failed": 0,
            "skipped": 0,
        }

    all_rules: list[dict[str, Any]] = json.loads(
        gate_rule_def_path.read_text(encoding="utf-8")
    )

    # Filter: only _COMPILE_TIME rules (架构不变量 #1: 输入隔离)
    compile_time_rules = [
        r
        for r in all_rules
        if isinstance(r, dict)
        and r.get("scenario", "").endswith(_COMPILE_TIME_SUFFIX)
    ]
    skipped = len(all_rules) - len(compile_time_rules)

    if not compile_time_rules:
        return {
            "result": "PASS",
            "message": "No _COMPILE_TIME rules found",
            "violations": [],
            "warnings": [],
            "total_rules": len(all_rules),
            "compile_time_rules": 0,
            "passed": 0,
            "failed": 0,
            "skipped": skipped,
        }

    # Parse git diff
    changed_configs = _parse_diff(git_diff_text)

    # Evaluate each rule
    violations: list[dict[str, Any]] = []
    warnings: list[dict[str, Any]] = []
    passed_count = 0
    failed_count = 0

    for rule in compile_time_rules:
        field_key = rule.get("field_key", "")
        value = rule.get("value", "")
        op = rule.get("op", "")

        # Check if this config key was changed in the PR (架构不变量 #2: 增量优先)
        changed_value: str | None = None
        for _file, configs in changed_configs.items():
            if field_key in configs:
                changed_value = configs[field_key]
                break

        if changed_value is None:
            # Config not in diff — skip (this rule is not triggered by this PR)
            skipped += 1
            continue

        # Check dynamic reference (架构不变量 #3: 动态引用降级)
        if _is_dynamic_ref(value):
            warnings.append({
                "config_key": field_key,
                "rule_op": op,
                "rule_value": value,
                "actual_value": changed_value,
                "message": (
                    f"Dynamic reference {value} skipped at compile time; "
                    f"runtime gate will validate"
                ),
            })
            continue

        # Evaluate
        ok, err_msg = _eval_rule(rule, changed_value)
        if ok:
            passed_count += 1
        else:
            failed_count += 1
            violations.append({
                "config_key": field_key,
                "file_path": "",  # will be filled from diff context if available
                "rule_op": op,
                "expected": value,
                "actual": changed_value,
                "error_hint": err_msg,
            })

    result = "FAIL" if failed_count > 0 else "PASS"
    return {
        "result": result,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "total_rules": len(all_rules),
        "compile_time_rules": len(compile_time_rules),
        "passed": passed_count,
        "failed": failed_count,
        "skipped": skipped,
        "violations": violations,
        "warnings": warnings,
    }


# ── Report writers ─────────────────────────────────────────────────────
def _write_json_report(report: dict[str, Any], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def _write_markdown_report(report: dict[str, Any], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Compile-Time Scanner Report",
        "",
        f"- Result: **{report['result']}**",
        f"- Generated at: {report.get('generated_at', 'N/A')}",
        f"- Total rules: {report.get('total_rules', 0)}",
        f"- Compile-time rules: {report.get('compile_time_rules', 0)}",
        f"- Passed: {report.get('passed', 0)}",
        f"- Failed: {report.get('failed', 0)}",
        f"- Skipped: {report.get('skipped', 0)}",
        "",
    ]

    violations = report.get("violations", [])
    if violations:
        lines.append("## Violations")
        lines.append("")
        lines.append("| Config Key | Op | Expected | Actual | Error Hint |")
        lines.append("|------------|----|----------|--------|------------|")
        for v in violations:
            lines.append(
                f"| `{v.get('config_key', '')}` "
                f"| `{v.get('rule_op', '')}` "
                f"| `{v.get('expected', '')}` "
                f"| `{v.get('actual', '')}` "
                f"| {v.get('error_hint', '')} |"
            )
        lines.append("")

    warnings_list = report.get("warnings", [])
    if warnings_list:
        lines.append("## Warnings")
        lines.append("")
        for w in warnings_list:
            lines.append(f"- `{w.get('config_key', '')}`: {w.get('message', '')}")
        lines.append("")

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


# ── CLI entry point ────────────────────────────────────────────────────
def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compile-time config scanner for BlessStar _COMPILE_TIME rules"
    )
    parser.add_argument(
        "--gate-rule-def",
        required=True,
        help="Path to gate_rule_def.json (CDD Skill output)",
    )
    parser.add_argument(
        "--git-diff",
        default=None,
        help="Path to git diff output (optional; auto-runs git diff HEAD~1 if omitted)",
    )
    parser.add_argument(
        "--report-dir",
        default=_REPORT_DIR_DEFAULT,
        help=f"Output directory for reports (default: {_REPORT_DIR_DEFAULT})",
    )
    args = parser.parse_args()

    _lib = Path(__file__).resolve().parents[1] / "lib"
    if str(_lib) not in sys.path:
        sys.path.insert(0, str(_lib))
    from repo_paths import repo_root

    repo = repo_root()
    gate_rule_def_path = repo / args.gate_rule_def

    # Get git diff
    if args.git_diff:
        diff_path = repo / args.git_diff
        if not diff_path.is_file():
            print(f"[FAIL] git diff file not found: {diff_path}", file=sys.stderr)
            return 2
        git_diff_text = diff_path.read_text(encoding="utf-8", errors="replace")
    else:
        git_diff_text = _run_git_diff(repo)
        if not git_diff_text:
            print("[WARN] No git diff available; scanning all rules against empty diff", file=sys.stderr)

    report = scan(gate_rule_def_path, git_diff_text, repo)

    if "error" in report:
        print(f"[FAIL] {report['error']}", file=sys.stderr)
        return 2

    json_path = repo / args.report_dir / "compile-time-scanner-report.json"
    md_path = repo / args.report_dir / "compile-time-scanner-report.md"
    _write_json_report(report, json_path)
    _write_markdown_report(report, md_path)

    print(f"[OK] JSON report: {json_path.relative_to(repo)}")
    print(f"[OK] Markdown report: {md_path.relative_to(repo)}")

    if report.get("violations"):
        for v in report["violations"]:
            print(f"[FAIL] {v['config_key']}: {v['error_hint']} (expected={v['expected']}, actual={v['actual']})")
        return 1

    if report["result"] == "PASS":
        print(f"[OK] All {report['passed']} compile-time rule(s) passed")
        return 0

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

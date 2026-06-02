#!/usr/bin/env python3
"""C-ST-5: clang-format check/apply for kernel/adapter/app (--batch include|src)."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parents[1] / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import collect_sources, repo_root


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--batch", choices=("include", "src"), required=True)
    parser.add_argument("--apply", action="store_true", help="write formatted files")
    args = parser.parse_args()

    clang_format = shutil.which("clang-format")
    if not clang_format:
        print("[SKIP] C-ST-5: clang-format not found in PATH")
        return 0

    root = repo_root()
    files = collect_sources(root, args.batch)
    if not files:
        print(f"[FAIL] C-ST-5: no files for batch {args.batch}")
        return 2

    bad: list[str] = []
    for path in files:
        cmd = [clang_format, f"-style=file:{root / '.clang-format'}", str(path)]
        if args.apply:
            subprocess.run(cmd, check=False)
            continue
        # dry-run with -Werror if supported
        dry = subprocess.run(
            [clang_format, "--dry-run", "-Werror", f"-style=file:{root / '.clang-format'}", str(path)],
            capture_output=True,
            text=True,
        )
        if dry.returncode != 0:
            # fallback: compare output
            formatted = subprocess.run(
                cmd, capture_output=True, text=True, check=False
            )
            original = path.read_text(encoding="utf-8", errors="replace")
            if formatted.stdout != original:
                bad.append(f"C-ST-5: {path.relative_to(root)}: needs clang-format")

    if args.apply:
        print(f"[OK] C-ST-5: clang-format applied to {len(files)} files (batch={args.batch})")
        return 0

    if bad:
        for line in bad[:40]:
            print(f"[FAIL] {line}")
        if len(bad) > 40:
            print(f"[FAIL] ... and {len(bad) - 40} more (run with --apply)")
        return 2
    print(f"[OK] C-ST-5 clang-format check passed (batch={args.batch}, files={len(files)})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

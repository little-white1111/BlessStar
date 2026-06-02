#!/usr/bin/env python3
"""Include gate for R8-03 + R8-13 (IMPL-08-20). Does not modify factory manifest."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

RE_ADAPTER_IN_KERNEL_TEST = re.compile(
    r'#\s*include\s+[<"](?:bs/)?adapter/', re.IGNORECASE
)
RE_CONFIG_MANAGER = re.compile(
    r'#\s*include\s+[<"][^>"]*ConfigManager\.h[>"]', re.IGNORECASE
)
RE_PATH_REGISTRY_DIRECT = re.compile(r"\bbs_path_registry_\w+\s*\(")

SKIP_DIRS = {"Source", "build", "build-meson", "factory", ".git", "_deps"}


def iter_source_files(repo_root: Path) -> list[Path]:
    out: list[Path] = []
    for ext in ("*.cpp", "*.h", "*.hpp", "*.c"):
        for path in repo_root.rglob(ext):
            parts = set(path.parts)
            if parts & SKIP_DIRS:
                continue
            rel = path.relative_to(repo_root)
            if rel.parts[0] not in ("kernel", "adapter"):
                continue
            out.append(path)
    return out


def is_kernel_test(path: Path) -> bool:
    return "kernel" in path.parts and "test" in path.parts


def is_adapter_production(path: Path) -> bool:
    if "adapter" not in path.parts:
        return False
    return "test" not in path.parts


def config_manager_allowed(path: Path, repo_root: Path) -> bool:
    rel = path.relative_to(repo_root).as_posix()
    if "ConfigManager.h" in path.name:
        return True
    if rel.startswith("kernel/state/src/") and "ConfigManager" in path.name:
        return True
    if rel.startswith("kernel/state/include/") and "ConfigManager" in path.name:
        return True
    if rel.startswith("kernel/state/test/"):
        return True
    if rel == "adapter/src/attach_config.cpp":
        return True
    return False


def check_file(path: Path, repo_root: Path) -> list[str]:
    errors: list[str] = []
    text = path.read_text(encoding="utf-8", errors="replace")
    rel = path.relative_to(repo_root).as_posix()

    if is_kernel_test(path):
        for i, line in enumerate(text.splitlines(), 1):
            if RE_ADAPTER_IN_KERNEL_TEST.search(line):
                errors.append(f"{rel}:{i}: INC-VIII-1 kernel test must not include adapter")

    if not config_manager_allowed(path, repo_root):
        for i, line in enumerate(text.splitlines(), 1):
            if RE_CONFIG_MANAGER.search(line):
                errors.append(
                    f"{rel}:{i}: INC-VIII-2 direct ConfigManager.h include not allowed"
                )

    if is_adapter_production(path):
        for i, line in enumerate(text.splitlines(), 1):
            if RE_PATH_REGISTRY_DIRECT.search(line):
                errors.append(
                    f"{rel}:{i}: INC-V-1 adapter production must use RegistryFacade, not PathRegistry"
                )

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="BlessStar include purity gate")
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="Repository root",
    )
    args = parser.parse_args()
    repo_root = args.repo_root.resolve()

    all_errors: list[str] = []
    for path in iter_source_files(repo_root):
        all_errors.extend(check_file(path, repo_root))

    if all_errors:
        print("Include gate FAILED:", file=sys.stderr)
        for e in all_errors:
            print(f"  {e}", file=sys.stderr)
        return 1

    print("Include gate OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())

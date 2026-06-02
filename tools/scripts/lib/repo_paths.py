#!/usr/bin/env python3
"""Shared path helpers for BlessStar tools/scripts gate checks."""

from __future__ import annotations

from pathlib import Path


def repo_root() -> Path:
    # tools/scripts/lib/repo_paths.py -> repo root
    return Path(__file__).resolve().parents[3]


def public_include_dirs(root: Path) -> list[Path]:
    """All module public include roots (kernel/adapter/app), excluding legacy/ trees."""
    out: list[Path] = []
    for base in ("kernel", "adapter", "app"):
        p = root / base
        if not p.exists():
            continue
        for inc in p.rglob("include"):
            if not inc.is_dir():
                continue
            if "legacy" in inc.parts:
                continue
            out.append(inc)
    return sorted(set(out))


def collect_sources(root: Path, batch: str) -> list[Path]:
    bases = [root / "kernel", root / "adapter", root / "app"]
    files: list[Path] = []
    if batch == "include":
        for inc in public_include_dirs(root):
            files.extend(inc.rglob("*.h"))
            files.extend(inc.rglob("*.hpp"))
    elif batch == "src":
        for base in bases:
            if not base.exists():
                continue
            for ext in ("*.c", "*.cpp", "*.cc"):
                for f in base.rglob(ext):
                    p = f.as_posix()
                    if "/include/" in p or "\\include\\" in str(f):
                        continue
                    files.append(f)
    else:
        raise ValueError(f"unknown batch: {batch}")
    return sorted(set(files))

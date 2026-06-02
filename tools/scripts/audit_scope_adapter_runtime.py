#!/usr/bin/env python3
"""Audit adapter + kernel/runtime public headers for C-ST-1 / C-ST-7 gaps."""

from __future__ import annotations

import re
import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parent / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import public_include_dirs, repo_root  # noqa: E402

MARKER = "C-ST-7 contract block"
LEGACY = "LEGACY-NOT-BUILT"
REQUIRED = ("Thread safety:", "Error semantics:", "Platform notes:")
FUNC_RE = re.compile(
    r"^\s*(?:extern\s+)?(?:[\w\s\*]+?)\s+([a-z][a-z0-9_]*)\s*\([^;]*\)\s*;",
    re.MULTILINE,
)
SKIP = {"ifdef", "ifndef", "define", "if", "else", "endif", "pragma"}
RESERVED = {
    "void", "int", "char", "short", "long", "float", "double", "signed", "unsigned",
    "const", "static", "inline", "struct", "enum", "typedef",
}


def in_scope(path: Path, root: Path) -> bool:
    rel = path.relative_to(root).as_posix()
    if rel.startswith("adapter/"):
        return True
    if rel.startswith("kernel/runtime/"):
        return True
    return False


def main() -> int:
    root = repo_root()
    missing_c7: list[str] = []
    incomplete_c7: list[str] = []
    legacy: list[str] = []
    bad_prefix: list[str] = []
    no_extern_c: list[str] = []

    for inc in public_include_dirs(root):
        for path in sorted(inc.rglob("*.h")):
            if not in_scope(path, root):
                continue
            rel = path.relative_to(root).as_posix()
            text = path.read_text(encoding="utf-8", errors="replace")
            if LEGACY in text:
                legacy.append(rel)
                continue
            if 'extern "C"' not in text:
                no_extern_c.append(rel)
                continue
            if MARKER not in text:
                missing_c7.append(rel)
            else:
                for field in REQUIRED:
                    if field not in text:
                        incomplete_c7.append(f"{rel}: missing {field}")
            for m in FUNC_RE.finditer(text):
                line_start = text.rfind("\n", 0, m.start()) + 1
                line = text[line_start : m.end()]
                if re.match(r"^\s*\*", line):
                    continue
                name = m.group(1)
                if name in SKIP or name in RESERVED:
                    continue
                if name.startswith("bs_") or name.startswith("Bs"):
                    continue
                if name.isupper():
                    continue
                bad_prefix.append(f"{rel}: {name}")

    print("=== Missing C-ST-7 ===")
    for x in missing_c7:
        print(x)
    print("=== Incomplete C-ST-7 ===")
    for x in incomplete_c7:
        print(x)
    print("=== C-ST-1 violations ===")
    for x in bad_prefix:
        print(x)
    print("=== LEGACY-NOT-BUILT ===")
    for x in legacy:
        print(x)
    print("=== No extern C ===")
    for x in no_extern_c:
        print(x)
    print(
        "SUMMARY",
        f"missing_c7={len(missing_c7)}",
        f"incomplete_c7={len(incomplete_c7)}",
        f"bad_prefix={len(bad_prefix)}",
        f"legacy={len(legacy)}",
        f"no_extern_c={len(no_extern_c)}",
    )
    return 1 if (missing_c7 or incomplete_c7 or bad_prefix) else 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""C-ST-1: public C API names in include/*.h should use bs_ prefix."""

from __future__ import annotations

import re
import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parents[1] / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import public_include_dirs, repo_root

LEGACY_SKIP_MARKER = "LEGACY-NOT-BUILT"

# Match plausible global function declarations (not struct/enum/typedef lines).
FUNC_RE = re.compile(
    r"^\s*(?:extern\s+)?(?:[\w\s\*]+?)\s+([a-z][a-z0-9_]*)\s*\([^;]*\)\s*;",
    re.MULTILINE,
)

SKIP_NAMES = {
    "ifdef",
    "ifndef",
    "define",
    "if",
    "else",
    "endif",
    "pragma",
}

RESERVED_TYPES = {
    "void",
    "int",
    "char",
    "short",
    "long",
    "float",
    "double",
    "signed",
    "unsigned",
    "const",
    "static",
    "inline",
    "struct",
    "enum",
    "typedef",
}


def main() -> int:
    root = repo_root()
    bad: list[str] = []
    for inc_dir in public_include_dirs(root):
        try:
            top = inc_dir.relative_to(root).parts[0]
        except ValueError:
            continue
        if top == "kernel" and "test_support" in inc_dir.parts:
            continue
        for path in sorted(inc_dir.rglob("*.h")):
            text = path.read_text(encoding="utf-8", errors="replace")
            if LEGACY_SKIP_MARKER in text:
                continue
            if 'extern "C"' not in text:
                continue
            for m in FUNC_RE.finditer(text):
                line_start = text.rfind("\n", 0, m.start()) + 1
                line = text[line_start : m.end()]
                if re.match(r"^\s*\*", line):
                    continue
                name = m.group(1)
                if name in SKIP_NAMES or name in RESERVED_TYPES:
                    continue
                if name.startswith("bs_") or name.startswith("Bs"):
                    continue
                if name.isupper():  # macros / enum constants
                    continue
                # C++ methods in headers wrapped in extern "C" are rare; skip operator
                if name in ("operator",):
                    continue
                rel = path.relative_to(root)
                bad.append(f"C-ST-1: {rel}: function '{name}' lacks bs_ prefix")
    if bad:
        for line in bad[:50]:
            print(f"[FAIL] {line}")
        if len(bad) > 50:
            print(f"[FAIL] ... and {len(bad) - 50} more")
        return 2
    print("[OK] C-ST-1 public API prefix check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

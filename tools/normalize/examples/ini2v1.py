#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
Convert INI configuration to BlessStar Config JSON v1.

Handles basic INI sections: [section]
key = value
# comments

Usage:
  python ini2v1.py <input.ini> <output.json>

Exit codes:
  0  success
  1  parse error (stderr message)
  2  I/O or usage error
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

_SECTION_RE = re.compile(r"^\s*\[(.+?)\]\s*$")
_KEYVAL_RE  = re.compile(r"^\s*([^=#]+?)\s*=\s*(.*?)\s*$")
_COMMENT_RE = re.compile(r"^\s*(#|;)")


def parse_ini(text: str) -> dict:
    """Parse INI text into {section: {key: value}}."""
    result: dict = {}
    current_section = "global"
    result[current_section] = {}

    for lineno, line in enumerate(text.splitlines(), 1):
        if _COMMENT_RE.match(line) or not line.strip():
            continue
        m = _SECTION_RE.match(line)
        if m:
            current_section = m.group(1).strip()
            if current_section not in result:
                result[current_section] = {}
            continue
        m = _KEYVAL_RE.match(line)
        if m:
            key = m.group(1).strip()
            val = m.group(2).strip()
            # strip surrounding quotes
            if len(val) >= 2 and val[0] == val[-1] and val[0] in ('"', "'"):
                val = val[1:-1]
            result[current_section][key] = val
            continue
        # If we get here, line is unrecognized
        print(f"parse error at line {lineno}: {line!r}", file=sys.stderr)
        return {}

    return result


def ini_to_v1_json(parsed: dict) -> dict:
    """Convert parsed INI dict to BlessStar Config JSON v1 structure."""
    instructions = []
    for section, kv in parsed.items():
        name = section
        meta = {}
        for k, v in kv.items():
            meta[k] = v
        instructions.append({
            "type": "type",
            "name": name,
            "metadata": meta,
        })

    return {
        "kernel_version": "1.0.0",
        "adapter_version": "1.0.0",
        "manual_requirements": [],
        "instructions": instructions,
    }


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: ini2v1.py <input.ini> <output.json>", file=sys.stderr)
        return 2
    in_path = Path(sys.argv[1])
    out_path = Path(sys.argv[2])
    if not in_path.is_file():
        print(f"input not found: {in_path}", file=sys.stderr)
        return 2
    try:
        raw = in_path.read_text(encoding="utf-8")
    except OSError as e:
        print(f"read error: {e}", file=sys.stderr)
        return 2

    parsed = parse_ini(raw)
    if not parsed:
        return 1

    doc = ini_to_v1_json(parsed)
    text = json.dumps(doc, ensure_ascii=False, indent=2) + "\n"
    try:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(text, encoding="utf-8", newline="\n")
    except OSError as e:
        print(f"write error: {e}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())

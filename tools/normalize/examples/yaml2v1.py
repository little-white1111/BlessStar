#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
Convert YAML configuration to BlessStar Config JSON v1.

Requires `pyyaml` or `ruamel.yaml`. Falls back to error if neither installed.

Usage:
  python yaml2v1.py <input.yaml> <output.json>

Exit codes:
  0  success
  1  parse error (stderr message)
  2  I/O or usage error
"""

from __future__ import annotations

import json
import sys
from pathlib import Path


def _load_yaml() -> object:
    """Try yaml parsers in order of preference."""
    try:
        import yaml as _yaml
        return _yaml
    except ImportError:
        pass
    try:
        import ruamel.yaml as _ryaml
        return _ryaml
    except ImportError:
        pass
    return None


def _yaml_value_to_meta(val: object) -> str:
    """Convert a YAML scalar to string metadata value."""
    if val is None:
        return ""
    if isinstance(val, bool):
        return "true" if val else "false"
    if isinstance(val, (int, float)):
        return str(val)
    if isinstance(val, str):
        return val
    return json.dumps(val, ensure_ascii=False)


def yaml_doc_to_v1_json(yaml_doc: dict, yaml_mod: object) -> dict:
    """Convert parsed YAML dict to BlessStar Config JSON v1."""
    instructions = []
    for key, val in yaml_doc.items():
        if not isinstance(val, dict):
            val = {"value": val}
        meta = {}
        for mk, mv in val.items():
            meta[mk] = _yaml_value_to_meta(mv)
        instructions.append({
            "type": "type",
            "name": str(key),
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
        print("usage: yaml2v1.py <input.yaml> <output.json>", file=sys.stderr)
        return 2

    yaml_mod = _load_yaml()
    if yaml_mod is None:
        print("error: pyyaml or ruamel.yaml required", file=sys.stderr)
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

    try:
        doc = yaml_mod.safe_load(raw) if hasattr(yaml_mod, "safe_load") else yaml_mod.load(raw)
    except Exception as e:
        print(f"yaml parse error: {e}", file=sys.stderr)
        return 1

    if not isinstance(doc, dict):
        print("yaml root must be a mapping", file=sys.stderr)
        return 1

    v1 = yaml_doc_to_v1_json(doc, yaml_mod)
    text = json.dumps(v1, ensure_ascii=False, indent=2) + "\n"
    try:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(text, encoding="utf-8", newline="\n")
    except OSError as e:
        print(f"write error: {e}", file=sys.stderr)
        return 2

    return 0


if __name__ == "__main__":
    sys.exit(main())

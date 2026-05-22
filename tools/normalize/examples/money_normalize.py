#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
Day10 official money metadata normalizer (2-A'): validate financial string fields in v1 JSON.

Runs structural + builtin type checks (via identity_normalize module), then enforces:
  - amount (if present): ^\\d+\\.\\d{2}$
  - tax_rate (if present): ^\\d+$ (no percent sign)
  - subject_code (if present): non-empty string

Usage:
  python money_normalize.py <input.json> <output.json>

Exit codes: 0 success, 1 validation error, 2 I/O or usage error.
"""

from __future__ import annotations

import importlib.util
import json
import re
import sys
from pathlib import Path

_AMOUNT_RE = re.compile(r"^\d+\.\d{2}$")
_TAX_RATE_RE = re.compile(r"^\d+$")


def _load_identity_module():
    path = Path(__file__).resolve().parent / "identity_normalize.py"
    spec = importlib.util.spec_from_file_location("bs_identity_normalize", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("failed to load identity_normalize.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _validate_money_metadata(doc: dict) -> None:
    instrs = doc.get("instructions", [])
    if not isinstance(instrs, list):
        return
    for i, item in enumerate(instrs):
        if not isinstance(item, dict):
            continue
        md = item.get("metadata")
        if not isinstance(md, dict):
            continue
        for key, val in md.items():
            if not isinstance(val, str):
                raise ValueError(f"instructions[{i}].metadata[{key!r}] must be a string")
            if key == "amount" and val and not _AMOUNT_RE.match(val):
                raise ValueError(
                    f"instructions[{i}].metadata.amount must match two decimal places "
                    f"(e.g. 100.50), got {val!r}"
                )
            if key == "tax_rate" and val and not _TAX_RATE_RE.match(val):
                raise ValueError(
                    f"instructions[{i}].metadata.tax_rate must be digits only (no %), got {val!r}"
                )
            if key == "subject_code" and val is not None and not val.strip():
                raise ValueError(f"instructions[{i}].metadata.subject_code must be non-empty")


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: money_normalize.py <input.json> <output.json>", file=sys.stderr)
        return 2
    in_path = Path(sys.argv[1])
    out_path = Path(sys.argv[2])
    if not in_path.is_file():
        print(f"input not found: {in_path}", file=sys.stderr)
        return 2
    try:
        identity = _load_identity_module()
        raw = in_path.read_text(encoding="utf-8")
        if raw.startswith("\ufeff"):
            raw = raw.lstrip("\ufeff")
        doc = json.loads(raw)
    except json.JSONDecodeError as e:
        print(f"invalid JSON: {e}", file=sys.stderr)
        return 1
    except OSError as e:
        print(f"read error: {e}", file=sys.stderr)
        return 2
    try:
        identity.validate_document(doc)
        _validate_money_metadata(doc)
    except ValueError as e:
        print(f"validation failed: {e}", file=sys.stderr)
        return 1
    if not isinstance(doc, dict):
        print("internal error: expected dict", file=sys.stderr)
        return 1
    text = identity.canonicalize(doc)
    try:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(text, encoding="utf-8", newline="\n")
    except OSError as e:
        print(f"write error: {e}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())

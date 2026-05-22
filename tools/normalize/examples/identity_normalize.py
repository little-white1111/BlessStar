#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
BlessStar M1 official normalizer: validate BlessStar Config JSON v1 and write a canonical UTF-8 JSON file.

Usage:
  python identity_normalize.py <input.json> <output.json>

Exit codes:
  0  success
  1  validation error (stderr message)
  2  I/O or usage error

Optional: if package `jsonschema` is installed, validates against ../blessstar_config_v1.schema.json.
Otherwise uses a strict structural check equivalent to the schema (stdlib only).

Sync note: BUILTIN_INSTRUCTION_TYPES must match kernel/ir/src/requirements.cpp kBuiltinTypes[].
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

# Must stay in sync with kernel/ir/src/requirements.cpp :: kBuiltinTypes[]
BUILTIN_INSTRUCTION_TYPES = frozenset({"test", "type1", "type2", "type3", "type"})


def _schema_path() -> Path:
    return Path(__file__).resolve().parent.parent / "blessstar_config_v1.schema.json"


def _enforce_builtin_instruction_types(doc: dict) -> None:
    """M1: each instruction.type must be in current kernel builtin (sync requirements.cpp)."""
    instrs = doc.get("instructions", [])
    if not isinstance(instrs, list):
        return
    for i, item in enumerate(instrs):
        if not isinstance(item, dict):
            continue
        tval = item.get("type")
        if not isinstance(tval, str):
            continue
        t = tval.strip()
        if t and t not in BUILTIN_INSTRUCTION_TYPES:
            raise ValueError(
                f"instructions[{i}].type {t!r} not in kernel builtin set "
                f"(sync with requirements.cpp)"
            )


def _validate_structural(doc: object) -> None:
    if not isinstance(doc, dict):
        raise ValueError("root must be a JSON object")
    extra = set(doc.keys()) - {
        "kernel_version",
        "adapter_version",
        "manual_requirements",
        "instructions",
    }
    if extra:
        raise ValueError(f"unknown top-level keys: {sorted(extra)}")
    for key in ("kernel_version", "adapter_version", "instructions"):
        if key not in doc:
            raise ValueError(f"missing required field: {key}")
    if not isinstance(doc["kernel_version"], str) or not doc["kernel_version"].strip():
        raise ValueError("kernel_version must be a non-empty string")
    if not isinstance(doc["adapter_version"], str) or not doc["adapter_version"].strip():
        raise ValueError("adapter_version must be a non-empty string")
    manual = doc.get("manual_requirements", [])
    if manual is None:
        manual = []
    if not isinstance(manual, list):
        raise ValueError("manual_requirements must be an array")
    for i, t in enumerate(manual):
        if not isinstance(t, str) or not t.strip():
            raise ValueError(f"manual_requirements[{i}] must be a non-empty string")
    instrs = doc["instructions"]
    if not isinstance(instrs, list):
        raise ValueError("instructions must be an array")
    for i, item in enumerate(instrs):
        if not isinstance(item, dict):
            raise ValueError(f"instructions[{i}] must be an object")
        ikeys = set(item.keys())
        if not {"type", "name"}.issubset(ikeys):
            raise ValueError(f"instructions[{i}] missing type or name")
        extra_i = ikeys - {"type", "name", "metadata"}
        if extra_i:
            raise ValueError(f"instructions[{i}] unknown keys: {sorted(extra_i)}")
        if not isinstance(item["type"], str) or not item["type"].strip():
            raise ValueError(f"instructions[{i}].type must be a non-empty string")
        if not isinstance(item["name"], str) or not item["name"].strip():
            raise ValueError(f"instructions[{i}].name must be a non-empty string")
        if "metadata" in item and item["metadata"] is not None:
            md = item["metadata"]
            if not isinstance(md, dict):
                raise ValueError(f"instructions[{i}].metadata must be an object")
            for k, v in md.items():
                if not isinstance(k, str) or not k.strip():
                    raise ValueError(f"instructions[{i}].metadata has invalid key")
                if not isinstance(v, str):
                    raise ValueError(
                        f"instructions[{i}].metadata[{k!r}] must be a string (MVP)"
                    )
    if isinstance(doc, dict):
        _enforce_builtin_instruction_types(doc)


def validate_document(doc: object) -> None:
    try:
        import jsonschema
        from jsonschema import ValidationError
    except ImportError:
        _validate_structural(doc)
        return
    schema_path = _schema_path()
    if not schema_path.is_file():
        _validate_structural(doc)
        return
    schema = json.loads(schema_path.read_text(encoding="utf-8"))
    try:
        jsonschema.validate(instance=doc, schema=schema)
    except ValidationError as e:
        raise ValueError(str(e)) from e
    if isinstance(doc, dict):
        _enforce_builtin_instruction_types(doc)


def canonicalize(doc: dict) -> str:
    """Stable UTF-8 JSON for file:// consumption (sorted keys recursively)."""
    return json.dumps(doc, ensure_ascii=False, indent=2, sort_keys=True) + "\n"


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: identity_normalize.py <input.json> <output.json>", file=sys.stderr)
        return 2
    in_path = Path(sys.argv[1])
    out_path = Path(sys.argv[2])
    if not in_path.is_file():
        print(f"input not found: {in_path}", file=sys.stderr)
        return 2
    try:
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
        validate_document(doc)
    except ValueError as e:
        print(f"validation failed: {e}", file=sys.stderr)
        return 1
    if not isinstance(doc, dict):
        print("internal error: expected dict after validation", file=sys.stderr)
        return 1
    text = canonicalize(doc)
    try:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(text, encoding="utf-8", newline="\n")
    except OSError as e:
        print(f"write error: {e}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Generate a ~10MB BlessStar Config JSON v1 fixture for Day19 baseline (T19.1)."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def build_payload(target_bytes: int) -> dict:
    base = {
        "kernel_version": "0.4.0",
        "adapter_version": "0.4.0",
        "manual_requirements": [],
        "instructions": [],
    }
    # Grow instructions until serialized size reaches target.
    idx = 0
    while True:
        base["instructions"].append(
            {
                "type": "test",
                "name": f"day19-pad-{idx}",
                "metadata": {"pad": "x" * 256},
            }
        )
        raw = json.dumps(base, separators=(",", ":")).encode("utf-8")
        if len(raw) >= target_bytes:
            return base
        idx += 1
        if idx > 50000:
            raise RuntimeError("fixture growth limit exceeded")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--size-mb", type=int, default=10)
    args = ap.parse_args()
    target = max(1024, args.size_mb * 1024 * 1024)
    payload = build_payload(target)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    size = args.output.stat().st_size
    print(f"[day19] wrote {args.output} ({size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

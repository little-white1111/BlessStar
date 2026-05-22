#!/usr/bin/env python3
"""Static anchor: bootstrap must call freeze after log bind (IMPL-05-01 / R8-09)."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

REQUIRED_SNIPPETS = [
    ("registry_bootstrap.cpp", "bs_adapter_log_bind_spdlog_bus"),
    ("registry_bootstrap.cpp", "bs_adapter_registry_bootstrap_freeze"),
    ("registry_bootstrap.cpp", "bs_adapter_attach_is_log_ready"),
]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
    )
    args = parser.parse_args()
    root = args.repo_root.resolve()
    errors: list[str] = []

    for rel, needle in REQUIRED_SNIPPETS:
        path = root / "adapter" / "src" / rel if "/" not in rel else root / rel
        if rel.count("/") == 0:
            path = root / "adapter" / "src" / rel
        else:
            path = root / rel
        if not path.is_file():
            errors.append(f"missing file: {path}")
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if needle not in text:
            errors.append(f"{path}: missing anchor {needle!r}")

    rb = root / "adapter" / "src" / "registry_bootstrap.cpp"
    if rb.is_file():
        text = rb.read_text(encoding="utf-8", errors="replace")
        bind_pos = text.find("bs_adapter_log_bind_spdlog_bus")
        freeze_pos = text.find("bs_adapter_registry_bootstrap_freeze")
        ready_pos = text.find("bs_adapter_attach_is_log_ready")
        if bind_pos >= 0 and freeze_pos >= 0 and bind_pos > freeze_pos:
            errors.append("registry_bootstrap: log bind must appear before freeze function body")
        if ready_pos >= 0 and freeze_pos >= 0:
            freeze_body = text[freeze_pos:]
            if "bs_adapter_attach_is_log_ready" not in freeze_body:
                errors.append("freeze must guard with bs_adapter_attach_is_log_ready")
            freeze_call = freeze_body.find("bs_registry_facade_freeze")
            if freeze_call >= 0:
                before_freeze = freeze_body[:freeze_call]
                if "builtin ir_gate" not in before_freeze:
                    errors.append(
                        "freeze: missing R5-02 anchor comment (builtin ir_gate registered in P1)"
                    )

    if errors:
        print("Freeze order gate FAILED:", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        return 1

    print("Freeze order gate OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())

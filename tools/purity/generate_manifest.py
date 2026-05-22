#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(description="Generate manifest.sha256 for factory package.")
    ap.add_argument("--root", required=True, help="Repo root")
    ap.add_argument("--factory", default="factory", help="Factory dir (relative to root)")
    ap.add_argument("--out", default="factory/manifest.sha256", help="Manifest output (relative to root)")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    factory_dir = (root / args.factory).resolve()
    out_path = (root / args.out).resolve()

    if not factory_dir.exists():
        raise SystemExit(f"factory dir not found: {factory_dir}")

    files = sorted([p for p in factory_dir.rglob("*") if p.is_file()])
    lines: list[str] = []
    for p in files:
        # Paths in manifest are relative to factory/ (matches `verify ... --kernel factory`).
        if p.resolve() == out_path.resolve():
            continue
        rel = p.relative_to(factory_dir).as_posix()
        digest = sha256_file(p)
        lines.append(f"{digest}  {rel}")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {out_path} ({len(lines)} entries)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

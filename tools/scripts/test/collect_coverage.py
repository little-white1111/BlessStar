#!/usr/bin/env python3
"""Collect CTest label coverage from cmake/Tests.cmake.

This is a reporting helper for REG-A'-4. It intentionally returns success even
when labels are sparse; PR blocking remains in contract/ctest gates.
"""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path


TEST_RE = re.compile(r"blessstar_add_unit_test\s*\(\s*([A-Za-z0-9_]+)")
LABELS_RE = re.compile(
    r"set_tests_properties\s*\(\s*([A-Za-z0-9_]+)\s+PROPERTIES.*?LABELS\s+\"([^\"]+)\"",
    re.DOTALL,
)


def collect(path: Path) -> dict:
    text = path.read_text(encoding="utf-8")
    tests = TEST_RE.findall(text)
    labels_by_test: dict[str, list[str]] = {name: [] for name in tests}
    for name, raw_labels in LABELS_RE.findall(text):
        labels_by_test[name] = [item for item in raw_labels.split(";") if item]

    tests_by_label: dict[str, list[str]] = defaultdict(list)
    unlabeled: list[str] = []
    for name in sorted(labels_by_test):
        labels = labels_by_test[name]
        if not labels:
            unlabeled.append(name)
        for label in labels:
            tests_by_label[label].append(name)

    return {
        "source": str(path).replace("\\", "/"),
        "test_count": len(labels_by_test),
        "label_count": len(tests_by_label),
        "labels": {label: sorted(names) for label, names in sorted(tests_by_label.items())},
        "unlabeled_tests": unlabeled,
    }


def write_markdown(report: dict, output: Path) -> None:
    lines = [
        "# CTest Label Coverage",
        "",
        f"- Source: `{report['source']}`",
        f"- Tests: {report['test_count']}",
        f"- Labels: {report['label_count']}",
        "",
        "| Label | Test Count |",
        "|-------|------------|",
    ]
    for label, names in report["labels"].items():
        lines.append(f"| `{label}` | {len(names)} |")
    if report["unlabeled_tests"]:
        lines.extend(["", "## Unlabeled Tests", ""])
        lines.extend(f"- `{name}`" for name in report["unlabeled_tests"])
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tests-cmake", default="cmake/Tests.cmake")
    parser.add_argument("--json-out", default="")
    parser.add_argument("--markdown-out", default="")
    args = parser.parse_args()

    report = collect(Path(args.tests_cmake))
    payload = json.dumps(report, ensure_ascii=False, indent=2)
    if args.json_out:
        Path(args.json_out).write_text(payload + "\n", encoding="utf-8")
    else:
        print(payload)
    if args.markdown_out:
        write_markdown(report, Path(args.markdown_out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

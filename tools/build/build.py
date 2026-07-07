#!/usr/bin/env python3
"""BlessStar CMake build helper.

Usage:
    build.py --release --print-cmake-args --build-dir=DIR
    build.py --sanitize --print-cmake-args --build-dir=DIR
    build.py --tsan    --print-cmake-args --build-dir=DIR
    build.py --release --print-cmake-args --build-dir=DIR --format=json
"""

import argparse
import json
import os
import sys

REPO_ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))


def cmake_args(args: argparse.Namespace) -> list[str]:
    cmd = [
        "cmake",
        "-S", REPO_ROOT,
        "-B", args.build_dir,
    ]

    if args.release:
        cmd.append("-DCMAKE_BUILD_TYPE=Release")
    elif args.sanitize:
        cmd.append("-DCMAKE_BUILD_TYPE=Debug")
        cmd.append("-DSANITIZE_ADDRESS=ON")
        cmd.append("-DSANITIZE_UNDEFINED=ON")
    elif args.tsan:
        cmd.append("-DCMAKE_BUILD_TYPE=Debug")
        cmd.append("-DSANITIZE_THREAD=ON")
    else:
        cmd.append("-DCMAKE_BUILD_TYPE=Release")

    return cmd


def main() -> None:
    parser = argparse.ArgumentParser(description="BlessStar CMake build helper")
    parser.add_argument("--release", action="store_true", help="Release build")
    parser.add_argument("--sanitize", action="store_true", help="ASan+UBSan build")
    parser.add_argument("--tsan", action="store_true", help="TSan build")
    parser.add_argument("--print-cmake-args", action="store_true", help="Print cmake arguments")
    parser.add_argument("--build-dir", default="build", help="Build directory")
    parser.add_argument("--format", choices=["shell", "json"], default="shell",
                        help="Output format (shell or json)")
    parsed = parser.parse_args()

    if parsed.print_cmake_args:
        cmd = cmake_args(parsed)
        if parsed.format == "json":
            print(json.dumps({"args": cmd}))
        else:
            # Output as shell-safe quoted string suitable for eval
            print(" ".join(sh_quote(a) for a in cmd))


def sh_quote(s: str) -> str:
    """Minimal shell quoting — add quotes only when needed."""
    if " " in s or "(" in s or ")" in s or "$" in s or '"' in s:
        return f"'{s.replace(chr(39), chr(39) + '\\' + chr(39))}'"
    return s


if __name__ == "__main__":
    main()

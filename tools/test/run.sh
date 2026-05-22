#!/bin/sh
#
# Run CTest for a BlessStar CMake build directory (POSIX-friendly wrapper).
# Equivalent to tools/test/run.ps1.
#
# Usage:
#   ./tools/test/run.sh [BUILD_DIR] [CONFIG]
#
#   BUILD_DIR  CMake binary directory (default: build)
#   CONFIG     Multi-config generator config, e.g. Release (optional).
#              If omitted, uses environment variable BLESSSTAR_CTEST_CONFIG when set.
#
# Examples:
#   ./tools/test/run.sh
#   ./tools/test/run.sh build/cmake
#   ./tools/test/run.sh build Release
#
# If not executable:  chmod +x tools/test/run.sh

set -e

build_dir=${1:-build}
config=${2:-}
if [ -z "$config" ]; then
	config=${BLESSSTAR_CTEST_CONFIG:-}
fi

if [ ! -d "$build_dir" ]; then
	echo "Build directory not found: $build_dir" >&2
	exit 1
fi

abs_build_dir=$(CDPATH= cd -- "$build_dir" && pwd)

if [ -n "$config" ]; then
	exec ctest --test-dir "$abs_build_dir" --output-on-failure -C "$config"
else
	exec ctest --test-dir "$abs_build_dir" --output-on-failure
fi

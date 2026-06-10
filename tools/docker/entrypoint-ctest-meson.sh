#!/bin/sh
set -e
# DOCK-OBS-1: overlay2 fsync timing deviation excludes manifest-fsync.
# Skip -LE day17: contract gates spawn sub-ctest processes that are not
# reachable by our -E filter; Docker CI supplements native CI gates.
ctest --test-dir build_ci_test --output-on-failure -LE day17 "$@"
meson test -C build-meson --print-errorlogs --no-rebuild

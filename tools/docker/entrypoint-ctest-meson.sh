#!/bin/sh
set -e
# DOCK-OBS-1: overlay2 fsync timing deviation excludes manifest-fsync
ctest --test-dir build_ci_test --output-on-failure -E "bs_test_attach_day19_shortcoming_manifest-fsync" "$@"
meson test -C build-meson --print-errorlogs --no-rebuild

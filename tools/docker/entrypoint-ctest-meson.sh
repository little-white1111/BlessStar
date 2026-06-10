#!/bin/sh
set -e
ctest --test-dir build_ci_test --output-on-failure "$@"
meson test -C build-meson --print-errorlogs --no-rebuild

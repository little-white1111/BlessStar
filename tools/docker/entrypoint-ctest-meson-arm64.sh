#!/bin/sh
set -e
python3 /src/tools/docker/patch_ctest_timeout_arm64.py /src/build_ci_test
# DOCK-OBS-2: ARM64 QEMU timing deviation excludes shortcoming_regression
ctest --test-dir build_ci_test --output-on-failure -E "bs_test_attach_day19_shortcoming_regression" "$@"
meson test -C build-meson --print-errorlogs --no-rebuild

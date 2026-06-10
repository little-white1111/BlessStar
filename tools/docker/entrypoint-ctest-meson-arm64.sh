#!/bin/sh
set -e
python3 /src/tools/docker/patch_ctest_timeout_arm64.py /src/build_ci_test
# ARM64 QEMU: skip -LE day17 (contract gates verified on native CI);
# also skip timing-sensitive shortcoming tests (DOCK-OBS-2).
ctest --test-dir build_ci_test --output-on-failure \
  -LE day17 \
  -E "bs_test_attach_day19_shortcoming_regression" "$@"
meson test -C build-meson --print-errorlogs --no-rebuild

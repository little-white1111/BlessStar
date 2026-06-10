#!/bin/sh
set -e
python3 /src/tools/docker/patch_ctest_timeout_arm64.py /src/build_ci_test
# DOCK-OBS-1/2: ARM64 QEMU skips contract gates (their sub-gates invoke
# independent ctest runs that are not affected by our -E filter) and
# timing-sensitive shortcoming tests verified only on native CI.
ctest --test-dir build_ci_test --output-on-failure \
  -LE day17 \
  -E "bs_test_attach_day19_shortcoming_(regression|manifest-fsync)" "$@"
meson test -C build-meson --print-errorlogs --no-rebuild

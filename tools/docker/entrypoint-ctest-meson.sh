#!/bin/sh
set -e
# Docker CI supplements native CI (DOCKER-CI-4).  Skip contract gates
# (their sub-ctest processes are not reachable by -E) and timing-
# sensitive shortcoming tests (DOCK-OBS-*): overlay2 fsync, QEMU
# thread/mutex, and ASan overhead all affect timing-dependent probes.
ctest --test-dir build_ci_test --output-on-failure \
  -LE day17 \
  -E "bs_test_attach_day19_shortcoming_" "$@"
meson test -C build-meson --print-errorlogs --no-rebuild

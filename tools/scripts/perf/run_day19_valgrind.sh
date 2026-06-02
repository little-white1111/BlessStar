#!/usr/bin/env bash
# T19.7: optional Valgrind on Day19 ci-profile stress (Linux only).
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
EXE="${BUILD_DIR}/bs_test_day19_stress_reload_loop"

if [[ ! -x "${EXE}" ]]; then
  echo "missing: ${EXE}" >&2
  exit 1
fi

export BS_DAY19_PROFILE=ci
valgrind --leak-check=full --error-exitcode=1 "${EXE}" --profile=ci

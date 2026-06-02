#!/usr/bin/env bash
# T19.11: Linux wall-clock 72h-RP full profile (Release evidence; not for PR).
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
PROFILE="${BS_DAY19_PROFILE:-full}"
OUT="${BS_DAY19_STRESS_OUT:-docs/day19_stress_full_latest.csv}"

EXE="${BUILD_DIR}/bs_test_day19_stress_reload_loop"
if [[ ! -x "${EXE}" ]]; then
  echo "missing executable: ${EXE}" >&2
  exit 1
fi

export BS_DAY19_PROFILE="${PROFILE}"
export BS_DAY19_STRESS_OUT="${OUT}"
echo "[day19] starting full stress profile=${PROFILE} out=${OUT}"
"${EXE}" --profile="${PROFILE}"
echo "[day19] completed full stress"

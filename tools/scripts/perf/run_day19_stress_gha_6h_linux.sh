#!/usr/bin/env bash
# T19 gha_6h: GitHub-hosted ubuntu long stress (~5h50m); not a substitute for Linux 72h full.
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build_day19_gha_6h}"
PROFILE="${BS_DAY19_PROFILE:-gha_6h}"
OUT="${BS_DAY19_STRESS_OUT:-docs/day19_stress_gha_6h_latest.csv}"
JSON_OUT="${BS_DAY19_STRESS_JSON:-docs/day19_stress_gha_6h_latest.json}"

EXE="${BUILD_DIR}/bs_test_day19_stress_reload_loop"
if [[ ! -x "${EXE}" ]]; then
  echo "missing executable: ${EXE}" >&2
  exit 1
fi

export BS_DAY19_PROFILE="${PROFILE}"
export BS_DAY19_STRESS_OUT="${OUT}"
export BS_DAY19_STRESS_JSON="${JSON_OUT}"
echo "[day19] starting gha_6h stress profile=${PROFILE} csv=${OUT} json=${JSON_OUT}"
set +e
"${EXE}" --profile="${PROFILE}"
rc=$?
set -e
if [[ ${rc} -ne 0 ]]; then
  echo "[day19] gha_6h stress FAILED exit=${rc}" >&2
  exit "${rc}"
fi
echo "[day19] completed gha_6h stress"

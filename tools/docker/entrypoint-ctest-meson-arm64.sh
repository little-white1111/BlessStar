#!/bin/sh
set -e
python3 /src/tools/docker/patch_ctest_timeout_arm64.py /src/build_ci_test
exec /usr/local/bin/docker-test-entrypoint.sh "$@"

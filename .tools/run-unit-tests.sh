#!/usr/bin/env bash
set -euo pipefail

# Run the unit tests in parallel. The test binary must run from tests/test_outputs, because the
# tests read/write project fixtures relative to the cwd; the cd is confined to a subshell so the
# caller's directory is unchanged even when this script is sourced.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly ROOT
readonly GTEST_PARALLEL="${ROOT}/tests/gtest-parallel/gtest-parallel"
readonly TEST_BINARY="${ROOT}/cmake-build-release-wsl-dev/tests/spp_tests"
readonly WORK_DIR="${ROOT}/tests/test_outputs"
readonly LOG_DIR="${ROOT}/tests"

(
    cd "${WORK_DIR}"
    "${GTEST_PARALLEL}" "${TEST_BINARY}" --output_dir="${LOG_DIR}" "$@"
)

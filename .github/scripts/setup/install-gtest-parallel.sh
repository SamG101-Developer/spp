#!/usr/bin/env bash
# Check gtest-parallel out at its pinned commit.
set -euo pipefail
source .github/scripts/lib/pinned-checkout.sh

pinned_checkout "${RUNNER_TEMP}/gtest-parallel" https://github.com/google/gtest-parallel.git "$GTEST_PARALLEL_COMMIT"

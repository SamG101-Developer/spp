#!/usr/bin/env bash
# Drive the gtest suite through gtest-parallel, writing one log per test.
set -euo pipefail

# Ensure the test binary can be found following the build stage. This is near enough guaranteed but a failsafe catches
# any edge case scenarios.
binary="build/tests/spp_tests"
[ "$RUNNER_OS" = "Windows" ] && binary="build/tests/spp_tests.exe"
if ! [ -x "$binary" ]; then
  echo "::error::test binary not found at $binary"
  exit 1
fi

# Ensure the gtest-parallel test runner script itself is present. The fetch step is skipped on a cache hit alone, so a
# cache entry that was saved from a half-finished clone leaves the directory in place with the script missing.
runner="${RUNNER_TEMP}/gtest-parallel/gtest-parallel"
if ! [ -f "$runner" ]; then
  echo "::error::gtest-parallel not found at $runner; bump GTEST_PARALLEL_CACHE_VERSION in .github/versions.env"
  exit 1
fi

# Run the parallel testing suite through the downloaded gtest-parallel script, setting the config options from the env
# flags.
mkdir -p "$OUTPUT_DIR"
python3 "$runner" \
  "./$binary" \
  --output_dir="$OUTPUT_DIR" \
  ${WORKERS:+--workers="$WORKERS"}
